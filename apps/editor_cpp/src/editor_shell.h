#pragma once

#include <string>
#include "editor_context.h"

namespace dse::editor {

struct PanelVisibility {
    bool* localization_preview = nullptr;
    bool* profiler = nullptr;
    bool* animation = nullptr;
    bool* tile_palette = nullptr;
    bool* terrain_editor = nullptr;
    bool* lua_console = nullptr;
};

void BeginEditorShell();
void EndEditorShell();
void DrawEditorMainMenu(EditorContext& ctx, bool* show_preferences = nullptr, bool* show_plugins = nullptr, bool* show_chat = nullptr, const PanelVisibility* panels = nullptr);

/// Draw the scene tab bar (call after BeginEditorShell + DrawEditorMainMenu)
void DrawSceneTabBar(EditorContext& ctx);

/// Get/Set the current scene file path (for window title display)
const std::string& GetCurrentScenePath();
void SetCurrentScenePath(const std::string& path);

/// Force rebuild of dock layout on the next frame (clears first_time guard).
void ResetEditorLayout();

} // namespace dse::editor
