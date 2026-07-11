#pragma once
#include <functional>
#include <string>
#include <vector>

namespace dse::editor {

struct EditorContext; // fwd

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
    bool background_tasks   = false;
};

/// Describes a single editor panel for data-driven registration.
///
/// A panel is fully described by its metadata + callbacks; neither EditorApp's
/// frame loop nor the shell menu needs to know the concrete panel type. New
/// panels register themselves (see DSE_EDITOR_PANEL / AddDeferredRegistrar)
/// and are automatically enumerated, drawn, toggled from the menu, and
/// persisted — without editing editor_app.cpp.
struct PanelEntry {
    std::string id;            // Unique key, e.g. "hierarchy"
    std::string display_name;  // User-visible name (T() key)
    std::string category;      // Menu category: "Core", "Debug", "Tool", "Plugin"
    bool* visible = nullptr;    // Points to owning show_ flag; nullptr = always drawn, no menu toggle
    bool  default_visible = false;

    // ── Behaviour callbacks (all optional) ──
    std::function<void(EditorContext&)> draw;      // Per-frame draw; nullptr = registry does not draw it
    std::function<bool()>               available; // Availability predicate; nullptr = always available
    std::function<void()>               init;      // One-time init hook
    std::function<void()>               shutdown;  // One-time shutdown hook

    std::string required_backend; // "", "opengl", "d3d11", "vulkan" — empty = any RHI
    std::string menu_icon;         // Optional MDI icon glyph prefix for the Window menu
    std::string test_id;           // Automation/test identifier (defaults to id if empty)
};

/// Central registry for all editor panels.
/// Panels self-register at startup; the registry is queryable for menus, layout save/restore.
class PanelRegistry {
public:
    static PanelRegistry& Get();

    /// Register a panel. Typically called once during EditorApp::Init or via a
    /// deferred registrar from the panel's own module.
    void Register(PanelEntry entry);

    /// Register a callback that will register one or more panels when the
    /// editor runs RunDeferredRegistrars(). Lets a panel module self-register
    /// (via the DSE_EDITOR_PANEL macro) without editor_app.cpp knowing about it.
    static void AddDeferredRegistrar(std::function<void(PanelRegistry&)> fn);

    /// Run (and clear) all deferred registrars queued by AddDeferredRegistrar.
    void RunDeferredRegistrars();

    /// Get all registered panels (read-only).
    const std::vector<PanelEntry>& GetAll() const { return panels_; }

    /// Find a panel by id (nullptr if not found).
    PanelEntry* Find(const std::string& id);
    const PanelEntry* Find(const std::string& id) const;

    /// Toggle panel visibility by id. Returns false if panel not found or not toggleable.
    bool Toggle(const std::string& id);

    /// Get panels filtered by category.
    std::vector<const PanelEntry*> GetByCategory(const std::string& category) const;

    /// Whether a panel is available in the current environment (predicate + backend).
    bool IsAvailable(const PanelEntry& entry) const;

    /// Run every panel's init / shutdown hook (once).
    void InitAll();
    void ShutdownAll();

    /// Draw every registered, available, visible panel. This is the single
    /// dispatch point that replaces the hardcoded panel calls in DrawEditorUI.
    void DrawAll(EditorContext& ctx);

    /// Render the Window menu contents (checkbox items grouped by category)
    /// for all toggleable panels. Replaces the hardcoded MenuItem list.
    void DrawWindowMenu();

    /// The RHI backend the editor is currently running on ("opengl"/"d3d11"/
    /// "vulkan"); used for availability filtering. Empty = unknown/any.
    void SetActiveBackend(std::string backend) { active_backend_ = std::move(backend); }
    const std::string& ActiveBackend() const { return active_backend_; }

private:
    PanelRegistry() = default;
    std::vector<PanelEntry> panels_;
    std::string active_backend_;
    bool inited_ = false;
};

} // namespace dse::editor
