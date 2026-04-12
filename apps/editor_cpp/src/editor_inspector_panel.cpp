#include "editor_inspector_panel.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "modules/gameplay_3d/animation/animator_system.h"
#include "imgui.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <cstring>
#include <string>
#include <variant>

#include "editor_shared_components.h"
#include "editor_toolbar.h"

namespace dse::editor {

namespace {

#define INSPECTOR_PROPERTY(label, code) \
    ImGui::AlignTextToFramePadding(); \
    ImGui::Text(label); \
    ImGui::NextColumn(); \
    ImGui::SetNextItemWidth(-1); \
    code; \
    ImGui::NextColumn();

bool IsInspectorReadOnly(const EditorInspectorPanelContext& context) {
    return IsEditorInPlayMode() && !context.is_2d;
}

void BeginInspectorReadOnlyScope(const EditorInspectorPanelContext& context) {
    if (IsInspectorReadOnly(context)) {
        ImGui::BeginDisabled(true);
    }
}

void EndInspectorReadOnlyScope(const EditorInspectorPanelContext& context) {
    if (IsInspectorReadOnly(context)) {
        ImGui::EndDisabled();
    }
}

void MarkSpriteRendererDirty(SpriteRendererComponent& sprite) {
    sprite.texture.reset();
}

void MarkRigidBody2DDirty(RigidBody2DComponent& rb2d) {
    rb2d.runtime_body = nullptr;
}

void MarkParticleEmitterDirty(ParticleEmitterComponent& emitter) {
    emitter.particles.clear();
    emitter.pending_burst = 0;
}

void DrawInspectorHeader(EditorInspectorPanelContext& context) {
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
    ImGui::AlignTextToFramePadding();
    ImGui::Checkbox("##Active", &context.inspector_active);
    ImGui::SameLine();

    char name_buf[64] = "";
    if (context.registry.all_of<EditorNameComponent>(context.selected_entity)) {
        std::strncpy(name_buf, context.registry.get<EditorNameComponent>(context.selected_entity).name.c_str(), sizeof(name_buf) - 1);
    } else {
        std::strncpy(name_buf, "Entity", sizeof(name_buf) - 1);
    }

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 70);
    if (ImGui::InputText("##Name", name_buf, sizeof(name_buf))) {
        if (context.registry.all_of<EditorNameComponent>(context.selected_entity)) {
            context.registry.get<EditorNameComponent>(context.selected_entity).name = name_buf;
        } else {
            context.registry.emplace<EditorNameComponent>(context.selected_entity, name_buf);
        }
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::Checkbox("Static", &context.inspector_static);
    ImGui::Separator();
}

void DrawTransformSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<TransformComponent>(context.selected_entity)) {
        return;
    }

    auto& transform = context.registry.get<TransformComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "transform_cols", false);
    ImGui::SetColumnWidth(0, 80.0f);

    float pos[3] = {transform.position.x, transform.position.y, transform.position.z};
    INSPECTOR_PROPERTY("Position", if (ImGui::DragFloat3("##pos", pos, 0.1f)) {
        transform.position = glm::vec3(pos[0], pos[1], pos[2]);
        transform.dirty = true;
    });

    glm::vec3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
    float rot[3] = {euler.x, euler.y, euler.z};
    if (context.is_2d) {
        INSPECTOR_PROPERTY("Rotation", if (ImGui::DragFloat("##rotZ", &rot[2], 0.1f)) {
            euler.z = rot[2];
            transform.rotation = glm::quat(glm::radians(euler));
            transform.dirty = true;
        });
    } else {
        INSPECTOR_PROPERTY("Rotation", if (ImGui::DragFloat3("##rot", rot, 0.1f)) {
            euler = glm::vec3(rot[0], rot[1], rot[2]);
            transform.rotation = glm::quat(glm::radians(euler));
            transform.dirty = true;
        });
    }

    float scale[3] = {transform.scale.x, transform.scale.y, transform.scale.z};
    INSPECTOR_PROPERTY("Scale", if (ImGui::DragFloat3("##scale", scale, 0.1f)) {
        transform.scale = glm::vec3(scale[0], scale[1], scale[2]);
        transform.dirty = true;
    });

    ImGui::Columns(1);
}

void DrawSpriteRendererSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<SpriteRendererComponent>(context.selected_entity)) {
        return;
    }

    auto& sprite = context.registry.get<SpriteRendererComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "spriterenderer_cols", false);
    ImGui::SetColumnWidth(0, 80.0f);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Shader");
    ImGui::NextColumn();
    ImGui::SetNextItemWidth(-1);
    ImGui::Button(sprite.shader_variant.empty() ? "None" : sprite.shader_variant.c_str(), ImVec2(-1, 0));
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            sprite.shader_variant = path;
            MarkSpriteRendererDirty(sprite);
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::NextColumn();

    float color[4] = {sprite.color.r, sprite.color.g, sprite.color.b, sprite.color.a};
    INSPECTOR_PROPERTY("Color", if (ImGui::ColorEdit4("##color", color)) {
        sprite.color = glm::vec4(color[0], color[1], color[2], color[3]);
        MarkSpriteRendererDirty(sprite);
    });

    ImGui::Columns(1);
}

