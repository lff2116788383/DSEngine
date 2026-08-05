/**
 * @file editor_plugin_hot_reload.cpp
 * @brief Plugin Hot Reload — native plugin DLL discovery, source watching,
 *        real recompile (via the unified process/task service) and hot-swap.
 *
 * Plugins are discovered from a plugins directory: each subdirectory that
 * contains a built `.dll` is treated as a native plugin. Source files under the
 * plugin's `src/` directory are watched via real filesystem modification times;
 * a change marks the plugin Modified. Reload runs the configured compile command
 * through dse::platform::RunProcess on the BackgroundTaskService and, on a
 * successful exit code, reloads the DLL via dse::core::DynamicLibrary. There is
 * no simulated build progress or demo data.
 */

#include "editor_plugin_hot_reload.h"
#include "editor_icons.h"
#include "editor_task_service.h"
#include "editor_panel_registry.h"

#include "engine/core/dynamic_library.h"
#include "engine/platform/process.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace dse::editor {

namespace {

namespace fs = std::filesystem;

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
    // Live DLL handle (null until loaded).
    std::shared_ptr<dse::core::DynamicLibrary> library;
    // Set by the completion callback of an in-flight reload build.
    std::shared_ptr<std::atomic<bool>> build_active;
};

struct HotReloadState {
    std::vector<PluginInfo> plugins;
    std::string plugins_dir = "plugins";
    // Global settings
    bool global_auto_reload = true;
    bool watch_all = true;
    float poll_interval = 1.0f;  // file polling interval
    float poll_timer = 0.0f;
    // Build console
    std::string build_log;
    bool show_build_console = true;
    bool build_in_progress = false;
    std::string build_status;
    // Compile command template
    char compile_command[256] = "cmake --build out/build/windows-x64-debug --target {plugin_name}";
    // Stats
    int total_reloads = 0;
    float uptime = 0.0f;

    bool initialized = false;
};

static HotReloadState s_state;

uint64_t FileMtime(const fs::path& p) {
    std::error_code ec;
    auto t = fs::last_write_time(p, ec);
    if (ec) return 0;
    return static_cast<uint64_t>(t.time_since_epoch().count());
}

void AppendLog(const std::string& line) {
    std::time_t now = std::time(nullptr);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    char stamp[16];
    std::strftime(stamp, sizeof(stamp), "[%H:%M:%S] ", &tm_buf);
    s_state.build_log += stamp;
    s_state.build_log += line;
    if (s_state.build_log.empty() || s_state.build_log.back() != '\n') s_state.build_log += '\n';
}

// Collect watchable source files under a plugin's src directory.
void CollectWatchedFiles(PluginInfo& plugin) {
    plugin.watched_files.clear();
    std::error_code ec;
    fs::path src(plugin.source_dir);
    if (plugin.source_dir.empty() || !fs::is_directory(src, ec)) return;
    for (auto it = fs::recursive_directory_iterator(src, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".h" ||
            ext == ".hpp" || ext == ".hxx") {
            WatchedFile wf;
            wf.path = it->path().string();
            wf.last_modified = FileMtime(it->path());
            wf.changed = false;
            plugin.watched_files.push_back(std::move(wf));
        }
    }
}

// Discover native plugins: any subdirectory of the plugins dir containing a
// built `.dll` (searched in the directory and a `bin/` subfolder).
void ScanPlugins() {
    s_state.plugins.clear();
    std::error_code ec;
    fs::path root(s_state.plugins_dir);
    if (!fs::is_directory(root, ec)) return;

    for (auto it = fs::directory_iterator(root, ec);
         !ec && it != fs::directory_iterator(); it.increment(ec)) {
        if (!it->is_directory(ec)) continue;
        const fs::path dir = it->path();

        fs::path dll;
        for (const fs::path& search : {dir, dir / "bin"}) {
            std::error_code sec;
            if (!fs::is_directory(search, sec)) continue;
            for (auto f = fs::directory_iterator(search, sec);
                 !sec && f != fs::directory_iterator(); f.increment(sec)) {
                if (f->is_regular_file(sec) && f->path().extension() == ".dll") {
                    dll = f->path();
                    break;
                }
            }
            if (!dll.empty()) break;
        }
        if (dll.empty()) continue;  // not a native plugin

        PluginInfo plugin;
        plugin.name = dir.filename().string();
        plugin.dll_path = dll.string();
        fs::path src = dir / "src";
        if (fs::is_directory(src, ec)) plugin.source_dir = src.string();
        CollectWatchedFiles(plugin);

        // Attempt to load the DLL to reflect a real Loaded/Error state.
        auto lib = std::make_shared<dse::core::DynamicLibrary>();
        if (lib->Load(dll.string())) {
            plugin.library = lib;
            plugin.state = PluginState::Loaded;
        } else {
            plugin.state = PluginState::Error;
            plugin.last_build_output = "Failed to load " + dll.string();
        }
        s_state.plugins.push_back(std::move(plugin));
    }

    std::ostringstream os;
    os << "Scanned '" << s_state.plugins_dir << "': found "
       << s_state.plugins.size() << " native plugin(s)";
    AppendLog(os.str());
}

