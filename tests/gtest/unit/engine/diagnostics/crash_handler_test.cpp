/**
 * @file crash_handler_test.cpp
 * @brief 崩溃报告子系统单元测试。
 *
 * 覆盖：
 *  - BreadcrumbBuffer 环形缓冲：追加/容量回绕/顺序/清空
 *  - FormatCrashReport / FormatCrashReportJson：内容与转义
 *  - WriteCrashReport：真实落地文件 + 内容校验
 *  - CrashReporter：面包屑/元数据进报告、WriteManualReport 真实写出
 *    （Windows 上附带校验 minidump 魔数 "MDMP"）
 */

#include <gtest/gtest.h>
#include "engine/diagnostics/crash_handler.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace dse::diagnostics;

// ----------------------------- BreadcrumbBuffer -----------------------------

// 测试 面包屑缓冲区：Push范围内容量Keeps顺序
TEST(BreadcrumbBufferTest, PushWithinCapacity_KeepsOrder) {
    BreadcrumbBuffer buf(4);
    buf.Push("a");
    buf.Push("b");
    buf.Push("c");
    auto snap = buf.Snapshot();
    ASSERT_EQ(snap.size(), 3u);
    EXPECT_EQ(snap[0], "a");
    EXPECT_EQ(snap[1], "b");
    EXPECT_EQ(snap[2], "c");
    EXPECT_EQ(buf.Size(), 3u);
}

// 测试 面包屑缓冲区：溢出Drops Oldest
TEST(BreadcrumbBufferTest, OverflowDropsOldest) {
    BreadcrumbBuffer buf(3);
    buf.Push("1");
    buf.Push("2");
    buf.Push("3");
    buf.Push("4");  // 挤掉 "1"
    buf.Push("5");  // 挤掉 "2"
    auto snap = buf.Snapshot();
    ASSERT_EQ(snap.size(), 3u);
    EXPECT_EQ(snap[0], "3");
    EXPECT_EQ(snap[1], "4");
    EXPECT_EQ(snap[2], "5");
    EXPECT_EQ(buf.Size(), 3u);
    EXPECT_EQ(buf.Capacity(), 3u);
}

// 测试 面包屑缓冲区：清空Empties
TEST(BreadcrumbBufferTest, Clear_Empties) {
    BreadcrumbBuffer buf(2);
    buf.Push("x");
    buf.Clear();
    EXPECT_EQ(buf.Size(), 0u);
    EXPECT_TRUE(buf.Snapshot().empty());
}

// 测试 面包屑缓冲区：零容量Coerced到单个
TEST(BreadcrumbBufferTest, ZeroCapacityCoercedToOne) {
    BreadcrumbBuffer buf(0);
    EXPECT_EQ(buf.Capacity(), 1u);
    buf.Push("only");
    auto snap = buf.Snapshot();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap[0], "only");
}

// ----------------------------- Format helpers -----------------------------

static CrashReportInfo MakeSampleInfo() {
    CrashReportInfo info;
    info.app_name = "TestApp";
    info.app_version = "9.9.9-test";
    info.timestamp_utc = "2026-06-10T10:13:00Z";
    info.reason = "EXCEPTION_ACCESS_VIOLATION (0xc0000005)";
    info.fault_address = "0xdeadbeef";
    info.os_info = "Windows (x64)";
    info.dump_file = "crash_TestApp.dmp";
    info.call_stack = {"FrameA + 0x10", "FrameB + 0x20"};
    info.modules = {"dse_engine.dll @ 0x1000"};
    info.breadcrumbs = {"loaded level", "spawned player"};
    info.metadata = {{"build", "Debug"}, {"scene", "title"}};
    return info;
}

// 测试 崩溃报告格式：文本包含键字段
TEST(CrashReportFormatTest, TextContainsKeyFields) {
    const std::string txt = FormatCrashReport(MakeSampleInfo());
    EXPECT_NE(txt.find("TestApp"), std::string::npos);
    EXPECT_NE(txt.find("9.9.9-test"), std::string::npos);
    EXPECT_NE(txt.find("EXCEPTION_ACCESS_VIOLATION"), std::string::npos);
    EXPECT_NE(txt.find("0xdeadbeef"), std::string::npos);
    EXPECT_NE(txt.find("FrameA + 0x10"), std::string::npos);
    EXPECT_NE(txt.find("spawned player"), std::string::npos);
    EXPECT_NE(txt.find("scene = title"), std::string::npos);
    EXPECT_NE(txt.find("dse_engine.dll"), std::string::npos);
}

// 测试 崩溃报告格式：空栈Renders Unavailable
TEST(CrashReportFormatTest, EmptyStackRendersUnavailable) {
    CrashReportInfo info = MakeSampleInfo();
    info.call_stack.clear();
    const std::string txt = FormatCrashReport(info);
    EXPECT_NE(txt.find("(unavailable)"), std::string::npos);
}

