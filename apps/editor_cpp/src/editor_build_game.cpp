#include "editor_build_game.h"

#include "imgui.h"
#include "editor_icons.h"
#include "editor_file_dialog.h"
#include "editor_console_panel.h"
#include "editor_scene_tabs.h"

#include "engine/assets/pak_writer.h"
#include "engine/assets/asset_scanner.h"
#include "engine/base/debug.h"

#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <atomic>
#include <mutex>

namespace dse::editor {

namespace {

struct BuildState {
    char output_dir[512] = {};
    char game_title[128] = "My Game";
    bool pack_all_data = true;         // true = pack entire data/, false = scene-referenced only
    bool include_scene = true;

    // Build progress
    std::atomic<bool> building{false};
    std::atomic<bool> build_done{false};
    std::atomic<bool> build_success{false};
    std::mutex log_mutex;
    std::vector<std::string> build_log;
    std::thread build_thread;

    bool dialog_open = false;
};

BuildState& GetState() {
    static BuildState state;
    return state;
}

void AppendLog(BuildState& state, const std::string& msg) {
    std::lock_guard<std::mutex> lock(state.log_mutex);
    state.build_log.push_back(msg);
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

    // 1. Locate the standalone exe next to editor
    fs::path exe_path;
    try {
        fs::path editor_dir = fs::current_path();
        // Try common names
        for (const auto& name : {"DSEngine_Game_release.exe", "DSEngine_Game.exe", "DSEngine_Game_debug.exe"}) {
            fs::path candidate = editor_dir / name;
            if (fs::exists(candidate)) {
                exe_path = candidate;
                break;
            }
        }
        // Also check bin/ directory
        if (exe_path.empty()) {
            for (const auto& name : {"DSEngine_Game_release.exe", "DSEngine_Game.exe"}) {
                fs::path candidate = editor_dir / ".." / "bin" / name;
                if (fs::exists(candidate)) {
                    exe_path = fs::canonical(candidate);
                    break;
                }
            }
        }
    } catch (...) {}

    if (exe_path.empty()) {
        AppendLog(state, "ERROR: Cannot find DSEngine_Game executable.");
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

    // 3. Collect files to pack
    std::vector<std::string> files_to_pack;
    std::string data_root = "data";
    // Try to find data directory
    fs::path data_dir;
    for (const auto& candidate : {fs::current_path() / "data", exe_dir / "data", exe_dir / ".." / "data"}) {
        if (fs::exists(candidate) && fs::is_directory(candidate)) {
            data_dir = fs::canonical(candidate);
            break;
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

    AppendLog(state, "=== Build Complete ===");
    AppendLog(state, "Output: " + out_dir.string());
    state.build_success = true;
    state.build_done = true;
    state.building = false;
}

} // namespace

void OpenBuildGameDialog() {
    auto& state = GetState();
    state.dialog_open = true;
    state.build_done = false;
    state.build_log.clear();
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

        ImGui::Checkbox("Pack all data/", &state.pack_all_data);
        ImGui::SameLine();
        ImGui::TextDisabled("(uncheck = scene-referenced only)");

        ImGui::EndDisabled();

        ImGui::Separator();

        // --- Build button ---
        if (!busy && !state.build_done.load()) {
            bool can_build = state.output_dir[0] != '\0' && state.game_title[0] != '\0';
            ImGui::BeginDisabled(!can_build);
            if (ImGui::Button("Build", ImVec2(120, 0))) {
                state.building = true;
                state.build_done = false;
                state.build_success = false;
                state.build_log.clear();
                if (state.build_thread.joinable()) state.build_thread.join();
                state.build_thread = std::thread([&state]() { DoBuild(state); });
                state.build_thread.detach();
            }
            ImGui::EndDisabled();
        } else if (busy) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Building...");
        } else if (state.build_done.load()) {
            if (state.build_success.load()) {
                ImGui::TextColored(ImVec4(0.2f, 1, 0.2f, 1), "Build Succeeded!");
            } else {
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Build Failed");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Close")) {
                state.dialog_open = false;
                ImGui::CloseCurrentPopup();
            }
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
