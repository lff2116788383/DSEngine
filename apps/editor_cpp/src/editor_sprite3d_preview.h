#pragma once

/**
 * @file editor_sprite3d_preview.h
 * @brief HD-2D Sprite3D 独立预览视口（docking 面板 + 引擎真渲染）
 *
 * 与 Inspector 内的缩略图 / 时间轴不同，本面板提供一个**独立窗口的实时 3D 预览**：
 * 用一台聚焦到所选 Sprite3D 实体的相机经 FramePipeline::RenderSceneWithCamera 重渲场景，
 * 把结果 RHI Blit 到面板私有 RT，再以 ImGui 图像显示。因此预览里能看到真实的
 * billboard 朝向、光照/阴影、atlas 逐帧动画与接触阴影，而不是一张静态缩略图。
 *
 * 渲染发生在 ImGui::NewFrame 之前（EditorApp 帧循环内），且随后会用编辑器相机重渲一次
 * 场景 RT，故主视口画面不受影响。Play/Pause 模式下引擎使用游戏相机，此时不做预览重渲，
 * 面板保留最后一帧并给出提示。
 */

#include <entt/entt.hpp>

namespace dse::render { class RhiDevice; }
class FramePipeline;  ///< engine/runtime/frame_pipeline.h（全局命名空间）

namespace dse::editor {

/// 帧内（ImGui::NewFrame 之前）调用：渲染预览相机画面到面板私有 RT，并恢复场景 RT。
/// @param pipeline  引擎帧流水线（空指针时空操作）
/// @param registry  当前世界 registry
/// @param selected  当前选中实体；须同时具备 Sprite3DComponent 与 TransformComponent
void RenderSprite3DPreviewViewport(FramePipeline* pipeline,
                                   entt::registry& registry,
                                   entt::entity selected);

/// 面板绘制（由 PanelRegistry::DrawAll 调用）。
void DrawSprite3DPreviewPanel(class EditorContext& ctx);

/// 打开预览窗口（Inspector 的「Open Preview Window」按钮调用）。
void OpenSprite3DPreview();

/// 释放面板持有的 RT / 纹理（须在 RHI 设备销毁前调用）。
void ReleaseSprite3DPreviewTargets();

/// 面板 id（Window 菜单 / 可见性绑定 / 测试共用）。
const char* Sprite3DPreviewPanelId();

} // namespace dse::editor
