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

    // Deterministic draw / menu ordering. Because panels self-register at
    // static-init time (across translation units, in unspecified order), the
    // registry sorts by this key in Finalize() so behaviour is stable
    // regardless of link order. Lower draws first. Default keeps unspecified
    // panels grouped after the explicitly-ordered ones but before viewports.
    int order = 500;
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

    /// Stable-sort panels by their `order` key. Call once after all
    /// registration (deferred + central) so draw / menu order is deterministic
    /// regardless of static-init link order.
    void Finalize();

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

    /// Returns the visible flag pointer of the panel currently being drawn.
    /// Panels call this to pass to ImGui::Begin(name, p_open) so they get a
    /// close (×) button that toggles visibility. Returns nullptr for panels
    /// with no visibility flag (always-drawn panels) — ImGui::Begin(name, nullptr)
    /// behaves identically to ImGui::Begin(name).
    bool* GetCurrentPanelOpen() const { return current_panel_open_; }

    /// Returns the id string of the panel currently being drawn.
    const std::string& GetCurrentPanelId() const { return current_panel_id_; }

    /// Whether any panel is currently maximized.
    bool HasMaximizedPanel() const { return !maximized_window_name_.empty(); }

    /// Whether the given ImGui window name is the currently maximized one.
    bool IsMaximized(const char* window_name) const {
        return window_name && maximized_window_name_ == window_name;
    }

    /// Toggle maximize for the given ImGui window name. When maximized, the
    /// window is undocked from its dock node and resized to fill the entire
    /// dockspace; restoring re-docks it back to its original dock node.
    void ToggleMaximize(const char* window_name);

    /// Cancel any active maximize (keep the window where it currently is).
    /// Used on layout reset / project switch so the maximized state does not
    /// leak across sessions.
    void ResetMaximize();

    /// Draw a maximize/restore (□) button in the panel title bar, to the left
    /// of the ImGui-provided close (×) button. Call right after ImGui::Begin.
    void DrawMaximizeRestoreButton();

    /// Render the Window menu contents (checkbox items grouped by category)
    /// for all toggleable panels. Replaces the hardcoded MenuItem list.
    void DrawWindowMenu();

    /// The RHI backend the editor is currently running on ("opengl"/"d3d11"/
    /// "vulkan"); used for availability filtering. Empty = unknown/any.
    void SetActiveBackend(std::string backend) { active_backend_ = std::move(backend); }
    const std::string& ActiveBackend() const { return active_backend_; }

    /// Set the dockspace ID (called from BeginEditorShell). Used to query the
    /// dockspace position/size when a panel is maximized.
    void SetDockspaceId(unsigned int id) { dockspace_id_ = id; }

private:
    PanelRegistry() = default;
    void RestoreMaximize();
    std::vector<PanelEntry> panels_;
    std::string active_backend_;
    bool inited_ = false;
    bool* current_panel_open_ = nullptr;  // set by DrawAll before calling draw
    std::string current_panel_id_;        // id of panel currently being drawn
    std::string maximized_window_name_;   // ImGui window name of the maximized window (empty = none)
    std::string maximized_panel_id_;      // panel hosting the maximized window (for DrawAll dispatch)
    unsigned int maximized_dock_id_ = 0;  // dock node to restore the window into (0 = was floating)
    unsigned int dockspace_id_ = 0;       // dockspace ID (for querying position/size)
};

/// Self-registration helper. A panel module places
///   DSE_EDITOR_PANEL([](dse::editor::PanelRegistry& reg){ reg.Register(...); });
/// at file scope; the registrar is queued at static-init time and run by the
/// editor via RunDeferredRegistrars(), so editor_app.cpp never names the panel.
#define DSE_EDITOR_PANEL_CONCAT_(a, b) a##b
#define DSE_EDITOR_PANEL_CONCAT(a, b) DSE_EDITOR_PANEL_CONCAT_(a, b)
#define DSE_EDITOR_PANEL(registrar)                                        \
    namespace {                                                            \
    const bool DSE_EDITOR_PANEL_CONCAT(dse_editor_panel_reg_, __LINE__) =  \
        (::dse::editor::PanelRegistry::AddDeferredRegistrar(registrar),     \
         true);                                                            \
    }

} // namespace dse::editor
