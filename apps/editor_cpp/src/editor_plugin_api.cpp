#include "editor_plugin_api.h"
#include "editor_context.h"
#include "editor_console_panel.h"
#include "imgui.h"

#include <algorithm>

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

} // namespace dse::editor
