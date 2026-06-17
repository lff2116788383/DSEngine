#include "editor_build_game.h"

#include "imgui.h"
#include "editor_icons.h"
#include "editor_file_dialog.h"
#include "editor_console_panel.h"
#include "editor_scene_tabs.h"
#include "editor_project.h"

#include "engine/assets/pak_writer.h"
#include "engine/assets/asset_scanner.h"
#include "engine/assets/bundle_packer.h"
#include "engine/runtime/app_manifest.h"
#include "engine/base/debug.h"

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

namespace dse::editor {

namespace {

enum class BuildPlatform { Windows, Linux, Web };
enum class BuildConfig { Release, Debug };

struct BuildState {
    char output_dir[512] = {};
    char game_title[128] = "My Game";
    bool pack_all_data = true;         // true = pack entire data/, false = scene-referenced only
    bool include_scene = true;
    BuildPlatform platform = BuildPlatform::Windows;
    BuildConfig config = BuildConfig::Release;
    bool compress_pak = true;
    bool encrypt = false;              // 加密为 game.bun（AES-128-CTR）而非明文 game.dpak
    char encrypt_key[65] = {};         // >=16 字符的 AES 密钥
    char icon_path[512] = {};

    // Build progress
    std::atomic<bool> building{false};
    std::atomic<bool> build_done{false};
    std::atomic<bool> build_success{false};
    std::mutex log_mutex;
    std::vector<std::string> build_log;
    std::thread build_thread;
    std::string last_output_dir;
    std::string last_exe_name;

    std::string last_launch_args;      // 加密构建时通过 launch 参数传 --bundle/--key/--script

    bool dialog_open = false;
    bool launch_after_build = false;
    float spinner_angle = 0.0f;
};

BuildState& GetState() {
    static BuildState state;
    return state;
}

void AppendLog(BuildState& state, const std::string& msg) {
    std::lock_guard<std::mutex> lock(state.log_mutex);
    state.build_log.push_back(msg);
}

// 在 exe 旁写一份松散的 game.dsmanifest（入口脚本 + 窗口 + 品牌化 splash），standalone
// 宿主在创建窗口/弹出 splash 之前读取，并在未显式传 --script 时用 entry_script 启动
// （与 dse CLI build 共用 WriteAppManifest，使编辑器出包同样“双击即玩”）。splash 图
// 必须松散放置（不能进 pak/bun），否则在挂载资源之前无法读取。
void WriteGameManifest(BuildState& state, const std::filesystem::path& out_dir) {
    namespace fs = std::filesystem;
    std::error_code ec;

    dse::runtime::AppManifest manifest;
    const std::string title = state.game_title;
    manifest.has_window_title = true;
    manifest.window_title = title;
    manifest.has_window_size = true;
    manifest.window_width = 1280;
    manifest.window_height = 720;

    // 入口脚本：取自当前打开项目的描述符，缺省回退 scripts/main.lua。
    {
        auto& proj = ProjectManager::Get();
        std::string entry = proj.HasOpenProject() ? proj.GetDescriptor().entry_script : std::string();
        if (entry.empty()) entry = "scripts/main.lua";
        manifest.entry_script = entry;
        manifest.has_entry_script = true;
    }

    // splash：保留编辑器既有的品牌化外观与时序（与旧手写清单一致）。
    manifest.has_splash = true;
    dse::platform::SplashConfig& cfg = manifest.splash;
    cfg.enabled = true;
    cfg.app_name = title;
    cfg.bg_argb = 0xFF1E1E28u;
    cfg.accent_argb = 0xFF4A9EFFu;
    cfg.fade_in_ms = 600;
    cfg.min_display_ms = 900;
    cfg.fade_out_ms = 500;

    if (state.icon_path[0] != '\0') {
        fs::path icon(state.icon_path);
        std::string ext = icon.extension().string();
        for (auto& ch : ext) ch = static_cast<char>(::tolower(static_cast<unsigned char>(ch)));
        const bool image_ok = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                               ext == ".bmp" || ext == ".tga");
        if (image_ok && fs::exists(icon, ec)) {
            fs::path dest = out_dir / ("splash" + ext);
            fs::copy_file(icon, dest, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                cfg.image_path = dest.filename().string();
                AppendLog(state, "Copied splash image -> " + dest.filename().string());
            }
        }
    }

