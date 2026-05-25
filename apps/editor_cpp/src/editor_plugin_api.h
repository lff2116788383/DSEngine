#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

#ifdef _WIN32
#define DSE_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define DSE_PLUGIN_EXPORT extern "C"
#endif

namespace dse::editor {

struct EditorContext;

/// A single custom menu item registered by a plugin
struct PluginMenuItem {
    std::string label;
    std::string submenu;            // empty = top-level "Plugins" menu
    std::function<void()> action;
};

/// A custom panel registered by a plugin
struct PluginPanel {
    std::string name;
    std::function<void(EditorContext&)> draw;
    bool visible = false;
};

/// Plugin descriptor — each plugin provides this via a registration callback
struct EditorPlugin {
    std::string name;
    std::string version;
    std::string author;
    std::string description;

    std::vector<PluginMenuItem> menu_items;
    std::vector<PluginPanel> panels;

    std::function<void(EditorContext&)> on_init;
    std::function<void()> on_shutdown;
    std::function<void(EditorContext&, float)> on_update;  // called every frame
};

/// Plugin manager — singleton registry
class EditorPluginManager {
public:
    static EditorPluginManager& Instance();

    /// Register a plugin (called at startup or from DLL entry point)
    void RegisterPlugin(std::shared_ptr<EditorPlugin> plugin);

    /// Unregister a plugin by name
    void UnregisterPlugin(const std::string& name);

    /// Initialize all plugins (call after EditorContext is ready)
    void InitAll(EditorContext& ctx);

    /// Shutdown all plugins
    void ShutdownAll();

    /// Update all plugins (call every frame)
    void UpdateAll(EditorContext& ctx, float dt);

    /// Draw all visible plugin panels
    void DrawAllPanels(EditorContext& ctx);

    /// Get menu items for the Plugins menu
    void DrawPluginMenuItems();

    /// Get all registered plugins (for plugin browser)
    const std::vector<std::shared_ptr<EditorPlugin>>& GetPlugins() const;

    /// Toggle a plugin panel's visibility
    void TogglePanelVisibility(const std::string& plugin_name, const std::string& panel_name);

    /// Load a plugin from a DLL/SO file. Returns true on success.
    bool LoadPluginFromDll(const std::string& dll_path);

    /// Scan a directory for plugin DLLs and load them
    int LoadPluginsFromDirectory(const std::string& dir_path);

    /// Get loaded DLL paths
    const std::vector<std::string>& GetLoadedDllPaths() const;

private:
    EditorPluginManager() = default;
    std::vector<std::shared_ptr<EditorPlugin>> plugins_;
    std::vector<std::string> loaded_dll_paths_;
    std::vector<void*> dll_handles_;  // HMODULE on Windows
};

/// Draw a plugin browser/manager panel (shows loaded plugins, allows load/unload)
void DrawPluginBrowserPanel();

// Plugin DLL entry point signature:
// DSE_PLUGIN_EXPORT void dse_plugin_register(dse::editor::EditorPluginManager* mgr);
typedef void (*DsePluginRegisterFn)(EditorPluginManager*);

} // namespace dse::editor
