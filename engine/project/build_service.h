/**
 * @file build_service.h
 * @brief P1-3: Unified BuildService — single entry point for game packaging
 *        across all platforms (Windows, Linux, Web, Android).
 *
 * Both the editor's "Build Game" dialog and the `dse` CLI `build` subcommand
 * call the same BuildService::RunGameBuild, ensuring identical artifact
 * layout and encryption behavior.
 *
 * Design:
 *  - Pure data interface (no UI dependencies, no ImGui, no global state).
 *  - Streaming log callback for real-time output.
 *  - Cooperative cancellation via atomic flag.
 *  - Platform dispatch is internal; callers see one function.
 *  - Asset packing (dpak/bun), manifest writing, and runtime copying
 *    are shared across desktop platforms; web delegates to RunWebBuild;
 *    android delegates to the export script.
 */

#ifndef DSE_PROJECT_BUILD_SERVICE_H
#define DSE_PROJECT_BUILD_SERVICE_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "engine/core/dse_export.h"

namespace dse::project {

/// Target platform for the build.
enum class BuildTarget {
    Windows,
    Linux,
    Web,        ///< Emscripten
    Android,    ///< APK via export script
};

/// Build configuration (optimization level).
enum class BuildMode {
    Release,
    Debug,
};

/// Streaming log callback: line (no trailing newline), is_stderr.
using BuildLogFn = std::function<void(const std::string& line, bool is_stderr)>;

/// Input options for a game build.
struct BuildOptions {
    // --- Identity ---
    std::string game_title;             ///< Display name / exe basename.

    // --- Output ---
    std::string output_dir;             ///< Where to place build artifacts.

    // --- Platform / Config ---
    BuildTarget target = BuildTarget::Windows;
    BuildMode mode = BuildMode::Release;

    // --- Asset packing ---
    bool pack_all_data = true;          ///< true = pack entire data/; false = scene-referenced only.
    bool compress_pak = true;           ///< Compress dpak entries.

    // --- Encryption ---
    bool encrypt = false;               ///< true = produce encrypted game.bun instead of plaintext game.dpak.
    std::string encrypt_key;            ///< AES-128-CTR key (>= 16 chars when encrypt=true).

    // --- Project ---
    std::string project_root;           ///< Path to project root (contains scripts/, scenes/, assets/).
    std::string asset_dir;              ///< Path to data/ directory (asset root).
    std::string entry_script;           ///< Lua entry script (e.g. "scripts/main.lua").
    std::string icon_path;              ///< Optional icon (.ico) for splash.

    // --- Android-specific ---
    std::string android_package_id = "com.dsengine.mygame";
    int android_api_level = 24;         ///< minSdkVersion.
    std::string android_keystore;       ///< Empty = auto-generated debug key.
    std::string android_keystore_pass;
    std::string android_key_alias;

    // --- Engine ---
    std::string engine_root;            ///< Repo root (for CMakePresets.json / export scripts).
    std::string runtime_exe_path;       ///< Pre-built runtime exe (desktop platforms).

    // --- Streaming / cancellation ---
    BuildLogFn on_line;                 ///< Non-null = receive build log lines.
    const std::atomic<bool>* cancel = nullptr;  ///< Non-null + true = cancel build.

    // --- Web-specific ---
    std::string web_preset;             ///< Explicit CMake preset; empty = auto-resolve.
    bool web_enable_3d = false;
};

/// Result of a game build.
struct BuildResult {
    bool ok = false;
    bool canceled = false;
    std::string error;                  ///< Non-empty on failure.

    std::string output_dir;             ///< Final artifact directory.
    std::string primary_artifact;       ///< Main output file (exe, apk, zip, etc.).
    std::string launch_args;            ///< Args to pass when launching (e.g. --bundle/--key).

    std::uint64_t total_bytes = 0;      ///< Sum of artifact sizes.
    int file_count = 0;                 ///< Number of files produced.
};

/**
 * @brief Execute a complete game build for the specified platform.
 *
 * This is the single unified entry point used by both the editor and the CLI.
 * Dispatches internally to:
 *  - Desktop (Windows/Linux): copy runtime + pack assets + write manifest
 *  - Web: RunWebBuild() with artifact collection + zip
 *  - Android: stage assets + run export_android_apk.ps1
 *
 * @param opts Build options (fully populated by caller).
 * @return BuildResult with artifact paths on success, or error on failure.
 */
DSE_EXPORT BuildResult RunGameBuild(const BuildOptions& opts);

/**
 * @brief Validate build options before starting a build.
 *
 * Checks for required fields (game_title, output_dir, etc.),
 * encryption key length, and returns a human-readable error string
 * (empty if all checks pass).
 */
DSE_EXPORT std::string ValidateBuildOptions(const BuildOptions& opts);

/**
 * @brief Resolve the engine repository root by searching upward for CMakePresets.json.
 *
 * @param start_dir Directory to start searching from.
 * @return Repo root path, or empty string if not found.
 */
DSE_EXPORT std::string FindEngineRoot(const std::string& start_dir);

/**
 * @brief Locate a pre-built runtime executable for desktop builds.
 *
 * Searches common names (dsengine_game_release.exe, dsengine_game, etc.)
 * relative to the given search directory.
 *
 * @param search_dir Directory to search in (typically editor exe dir).
 * @return Full path to runtime exe, or empty if not found.
 */
DSE_EXPORT std::string FindRuntimeExe(const std::string& search_dir);

} // namespace dse::project

#endif // DSE_PROJECT_BUILD_SERVICE_H