void InitHotReload() {
    if (s_state.initialized) return;
    s_state.initialized = true;
    ScanPlugins();
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

// Split the compile command template into an argv, substituting {plugin_name}.
std::vector<std::string> BuildCompileArgs(const std::string& tmpl, const std::string& plugin_name) {
    std::string cmd = tmpl;
    const std::string token = "{plugin_name}";
    for (size_t pos = cmd.find(token); pos != std::string::npos; pos = cmd.find(token, pos)) {
        cmd.replace(pos, token.size(), plugin_name);
        pos += plugin_name.size();
    }
    std::vector<std::string> args;
    std::istringstream iss(cmd);
    std::string tok;
    while (iss >> tok) args.push_back(tok);
    return args;
}

// Reload one plugin for real: run the configured compile command through the
// unified process service; on success, hot-swap the DLL via DynamicLibrary.
void ReloadPlugin(int index) {
    if (index < 0 || index >= static_cast<int>(s_state.plugins.size())) return;
    PluginInfo& plugin = s_state.plugins[index];
    if (plugin.build_active && plugin.build_active->load()) return;  // already building

    std::vector<std::string> argv = BuildCompileArgs(s_state.compile_command, plugin.name);
    if (argv.empty()) {
        AppendLog("Compile command is empty; cannot reload " + plugin.name);
        return;
    }

    plugin.state = PluginState::Compiling;
    plugin.time_since_modified = 0.0f;
    plugin.build_active = std::make_shared<std::atomic<bool>>(true);
    s_state.build_in_progress = true;
    s_state.build_status = "Building " + plugin.name + "...";
    AppendLog("Recompiling " + plugin.name + "...");

    const std::string exe = argv.front();
    std::vector<std::string> args(argv.begin() + 1, argv.end());
    const std::string name = plugin.name;
    const std::string dll_path = plugin.dll_path;
    auto build_active = plugin.build_active;
    auto exit_code = std::make_shared<std::atomic<int>>(-1);
    auto launched = std::make_shared<std::atomic<bool>>(false);
    auto elapsed = std::make_shared<std::atomic<float>>(0.0f);
    auto launch_err = std::make_shared<std::string>();

    BackgroundTaskService::Get().Submit(
        "Reload " + name,
        [exe, args, dll_path, exit_code, launched, launch_err, elapsed](TaskContext& ctx) -> bool {
            ctx.SetProgress(-1.0f, "compiling");
            const auto start = std::chrono::steady_clock::now();
            dse::platform::ProcessOptions opts;
            opts.executable = exe;
            opts.args = args;
            opts.merge_stderr = true;
            const std::atomic<bool>& cancel = ctx.CancelFlag();
            dse::platform::ProcessResult r = dse::platform::RunProcess(
                opts,
                [&ctx](std::string_view line, bool is_err) {
                    if (!line.empty()) ctx.Log(std::string(line), is_err);
                },
                std::chrono::milliseconds::zero(),
                &cancel);
            const auto end = std::chrono::steady_clock::now();
            elapsed->store(std::chrono::duration<float>(end - start).count());
            launched->store(r.launched);
            exit_code->store(r.exit_code);
            if (!r.launched) {
                *launch_err = r.error.empty() ? (exe + " not found on PATH") : r.error;
                ctx.Fail(*launch_err);
                return false;
            }
            if (r.canceled) return false;
            if (r.exit_code != 0) {
                ctx.Fail("compile exited with code " + std::to_string(r.exit_code));
                return false;
            }
            return true;
        },
        [index, name, dll_path, exit_code, launched, launch_err, elapsed, build_active](bool success) {
            build_active->store(false);
            s_state.build_in_progress = false;
            // The plugin vector may have been re-scanned; re-resolve by name.
            PluginInfo* p = nullptr;
            if (index >= 0 && index < static_cast<int>(s_state.plugins.size()) &&
                s_state.plugins[index].name == name) {
                p = &s_state.plugins[index];
            } else {
                for (auto& cand : s_state.plugins)
                    if (cand.name == name) { p = &cand; break; }
            }

            if (!success) {
                s_state.build_status = "Build failed";
                if (!launched->load())
                    AppendLog(name + " reload failed: " + *launch_err);
                else
                    AppendLog(name + " compile failed (exit " +
                              std::to_string(exit_code->load()) + ")");
                if (p) { p->state = PluginState::Error; p->build_errors = 1; }
                return;
            }

            s_state.build_status = "Build complete";
            s_state.total_reloads++;
            if (!p) return;
            p->last_build_time = elapsed->load();
            p->build_errors = 0;
            p->build_warnings = 0;
            for (auto& f : p->watched_files) {
                f.changed = false;
                f.last_modified = FileMtime(f.path);
            }
            // Hot-swap: drop the old handle, load the freshly built DLL.
            p->library.reset();
            auto lib = std::make_shared<dse::core::DynamicLibrary>();
            if (lib->Load(dll_path)) {
                p->library = lib;
                p->state = PluginState::Loaded;
                p->reload_count++;
                AppendLog(name + " reloaded successfully (" +
                          std::to_string(p->last_build_time) + "s)");
            } else {
                p->state = PluginState::Error;
                p->last_build_output = "Built but failed to load " + dll_path;
                AppendLog(p->last_build_output);
            }
        });
}

// Poll watched files for real modifications.
void PollWatchedFiles() {
    for (auto& plugin : s_state.plugins) {
        if (!plugin.watch_enabled || !s_state.watch_all) continue;
        bool any_changed = false;
        for (auto& wf : plugin.watched_files) {
            uint64_t m = FileMtime(wf.path);
            if (m != 0 && m != wf.last_modified) {
                wf.last_modified = m;
                wf.changed = true;
                any_changed = true;
            }
        }
        if (any_changed && plugin.state == PluginState::Loaded) {
            plugin.state = PluginState::Modified;
            plugin.time_since_modified = 0.0f;
        }
    }
}

} // anonymous namespace

