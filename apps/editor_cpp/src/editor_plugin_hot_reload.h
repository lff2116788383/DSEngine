#pragma once

#include "editor_context.h"
#include <string>
#include <vector>

namespace dse::editor {

/// Draw the Plugin Hot Reload panel (DLL recompile + hot-swap workflow)
void DrawPluginHotReloadPanel(EditorContext& ctx);

// ─── Test accessors ─────────────────────────────────────────────────────
enum class HotReloadPluginState { Loaded, Unloaded, Compiling, Error, Modified };

struct PluginTestInfo {
    std::string name;
    HotReloadPluginState state = HotReloadPluginState::Unloaded;
    bool auto_reload = true;
};

struct PluginHotReloadTestState {
    std::vector<PluginTestInfo> plugins;
    bool global_auto_reload = true;
    int selected_plugin = -1;
};

PluginHotReloadTestState& GetPluginHotReloadState();
void PluginHotReloadTriggerBuild(int plugin_index);

} // namespace dse::editor
