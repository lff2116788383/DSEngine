#include "editor_viewport_panel.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace dse::editor {

void DrawSceneViewportPanel(EditorViewportPanelContext& context,
                            int& current_gizmo_operation,
                            int current_gizmo_mode,
                            bool (*build_active_camera_matrices)(entt::registry&, float, glm::mat4&, glm::mat4&)) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Scene");
    ImVec2 scene_panel_size = ImGui::GetContentRegionAvail();
    ImVec2 window_pos = ImGui::GetWindowPos();

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver()) {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        float x = (mouse_pos.x - window_pos.x) - scene_panel_size.x / 2.0f;
        float y = (mouse_pos.y - window_pos.y) - scene_panel_size.y / 2.0f;

        float min_dist = 10000.0f;
        entt::entity picked = entt::null;
        for (auto entity : context.registry.storage<entt::entity>()) {
            if (!context.registry.valid(entity)) {
                continue;
            }
            if (context.registry.all_of<TransformComponent>(entity)) {
                auto& t = context.registry.get<TransformComponent>(entity);
                float dx = (t.position.x * 50.0f) - x;
                float dy = (-t.position.y * 50.0f) - y;
                float dist = dx * dx + dy * dy;
                if (dist < 2500.0f && dist < min_dist) {
                    min_dist = dist;
                    picked = entity;
                }
            }
        }
        if (picked != entt::null) {
            context.selected_entity = picked;
        }
    }

    if (context.texture_id != 0) {
        ImGui::Image((ImTextureID)(intptr_t)context.texture_id, scene_panel_size, ImVec2(0, 1), ImVec2(1, 0));

        if (context.selected_entity != entt::null && context.registry.all_of<TransformComponent>(context.selected_entity)) {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(window_pos.x, window_pos.y, scene_panel_size.x, scene_panel_size.y);

            glm::mat4 view_matrix(1.0f);
            glm::mat4 proj_matrix(1.0f);
            const float panel_aspect = scene_panel_size.y > 0.0f ? (scene_panel_size.x / scene_panel_size.y) : 1.7777f;
            if (!build_active_camera_matrices || !build_active_camera_matrices(context.registry, panel_aspect, view_matrix, proj_matrix)) {
                view_matrix = glm::lookAt(glm::vec3(0, 0, 10), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
                proj_matrix = glm::perspective(glm::radians(45.0f), panel_aspect, 0.1f, 1000.0f);
            }

            auto& transform = context.registry.get<TransformComponent>(context.selected_entity);
            glm::mat4 model_matrix = transform.local_to_world;

            ImGuizmo::OPERATION gizmo_op = ImGuizmo::TRANSLATE;
            if (current_gizmo_operation == 0) gizmo_op = ImGuizmo::TRANSLATE;
            else if (current_gizmo_operation == 1) gizmo_op = ImGuizmo::ROTATE;
            else if (current_gizmo_operation == 2) gizmo_op = ImGuizmo::SCALE;

            ImGuizmo::MODE gizmo_mode = (current_gizmo_mode == 0) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

            if (ImGui::IsKeyPressed(ImGuiKey_W)) current_gizmo_operation = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) current_gizmo_operation = 1;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) current_gizmo_operation = 2;

            if (current_gizmo_operation != -1 && ImGuizmo::Manipulate(glm::value_ptr(view_matrix), glm::value_ptr(proj_matrix), gizmo_op, gizmo_mode, glm::value_ptr(model_matrix))) {
                glm::vec3 translation, rotation, scale;
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model_matrix), glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale));
                transform.position = translation;
                transform.rotation = glm::quat(glm::radians(rotation));
                transform.scale = scale;
                transform.dirty = true;
            }
        }
    } else {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetCursorScreenPos();
        ImVec2 p_max = ImVec2(p_min.x + scene_panel_size.x, p_min.y + scene_panel_size.y);
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(40, 40, 40, 255));
        for (float i = 0; i < scene_panel_size.x; i += 50) {
            draw_list->AddLine(ImVec2(p_min.x + i, p_min.y), ImVec2(p_min.x + i, p_max.y), IM_COL32(60, 60, 60, 255));
        }
        for (float i = 0; i < scene_panel_size.y; i += 50) {
            draw_list->AddLine(ImVec2(p_min.x, p_min.y + i), ImVec2(p_max.x, p_min.y + i), IM_COL32(60, 60, 60, 255));
        }
        draw_list->AddText(ImVec2(p_min.x + scene_panel_size.x / 2 - 50, p_min.y + scene_panel_size.y / 2), IM_COL32(200, 200, 200, 255), "Scene View");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void DrawGameViewportPanel(unsigned int texture_id) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Game");
    ImVec2 game_panel_size = ImGui::GetContentRegionAvail();
    if (texture_id != 0) {
        ImGui::Image((ImTextureID)(intptr_t)texture_id, game_panel_size, ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetCursorScreenPos();
        ImVec2 p_max = ImVec2(p_min.x + game_panel_size.x, p_min.y + game_panel_size.y);
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(20, 20, 20, 255));
        draw_list->AddText(ImVec2(p_min.x + game_panel_size.x / 2 - 40, p_min.y + game_panel_size.y / 2), IM_COL32(150, 150, 150, 255), "Game View");
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace dse::editor
