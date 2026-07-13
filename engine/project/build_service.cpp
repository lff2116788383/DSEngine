/**
 * @file build_service.cpp
 * @brief P1-3: Unified BuildService implementation.
 *
 * Extracts the build logic from editor_build_game.cpp into an engine-level
 * service that both the editor and CLI can call. The editor's DoBuild()
 * and the dse CLI's build subcommand now share the same code path.
 */

#include "engine/project/build_service.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "engine/assets/pak_writer.h"
#include "engine/assets/asset_scanner.h"
#include "engine/assets/bundle_packer.h"
#include "engine/runtime/app_manifest.h"
#include "engine/platform/process.h"
#include "engine/project/web_build.h"
#include "engine/base/debug.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace dse::project {

namespace {

// -----------------------------------------------------------------------
// Logging helper
// -----------------------------------------------------------------------

void Log(const BuildOptions& opts, const std::string& msg, bool is_err = false) {
    if (opts.on_line) {
        opts.on_line(msg, is_err);
    }
}

// -----------------------------------------------------------------------
// Manifest writing (shared across platforms)
// -----------------------------------------------------------------------

bool WriteGameManifest(const BuildOptions& opts, const fs::path& out_dir) {
    std::error_code ec;

    dse::runtime::AppManifest manifest;
    manifest.has_window_title = true;
    manifest.window_title = opts.game_title;
    manifest.has_window_size = true;
    manifest.window_width = 1280;
    manifest.window_height = 720;

    std::string entry = opts.entry_script;
    if (entry.empty()) entry = "scripts/main.lua";
    manifest.entry_script = entry;
    manifest.has_entry_script = true;

    manifest.has_splash = true;
    dse::platform::SplashConfig& cfg = manifest.splash;
    cfg.enabled = true;
    cfg.app_name = opts.game_title;
    cfg.bg_argb = 0xFF1E1E28u;
    cfg.accent_argb = 0xFF4A9EFFu;
    cfg.fade_in_ms = 600;
    cfg.min_display_ms = 900;
    cfg.fade_out_ms = 500;

    if (!opts.icon_path.empty()) {
        fs::path icon(opts.icon_path);
        std::string ext = icon.extension().string();
        for (auto& ch : ext) ch = static_cast<char>(::tolower(static_cast<unsigned char>(ch)));
        const bool image_ok = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                               ext == ".bmp" || ext == ".tga");
        if (image_ok && fs::exists(icon, ec)) {
            fs::path dest = out_dir / ("splash" + ext);
            fs::copy_file(icon, dest, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                cfg.image_path = dest.filename().string();
                Log(opts, "Copied splash image -> " + dest.filename().string());
            }
        }
    }

    if (dse::runtime::WriteAppManifest((out_dir / "game.dsmanifest").string(), manifest)) {
        Log(opts, "Wrote game.dsmanifest (entry_script + window + splash)");
        return true;
    }
    Log(opts, "WARNING: Failed to write game.dsmanifest", true);
    return false;
}

// -----------------------------------------------------------------------
// Run logged command (shared with editor)
// -----------------------------------------------------------------------

bool RunLoggedCommand(const BuildOptions& opts, const fs::path& exe,
                      const std::vector<std::string>& args) {
    dse::platform::ProcessOptions popts;
    popts.executable = exe.string();
    popts.args = args;
    popts.merge_stderr = true;
    dse::platform::ProcessResult r = dse::platform::RunProcess(
        popts,
        [&opts](std::string_view line, bool /*is_stderr*/) {
            if (!line.empty()) Log(opts, std::string(line));
        },
        std::chrono::milliseconds::zero(),
        opts.cancel);
    if (!r.launched) {
        Log(opts, "ERROR: Failed to start command: " +
                  (r.error.empty() ? exe.string() : r.error), true);
        return false;
    }
    if (r.canceled) {
        Log(opts, "Build canceled.");
        return false;
    }
    return r.exit_code == 0;
}

// -----------------------------------------------------------------------
// Android export script finder
// -----------------------------------------------------------------------

