/**
 * @file web_build_test.cpp
 * @brief ResolveWebPreset / PlanWebBuild / RunWebBuild（dse build --target web 的核心逻辑）单元测试
 */

#include <gtest/gtest.h>
#include "engine/project/web_build.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

void SetEnvVar(const char* key, const char* value) {
#if defined(_WIN32)
    _putenv_s(key, value);
#else
    if (value != nullptr && value[0] != '\0') {
        setenv(key, value, 1);
    } else {
        unsetenv(key);
    }
#endif
}

// debug × enable_3d → 四个预设；explicit 优先。
TEST(WebBuildPresetTest, ResolvesFromFlags) {
    EXPECT_EQ(dse::project::ResolveWebPreset(false, false, ""), "web-release");
    EXPECT_EQ(dse::project::ResolveWebPreset(true, false, ""), "web-debug");
    EXPECT_EQ(dse::project::ResolveWebPreset(false, true, ""), "web-release-3d");
    EXPECT_EQ(dse::project::ResolveWebPreset(true, true, ""), "web-debug-3d");
}

TEST(WebBuildPresetTest, ExplicitPresetWins) {
    EXPECT_EQ(dse::project::ResolveWebPreset(true, true, "web-release"), "web-release");
    EXPECT_EQ(dse::project::ResolveWebPreset(false, false, "custom-preset"), "custom-preset");
}

// 命令串与解析出的预设一致。
TEST(WebBuildPlanTest, BuildsCommandStrings) {
    dse::project::WebBuildOptions opts;
    opts.enable_3d = true;  // -> web-release-3d
    auto plan = dse::project::PlanWebBuild(opts);
    EXPECT_EQ(plan.preset, "web-release-3d");
    EXPECT_EQ(plan.configure_command, "cmake --preset web-release-3d");
    EXPECT_EQ(plan.build_command, "cmake --build --preset web-release-3d");
}

TEST(WebBuildPlanTest, ExplicitPresetUsedInCommands) {
    dse::project::WebBuildOptions opts;
    opts.preset = "web-debug";
    auto plan = dse::project::PlanWebBuild(opts);
    EXPECT_EQ(plan.configure_command, "cmake --preset web-debug");
    EXPECT_EQ(plan.build_command, "cmake --build --preset web-debug");
}

class WebBuildRunTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() / "dse_web_build_test";
        std::error_code ec;
        fs::remove_all(root_, ec);
        fs::create_directories(root_, ec);
    }
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
        SetEnvVar("EMSDK", "");  // 复位，避免影响其他测试
    }
    fs::path root_;
};

// EMSDK 未设置：直接失败并提示 EMSDK。
TEST_F(WebBuildRunTest, FailsWhenEmsdkUnset) {
    SetEnvVar("EMSDK", "");
    dse::project::WebBuildOptions opts;
    opts.source_dir = root_.string();
    opts.run_configure = false;
    opts.run_build = false;
    auto res = dse::project::RunWebBuild(opts);
    EXPECT_FALSE(res.ok);
    EXPECT_NE(res.error.find("EMSDK"), std::string::npos);
}

// 仓库根缺 CMakePresets.json：失败。
TEST_F(WebBuildRunTest, FailsWhenPresetsFileMissing) {
    SetEnvVar("EMSDK", (root_ / "fake_emsdk").string().c_str());
    dse::project::WebBuildOptions opts;
    opts.source_dir = root_.string();
    opts.run_configure = false;
    opts.run_build = false;
    auto res = dse::project::RunWebBuild(opts);
    EXPECT_FALSE(res.ok);
    EXPECT_NE(res.error.find("CMakePresets.json"), std::string::npos);
}

// EMSDK + CMakePresets.json 就绪，且关闭配置/编译：成功返回（不实际调用 cmake）。
TEST_F(WebBuildRunTest, SucceedsWithoutInvokingCMakeWhenStepsDisabled) {
    SetEnvVar("EMSDK", (root_ / "fake_emsdk").string().c_str());
    std::ofstream(root_ / "CMakePresets.json") << "{}";
    dse::project::WebBuildOptions opts;
    opts.source_dir = root_.string();
    opts.enable_3d = true;
    opts.run_configure = false;
    opts.run_build = false;
    auto res = dse::project::RunWebBuild(opts);
    ASSERT_TRUE(res.ok) << res.error;
    EXPECT_EQ(res.preset, "web-release-3d");
    EXPECT_EQ(res.configure_exit, 0);
    EXPECT_EQ(res.build_exit, 0);
}

} // namespace
