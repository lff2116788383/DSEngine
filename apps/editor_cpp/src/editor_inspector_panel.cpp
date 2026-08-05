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
#include <functional>
#include <memory>
#include <string>
#include <variant>

#include "engine/ecs/components_3d_render.h"   // GrassComponent, DecalComponent, WaterComponent, GIProbeVolumeComponent, LODGroupComponent, HairComponent, MorphTargetComponent
#include "engine/ecs/components_3d_tree.h"     // TreeComponent
#include "engine/ecs/components_3d_foliage.h"  // FoliageComponent
#include "engine/ecs/components_3d_navmesh.h"  // DynamicObstacleComponent, NavMeshAutoRebakeComponent
#include "engine/ecs/components_3d_terrain_tile.h"  // TerrainTileManagerComponent
#include "engine/ecs/components_3d_sky.h"      // AtmosphereComponent, VolumetricCloudComponent, DayNightCycleComponent
#include "engine/ecs/components_3d_impostor.h" // ImpostorComponent
#include "engine/render/particles/gpu_particle_system.h" // GpuParticleComponent
#include "engine/render/gi/lightmap_baker.h" // LightmapComponent
#include "engine/reflect/reflect.h"
#include "engine/reflect/component_reflection.h"
#include "editor_reflected_inspector.h"

#include "editor_shared_components.h"
#include "editor_toolbar.h"
#include "editor_shortcuts.h"
#include "editor_console_panel.h"
#include "editor_selection.h"
#include "editor_particle_panel.h"
#include "editor_audio_panel.h"
#include "editor_prefab_override.h"
#include "editor_entity_snapshot.h"   // EntitySnapshot（单实体组件抓取/补回）
#include "editor_csharp_panel.h"

#include "editor_panel_registry.h"

namespace dse::editor {

namespace {

#define INSPECTOR_PROPERTY(label, code) \
    ImGui::AlignTextToFramePadding(); \
    ImGui::Text(label); \
    ImGui::NextColumn(); \
    ImGui::SetNextItemWidth(-1); \
    code; \
    ImGui::NextColumn();

bool IsInspectorReadOnly(const EditorContext& context) {
    // Remote Inspector: Play 模式下允许属性编辑（运行时调试）
    // 结构性操作（Add/Remove Component）仍然禁止
    (void)context;
    return false;
}

bool IsInspectorStructuralReadOnly() {
    return IsEditorInPlayMode();
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
// out_activated / out_deactivated_after_edit 聚合三个分量(X/Y/Z)子控件的激活/编辑结束态：
// 否则调用方在本 helper 之后查 IsItemActivated()/IsItemDeactivatedAfterEdit() 只会命中最后一个
// (Z) 子控件，导致改 X/Y 时漏记 undo。
bool DrawVec3WithColorLabels(const char* id, float v[3], float speed = 0.1f,
                             bool* out_activated = nullptr,
                             bool* out_deactivated_after_edit = nullptr) {
    bool changed = false;
    if (out_activated) *out_activated = false;
    if (out_deactivated_after_edit) *out_deactivated_after_edit = false;
    auto track_item_state = [&]() {
        if (out_activated && ImGui::IsItemActivated()) *out_activated = true;
        if (out_deactivated_after_edit && ImGui::IsItemDeactivatedAfterEdit())
            *out_deactivated_after_edit = true;
    };
    float line_width = ImGui::GetContentRegionAvail().x;
    float field_width = (line_width - 3 * (ImGui::GetFrameHeight() + 4.0f + ImGui::CalcTextSize("X").x + 8.0f)) / 3.0f;
    if (field_width < 30.0f) field_width = 30.0f;

    ImGui::PushID(id);

    // X (red)
    DrawColoredAxisLabel("X", ImVec4(0.85f, 0.20f, 0.20f, 1.0f));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_width);
    if (ImGui::DragFloat("##x", &v[0], speed)) changed = true;
    track_item_state();
    ImGui::SameLine();

    // Y (green)
    DrawColoredAxisLabel("Y", ImVec4(0.30f, 0.75f, 0.20f, 1.0f));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_width);
    if (ImGui::DragFloat("##y", &v[1], speed)) changed = true;
    track_item_state();
    ImGui::SameLine();

    // Z (blue)
    DrawColoredAxisLabel("Z", ImVec4(0.20f, 0.40f, 0.90f, 1.0f));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_width);
    if (ImGui::DragFloat("##z", &v[2], speed)) changed = true;
    track_item_state();

    ImGui::PopID();
    return changed;
}

void BeginInspectorReadOnlyScope(const EditorContext& context) {
    (void)context;
}