fs::path FindAndroidExportScript(const std::string& engine_root) {
    std::error_code ec;
    if (!engine_root.empty()) {
        fs::path candidate = fs::path(engine_root) / "scripts" / "export_android_apk.ps1";
        if (fs::exists(candidate, ec)) return candidate;
    }
    // Fallback: search upward from cwd
    fs::path dir = fs::current_path(ec);
    for (int i = 0; i < 8 && !dir.empty(); ++i) {
        fs::path candidate = dir / "scripts" / "export_android_apk.ps1";
        if (fs::exists(candidate, ec)) return candidate;
        fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

// -----------------------------------------------------------------------
// Desktop build (Windows / Linux)
// -----------------------------------------------------------------------

BuildResult BuildDesktop(const BuildOptions& opts) {
    BuildResult result;
    fs::path out_dir(opts.output_dir);
    std::error_code ec;

    Log(opts, "=== Build Game Started ===");

    fs::create_directories(out_dir, ec);
    if (ec) {
        result.error = "Cannot create output directory: " + ec.message();
        Log(opts, "ERROR: " + result.error, true);
        return result;
    }
    result.output_dir = out_dir.string();

    // Determine exe extension
    std::string exe_ext = (opts.target == BuildTarget::Windows) ? ".exe" : "";
    std::string game_exe_name = opts.game_title + exe_ext;

    // Locate runtime exe
    std::string runtime_path = opts.runtime_exe_path;
    if (runtime_path.empty()) {
        // Search relative to engine root or cwd
        fs::path search_dir = !opts.engine_root.empty()
            ? fs::path(opts.engine_root)
            : fs::current_path(ec);
        runtime_path = FindRuntimeExe(search_dir.string());
    }
    if (runtime_path.empty()) {
        result.error = "Cannot find dsengine_game executable. Build the 'dse_standalone' target first.";
        Log(opts, "ERROR: " + result.error, true);
        return result;
    }
    Log(opts, "Found runtime: " + runtime_path);

    // Copy runtime exe
    fs::path exe_path(runtime_path);
    fs::path dest_exe = out_dir / game_exe_name;
    fs::copy_file(exe_path, dest_exe, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        result.error = "Failed to copy exe: " + ec.message();
        Log(opts, "ERROR: " + result.error, true);
        return result;
    }
    Log(opts, "Copied exe -> " + dest_exe.string());
    result.primary_artifact = dest_exe.string();

    // Copy DLLs (Windows only)
    int dll_count = 0;
    fs::path exe_dir = exe_path.parent_path();
    for (const auto& entry : fs::directory_iterator(exe_dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".dll") {
            fs::path dest_dll = out_dir / entry.path().filename();
            fs::copy_file(entry.path(), dest_dll, fs::copy_options::overwrite_existing, ec);
            if (!ec) ++dll_count;
        }
    }
    if (dll_count > 0) Log(opts, "Copied " + std::to_string(dll_count) + " DLLs");

    // Write manifest
    WriteGameManifest(opts, out_dir);

    // --- Encryption path (game.bun) ---
    if (opts.encrypt) {
        if (opts.encrypt_key.size() < 16) {
            result.error = "Encryption key must be at least 16 characters";
            Log(opts, "ERROR: " + result.error, true);
            return result;
        }

        fs::path project_root(opts.project_root);
        fs::path staging = out_dir / ".dse_stage";
        fs::remove_all(staging, ec);
        fs::create_directories(staging, ec);
        for (const char* sub : {"scripts", "scenes", "assets"}) {
            fs::path src = project_root / sub;
            if (fs::exists(src, ec)) {
                fs::copy(src, staging / sub,
                         fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
            }
        }
        if (fs::exists(project_root / "project.dseproj", ec)) {
            fs::copy_file(project_root / "project.dseproj", staging / "project.dseproj",
                          fs::copy_options::overwrite_existing, ec);
        }

        fs::path bundle_path = out_dir / "game.bun";
        Log(opts, "Packing encrypted bundle " + bundle_path.string() + " ...");
        bool ok = dse::assets::PackDirectoryToBundle(staging.string(), bundle_path.string(), opts.encrypt_key);
        fs::remove_all(staging, ec);
        if (!ok) {
            result.error = "Failed to write encrypted bundle";
            Log(opts, "ERROR: " + result.error, true);
            return result;
        }

        std::string entry = opts.entry_script;
        if (entry.empty()) entry = "scripts/main.lua";
        result.launch_args = "--bundle=game.bun --key=" + opts.encrypt_key + " --script=" + entry;

        // Write launch.bat
        {
            std::ofstream bat(out_dir / "launch.bat", std::ios::trunc);
            bat << "@echo off\r\n";
            bat << "cd /d \"%~dp0\"\r\n";
            bat << "\"" << game_exe_name << "\" " << result.launch_args << "\r\n";
        }

        auto bun_sz = fs::file_size(bundle_path, ec);
        if (!ec) {
            result.total_bytes += bun_sz;
            double mb = static_cast<double>(bun_sz) / (1024.0 * 1024.0);
            char buf[64];
            snprintf(buf, sizeof(buf), "Encrypted bundle size: %.2f MB", mb);
            Log(opts, buf);
        }
        Log(opts, "Generated launch.bat (passes --bundle/--key/--script)");
        Log(opts, "=== Build Complete (encrypted) ===");
        result.ok = true;
        return result;
    }

    // --- Plaintext path (game.dpak + loose scripts) ---

    // Copy project scripts/ and scenes/ for double-click run
    if (!opts.project_root.empty()) {
        fs::path project_root(opts.project_root);
        for (const char* sub : {"scripts", "scenes"}) {
            fs::path src = project_root / sub;
            if (fs::exists(src, ec) && fs::is_directory(src, ec)) {
                fs::copy(src, out_dir / sub,
                         fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
                if (!ec) Log(opts, std::string("Copied ") + sub + "/ (loose, for double-click run)");
            }
        }
    }

    // Pack data directory
    fs::path data_dir;
    if (!opts.asset_dir.empty() && fs::exists(opts.asset_dir) && fs::is_directory(opts.asset_dir)) {
        data_dir = fs::canonical(opts.asset_dir);
    }
    if (data_dir.empty()) {
        Log(opts, "WARNING: No data directory found, skipping asset packing");
    } else {
        Log(opts, "Data root: " + data_dir.string());

        std::vector<std::string> files_to_pack;
        if (opts.pack_all_data) {
            files_to_pack = dse::pak::CollectDirectoryFiles(data_dir.string());
            Log(opts, "Packing all data: " + std::to_string(files_to_pack.size()) + " files");
        }

        if (!files_to_pack.empty()) {
            fs::path pak_path = out_dir / "game.dpak";
            Log(opts, "Writing " + pak_path.string() + " ...");
            bool ok = dse::pak::WriteDpak(pak_path.string(), data_dir.string(), files_to_pack);
            if (ok) {
                Log(opts, "Pak written successfully");
                auto sz = fs::file_size(pak_path, ec);
                if (!ec) {
                    result.total_bytes += sz;
                    double mb = static_cast<double>(sz) / (1024.0 * 1024.0);
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Pak size: %.1f MB", mb);
                    Log(opts, buf);
                }
            } else {
                result.error = "Failed to write pak";
                Log(opts, "ERROR: " + result.error, true);
                return result;
            }
        }
    }

    Log(opts, "=== Build Complete ===");
    Log(opts, "Output: " + out_dir.string());
    result.ok = true;
    return result;
}

// -----------------------------------------------------------------------
// Web build
// -----------------------------------------------------------------------

BuildResult BuildWeb(const BuildOptions& opts) {
    BuildResult result;
    fs::path out_dir(opts.output_dir);
    std::error_code ec;

    Log(opts, "=== Web Build Started ===");

    fs::create_directories(out_dir, ec);
    if (ec) {
        result.error = "Cannot create output directory: " + ec.message();
        Log(opts, "ERROR: " + result.error, true);
        return result;
    }
    result.output_dir = out_dir.string();

    // Toolchain check
    dse::project::EmscriptenStatus em = dse::project::DetectEmscripten();
    if (!em.available) {
        result.error = em.detail;
        Log(opts, "ERROR: " + em.detail, true);
        Log(opts, "  " + em.install_hint, true);
        return result;
    }
    Log(opts, em.detail);

    // Locate repo root
    std::string repo = opts.engine_root;
    if (repo.empty()) {
        repo = FindEngineRoot(fs::current_path(ec).string());
    }
    if (repo.empty()) {
        result.error = "CMakePresets.json not found; run from the engine repository.";
        Log(opts, "ERROR: " + result.error, true);
        return result;
    }
    Log(opts, "Repo root: " + repo);

    // Configure + build + collect artifacts
    dse::project::WebBuildOptions wopts;
    wopts.source_dir = repo;
    wopts.preset = opts.web_preset;
    wopts.debug = (opts.mode == BuildMode::Debug);
    wopts.enable_3d = opts.web_enable_3d;
    wopts.collect_artifacts = true;
    wopts.dist_dir = out_dir.string();
    wopts.cancel = opts.cancel;
    wopts.on_line = [&opts](const std::string& line, bool /*is_err*/) {
        Log(opts, line);
    };
    dse::project::WebBuildResult wres = dse::project::RunWebBuild(wopts);
    if (!wres.ok) {
        result.canceled = wres.canceled;
        result.error = wres.canceled ? "Web build canceled." : wres.error;
        Log(opts, result.error, true);
        return result;
    }
    Log(opts, "Collected " + std::to_string(wres.artifacts.size()) +
              " web artifact(s) -> " + wres.artifact_dir);

    WriteGameManifest(opts, out_dir);

    result.primary_artifact = out_dir.string();
    result.total_bytes = wres.artifact_bytes;
    result.file_count = static_cast<int>(wres.artifacts.size());
    Log(opts, "=== Web Export Complete ===");
    result.ok = true;
    return result;
}

// -----------------------------------------------------------------------
// Android build
// -----------------------------------------------------------------------

BuildResult BuildAndroid(const BuildOptions& opts) {
    BuildResult result;
    fs::path out_dir(opts.output_dir);
    std::error_code ec;

    Log(opts, "=== Build Game Started (Android) ===");

    fs::create_directories(out_dir, ec);
    if (ec) {
        result.error = "Cannot create output directory: " + ec.message();
        Log(opts, "ERROR: " + result.error, true);
        return result;
    }
    result.output_dir = out_dir.string();
    result.primary_artifact = (out_dir / (opts.game_title + ".apk")).string();

    if (opts.project_root.empty()) {
        result.error = "Android build requires a project root";
        Log(opts, "ERROR: " + result.error, true);
        return result;
    }
    if (opts.encrypt && opts.encrypt_key.size() < 16) {
        result.error = "Encryption key must be at least 16 characters";
        Log(opts, "ERROR: " + result.error, true);
        return result;
    }

    fs::path script = FindAndroidExportScript(opts.engine_root);
    if (script.empty()) {
        result.error = "Cannot find scripts/export_android_apk.ps1";
        Log(opts, "ERROR: " + result.error, true);
        return result;
    }

    // Stage assets
    fs::path assets_stage = out_dir / ".dse_android_assets";
    fs::remove_all(assets_stage, ec);
    fs::create_directories(assets_stage, ec);

    WriteGameManifest(opts, assets_stage);

    fs::path project_root(opts.project_root);
    fs::path pack_stage = out_dir / ".dse_stage";
    fs::remove_all(pack_stage, ec);
    fs::create_directories(pack_stage, ec);
    for (const char* sub : {"scripts", "scenes", "assets"}) {
        fs::path src = project_root / sub;
        if (fs::exists(src, ec)) {
            fs::copy(src, pack_stage / sub,
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        }
    }
    if (fs::exists(project_root / "project.dseproj", ec)) {
        fs::copy_file(project_root / "project.dseproj", pack_stage / "project.dseproj",
                      fs::copy_options::overwrite_existing, ec);
    }

    std::string entry = opts.entry_script;
    if (entry.empty()) entry = "scripts/main.lua";

    bool packed = false;
    if (opts.encrypt) {
        fs::path bundle_path = assets_stage / "game.bun";
        Log(opts, "Packing encrypted bundle game.bun ...");
        packed = dse::assets::PackDirectoryToBundle(pack_stage.string(), bundle_path.string(), opts.encrypt_key);
        if (packed) {
            std::ofstream cfg(assets_stage / "launch.cfg", std::ios::trunc);
            cfg << "bundle=game.bun\n" << "key=" << opts.encrypt_key << "\n" << "script=" << entry << "\n";
        }
    } else {
        auto files = dse::pak::CollectDirectoryFiles(pack_stage.string());
        Log(opts, "Packing game.dpak: " + std::to_string(files.size()) + " files");
        packed = !files.empty() &&
                 dse::pak::WriteDpak((assets_stage / "game.dpak").string(),
                                     pack_stage.string(), files);
    }
    fs::remove_all(pack_stage, ec);
    if (!packed) {
        result.error = "Failed to pack game assets";
        Log(opts, "ERROR: " + result.error, true);
        return result;
    }

    // Run export script
    fs::path out_apk = out_dir / (opts.game_title + ".apk");
    std::vector<std::string> args = {
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script.string(),
        "-GameTitle", opts.game_title,
        "-PackageId", opts.android_package_id,
        "-OutApk", out_apk.string(),
        "-AssetsDir", assets_stage.string(),
        "-ApiLevel", std::to_string(opts.android_api_level),
        "-Config", (opts.mode == BuildMode::Release ? "Release" : "Debug"),
    };
    if (!opts.android_keystore.empty()) {
        args.push_back("-Keystore");     args.push_back(opts.android_keystore);
        args.push_back("-KeystorePass"); args.push_back(opts.android_keystore_pass);
        args.push_back("-KeyAlias");     args.push_back(opts.android_key_alias);
    }

    Log(opts, "Running export script (first run cross-compiles the engine; this can take a while)...");
    const bool ok = RunLoggedCommand(opts, "powershell", args);
    fs::remove_all(assets_stage, ec);

    if (!ok) {
        result.error = "Android export failed (see log above)";
        Log(opts, "ERROR: " + result.error, true);
        return result;
    }
    Log(opts, "=== Build Complete (Android) ===");
    Log(opts, "Output: " + out_apk.string());
    Log(opts, "Install: adb install -r \"" + out_apk.string() + "\"");
    result.ok = true;
    return result;
}

} // anonymous namespace

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

std::string FindEngineRoot(const std::string& start_dir) {
    std::error_code ec;
    fs::path dir(start_dir);
    if (dir.empty()) dir = fs::current_path(ec);
    for (int i = 0; i < 8 && !dir.empty(); ++i) {
        if (fs::exists(dir / "CMakePresets.json", ec)) return dir.string();
        fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

std::string FindRuntimeExe(const std::string& search_dir) {
    std::error_code ec;
    fs::path base(search_dir);
    if (base.empty()) base = fs::current_path(ec);

    // Try common names in search dir
    for (const auto& name : {"dsengine_game_release.exe", "dsengine_game.exe", "dsengine_game_debug.exe",
                              "dsengine_game_release", "dsengine_game", "dsengine_game_debug"}) {
        fs::path candidate = base / name;
        if (fs::exists(candidate, ec)) return candidate.string();
    }
    // Also check bin/ subdirectory
    for (const auto& name : {"dsengine_game_release.exe", "dsengine_game.exe",
                              "dsengine_game_release", "dsengine_game"}) {
        fs::path candidate = base / "bin" / name;
        if (fs::exists(candidate, ec)) return fs::canonical(candidate, ec).string();
    }
    // Also check ../bin/
    for (const auto& name : {"dsengine_game_release.exe", "dsengine_game.exe",
                              "dsengine_game_release", "dsengine_game"}) {
        fs::path candidate = base / ".." / "bin" / name;
        if (fs::exists(candidate, ec)) return fs::canonical(candidate, ec).string();
    }
    return {};
}

std::string ValidateBuildOptions(const BuildOptions& opts) {
    if (opts.game_title.empty()) return "Game title is required";
    if (opts.output_dir.empty()) return "Output directory is required";
    if (opts.encrypt) {
        if (opts.encrypt_key.size() < 16) return "Encryption key must be at least 16 characters";
    }
    if (opts.target == BuildTarget::Android) {
        if (opts.project_root.empty()) return "Android build requires a project root";
    }
    if (opts.target == BuildTarget::Web) {
        // engine_root can be auto-detected at build time
    }
    return {};  // all checks pass
}

BuildResult RunGameBuild(const BuildOptions& opts) {
    // Validate
    std::string validation_error = ValidateBuildOptions(opts);
    if (!validation_error.empty()) {
        BuildResult r;
        r.error = validation_error;
        if (opts.on_line) opts.on_line("ERROR: " + validation_error, true);
        return r;
    }

    // Check cancel before starting
    if (opts.cancel && opts.cancel->load()) {
        BuildResult r;
        r.canceled = true;
        r.error = "Build canceled before start";
        return r;
    }

    // Dispatch by platform
    switch (opts.target) {
        case BuildTarget::Windows:
        case BuildTarget::Linux:
            return BuildDesktop(opts);
        case BuildTarget::Web:
            return BuildWeb(opts);
        case BuildTarget::Android:
            return BuildAndroid(opts);
    }
    BuildResult r;
    r.error = "Unknown build target";
    return r;
}

} // namespace dse::project
