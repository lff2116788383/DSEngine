#pragma once

#include <string>
#include "editor_context.h"

namespace dse::editor {

void BeginEditorShell();
void EndEditorShell();
void DrawEditorMainMenu(EditorContext& ctx, bool* show_preferences = nullptr, bool* show_plugins = nullptr, bool* show_chat = nullptr);

/// Draw the scene tab bar (call after BeginEditorShell + DrawEditorMainMenu)
void DrawSceneTabBar(EditorContext& ctx);

/// Get/Set the current scene file path (for window title display)
const std::string& GetCurrentScenePath();
void SetCurrentScenePath(const std::string& path);

} // namespace dse::editor
