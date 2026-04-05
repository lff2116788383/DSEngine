#include "editor_hierarchy_panel.h"

#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "imgui.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include <string>

namespace dse::editor {

void DrawHierarchyPanel(EditorHierarchyPanelContext& context) {
    ImGui::Begin("Hierarchy");

    bool hierarchy_clicked = ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0);

    if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto entity : context.registry.storage<entt::entity>()) {
            if (!context.registry.valid(entity)) {
                continue;
            }

            if (context.registry.all_of<ParentComponent>(entity) &&
                context.registry.get<ParentComponent>(entity).parent != entt::null) {
                continue;
            }

            std::string entity_name = "Entity " + std::to_string(static_cast<uint32_t>(entity));
            if (context.registry.all_of<EditorNameComponent>(entity)) {
                entity_name = context.registry.get<EditorNameComponent>(entity).name;
            }

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                                       ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                       ImGuiTreeNodeFlags_SpanAvailWidth;
            if (context.selected_entity == entity) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", entity_name.c_str());
            if (ImGui::IsItemClicked()) {
                context.selected_entity = entity;
                hierarchy_clicked = false;
            }
        }
        ImGui::TreePop();
    }

    if (hierarchy_clicked) {
        context.selected_entity = entt::null;
    }

    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::MenuItem("Create Empty Entity")) {
            auto new_ent = context.world.CreateEntity();
            context.registry.emplace<EditorNameComponent>(new_ent, "New Entity");
            context.registry.emplace<TransformComponent>(new_ent);
            context.selected_entity = new_ent;
        }
        if (ImGui::MenuItem("Create UI Entity")) {
            auto new_ent = context.world.CreateEntity();
            context.registry.emplace<EditorNameComponent>(new_ent, "New UI Element");
            context.registry.emplace<TransformComponent>(new_ent);
            context.registry.emplace<UIRendererComponent>(new_ent);
            context.selected_entity = new_ent;
        }
        if (ImGui::BeginMenu("Create 3D Object")) {
            if (ImGui::MenuItem("Camera 3D")) {
                auto new_ent = context.world.CreateEntity();
                context.registry.emplace<EditorNameComponent>(new_ent, "Camera 3D");
                context.registry.emplace<TransformComponent>(new_ent, glm::vec3(0, 0, 5));
                context.registry.emplace<dse::Camera3DComponent>(new_ent);
                context.selected_entity = new_ent;
            }
            if (ImGui::MenuItem("Directional Light")) {
                auto new_ent = context.world.CreateEntity();
                context.registry.emplace<EditorNameComponent>(new_ent, "Directional Light");
                auto& t = context.registry.emplace<TransformComponent>(new_ent, glm::vec3(0, 5, 0));
                t.rotation = glm::quat(glm::vec3(glm::radians(-45.0f), glm::radians(-30.0f), 0.0f));
                context.registry.emplace<dse::DirectionalLight3DComponent>(new_ent);
                context.selected_entity = new_ent;
            }
            if (ImGui::MenuItem("Cube")) {
                auto new_ent = context.world.CreateEntity();
                context.registry.emplace<EditorNameComponent>(new_ent, "Cube");
                context.registry.emplace<TransformComponent>(new_ent);
                auto& mesh = context.registry.emplace<dse::MeshRendererComponent>(new_ent);
                mesh.mesh_path = "data/models/cube.dmesh";
                context.selected_entity = new_ent;
            }
            if (ImGui::MenuItem("Physics Box")) {
                auto new_ent = context.world.CreateEntity();
                context.registry.emplace<EditorNameComponent>(new_ent, "Physics Box");
                context.registry.emplace<TransformComponent>(new_ent, glm::vec3(0, 5, 0));
                auto& mesh = context.registry.emplace<dse::MeshRendererComponent>(new_ent);
                mesh.mesh_path = "data/models/cube.dmesh";
                context.registry.emplace<dse::RigidBody3DComponent>(new_ent);
                context.registry.emplace<dse::BoxCollider3DComponent>(new_ent);
                context.selected_entity = new_ent;
            }
            if (ImGui::MenuItem("Particle System 3D")) {
                auto new_ent = context.world.CreateEntity();
                context.registry.emplace<EditorNameComponent>(new_ent, "Particle 3D");
                context.registry.emplace<TransformComponent>(new_ent);
                context.registry.emplace<dse::ParticleSystem3DComponent>(new_ent);
                context.selected_entity = new_ent;
            }
            ImGui::EndMenu();
        }
        if (context.selected_entity != entt::null && ImGui::MenuItem("Delete Entity")) {
            context.world.DestroyEntity(context.selected_entity);
            context.selected_entity = entt::null;
        }
        if (context.selected_entity != entt::null && ImGui::MenuItem("Duplicate Entity")) {
            auto new_ent = context.world.CreateEntity();
            if (context.registry.all_of<EditorNameComponent>(context.selected_entity)) {
                context.registry.emplace<EditorNameComponent>(
                    new_ent,
                    context.registry.get<EditorNameComponent>(context.selected_entity).name + " (Copy)");
            }
            if (context.registry.all_of<TransformComponent>(context.selected_entity)) {
                context.registry.emplace<TransformComponent>(
                    new_ent,
                    context.registry.get<TransformComponent>(context.selected_entity));
            }
            context.selected_entity = new_ent;
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace dse::editor
