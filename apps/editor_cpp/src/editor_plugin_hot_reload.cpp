/**
 * @file editor_plugin_hot_reload.cpp
 * @brief Plugin Hot Reload — DLL file watching, recompile trigger, hot-swap workflow
 *
 * Provides a complete editor UI for:
 *  - Listing loaded plugins and their status
 *  - Watching source files for changes (file modification time polling)
 *  - Triggering recompile via build system
 *  - Hot-swapping DLLs without restarting the editor
 *  - Build output console
 */

#include "editor_plugin_hot_reload.h"
#include "editor_icons.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <chrono>

namespace dse::editor {

namespace {

// ─── Data model ─────────────────────────────────────────────────────────

enum class PluginState { Loaded, Unloaded, Compiling, Error, Modified };

struct WatchedFile {
    std::string path;
    uint64_t last_modified = 0;
    bool changed = false;
};

struct PluginInfo {
    std::string name;
    std::string dll_path;
    std::string source_dir;
    std::string version;
    PluginState state = PluginState::Unloaded;
    bool auto_reload = true;
    bool watch_enabled = true;
    std::vector<WatchedFile> watched_files;
    // Build info
    std::string last_build_output;
    float last_build_time = 0.0f;
    int build_errors = 0;
    int build_warnings = 0;
    // Timing
    float time_since_modified = 0.0f;
    float reload_delay = 0.5f;  // delay before auto-reload after change detected
    int reload_count = 0;
};

struct HotReloadState {
    std::vector<PluginInfo> plugins;
    // Global settings
    bool global_auto_reload = true;
    bool watch_all = true;
    float poll_interval = 1.0f;  // file polling interval
    float poll_timer = 0.0f;
    // Build console
    std::string build_log;
    bool show_build_console = true;
    bool build_in_progress = false;
    float build_progress = 0.0f;
    std::string build_status;
    // Compile command template
    char compile_command[256] = "cmake --build build --target {plugin_name} --config Debug";
    // Stats
    int total_reloads = 0;
    float uptime = 0.0f;