void EndInspectorReadOnlyScope(const EditorContext& context) {
    (void)context;
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

void DrawInspectorHeader(EditorContext& context) {
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

    static std::string s_name_before_edit;
    static entt::entity s_name_edit_entity = entt::null;

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 70);
    if (ImGui::InputText("##Name", name_buf, sizeof(name_buf))) {
        if (context.registry.all_of<EditorNameComponent>(context.selected_entity)) {
            context.registry.get<EditorNameComponent>(context.selected_entity).name = name_buf;
        } else {
            context.registry.emplace<EditorNameComponent>(context.selected_entity, name_buf);
        }
    }
    if (ImGui::IsItemActivated()) {
        s_name_before_edit = name_buf;
        s_name_edit_entity = context.selected_entity;
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && s_name_edit_entity == context.selected_entity) {
        std::string old_name = s_name_before_edit;
        std::string new_name = name_buf;
        entt::entity ent = context.selected_entity;
        auto& reg = context.registry;
        GetUndoRedoManager().Execute(std::make_unique<PropertyChangeCommand<std::string>>(
            "Rename Entity", old_name, new_name,
            [&reg, ent](const std::string& v) {
                if (reg.valid(ent) && reg.all_of<EditorNameComponent>(ent))
                    reg.get<EditorNameComponent>(ent).name = v;
            }), true);
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::Checkbox("Static", &context.inspector_static);
    ImGui::Separator();
}

void DrawTransformSection(EditorContext& context) {
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
    ImGui::PushID("##pos_undo");
    bool pos_activated = false, pos_deactivated = false;
    if (DrawVec3WithColorLabels("##pos", pos, 0.1f, &pos_activated, &pos_deactivated)) {
        transform.position = glm::vec3(pos[0], pos[1], pos[2]);
        transform.dirty = true;
    }
    if (pos_activated) {
        s_edit_start_pos = transform.position;
        s_edit_entity = current_entity;
    }
    if (pos_deactivated && s_edit_entity == current_entity) {
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
        if (ImGui::DragFloat("##rotZ", &rot[2], 0.1f)) {
            euler.z = rot[2];
            transform.rotation = glm::quat(glm::radians(euler));
            transform.dirty = true;
        }
        if (ImGui::IsItemActivated()) {
            s_edit_start_rot = euler;
            s_edit_entity = current_entity;
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
        bool rot_activated = false, rot_deactivated = false;
        if (DrawVec3WithColorLabels("##rot", rot, 0.1f, &rot_activated, &rot_deactivated)) {
            euler = glm::vec3(rot[0], rot[1], rot[2]);
            transform.rotation = glm::quat(glm::radians(euler));
            transform.dirty = true;
        }
        if (rot_activated) {
            s_edit_start_rot = glm::degrees(glm::eulerAngles(transform.rotation));
            s_edit_entity = current_entity;
        }
        if (rot_deactivated && s_edit_entity == current_entity) {
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
    bool scale_activated = false, scale_deactivated = false;
    if (DrawVec3WithColorLabels("##scale", scale, 0.1f, &scale_activated, &scale_deactivated)) {
        transform.scale = glm::vec3(scale[0], scale[1], scale[2]);
        transform.dirty = true;
    }
    if (scale_activated) {
        s_edit_start_scale = transform.scale;
        s_edit_entity = current_entity;
    }
    if (scale_deactivated && s_edit_entity == current_entity) {
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

#include "editor_inspector_sections_render.inl"

#include "editor_inspector_sections_ui.inl"

#include "editor_inspector_sections_env.inl"

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
        20,
        [](entt::registry& r, entt::entity e) { if (r.all_of<SpriteRendererComponent>(e)) r.erase<SpriteRendererComponent>(e); }});
    reg.Register({"RigidBody 2D", "2D", DrawRigidBody2DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<RigidBody2DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<RigidBody2DComponent>(e)) r.emplace<RigidBody2DComponent>(e); },
        21,
        [](entt::registry& r, entt::entity e) { if (r.all_of<RigidBody2DComponent>(e)) r.erase<RigidBody2DComponent>(e); }});
    reg.Register({"Particle Emitter", "2D", DrawParticleEmitterSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<ParticleEmitterComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<ParticleEmitterComponent>(e)) r.emplace<ParticleEmitterComponent>(e); },
        22,
        [](entt::registry& r, entt::entity e) { if (r.all_of<ParticleEmitterComponent>(e)) r.erase<ParticleEmitterComponent>(e); }});
    reg.Register({"Box Collider 2D", "2D", DrawBoxCollider2DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<BoxCollider2DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<BoxCollider2DComponent>(e)) r.emplace<BoxCollider2DComponent>(e); },
        23,
        [](entt::registry& r, entt::entity e) { if (r.all_of<BoxCollider2DComponent>(e)) r.erase<BoxCollider2DComponent>(e); }});
    reg.Register({"Circle Collider 2D", "2D", DrawCircleCollider2DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<CircleCollider2DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<CircleCollider2DComponent>(e)) r.emplace<CircleCollider2DComponent>(e); },
        24,
        [](entt::registry& r, entt::entity e) { if (r.all_of<CircleCollider2DComponent>(e)) r.erase<CircleCollider2DComponent>(e); }});
    reg.Register({"Polygon Collider 2D", "2D", DrawPolygonCollider2DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<PolygonCollider2DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<PolygonCollider2DComponent>(e)) r.emplace<PolygonCollider2DComponent>(e); },
        25,
        [](entt::registry& r, entt::entity e) { if (r.all_of<PolygonCollider2DComponent>(e)) r.erase<PolygonCollider2DComponent>(e); }});
    reg.Register({"Joint 2D", "2D", DrawJoint2DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<Joint2DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<Joint2DComponent>(e)) r.emplace<Joint2DComponent>(e); },
        26,
        [](entt::registry& r, entt::entity e) { if (r.all_of<Joint2DComponent>(e)) r.erase<Joint2DComponent>(e); }});

    // --- Script ---
    reg.Register({"Script", "Script", DrawScriptSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<ScriptComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<ScriptComponent>(e)) r.emplace<ScriptComponent>(e); },
        15,
        [](entt::registry& r, entt::entity e) { if (r.all_of<ScriptComponent>(e)) r.erase<ScriptComponent>(e); }});
    reg.Register({"C# Script", "Script", DrawCSharpScriptSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<CSharpScriptComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<CSharpScriptComponent>(e)) r.emplace<CSharpScriptComponent>(e); },
        16,
        [](entt::registry& r, entt::entity e) { if (r.all_of<CSharpScriptComponent>(e)) r.erase<CSharpScriptComponent>(e); }});

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
        30,
        [](entt::registry& r, entt::entity e) { if (r.all_of<UILabelComponent>(e)) r.erase<UILabelComponent>(e); }});

    // --- 3D Rendering ---
    reg.Register({"Mesh Renderer", "3D", DrawMeshRendererSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::MeshRendererComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::MeshRendererComponent>(e)) r.emplace<dse::MeshRendererComponent>(e); },
        40,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::MeshRendererComponent>(e)) r.erase<dse::MeshRendererComponent>(e); }});
    reg.Register({"Camera 3D", "3D", DrawCamera3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::Camera3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::Camera3DComponent>(e)) r.emplace<dse::Camera3DComponent>(e); },
        41,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::Camera3DComponent>(e)) r.erase<dse::Camera3DComponent>(e); }});
    reg.Register({"Directional Light", "3D", DrawDirectionalLightSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::DirectionalLight3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::DirectionalLight3DComponent>(e)) r.emplace<dse::DirectionalLight3DComponent>(e); },
        42,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::DirectionalLight3DComponent>(e)) r.erase<dse::DirectionalLight3DComponent>(e); }});
    reg.Register({"Point Light", "3D", DrawPointLightSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::PointLightComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::PointLightComponent>(e)) r.emplace<dse::PointLightComponent>(e); },
        43,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::PointLightComponent>(e)) r.erase<dse::PointLightComponent>(e); }});
    reg.Register({"Spot Light", "3D", DrawSpotLightSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::SpotLightComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::SpotLightComponent>(e)) r.emplace<dse::SpotLightComponent>(e); },
        44,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::SpotLightComponent>(e)) r.erase<dse::SpotLightComponent>(e); }});
    reg.Register({"Sky Light", "3D", DrawSkyLightSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::SkyLightComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::SkyLightComponent>(e)) r.emplace<dse::SkyLightComponent>(e); },
        45,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::SkyLightComponent>(e)) r.erase<dse::SkyLightComponent>(e); }});
    reg.Register({"Skybox", "3D", DrawSkyboxSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::SkyboxComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::SkyboxComponent>(e)) r.emplace<dse::SkyboxComponent>(e); },
        46,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::SkyboxComponent>(e)) r.erase<dse::SkyboxComponent>(e); }});
    reg.Register({"Animator 3D", "3D", DrawAnimator3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::Animator3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::Animator3DComponent>(e)) r.emplace<dse::Animator3DComponent>(e); },
        47,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::Animator3DComponent>(e)) r.erase<dse::Animator3DComponent>(e); }});
    reg.Register({"Free Camera Controller", "3D", DrawFreeCameraControllerSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::FreeCameraControllerComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::FreeCameraControllerComponent>(e)) r.emplace<dse::FreeCameraControllerComponent>(e); },
        48,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::FreeCameraControllerComponent>(e)) r.erase<dse::FreeCameraControllerComponent>(e); }});
    reg.Register({"Terrain", "3D", DrawTerrainSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<TerrainComponent>(e); },
        nullptr,  // Terrain 不通过 Add Component 添加
        49});
    reg.Register({"Post Process", "3D", DrawPostProcessSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::PostProcessComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::PostProcessComponent>(e)) r.emplace<dse::PostProcessComponent>(e); },
        50,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::PostProcessComponent>(e)) r.erase<dse::PostProcessComponent>(e); }});
    reg.Register({"Tree", "3D", DrawTreeSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::TreeComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::TreeComponent>(e)) r.emplace<dse::TreeComponent>(e); },
        51,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::TreeComponent>(e)) r.erase<dse::TreeComponent>(e); }});
    reg.Register({"Grass", "3D", DrawGrassSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::GrassComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::GrassComponent>(e)) r.emplace<dse::GrassComponent>(e); },
        52,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::GrassComponent>(e)) r.erase<dse::GrassComponent>(e); }});

    // --- Physics 3D ---
    reg.Register({"RigidBody 3D", "Physics", DrawRigidBody3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::RigidBody3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::RigidBody3DComponent>(e)) r.emplace<dse::RigidBody3DComponent>(e); },
        60,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::RigidBody3DComponent>(e)) r.erase<dse::RigidBody3DComponent>(e); }});
    reg.Register({"Box Collider 3D", "Physics", DrawBoxCollider3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::BoxCollider3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::BoxCollider3DComponent>(e)) r.emplace<dse::BoxCollider3DComponent>(e); },
        61,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::BoxCollider3DComponent>(e)) r.erase<dse::BoxCollider3DComponent>(e); }});
    reg.Register({"Sphere Collider 3D", "Physics", DrawSphereCollider3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::SphereCollider3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::SphereCollider3DComponent>(e)) r.emplace<dse::SphereCollider3DComponent>(e); },
        62,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::SphereCollider3DComponent>(e)) r.erase<dse::SphereCollider3DComponent>(e); }});
    reg.Register({"Capsule Collider 3D", "Physics", DrawCapsuleCollider3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::CapsuleCollider3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::CapsuleCollider3DComponent>(e)) r.emplace<dse::CapsuleCollider3DComponent>(e); },
        63,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::CapsuleCollider3DComponent>(e)) r.erase<dse::CapsuleCollider3DComponent>(e); }});
    reg.Register({"Character Controller 3D", "Physics", DrawCharacterController3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::CharacterController3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::CharacterController3DComponent>(e)) r.emplace<dse::CharacterController3DComponent>(e); },
        64,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::CharacterController3DComponent>(e)) r.erase<dse::CharacterController3DComponent>(e); }});
    reg.Register({"Mesh Collider 3D", "Physics", DrawMeshCollider3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::MeshCollider3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::MeshCollider3DComponent>(e)) r.emplace<dse::MeshCollider3DComponent>(e); },
        65,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::MeshCollider3DComponent>(e)) r.erase<dse::MeshCollider3DComponent>(e); }});
    reg.Register({"Joint 3D", "Physics", DrawJoint3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::Joint3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::Joint3DComponent>(e)) r.emplace<dse::Joint3DComponent>(e); },
        66,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::Joint3DComponent>(e)) r.erase<dse::Joint3DComponent>(e); }});
    reg.Register({"Particle System 3D", "3D", DrawParticleSystem3DSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::ParticleSystem3DComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::ParticleSystem3DComponent>(e)) r.emplace<dse::ParticleSystem3DComponent>(e); },
        67,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::ParticleSystem3DComponent>(e)) r.erase<dse::ParticleSystem3DComponent>(e); }});

    // --- Probes ---
    reg.Register({"Light Probe", "3D", DrawLightProbeSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::LightProbeComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::LightProbeComponent>(e)) r.emplace<dse::LightProbeComponent>(e); },
        70,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::LightProbeComponent>(e)) r.erase<dse::LightProbeComponent>(e); }});
    reg.Register({"Reflection Probe", "3D", DrawReflectionProbeSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::ReflectionProbeComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::ReflectionProbeComponent>(e)) r.emplace<dse::ReflectionProbeComponent>(e); },
        71,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::ReflectionProbeComponent>(e)) r.erase<dse::ReflectionProbeComponent>(e); }});
    reg.Register({"Decal", "3D", DrawDecalSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::DecalComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::DecalComponent>(e)) r.emplace<dse::DecalComponent>(e); },
        72,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::DecalComponent>(e)) r.erase<dse::DecalComponent>(e); }});
    reg.Register({"Water", "3D", DrawWaterSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::WaterComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::WaterComponent>(e)) r.emplace<dse::WaterComponent>(e); },
        73,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::WaterComponent>(e)) r.erase<dse::WaterComponent>(e); }});
    reg.Register({"GI Probe Volume", "3D", DrawGIProbeVolumeSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::GIProbeVolumeComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::GIProbeVolumeComponent>(e)) r.emplace<dse::GIProbeVolumeComponent>(e); },
        74,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::GIProbeVolumeComponent>(e)) r.erase<dse::GIProbeVolumeComponent>(e); }});
    reg.Register({"LOD Group", "3D", DrawLODGroupSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::LODGroupComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::LODGroupComponent>(e)) r.emplace<dse::LODGroupComponent>(e); },
        75,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::LODGroupComponent>(e)) r.erase<dse::LODGroupComponent>(e); }});
    reg.Register({"Foliage", "3D", DrawFoliageSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::FoliageComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::FoliageComponent>(e)) r.emplace<dse::FoliageComponent>(e); },
        76,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::FoliageComponent>(e)) r.erase<dse::FoliageComponent>(e); }});
    reg.Register({"Dynamic Obstacle", "3D", DrawDynamicObstacleSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::DynamicObstacleComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::DynamicObstacleComponent>(e)) r.emplace<dse::DynamicObstacleComponent>(e); },
        77,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::DynamicObstacleComponent>(e)) r.erase<dse::DynamicObstacleComponent>(e); }});
    reg.Register({"NavMesh Auto Rebake", "3D", DrawNavMeshAutoRebakeSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::NavMeshAutoRebakeComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::NavMeshAutoRebakeComponent>(e)) r.emplace<dse::NavMeshAutoRebakeComponent>(e); },
        78,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::NavMeshAutoRebakeComponent>(e)) r.erase<dse::NavMeshAutoRebakeComponent>(e); }});
    reg.Register({"Terrain Tile Manager", "3D", DrawTerrainTileManagerSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::TerrainTileManagerComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::TerrainTileManagerComponent>(e)) r.emplace<dse::TerrainTileManagerComponent>(e); },
        79,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::TerrainTileManagerComponent>(e)) r.erase<dse::TerrainTileManagerComponent>(e); }});

    // --- Advanced Physics ---
