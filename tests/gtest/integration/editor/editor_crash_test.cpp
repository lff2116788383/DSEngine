/**
 * @file editor_crash_test.cpp
 * @brief 编辑器侧崩溃捕获薄封装（editor_crash.h）的无头测试。
 *
 * 覆盖：
 * - FindLatestCrashReportInDir：仅匹配编辑器报告前缀、取最新、忽略无关文件/不存在目录
 * - GetEditorCrashDir：DSE_CRASH_DIR 覆盖与默认值
 * - InstallEditorCrashHandler：app_name 身份、process 元数据、breadcrumb/metadata 转发
 * - GetPreviousSessionCrashReport：安装前快照上次会话遗留报告
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "editor_crash.h"
#include "engine/diagnostics/crash_handler.h"

namespace fs = std::filesystem;
using namespace dse::editor;

namespace {

void SetEnvVar(const char* name, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void UnsetEnvVar(const char* name) {
#if defined(_WIN32)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

fs::path MakeUniqueTempDir(const std::string& tag) {
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    fs::path dir = fs::temp_directory_path() / ("dse_crash_test_" + tag + "_" + std::to_string(now));
    fs::create_directories(dir);
    return dir;
}

void WriteFile(const fs::path& p, const std::string& content) {
    std::ofstream ofs(p, std::ios::trunc);
    ofs << content;
}

class EditorCrashTest : public ::testing::Test {
protected:
    void TearDown() override {
        ResetEditorCrashHandlerForTesting();
        UnsetEnvVar("DSE_CRASH_DIR");
        UnsetEnvVar("DSE_CRASH_HANDLER");
    }
};

}  // namespace

// ── FindLatestCrashReportInDir ────────────────────────────────────────────────

// 测试 编辑器崩溃：查找最新空开启缺失目录
TEST_F(EditorCrashTest, FindLatest_EmptyOnMissingDir) {
    EXPECT_TRUE(FindLatestCrashReportInDir("definitely/not/here_xyz").empty());
    EXPECT_TRUE(FindLatestCrashReportInDir("").empty());
}

// 测试 编辑器崩溃：查找最新忽略非编辑器Reports
TEST_F(EditorCrashTest, FindLatest_IgnoresNonEditorReports) {
    fs::path dir = MakeUniqueTempDir("ignore");
    WriteFile(dir / "crash_DSEngine_20260101_1.txt", "player crash");   // 引擎/玩家进程，前缀不符
    WriteFile(dir / "crash_DSEngine-Editor_20260101_1.dmp", "dump");    // 非 .txt
    WriteFile(dir / "notes.txt", "unrelated");
    EXPECT_TRUE(FindLatestCrashReportInDir(dir.string()).empty());
    fs::remove_all(dir);
}

// 测试 编辑器崩溃：查找最新返回Newest编辑器报告
TEST_F(EditorCrashTest, FindLatest_ReturnsNewestEditorReport) {
    fs::path dir = MakeUniqueTempDir("newest");
    fs::path older = dir / "crash_DSEngine-Editor_20260101_1.txt";
    fs::path newer = dir / "crash_DSEngine-Editor_20260102_2.txt";
    WriteFile(older, "older");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    WriteFile(newer, "newer");
    // 显式提升 newer 的修改时间，避免文件系统时间粒度造成的并列。
    fs::last_write_time(newer, fs::file_time_type::clock::now());

    const std::string found = FindLatestCrashReportInDir(dir.string());
    EXPECT_EQ(fs::path(found).filename().string(), "crash_DSEngine-Editor_20260102_2.txt");
    fs::remove_all(dir);
}

// ── GetEditorCrashDir ────────────────────────────────────────────────────────

// 测试 编辑器崩溃：获取崩溃目录默认且覆盖
TEST_F(EditorCrashTest, GetCrashDir_DefaultAndOverride) {
    UnsetEnvVar("DSE_CRASH_DIR");
    EXPECT_EQ(GetEditorCrashDir(), "crashes/editor");

    SetEnvVar("DSE_CRASH_DIR", "my/custom/dir");
    EXPECT_EQ(GetEditorCrashDir(), "my/custom/dir");
}

// ── Install + breadcrumb/metadata 转发 ───────────────────────────────────────

// 测试 编辑器崩溃：Install设置编辑器单位且Forwards
TEST_F(EditorCrashTest, Install_SetsEditorIdentityAndForwards) {
    fs::path dir = MakeUniqueTempDir("install");
    SetEnvVar("DSE_CRASH_DIR", dir.string());

    InstallEditorCrashHandler();
    ASSERT_TRUE(IsEditorCrashHandlerInstalled());

    AddEditorBreadcrumb("editor: unit-test breadcrumb");
    SetEditorCrashMetadata("scene", "assets/level1.dscene");

    const auto info = dse::diagnostics::CrashReporter::Instance().BuildBaseInfo("unit-test");
    EXPECT_EQ(info.app_name, "DSEngine-Editor");

    bool found_breadcrumb = false;
    for (const auto& b : info.breadcrumbs) {
        if (b.find("editor: unit-test breadcrumb") != std::string::npos) found_breadcrumb = true;
    }
    EXPECT_TRUE(found_breadcrumb);

    bool found_process = false, found_scene = false;
    for (const auto& kv : info.metadata) {
        if (kv.first == "process" && kv.second == "editor") found_process = true;
        if (kv.first == "scene" && kv.second == "assets/level1.dscene") found_scene = true;
    }
    EXPECT_TRUE(found_process);
    EXPECT_TRUE(found_scene);

    fs::remove_all(dir);
}

// 测试 编辑器崩溃：Install禁用按Env
TEST_F(EditorCrashTest, Install_DisabledByEnv) {
    SetEnvVar("DSE_CRASH_HANDLER", "0");
    InstallEditorCrashHandler();
    EXPECT_FALSE(IsEditorCrashHandlerInstalled());
}

// ── GetPreviousSessionCrashReport ────────────────────────────────────────────

// 测试 编辑器崩溃：先前Session Snapshots Leftover报告
TEST_F(EditorCrashTest, PreviousSession_SnapshotsLeftoverReport) {
    fs::path dir = MakeUniqueTempDir("prev");
    fs::path leftover = dir / "crash_DSEngine-Editor_20260101_99.txt";
    WriteFile(leftover, "leftover from previous session");
    SetEnvVar("DSE_CRASH_DIR", dir.string());

    InstallEditorCrashHandler();
    EXPECT_EQ(fs::path(GetPreviousSessionCrashReport()).filename().string(),
              "crash_DSEngine-Editor_20260101_99.txt");

    fs::remove_all(dir);
}

// 测试 编辑器崩溃：先前Session空当无Leftover
TEST_F(EditorCrashTest, PreviousSession_EmptyWhenNoLeftover) {
    fs::path dir = MakeUniqueTempDir("clean");
    SetEnvVar("DSE_CRASH_DIR", dir.string());

    InstallEditorCrashHandler();
    EXPECT_TRUE(GetPreviousSessionCrashReport().empty());

    fs::remove_all(dir);
}
