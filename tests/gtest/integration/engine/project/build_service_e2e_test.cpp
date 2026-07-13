/**
 * @file build_service_e2e_test.cpp
 * @brief P1-3 E2E integration tests for BuildService.
 *
 * These tests verify the full build pipeline end-to-end. They are
 * environment-aware: tests that require Emscripten, Android SDK, or
 * a pre-built runtime are automatically skipped when the toolchain
 * is not available, rather than failing.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

#include "engine/project/build_service.h"
#include "engine/project/web_build.h"

using namespace dse::project;
namespace fs = std::filesystem;

namespace {

fs::path MakeTempDir(const std::string& tag) {
    auto base = fs::temp_directory_path() /
                ("dse_build_e2e_" + tag + "_" +
                 std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
    fs::remove_all(base);
    fs::create_directories(base);
    return base;
}

void WriteFile(const fs::path& p, const std::string& content) {
    std::ofstream f(p, std::ios::binary);
    f << content;
}

} // namespace

// ============================================================
// Desktop build E2E (requires pre-built runtime exe)
// ============================================================

TEST(BuildServiceE2E, DesktopBuildWithRuntimeSucceeds) {
    // Locate runtime exe
    std::string runtime = FindRuntimeExe(fs::current_path().string());
    if (runtime.empty()) {
        // Try engine root
        std::string root = FindEngineRoot(fs::current_path().string());
        if (!root.empty()) {
            runtime = FindRuntimeExe(root);
        }
    }
    if (runtime.empty()) {
        GTEST_SKIP() << "No pre-built dsengine_game runtime found; "
                        "build dse_standalone target first.";
    }

    auto out_dir = MakeTempDir("desktop_e2e");
    auto proj_dir = MakeTempDir("desktop_proj");
    fs::create_directories(proj_dir / "scripts");
    WriteFile(proj_dir / "scripts" / "main.lua", "print('hello')\n");

    BuildOptions opts;
    opts.game_title = "TestGame";
    opts.output_dir = out_dir.string();
    opts.target = BuildTarget::Windows;
    opts.mode = BuildMode::Release;
    opts.runtime_exe_path = runtime;
    opts.project_root = proj_dir.string();
    opts.entry_script = "scripts/main.lua";

    std::vector<std::string> log;
    opts.on_line = [&log](const std::string& line, bool) {
        log.push_back(line);
    };

    BuildResult r = RunGameBuild(opts);
    EXPECT_TRUE(r.ok) << r.error;
    EXPECT_FALSE(r.primary_artifact.empty());
    EXPECT_TRUE(fs::exists(r.primary_artifact));

    // Manifest should exist
    EXPECT_TRUE(fs::exists(out_dir / "game.dsmanifest"));

    // Scripts should be copied
    EXPECT_TRUE(fs::exists(out_dir / "scripts" / "main.lua"));

    // Log should have entries
    EXPECT_FALSE(log.empty());

    fs::remove_all(out_dir);
    fs::remove_all(proj_dir);
}

TEST(BuildServiceE2E, DesktopBuildEncryptedProducesBun) {
    std::string runtime = FindRuntimeExe(fs::current_path().string());
    if (runtime.empty()) {
        GTEST_SKIP() << "No pre-built runtime found";
    }

    auto out_dir = MakeTempDir("enc_e2e");
    auto proj_dir = MakeTempDir("enc_proj");
    fs::create_directories(proj_dir / "scripts");
    WriteFile(proj_dir / "scripts" / "main.lua", "print('encrypted')\n");

    BuildOptions opts;
    opts.game_title = "EncGame";
    opts.output_dir = out_dir.string();
    opts.target = BuildTarget::Windows;
    opts.mode = BuildMode::Release;
    opts.runtime_exe_path = runtime;
    opts.project_root = proj_dir.string();
    opts.entry_script = "scripts/main.lua";
    opts.encrypt = true;
    opts.encrypt_key = "0123456789ABCDEF";

    BuildResult r = RunGameBuild(opts);
    EXPECT_TRUE(r.ok) << r.error;
    EXPECT_TRUE(fs::exists(out_dir / "game.bun"));
    EXPECT_FALSE(r.launch_args.empty());
    EXPECT_NE(r.launch_args.find("--bundle=game.bun"), std::string::npos);
    EXPECT_NE(r.launch_args.find("--key="), std::string::npos);

    fs::remove_all(out_dir);
    fs::remove_all(proj_dir);
}

// ============================================================
// Web build E2E (requires Emscripten)
// ============================================================

TEST(BuildServiceE2E, WebBuildEndToEnd) {
    // Check Emscripten availability
    EmscriptenStatus em = DetectEmscripten();
    if (!em.available) {
        GTEST_SKIP() << "Emscripten not available: " << em.detail
                     << "\nHint: " << em.install_hint;
    }

    // Find repo root with CMakePresets.json
    std::string root = FindEngineRoot(fs::current_path().string());
    if (root.empty()) {
        GTEST_SKIP() << "CMakePresets.json not found; run from engine repository";
    }

    auto out_dir = MakeTempDir("web_e2e");

    BuildOptions opts;
    opts.game_title = "WebGame";
    opts.output_dir = out_dir.string();
    opts.target = BuildTarget::Web;
    opts.mode = BuildMode::Release;
    opts.engine_root = root;

    std::atomic<bool> cancel{false};
    opts.cancel = &cancel;

    BuildResult r = RunGameBuild(opts);
    EXPECT_TRUE(r.ok) << r.error;
    EXPECT_GT(r.file_count, 0);

    fs::remove_all(out_dir);
}

// ============================================================
// Validation + diagnostic E2E
// ============================================================

TEST(BuildServiceE2E, BuildProducesReadableLog) {
    std::string runtime = FindRuntimeExe(fs::current_path().string());
    if (runtime.empty()) {
        GTEST_SKIP() << "No pre-built runtime found";
    }

    auto out_dir = MakeTempDir("log_e2e");
    auto proj_dir = MakeTempDir("log_proj");

    BuildOptions opts;
    opts.game_title = "LogTest";
    opts.output_dir = out_dir.string();
    opts.target = BuildTarget::Windows;
    opts.runtime_exe_path = runtime;
    opts.project_root = proj_dir.string();

    std::vector<std::string> log_lines;
    opts.on_line = [&log_lines](const std::string& line, bool is_err) {
        log_lines.push_back(line);
    };

    BuildResult r = RunGameBuild(opts);

    // Log should contain build start and completion messages
    bool found_start = false;
    bool found_output = false;
    for (const auto& l : log_lines) {
        if (l.find("Build Game Started") != std::string::npos) found_start = true;
        if (l.find("Output:") != std::string::npos) found_output = true;
    }
    EXPECT_TRUE(found_start);
    if (r.ok) {
        EXPECT_TRUE(found_output);
    }

    fs::remove_all(out_dir);
    fs::remove_all(proj_dir);
}

TEST(BuildServiceE2E, CancelBeforeStartProducesCanceledResult) {
    auto out_dir = MakeTempDir("cancel_e2e");

    BuildOptions opts;
    opts.game_title = "CancelTest";
    opts.output_dir = out_dir.string();
    opts.target = BuildTarget::Windows;

    std::atomic<bool> cancel{true};
    opts.cancel = &cancel;

    BuildResult r = RunGameBuild(opts);
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.canceled);

    fs::remove_all(out_dir);
}