void DrawPluginHotReloadPanel(EditorContext& /*ctx*/) {
    InitHotReload();
    auto& state = s_state;

    float dt = ImGui::GetIO().DeltaTime;
    state.uptime += dt;

    // Real file-modification polling on the configured interval.
    state.poll_timer += dt;
    if (state.poll_timer >= state.poll_interval) {
        state.poll_timer = 0.0f;
        PollWatchedFiles();
    }

    // Auto-reload after a change has settled.
    for (int i = 0; i < static_cast<int>(state.plugins.size()); ++i) {
        auto& plugin = state.plugins[i];
        if (plugin.state == PluginState::Modified && plugin.auto_reload && state.global_auto_reload) {
            plugin.time_since_modified += dt;
            if (plugin.time_since_modified >= plugin.reload_delay) {
                ReloadPlugin(i);
            }
        }
    }

    ImGui::Begin(MDI_ICON_RELOAD "  Plugin Hot Reload", PanelRegistry::Get().GetCurrentPanelOpen());
    PanelRegistry::Get().DrawMaximizeRestoreButton();

    // ─── Toolbar ─────────────────────────────────────────────────────────
    {
        if (ImGui::Button(MDI_ICON_RELOAD_ALERT " Reload All")) {
            for (int i = 0; i < static_cast<int>(state.plugins.size()); ++i) {
                auto& p = state.plugins[i];
                if (p.state == PluginState::Loaded || p.state == PluginState::Modified ||
                    p.state == PluginState::Error) {
                    ReloadPlugin(i);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_FOLDER_OPEN " Rescan")) {
            ScanPlugins();
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

    if (state.build_in_progress) {
        ImGui::ProgressBar(-1.0f, ImVec2(-1, 0), state.build_status.c_str());
    }

    ImGui::Separator();

    // ─── Plugin list ─────────────────────────────────────────────────────
    ImGui::BeginChild("PluginList", ImVec2(0, -150), ImGuiChildFlags_Borders);

    if (state.plugins.empty()) {
        ImGui::TextDisabled("No native plugins found under '%s'.", state.plugins_dir.c_str());
        ImGui::TextDisabled("A native plugin is a subdirectory containing a built .dll");
        ImGui::TextDisabled("(optionally with a src/ folder to watch).");
    }

    for (int i = 0; i < static_cast<int>(state.plugins.size()); i++) {
        auto& plugin = state.plugins[i];
        ImGui::PushID(i);

        bool expanded = ImGui::TreeNodeEx(plugin.name.c_str(),
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(PluginStateColor(plugin.state)),
                           "%s", PluginStateLabel(plugin.state));
        ImGui::SameLine();
        if (plugin.state == PluginState::Compiling) {
            ImGui::TextDisabled("...");
        } else if (plugin.state == PluginState::Loaded || plugin.state == PluginState::Modified) {
            if (ImGui::SmallButton("Reload")) ReloadPlugin(i);
            ImGui::SameLine();
            if (ImGui::SmallButton("Unload")) {
                plugin.library.reset();
                plugin.state = PluginState::Unloaded;
            }
        } else if (plugin.state == PluginState::Unloaded || plugin.state == PluginState::Error) {
            if (ImGui::SmallButton("Load")) {
                auto lib = std::make_shared<dse::core::DynamicLibrary>();
                if (lib->Load(plugin.dll_path)) {
                    plugin.library = lib;
                    plugin.state = PluginState::Loaded;
                    plugin.build_errors = 0;
                } else {
                    plugin.state = PluginState::Error;
                    plugin.last_build_output = "Failed to load " + plugin.dll_path;
                }
            }
        }

        if (expanded) {
            ImGui::Indent(16);
            ImGui::Text("DLL: %s", plugin.dll_path.c_str());
            ImGui::Text("Source: %s", plugin.source_dir.empty() ? "(none)" : plugin.source_dir.c_str());
            ImGui::Checkbox("Auto Reload", &plugin.auto_reload);
            ImGui::SameLine();
            ImGui::Checkbox("Watch", &plugin.watch_enabled);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            ImGui::DragFloat("Delay", &plugin.reload_delay, 0.1f, 0.1f, 5.0f, "%.1f");

            if (plugin.reload_count > 0) {
                ImGui::TextDisabled("Reloaded %d times (last build: %.2fs)",
                                    plugin.reload_count, plugin.last_build_time);
            }

            if (!plugin.watched_files.empty()) {
                ImGui::Text("Watched Files (%d):", static_cast<int>(plugin.watched_files.size()));
                for (auto& wf : plugin.watched_files) {
                    ImU32 fc = wf.changed ? IM_COL32(255, 200, 50, 255) : IM_COL32(150, 150, 150, 255);
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(fc), "  %s %s",
                                       wf.changed ? MDI_ICON_CIRCLE_MEDIUM : MDI_ICON_CIRCLE_OUTLINE,
                                       wf.path.c_str());
                }
            }

            if (!plugin.last_build_output.empty() && plugin.state == PluginState::Error) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 80, 80, 255));
                ImGui::TextWrapped("%s", plugin.last_build_output.c_str());
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
        ImGui::TextDisabled("Full build output streams to the Background Tasks panel.");
    }

    ImGui::End();
}

// ─── Test accessors ─────────────────────────────────────────────────────
static PluginHotReloadTestState s_test_state;

PluginHotReloadTestState& GetPluginHotReloadState() {
    InitHotReload();
    s_test_state.plugins.clear();
    for (auto& p : s_state.plugins) {
        PluginTestInfo ti;
        ti.name = p.name;
        ti.state = static_cast<HotReloadPluginState>(static_cast<int>(p.state));
        ti.auto_reload = p.auto_reload;
        s_test_state.plugins.push_back(ti);
    }
    s_test_state.global_auto_reload = s_state.global_auto_reload;
    s_test_state.selected_plugin = -1;
    return s_test_state;
}

void PluginHotReloadTriggerBuild(int plugin_index) {
    InitHotReload();
    ReloadPlugin(plugin_index);
}

} // namespace dse::editor