#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
    reg.Register({"Ragdoll", "Physics", DrawRagdollSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::RagdollComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::RagdollComponent>(e)) r.emplace<dse::RagdollComponent>(e); },
        80,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::RagdollComponent>(e)) r.erase<dse::RagdollComponent>(e); }});
    reg.Register({"Vehicle", "Physics", DrawVehicleSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::VehicleComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::VehicleComponent>(e)) r.emplace<dse::VehicleComponent>(e); },
        82,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::VehicleComponent>(e)) r.erase<dse::VehicleComponent>(e); }});
    reg.Register({"Buoyancy", "Physics", DrawBuoyancySection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::BuoyancyComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::BuoyancyComponent>(e)) r.emplace<dse::BuoyancyComponent>(e); },
        84,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::BuoyancyComponent>(e)) r.erase<dse::BuoyancyComponent>(e); }});
#endif
    reg.Register({"Soft Body", "Physics", DrawSoftBodySection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::SoftBodyComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::SoftBodyComponent>(e)) r.emplace<dse::SoftBodyComponent>(e); },
        81,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::SoftBodyComponent>(e)) r.erase<dse::SoftBodyComponent>(e); }});
    reg.Register({"Rope", "Physics", DrawRopeSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::RopeComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::RopeComponent>(e)) r.emplace<dse::RopeComponent>(e); },
        83,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::RopeComponent>(e)) r.erase<dse::RopeComponent>(e); }});

    // --- Sky ---
    reg.Register({"Atmosphere", "Sky", DrawAtmosphereSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::AtmosphereComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::AtmosphereComponent>(e)) r.emplace<dse::AtmosphereComponent>(e); },
        85,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::AtmosphereComponent>(e)) r.erase<dse::AtmosphereComponent>(e); }});
    reg.Register({"Volumetric Cloud", "Sky", DrawVolumetricCloudSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::VolumetricCloudComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::VolumetricCloudComponent>(e)) r.emplace<dse::VolumetricCloudComponent>(e); },
        86,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::VolumetricCloudComponent>(e)) r.erase<dse::VolumetricCloudComponent>(e); }});
    reg.Register({"Day/Night Cycle", "Sky", DrawDayNightCycleSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::DayNightCycleComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::DayNightCycleComponent>(e)) r.emplace<dse::DayNightCycleComponent>(e); },
        87,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::DayNightCycleComponent>(e)) r.erase<dse::DayNightCycleComponent>(e); }});

    // --- Hair / MorphTarget ---
    reg.Register({"Hair", "3D", DrawHairSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::HairComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::HairComponent>(e)) r.emplace<dse::HairComponent>(e); },
        88,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::HairComponent>(e)) r.erase<dse::HairComponent>(e); }});
    reg.Register({"Morph Target", "3D", DrawMorphTargetSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::MorphTargetComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::MorphTargetComponent>(e)) r.emplace<dse::MorphTargetComponent>(e); },
        89,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::MorphTargetComponent>(e)) r.erase<dse::MorphTargetComponent>(e); }});
    // --- Impostor LOD ---
    reg.Register({"Impostor LOD", "3D", DrawImpostorSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::ImpostorComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::ImpostorComponent>(e)) r.emplace<dse::ImpostorComponent>(e); },
        90,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::ImpostorComponent>(e)) r.erase<dse::ImpostorComponent>(e); }});
    // --- GPU Particles ---
    reg.Register({"GPU Particle Emitter", "3D", DrawGpuParticleSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::render::GpuParticleComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::render::GpuParticleComponent>(e)) r.emplace<dse::render::GpuParticleComponent>(e); },
        91,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::render::GpuParticleComponent>(e)) r.erase<dse::render::GpuParticleComponent>(e); }});
    // --- Lightmap ---
    reg.Register({"Lightmap", "3D", DrawLightmapSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::render::LightmapComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::render::LightmapComponent>(e)) r.emplace<dse::render::LightmapComponent>(e); },
        92,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::render::LightmapComponent>(e)) r.erase<dse::render::LightmapComponent>(e); }});

    // --- Audio (使用 editor_audio_panel.h 的 DrawAudioSection 适配) ---
    reg.Register({"Name", "Core", nullptr,
        [](entt::registry& r, entt::entity e) { return r.all_of<EditorNameComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<EditorNameComponent>(e)) r.emplace<EditorNameComponent>(e, "New Component"); },
        11});
    reg.Register({"Audio Source", "Audio",
        [](EditorContext& ctx) { DrawAudioSection(ctx.registry, ctx.selected_entity); },
        [](entt::registry& r, entt::entity e) { return r.all_of<AudioSourceComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<AudioSourceComponent>(e)) r.emplace<AudioSourceComponent>(e); },
        80,
        [](entt::registry& r, entt::entity e) { if (r.all_of<AudioSourceComponent>(e)) r.erase<AudioSourceComponent>(e); }});
    reg.Register({"Audio Listener", "Audio",
        [](EditorContext& ctx) {
            if (!ctx.registry.all_of<AudioSourceComponent>(ctx.selected_entity))
                DrawAudioSection(ctx.registry, ctx.selected_entity);
        },
        [](entt::registry& r, entt::entity e) { return r.all_of<AudioListenerComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<AudioListenerComponent>(e)) r.emplace<AudioListenerComponent>(e); },
        81,
        [](entt::registry& r, entt::entity e) { if (r.all_of<AudioListenerComponent>(e)) r.erase<AudioListenerComponent>(e); }});
    reg.Register({"UI Renderer", "UI", DrawUIRendererSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIRendererComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIRendererComponent>(e)) r.emplace<UIRendererComponent>(e); },
        28,
        [](entt::registry& r, entt::entity e) { if (r.all_of<UIRendererComponent>(e)) r.erase<UIRendererComponent>(e); }});
    reg.Register({"UI Button", "UI", DrawUIButtonSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIButtonComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIButtonComponent>(e)) r.emplace<UIButtonComponent>(e); },
        29,
        [](entt::registry& r, entt::entity e) { if (r.all_of<UIButtonComponent>(e)) r.erase<UIButtonComponent>(e); }});
    reg.Register({"UI Panel", "UI", DrawUIPanelSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIPanelComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIPanelComponent>(e)) r.emplace<UIPanelComponent>(e); },
        35,
        [](entt::registry& r, entt::entity e) { if (r.all_of<UIPanelComponent>(e)) r.erase<UIPanelComponent>(e); }});
    reg.Register({"UI Mask", "UI", DrawUIMaskSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIMaskComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIMaskComponent>(e)) r.emplace<UIMaskComponent>(e); },
        36,
        [](entt::registry& r, entt::entity e) { if (r.all_of<UIMaskComponent>(e)) r.erase<UIMaskComponent>(e); }});
    reg.Register({"UI Rich Text", "UI", DrawUIRichTextSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIRichTextComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIRichTextComponent>(e)) r.emplace<UIRichTextComponent>(e); },
        37,
        [](entt::registry& r, entt::entity e) { if (r.all_of<UIRichTextComponent>(e)) r.erase<UIRichTextComponent>(e); }});
    reg.Register({"UI Joystick", "UI", DrawUIJoystickSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIJoystickComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIJoystickComponent>(e)) r.emplace<UIJoystickComponent>(e); },
        38,
        [](entt::registry& r, entt::entity e) { if (r.all_of<UIJoystickComponent>(e)) r.erase<UIJoystickComponent>(e); }});
    reg.Register({"UI Anchor", "UI", DrawUIAnchorSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIAnchorComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIAnchorComponent>(e)) r.emplace<UIAnchorComponent>(e); },
        31,
        [](entt::registry& r, entt::entity e) { if (r.all_of<UIAnchorComponent>(e)) r.erase<UIAnchorComponent>(e); }});
    reg.Register({"UI Grid Layout", "UI", DrawUIGridLayoutSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIGridLayoutComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIGridLayoutComponent>(e)) r.emplace<UIGridLayoutComponent>(e); },
        32,
        [](entt::registry& r, entt::entity e) { if (r.all_of<UIGridLayoutComponent>(e)) r.erase<UIGridLayoutComponent>(e); }});
    reg.Register({"UI Canvas Scaler", "UI", DrawUICanvasScalerSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<UICanvasScalerComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UICanvasScalerComponent>(e)) r.emplace<UICanvasScalerComponent>(e); },
        33,
        [](entt::registry& r, entt::entity e) { if (r.all_of<UICanvasScalerComponent>(e)) r.erase<UICanvasScalerComponent>(e); }});
    reg.Register({"UI Animation", "UI", DrawUIAnimationSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<UIAnimationComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<UIAnimationComponent>(e)) r.emplace<UIAnimationComponent>(e); },
        34,
        [](entt::registry& r, entt::entity e) { if (r.all_of<UIAnimationComponent>(e)) r.erase<UIAnimationComponent>(e); }});

    // --- Advanced Physics ---