void DrawRigidBody2DSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<RigidBody2DComponent>(context.selected_entity)) {
        return;
    }

    auto& rb2d = context.registry.get<RigidBody2DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("RigidBody 2D", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "rb2d_cols", false);
    ImGui::SetColumnWidth(0, 80.0f);

    const char* body_types[] = { "Static", "Kinematic", "Dynamic" };
    int current_type = static_cast<int>(rb2d.type);
    INSPECTOR_PROPERTY("Body Type", if (ImGui::Combo("##type", &current_type, body_types, IM_ARRAYSIZE(body_types))) {
        rb2d.type = static_cast<RigidBody2DType>(current_type);
        MarkRigidBody2DDirty(rb2d);
    });

    float vel[2] = {rb2d.velocity.x, rb2d.velocity.y};
    INSPECTOR_PROPERTY("Velocity", if (ImGui::DragFloat2("##vel", vel, 0.1f)) {
        rb2d.velocity = glm::vec2(vel[0], vel[1]);
        MarkRigidBody2DDirty(rb2d);
    });

    INSPECTOR_PROPERTY("Gravity", if (ImGui::DragFloat("##grav", &rb2d.gravity_scale, 0.1f)) {
        MarkRigidBody2DDirty(rb2d);
    });
    ImGui::Columns(1);
}

void DrawMeshRendererSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::MeshRendererComponent>(context.selected_entity)) {
        return;
    }

    auto& mesh = context.registry.get<dse::MeshRendererComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "mesh_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);

    char mesh_buf[256] = {};
    std::strncpy(mesh_buf, mesh.mesh_path.c_str(), sizeof(mesh_buf) - 1);
    INSPECTOR_PROPERTY("Mesh Path", if (ImGui::InputText("##mesh_path", mesh_buf, sizeof(mesh_buf))) {
        mesh.mesh_path = mesh_buf;
    });

    INSPECTOR_PROPERTY("Material Instance", ImGui::DragScalar("##mat_inst", ImGuiDataType_U32, &mesh.material_instance_id, 1.0f, nullptr, nullptr, "%u"));
    ImGui::Separator();
    ImGui::Text("Material Overrides (PBR)");
    INSPECTOR_PROPERTY("Albedo Tint", ImGui::ColorEdit4("##mesh_color", glm::value_ptr(mesh.color)));
    INSPECTOR_PROPERTY("Emissive", ImGui::ColorEdit3("##mesh_emissive", glm::value_ptr(mesh.emissive)));
    INSPECTOR_PROPERTY("Metallic", ImGui::SliderFloat("##mesh_metallic", &mesh.metallic, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Roughness", ImGui::SliderFloat("##mesh_roughness", &mesh.roughness, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Ambient Occlusion", ImGui::SliderFloat("##mesh_ao", &mesh.ao, 0.0f, 1.0f));
    ImGui::Separator();
    INSPECTOR_PROPERTY("Receive Shadow", ImGui::Checkbox("##receive_shadow", &mesh.receive_shadow));

    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawCamera3DSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::Camera3DComponent>(context.selected_entity)) {
        return;
    }

    auto& camera = context.registry.get<dse::Camera3DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Camera 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "cam3d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##cam3d_enabled", &camera.enabled));
    INSPECTOR_PROPERTY("Priority", ImGui::DragInt("##cam3d_priority", &camera.priority, 1));
    INSPECTOR_PROPERTY("FOV", ImGui::DragFloat("##cam3d_fov", &camera.fov, 1.0f, 10.0f, 170.0f));
    INSPECTOR_PROPERTY("Near Clip", ImGui::DragFloat("##cam3d_near", &camera.near_clip, 0.05f, 0.01f, 10.0f));
    INSPECTOR_PROPERTY("Far Clip", ImGui::DragFloat("##cam3d_far", &camera.far_clip, 10.0f, 10.0f, 10000.0f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawDirectionalLightSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::DirectionalLight3DComponent>(context.selected_entity)) {
        return;
    }

    auto& light = context.registry.get<dse::DirectionalLight3DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "dirlight_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##dirlight_enabled", &light.enabled));
    INSPECTOR_PROPERTY("Direction", ImGui::DragFloat3("##dirlight_dir", glm::value_ptr(light.direction), 0.05f, -1.0f, 1.0f));
    INSPECTOR_PROPERTY("Color", ImGui::ColorEdit3("##dirlight_color", glm::value_ptr(light.color)));
    INSPECTOR_PROPERTY("Intensity", ImGui::DragFloat("##dirlight_int", &light.intensity, 0.05f, 0.0f, 10.0f));
    INSPECTOR_PROPERTY("Ambient", ImGui::DragFloat("##dirlight_amb", &light.ambient_intensity, 0.05f, 0.0f, 1.0f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawPointLightSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::PointLightComponent>(context.selected_entity)) {
        return;
    }

    auto& light = context.registry.get<dse::PointLightComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "ptlight_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##ptlight_enabled", &light.enabled));
    INSPECTOR_PROPERTY("Color", ImGui::ColorEdit3("##ptlight_color", glm::value_ptr(light.color)));
    INSPECTOR_PROPERTY("Intensity", ImGui::DragFloat("##ptlight_int", &light.intensity, 0.05f, 0.0f, 10.0f));
    INSPECTOR_PROPERTY("Radius", ImGui::DragFloat("##ptlight_rad", &light.radius, 0.5f, 0.1f, 1000.0f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawSkyboxSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::SkyboxComponent>(context.selected_entity)) {
        return;
    }

    auto& skybox = context.registry.get<dse::SkyboxComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "skybox_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##skybox_enabled", &skybox.enabled));

    char path_buf[256] = {};
    std::strncpy(path_buf, skybox.cubemap_path.c_str(), sizeof(path_buf) - 1);
    INSPECTOR_PROPERTY("Cubemap Path", if (ImGui::InputText("##skybox_path", path_buf, sizeof(path_buf))) {
        skybox.cubemap_path = path_buf;
    });

    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawAnimator3DSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::Animator3DComponent>(context.selected_entity)) {
        return;
    }

    auto& animator = context.registry.get<dse::Animator3DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Animator 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "anim_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##anim_enabled", &animator.enabled));

    char skel_buf[256] = {};
    std::strncpy(skel_buf, animator.dskel_path.c_str(), sizeof(skel_buf) - 1);
    INSPECTOR_PROPERTY("Skeleton Path", if (ImGui::InputText("##anim_skel", skel_buf, sizeof(skel_buf))) {
        animator.dskel_path = skel_buf;
    });

    if (animator.state_machine) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "[FSM Active]");
        INSPECTOR_PROPERTY("Current State", ImGui::Text("%s", animator.current_state_name.c_str()));
        INSPECTOR_PROPERTY("Is Transitioning", ImGui::Text("%s (%.2f%%)", animator.is_transitioning ? "Yes" : "No", animator.transition_progress * 100.0f));
        if (animator.is_transitioning) {
            INSPECTOR_PROPERTY("Next State", ImGui::Text("%s", animator.next_state_name.c_str()));
        }

        ImGui::Separator();
        ImGui::Text("FSM Parameters:");
        for (const auto& kv : animator.state_machine->GetParameters()) {
            if (kv.second.type == dse::gameplay3d::AnimParamType::Float) {
                float val = std::get<float>(kv.second.value);
                INSPECTOR_PROPERTY(kv.first.c_str(), if (ImGui::DragFloat((std::string("##fsm_") + kv.first).c_str(), &val, 0.1f)) {
                    animator.state_machine->SetFloat(kv.first, val);
                });
            } else if (kv.second.type == dse::gameplay3d::AnimParamType::Trigger) {
                INSPECTOR_PROPERTY(kv.first.c_str(), if (ImGui::Button((std::string("Trigger##fsm_") + kv.first).c_str())) {
                    animator.state_machine->SetTrigger(kv.first);
                });
            }
        }
    } else {
        char anim_buf[256] = {};
        std::strncpy(anim_buf, animator.danim_path.c_str(), sizeof(anim_buf) - 1);
        INSPECTOR_PROPERTY("Anim Path", if (ImGui::InputText("##anim_path", anim_buf, sizeof(anim_buf))) {
            animator.danim_path = anim_buf;
        });
        INSPECTOR_PROPERTY("Speed", ImGui::DragFloat("##anim_speed", &animator.speed, 0.1f, 0.0f, 10.0f));
        INSPECTOR_PROPERTY("Loop", ImGui::Checkbox("##anim_loop", &animator.loop));
    }

    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawFreeCameraControllerSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::FreeCameraControllerComponent>(context.selected_entity)) {
        return;
    }

    auto& controller = context.registry.get<dse::FreeCameraControllerComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Free Camera Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "freecam_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##freecam_enabled", &controller.enabled));
    INSPECTOR_PROPERTY("Move Speed", ImGui::DragFloat("##freecam_speed", &controller.move_speed, 0.1f, 0.1f, 100.0f));
    INSPECTOR_PROPERTY("Sensitivity", ImGui::DragFloat("##freecam_sens", &controller.mouse_sensitivity, 0.01f, 0.01f, 2.0f));
    INSPECTOR_PROPERTY("Pitch", ImGui::DragFloat("##freecam_pitch", &controller.pitch, 1.0f, -89.0f, 89.0f));
    INSPECTOR_PROPERTY("Yaw", ImGui::DragFloat("##freecam_yaw", &controller.yaw, 1.0f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawTerrainSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::TerrainComponent>(context.selected_entity)) {
        return;
    }

    auto& terrain = context.registry.get<dse::TerrainComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "terrain_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##terrain_enabled", &terrain.enabled));

    char path_buf[256] = {};
    std::strncpy(path_buf, terrain.heightmap_path.c_str(), sizeof(path_buf) - 1);
    INSPECTOR_PROPERTY("Heightmap Path", if (ImGui::InputText("##terrain_path", path_buf, sizeof(path_buf))) {
        terrain.heightmap_path = path_buf;
        terrain.is_dirty = true;
    });

    INSPECTOR_PROPERTY("Width", if (ImGui::DragFloat("##terrain_width", &terrain.width, 1.0f, 10.0f, 1000.0f)) { terrain.is_dirty = true; });
    INSPECTOR_PROPERTY("Depth", if (ImGui::DragFloat("##terrain_depth", &terrain.depth, 1.0f, 10.0f, 1000.0f)) { terrain.is_dirty = true; });
    INSPECTOR_PROPERTY("Max Height", if (ImGui::DragFloat("##terrain_height", &terrain.max_height, 0.5f, 1.0f, 200.0f)) { terrain.is_dirty = true; });
    INSPECTOR_PROPERTY("Dynamic LOD", ImGui::Checkbox("##terrain_lod", &terrain.use_dynamic_lod));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawUILabelSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<UILabelComponent>(context.selected_entity)) {
        return;
    }

    auto& label = context.registry.get<UILabelComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("UI Label", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "uilabel_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);

    char text_buf[256] = {};
    std::strncpy(text_buf, label.text.c_str(), sizeof(text_buf) - 1);
    INSPECTOR_PROPERTY("Text", if (ImGui::InputText("##label_text", text_buf, sizeof(text_buf))) {
        label.text = text_buf;
        label.dirty = true;
    });

    ImGui::Separator();
    ImGui::Text("Localization");
    INSPECTOR_PROPERTY("Enable Loc", if (ImGui::Checkbox("##use_loc", &label.use_localization)) {
        label.dirty = true;
    });

    if (label.use_localization) {
        char key_buf[128] = {};
        std::strncpy(key_buf, label.localization_key.c_str(), sizeof(key_buf) - 1);
        INSPECTOR_PROPERTY("Loc Key", if (ImGui::InputText("##loc_key", key_buf, sizeof(key_buf))) {
            label.localization_key = key_buf;
            label.dirty = true;
        });

        char fallback_buf[256] = {};
        std::strncpy(fallback_buf, label.fallback_text.c_str(), sizeof(fallback_buf) - 1);
        INSPECTOR_PROPERTY("Fallback", if (ImGui::InputText("##fallback_text", fallback_buf, sizeof(fallback_buf))) {
            label.fallback_text = fallback_buf;
            label.dirty = true;
        });

        ImGui::Separator();
        ImGui::Text("Loc Parameters");

        std::string key_to_erase;
        for (auto& [param_key, param_value] : label.localization_params) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("{%s}", param_key.c_str());
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(-30.0f);

            char val_buf[128] = {};
            std::strncpy(val_buf, param_value.c_str(), sizeof(val_buf) - 1);
            if (ImGui::InputText((std::string("##val_") + param_key).c_str(), val_buf, sizeof(val_buf))) {
                param_value = val_buf;
                label.dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button((std::string("X##") + param_key).c_str(), ImVec2(24, 0))) {
                key_to_erase = param_key;
            }
            ImGui::NextColumn();
        }

        if (!key_to_erase.empty()) {
            label.localization_params.erase(key_to_erase);
            label.dirty = true;
        }

        static char new_param_name[64] = "";
        static char new_param_value[128] = "";
        INSPECTOR_PROPERTY("New Param", ImGui::InputTextWithHint("##new_param_name", "Key (e.g. name)", new_param_name, sizeof(new_param_name)));
        INSPECTOR_PROPERTY("New Value", ImGui::InputTextWithHint("##new_param_value", "Value", new_param_value, sizeof(new_param_value)));
        INSPECTOR_PROPERTY("", if (ImGui::Button("Add Parameter", ImVec2(-1, 0))) {
            if (std::strlen(new_param_name) > 0) {
                label.localization_params[new_param_name] = new_param_value;
                label.dirty = true;
                new_param_name[0] = '\0';
                new_param_value[0] = '\0';
            }
        });
    }

    ImGui::Separator();
    ImGui::Text("Appearance");
    float color[4] = {label.color.r, label.color.g, label.color.b, label.color.a};
    INSPECTOR_PROPERTY("Color", if (ImGui::ColorEdit4("##label_color", color)) {
        label.color = glm::vec4(color[0], color[1], color[2], color[3]);
        label.dirty = true;
    });
    INSPECTOR_PROPERTY("Glyph Size", if (ImGui::DragFloat2("##glyph_size", glm::value_ptr(label.glyph_size), 1.0f, 1.0f, 256.0f)) {
        label.dirty = true;
    });
    INSPECTOR_PROPERTY("Spacing", if (ImGui::DragFloat("##spacing", &label.spacing, 0.1f)) {
        label.dirty = true;
    });

    ImGui::Columns(1);
}

void DrawParticleEmitterSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<ParticleEmitterComponent>(context.selected_entity)) {
        return;
    }

    auto& emitter = context.registry.get<ParticleEmitterComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "particle_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    INSPECTOR_PROPERTY("Emitting", if (ImGui::Checkbox("##emitting", &emitter.emitting)) { MarkParticleEmitterDirty(emitter); });
    INSPECTOR_PROPERTY("Max Particles", if (ImGui::DragInt("##max_particles", &emitter.max_particles, 1.0f, 1, 5000)) { MarkParticleEmitterDirty(emitter); });
    INSPECTOR_PROPERTY("Emit Rate", if (ImGui::DragFloat("##emit_rate", &emitter.emit_rate, 0.1f, 0.0f, 1000.0f)) { MarkParticleEmitterDirty(emitter); });
    INSPECTOR_PROPERTY("Burst", if (ImGui::Button("Emit 10", ImVec2(-1, 0))) { emitter.pending_burst += 10; });
    INSPECTOR_PROPERTY("Gravity", if (ImGui::DragFloat3("##gravity", glm::value_ptr(emitter.gravity), 0.05f)) { MarkParticleEmitterDirty(emitter); });

    ImGui::Separator();
    ImGui::Text("Randomize Params");
    INSPECTOR_PROPERTY("Enable Random", if (ImGui::Checkbox("##random_params", &emitter.use_random_params)) { MarkParticleEmitterDirty(emitter); });

    if (emitter.use_random_params) {
        INSPECTOR_PROPERTY("Life Time", if (ImGui::DragFloat2("##life_time_range", &emitter.life_time_min, 0.05f, 0.05f, 30.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Size", if (ImGui::DragFloat2("##size_range", &emitter.size_min, 0.05f, 0.01f, 100.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Velocity Min", if (ImGui::DragFloat3("##vel_min", glm::value_ptr(emitter.velocity_min), 0.1f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Velocity Max", if (ImGui::DragFloat3("##vel_max", glm::value_ptr(emitter.velocity_max), 0.1f)) { MarkParticleEmitterDirty(emitter); });
    } else {
        INSPECTOR_PROPERTY("Life Time", if (ImGui::DragFloat("##life_time", &emitter.start_life_time, 0.05f, 0.05f, 30.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Start Size", if (ImGui::DragFloat("##start_size", &emitter.start_size, 0.05f, 0.01f, 100.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Start Color", if (ImGui::ColorEdit4("##start_color", glm::value_ptr(emitter.start_color))) { MarkParticleEmitterDirty(emitter); });
    }

    auto draw_curve_inspector = [&emitter](const char* label_text, ParticleCurve& curve, float min_val, float max_val) {
        INSPECTOR_PROPERTY(label_text, if (ImGui::Checkbox((std::string("##enabled_") + label_text).c_str(), &curve.enabled)) { MarkParticleEmitterDirty(emitter); });
        if (curve.enabled) {
            const char* curve_types[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut" };
            int current_type = static_cast<int>(curve.type);
            INSPECTOR_PROPERTY("  Type", if (ImGui::Combo((std::string("##type_") + label_text).c_str(), &current_type, curve_types, IM_ARRAYSIZE(curve_types))) {
                curve.type = static_cast<ParticleCurveType>(current_type);
                MarkParticleEmitterDirty(emitter);
            });
            INSPECTOR_PROPERTY("  Start", if (ImGui::DragFloat((std::string("##start_") + label_text).c_str(), &curve.start_value, 0.05f, min_val, max_val)) { MarkParticleEmitterDirty(emitter); });
            INSPECTOR_PROPERTY("  End", if (ImGui::DragFloat((std::string("##end_") + label_text).c_str(), &curve.end_value, 0.05f, min_val, max_val)) { MarkParticleEmitterDirty(emitter); });
        }
    };

    ImGui::Separator();
    ImGui::Text("Curves over Lifetime");
    draw_curve_inspector("Size Curve", emitter.size_curve, 0.0f, 100.0f);
    draw_curve_inspector("Alpha Curve", emitter.alpha_curve, 0.0f, 1.0f);
    draw_curve_inspector("Speed Curve", emitter.speed_curve, 0.0f, 10.0f);

    ImGui::Separator();
    ImGui::Text("Collision");
    const char* collision_modes[] = { "None", "GroundPlane", "Box2D" };
    int collision_mode = static_cast<int>(emitter.collision_mode);
    INSPECTOR_PROPERTY("Mode", if (ImGui::Combo("##collision_mode", &collision_mode, collision_modes, IM_ARRAYSIZE(collision_modes))) {
        emitter.collision_mode = static_cast<ParticleCollisionMode>(collision_mode);
        MarkParticleEmitterDirty(emitter);
    });

    if (emitter.collision_mode != ParticleCollisionMode::None) {
        INSPECTOR_PROPERTY("Bounce", if (ImGui::DragFloat("##collision_bounce", &emitter.collision_bounce, 0.01f, 0.0f, 1.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Friction", if (ImGui::DragFloat("##collision_friction", &emitter.collision_friction, 0.01f, 0.0f, 1.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Life Loss", if (ImGui::DragFloat("##collision_life_loss", &emitter.collision_life_loss, 0.01f, 0.0f, 1.0f)) { MarkParticleEmitterDirty(emitter); });
        if (emitter.collision_mode == ParticleCollisionMode::GroundPlane) {
            INSPECTOR_PROPERTY("Ground Y", if (ImGui::DragFloat("##ground_y", &emitter.ground_y, 0.05f)) { MarkParticleEmitterDirty(emitter); });
        }
    }

    ImGui::Columns(1);
    ImGui::TextDisabled("Active Particles: %d", static_cast<int>(emitter.particles.size()));
}

void DrawRigidBody3DSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::RigidBody3DComponent>(context.selected_entity)) {
        return;
    }

    auto& rb = context.registry.get<dse::RigidBody3DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("RigidBody 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "rb3d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    const char* types[] = { "Static", "Kinematic", "Dynamic" };
    int type_idx = static_cast<int>(rb.type);
    INSPECTOR_PROPERTY("Body Type", if (ImGui::Combo("##rb3d_type", &type_idx, types, IM_ARRAYSIZE(types))) {
        rb.type = static_cast<dse::RigidBody3DType>(type_idx);
    });
    INSPECTOR_PROPERTY("Mass", ImGui::DragFloat("##rb3d_mass", &rb.mass, 0.1f, 0.0f));
    INSPECTOR_PROPERTY("Drag", ImGui::DragFloat("##rb3d_drag", &rb.drag, 0.05f, 0.0f));
    INSPECTOR_PROPERTY("Angular Drag", ImGui::DragFloat("##rb3d_angdrag", &rb.angular_drag, 0.05f, 0.0f));
    INSPECTOR_PROPERTY("Use Gravity", ImGui::Checkbox("##rb3d_grav", &rb.use_gravity));
    INSPECTOR_PROPERTY("Gravity Scale", ImGui::DragFloat("##rb3d_gscale", &rb.gravity_scale, 0.1f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawBoxCollider3DSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::BoxCollider3DComponent>(context.selected_entity)) {
        return;
    }

    auto& collider = context.registry.get<dse::BoxCollider3DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Box Collider 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "boxcol3d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Size", ImGui::DragFloat3("##boxcol3d_size", glm::value_ptr(collider.size), 0.1f, 0.01f));
    INSPECTOR_PROPERTY("Center", ImGui::DragFloat3("##boxcol3d_center", glm::value_ptr(collider.center), 0.1f));
    INSPECTOR_PROPERTY("Is Trigger", ImGui::Checkbox("##boxcol3d_trigger", &collider.is_trigger));
    INSPECTOR_PROPERTY("Bounciness", ImGui::DragFloat("##boxcol3d_bounce", &collider.bounciness, 0.05f, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Friction", ImGui::DragFloat("##boxcol3d_fric", &collider.friction, 0.05f, 0.0f, 1.0f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawSphereCollider3DSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::SphereCollider3DComponent>(context.selected_entity)) {
        return;
    }

    auto& collider = context.registry.get<dse::SphereCollider3DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Sphere Collider 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "sphcol3d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Radius", ImGui::DragFloat("##sphcol3d_rad", &collider.radius, 0.1f, 0.01f));
    INSPECTOR_PROPERTY("Center", ImGui::DragFloat3("##sphcol3d_center", glm::value_ptr(collider.center), 0.1f));
    INSPECTOR_PROPERTY("Is Trigger", ImGui::Checkbox("##sphcol3d_trigger", &collider.is_trigger));
    INSPECTOR_PROPERTY("Bounciness", ImGui::DragFloat("##sphcol3d_bounce", &collider.bounciness, 0.05f, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Friction", ImGui::DragFloat("##sphcol3d_fric", &collider.friction, 0.05f, 0.0f, 1.0f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawParticleSystem3DSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::ParticleSystem3DComponent>(context.selected_entity)) {
        return;
    }

    auto& ps = context.registry.get<dse::ParticleSystem3DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Particle System 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "ps3d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##ps3d_en", &ps.enabled));
    INSPECTOR_PROPERTY("Max Particles", ImGui::Text("%d (Active: %d)", ps.max_particles, ps.active_particle_count));
    INSPECTOR_PROPERTY("Emission Rate", ImGui::DragFloat("##ps3d_rate", &ps.emission_rate, 1.0f, 0.0f, 10000.0f));

    ImGui::Separator();
    ImGui::Text("Life"); ImGui::NextColumn(); ImGui::NextColumn();
    INSPECTOR_PROPERTY("Min", ImGui::DragFloat("##ps3d_lmin", &ps.start_life_min, 0.1f, 0.1f, 10.0f));
    INSPECTOR_PROPERTY("Max", ImGui::DragFloat("##ps3d_lmax", &ps.start_life_max, 0.1f, 0.1f, 10.0f));

    ImGui::Separator();
    ImGui::Text("Size"); ImGui::NextColumn(); ImGui::NextColumn();
    INSPECTOR_PROPERTY("Min", ImGui::DragFloat("##ps3d_smin", &ps.start_size_min, 0.05f, 0.01f, 5.0f));
    INSPECTOR_PROPERTY("Max", ImGui::DragFloat("##ps3d_smax", &ps.start_size_max, 0.05f, 0.01f, 5.0f));

    ImGui::Separator();
    ImGui::Text("Speed"); ImGui::NextColumn(); ImGui::NextColumn();
    INSPECTOR_PROPERTY("Min", ImGui::DragFloat("##ps3d_spmin", &ps.start_speed_min, 0.1f, 0.0f, 50.0f));
    INSPECTOR_PROPERTY("Max", ImGui::DragFloat("##ps3d_spmax", &ps.start_speed_max, 0.1f, 0.0f, 50.0f));
    INSPECTOR_PROPERTY("Color", ImGui::ColorEdit4("##ps3d_color", glm::value_ptr(ps.start_color)));
    INSPECTOR_PROPERTY("Gravity", ImGui::DragFloat3("##ps3d_grav", glm::value_ptr(ps.gravity), 0.1f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawPostProcessSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::PostProcessComponent>(context.selected_entity)) {
        return;
    }

    auto& pp = context.registry.get<dse::PostProcessComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "pp_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##pp_enabled", &pp.enabled));
    INSPECTOR_PROPERTY("Exposure", ImGui::DragFloat("##pp_exp", &pp.exposure, 0.05f, 0.0f, 10.0f));
    ImGui::Separator();
    ImGui::Text("Bloom");
    ImGui::NextColumn(); ImGui::NextColumn();
    INSPECTOR_PROPERTY("Bloom Enabled", ImGui::Checkbox("##pp_bloom_en", &pp.bloom_enabled));
    INSPECTOR_PROPERTY("Threshold", ImGui::DragFloat("##pp_bloom_thresh", &pp.bloom_threshold, 0.05f, 0.0f, 10.0f));
    INSPECTOR_PROPERTY("Intensity", ImGui::DragFloat("##pp_bloom_int", &pp.bloom_intensity, 0.05f, 0.0f, 10.0f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawAddComponentSection(EditorInspectorPanelContext& context) {
    const bool read_only = IsInspectorReadOnly(context);
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 60);
    if (read_only) {
        ImGui::BeginDisabled(true);
    }
    if (ImGui::Button("Add Component", ImVec2(120, 30))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (!ImGui::BeginPopup("AddComponentPopup")) {
        return;
    }

    if (ImGui::MenuItem("Transform") && !context.registry.all_of<TransformComponent>(context.selected_entity)) {
        context.registry.emplace<TransformComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Name") && !context.registry.all_of<EditorNameComponent>(context.selected_entity)) {
        context.registry.emplace<EditorNameComponent>(context.selected_entity, "New Component");
    }
    if (ImGui::MenuItem("Sprite Renderer") && !context.registry.all_of<SpriteRendererComponent>(context.selected_entity)) {
        context.registry.emplace<SpriteRendererComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("RigidBody 2D") && !context.registry.all_of<RigidBody2DComponent>(context.selected_entity)) {
        context.registry.emplace<RigidBody2DComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Mesh Renderer") && !context.registry.all_of<dse::MeshRendererComponent>(context.selected_entity)) {
        context.registry.emplace<dse::MeshRendererComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("UI Label") && !context.registry.all_of<UILabelComponent>(context.selected_entity)) {
        auto& label = context.registry.emplace<UILabelComponent>(context.selected_entity);
        label.text = "Label";
        label.fallback_text = "Label";
    }
    if (ImGui::MenuItem("Particle Emitter") && !context.registry.all_of<ParticleEmitterComponent>(context.selected_entity)) {
        context.registry.emplace<ParticleEmitterComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Free Camera Controller (3D)") && !context.registry.all_of<dse::FreeCameraControllerComponent>(context.selected_entity)) {
        context.registry.emplace<dse::FreeCameraControllerComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Directional Light (3D)") && !context.registry.all_of<dse::DirectionalLight3DComponent>(context.selected_entity)) {
        context.registry.emplace<dse::DirectionalLight3DComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Point Light (3D)") && !context.registry.all_of<dse::PointLightComponent>(context.selected_entity)) {
        context.registry.emplace<dse::PointLightComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Animator (3D)") && !context.registry.all_of<dse::Animator3DComponent>(context.selected_entity)) {
        context.registry.emplace<dse::Animator3DComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Camera 3D") && !context.registry.all_of<dse::Camera3DComponent>(context.selected_entity)) {
        context.registry.emplace<dse::Camera3DComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Skybox (3D)") && !context.registry.all_of<dse::SkyboxComponent>(context.selected_entity)) {
        context.registry.emplace<dse::SkyboxComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("RigidBody 3D") && !context.registry.all_of<dse::RigidBody3DComponent>(context.selected_entity)) {
        context.registry.emplace<dse::RigidBody3DComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Box Collider 3D") && !context.registry.all_of<dse::BoxCollider3DComponent>(context.selected_entity)) {
        context.registry.emplace<dse::BoxCollider3DComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Sphere Collider 3D") && !context.registry.all_of<dse::SphereCollider3DComponent>(context.selected_entity)) {
        context.registry.emplace<dse::SphereCollider3DComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Particle System 3D") && !context.registry.all_of<dse::ParticleSystem3DComponent>(context.selected_entity)) {
        context.registry.emplace<dse::ParticleSystem3DComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("Post Process (3D)") && !context.registry.all_of<dse::PostProcessComponent>(context.selected_entity)) {
        context.registry.emplace<dse::PostProcessComponent>(context.selected_entity);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("UI Anchor") && !context.registry.all_of<UIAnchorComponent>(context.selected_entity)) {
        context.registry.emplace<UIAnchorComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("UI Grid Layout") && !context.registry.all_of<UIGridLayoutComponent>(context.selected_entity)) {
        context.registry.emplace<UIGridLayoutComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("UI Canvas Scaler") && !context.registry.all_of<UICanvasScalerComponent>(context.selected_entity)) {
        context.registry.emplace<UICanvasScalerComponent>(context.selected_entity);
    }
    if (ImGui::MenuItem("UI Animation") && !context.registry.all_of<UIAnimationComponent>(context.selected_entity)) {
        context.registry.emplace<UIAnimationComponent>(context.selected_entity);
    }

    ImGui::EndPopup();
    if (read_only) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("Play 模式下已禁用 3D Inspector 编辑。请退出 Play 后修改 3D 组件。");
    }
}

} // namespace

void DrawInspectorPanel(EditorInspectorPanelContext& context,
                        void (*draw_ui_layout_inspector)(entt::registry&, entt::entity)) {
    ImGui::Begin("Inspector");
    if (context.selected_entity != entt::null && context.registry.valid(context.selected_entity)) {
        DrawInspectorHeader(context);
        DrawTransformSection(context);
        DrawSpriteRendererSection(context);
        DrawRigidBody2DSection(context);
        DrawUILabelSection(context);
        DrawParticleEmitterSection(context);
        DrawMeshRendererSection(context);
        DrawCamera3DSection(context);
        DrawDirectionalLightSection(context);
        DrawPointLightSection(context);
        DrawSkyboxSection(context);
        DrawAnimator3DSection(context);
        DrawFreeCameraControllerSection(context);
        DrawTerrainSection(context);
        DrawRigidBody3DSection(context);
        DrawBoxCollider3DSection(context);
        DrawSphereCollider3DSection(context);
        DrawParticleSystem3DSection(context);
        DrawPostProcessSection(context);
        if (draw_ui_layout_inspector) {
            draw_ui_layout_inspector(context.registry, context.selected_entity);
        }
        DrawAddComponentSection(context);
        ImGui::PopStyleColor();
    } else {
        ImGui::TextDisabled("No Entity Selected");
    }
    ImGui::End();
}

} // namespace dse::editor
