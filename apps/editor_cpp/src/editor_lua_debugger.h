#pragma once

namespace dse::editor {
struct EditorContext;

/// Draw the Lua Script Debugger panel (breakpoints, call stack, locals, watch)
void DrawLuaDebuggerPanel(EditorContext& ctx);

} // namespace dse::editor
