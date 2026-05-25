#include "editor_plugin_api.h"
#include "editor_context.h"
#include "editor_console_panel.h"
#include "imgui.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

#include <algorithm>
#include <filesystem>

namespace dse::editor {

EditorPluginManager& EditorPluginManager::Instance() {
    static EditorPluginManager mgr;
    return mgr;
}

void EditorPluginManager::RegisterPlugin(std::shared_ptr<EditorPlugin> plugin) {
    if (!plugin) return;
    // Prevent duplicate
    for (const auto& p : plugins_) {
        if (p->name == plugin->name) return;
    }
    plugins_.push_back(std::move(plugin));
    EditorLog(LogLevel::Info, "Plugin registered: " + plugins_.back()->name);
}

void EditorPluginManager::UnregisterPlugin(const std::string& name) {
    plugins_.erase(
        std::remove_if(plugins_.begin(), plugins_.end(),
            [&](const std::shared_ptr<EditorPlugin>& p) { return p->name == name; }),
        plugins_.end());
}

void EditorPluginManager::InitAll(EditorContext& ctx) {
    for (auto& p : plugins_) {
        if (p->on_init) {
            p->on_init(ctx);
            EditorLog(LogLevel::Info, "Plugin initialized: " + p->name);
        }
    }
}

void EditorPluginManager::ShutdownAll() {
    for (auto& p : plugins_) {
        if (p->on_shutdown) {
            p->on_shutdown();
        }
    }
    plugins_.clear();
}

void EditorPluginManager::UpdateAll(EditorContext& ctx, float dt) {
    for (auto& p : plugins_) {
        if (p->on_update) {
            p->on_update(ctx, dt);
        }
    }
}

void EditorPluginManager::DrawAllPanels(EditorContext& ctx) {
    for (auto& p : plugins_) {
        for (auto& panel : p->panels) {
            if (!panel.visible) continue;
            ImGui::Begin(panel.name.c_str(), &panel.visible);
            if (panel.draw) {
                panel.draw(ctx);
            }
            ImGui::End();
        }
    }
}

void EditorPluginManager::DrawPluginMenuItems() {
    for (auto& p : plugins_) {
        for (auto& item : p->menu_items) {
            if (item.submenu.empty()) {
                if (ImGui::MenuItem(item.label.c_str())) {
                    if (item.action) item.action();
                }
            }
        }
        // Submenus
        for (auto& p2 : plugins_) {
            std::string last_submenu;
            for (auto& item : p2->menu_items) {
                if (item.submenu.empty()) continue;
                if (item.submenu != last_submenu) {
                    if (!last_submenu.empty()) ImGui::EndMenu();
                    last_submenu = item.submenu;
                    if (!ImGui::BeginMenu(item.submenu.c_str())) {
                        last_submenu.clear();
                        continue;
                    }
                }
                if (ImGui::MenuItem(item.label.c_str())) {
                    if (item.action) item.action();
                }
            }
            if (!last_submenu.empty()) ImGui::EndMenu();
        }
        break; // submenu loop is done for all plugins above
    }
    // Panel toggles
    if (!plugins_.empty()) {
        ImGui::Separator();
        for (auto& p : plugins_) {
            for (auto& panel : p->panels) {
                ImGui::MenuItem(panel.name.c_str(), nullptr, &panel.visible);
            }
        }
    }
}

const std::vector<std::shared_ptr<EditorPlugin>>& EditorPluginManager::GetPlugins() const {
    return plugins_;
}

void EditorPluginManager::TogglePanelVisibility(const std::string& plugin_name, const std::string& panel_name) {
    for (auto& p : plugins_) {
        if (p->name != plugin_name) continue;
        for (auto& panel : p->panels) {
            if (panel.name == panel_name) {
                panel.visible = !panel.visible;
                return;
            }
        }
    }
}

bool EditorPluginManager::LoadPluginFromDll(const std::string& dll_path) {
#ifdef _WIN32
    HMODULE hmod = LoadLibraryA(dll_path.c_str());
    if (!hmod) {
        DWORD err = GetLastError();
        EditorLog(LogLevel::Error, "Failed to load plugin DLL: " + dll_path + " (error " + std::to_string(err) + ")");
        return false;
    }

    auto fn = reinterpret_cast<DsePluginRegisterFn>(GetProcAddress(hmod, "dse_plugin_register"));
    if (!fn) {
        EditorLog(LogLevel::Error, "DLL missing 'dse_plugin_register' entry: " + dll_path);
        FreeLibrary(hmod);
        return false;
    }

    size_t before = plugins_.size();
    fn(this);

    if (plugins_.size() > before) {
        dll_handles_.push_back(static_cast<void*>(hmod));
        loaded_dll_paths_.push_back(dll_path);
        EditorLog(LogLevel::Info, "Loaded plugin DLL: " + dll_path);
        return true;
    } else {
        EditorLog(LogLevel::Warning, "Plugin DLL registered nothing: " + dll_path);
        FreeLibrary(hmod);
        return false;
    }
#else
    EditorLog(LogLevel::Warning, "DLL plugin loading not supported on this platform");
    return false;
#endif
}

int EditorPluginManager::LoadPluginsFromDirectory(const std::string& dir_path) {
    namespace fs = std::filesystem;
    int count = 0;
    std::error_code ec;
    if (!fs::exists(dir_path, ec) || !fs::is_directory(dir_path, ec)) return 0;

    for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
#ifdef _WIN32
        if (ext != ".dll") continue;
#else
        if (ext != ".so") continue;
#endif
        if (LoadPluginFromDll(entry.path().string())) ++count;
    }
    return count;
}

