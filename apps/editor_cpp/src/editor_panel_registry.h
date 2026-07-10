#pragma once
#include <string>
#include <vector>

namespace dse::editor {

/// Consolidated panel visibility state.
/// Replaces the former 28 individual `bool show_*` members in EditorApp.
/// All fields are default-initialized: Core panels are visible, others hidden.
struct PanelVisibilityState {
    // ── Core panels (default visible) ──
    bool hierarchy          = true;
    bool inspector          = true;
    bool console            = true;
    bool scene              = true;
    bool game               = true;
    bool sequencer          = true;

    // ── Optional panels (default hidden) ──
    bool preferences        = false;
    bool profiler           = false;
    bool localization_preview = false;
    bool undo_history       = false;
    bool streaming_debug    = false;
    bool lua_debugger       = false;
    bool animation          = false;
    bool tile_palette       = false;
    bool terrain_editor     = false;
    bool vegetation_brush   = false;
    bool lua_console        = false;
    bool asset_browser      = false;
    bool animation_timeline = false;
    bool navmesh            = false;
    bool shader_graph       = false;
    bool git                = false;
    bool multi_viewport     = false;
    bool anim_state_machine = false;
    bool curve_editor       = false;
    bool anim_retarget      = false;
    bool blueprint          = false;
    bool csharp_panel       = false;
    bool plugins            = false;
    bool ai_agent           = false;
};

/// Describes a single editor panel for data-driven registration.
struct PanelEntry {
    std::string id;            // Unique key, e.g. "hierarchy"
    std::string display_name;  // User-visible name (T() key)
    std::string category;      // Menu category: "Core", "Debug", "Tool", "Plugin"
    bool* visible = nullptr;   // Points to the owning show_ flag
    bool  default_visible = false;
};

/// Central registry for all editor panels.
/// Panels self-register at startup; the registry is queryable for menus, layout save/restore.
class PanelRegistry {
public:
    static PanelRegistry& Get();

    /// Register a panel. Typically called once during EditorApp::Init.
    void Register(PanelEntry entry);

    /// Get all registered panels (read-only).
    const std::vector<PanelEntry>& GetAll() const { return panels_; }

    /// Find a panel by id (nullptr if not found).
    PanelEntry* Find(const std::string& id);
    const PanelEntry* Find(const std::string& id) const;

    /// Toggle panel visibility by id. Returns false if panel not found.
    bool Toggle(const std::string& id);

    /// Get panels filtered by category.
    std::vector<const PanelEntry*> GetByCategory(const std::string& category) const;

private:
    PanelRegistry() = default;
    std::vector<PanelEntry> panels_;
};

} // namespace dse::editor
