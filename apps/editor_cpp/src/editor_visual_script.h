#pragma once
// ============================================================
// DEPRECATED: Visual Script is superseded by the Blueprint system.
// Use editor_blueprint.h / editor_blueprint.cpp instead.
// This module will be removed in a future release.
// ============================================================


#include "editor_context.h"
#include <string>

namespace dse::editor {

/// Draw the Visual Script (Blueprint) editor panel
[[deprecated("Use Blueprint system (DrawBlueprintEditor) instead")]]
void DrawVisualScriptEditor(EditorContext& ctx);

/// Get the last generated Lua code from the visual script graph
[[deprecated("Use Blueprint system instead")]]
const std::string& GetVisualScriptLuaOutput();

// ── 测试访问器（供 UI 测试断言/复位图状态；普通运行不需要） ──────────────────
[[deprecated("Use Blueprint system instead")]]
int VisualScriptNodeCount();
[[deprecated("Use Blueprint system instead")]]
int VisualScriptLinkCount();
[[deprecated("Use Blueprint system instead")]]
void VisualScriptResetGraph();

} // namespace dse::editor
