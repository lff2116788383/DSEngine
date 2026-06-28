#pragma once

#include "editor_context.h"
#include <string>

namespace dse::editor {

/// Draw the Visual Script (Blueprint) editor panel
void DrawVisualScriptEditor(EditorContext& ctx);

/// Get the last generated Lua code from the visual script graph
const std::string& GetVisualScriptLuaOutput();

// ── 测试访问器（供 UI 测试断言/复位图状态；普通运行不需要） ──────────────────
int VisualScriptNodeCount();
int VisualScriptLinkCount();
void VisualScriptResetGraph();

} // namespace dse::editor