// 测试 崩溃报告格式：JSON Escapes且包含Arrays
TEST(CrashReportFormatTest, JsonEscapesAndContainsArrays) {
    CrashReportInfo info = MakeSampleInfo();
    info.breadcrumbs = {"line\"with\"quotes", "tab\there"};
    const std::string json = FormatCrashReportJson(info);
    EXPECT_NE(json.find("\"app\":\"TestApp\""), std::string::npos);
    EXPECT_NE(json.find("\\\"with\\\""), std::string::npos);  // 引号被转义
    EXPECT_NE(json.find("\\t"), std::string::npos);            // 制表符被转义
    EXPECT_NE(json.find("\"call_stack\":["), std::string::npos);
    EXPECT_NE(json.find("\"breadcrumbs\":["), std::string::npos);
}

// ----------------------------- WriteCrashReport -----------------------------

class CrashReportFileTest : public ::testing::Test {
protected:
    std::filesystem::path dir_;
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() /
               ("dse_crash_test_" + std::to_string(::testing::UnitTest::GetInstance()
                                                       ->random_seed()) +
                "_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }
    static std::string ReadAll(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
};

// 测试 崩溃报告文件：写入文件带内容
TEST_F(CrashReportFileTest, WritesFileWithContent) {
    const std::string path = WriteCrashReport(dir_.string(), MakeSampleInfo());
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(std::filesystem::exists(path));
    const std::string content = ReadAll(path);
    EXPECT_NE(content.find("DSEngine Crash Report"), std::string::npos);
    EXPECT_NE(content.find("EXCEPTION_ACCESS_VIOLATION"), std::string::npos);
}

// ----------------------------- CrashReporter -----------------------------

// 测试 崩溃报告文件：Reporter Builds信息从Breadcrumbs且元数据
TEST_F(CrashReportFileTest, ReporterBuildsInfoFromBreadcrumbsAndMetadata) {
    CrashHandlerConfig cfg;
    cfg.app_name = "SelfTestApp";
    cfg.dump_dir = dir_.string();
    cfg.write_minidump = false;
    cfg.max_breadcrumbs = 8;
    CrashReporter& r = CrashReporter::Instance();
    ASSERT_TRUE(r.Install(cfg));

    r.AddBreadcrumb("crumb-one");
    r.AddBreadcrumb("crumb-two");
    r.SetMetadata("level", "forest");

    CrashReportInfo info = r.BuildBaseInfo("manual-check");
    EXPECT_EQ(info.app_name, "SelfTestApp");
    EXPECT_FALSE(info.app_version.empty());  // 取自 DSE_VERSION_STRING
    EXPECT_EQ(info.reason, "manual-check");
    ASSERT_GE(info.breadcrumbs.size(), 2u);
    EXPECT_EQ(info.breadcrumbs[info.breadcrumbs.size() - 2], "crumb-one");
    EXPECT_EQ(info.breadcrumbs.back(), "crumb-two");
    bool found_meta = false;
    for (const auto& kv : info.metadata) {
        if (kv.first == "level" && kv.second == "forest") found_meta = true;
    }
    EXPECT_TRUE(found_meta);

    r.Uninstall();
}

// 测试 崩溃报告文件：手动报告写入文件
TEST_F(CrashReportFileTest, ManualReportWritesFile) {
    CrashHandlerConfig cfg;
    cfg.app_name = "ManualApp";
    cfg.dump_dir = dir_.string();
#if defined(_WIN32)
    cfg.write_minidump = true;
#else
    cfg.write_minidump = false;
#endif
    CrashReporter& r = CrashReporter::Instance();
    ASSERT_TRUE(r.Install(cfg));
    r.AddBreadcrumb("before-manual-report");

    const std::string path = r.WriteManualReport("self-diagnostic");
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(std::filesystem::exists(path));
    EXPECT_EQ(r.LastReportPath(), path);

    const std::string content = ReadAll(path);
    EXPECT_NE(content.find("self-diagnostic"), std::string::npos);
    EXPECT_NE(content.find("before-manual-report"), std::string::npos);

#if defined(_WIN32)
    // 校验确有 minidump 落地，且魔数为 "MDMP"。
    bool found_dmp = false;
    for (const auto& e : std::filesystem::directory_iterator(dir_)) {
        if (e.path().extension() == ".dmp") {
            std::ifstream in(e.path(), std::ios::binary);
            char magic[4] = {};
            in.read(magic, 4);
            if (magic[0] == 'M' && magic[1] == 'D' && magic[2] == 'M' && magic[3] == 'P') {
                found_dmp = true;
            }
        }
    }
    EXPECT_TRUE(found_dmp);
#endif

    r.Uninstall();
}