#ifdef DSE_ENABLE_PHYSX
    reg.Register({"Ragdoll", "Physics", DrawRagdollSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::RagdollComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::RagdollComponent>(e)) r.emplace<dse::RagdollComponent>(e); },
        90,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::RagdollComponent>(e)) r.erase<dse::RagdollComponent>(e); }});
    reg.Register({"Vehicle", "Physics", DrawVehicleSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::VehicleComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::VehicleComponent>(e)) r.emplace<dse::VehicleComponent>(e); },
        92,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::VehicleComponent>(e)) r.erase<dse::VehicleComponent>(e); }});
    reg.Register({"Buoyancy", "Physics", DrawBuoyancySection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::BuoyancyComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::BuoyancyComponent>(e)) r.emplace<dse::BuoyancyComponent>(e); },
        94,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::BuoyancyComponent>(e)) r.erase<dse::BuoyancyComponent>(e); }});
#endif
    reg.Register({"Soft Body", "Physics", DrawSoftBodySection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::SoftBodyComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::SoftBodyComponent>(e)) r.emplace<dse::SoftBodyComponent>(e); },
        91,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::SoftBodyComponent>(e)) r.erase<dse::SoftBodyComponent>(e); }});
    reg.Register({"Rope", "Physics", DrawRopeSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<dse::RopeComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::RopeComponent>(e)) r.emplace<dse::RopeComponent>(e); },
        93,
        [](entt::registry& r, entt::entity e) { if (r.all_of<dse::RopeComponent>(e)) r.erase<dse::RopeComponent>(e); }});

    // --- Spine ---
    reg.Register({"Spine Renderer", "2D", DrawSpineRendererSection,
        [](entt::registry& r, entt::entity e) { return r.all_of<SpineRendererComponent>(e); },
        [](entt::registry& r, entt::entity e) { if (!r.all_of<SpineRendererComponent>(e)) r.emplace<SpineRendererComponent>(e); },
        27,
        [](entt::registry& r, entt::entity e) { if (r.all_of<SpineRendererComponent>(e)) r.erase<SpineRendererComponent>(e); }});
}

} // namespace

