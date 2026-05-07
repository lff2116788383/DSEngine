#include "editor_scene_tabs.h"

#include <filesystem>

#include "imgui.h"
#include "editor_scene_io.h"
#include "editor_shell.h"
#include "editor_shortcuts.h"
#include "editor_console_panel.h"
#include "editor_selection.h"
#include "editor_icons.h"

namespace dse::editor {

SceneTabManager& SceneTabManager::Get() {
    static SceneTabManager instance;
    return instance;
}

std::string SceneTabManager::ExtractDisplayName(const std::string& path) {
    if (path.empty() || path == "Untitled") return "Untitled";
    return std::filesystem::path(path).filename().string();
}

void SceneTabManager::Init(const std::string& scene_path) {
    tabs_.clear();
    SceneTab tab;
    tab.file_path = scene_path;
    tab.display_name = ExtractDisplayName(scene_path);
    tab.dirty = false;
    tab.has_snapshot = false;
    tabs_.push_back(std::move(tab));
    active_index_ = 0;
    last_undo_count_ = 0;
    pending_switch_ = -1;
}

int SceneTabManager::NewScene(entt::registry& registry) {
    // Snapshot current tab before switching away
    SnapshotActiveTab(registry);

    // Create new empty tab
    SceneTab tab;
    tab.file_path = "";
    tab.display_name = "Untitled";
    tab.dirty = false;
    tab.has_snapshot = false;
    tabs_.push_back(std::move(tab));

    const int new_index = static_cast<int>(tabs_.size()) - 1;

    // Switch: clear registry for new empty scene
    registry.clear();
    active_index_ = new_index;
    pending_switch_ = new_index;

    // Reset editor state
    SelectionManager::Get().Clear();
    GetUndoRedoManager().Clear();
    last_undo_count_ = 0;
    SetCurrentScenePath("Untitled");
    EditorLog(LogLevel::Info, "New scene tab created");

    return new_index;
}

int SceneTabManager::OpenScene(entt::registry& registry, const std::string& path) {
    // Check if already open in another tab
    const int existing = FindTabByPath(path);
    if (existing >= 0) {
        if (existing != active_index_) {
            SnapshotActiveTab(registry);
            RestoreTabSnapshot(existing, registry);
            pending_switch_ = existing;
        }
        return existing;
    }

    // Snapshot current tab
    SnapshotActiveTab(registry);

    // Create new tab and load the scene
    SceneTab tab;
    tab.file_path = path;
    tab.display_name = ExtractDisplayName(path);
    tab.dirty = false;
    tab.has_snapshot = false;
    tabs_.push_back(std::move(tab));

    const int new_index = static_cast<int>(tabs_.size()) - 1;

    // Load scene into the active registry
    LoadScene(registry, path);
    active_index_ = new_index;
    pending_switch_ = new_index;

    // Reset editor state
    SelectionManager::Get().Clear();
    GetUndoRedoManager().Clear();
    last_undo_count_ = 0;
    SetCurrentScenePath(path);
    EditorLog(LogLevel::Info, "Opened scene in new tab: " + path);

    return new_index;
}

bool SceneTabManager::CloseTab(int index, entt::registry& registry) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return false;

    // If closing the last remaining tab, reset it to empty Untitled
    if (tabs_.size() == 1) {
        tabs_[0].file_path = "";
        tabs_[0].display_name = "Untitled";
        tabs_[0].dirty = false;
        tabs_[0].has_snapshot = false;
        tabs_[0].snapshot.clear();
        registry.clear();
        active_index_ = 0;
        SelectionManager::Get().Clear();
        GetUndoRedoManager().Clear();
        last_undo_count_ = 0;
        SetCurrentScenePath("Untitled");
        EditorLog(LogLevel::Info, "Scene tab closed (reset to Untitled)");
        return false;
    }

    const bool closing_active = (index == active_index_);
    tabs_.erase(tabs_.begin() + index);

    if (closing_active) {
        // Switch to adjacent tab
        if (active_index_ >= static_cast<int>(tabs_.size())) {
            active_index_ = static_cast<int>(tabs_.size()) - 1;
        }
        RestoreTabSnapshot(active_index_, registry);
    } else if (index < active_index_) {
        active_index_--;
    }

    EditorLog(LogLevel::Info, "Scene tab closed");
    return true;
}

void SceneTabManager::SwitchTo(int index, entt::registry& registry) {
    if (index < 0 || index >= static_cast<int>(tabs_.size()) || index == active_index_) return;

    SnapshotActiveTab(registry);
    RestoreTabSnapshot(index, registry);
}

void SceneTabManager::MarkDirty() {
    if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
        tabs_[active_index_].dirty = true;
    }
}

