#pragma once

#include "editor_context.h"
#include <glm/mat4x4.hpp>

namespace dse::editor {

void DrawSceneViewportPanel(EditorContext& ctx,
                            unsigned int scene_texture_id,
                            bool (*build_active_camera_matrices)(entt::registry&, float, glm::mat4&, glm::mat4&));

void DrawGameViewportPanel(unsigned int texture_id);

} // namespace dse::editor
