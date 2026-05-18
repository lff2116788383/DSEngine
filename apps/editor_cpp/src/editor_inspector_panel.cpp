#include "editor_inspector_panel.h"
#include "editor_inspector_registry.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/audio.h"
#include "modules/gameplay_3d/animation/animator_system.h"
#include "modules/gameplay_3d/animation/animation_state_machine.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <cstring>
#include <string>
#include <variant>

#include "editor_shared_components.h"
#include "editor_toolbar.h"
#include "editor_shortcuts.h"
#include "editor_console_panel.h"
#include "editor_selection.h"
#include "editor_particle_panel.h"

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

// Undo helper: captures old value on IsItemActivated, pushes PropertyChangeCommand on IsItemDeactivatedAfterEdit.
// Only one ImGui item can be active at a time, so a single static per-type is safe.
template<typename T>
void InspectorUndoCheck(const char* desc, T& current_value, entt::entity entity, std::function<void(const T&)> setter) {
    static T s_old_val{};
    static entt::entity s_ent = entt::null;
    if (ImGui::IsItemActivated()) {
        s_old_val = current_value;
        s_ent = entity;
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && s_ent == entity) {
        T old_v = s_old_val;
        T new_v = current_value;
        GetUndoRedoManager().Execute(
            std::make_unique<PropertyChangeCommand<T>>(desc, old_v, new_v, setter), true);
    }
}

// Helper to create a component property setter without comma-containing lambdas.
// This avoids MSVC traditional preprocessor issues with commas in lambda captures inside macros.
template<typename Comp, typename Field>
std::function<void(const Field&)> MakeCompSetter(entt::registry& reg, entt::entity ent, Field Comp::*member) {
    return [&reg, ent, member](const Field& v) {
        if (reg.valid(ent) && reg.all_of<Comp>(ent))
            reg.get<Comp>(ent).*member = v;
    };
}

// Same as INSPECTOR_PROPERTY but with undo tracking between widget and NextColumn.
// Uses variadic args for setter because lambdas with capture lists contain commas
// that confuse MSVC's traditional preprocessor.
#define INSPECTOR_PROPERTY_U(label, widget_code, desc, current_val, ent, ...) \
    ImGui::AlignTextToFramePadding(); \
    ImGui::Text(label); \
    ImGui::NextColumn(); \
    ImGui::SetNextItemWidth(-1); \
    widget_code; \
    InspectorUndoCheck(desc, current_val, ent, __VA_ARGS__); \
    ImGui::NextColumn();


// Draw a small colored square label (X=red, Y=green, Z=blue) before a DragFloat
void DrawColoredAxisLabel(const char* label, const ImVec4& color) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float size = ImGui::GetFrameHeight();
    draw_list->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size),
        ImGui::ColorConvertFloat4ToU32(color), 2.0f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + size + 4.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", label);
}

