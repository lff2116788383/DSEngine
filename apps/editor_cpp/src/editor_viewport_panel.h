#pragma once

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>

namespace dse::editor {

struct EditorViewportPanelContext {
    entt::registry& registry;
    entt::entity& selected_entity;
    unsigned int texture_id = 0;
};

void DrawSceneViewportPanel(EditorViewportPanelContext& context,
                            int& current_gizmo_operation,
                            int current_gizmo_mode,
                            bool (*build_active_camera_matrices)(entt::registry&, float, glm::mat4&, glm::mat4&));

void DrawGameViewportPanel(unsigned int texture_id);

} // namespace dse::editor
