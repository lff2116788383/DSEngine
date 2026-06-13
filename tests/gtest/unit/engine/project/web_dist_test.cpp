/**
 * @file web_dist_test.cpp
 * @brief CollectWebDistribution（dse dist --target web 的核心逻辑）单元测试
 */

#include <gtest/gtest.h>
#include "engine/project/web_dist.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

class WebDistTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() / "dse_web_dist_test";
        std::error_code ec;
        fs::remove_all(root_, ec);
        in_ = root_ / "bin";
        out_ = root_ / "dist" / "web";
        fs::create_directories(in_, ec);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    void WriteFile(const fs::path& dir, const std::string& name, const std::string& content) {
        std::ofstream out((dir / name).string(), std::ios::binary);
        out << content;
    }

    bool HasFile(const dse::project::WebDistResult& r, const std::string& name) const {
        return std::find(r.files.begin(), r.files.end(), name) != r.files.end();
    }

    fs::path root_;
    fs::path in_;
    fs::path out_;
};

// 必需产物齐全 + 可选 .data 存在：全部收集，总字节数累加。
TEST_F(WebDistTest, CollectsRequiredAndOptionalArtifacts) {
    WriteFile(in_, "index.html", "<html></html>");      // 13 bytes
    WriteFile(in_, "index.js", "var Module={};");        // 14 bytes
    WriteFile(in_, "index.wasm", std::string("\0asm\1\0\0\0", 8));  // 8 bytes (embedded NULs)
    WriteFile(in_, "index.data", "PRELOAD");              // 7 bytes

    auto res = dse::project::CollectWebDistribution(in_.string(), out_.string());

    ASSERT_TRUE(res.ok) << res.error;
    EXPECT_EQ(res.files.size(), 4u);
    EXPECT_TRUE(HasFile(res, "index.html"));
    EXPECT_TRUE(HasFile(res, "index.js"));
    EXPECT_TRUE(HasFile(res, "index.wasm"));
    EXPECT_TRUE(HasFile(res, "index.data"));
    EXPECT_EQ(res.total_bytes, 13u + 14u + 8u + 7u);

    // 产物确实被拷到输出目录。
    EXPECT_TRUE(fs::exists(out_ / "index.html"));
    EXPECT_TRUE(fs::exists(out_ / "index.wasm"));
    EXPECT_TRUE(fs::exists(out_ / "index.data"));
}

// 仅必需产物（无 .data）：成功，只收集 3 个文件。
TEST_F(WebDistTest, OptionalArtifactsMayBeAbsent) {
    WriteFile(in_, "index.html", "x");
    WriteFile(in_, "index.js", "y");
    WriteFile(in_, "index.wasm", "z");

    auto res = dse::project::CollectWebDistribution(in_.string(), out_.string());

    ASSERT_TRUE(res.ok) << res.error;
    EXPECT_EQ(res.files.size(), 3u);
    EXPECT_FALSE(HasFile(res, "index.data"));
}

// 缺少必需产物：失败，且不创建输出目录。
TEST_F(WebDistTest, FailsWhenRequiredArtifactMissing) {
    WriteFile(in_, "index.html", "x");
    WriteFile(in_, "index.js", "y");
    // 故意不写 index.wasm

    auto res = dse::project::CollectWebDistribution(in_.string(), out_.string());

    EXPECT_FALSE(res.ok);
    EXPECT_NE(res.error.find("index.wasm"), std::string::npos);
    EXPECT_FALSE(fs::exists(out_));
}

// 输入目录不存在：失败。
TEST_F(WebDistTest, FailsWhenInputDirMissing) {
    auto res = dse::project::CollectWebDistribution(
        (root_ / "does_not_exist").string(), out_.string());
    EXPECT_FALSE(res.ok);
}

} // namespace
