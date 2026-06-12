#pragma once

// 骨骼动画重定向面板（ImGui）。源/目标模型导入与 UI 在此；纯映射逻辑见
// editor_anim_retarget_core.{h,cpp}。

namespace dse::editor {

struct EditorContext;

/// 绘制 Animation Retargeting 面板内容（调用方负责 Begin/End 窗口）。
void DrawAnimRetargetPanel(EditorContext& ctx);

}  // namespace dse::editor
