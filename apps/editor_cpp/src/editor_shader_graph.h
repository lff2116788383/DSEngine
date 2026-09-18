#pragma once

namespace dse::editor {
struct EditorContext;

/// Draw the Shader/Material Graph node editor panel
void DrawShaderGraphPanel(EditorContext& ctx);

// ── 测试访问器（供 UI 测试断言/复位图状态；普通运行不需要） ──────────────────
int ShaderGraphNodeCount();
int ShaderGraphLinkCount();
/// 测试用：取第 index 个节点上一次绘制时的**屏幕**左上角（已含画布平移/缩放）。
/// 画布建节点后会取景/滚动，UI 测试写死屏幕坐标会失准；引脚坐标 = 该点 + 固定偏移。
/// @return index 越界（该节点尚未绘制过）时返回 false。
bool ShaderGraphNodeScreenPos(int index, float* out_x, float* out_y);
/// 测试用：取某节点第 pin_index 个输入/输出引脚的**屏幕**坐标（复用面板自己的引脚布局公式）。
/// 引脚位置不是简单的固定偏移：输出引脚排在所有输入引脚之下，且随画布平移/缩放变化。
bool ShaderGraphPinScreenPos(int node_index, bool is_output, int pin_index, float* out_x, float* out_y);
void ShaderGraphResetGraph();  // 清空并重建默认图（4 节点 + 1 连线）

} // namespace dse::editor