// ─── InspectorRegistry 方法实现 ─────────────────────────────────────────────

void InspectorRegistry::DrawAll(EditorContext& context) {
    for (const auto& entry : GetEntries()) {
        if (entry.draw) {
            entry.draw(context);
        }
    }
}

namespace {
// 把「添加组件」包成可撤销命令：Execute=add，Undo=remove（仅触及该组件类型，
// 不影响实体上其它组件）。
void ExecuteAddComponent(entt::registry& registry, entt::entity entity,
                         const std::string& name,
                         ComponentAddFunc add_fn, ComponentRemoveFunc remove_fn) {
    entt::registry* reg = &registry;
    GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
        "Add " + name,
        [reg, entity, add_fn]()    { if (add_fn) add_fn(*reg, entity); },
        [reg, entity, remove_fn]() { if (remove_fn) remove_fn(*reg, entity); }
    ), false);
}

// 把「移除组件」包成可撤销命令：移除前用 EntitySnapshot 抓取整实体组件数据，
// Undo 时仅补回「当前缺失」的组件（即被移除的那个），连同其数据一并恢复，
// 而其它组件（可能在此期间被改过）保持不动。
void ExecuteRemoveComponent(entt::registry& registry, entt::entity entity,
                            const std::string& name, ComponentRemoveFunc remove_fn) {
    auto snap = std::make_shared<EntitySnapshot>(EntitySnapshot::Capture(registry, entity));
    entt::registry* reg = &registry;
    GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
        "Remove " + name,
        [reg, entity, remove_fn]() { if (remove_fn) remove_fn(*reg, entity); },
        [reg, entity, snap]()      { snap->RestoreMissingInPlace(*reg, entity); }
    ), false);
}
} // namespace