    if (dse::runtime::WriteAppManifest((out_dir / "game.dsmanifest").string(), manifest)) {
        AppendLog(state, "Wrote game.dsmanifest (entry_script + window + splash)");
    } else {
        AppendLog(state, "WARNING: Failed to write game.dsmanifest");
    }
}

void DoBuild(BuildState& state) {
    namespace fs = std::filesystem;

    AppendLog(state, "=== Build Game Started ===");

    fs::path out_dir(state.output_dir);
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        AppendLog(state, "ERROR: Cannot create output directory: " + ec.message());
        state.build_success = false;
        state.build_done = true;
        state.building = false;
        return;
    }

    // Capture output info for later
    state.last_output_dir = out_dir.string();
    state.last_exe_name   = std::string(state.game_title) + ".exe";

    // 1. Locate the standalone exe next to editor
    fs::path exe_path;
    try {
        fs::path editor_dir = fs::current_path();
        // Try common names
        for (const auto& name : {"dsengine_game_release.exe", "dsengine_game.exe", "dsengine_game_debug.exe"}) {
            fs::path candidate = editor_dir / name;
            if (fs::exists(candidate)) {
                exe_path = candidate;
                break;
            }
        }
        // Also check bin/ directory
        if (exe_path.empty()) {
            for (const auto& name : {"dsengine_game_release.exe", "dsengine_game.exe"}) {
                fs::path candidate = editor_dir / ".." / "bin" / name;
                if (fs::exists(candidate)) {
                    exe_path = fs::canonical(candidate);
                    break;
                }
            }
        }
    } catch (...) {}

    if (exe_path.empty()) {
        AppendLog(state, "ERROR: Cannot find dsengine_game executable.");
        AppendLog(state, "  Build the 'dse_standalone' target first.");
        state.build_success = false;
        state.build_done = true;
        state.building = false;
        return;
    }
    AppendLog(state, "Found runtime: " + exe_path.string());

    // 2. Copy runtime exe + DLLs
    std::string game_exe_name = std::string(state.game_title) + ".exe";
    fs::path dest_exe = out_dir / game_exe_name;
    fs::copy_file(exe_path, dest_exe, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        AppendLog(state, "ERROR: Failed to copy exe: " + ec.message());
        state.build_success = false;
        state.build_done = true;
        state.building = false;
        return;
    }
    AppendLog(state, "Copied exe -> " + dest_exe.string());

    // Copy DLLs from same directory as exe
    fs::path exe_dir = exe_path.parent_path();
    int dll_count = 0;
    for (const auto& entry : fs::directory_iterator(exe_dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".dll") {
            fs::path dest_dll = out_dir / entry.path().filename();
            fs::copy_file(entry.path(), dest_dll, fs::copy_options::overwrite_existing, ec);
            if (!ec) ++dll_count;
        }
    }
    AppendLog(state, "Copied " + std::to_string(dll_count) + " DLLs");

    // 写启动清单（窗口 + 品牌化 splash），两种打包路径（明文/加密）都需要。
    WriteGameManifest(state, out_dir);

    state.last_launch_args.clear();

