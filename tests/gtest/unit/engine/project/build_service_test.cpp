/**
 * @file build_service_test.cpp
 * @brief Unit tests for P1-3 unified BuildService.
 *
 * Tests the pure-logic functions: ValidateBuildOptions, FindEngineRoot,
 * FindRuntimeExe, and the build dispatch routing.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "engine/project/build_service.h"

using namespace dse::project;
namespace fs = std::filesystem;

// ============================================================
// ValidateBuildOptions
// ============================================================

TEST(BuildServiceTest, ValidateRejectsEmptyTitle) {
    BuildOptions opts;
    opts.output_dir = "/tmp/out";
    auto err = ValidateBuildOptions(opts);
    EXPECT_FALSE(err.empty());
    EXPECT_NE(err.find("title"), std::string::npos);
}

TEST(BuildServiceTest, ValidateRejectsEmptyOutputDir) {
    BuildOptions opts;
    opts.game_title = "Test";
    auto err = ValidateBuildOptions(opts);
    EXPECT_FALSE(err.empty());
    EXPECT_NE(err.find("Output"), std::string::npos);
}

TEST(BuildServiceTest, ValidateRejectsShortEncryptKey) {
    BuildOptions opts;
    opts.game_title = "Test";
    opts.output_dir = "/tmp/out";
    opts.encrypt = true;
    opts.encrypt_key = "short";  // < 16 chars
    auto err = ValidateBuildOptions(opts);
    EXPECT_FALSE(err.empty());
    EXPECT_NE(err.find("Encryption key"), std::string::npos);
}

TEST(BuildServiceTest, ValidateAcceptsValidOptions) {
    BuildOptions opts;
    opts.game_title = "MyGame";
    opts.output_dir = "/tmp/out";
    opts.encrypt = true;
    opts.encrypt_key = "0123456789ABCDEF";  // exactly 16 chars
    auto err = ValidateBuildOptions(opts);
    EXPECT_TRUE(err.empty()) << err;
}

TEST(BuildServiceTest, ValidateRejectsAndroidWithoutProjectRoot) {
    BuildOptions opts;
    opts.game_title = "MyGame";
    opts.output_dir = "/tmp/out";
    opts.target = BuildTarget::Android;
    auto err = ValidateBuildOptions(opts);
    EXPECT_FALSE(err.empty());
    EXPECT_NE(err.find("project root"), std::string::npos);
}

TEST(BuildServiceTest, ValidateAcceptsAndroidWithProjectRoot) {
    BuildOptions opts;
    opts.game_title = "MyGame";
    opts.output_dir = "/tmp/out";
    opts.target = BuildTarget::Android;
    opts.project_root = "/tmp/project";
    auto err = ValidateBuildOptions(opts);
    EXPECT_TRUE(err.empty()) << err;
}

// ============================================================
// FindEngineRoot
// ============================================================

TEST(BuildServiceTest, FindEngineRootFindsCMakePresets) {
    // Create a temp dir with CMakePresets.json
    auto tmp = fs::temp_directory_path() / "dse_build_test_findroot";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    {
        std::ofstream f(tmp / "CMakePresets.json");
        f << "{}";
    }

    auto result = FindEngineRoot(tmp.string());
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(fs::path(result).filename(), tmp.filename());

    // Also test subdirectory search
    auto subdir = tmp / "sub" / "dir";
    fs::create_directories(subdir);
    result = FindEngineRoot(subdir.string());
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(fs::path(result).filename(), tmp.filename());

    // Non-existent dir → empty or searches from cwd
    result = FindEngineRoot("/nonexistent/path/that/does/not/exist");
    // May return empty or find root from cwd; either way should not crash
    // (We don't assert empty because cwd might have CMakePresets.json)

    fs::remove_all(tmp);
}

TEST(BuildServiceTest, FindEngineRootEmptyStartUsesCwd) {
    // Should not crash when start_dir is empty
    auto result = FindEngineRoot("");
    // May or may not find a root depending on cwd; just verify no crash
    SUCCEED();
}

// ============================================================
// FindRuntimeExe
// ============================================================

TEST(BuildServiceTest, FindRuntimeExeEmptyWhenNotFound) {
    auto tmp = fs::temp_directory_path() / "dse_build_test_findexe";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    auto result = FindRuntimeExe(tmp.string());
    EXPECT_TRUE(result.empty());
    fs::remove_all(tmp);
}

// ============================================================
// RunGameBuild validation path
// ============================================================

TEST(BuildServiceTest, RunGameBuildRejectsInvalidOptions) {
    BuildOptions opts;
    // Missing game_title and output_dir
    BuildResult r = RunGameBuild(opts);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
}

TEST(BuildServiceTest, RunGameBuildCanceledBeforeStart) {
    BuildOptions opts;
    opts.game_title = "Test";
    opts.output_dir = "/tmp/out";
    std::atomic<bool> cancel{true};
    opts.cancel = &cancel;
    BuildResult r = RunGameBuild(opts);
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.canceled);
}

// ============================================================
// RunGameBuild desktop path (missing runtime → fails gracefully)
// ============================================================

TEST(BuildServiceTest, RunGameBuildDesktopFailsWithoutRuntime) {
    BuildOptions opts;
    opts.game_title = "TestGame";
    opts.output_dir = (fs::temp_directory_path() / "dse_build_test_out").string();
    opts.target = BuildTarget::Windows;
    opts.mode = BuildMode::Release;
    // No runtime_exe_path, engine_root set to a dir without runtime exe
    opts.engine_root = (fs::temp_directory_path() / "dse_build_test_noruntime").string();
    fs::remove_all(opts.engine_root);
    fs::create_directories(opts.engine_root);

    std::vector<std::string> log_lines;
    opts.on_line = [&log_lines](const std::string& line, bool) {
        log_lines.push_back(line);
    };

    BuildResult r = RunGameBuild(opts);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
    EXPECT_NE(r.error.find("dsengine_game"), std::string::npos);

    // Verify logging was invoked
    bool found_error_log = false;
    for (const auto& l : log_lines) {
        if (l.find("ERROR") != std::string::npos) {
            found_error_log = true;
            break;
        }
    }
    EXPECT_TRUE(found_error_log);

    fs::remove_all(opts.engine_root);
    fs::remove_all(opts.output_dir);
}

// ============================================================
// BuildMode / BuildTarget enum coverage
// ============================================================

TEST(BuildServiceTest, BuildTargetEnumValues) {
    EXPECT_NE(static_cast<int>(BuildTarget::Windows), static_cast<int>(BuildTarget::Linux));
    EXPECT_NE(static_cast<int>(BuildTarget::Web), static_cast<int>(BuildTarget::Android));
    EXPECT_NE(static_cast<int>(BuildTarget::Windows), static_cast<int>(BuildTarget::Web));
}

TEST(BuildServiceTest, BuildModeEnumValues) {
    EXPECT_NE(static_cast<int>(BuildMode::Release), static_cast<int>(BuildMode::Debug));
}