void InspectorRegistry::DrawAddComponentMenu(EditorContext& context) {
    const bool read_only = IsInspectorStructuralReadOnly();
    ImGui::Separator();
    ImGui::Spacing();

    const float pad = ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
    const float add_w = ImGui::CalcTextSize("Add Component").x + pad;
    const float rem_w = ImGui::CalcTextSize("Remove Component").x + pad;
    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float total_w = add_w + sp + rem_w;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_w) * 0.5f);

    if (read_only) {
        ImGui::BeginDisabled(true);
    }
    if (ImGui::Button("Add Component", ImVec2(add_w, 30))) {
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
            ExecuteAddComponent(context.registry, context.selected_entity,
                                entry.component_name, entry.add, entry.remove);
        }
    }
    ImGui::EndPopup();
}

void InspectorRegistry::DrawRemoveComponentMenu(EditorContext& context) {
    if (IsInspectorStructuralReadOnly()) return;

    bool any_removable = false;
    for (const auto& entry : GetEntries()) {
        if (!entry.remove) continue;
        if (entry.has && entry.has(context.registry, context.selected_entity)) {
            any_removable = true;
            break;
        }
    }

    const float pad = ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
    const float rem_w = ImGui::CalcTextSize("Remove Component").x + pad;
    ImGui::SameLine();
    if (!any_removable) ImGui::BeginDisabled(true);
    if (ImGui::Button("Remove Component", ImVec2(rem_w, 30))) {
        ImGui::OpenPopup("RemoveComponentPopup");
    }
    if (!any_removable) ImGui::EndDisabled();

    if (!ImGui::BeginPopup("RemoveComponentPopup")) return;

    std::string last_category;
    for (const auto& entry : GetEntries()) {
        if (!entry.remove) continue;
        if (!entry.has || !entry.has(context.registry, context.selected_entity)) continue;

        if (!last_category.empty() && entry.category != last_category) {
            ImGui::Separator();
        }
        last_category = entry.category;

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
        if (ImGui::MenuItem(entry.component_name.c_str())) {
            ExecuteRemoveComponent(context.registry, context.selected_entity,
                                   entry.component_name, entry.remove);
        }
        ImGui::PopStyleColor();
    }

    if (!any_removable) {
        ImGui::TextDisabled("No removable components");
    }

    ImGui::EndPopup();
}

// ─── DrawInspectorPanel（注册表驱动） ────────────────────────────────────────