const std::vector<std::string>& EditorPluginManager::GetLoadedDllPaths() const {
    return loaded_dll_paths_;
}

// ─── Plugin Browser Panel ────────────────────────────────────────────────────

void DrawPluginBrowserPanel() {
    auto& mgr = EditorPluginManager::Instance();
    auto& plugins = mgr.GetPlugins();

    ImGui::Text("Loaded Plugins: %d", static_cast<int>(plugins.size()));
    ImGui::Separator();

    if (ImGui::Button("Load Plugin DLL...")) {
#ifdef _WIN32
        char filename[MAX_PATH] = "";
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "Plugin DLLs (*.dll)\0*.dll\0All Files\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn)) {
            mgr.LoadPluginFromDll(filename);
        }
#endif
    }
    ImGui::SameLine();
    if (ImGui::Button("Scan plugins/ Dir")) {
        namespace fs = std::filesystem;
        std::string scan_dir = (fs::current_path() / "plugins").string();
        int loaded = mgr.LoadPluginsFromDirectory(scan_dir);
        EditorLog(LogLevel::Info, "Scanned plugins/ — loaded " + std::to_string(loaded) + " new plugins");
    }

    ImGui::Separator();

    for (size_t i = 0; i < plugins.size(); ++i) {
        auto& p = plugins[i];
        ImGui::PushID(static_cast<int>(i));

        if (ImGui::CollapsingHeader(p->name.c_str())) {
            ImGui::Indent();
            ImGui::TextDisabled("Version: %s", p->version.c_str());
            ImGui::TextDisabled("Author:  %s", p->author.c_str());
            if (!p->description.empty()) {
                ImGui::TextWrapped("%s", p->description.c_str());
            }
            if (!p->panels.empty()) {
                ImGui::Text("Panels:");
                for (auto& panel : p->panels) {
                    bool vis = panel.visible;
                    if (ImGui::Checkbox(panel.name.c_str(), &vis)) {
                        panel.visible = vis;
                    }
                }
            }
            if (!p->menu_items.empty()) {
                ImGui::Text("Menu Items: %d", static_cast<int>(p->menu_items.size()));
            }
            ImGui::Unindent();
        }

        ImGui::PopID();
    }

    // Show DLL paths
    auto& paths = mgr.GetLoadedDllPaths();
    if (!paths.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("DLL Paths:");
        for (auto& path : paths) {
            ImGui::BulletText("%s", path.c_str());
        }
    }
}

} // namespace dse::editor