// Draw Vec3 property with colored X/Y/Z labels
bool DrawVec3WithColorLabels(const char* id, float v[3], float speed = 0.1f) {
    bool changed = false;
    float line_width = ImGui::GetContentRegionAvail().x;
    float field_width = (line_width - 3 * (ImGui::GetFrameHeight() + 4.0f + ImGui::CalcTextSize("X").x + 8.0f)) / 3.0f;
    if (field_width < 30.0f) field_width = 30.0f;

    ImGui::PushID(id);

    // X (red)
    DrawColoredAxisLabel("X", ImVec4(0.85f, 0.20f, 0.20f, 1.0f));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_width);
    if (ImGui::DragFloat("##x", &v[0], speed)) changed = true;
    ImGui::SameLine();

    // Y (green)
    DrawColoredAxisLabel("Y", ImVec4(0.30f, 0.75f, 0.20f, 1.0f));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_width);
    if (ImGui::DragFloat("##y", &v[1], speed)) changed = true;
    ImGui::SameLine();

    // Z (blue)
    DrawColoredAxisLabel("Z", ImVec4(0.20f, 0.40f, 0.90f, 1.0f));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_width);
    if (ImGui::DragFloat("##z", &v[2], speed)) changed = true;

    ImGui::PopID();
    return changed;
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
    if (!ImGui::CollapsingHeader(MDI_ICON_AXIS_ARROW "  Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    // Undo/Redo: track values at edit start
    static glm::vec3 s_edit_start_pos{0};
    static glm::vec3 s_edit_start_rot{0};
    static glm::vec3 s_edit_start_scale{1};
    static entt::entity s_edit_entity = entt::null;

    const entt::entity current_entity = context.selected_entity;

    // Position with colored X/Y/Z
    ImGui::Text("Position");
    float pos[3] = {transform.position.x, transform.position.y, transform.position.z};
    if (ImGui::IsItemActive() == false && ImGui::IsMouseClicked(0)) {
        // Will be captured below per-widget
    }
    ImGui::PushID("##pos_undo");
    if (DrawVec3WithColorLabels("##pos", pos)) {
        transform.position = glm::vec3(pos[0], pos[1], pos[2]);
        transform.dirty = true;
    }
    if (ImGui::IsItemActivated()) {
        s_edit_start_pos = transform.position;
        s_edit_entity = current_entity;
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && s_edit_entity == current_entity) {
        glm::vec3 old_val = s_edit_start_pos;
        glm::vec3 new_val = transform.position;
        entt::entity ent = current_entity;
        auto& reg = context.registry;
        auto cmd = std::make_unique<PropertyChangeCommand<glm::vec3>>(
            "Transform.Position",
            old_val, new_val,
            [&reg, ent](const glm::vec3& v) {
                if (reg.valid(ent) && reg.all_of<TransformComponent>(ent)) {
                    reg.get<TransformComponent>(ent).position = v;
                    reg.get<TransformComponent>(ent).dirty = true;
                }
            });
        GetUndoRedoManager().Execute(std::move(cmd), true);
    }
    ImGui::PopID();

    // Rotation
    glm::vec3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
    float rot[3] = {euler.x, euler.y, euler.z};
    if (context.is_2d) {
        ImGui::Text("Rotation");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::IsItemActivated()) {
            s_edit_start_rot = euler;
            s_edit_entity = current_entity;
        }
        if (ImGui::DragFloat("##rotZ", &rot[2], 0.1f)) {
            euler.z = rot[2];
            transform.rotation = glm::quat(glm::radians(euler));
            transform.dirty = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && s_edit_entity == current_entity) {
            glm::vec3 old_euler = s_edit_start_rot;
            glm::vec3 new_euler = euler;
            entt::entity ent = current_entity;
            auto& reg = context.registry;
            auto cmd = std::make_unique<PropertyChangeCommand<glm::vec3>>(
                "Transform.Rotation",
                old_euler, new_euler,
                [&reg, ent](const glm::vec3& v) {
                    if (reg.valid(ent) && reg.all_of<TransformComponent>(ent)) {
                        reg.get<TransformComponent>(ent).rotation = glm::quat(glm::radians(v));
                        reg.get<TransformComponent>(ent).dirty = true;
                    }
                });
            GetUndoRedoManager().Execute(std::move(cmd), true);
        }
    } else {
        ImGui::Text("Rotation");
        ImGui::PushID("##rot_undo");
        if (DrawVec3WithColorLabels("##rot", rot)) {
            euler = glm::vec3(rot[0], rot[1], rot[2]);
            transform.rotation = glm::quat(glm::radians(euler));
            transform.dirty = true;
        }
        if (ImGui::IsItemActivated()) {
            s_edit_start_rot = glm::degrees(glm::eulerAngles(transform.rotation));
            s_edit_entity = current_entity;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && s_edit_entity == current_entity) {
            glm::vec3 old_euler = s_edit_start_rot;
            glm::vec3 new_euler = glm::vec3(rot[0], rot[1], rot[2]);
            entt::entity ent = current_entity;
            auto& reg = context.registry;
            auto cmd = std::make_unique<PropertyChangeCommand<glm::vec3>>(
                "Transform.Rotation",
                old_euler, new_euler,
                [&reg, ent](const glm::vec3& v) {
                    if (reg.valid(ent) && reg.all_of<TransformComponent>(ent)) {
                        reg.get<TransformComponent>(ent).rotation = glm::quat(glm::radians(v));
                        reg.get<TransformComponent>(ent).dirty = true;
                    }
                });
            GetUndoRedoManager().Execute(std::move(cmd), true);
        }
        ImGui::PopID();
    }

    // Scale with colored X/Y/Z
    ImGui::Text("Scale");
    ImGui::PushID("##scale_undo");
    float scale[3] = {transform.scale.x, transform.scale.y, transform.scale.z};
    if (DrawVec3WithColorLabels("##scale", scale)) {
        transform.scale = glm::vec3(scale[0], scale[1], scale[2]);
        transform.dirty = true;
    }
    if (ImGui::IsItemActivated()) {
        s_edit_start_scale = transform.scale;
        s_edit_entity = current_entity;
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && s_edit_entity == current_entity) {
        glm::vec3 old_val = s_edit_start_scale;
        glm::vec3 new_val = transform.scale;
        entt::entity ent = current_entity;
        auto& reg = context.registry;
        auto cmd = std::make_unique<PropertyChangeCommand<glm::vec3>>(
            "Transform.Scale",
            old_val, new_val,
            [&reg, ent](const glm::vec3& v) {
                if (reg.valid(ent) && reg.all_of<TransformComponent>(ent)) {
                    reg.get<TransformComponent>(ent).scale = v;
                    reg.get<TransformComponent>(ent).dirty = true;
                }
            });
        GetUndoRedoManager().Execute(std::move(cmd), true);
    }
    ImGui::PopID();
}

void DrawSpriteRendererSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<SpriteRendererComponent>(context.selected_entity)) {
        return;
    }

    auto& sprite = context.registry.get<SpriteRendererComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_PALETTE "  Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    INSPECTOR_PROPERTY_U("Color",
        if (ImGui::ColorEdit4("##color", color)) {
            sprite.color = glm::vec4(color[0], color[1], color[2], color[3]);
            MarkSpriteRendererDirty(sprite);
        },
        "Sprite.Color", sprite.color, context.selected_entity,
        MakeCompSetter<SpriteRendererComponent>(context.registry, context.selected_entity, &SpriteRendererComponent::color));

    ImGui::Columns(1);
}

void DrawRigidBody2DSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<RigidBody2DComponent>(context.selected_entity)) {
        return;
    }

    auto& rb2d = context.registry.get<RigidBody2DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_RUN "  RigidBody 2D", ImGuiTreeNodeFlags_DefaultOpen)) {
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

    INSPECTOR_PROPERTY_U("Gravity",
        if (ImGui::DragFloat("##grav", &rb2d.gravity_scale, 0.1f)) { MarkRigidBody2DDirty(rb2d); },
        "RigidBody2D.Gravity", rb2d.gravity_scale, context.selected_entity,
        MakeCompSetter<RigidBody2DComponent>(context.registry, context.selected_entity, &RigidBody2DComponent::gravity_scale));
    ImGui::Columns(1);
}

void DrawMeshRendererSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::MeshRendererComponent>(context.selected_entity)) {
        return;
    }

    auto& mesh = context.registry.get<dse::MeshRendererComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_SPHERE "  Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            std::string p(path);
            if (p.ends_with(".obj") || p.ends_with(".fbx") || p.ends_with(".gltf") || p.ends_with(".glb") || p.ends_with(".dae")) {
                mesh.mesh_path = p;
            }
        }
        ImGui::EndDragDropTarget();
    }

    INSPECTOR_PROPERTY("Material Instance", ImGui::DragScalar("##mat_inst", ImGuiDataType_U32, &mesh.material_instance_id, 1.0f, nullptr, nullptr, "%u"));
    ImGui::Separator();
    ImGui::Text("Material Overrides (PBR)");
    INSPECTOR_PROPERTY_U("Albedo Tint", ImGui::ColorEdit4("##mesh_color", glm::value_ptr(mesh.color)),
        "Mesh.AlbedoTint", mesh.color, context.selected_entity,
        MakeCompSetter<dse::MeshRendererComponent>(context.registry, context.selected_entity, &dse::MeshRendererComponent::color));
    INSPECTOR_PROPERTY_U("Emissive", ImGui::ColorEdit3("##mesh_emissive", glm::value_ptr(mesh.emissive)),
        "Mesh.Emissive", mesh.emissive, context.selected_entity,
        MakeCompSetter<dse::MeshRendererComponent>(context.registry, context.selected_entity, &dse::MeshRendererComponent::emissive));
    INSPECTOR_PROPERTY_U("Metallic", ImGui::SliderFloat("##mesh_metallic", &mesh.metallic, 0.0f, 1.0f),
        "Mesh.Metallic", mesh.metallic, context.selected_entity,
        MakeCompSetter<dse::MeshRendererComponent>(context.registry, context.selected_entity, &dse::MeshRendererComponent::metallic));
    INSPECTOR_PROPERTY_U("Roughness", ImGui::SliderFloat("##mesh_roughness", &mesh.roughness, 0.0f, 1.0f),
        "Mesh.Roughness", mesh.roughness, context.selected_entity,
        MakeCompSetter<dse::MeshRendererComponent>(context.registry, context.selected_entity, &dse::MeshRendererComponent::roughness));
    INSPECTOR_PROPERTY_U("Ambient Occlusion", ImGui::SliderFloat("##mesh_ao", &mesh.ao, 0.0f, 1.0f),
        "Mesh.AO", mesh.ao, context.selected_entity,
        MakeCompSetter<dse::MeshRendererComponent>(context.registry, context.selected_entity, &dse::MeshRendererComponent::ao));
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
    if (!ImGui::CollapsingHeader(MDI_ICON_VIDEO "  Camera 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "cam3d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##cam3d_enabled", &camera.enabled));
    INSPECTOR_PROPERTY_U("Priority", ImGui::DragInt("##cam3d_priority", &camera.priority, 1),
        "Camera3D.Priority", camera.priority, context.selected_entity,
        MakeCompSetter<dse::Camera3DComponent>(context.registry, context.selected_entity, &dse::Camera3DComponent::priority));
    INSPECTOR_PROPERTY_U("FOV", ImGui::DragFloat("##cam3d_fov", &camera.fov, 1.0f, 10.0f, 170.0f),
        "Camera3D.FOV", camera.fov, context.selected_entity,
        MakeCompSetter<dse::Camera3DComponent>(context.registry, context.selected_entity, &dse::Camera3DComponent::fov));
    INSPECTOR_PROPERTY_U("Near Clip", ImGui::DragFloat("##cam3d_near", &camera.near_clip, 0.05f, 0.01f, 10.0f),
        "Camera3D.NearClip", camera.near_clip, context.selected_entity,
        MakeCompSetter<dse::Camera3DComponent>(context.registry, context.selected_entity, &dse::Camera3DComponent::near_clip));
    INSPECTOR_PROPERTY_U("Far Clip", ImGui::DragFloat("##cam3d_far", &camera.far_clip, 10.0f, 10.0f, 10000.0f),
        "Camera3D.FarClip", camera.far_clip, context.selected_entity,
        MakeCompSetter<dse::Camera3DComponent>(context.registry, context.selected_entity, &dse::Camera3DComponent::far_clip));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawDirectionalLightSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::DirectionalLight3DComponent>(context.selected_entity)) {
        return;
    }

    auto& light = context.registry.get<dse::DirectionalLight3DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_WEATHER_SUNNY "  Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "dirlight_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##dirlight_enabled", &light.enabled));
    INSPECTOR_PROPERTY_U("Direction", ImGui::DragFloat3("##dirlight_dir", glm::value_ptr(light.direction), 0.05f, -1.0f, 1.0f),
        "DirLight.Direction", light.direction, context.selected_entity,
        MakeCompSetter<dse::DirectionalLight3DComponent>(context.registry, context.selected_entity, &dse::DirectionalLight3DComponent::direction));
    INSPECTOR_PROPERTY_U("Color", ImGui::ColorEdit3("##dirlight_color", glm::value_ptr(light.color)),
        "DirLight.Color", light.color, context.selected_entity,
        MakeCompSetter<dse::DirectionalLight3DComponent>(context.registry, context.selected_entity, &dse::DirectionalLight3DComponent::color));
    INSPECTOR_PROPERTY_U("Intensity", ImGui::DragFloat("##dirlight_int", &light.intensity, 0.05f, 0.0f, 10.0f),
        "DirLight.Intensity", light.intensity, context.selected_entity,
        MakeCompSetter<dse::DirectionalLight3DComponent>(context.registry, context.selected_entity, &dse::DirectionalLight3DComponent::intensity));
    INSPECTOR_PROPERTY_U("Ambient", ImGui::DragFloat("##dirlight_amb", &light.ambient_intensity, 0.05f, 0.0f, 1.0f),
        "DirLight.Ambient", light.ambient_intensity, context.selected_entity,
        MakeCompSetter<dse::DirectionalLight3DComponent>(context.registry, context.selected_entity, &dse::DirectionalLight3DComponent::ambient_intensity));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawPointLightSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::PointLightComponent>(context.selected_entity)) {
        return;
    }

    auto& light = context.registry.get<dse::PointLightComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_LIGHTBULB "  Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "ptlight_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##ptlight_enabled", &light.enabled));
    INSPECTOR_PROPERTY_U("Color", ImGui::ColorEdit3("##ptlight_color", glm::value_ptr(light.color)),
        "PointLight.Color", light.color, context.selected_entity,
        MakeCompSetter<dse::PointLightComponent>(context.registry, context.selected_entity, &dse::PointLightComponent::color));
    INSPECTOR_PROPERTY_U("Intensity", ImGui::DragFloat("##ptlight_int", &light.intensity, 0.05f, 0.0f, 10.0f),
        "PointLight.Intensity", light.intensity, context.selected_entity,
        MakeCompSetter<dse::PointLightComponent>(context.registry, context.selected_entity, &dse::PointLightComponent::intensity));
    INSPECTOR_PROPERTY_U("Radius", ImGui::DragFloat("##ptlight_rad", &light.radius, 0.5f, 0.1f, 1000.0f),
        "PointLight.Radius", light.radius, context.selected_entity,
        MakeCompSetter<dse::PointLightComponent>(context.registry, context.selected_entity, &dse::PointLightComponent::radius));
    INSPECTOR_PROPERTY_U("Falloff", ImGui::DragFloat("##ptlight_falloff", &light.falloff, 0.05f, 0.0f, 16.0f),
        "PointLight.Falloff", light.falloff, context.selected_entity,
        MakeCompSetter<dse::PointLightComponent>(context.registry, context.selected_entity, &dse::PointLightComponent::falloff));
    INSPECTOR_PROPERTY("Cast Shadow", ImGui::Checkbox("##ptlight_shadow", &light.cast_shadow));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawSpotLightSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::SpotLightComponent>(context.selected_entity)) {
        return;
    }

    auto& light = context.registry.get<dse::SpotLightComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_LIGHTBULB "  Spot Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "spotlight_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##spotlight_enabled", &light.enabled));
    INSPECTOR_PROPERTY_U("Direction", ImGui::DragFloat3("##spotlight_dir", glm::value_ptr(light.direction), 0.05f, -1.0f, 1.0f),
        "SpotLight.Direction", light.direction, context.selected_entity,
        MakeCompSetter<dse::SpotLightComponent>(context.registry, context.selected_entity, &dse::SpotLightComponent::direction));
    INSPECTOR_PROPERTY_U("Color", ImGui::ColorEdit3("##spotlight_color", glm::value_ptr(light.color)),
        "SpotLight.Color", light.color, context.selected_entity,
        MakeCompSetter<dse::SpotLightComponent>(context.registry, context.selected_entity, &dse::SpotLightComponent::color));
    INSPECTOR_PROPERTY_U("Intensity", ImGui::DragFloat("##spotlight_int", &light.intensity, 0.05f, 0.0f, 10.0f),
        "SpotLight.Intensity", light.intensity, context.selected_entity,
        MakeCompSetter<dse::SpotLightComponent>(context.registry, context.selected_entity, &dse::SpotLightComponent::intensity));
    INSPECTOR_PROPERTY_U("Radius", ImGui::DragFloat("##spotlight_rad", &light.radius, 0.5f, 0.1f, 1000.0f),
        "SpotLight.Radius", light.radius, context.selected_entity,
        MakeCompSetter<dse::SpotLightComponent>(context.registry, context.selected_entity, &dse::SpotLightComponent::radius));
    INSPECTOR_PROPERTY_U("Falloff", ImGui::DragFloat("##spotlight_falloff", &light.falloff, 0.05f, 0.0f, 16.0f),
        "SpotLight.Falloff", light.falloff, context.selected_entity,
        MakeCompSetter<dse::SpotLightComponent>(context.registry, context.selected_entity, &dse::SpotLightComponent::falloff));
    INSPECTOR_PROPERTY_U("Inner Cone", ImGui::DragFloat("##spotlight_inner", &light.inner_cone_angle, 0.25f, 0.0f, 89.0f),
        "SpotLight.InnerCone", light.inner_cone_angle, context.selected_entity,
        MakeCompSetter<dse::SpotLightComponent>(context.registry, context.selected_entity, &dse::SpotLightComponent::inner_cone_angle));
    INSPECTOR_PROPERTY_U("Outer Cone", ImGui::DragFloat("##spotlight_outer", &light.outer_cone_angle, 0.25f, 0.0f, 89.0f),
        "SpotLight.OuterCone", light.outer_cone_angle, context.selected_entity,
        MakeCompSetter<dse::SpotLightComponent>(context.registry, context.selected_entity, &dse::SpotLightComponent::outer_cone_angle));
    INSPECTOR_PROPERTY("Cast Shadow", ImGui::Checkbox("##spotlight_shadow", &light.cast_shadow));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawSkyLightSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::SkyLightComponent>(context.selected_entity)) {
        return;
    }

    auto& light = context.registry.get<dse::SkyLightComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_WEATHER_SUNNY "  Sky Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "skylight_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##skylight_enabled", &light.enabled));
    INSPECTOR_PROPERTY_U("Up Color", ImGui::ColorEdit3("##skylight_up", glm::value_ptr(light.up_color)),
        "SkyLight.UpColor", light.up_color, context.selected_entity,
        MakeCompSetter<dse::SkyLightComponent>(context.registry, context.selected_entity, &dse::SkyLightComponent::up_color));
    INSPECTOR_PROPERTY_U("Down Color", ImGui::ColorEdit3("##skylight_down", glm::value_ptr(light.down_color)),
        "SkyLight.DownColor", light.down_color, context.selected_entity,
        MakeCompSetter<dse::SkyLightComponent>(context.registry, context.selected_entity, &dse::SkyLightComponent::down_color));
    INSPECTOR_PROPERTY_U("Intensity", ImGui::DragFloat("##skylight_int", &light.intensity, 0.05f, 0.0f, 10.0f),
        "SkyLight.Intensity", light.intensity, context.selected_entity,
        MakeCompSetter<dse::SkyLightComponent>(context.registry, context.selected_entity, &dse::SkyLightComponent::intensity));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawSkyboxSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::SkyboxComponent>(context.selected_entity)) {
        return;
    }

    auto& skybox = context.registry.get<dse::SkyboxComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_VIEW_IN_AR "  Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            skybox.cubemap_path = static_cast<const char*>(payload->Data);
        }
        ImGui::EndDragDropTarget();
    }

    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawAnimator3DSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::Animator3DComponent>(context.selected_entity)) {
        return;
    }

    auto& animator = context.registry.get<dse::Animator3DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_ANIMATION "  Animator 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string p(static_cast<const char*>(payload->Data));
            if (p.ends_with(".dskel")) animator.dskel_path = p;
        }
        ImGui::EndDragDropTarget();
    }

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
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string p(static_cast<const char*>(payload->Data));
                if (p.ends_with(".danim")) animator.danim_path = p;
            }
            ImGui::EndDragDropTarget();
        }
        INSPECTOR_PROPERTY("Speed", ImGui::DragFloat("##anim_speed", &animator.speed, 0.1f, 0.0f, 10.0f));
        INSPECTOR_PROPERTY("Loop", ImGui::Checkbox("##anim_loop", &animator.loop));
        INSPECTOR_PROPERTY("Use Blend Tree", ImGui::Checkbox("##anim_tree", &animator.use_anim_tree));
        INSPECTOR_PROPERTY("Blend Value", ImGui::DragFloat("##anim_blend_value", &animator.blend_parameter_value, 0.05f, -100.0f, 100.0f));

        char blend_param_buf[128] = {};
        std::strncpy(blend_param_buf, animator.blend_parameter.c_str(), sizeof(blend_param_buf) - 1);
        INSPECTOR_PROPERTY("Blend Param", if (ImGui::InputText("##anim_blend_param", blend_param_buf, sizeof(blend_param_buf))) {
            animator.blend_parameter = blend_param_buf;
        });

        if (animator.use_anim_tree) {
            ImGui::Separator();
            ImGui::Text("Blend Nodes");
            for (size_t i = 0; i < animator.blend_nodes.size(); ++i) {
                auto& node = animator.blend_nodes[i];
                ImGui::PushID(static_cast<int>(i));

                char node_name_buf[128] = {};
                std::strncpy(node_name_buf, node.name.c_str(), sizeof(node_name_buf) - 1);
                INSPECTOR_PROPERTY("Node Name", if (ImGui::InputText("##blend_name", node_name_buf, sizeof(node_name_buf))) {
                    node.name = node_name_buf;
                });

                char node_path_buf[256] = {};
                std::strncpy(node_path_buf, node.danim_path.c_str(), sizeof(node_path_buf) - 1);
                INSPECTOR_PROPERTY("Node Anim", if (ImGui::InputText("##blend_path", node_path_buf, sizeof(node_path_buf))) {
                    node.danim_path = node_path_buf;
                });

                INSPECTOR_PROPERTY("Node Speed", ImGui::DragFloat("##blend_speed", &node.speed, 0.05f, 0.0f, 10.0f));
                INSPECTOR_PROPERTY("Node Loop", ImGui::Checkbox("##blend_loop", &node.loop));
                INSPECTOR_PROPERTY("Weight", ImGui::DragFloat("##blend_weight", &node.weight, 0.01f, 0.0f, 1.0f));
                INSPECTOR_PROPERTY("Threshold", ImGui::DragFloat("##blend_threshold", &node.threshold, 0.05f, -100.0f, 100.0f));

                if (ImGui::Button("Remove Node")) {
                    animator.blend_nodes.erase(animator.blend_nodes.begin() + static_cast<std::ptrdiff_t>(i));
                    ImGui::PopID();
                    break;
                }
                ImGui::Separator();
                ImGui::PopID();
            }

            if (ImGui::Button("Add Blend Node")) {
                animator.blend_nodes.push_back(dse::AnimBlendNode{});
            }
        }
    }

    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawFreeCameraControllerSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::FreeCameraControllerComponent>(context.selected_entity)) {
        return;
    }

    auto& controller = context.registry.get<dse::FreeCameraControllerComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_VIDEO "  Free Camera Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (!ImGui::CollapsingHeader(MDI_ICON_TERRAIN "  Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
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

    if (terrain.use_dynamic_lod) {
        INSPECTOR_PROPERTY("LOD Levels", ImGui::DragInt("##terrain_lod_levels", &terrain.max_lod_levels, 0.1f, 1, 8));
        INSPECTOR_PROPERTY("LOD Dist Factor", ImGui::DragFloat("##terrain_lod_dist", &terrain.lod_distance_factor, 1.0f, 5.0f, 500.0f));

        ImGui::Separator();
        ImGui::Text("LOD Preview"); ImGui::NextColumn(); ImGui::NextColumn();
        INSPECTOR_PROPERTY("Current LOD", ImGui::Text("%d / %d", terrain.current_lod, terrain.max_lod_levels - 1));

        // Show triangle count for current LOD
        unsigned int tri_count = 0;
        if (terrain.current_lod < static_cast<int>(terrain.lod_index_counts.size())) {
            tri_count = terrain.lod_index_counts[terrain.current_lod] / 3;
        } else if (terrain.index_count > 0) {
            tri_count = terrain.index_count / 3;
        }
        INSPECTOR_PROPERTY("Triangles", ImGui::Text("%u", tri_count));
        INSPECTOR_PROPERTY("Resolution", ImGui::Text("%d x %d", terrain.resolution_x, terrain.resolution_z));

        // LOD level bar visualization
        ImGui::NextColumn(); ImGui::NextColumn();
        float lod_frac = terrain.max_lod_levels > 1
            ? static_cast<float>(terrain.current_lod) / static_cast<float>(terrain.max_lod_levels - 1)
            : 0.0f;
        ImGui::ProgressBar(lod_frac, ImVec2(-1, 0),
            (std::string("LOD ") + std::to_string(terrain.current_lod)).c_str());
        ImGui::NextColumn();
    }

    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawUILabelSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<UILabelComponent>(context.selected_entity)) {
        return;
    }

    auto& label = context.registry.get<UILabelComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_FORMAT_TEXT "  UI Label", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (!ImGui::CollapsingHeader(MDI_ICON_CREATION "  Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen)) {
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
            const char* curve_types[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", "Custom" };
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
    if (!ImGui::CollapsingHeader(MDI_ICON_RUN "  RigidBody 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (!ImGui::CollapsingHeader(MDI_ICON_CUBE_OUTLINE "  Box Collider 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (!ImGui::CollapsingHeader(MDI_ICON_SPHERE "  Sphere Collider 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (!ImGui::CollapsingHeader(MDI_ICON_CREATION "  Particle System 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (!ImGui::CollapsingHeader(MDI_ICON_POST_PROCESS "  Post Processing", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    ImGui::Separator();
    ImGui::Text("SSAO");
    ImGui::NextColumn(); ImGui::NextColumn();
    INSPECTOR_PROPERTY("SSAO Enabled", ImGui::Checkbox("##pp_ssao_en", &pp.ssao_enabled));
    INSPECTOR_PROPERTY("Radius", ImGui::DragFloat("##pp_ssao_rad", &pp.ssao_radius, 0.01f, 0.01f, 5.0f));
    INSPECTOR_PROPERTY("Bias", ImGui::DragFloat("##pp_ssao_bias", &pp.ssao_bias, 0.001f, 0.0f, 0.5f));
    ImGui::Separator();
    ImGui::Text("FXAA");
    ImGui::NextColumn(); ImGui::NextColumn();
    INSPECTOR_PROPERTY("FXAA Enabled", ImGui::Checkbox("##pp_fxaa_en", &pp.fxaa_enabled));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawLightProbeSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::LightProbeComponent>(context.selected_entity)) {
        return;
    }

    auto& probe = context.registry.get<dse::LightProbeComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_LIGHTBULB "  Light Probe", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "lprobe_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##lprobe_en", &probe.enabled));
    INSPECTOR_PROPERTY("Radius", ImGui::DragFloat("##lprobe_rad", &probe.influence_radius, 0.5f, 0.5f, 200.0f));
    INSPECTOR_PROPERTY("Show Debug", ImGui::Checkbox("##lprobe_debug", &probe.show_debug));
    INSPECTOR_PROPERTY("Needs Rebake", ImGui::Checkbox("##lprobe_rebake", &probe.needs_rebake));

    // SH coefficient preview (read-only display of first 3 bands)
    ImGui::Separator();
    ImGui::Text("SH Coefficients"); ImGui::NextColumn(); ImGui::NextColumn();
    for (int i = 0; i < 9; ++i) {
        char label[32];
        std::snprintf(label, sizeof(label), "SH[%d]", i);
        INSPECTOR_PROPERTY(label, ImGui::Text("(%.2f, %.2f, %.2f)",
            probe.sh_coefficients[i].x, probe.sh_coefficients[i].y, probe.sh_coefficients[i].z));
    }
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawReflectionProbeSection(EditorInspectorPanelContext& context) {
    if (!context.registry.all_of<dse::ReflectionProbeComponent>(context.selected_entity)) {
        return;
    }

    auto& probe = context.registry.get<dse::ReflectionProbeComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_CUBE_OUTLINE "  Reflection Probe", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "rprobe_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##rprobe_en", &probe.enabled));
    INSPECTOR_PROPERTY("Radius", ImGui::DragFloat("##rprobe_rad", &probe.influence_radius, 0.5f, 0.5f, 200.0f));
    INSPECTOR_PROPERTY("Box Projection", ImGui::Checkbox("##rprobe_box", &probe.use_box_projection));
    if (probe.use_box_projection) {
        INSPECTOR_PROPERTY("Box Size X", ImGui::DragFloat("##rprobe_bx", &probe.box_size_x, 0.5f, 0.5f, 200.0f));
        INSPECTOR_PROPERTY("Box Size Y", ImGui::DragFloat("##rprobe_by", &probe.box_size_y, 0.5f, 0.5f, 200.0f));
        INSPECTOR_PROPERTY("Box Size Z", ImGui::DragFloat("##rprobe_bz", &probe.box_size_z, 0.5f, 0.5f, 200.0f));
    }
    INSPECTOR_PROPERTY("Resolution", ImGui::DragInt("##rprobe_res", &probe.resolution, 1.0f, 32, 2048));
    INSPECTOR_PROPERTY("Show Debug", ImGui::Checkbox("##rprobe_debug", &probe.show_debug));
    INSPECTOR_PROPERTY("Needs Rebake", ImGui::Checkbox("##rprobe_rebake", &probe.needs_rebake));
    INSPECTOR_PROPERTY("Cubemap", ImGui::Text("Handle: %u", probe.cubemap_handle));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

// ─── 注册所有 Inspector Section 到注册表 ────────────────────────────────────

void RegisterAllInspectorSections() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    auto& reg = InspectorRegistry::Get();

    // --- Core ---
    reg.Register({"Transform", "Core", DrawTransformSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<TransformComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<TransformComponent>(e)) r.emplace<TransformComponent>(e); },
        10});

    // --- 2D ---
    reg.Register({"Sprite Renderer", "2D", DrawSpriteRendererSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<SpriteRendererComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<SpriteRendererComponent>(e)) r.emplace<SpriteRendererComponent>(e); },
        20});
    reg.Register({"RigidBody 2D", "2D", DrawRigidBody2DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<RigidBody2DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<RigidBody2DComponent>(e)) r.emplace<RigidBody2DComponent>(e); },
        21});
    reg.Register({"Particle Emitter", "2D", DrawParticleEmitterSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<ParticleEmitterComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<ParticleEmitterComponent>(e)) r.emplace<ParticleEmitterComponent>(e); },
        22});

    // --- UI ---
    reg.Register({"UI Label", "UI", DrawUILabelSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<UILabelComponent>(e); },
        [](entt::registry& r, entt::entity e) {
            if (!r.all_of<UILabelComponent>(e)) {
                auto& label = r.emplace<UILabelComponent>(e);
                label.text = "Label";
                label.fallback_text = "Label";
            }
        },
        30});

    // --- 3D Rendering ---
    reg.Register({"Mesh Renderer", "3D", DrawMeshRendererSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::MeshRendererComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::MeshRendererComponent>(e)) r.emplace<dse::MeshRendererComponent>(e); },
        40});
    reg.Register({"Camera 3D", "3D", DrawCamera3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::Camera3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::Camera3DComponent>(e)) r.emplace<dse::Camera3DComponent>(e); },
        41});
    reg.Register({"Directional Light", "3D", DrawDirectionalLightSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::DirectionalLight3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::DirectionalLight3DComponent>(e)) r.emplace<dse::DirectionalLight3DComponent>(e); },
        42});
    reg.Register({"Point Light", "3D", DrawPointLightSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::PointLightComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::PointLightComponent>(e)) r.emplace<dse::PointLightComponent>(e); },
        43});
    reg.Register({"Spot Light", "3D", DrawSpotLightSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::SpotLightComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::SpotLightComponent>(e)) r.emplace<dse::SpotLightComponent>(e); },
        44});
    reg.Register({"Sky Light", "3D", DrawSkyLightSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::SkyLightComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::SkyLightComponent>(e)) r.emplace<dse::SkyLightComponent>(e); },
        45});
    reg.Register({"Skybox", "3D", DrawSkyboxSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::SkyboxComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::SkyboxComponent>(e)) r.emplace<dse::SkyboxComponent>(e); },
        46});
    reg.Register({"Animator 3D", "3D", DrawAnimator3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::Animator3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::Animator3DComponent>(e)) r.emplace<dse::Animator3DComponent>(e); },
        47});
    reg.Register({"Free Camera Controller", "3D", DrawFreeCameraControllerSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::FreeCameraControllerComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::FreeCameraControllerComponent>(e)) r.emplace<dse::FreeCameraControllerComponent>(e); },
        48});
    reg.Register({"Terrain", "3D", DrawTerrainSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<TerrainComponent>(e); },
        nullptr,  // Terrain 不通过 Add Component 添加
        49});
    reg.Register({"Post Process", "3D", DrawPostProcessSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::PostProcessComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::PostProcessComponent>(e)) r.emplace<dse::PostProcessComponent>(e); },
        50});

    // --- Physics 3D ---
    reg.Register({"RigidBody 3D", "Physics", DrawRigidBody3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::RigidBody3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::RigidBody3DComponent>(e)) r.emplace<dse::RigidBody3DComponent>(e); },
        60});
    reg.Register({"Box Collider 3D", "Physics", DrawBoxCollider3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::BoxCollider3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::BoxCollider3DComponent>(e)) r.emplace<dse::BoxCollider3DComponent>(e); },
        61});
    reg.Register({"Sphere Collider 3D", "Physics", DrawSphereCollider3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::SphereCollider3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::SphereCollider3DComponent>(e)) r.emplace<dse::SphereCollider3DComponent>(e); },
        62});
    reg.Register({"Particle System 3D", "3D", DrawParticleSystem3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::ParticleSystem3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::ParticleSystem3DComponent>(e)) r.emplace<dse::ParticleSystem3DComponent>(e); },
        63});

    // --- Probes ---
    reg.Register({"Light Probe", "3D", DrawLightProbeSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::LightProbeComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::LightProbeComponent>(e)) r.emplace<dse::LightProbeComponent>(e); },
        70});
    reg.Register({"Reflection Probe", "3D", DrawReflectionProbeSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::ReflectionProbeComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::ReflectionProbeComponent>(e)) r.emplace<dse::ReflectionProbeComponent>(e); },
        71});

    // --- Add-only (无 Inspector Section 但可通过 Add Component 添加) ---
    reg.Register({"Name", "Core", nullptr,
        [](entt::registry& r, entt::entity e) { return r.all_of<EditorNameComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<EditorNameComponent>(e)) r.emplace<EditorNameComponent>(e, "New Component"); },
        11});
    reg.Register({"Audio Source", "Audio", nullptr,
        [](entt::registry& r, entt::entity e) { return r.all_of<AudioSourceComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<AudioSourceComponent>(e)) r.emplace<AudioSourceComponent>(e); },
        80});
    reg.Register({"Audio Listener", "Audio", nullptr,
        [](entt::registry& r, entt::entity e) { return r.all_of<AudioListenerComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<AudioListenerComponent>(e)) r.emplace<AudioListenerComponent>(e); },
        81});
    reg.Register({"UI Anchor", "UI", nullptr,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIAnchorComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIAnchorComponent>(e)) r.emplace<UIAnchorComponent>(e); },
        31});
    reg.Register({"UI Grid Layout", "UI", nullptr,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIGridLayoutComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIGridLayoutComponent>(e)) r.emplace<UIGridLayoutComponent>(e); },
        32});
    reg.Register({"UI Canvas Scaler", "UI", nullptr,
        [](entt::registry& r, entt::entity e) { return r.all_of<UICanvasScalerComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UICanvasScalerComponent>(e)) r.emplace<UICanvasScalerComponent>(e); },
        33});
    reg.Register({"UI Animation", "UI", nullptr,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIAnimationComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIAnimationComponent>(e)) r.emplace<UIAnimationComponent>(e); },
        34});
}

} // namespace

