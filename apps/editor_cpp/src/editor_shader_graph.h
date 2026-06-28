#pragma once

namespace dse::editor {
struct EditorContext;

/// Draw the Shader/Material Graph node editor panel
void DrawShaderGraphPanel(EditorContext& ctx);

// ── 测试访问器（供 UI 测试断言/复位图状态；普通运行不需要） ──────────────────
int ShaderGraphNodeCount();
int ShaderGraphLinkCount();
void ShaderGraphResetGraph();  // 清空并重建默认图（4 节点 + 1 连线）

} // namespace dse::editor