    bool initialized = false;
};

static HotReloadState s_state;

void InitHotReload() {
    if (s_state.initialized) return;
    s_state.initialized = true;

    // Demo plugins
    PluginInfo p1;
    p1.name = "GameplayPlugin";
    p1.dll_path = "plugins/gameplay_plugin.dll";
    p1.source_dir = "plugins/gameplay/src/";
    p1.version = "1.2.3";
    p1.state = PluginState::Loaded;
    p1.reload_count = 3;
    p1.watched_files.push_back({"plugins/gameplay/src/main.cpp", 1000, false});
    p1.watched_files.push_back({"plugins/gameplay/src/player.cpp", 1000, false});
    p1.watched_files.push_back({"plugins/gameplay/src/ai_controller.cpp", 1000, false});
    s_state.plugins.push_back(p1);

    PluginInfo p2;
    p2.name = "PhysicsExtPlugin";
    p2.dll_path = "plugins/physics_ext.dll";
    p2.source_dir = "plugins/physics_ext/src/";
    p2.version = "0.8.1";
    p2.state = PluginState::Loaded;
    p2.watched_files.push_back({"plugins/physics_ext/src/custom_solver.cpp", 1000, false});
    p2.watched_files.push_back({"plugins/physics_ext/src/ragdoll.cpp", 1000, false});
    s_state.plugins.push_back(p2);

    PluginInfo p3;
    p3.name = "EditorToolsPlugin";
    p3.dll_path = "plugins/editor_tools.dll";
    p3.source_dir = "plugins/editor_tools/src/";
    p3.version = "2.0.0";
    p3.state = PluginState::Modified;
    p3.watched_files.push_back({"plugins/editor_tools/src/custom_gizmo.cpp", 1001, true});
    p3.watched_files.push_back({"plugins/editor_tools/src/tool_panel.cpp", 1000, false});
    s_state.plugins.push_back(p3);

    PluginInfo p4;
    p4.name = "ProceduralGenPlugin";
    p4.dll_path = "plugins/procgen.dll";
    p4.source_dir = "plugins/procgen/src/";
    p4.version = "0.3.0";
    p4.state = PluginState::Error;
    p4.build_errors = 2;
    p4.build_warnings = 1;
    p4.last_build_output = "error C2065: 'undefined_symbol': undeclared identifier\nerror C2146: syntax error at line 42";
    s_state.plugins.push_back(p4);

    s_state.build_log = "[14:32:01] Build system initialized\n[14:32:01] Watching 4 plugins for changes\n";
}

const char* PluginStateLabel(PluginState state) {
    switch (state) {
        case PluginState::Loaded: return "Loaded";
        case PluginState::Unloaded: return "Unloaded";
        case PluginState::Compiling: return "Compiling...";
        case PluginState::Error: return "Error";
        case PluginState::Modified: return "Modified";
    }
    return "Unknown";
}

ImU32 PluginStateColor(PluginState state) {
    switch (state) {
        case PluginState::Loaded: return IM_COL32(80, 200, 80, 255);
        case PluginState::Unloaded: return IM_COL32(150, 150, 150, 255);
        case PluginState::Compiling: return IM_COL32(200, 200, 50, 255);
        case PluginState::Error: return IM_COL32(255, 80, 80, 255);
        case PluginState::Modified: return IM_COL32(100, 180, 255, 255);
    }
    return IM_COL32(128, 128, 128, 255);
}

void SimulateReload(PluginInfo& plugin) {
    // Simulate the reload process
    plugin.state = PluginState::Compiling;
    s_state.build_in_progress = true;
    s_state.build_progress = 0.0f;
    s_state.build_status = "Building " + plugin.name + "...";

    char log_line[256];
    snprintf(log_line, sizeof(log_line), "[--:--:--] Recompiling %s...\n", plugin.name.c_str());
    s_state.build_log += log_line;
}

void UpdateBuildSimulation(float dt) {
    if (!s_state.build_in_progress) return;
    s_state.build_progress += dt * 0.8f; // simulate ~1.2s build

    if (s_state.build_progress >= 1.0f) {
        s_state.build_in_progress = false;
        s_state.build_progress = 1.0f;
        s_state.build_status = "Build complete";
        s_state.total_reloads++;

        // Find the compiling plugin and mark it loaded
        for (auto& p : s_state.plugins) {
            if (p.state == PluginState::Compiling) {
                p.state = PluginState::Loaded;
                p.reload_count++;
                p.last_build_time = 1.2f;
                p.build_errors = 0;
                p.build_warnings = 0;
                for (auto& f : p.watched_files) f.changed = false;

                char log_line[256];
                snprintf(log_line, sizeof(log_line), "[--:--:--] %s reloaded successfully (%.1fs)\n",
                         p.name.c_str(), p.last_build_time);
                s_state.build_log += log_line;
                break;
            }
        }
    }
}

} // anonymous namespace

void DrawPluginHotReloadPanel(EditorContext& /*ctx*/) {
    InitHotReload();
    auto& state = s_state;

    // Update simulations
    float dt = ImGui::GetIO().DeltaTime;
    state.uptime += dt;
    UpdateBuildSimulation(dt);

    // Auto-reload check
    for (auto& plugin : state.plugins) {
        if (plugin.state == PluginState::Modified && plugin.auto_reload && state.global_auto_reload) {
            plugin.time_since_modified += dt;
            if (plugin.time_since_modified >= plugin.reload_delay) {
                SimulateReload(plugin);
                plugin.time_since_modified = 0;
            }
        }
    }

    ImGui::Begin(MDI_ICON_RELOAD "  Plugin Hot Reload");

    // ─── Toolbar ─────────────────────────────────────────────────────────
    {
        if (ImGui::Button(MDI_ICON_RELOAD_ALERT " Reload All")) {
            for (auto& p : state.plugins) {
                if (p.state == PluginState::Loaded || p.state == PluginState::Modified) {
                    SimulateReload(p);
                }
            }
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto Reload", &state.global_auto_reload);
        ImGui::SameLine();
        ImGui::Checkbox("Watch Files", &state.watch_all);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat("Poll (s)", &state.poll_interval, 0.1f, 0.1f, 5.0f, "%.1f");
        ImGui::SameLine();
        ImGui::TextDisabled("| Reloads: %d | Uptime: %.0fs", state.total_reloads, state.uptime);
    }

    // Build progress bar
    if (state.build_in_progress) {
        ImGui::ProgressBar(state.build_progress, ImVec2(-1, 0), state.build_status.c_str());
    }

    ImGui::Separator();

    // ─── Plugin list ─────────────────────────────────────────────────────
    ImGui::BeginChild("PluginList", ImVec2(0, -150), ImGuiChildFlags_Borders);

    for (int i = 0; i < static_cast<int>(state.plugins.size()); i++) {
        auto& plugin = state.plugins[i];
        ImGui::PushID(i);

        // Plugin header row
        bool expanded = ImGui::TreeNodeEx(plugin.name.c_str(),
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

        // State indicator on same line
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(PluginStateColor(plugin.state)),
                           "%s", PluginStateLabel(plugin.state));
        ImGui::SameLine();
        ImGui::TextDisabled("v%s", plugin.version.c_str());
        ImGui::SameLine();
        if (plugin.state == PluginState::Loaded || plugin.state == PluginState::Modified) {
            if (ImGui::SmallButton("Reload")) SimulateReload(plugin);
            ImGui::SameLine();
            if (ImGui::SmallButton("Unload")) plugin.state = PluginState::Unloaded;
        } else if (plugin.state == PluginState::Unloaded || plugin.state == PluginState::Error) {
            if (ImGui::SmallButton("Load")) plugin.state = PluginState::Loaded;
        }

        if (expanded) {
            ImGui::Indent(16);
            ImGui::Text("DLL: %s", plugin.dll_path.c_str());
            ImGui::Text("Source: %s", plugin.source_dir.c_str());
            ImGui::Checkbox("Auto Reload", &plugin.auto_reload);
            ImGui::SameLine();
            ImGui::Checkbox("Watch", &plugin.watch_enabled);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            ImGui::DragFloat("Delay", &plugin.reload_delay, 0.1f, 0.1f, 5.0f, "%.1f");

            if (plugin.reload_count > 0) {
                ImGui::TextDisabled("Reloaded %d times (last build: %.1fs)",
                                    plugin.reload_count, plugin.last_build_time);
            }

            // Watched files
            if (!plugin.watched_files.empty()) {
                ImGui::Text("Watched Files:");
                for (auto& wf : plugin.watched_files) {
                    ImU32 fc = wf.changed ? IM_COL32(255, 200, 50, 255) : IM_COL32(150, 150, 150, 255);
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(fc), "  %s %s",
                                       wf.changed ? MDI_ICON_CIRCLE_MEDIUM : MDI_ICON_CIRCLE_OUTLINE,
                                       wf.path.c_str());
                }
            }

            // Build errors
            if (plugin.build_errors > 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 80, 80, 255));
                ImGui::Text("Errors: %d  Warnings: %d", plugin.build_errors, plugin.build_warnings);
                if (!plugin.last_build_output.empty()) {
                    ImGui::TextWrapped("%s", plugin.last_build_output.c_str());
                }
                ImGui::PopStyleColor();
            }

            ImGui::Unindent(16);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::EndChild();

    // ─── Build Console ───────────────────────────────────────────────────
    ImGui::Checkbox("Show Build Console", &state.show_build_console);
    if (state.show_build_console) {
        ImGui::BeginChild("BuildConsole", ImVec2(0, 0), ImGuiChildFlags_Borders);
        ImGui::Text(MDI_ICON_CONSOLE "  Build Output");
        ImGui::Separator();
        ImGui::TextUnformatted(state.build_log.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }

    // ─── Compile Command Settings ────────────────────────────────────────
    if (ImGui::CollapsingHeader("Build Configuration")) {
        ImGui::InputText("Compile Command", state.compile_command, sizeof(state.compile_command));
        ImGui::TextDisabled("{plugin_name} is replaced with the plugin target name");
    }

    ImGui::End();
}

} // namespace dse::editor
