#pragma once

#include "editor_context.h"

namespace dse::editor {

/// Draw the C# Script section in the Inspector for selected entity
void DrawCSharpScriptSection(EditorContext& ctx);

/// Draw the standalone C# Script Management panel (build, reload, status)
void DrawCSharpPanel(EditorContext& ctx);

} // namespace dse::editor