void SceneTabManager::MarkClean() {
    if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
        tabs_[active_index_].dirty = false;
        last_undo_count_ = GetUndoRedoManager().GetUndoCount();
    }
}

void SceneTabManager::SetCurrentPath(const std::string& path) {
    if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
        tabs_[active_index_].file_path = path;
        tabs_[active_index_].display_name = ExtractDisplayName(path);
        SetCurrentScenePath(path);
    }
}

void SceneTabManager::UpdateDirtyState() {
    if (active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) return;
    const int current_undo_count = GetUndoRedoManager().GetUndoCount();
    if (current_undo_count != last_undo_count_) {
        if (current_undo_count > last_undo_count_) {
            tabs_[active_index_].dirty = true;
        }
        last_undo_count_ = current_undo_count;
    }
}

std::string SceneTabManager::GetActiveDisplayName() const {
    if (active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) return "Untitled";
    return tabs_[active_index_].display_name;
}

std::string SceneTabManager::GetActiveFilePath() const {
    if (active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) return "";
    return tabs_[active_index_].file_path;
}

bool SceneTabManager::IsAnyTabDirty() const {
    for (const auto& tab : tabs_) {
        if (tab.dirty) return true;
    }
    return false;
}

int SceneTabManager::FindTabByPath(const std::string& path) const {
    if (path.empty()) return -1;
    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
        if (tabs_[i].file_path == path) return i;
    }
    return -1;
}

void SceneTabManager::SnapshotActiveTab(entt::registry& registry) {
    if (active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) return;
    CopyRegistry(tabs_[active_index_].snapshot, registry);
    tabs_[active_index_].has_snapshot = true;
}

void SceneTabManager::RestoreTabSnapshot(int index, entt::registry& registry) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;

    if (tabs_[index].has_snapshot) {
        CopyRegistry(registry, tabs_[index].snapshot);
    } else if (!tabs_[index].file_path.empty() && tabs_[index].file_path != "Untitled") {
        LoadScene(registry, tabs_[index].file_path);
    } else {
        registry.clear();
    }

    active_index_ = index;
    SelectionManager::Get().Clear();
    GetUndoRedoManager().Clear();
    last_undo_count_ = 0;
    const std::string& path = tabs_[index].file_path;
    SetCurrentScenePath(path.empty() ? "Untitled" : path);
}

bool SceneTabManager::DrawTabBar(entt::registry& registry) {
    // Auto-detect dirty from undo stack growth
    UpdateDirtyState();

    bool tab_changed = false;
    int close_request = -1;

    const ImGuiTabBarFlags bar_flags =
        ImGuiTabBarFlags_Reorderable |
        ImGuiTabBarFlags_AutoSelectNewTabs |
        ImGuiTabBarFlags_TabListPopupButton |
        ImGuiTabBarFlags_FittingPolicyScroll;

    if (ImGui::BeginTabBar("##SceneTabs", bar_flags)) {
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
            ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
            if (tabs_[i].dirty) {
                flags |= ImGuiTabItemFlags_UnsavedDocument;
            }
            // Handle programmatic tab switch (from menu New/Open)
            if (i == pending_switch_) {
                flags |= ImGuiTabItemFlags_SetSelected;
                pending_switch_ = -1;
            }

            // Build label: "filename *###SceneTabN"
            std::string label = tabs_[i].display_name;
            if (tabs_[i].dirty) label += " *";
            label += "###SceneTab" + std::to_string(i);

            bool open = true;
            if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
                // This tab is selected by the user
                if (i != active_index_) {
                    SwitchTo(i, registry);
                    tab_changed = true;
                }
                ImGui::EndTabItem();
            }

            // Right-click context menu on tab
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Close")) {
                    close_request = i;
                }
                if (ImGui::MenuItem("Close Others", nullptr, false, tabs_.size() > 1)) {
                    // Close all except this one
                    SnapshotActiveTab(registry);
                    SceneTab keep = std::move(tabs_[i]);
                    tabs_.clear();
                    tabs_.push_back(std::move(keep));
                    active_index_ = 0;
                    RestoreTabSnapshot(0, registry);
                    tab_changed = true;
                    ImGui::EndPopup();
                    ImGui::EndTabBar();
                    return true;
                }
                ImGui::EndPopup();
            }

            if (!open) {
                close_request = i;
            }
        }

        // "+" button to create a new empty tab
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
            NewScene(registry);
            tab_changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("New Scene (Ctrl+N)");
        }

        ImGui::EndTabBar();
    }

    // Process deferred close request
    if (close_request >= 0) {
        CloseTab(close_request, registry);
        tab_changed = true;
    }

    return tab_changed;
}

} // namespace dse::editor