// ─── InspectorRegistry 方法实现 ─────────────────────────────────────────────

void InspectorRegistry::DrawAll(EditorInspectorPanelContext& context) {
    for (const auto& entry : GetEntries()) {
        if (entry.draw) {
            entry.draw(context);
        }
    }
}

void InspectorRegistry::DrawAddComponentMenu(EditorInspectorPanelContext& context) {
    const bool read_only = IsEditorInPlayMode() && !context.is_2d;
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 60);
    if (read_only) {
        ImGui::BeginDisabled(true);
    }
    if (ImGui::Button("Add Component", ImVec2(120, 30))) {
        ImGui::OpenPopup("AddComponentPopup");
    }
    if (read_only) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("Play 模式下已禁用 3D Inspector 编辑。请退出 Play 后修改 3D 组件。");
    }

    if (!ImGui::BeginPopup("AddComponentPopup")) return;

    std::string last_category;
    for (const auto& entry : GetEntries()) {
        if (!entry.add) continue;  // 不可手动添加
        if (entry.has && entry.has(context.registry, context.selected_entity)) continue;  // 已存在

        if (!last_category.empty() && entry.category != last_category) {
            ImGui::Separator();
        }
        last_category = entry.category;

        if (ImGui::MenuItem(entry.component_name.c_str())) {
            entry.add(context.registry, context.selected_entity);
        }
    }
    ImGui::EndPopup();
}

