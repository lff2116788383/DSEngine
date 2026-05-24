#pragma once

#include "editor_context.h"
#include <glm/mat4x4.hpp>
#include <entt/entt.hpp>
#include "imgui.h"
#include <glm/vec2.hpp>

class FramePipeline;

namespace dse::editor {

void DrawSceneViewportPanel(EditorContext& ctx,
                            unsigned int scene_texture_id,
                            bool (*build_active_camera_matrices)(entt::registry&, float, glm::mat4&, glm::mat4&),
                            FramePipeline* pipeline = nullptr);

void DrawGameViewportPanel(unsigned int texture_id);

/// 缓存 Scene 面板的 aspect ratio（上一帧），供 SetEditorCamera 使用
float GetCachedSceneViewportAspect();
void  SetCachedSceneViewportAspect(float aspect);

void DrawPhysicsColliderOverlay(entt::registry& registry,
                                entt::entity selected,
                                ImDrawList* dl,
                                const glm::vec2& vp_pos, const glm::vec2& vp_size,
                                const glm::mat4& view, const glm::mat4& proj);

} // namespace dse::editor
