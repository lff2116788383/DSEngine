#pragma once

#include <string>
#include <vector>
#include <entt/entt.hpp>

namespace dse::editor {

struct SceneTab {
    std::string file_path;        // empty for "Untitled" new scenes
    std::string display_name;     // Display name (filename or "Untitled")
    bool dirty = false;           // Has unsaved changes since last save
    entt::registry snapshot;      // Holds scene data when tab is not active
    bool has_snapshot = false;    // True after first deactivation
};

class SceneTabManager {
public:
    static SceneTabManager& Get();

    /// Initialize with the current scene state (call once at startup)
    void Init(const std::string& scene_path);

    /// Create a new empty scene tab and switch to it
    int NewScene(entt::registry& registry);

    /// Open a scene file in a new tab (or switch to existing tab if already open)
    int OpenScene(entt::registry& registry, const std::string& path);

    /// Close a tab by index. If last tab, replaces with empty Untitled.
    bool CloseTab(int index, entt::registry& registry);

    /// Switch to a specific tab index
    void SwitchTo(int index, entt::registry& registry);

    /// Mark current tab as dirty (unsaved changes)
    void MarkDirty();

    /// Mark current tab as clean (just saved)
    void MarkClean();

    /// Update the current tab's file path (after Save As)
    void SetCurrentPath(const std::string& path);

    /// Draw the tab bar UI. Returns true if active tab changed this frame.
    bool DrawTabBar(entt::registry& registry);

    /// Auto-detect dirty state by checking undo stack growth
    void UpdateDirtyState();

    int GetActiveIndex() const { return active_index_; }
    int GetTabCount() const { return static_cast<int>(tabs_.size()); }
    const SceneTab& GetActiveTab() const { return tabs_[active_index_]; }
    SceneTab& GetActiveTab() { return tabs_[active_index_]; }
    std::string GetActiveDisplayName() const;
    std::string GetActiveFilePath() const;
    bool IsAnyTabDirty() const;

    /// Find tab by file path (-1 if not found)
    int FindTabByPath(const std::string& path) const;

private:
    SceneTabManager() = default;
    static std::string ExtractDisplayName(const std::string& path);
    void SnapshotActiveTab(entt::registry& registry);
    void RestoreTabSnapshot(int index, entt::registry& registry);

    std::vector<SceneTab> tabs_;
    int active_index_ = -1;
    int last_undo_count_ = 0;       // For dirty detection
    int pending_switch_ = -1;       // Programmatic tab switch (menu-driven)
};

} // namespace dse::editor