    // 3a. 端到端加密构建：把项目 scripts/scenes/assets 暂存后打包成加密 game.bun。
    //     与 dse CLI build / 运行时 MountBundle 共用同一 PackDirectoryToBundle，磁盘不留明文。
    if (state.encrypt) {
        auto& proj = ProjectManager::Get();
        std::string key(state.encrypt_key);
        if (!proj.HasOpenProject()) {
            AppendLog(state, "ERROR: Encrypted build requires an open project");
            state.build_success = false; state.build_done = true; state.building = false; return;
        }
        if (key.size() < 16) {
            AppendLog(state, "ERROR: Encryption key must be at least 16 characters");
            state.build_success = false; state.build_done = true; state.building = false; return;
        }

        fs::path project_root = proj.GetProjectRoot();
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
        AppendLog(state, "Packing encrypted bundle " + bundle_path.string() + " ...");
        bool ok = dse::assets::PackDirectoryToBundle(staging.string(), bundle_path.string(), key);
        fs::remove_all(staging, ec);
        if (!ok) {
            AppendLog(state, "ERROR: Failed to write encrypted bundle");
            state.build_success = false; state.build_done = true; state.building = false; return;
        }

        std::string entry = proj.GetDescriptor().entry_script;
        if (entry.empty()) entry = "scripts/main.lua";
        std::string launch_args = "--bundle=game.bun --key=" + key + " --script=" + entry;
        state.last_launch_args = launch_args;
        {
            std::ofstream bat(out_dir / "launch.bat", std::ios::trunc);
            bat << "@echo off\r\n";
            bat << "cd /d \"%~dp0\"\r\n";
            bat << "\"" << game_exe_name << "\" " << launch_args << "\r\n";
        }

        auto bun_sz = fs::file_size(bundle_path, ec);
        if (!ec) {
            double mb = static_cast<double>(bun_sz) / (1024.0 * 1024.0);
            char buf[64];
            snprintf(buf, sizeof(buf), "Encrypted bundle size: %.2f MB", mb);
            AppendLog(state, buf);
        }
        AppendLog(state, "Generated launch.bat (passes --bundle/--key/--script)");
        AppendLog(state, "=== Build Complete (encrypted) ===");
        AppendLog(state, "Output: " + out_dir.string());
        state.build_success = true;
        state.build_done = true;
        state.building = false;
        return;
    }

    // 3. Collect files to pack
    std::vector<std::string> files_to_pack;
    // Prefer the open project's asset directory
    fs::path data_dir;
    {
        auto& proj = ProjectManager::Get();
        if (proj.HasOpenProject()) {
            fs::path pd = proj.GetAssetDir();
            if (fs::exists(pd) && fs::is_directory(pd)) {
                data_dir = fs::canonical(pd);
            }
        }
    }
    // Fallback: search relative to editor exe
    if (data_dir.empty()) {
        for (const auto& candidate : {fs::current_path() / "data", exe_dir / "data", exe_dir / ".." / "data"}) {
            if (fs::exists(candidate) && fs::is_directory(candidate)) {
                data_dir = fs::canonical(candidate);
                break;
            }
        }
    }

    if (data_dir.empty()) {
        AppendLog(state, "WARNING: No 'data' directory found, skipping asset packing");
    } else {
        AppendLog(state, "Data root: " + data_dir.string());

        if (state.pack_all_data) {
            files_to_pack = dse::pak::CollectDirectoryFiles(data_dir.string());
            AppendLog(state, "Packing all data: " + std::to_string(files_to_pack.size()) + " files");
        } else {
            // Scene-referenced only
            auto& tab_mgr = SceneTabManager::Get();
            std::string scene_path = tab_mgr.GetActiveFilePath();
            if (!scene_path.empty()) {
                auto refs = dse::pak::ScanSceneAssetPaths(scene_path);
                AppendLog(state, "Scene references " + std::to_string(refs.size()) + " assets");
                for (const auto& ref : refs) {
                    fs::path full = data_dir / ref;
                    if (fs::exists(full)) {
                        files_to_pack.push_back(full.string());
                    } else {
                        AppendLog(state, "  MISSING: " + ref);
                    }
                }
                // Always include the scene file itself
                files_to_pack.push_back(scene_path);
            } else {
                AppendLog(state, "WARNING: No active scene to scan");
            }
        }

        // 4. Write .dpak
        if (!files_to_pack.empty()) {
            fs::path pak_path = out_dir / "game.dpak";
            AppendLog(state, "Writing " + pak_path.string() + " ...");
            bool ok = dse::pak::WriteDpak(pak_path.string(), data_dir.string(), files_to_pack);
            if (ok) {
                AppendLog(state, "Pak written successfully");
            } else {
                AppendLog(state, "ERROR: Failed to write pak");
                state.build_success = false;
                state.build_done = true;
                state.building = false;
                return;
            }
        }
    }

