#pragma once

#include "editor_context.h"
#include <string>

namespace dse::editor {

/// Draw the Visual Script (Blueprint) editor panel
void DrawVisualScriptEditor(EditorContext& ctx);

/// Get the last generated Lua code from the visual script graph
const std::string& GetVisualScriptLuaOutput();

} // namespace dse::editor