// ─── DrawInspectorPanel（注册表驱动） ────────────────────────────────────────

void DrawInspectorPanel(EditorInspectorPanelContext& context,
                        void (*draw_ui_layout_inspector)(entt::registry&, entt::entity)) {
    RegisterAllInspectorSections();

    ImGui::Begin("Inspector");

    auto& selection = SelectionManager::Get();
    if (selection.IsMultiSelect()) {
        ImGui::Text("%d entities selected", selection.Count());
        ImGui::Separator();

        glm::vec3 avg_pos(0.0f);
        glm::vec3 avg_scale(0.0f);
        int transform_count = 0;
        for (auto ent : selection.GetAll()) {
            if (context.registry.valid(ent) && context.registry.all_of<TransformComponent>(ent)) {
                auto& t = context.registry.get<TransformComponent>(ent);
                avg_pos += t.position;
                avg_scale += t.scale;
                transform_count++;
            }
        }
        if (transform_count > 0) {
            avg_pos /= static_cast<float>(transform_count);
            avg_scale /= static_cast<float>(transform_count);
            ImGui::Text("Average Position: (%.2f, %.2f, %.2f)", avg_pos.x, avg_pos.y, avg_pos.z);
            ImGui::Text("Average Scale: (%.2f, %.2f, %.2f)", avg_scale.x, avg_scale.y, avg_scale.z);
        }
        ImGui::Separator();
        ImGui::TextDisabled("Multi-selection: individual editing disabled");
    } else if (context.selected_entity != entt::null && context.registry.valid(context.selected_entity)) {
        DrawInspectorHeader(context);

        // 注册表驱动：遍历所有已注册的组件 Section
        InspectorRegistry::Get().DrawAll(context);

        // 粒子曲线编辑器（特殊签名，跟随 ParticleEmitter Section 后绘制）
        DrawParticleCurveEditor(context.registry, context.selected_entity);

        // 外部回调（UI Layout Inspector 等）
        if (draw_ui_layout_inspector) {
            draw_ui_layout_inspector(context.registry, context.selected_entity);
        }

        // Add Component 菜单（注册表驱动）
        InspectorRegistry::Get().DrawAddComponentMenu(context);
        ImGui::PopStyleColor();
    } else {
        ImGui::TextDisabled("No Entity Selected");
    }
    ImGui::End();
}

} // namespace dse::editor