void DrawInspectorPanel(EditorContext& context) {
    RegisterAllInspectorSections();

    ImGui::Begin("Inspector", PanelRegistry::Get().GetCurrentPanelOpen());
    PanelRegistry::Get().DrawMaximizeRestoreButton();

    if (IsEditorInPlayMode()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.15f, 0.0f, 0.8f));
        ImGui::BeginChild("##remote_banner", ImVec2(0, 24), false);
        ImGui::SetCursorPosX(4);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
            MDI_ICON_ALERT "  Remote Inspector — changes will be lost on Stop");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    auto& selection = SelectionManager::Get();
    if (selection.IsMultiSelect()) {
        static glm::vec3 s_ms_pos_delta{0.0f, 0.0f, 0.0f};
        static glm::vec3 s_ms_scale_factor{1.0f, 1.0f, 1.0f};

        ImGui::Text("%d entities selected", selection.Count());
        ImGui::Separator();

        // ── Average info (read-only) ──────────────────────────────────────
        glm::vec3 avg_pos(0.0f);
        glm::vec3 avg_scale(0.0f);
        int transform_count = 0;
        for (auto ent : selection.GetAll()) {
            if (context.registry.valid(ent) && context.registry.all_of<TransformComponent>(ent)) {
                const auto& t = context.registry.get<TransformComponent>(ent);
                avg_pos += t.position;
                avg_scale += t.scale;
                transform_count++;
            }
        }
        if (transform_count > 0) {
            avg_pos /= static_cast<float>(transform_count);
            avg_scale /= static_cast<float>(transform_count);
            ImGui::TextDisabled("Avg pos  (%.2f, %.2f, %.2f)", avg_pos.x, avg_pos.y, avg_pos.z);
            ImGui::TextDisabled("Avg scale (%.2f, %.2f, %.2f)", avg_scale.x, avg_scale.y, avg_scale.z);
        }
        ImGui::Separator();

        // ── Batch Move (delta) ────────────────────────────────────────────
        if (ImGui::CollapsingHeader(MDI_ICON_ARROW_ALL "  Batch Move", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Move Delta:");
            float pd[3] = {s_ms_pos_delta.x, s_ms_pos_delta.y, s_ms_pos_delta.z};
            DrawVec3WithColorLabels("##ms_pd", pd);
            s_ms_pos_delta = {pd[0], pd[1], pd[2]};

            if (ImGui::Button("Apply Move##ms")) {
                auto compound = std::make_unique<CompoundCommand>("Batch Move");
                for (auto ent : selection.GetAll()) {
                    if (!context.registry.valid(ent)) continue;
                    if (!context.registry.all_of<TransformComponent>(ent)) continue;
                    auto& t = context.registry.get<TransformComponent>(ent);
                    glm::vec3 old_pos = t.position;
                    glm::vec3 new_pos = t.position + s_ms_pos_delta;
                    t.position = new_pos;
                    t.dirty = true;
                    auto& reg = context.registry;
                    compound->AddCommand(std::make_unique<PropertyChangeCommand<glm::vec3>>(
                        "Transform.Position", old_pos, new_pos,
                        [&reg, ent](const glm::vec3& v) {
                            if (reg.valid(ent) && reg.all_of<TransformComponent>(ent)) {
                                reg.get<TransformComponent>(ent).position = v;
                                reg.get<TransformComponent>(ent).dirty = true;
                            }
                        }));
                }
                if (!compound->IsEmpty())
                    GetUndoRedoManager().Execute(std::move(compound), false);
                s_ms_pos_delta = {0.0f, 0.0f, 0.0f};
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset##ms_pos")) s_ms_pos_delta = {0.0f, 0.0f, 0.0f};
        }

        // ── Batch Scale (factor) ──────────────────────────────────────────
        if (ImGui::CollapsingHeader(MDI_ICON_RESIZE "  Batch Scale", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Scale Factor:");
            float sf[3] = {s_ms_scale_factor.x, s_ms_scale_factor.y, s_ms_scale_factor.z};
            DrawVec3WithColorLabels("##ms_sf", sf, 0.01f);
            s_ms_scale_factor = {sf[0], sf[1], sf[2]};

            if (ImGui::Button("Apply Scale##ms")) {
                auto compound = std::make_unique<CompoundCommand>("Batch Scale");
                for (auto ent : selection.GetAll()) {
                    if (!context.registry.valid(ent)) continue;
                    if (!context.registry.all_of<TransformComponent>(ent)) continue;
                    auto& t = context.registry.get<TransformComponent>(ent);
                    glm::vec3 old_scale = t.scale;
                    glm::vec3 new_scale = t.scale * s_ms_scale_factor;
                    t.scale = new_scale;
                    t.dirty = true;
                    auto& reg = context.registry;
                    compound->AddCommand(std::make_unique<PropertyChangeCommand<glm::vec3>>(
                        "Transform.Scale", old_scale, new_scale,
                        [&reg, ent](const glm::vec3& v) {
                            if (reg.valid(ent) && reg.all_of<TransformComponent>(ent)) {
                                reg.get<TransformComponent>(ent).scale = v;
                                reg.get<TransformComponent>(ent).dirty = true;
                            }
                        }));
                }
                if (!compound->IsEmpty())
                    GetUndoRedoManager().Execute(std::move(compound), false);
                s_ms_scale_factor = {1.0f, 1.0f, 1.0f};
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset##ms_scale")) s_ms_scale_factor = {1.0f, 1.0f, 1.0f};
        }

        // ── Batch Rotate (delta, euler degrees) ─────────────────────────────
        {
            static glm::vec3 s_ms_rot_delta{0.0f, 0.0f, 0.0f};
            if (ImGui::CollapsingHeader(MDI_ICON_ROTATE_3D_VARIANT "  Batch Rotate")) {
                ImGui::Text("Rotation Delta (degrees):");
                float rd[3] = {s_ms_rot_delta.x, s_ms_rot_delta.y, s_ms_rot_delta.z};
                DrawVec3WithColorLabels("##ms_rd", rd, 1.0f);
                s_ms_rot_delta = {rd[0], rd[1], rd[2]};

                if (ImGui::Button("Apply Rotate##ms")) {
                    auto compound = std::make_unique<CompoundCommand>("Batch Rotate");
                    for (auto ent : selection.GetAll()) {
                        if (!context.registry.valid(ent)) continue;
                        if (!context.registry.all_of<TransformComponent>(ent)) continue;
                        auto& t = context.registry.get<TransformComponent>(ent);
                        glm::quat delta_q = glm::quat(glm::radians(s_ms_rot_delta));
                        glm::quat old_rot = t.rotation;
                        glm::quat new_rot = delta_q * t.rotation;
                        t.rotation = new_rot;
                        t.dirty = true;
                        auto& reg = context.registry;
                        compound->AddCommand(std::make_unique<PropertyChangeCommand<glm::quat>>(
                            "Transform.Rotation", old_rot, new_rot,
                            [&reg, ent](const glm::quat& v) {
                                if (reg.valid(ent) && reg.all_of<TransformComponent>(ent)) {
                                    reg.get<TransformComponent>(ent).rotation = v;
                                    reg.get<TransformComponent>(ent).dirty = true;
                                }
                            }));
                    }
                    if (!compound->IsEmpty())
                        GetUndoRedoManager().Execute(std::move(compound), false);
                    s_ms_rot_delta = {0.0f, 0.0f, 0.0f};
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset##ms_rot")) s_ms_rot_delta = {0.0f, 0.0f, 0.0f};
            }
        }

        ImGui::Separator();

        // ── Batch Set Position (absolute) ───────────────────────────────────
        {
            static glm::vec3 s_ms_set_pos{0.0f, 0.0f, 0.0f};
            if (ImGui::CollapsingHeader("Set All Positions")) {
                float sp[3] = {s_ms_set_pos.x, s_ms_set_pos.y, s_ms_set_pos.z};
                DrawVec3WithColorLabels("##ms_sp", sp);
                s_ms_set_pos = {sp[0], sp[1], sp[2]};
                if (ImGui::Button("Apply##ms_setpos")) {
                    auto compound = std::make_unique<CompoundCommand>("Batch Set Position");
                    for (auto ent : selection.GetAll()) {
                        if (!context.registry.valid(ent)) continue;
                        if (!context.registry.all_of<TransformComponent>(ent)) continue;
                        auto& t = context.registry.get<TransformComponent>(ent);
                        glm::vec3 old_pos = t.position;
                        t.position = s_ms_set_pos;
                        t.dirty = true;
                        auto& reg = context.registry;
                        compound->AddCommand(std::make_unique<PropertyChangeCommand<glm::vec3>>(
                            "Transform.Position", old_pos, s_ms_set_pos,
                            [&reg, ent](const glm::vec3& v) {
                                if (reg.valid(ent) && reg.all_of<TransformComponent>(ent)) {
                                    reg.get<TransformComponent>(ent).position = v;
                                    reg.get<TransformComponent>(ent).dirty = true;
                                }
                            }));
                    }
                    if (!compound->IsEmpty())
                        GetUndoRedoManager().Execute(std::move(compound), false);
                }
            }
        }

        ImGui::Separator();

        // ── Common Components Summary ───────────────────────────────────────
        if (ImGui::CollapsingHeader("Common Components")) {
            auto all_ents = selection.GetAll();
            if (!all_ents.empty()) {
                auto check_all_have = [&](auto tag, const char* name) {
                    using C = decltype(tag);
                    int count = 0;
                    for (auto ent : all_ents) {
                        if (context.registry.valid(ent) && context.registry.all_of<C>(ent)) count++;
                    }
                    if (count == static_cast<int>(all_ents.size())) {
                        ImGui::BulletText("%s (%d/%d)", name, count, (int)all_ents.size());
                    } else if (count > 0) {
                        ImGui::BulletText("%s (%d/%d - partial)", name, count, (int)all_ents.size());
                    }
                };
                check_all_have(TransformComponent{}, "Transform");
                check_all_have(SpriteRendererComponent{}, "SpriteRenderer");
                check_all_have(dse::MeshRendererComponent{}, "MeshRenderer");
                check_all_have(dse::Camera3DComponent{}, "Camera3D");
                check_all_have(dse::DirectionalLight3DComponent{}, "DirectionalLight");
                check_all_have(dse::PointLightComponent{}, "PointLight");
                check_all_have(dse::RigidBody3DComponent{}, "RigidBody3D");
                check_all_have(dse::BoxCollider3DComponent{}, "BoxCollider3D");
                check_all_have(dse::SphereCollider3DComponent{}, "SphereCollider3D");
            }
        }

        // ── Batch Delete ────────────────────────────────────────────────────
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Delete All Selected", ImVec2(-1, 0))) {
            auto entities = selection.GetAll();
            for (auto ent : entities) {
                if (context.registry.valid(ent)) {
                    context.world.DestroyEntity(ent);
                }
            }
            selection.Clear();
            context.selected_entity = entt::null;
            EditorLog(LogLevel::Info, "Deleted " + std::to_string(entities.size()) + " entities");
        }
        ImGui::PopStyleColor();
    } else if (context.selected_entity != entt::null && context.registry.valid(context.selected_entity)) {
        DrawInspectorHeader(context);

        // 注册表驱动：遍历所有已注册的组件 Section
        InspectorRegistry::Get().DrawAll(context);

        // 粒子曲线编辑器（特殊签名，跟随 ParticleEmitter Section 后绘制）
        DrawParticleCurveEditor(context.registry, context.selected_entity);

        // Prefab override tracking
        DrawPrefabOverrideSection(context);

        // Add / Remove Component 菜单（注册表驱动）
        InspectorRegistry::Get().DrawAddComponentMenu(context);
        InspectorRegistry::Get().DrawRemoveComponentMenu(context);
        ImGui::PopStyleColor();
    } else {
        ImGui::TextDisabled("No Entity Selected");
    }
    ImGui::End();
}

// P0-6 self-registration: data-driven; editor_app binds visibility by id.
DSE_EDITOR_PANEL([](dse::editor::PanelRegistry& reg) {
    dse::editor::PanelEntry e;
    e.id = "inspector";
    e.display_name = "Inspector";
    e.category = "Core";
    e.menu_icon = MDI_ICON_INFORMATION;
    e.order = 20;
    e.default_visible = true;
    e.draw = [](dse::editor::EditorContext& ctx) { DrawInspectorPanel(ctx); };
    reg.Register(std::move(e));
});

} // namespace dse::editor
