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
            const entt::entity source = context.selected_entity;
            auto new_ent = context.world.CreateEntity();
            auto copy_component = [&](auto type_tag) {
                using Component = decltype(type_tag);
                if (context.registry.all_of<Component>(source) && !context.registry.all_of<Component>(new_ent)) {
                    context.registry.emplace<Component>(new_ent, context.registry.get<Component>(source));
                }
            };
            auto copy_runtime_reset_component = [&](auto type_tag, auto reset_runtime) {
                using Component = decltype(type_tag);
                if (context.registry.all_of<Component>(source) && !context.registry.all_of<Component>(new_ent)) {
                    auto component = context.registry.get<Component>(source);
                    reset_runtime(component);
                    context.registry.emplace<Component>(new_ent, std::move(component));
                }
            };

            copy_component(EditorNameComponent{});
            if (context.registry.all_of<EditorNameComponent>(new_ent)) {
                context.registry.get<EditorNameComponent>(new_ent).name += " (Copy)";
            }

            copy_component(TransformComponent{});
            copy_component(SpriteRendererComponent{});
            copy_runtime_reset_component(UIRendererComponent{}, [](UIRendererComponent& ui) {
                ui.is_hovered = false;
                ui.is_pressed = false;
                ui.runtime_model = glm::mat4(1.0f);
            });
            copy_runtime_reset_component(UILabelComponent{}, [](UILabelComponent& label) {
                label.runtime_glyph_entities.clear();
                label.dirty = true;
            });
            copy_component(UIAnchorComponent{});
            copy_component(UIGridLayoutComponent{});
            copy_component(UICanvasScalerComponent{});
            copy_component(UIAnimationComponent{});
            copy_runtime_reset_component(UIRichTextComponent{}, [](UIRichTextComponent& rich) {
                rich.dirty = true;
            });
            copy_runtime_reset_component(RigidBody2DComponent{}, [](RigidBody2DComponent& rigidbody) {
                rigidbody.runtime_body = nullptr;
            });
            copy_runtime_reset_component(ParticleEmitterComponent{}, [](ParticleEmitterComponent& emitter) {
                emitter.particles.clear();
                emitter.emit_accumulator = 0.0f;
                emitter.pending_burst = 0;
            });
            copy_component(dse::Camera3DComponent{});
            copy_component(dse::DirectionalLight3DComponent{});
            copy_component(dse::PointLightComponent{});
            copy_component(dse::MeshRendererComponent{});
            copy_component(dse::Animator3DComponent{});
            copy_component(dse::FreeCameraControllerComponent{});
            copy_component(dse::TerrainComponent{});
            copy_runtime_reset_component(dse::RigidBody3DComponent{}, [](dse::RigidBody3DComponent& rigidbody) {
                rigidbody.runtime_body = nullptr;
            });
            copy_runtime_reset_component(dse::BoxCollider3DComponent{}, [](dse::BoxCollider3DComponent& collider) {
                collider.runtime_shape = nullptr;
            });
            copy_runtime_reset_component(dse::SphereCollider3DComponent{}, [](dse::SphereCollider3DComponent& collider) {
                collider.runtime_shape = nullptr;
            });
            copy_runtime_reset_component(dse::ParticleSystem3DComponent{}, [](dse::ParticleSystem3DComponent& particle_system) {
                particle_system.particles.clear();
                particle_system.emission_accumulator = 0.0f;
                particle_system.active_particle_count = 0;
                particle_system.instance_vbo = 0;
                particle_system.texture_handle = 0;
                particle_system.initialized = false;
            });
            copy_component(dse::PostProcessComponent{});

            if (context.registry.all_of<TransformComponent>(new_ent)) {
                context.registry.get<TransformComponent>(new_ent).dirty = true;
            }
            context.selected_entity = new_ent;
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace dse::editor