    // 5. Copy data/ directory as fallback (loose files)
    fs::path dest_data = out_dir / "data";
    if (!data_dir.empty() && !fs::exists(dest_data)) {
        AppendLog(state, "Copying data/ directory as fallback...");
        fs::copy(data_dir, dest_data, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        if (ec) {
            AppendLog(state, "WARNING: Some files failed to copy: " + ec.message());
        }
    }

    // 5b. 明文出包：把项目 scripts/ 与 scenes/ 松散拷到 exe 旁，使 game.dsmanifest 的
    //     entry_script（如 scripts/main.lua）在磁盘上可直接解析——双击 exe 即玩，
    //     无需 launch.bat（与 dse CLI build 的明文路径行为一致）。
    {
        auto& proj = ProjectManager::Get();
        if (proj.HasOpenProject()) {
            fs::path project_root = proj.GetProjectRoot();
            for (const char* sub : {"scripts", "scenes"}) {
                fs::path src = project_root / sub;
                if (fs::exists(src, ec) && fs::is_directory(src, ec)) {
                    fs::copy(src, out_dir / sub,
                             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
                    if (!ec) AppendLog(state, std::string("Copied ") + sub + "/ (loose, for double-click run)");
                }
            }
        }
    }

    // Report final pak size
    fs::path pak_path_final = out_dir / "game.dpak";
    if (fs::exists(pak_path_final)) {
        auto sz = fs::file_size(pak_path_final, ec);
        if (!ec) {
            double mb = static_cast<double>(sz) / (1024.0 * 1024.0);
            char buf[64];
            snprintf(buf, sizeof(buf), "Pak size: %.1f MB", mb);
            AppendLog(state, buf);
        }
    }

    AppendLog(state, "=== Build Complete ===");
    AppendLog(state, "Output: " + out_dir.string());
    state.build_success = true;
    state.build_done = true;
    state.building = false;
}

} // namespace

static void OpenInExplorer(const std::string& path) {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "explore", path.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#endif
}

static void LaunchExe(const std::string& dir, const std::string& exe_name, const std::string& args = "") {
#if defined(_WIN32)
    std::filesystem::path full = std::filesystem::path(dir) / exe_name;
    ShellExecuteA(nullptr, "open", full.string().c_str(),
                  args.empty() ? nullptr : args.c_str(), dir.c_str(), SW_SHOWDEFAULT);
#endif
}

void OpenBuildGameDialog() {
    auto& state = GetState();
    state.dialog_open = true;
    state.build_done = false;
    state.build_log.clear();
    state.launch_after_build = false;
    ImGui::OpenPopup("Build Game");
}

void DrawBuildGameDialog() {
    auto& state = GetState();

    if (!state.dialog_open) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Build Game", &state.dialog_open, ImGuiWindowFlags_NoResize)) {
        bool busy = state.building.load();

        // --- Settings ---
        ImGui::BeginDisabled(busy);

        ImGui::Text("Game Title:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##title", state.game_title, sizeof(state.game_title));

        ImGui::Text("Output Dir:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(340);
        ImGui::InputText("##outdir", state.output_dir, sizeof(state.output_dir));
        ImGui::SameLine();
        if (ImGui::SmallButton("...")) {
            std::string folder = BrowseFolderDialog("Select Build Output Folder");
            if (!folder.empty()) {
                strncpy(state.output_dir, folder.c_str(), sizeof(state.output_dir) - 1);
            }
        }

        // Platform
        ImGui::Text("Platform:  ");
        ImGui::SameLine();
        const char* platform_names[] = {"Windows", "Linux", "Web (Emscripten)"};
        int plat_idx = static_cast<int>(state.platform);
        ImGui::SetNextItemWidth(160);
        if (ImGui::Combo("##platform", &plat_idx, platform_names, 3)) {
            state.platform = static_cast<BuildPlatform>(plat_idx);
        }

        // Config
        ImGui::Text("Config:    ");
        ImGui::SameLine();
        const char* config_names[] = {"Release", "Debug"};
        int cfg_idx = static_cast<int>(state.config);
        ImGui::SetNextItemWidth(100);
        if (ImGui::Combo("##config", &cfg_idx, config_names, 2)) {
            state.config = static_cast<BuildConfig>(cfg_idx);
        }

        ImGui::Checkbox("Pack all data/", &state.pack_all_data);
        ImGui::SameLine();
        ImGui::Checkbox("Compress", &state.compress_pak);

        // 加密：勾选后产出加密 game.bun（端到端，运行时用同一 key 解密挂载）
        ImGui::Checkbox("Encrypt (AES-128 -> game.bun)", &state.encrypt);
        if (state.encrypt) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("##enckey", state.encrypt_key, sizeof(state.encrypt_key),
                             ImGuiInputTextFlags_Password);
            ImGui::SameLine();
            ImGui::TextDisabled("(key >= 16 chars)");
        }

        // Icon (Windows only)
        if (state.platform == BuildPlatform::Windows) {
            ImGui::Text("Icon (.ico):");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(340);
            ImGui::InputText("##icon", state.icon_path, sizeof(state.icon_path));
            ImGui::SameLine();
            ImGui::TextDisabled("(optional)");
        }

        ImGui::EndDisabled();

        ImGui::Separator();

        // --- Build buttons ---
        auto do_build = [&state](bool launch) {
            state.building = true;
            state.build_done = false;
            state.build_success = false;
            state.build_log.clear();
            state.launch_after_build = launch;
            if (state.build_thread.joinable()) state.build_thread.join();
            state.build_thread = std::thread([&state]() { DoBuild(state); });
            state.build_thread.detach();
        };

        if (!busy) {
            bool key_ok = !state.encrypt || std::string(state.encrypt_key).size() >= 16;
            bool can_build = state.output_dir[0] != '\0' && state.game_title[0] != '\0' && key_ok;
            ImGui::BeginDisabled(!can_build);

            const char* build_label = state.build_done.load() ? "Rebuild" : "Build";
            if (ImGui::Button(build_label, ImVec2(100, 0))) { do_build(false); }
            ImGui::SameLine();
            if (ImGui::Button("Build & Run", ImVec2(100, 0))) { do_build(true); }
            ImGui::EndDisabled();

            if (state.build_done.load()) {
                ImGui::SameLine();
                if (state.build_success.load()) {
                    ImGui::TextColored(ImVec4(0.2f, 1, 0.2f, 1), "  Build Succeeded!");
                    ImGui::SameLine();
                    if (ImGui::SmallButton(MDI_ICON_FOLDER_OPEN " Open Folder")) {
                        OpenInExplorer(state.last_output_dir);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton(MDI_ICON_PLAY " Run")) {
                        LaunchExe(state.last_output_dir, state.last_exe_name, state.last_launch_args);
                    }
                } else {
                    ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "  Build Failed");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Close")) {
                    state.dialog_open = false;
                    ImGui::CloseCurrentPopup();
                }
            }
        } else {
            // Spinning progress indicator
            state.spinner_angle += ImGui::GetIO().DeltaTime * 4.0f;
            if (state.spinner_angle > 6.2832f) state.spinner_angle -= 6.2832f;
            const char* spinners[] = {"|" , "/", "-", "\\"};
            int idx = static_cast<int>(state.spinner_angle / 6.2832f * 4.0f) % 4;
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s  Building...", spinners[idx]);
        }

        // Auto-launch after successful build
        if (state.build_done.load() && state.build_success.load() && state.launch_after_build) {
            state.launch_after_build = false;
            LaunchExe(state.last_output_dir, state.last_exe_name, state.last_launch_args);
        }

        // --- Log ---
        ImGui::Separator();
        ImGui::BeginChild("BuildLog", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None);
        {
            std::lock_guard<std::mutex> lock(state.log_mutex);
            for (const auto& line : state.build_log) {
                bool is_err = line.find("ERROR") != std::string::npos;
                bool is_warn = line.find("WARNING") != std::string::npos;
                if (is_err) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));
                else if (is_warn) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.8f, 0.2f, 1));
                ImGui::TextWrapped("%s", line.c_str());
                if (is_err || is_warn) ImGui::PopStyleColor();
            }
        }
        if (busy) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        ImGui::EndPopup();
    }
}

} // namespace dse::editor
