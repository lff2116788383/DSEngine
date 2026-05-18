#include "editor_scene_io.h"

#include <fstream>
#include <iterator>
#include <utility>

#include <glm/gtc/type_ptr.hpp>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"

#include "editor_shared_components.h"

namespace {
void WriteVec2(rapidjson::Value& parent,
               const char* name,
               const glm::vec2& value,
               rapidjson::Document::AllocatorType& allocator) {
    rapidjson::Value arr(rapidjson::kArrayType);
    arr.PushBack(value.x, allocator).PushBack(value.y, allocator);
    parent.AddMember(rapidjson::Value(name, allocator).Move(), arr, allocator);
}

void WriteVec3(rapidjson::Value& parent,
               const char* name,
               const glm::vec3& value,
               rapidjson::Document::AllocatorType& allocator) {
    rapidjson::Value arr(rapidjson::kArrayType);
    arr.PushBack(value.x, allocator).PushBack(value.y, allocator).PushBack(value.z, allocator);
    parent.AddMember(rapidjson::Value(name, allocator).Move(), arr, allocator);
}

void WriteVec4(rapidjson::Value& parent,
               const char* name,
               const glm::vec4& value,
               rapidjson::Document::AllocatorType& allocator) {
    rapidjson::Value arr(rapidjson::kArrayType);
    arr.PushBack(value.x, allocator).PushBack(value.y, allocator).PushBack(value.z, allocator).PushBack(value.w, allocator);
    parent.AddMember(rapidjson::Value(name, allocator).Move(), arr, allocator);
}

void ReadVec2(const rapidjson::Value& parent, const char* name, glm::vec2& out) {
    if (parent.HasMember(name) && parent[name].IsArray()) {
        auto arr = parent[name].GetArray();
        if (arr.Size() >= 2) {
            out = glm::vec2(arr[0].GetFloat(), arr[1].GetFloat());
        }
    }
}

void ReadVec3(const rapidjson::Value& parent, const char* name, glm::vec3& out) {
    if (parent.HasMember(name) && parent[name].IsArray()) {
        auto arr = parent[name].GetArray();
        if (arr.Size() >= 3) {
            out = glm::vec3(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat());
        }
    }
}

void ReadVec4(const rapidjson::Value& parent, const char* name, glm::vec4& out) {
    if (parent.HasMember(name) && parent[name].IsArray()) {
        auto arr = parent[name].GetArray();
        if (arr.Size() >= 4) {
            out = glm::vec4(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat(), arr[3].GetFloat());
        }
    }
}
}

namespace dse::editor {

void CopyRegistry(entt::registry& dst, entt::registry& src) {
    dst.clear();
    for (auto entity : src.storage<entt::entity>()) {
        if (!src.valid(entity)) continue;
        auto new_ent = dst.create(entity);

        if (src.all_of<EditorNameComponent>(entity)) dst.emplace<EditorNameComponent>(new_ent, src.get<EditorNameComponent>(entity));
        if (src.all_of<SiblingIndexComponent>(entity)) dst.emplace<SiblingIndexComponent>(new_ent, src.get<SiblingIndexComponent>(entity));
        if (src.all_of<TransformComponent>(entity)) dst.emplace<TransformComponent>(new_ent, src.get<TransformComponent>(entity));
        if (src.all_of<SpriteRendererComponent>(entity)) dst.emplace<SpriteRendererComponent>(new_ent, src.get<SpriteRendererComponent>(entity));
        if (src.all_of<UIRendererComponent>(entity)) {
            auto ui = src.get<UIRendererComponent>(entity);
            ui.is_hovered = false;
            ui.is_pressed = false;
            ui.runtime_model = glm::mat4(1.0f);
            dst.emplace<UIRendererComponent>(new_ent, std::move(ui));
        }
        if (src.all_of<UILabelComponent>(entity)) {
            auto label = src.get<UILabelComponent>(entity);
            label.runtime_glyph_entities.clear();
            label.dirty = true;
            dst.emplace<UILabelComponent>(new_ent, std::move(label));
        }
        if (src.all_of<UIAnchorComponent>(entity)) dst.emplace<UIAnchorComponent>(new_ent, src.get<UIAnchorComponent>(entity));
        if (src.all_of<UIGridLayoutComponent>(entity)) dst.emplace<UIGridLayoutComponent>(new_ent, src.get<UIGridLayoutComponent>(entity));
        if (src.all_of<UICanvasScalerComponent>(entity)) dst.emplace<UICanvasScalerComponent>(new_ent, src.get<UICanvasScalerComponent>(entity));
        if (src.all_of<UIAnimationComponent>(entity)) dst.emplace<UIAnimationComponent>(new_ent, src.get<UIAnimationComponent>(entity));
        if (src.all_of<UIRichTextComponent>(entity)) {
            auto rich = src.get<UIRichTextComponent>(entity);
            rich.dirty = true;
            dst.emplace<UIRichTextComponent>(new_ent, std::move(rich));
        }
        if (src.all_of<RigidBody2DComponent>(entity)) {
            auto rigidbody = src.get<RigidBody2DComponent>(entity);
            rigidbody.runtime_body = nullptr;
            dst.emplace<RigidBody2DComponent>(new_ent, std::move(rigidbody));
        }
        if (src.all_of<ParticleEmitterComponent>(entity)) {
            auto emitter = src.get<ParticleEmitterComponent>(entity);
            emitter.particles.clear();
            emitter.emit_accumulator = 0.0f;
            emitter.pending_burst = 0;
            dst.emplace<ParticleEmitterComponent>(new_ent, std::move(emitter));
        }

        if (src.all_of<dse::MeshRendererComponent>(entity)) {
            dst.emplace<dse::MeshRendererComponent>(new_ent, src.get<dse::MeshRendererComponent>(entity));
        }
        if (src.all_of<dse::Camera3DComponent>(entity)) {
            dst.emplace<dse::Camera3DComponent>(new_ent, src.get<dse::Camera3DComponent>(entity));
        }
        if (src.all_of<dse::DirectionalLight3DComponent>(entity)) {
            dst.emplace<dse::DirectionalLight3DComponent>(new_ent, src.get<dse::DirectionalLight3DComponent>(entity));
        }
        if (src.all_of<dse::PointLightComponent>(entity)) {
            dst.emplace<dse::PointLightComponent>(new_ent, src.get<dse::PointLightComponent>(entity));
        }
        if (src.all_of<dse::SpotLightComponent>(entity)) {
            dst.emplace<dse::SpotLightComponent>(new_ent, src.get<dse::SpotLightComponent>(entity));
        }
        if (src.all_of<dse::SkyLightComponent>(entity)) {
            dst.emplace<dse::SkyLightComponent>(new_ent, src.get<dse::SkyLightComponent>(entity));
        }
        if (src.all_of<dse::SkyboxComponent>(entity)) {
            dst.emplace<dse::SkyboxComponent>(new_ent, src.get<dse::SkyboxComponent>(entity));
        }
        if (src.all_of<dse::Animator3DComponent>(entity)) {
            auto animator = src.get<dse::Animator3DComponent>(entity);
            animator.state_machine.reset();
            animator.final_bone_matrices.clear();
            animator.transition_progress = 0.0f;
            animator.current_time = 0.0f;
            dst.emplace<dse::Animator3DComponent>(new_ent, std::move(animator));
        }
        if (src.all_of<dse::TerrainComponent>(entity)) {
            auto terrain = src.get<dse::TerrainComponent>(entity);
            terrain.is_dirty = true;
            dst.emplace<dse::TerrainComponent>(new_ent, std::move(terrain));
        }
    }
}

void SaveScene(entt::registry& registry, const std::string& filepath) {
    rapidjson::Document doc;
    doc.SetArray();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    for (auto entity : registry.storage<entt::entity>()) {
        if (!registry.valid(entity)) continue;
        rapidjson::Value ent_obj(rapidjson::kObjectType);
        ent_obj.AddMember("id", static_cast<uint32_t>(entity), allocator);

        if (registry.all_of<EditorNameComponent>(entity)) {
            rapidjson::Value name_val;
            name_val.SetString(registry.get<EditorNameComponent>(entity).name.c_str(), allocator);
            ent_obj.AddMember("name", name_val, allocator);
        }

        if (registry.all_of<SiblingIndexComponent>(entity)) {
            rapidjson::Value si_obj(rapidjson::kObjectType);
            si_obj.AddMember("index", registry.get<SiblingIndexComponent>(entity).index, allocator);
            ent_obj.AddMember("sibling_index", si_obj, allocator);
        }

        if (registry.all_of<TransformComponent>(entity)) {
            auto& t = registry.get<TransformComponent>(entity);
            rapidjson::Value t_obj(rapidjson::kObjectType);
            WriteVec3(t_obj, "position", t.position, allocator);
            rapidjson::Value rot(rapidjson::kArrayType);
            rot.PushBack(t.rotation.x, allocator).PushBack(t.rotation.y, allocator).PushBack(t.rotation.z, allocator).PushBack(t.rotation.w, allocator);
            t_obj.AddMember("rotation", rot, allocator);
            WriteVec3(t_obj, "scale", t.scale, allocator);
            t_obj.AddMember("dirty", t.dirty, allocator);
            ent_obj.AddMember("transform", t_obj, allocator);
        }

        if (registry.all_of<SpriteRendererComponent>(entity)) {
            auto& s = registry.get<SpriteRendererComponent>(entity);
            rapidjson::Value s_obj(rapidjson::kObjectType);
            rapidjson::Value path_val;
            path_val.SetString(s.shader_variant.c_str(), allocator);
            s_obj.AddMember("path", path_val, allocator);
            WriteVec4(s_obj, "color", s.color, allocator);
            WriteVec4(s_obj, "uv", s.uv, allocator);
            WriteVec2(s_obj, "uv_offset", s.uv_offset, allocator);
            WriteVec2(s_obj, "uv_scroll_speed", s.uv_scroll_speed, allocator);
            s_obj.AddMember("sorting_layer", s.sorting_layer, allocator);
            s_obj.AddMember("order_in_layer", s.order_in_layer, allocator);
            s_obj.AddMember("visible", s.visible, allocator);
            ent_obj.AddMember("sprite", s_obj, allocator);
        }

        if (registry.all_of<UIRendererComponent>(entity)) {
            auto& ui = registry.get<UIRendererComponent>(entity);
            rapidjson::Value ui_obj(rapidjson::kObjectType);
            ui_obj.AddMember("texture_handle", ui.texture_handle, allocator);
            WriteVec4(ui_obj, "color", ui.color, allocator);
            WriteVec4(ui_obj, "uv", ui.uv, allocator);
            ui_obj.AddMember("order", ui.order, allocator);
            ui_obj.AddMember("visible", ui.visible, allocator);
            ui_obj.AddMember("scale", ui.scale, allocator);
            ui_obj.AddMember("hover_scale", ui.hover_scale, allocator);
            ui_obj.AddMember("pressed_scale", ui.pressed_scale, allocator);
            ui_obj.AddMember("scale_lerp_speed", ui.scale_lerp_speed, allocator);
            WriteVec2(ui_obj, "position", ui.position, allocator);
            WriteVec2(ui_obj, "size", ui.size, allocator);
            WriteVec2(ui_obj, "anchor_min", ui.anchor_min, allocator);
            WriteVec2(ui_obj, "anchor_max", ui.anchor_max, allocator);
            WriteVec2(ui_obj, "pivot", ui.pivot, allocator);
            ui_obj.AddMember("interactable", ui.interactable, allocator);
            ent_obj.AddMember("ui_renderer", ui_obj, allocator);
        }

        if (registry.all_of<UILabelComponent>(entity)) {
            auto& label = registry.get<UILabelComponent>(entity);
            rapidjson::Value label_obj(rapidjson::kObjectType);
            label_obj.AddMember("text", rapidjson::Value(label.text.c_str(), allocator).Move(), allocator);
            label_obj.AddMember("use_localization", label.use_localization, allocator);
            label_obj.AddMember("localization_key", rapidjson::Value(label.localization_key.c_str(), allocator).Move(), allocator);
            label_obj.AddMember("fallback_text", rapidjson::Value(label.fallback_text.c_str(), allocator).Move(), allocator);
            rapidjson::Value params_obj(rapidjson::kObjectType);
            for (const auto& [key, value] : label.localization_params) {
                params_obj.AddMember(rapidjson::Value(key.c_str(), allocator).Move(), rapidjson::Value(value.c_str(), allocator).Move(), allocator);
            }
            label_obj.AddMember("localization_params", params_obj, allocator);
            label_obj.AddMember("number_value", label.number_value, allocator);
            label_obj.AddMember("numeric_mode", label.numeric_mode, allocator);
            label_obj.AddMember("font_texture_handle", label.font_texture_handle, allocator);
            WriteVec2(label_obj, "glyph_size", label.glyph_size, allocator);
            WriteVec2(label_obj, "offset", label.offset, allocator);
            label_obj.AddMember("spacing", label.spacing, allocator);
            label_obj.AddMember("atlas_cols", label.atlas_cols, allocator);
            label_obj.AddMember("atlas_rows", label.atlas_rows, allocator);
            label_obj.AddMember("ascii_start", label.ascii_start, allocator);
            WriteVec4(label_obj, "color", label.color, allocator);
            ent_obj.AddMember("ui_label", label_obj, allocator);
        }

        if (registry.all_of<RigidBody2DComponent>(entity)) {
            auto& rb = registry.get<RigidBody2DComponent>(entity);
            rapidjson::Value rb_obj(rapidjson::kObjectType);
            rb_obj.AddMember("type", static_cast<int>(rb.type), allocator);
            WriteVec2(rb_obj, "velocity", rb.velocity, allocator);
            rb_obj.AddMember("gravity_scale", rb.gravity_scale, allocator);
            rb_obj.AddMember("fixed_rotation", rb.fixed_rotation, allocator);
            ent_obj.AddMember("rigidbody2d", rb_obj, allocator);
        }

        if (registry.all_of<ParticleEmitterComponent>(entity)) {
            auto& emitter = registry.get<ParticleEmitterComponent>(entity);
            rapidjson::Value emitter_obj(rapidjson::kObjectType);
            emitter_obj.AddMember("texture_handle", emitter.texture_handle, allocator);
            emitter_obj.AddMember("max_particles", emitter.max_particles, allocator);
            emitter_obj.AddMember("emit_rate", emitter.emit_rate, allocator);
            emitter_obj.AddMember("emit_rate_scale", emitter.emit_rate_scale, allocator);
            emitter_obj.AddMember("emitting", emitter.emitting, allocator);
            emitter_obj.AddMember("start_life_time", emitter.start_life_time, allocator);
            emitter_obj.AddMember("start_size", emitter.start_size, allocator);
            WriteVec4(emitter_obj, "start_color", emitter.start_color, allocator);
            WriteVec3(emitter_obj, "velocity_min", emitter.velocity_min, allocator);
            WriteVec3(emitter_obj, "velocity_max", emitter.velocity_max, allocator);
            emitter_obj.AddMember("life_time_min", emitter.life_time_min, allocator);
            emitter_obj.AddMember("life_time_max", emitter.life_time_max, allocator);
            emitter_obj.AddMember("size_min", emitter.size_min, allocator);
            emitter_obj.AddMember("size_max", emitter.size_max, allocator);
            emitter_obj.AddMember("rotation_min", emitter.rotation_min, allocator);
            emitter_obj.AddMember("rotation_max", emitter.rotation_max, allocator);
            emitter_obj.AddMember("angular_velocity_min", emitter.angular_velocity_min, allocator);
            emitter_obj.AddMember("angular_velocity_max", emitter.angular_velocity_max, allocator);
            emitter_obj.AddMember("use_random_params", emitter.use_random_params, allocator);
            emitter_obj.AddMember("use_size_curve", emitter.use_size_curve, allocator);
            emitter_obj.AddMember("size_curve_end", emitter.size_curve_end, allocator);
            emitter_obj.AddMember("use_alpha_curve", emitter.use_alpha_curve, allocator);
            emitter_obj.AddMember("alpha_curve_end", emitter.alpha_curve_end, allocator);
            emitter_obj.AddMember("use_color_curve", emitter.use_color_curve, allocator);
            WriteVec4(emitter_obj, "color_curve_end", emitter.color_curve_end, allocator);
            emitter_obj.AddMember("use_speed_curve", emitter.use_speed_curve, allocator);
            emitter_obj.AddMember("speed_curve_end_scale", emitter.speed_curve_end_scale, allocator);
            auto write_curve = [&](const char* name, const ParticleCurve& curve) {
                rapidjson::Value curve_obj(rapidjson::kObjectType);
                curve_obj.AddMember("enabled", curve.enabled, allocator);
                curve_obj.AddMember("type", static_cast<int>(curve.type), allocator);
                curve_obj.AddMember("start_value", curve.start_value, allocator);
                curve_obj.AddMember("end_value", curve.end_value, allocator);
                emitter_obj.AddMember(rapidjson::Value(name, allocator).Move(), curve_obj, allocator);
            };
            write_curve("size_curve", emitter.size_curve);
            write_curve("alpha_curve", emitter.alpha_curve);
            write_curve("speed_curve", emitter.speed_curve);
            WriteVec3(emitter_obj, "gravity", emitter.gravity, allocator);
            emitter_obj.AddMember("enable_collision", emitter.enable_collision, allocator);
            emitter_obj.AddMember("collision_mode", static_cast<int>(emitter.collision_mode), allocator);
            emitter_obj.AddMember("collision_bounce", emitter.collision_bounce, allocator);
            emitter_obj.AddMember("collision_friction", emitter.collision_friction, allocator);
            emitter_obj.AddMember("collision_life_loss", emitter.collision_life_loss, allocator);
            emitter_obj.AddMember("ground_y", emitter.ground_y, allocator);
            emitter_obj.AddMember("use_ground_collision", emitter.use_ground_collision, allocator);
            ent_obj.AddMember("particle_emitter", emitter_obj, allocator);
        }

        if (registry.all_of<UIAnchorComponent>(entity)) {
            auto& anchor = registry.get<UIAnchorComponent>(entity);
            rapidjson::Value anchor_obj(rapidjson::kObjectType);
            anchor_obj.AddMember("anchor", static_cast<int>(anchor.anchor), allocator);
            WriteVec2(anchor_obj, "offset", anchor.offset, allocator);
            ent_obj.AddMember("ui_anchor", anchor_obj, allocator);
        }

        if (registry.all_of<UIGridLayoutComponent>(entity)) {
            auto& grid = registry.get<UIGridLayoutComponent>(entity);
            rapidjson::Value grid_obj(rapidjson::kObjectType);
            grid_obj.AddMember("columns", grid.columns, allocator);
            grid_obj.AddMember("rows", grid.rows, allocator);
            WriteVec2(grid_obj, "cell_size", grid.cell_size, allocator);
            WriteVec2(grid_obj, "spacing", grid.spacing, allocator);
            grid_obj.AddMember("alignment", grid.alignment, allocator);
            ent_obj.AddMember("ui_grid_layout", grid_obj, allocator);
        }

        if (registry.all_of<UICanvasScalerComponent>(entity)) {
            auto& scaler = registry.get<UICanvasScalerComponent>(entity);
            rapidjson::Value scaler_obj(rapidjson::kObjectType);
            WriteVec2(scaler_obj, "reference_resolution", scaler.reference_resolution, allocator);
            scaler_obj.AddMember("scale_factor", scaler.scale_factor, allocator);
            scaler_obj.AddMember("match_width_or_height", scaler.match_width_or_height, allocator);
            ent_obj.AddMember("ui_canvas_scaler", scaler_obj, allocator);
        }

        if (registry.all_of<UIAnimationComponent>(entity)) {
            auto& anim = registry.get<UIAnimationComponent>(entity);
            rapidjson::Value anim_obj(rapidjson::kObjectType);
            WriteVec2(anim_obj, "target_position", anim.target_position, allocator);
            WriteVec2(anim_obj, "target_scale", anim.target_scale, allocator);
            anim_obj.AddMember("target_alpha", anim.target_alpha, allocator);
            WriteVec4(anim_obj, "target_color", anim.target_color, allocator);
            anim_obj.AddMember("animate_position", anim.animate_position, allocator);
            anim_obj.AddMember("animate_scale", anim.animate_scale, allocator);
            anim_obj.AddMember("animate_alpha", anim.animate_alpha, allocator);
            anim_obj.AddMember("animate_color", anim.animate_color, allocator);
            anim_obj.AddMember("duration", anim.duration, allocator);
            anim_obj.AddMember("elapsed", anim.elapsed, allocator);
            anim_obj.AddMember("delay", anim.delay, allocator);
            anim_obj.AddMember("delay_remaining", anim.delay_remaining, allocator);
            anim_obj.AddMember("loop", anim.loop, allocator);
            anim_obj.AddMember("ping_pong", anim.ping_pong, allocator);
            anim_obj.AddMember("playing", anim.playing, allocator);
            anim_obj.AddMember("reverse", anim.reverse, allocator);
            anim_obj.AddMember("easing", anim.easing, allocator);
            WriteVec2(anim_obj, "start_position", anim.start_position, allocator);
            WriteVec2(anim_obj, "start_scale", anim.start_scale, allocator);
            anim_obj.AddMember("start_alpha", anim.start_alpha, allocator);
            WriteVec4(anim_obj, "start_color", anim.start_color, allocator);
            ent_obj.AddMember("ui_animation", anim_obj, allocator);
        }

        if (registry.all_of<dse::Camera3DComponent>(entity)) {
            auto& cam = registry.get<dse::Camera3DComponent>(entity);
            rapidjson::Value cam_obj(rapidjson::kObjectType);
            cam_obj.AddMember("enabled", cam.enabled, allocator);
            cam_obj.AddMember("priority", cam.priority, allocator);
            cam_obj.AddMember("fov", cam.fov, allocator);
            cam_obj.AddMember("near_clip", cam.near_clip, allocator);
            cam_obj.AddMember("far_clip", cam.far_clip, allocator);
            ent_obj.AddMember("camera3d", cam_obj, allocator);
        }

        if (registry.all_of<dse::DirectionalLight3DComponent>(entity)) {
            auto& light = registry.get<dse::DirectionalLight3DComponent>(entity);
            rapidjson::Value light_obj(rapidjson::kObjectType);
            light_obj.AddMember("enabled", light.enabled, allocator);
            WriteVec3(light_obj, "color", light.color, allocator);
            light_obj.AddMember("intensity", light.intensity, allocator);
            light_obj.AddMember("cast_shadow", light.cast_shadow, allocator);
            light_obj.AddMember("shadow_strength", light.shadow_strength, allocator);
            ent_obj.AddMember("directional_light3d", light_obj, allocator);
        }

        if (registry.all_of<dse::MeshRendererComponent>(entity)) {
            auto& mesh = registry.get<dse::MeshRendererComponent>(entity);
            rapidjson::Value mesh_obj(rapidjson::kObjectType);
            mesh_obj.AddMember("visible", mesh.visible, allocator);
            mesh_obj.AddMember("receive_shadow", mesh.receive_shadow, allocator);
            mesh_obj.AddMember("mesh_path", rapidjson::Value(mesh.mesh_path.c_str(), allocator).Move(), allocator);
            mesh_obj.AddMember("material_instance_id", mesh.material_instance_id, allocator);
            mesh_obj.AddMember("shader_variant", rapidjson::Value(mesh.shader_variant.c_str(), allocator).Move(), allocator);
            mesh_obj.AddMember("material_data_source", static_cast<int>(mesh.material_data_source), allocator);
            WriteVec4(mesh_obj, "color", mesh.color, allocator);
            WriteVec3(mesh_obj, "emissive", mesh.emissive, allocator);
            mesh_obj.AddMember("metallic", mesh.metallic, allocator);
            mesh_obj.AddMember("roughness", mesh.roughness, allocator);
            mesh_obj.AddMember("ao", mesh.ao, allocator);
            mesh_obj.AddMember("normal_strength", mesh.normal_strength, allocator);
            mesh_obj.AddMember("material_alpha_cutoff", mesh.material_alpha_cutoff, allocator);
            mesh_obj.AddMember("material_alpha_test", mesh.material_alpha_test, allocator);
            mesh_obj.AddMember("material_double_sided", mesh.material_double_sided, allocator);
            ent_obj.AddMember("mesh_renderer", mesh_obj, allocator);
        }

        if (registry.all_of<dse::PointLightComponent>(entity)) {
            auto& light = registry.get<dse::PointLightComponent>(entity);
            rapidjson::Value light_obj(rapidjson::kObjectType);
            light_obj.AddMember("enabled", light.enabled, allocator);
            WriteVec3(light_obj, "color", light.color, allocator);
            light_obj.AddMember("intensity", light.intensity, allocator);
            light_obj.AddMember("radius", light.radius, allocator);
            light_obj.AddMember("falloff", light.falloff, allocator);
            light_obj.AddMember("cast_shadow", light.cast_shadow, allocator);
            ent_obj.AddMember("point_light", light_obj, allocator);
        }

        if (registry.all_of<dse::SpotLightComponent>(entity)) {
            auto& light = registry.get<dse::SpotLightComponent>(entity);
            rapidjson::Value light_obj(rapidjson::kObjectType);
            light_obj.AddMember("enabled", light.enabled, allocator);
            WriteVec3(light_obj, "color", light.color, allocator);
            WriteVec3(light_obj, "direction", light.direction, allocator);
            light_obj.AddMember("intensity", light.intensity, allocator);
            light_obj.AddMember("radius", light.radius, allocator);
            light_obj.AddMember("falloff", light.falloff, allocator);
            light_obj.AddMember("inner_cone_angle", light.inner_cone_angle, allocator);
            light_obj.AddMember("outer_cone_angle", light.outer_cone_angle, allocator);
            light_obj.AddMember("cast_shadow", light.cast_shadow, allocator);
            ent_obj.AddMember("spot_light", light_obj, allocator);
        }

        if (registry.all_of<dse::SkyLightComponent>(entity)) {
            auto& light = registry.get<dse::SkyLightComponent>(entity);
            rapidjson::Value light_obj(rapidjson::kObjectType);
            light_obj.AddMember("enabled", light.enabled, allocator);
            WriteVec3(light_obj, "up_color", light.up_color, allocator);
            WriteVec3(light_obj, "down_color", light.down_color, allocator);
            light_obj.AddMember("intensity", light.intensity, allocator);
            ent_obj.AddMember("sky_light", light_obj, allocator);
        }

        if (registry.all_of<dse::SkyboxComponent>(entity)) {
            auto& skybox = registry.get<dse::SkyboxComponent>(entity);
            rapidjson::Value skybox_obj(rapidjson::kObjectType);
            skybox_obj.AddMember("enabled", skybox.enabled, allocator);
            skybox_obj.AddMember("cubemap_handle", skybox.cubemap_handle, allocator);
            skybox_obj.AddMember("cubemap_path", rapidjson::Value(skybox.cubemap_path.c_str(), allocator).Move(), allocator);
            ent_obj.AddMember("skybox", skybox_obj, allocator);
        }

        if (registry.all_of<dse::Animator3DComponent>(entity)) {
            auto& animator = registry.get<dse::Animator3DComponent>(entity);
            rapidjson::Value animator_obj(rapidjson::kObjectType);
            animator_obj.AddMember("enabled", animator.enabled, allocator);
            animator_obj.AddMember("dskel_path", rapidjson::Value(animator.dskel_path.c_str(), allocator).Move(), allocator);
            animator_obj.AddMember("danim_path", rapidjson::Value(animator.danim_path.c_str(), allocator).Move(), allocator);
            animator_obj.AddMember("speed", animator.speed, allocator);
            animator_obj.AddMember("loop", animator.loop, allocator);
            animator_obj.AddMember("use_anim_tree", animator.use_anim_tree, allocator);
            animator_obj.AddMember("blend_parameter", rapidjson::Value(animator.blend_parameter.c_str(), allocator).Move(), allocator);
            animator_obj.AddMember("blend_parameter_value", animator.blend_parameter_value, allocator);
            rapidjson::Value blend_nodes(rapidjson::kArrayType);
            for (const auto& node : animator.blend_nodes) {
                rapidjson::Value node_obj(rapidjson::kObjectType);
                node_obj.AddMember("name", rapidjson::Value(node.name.c_str(), allocator).Move(), allocator);
                node_obj.AddMember("danim_path", rapidjson::Value(node.danim_path.c_str(), allocator).Move(), allocator);
                node_obj.AddMember("current_time", node.current_time, allocator);
                node_obj.AddMember("speed", node.speed, allocator);
                node_obj.AddMember("loop", node.loop, allocator);
                node_obj.AddMember("weight", node.weight, allocator);
                node_obj.AddMember("threshold", node.threshold, allocator);
                blend_nodes.PushBack(node_obj, allocator);
            }
            animator_obj.AddMember("blend_nodes", blend_nodes, allocator);
            ent_obj.AddMember("animator3d", animator_obj, allocator);
        }

        if (registry.all_of<dse::TerrainComponent>(entity)) {
            auto& terrain = registry.get<dse::TerrainComponent>(entity);
            rapidjson::Value terrain_obj(rapidjson::kObjectType);
            terrain_obj.AddMember("enabled", terrain.enabled, allocator);
            terrain_obj.AddMember("heightmap_path", rapidjson::Value(terrain.heightmap_path.c_str(), allocator).Move(), allocator);
            terrain_obj.AddMember("texture_handle", terrain.texture_handle, allocator);
            terrain_obj.AddMember("width", terrain.width, allocator);
            terrain_obj.AddMember("depth", terrain.depth, allocator);
            terrain_obj.AddMember("max_height", terrain.max_height, allocator);
            terrain_obj.AddMember("resolution_x", terrain.resolution_x, allocator);
            terrain_obj.AddMember("resolution_z", terrain.resolution_z, allocator);
            terrain_obj.AddMember("use_dynamic_lod", terrain.use_dynamic_lod, allocator);
            terrain_obj.AddMember("max_lod_levels", terrain.max_lod_levels, allocator);
            terrain_obj.AddMember("lod_distance_factor", terrain.lod_distance_factor, allocator);
            terrain_obj.AddMember("visible", terrain.visible, allocator);
            ent_obj.AddMember("terrain", terrain_obj, allocator);
        }

        if (registry.all_of<dse::RigidBody3DComponent>(entity)) {
            auto& rb = registry.get<dse::RigidBody3DComponent>(entity);
            rapidjson::Value rb_obj(rapidjson::kObjectType);
            rb_obj.AddMember("type", static_cast<int>(rb.type), allocator);
            rb_obj.AddMember("mass", rb.mass, allocator);
            rb_obj.AddMember("drag", rb.drag, allocator);
            rb_obj.AddMember("angular_drag", rb.angular_drag, allocator);
            rb_obj.AddMember("gravity_scale", rb.gravity_scale, allocator);
            rb_obj.AddMember("use_gravity", rb.use_gravity, allocator);
            rb_obj.AddMember("is_kinematic", rb.is_kinematic, allocator);
            ent_obj.AddMember("rigidbody3d", rb_obj, allocator);
        }

        doc.PushBack(ent_obj, allocator);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream ofs(filepath);
    if (ofs.is_open()) {
        ofs << buffer.GetString();
    }
}

void LoadScene(entt::registry& registry, const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    rapidjson::Document doc;
    doc.Parse(content.c_str());

    if (!doc.IsArray()) return;

    registry.clear();
    for (auto& v : doc.GetArray()) {
        auto entity = registry.create();

        if (v.HasMember("name") && v["name"].IsString()) {
            registry.emplace<EditorNameComponent>(entity, v["name"].GetString());
        }

        if (v.HasMember("sibling_index") && v["sibling_index"].IsObject()) {
            const auto& si_obj = v["sibling_index"];
            auto& si = registry.emplace<SiblingIndexComponent>(entity);
            if (si_obj.HasMember("index") && si_obj["index"].IsInt()) si.index = si_obj["index"].GetInt();
        }

        if (v.HasMember("transform") && v["transform"].IsObject()) {
            auto& t_obj = v["transform"];
            auto& t = registry.emplace<TransformComponent>(entity);
            ReadVec3(t_obj, "position", t.position);
            if (t_obj.HasMember("rotation") && t_obj["rotation"].IsArray()) {
                auto rot = t_obj["rotation"].GetArray();
                if (rot.Size() >= 4) t.rotation = glm::quat(rot[3].GetFloat(), rot[0].GetFloat(), rot[1].GetFloat(), rot[2].GetFloat());
            }
            ReadVec3(t_obj, "scale", t.scale);
            if (t_obj.HasMember("dirty") && t_obj["dirty"].IsBool()) t.dirty = t_obj["dirty"].GetBool();
        }

        if (v.HasMember("sprite") && v["sprite"].IsObject()) {
            auto& s_obj = v["sprite"];
            auto& s = registry.emplace<SpriteRendererComponent>(entity);
            if (s_obj.HasMember("path") && s_obj["path"].IsString()) {
                s.shader_variant = s_obj["path"].GetString();
            }
            ReadVec4(s_obj, "color", s.color);
            ReadVec4(s_obj, "uv", s.uv);
            ReadVec2(s_obj, "uv_offset", s.uv_offset);
            ReadVec2(s_obj, "uv_scroll_speed", s.uv_scroll_speed);
            if (s_obj.HasMember("sorting_layer") && s_obj["sorting_layer"].IsInt()) s.sorting_layer = s_obj["sorting_layer"].GetInt();
            if (s_obj.HasMember("order_in_layer") && s_obj["order_in_layer"].IsInt()) s.order_in_layer = s_obj["order_in_layer"].GetInt();
            if (s_obj.HasMember("visible") && s_obj["visible"].IsBool()) s.visible = s_obj["visible"].GetBool();
        }

        if (v.HasMember("ui_renderer") && v["ui_renderer"].IsObject()) {
            auto& ui_obj = v["ui_renderer"];
            auto& ui = registry.emplace<UIRendererComponent>(entity);
            if (ui_obj.HasMember("texture_handle") && ui_obj["texture_handle"].IsUint()) ui.texture_handle = ui_obj["texture_handle"].GetUint();
            ReadVec4(ui_obj, "color", ui.color);
            ReadVec4(ui_obj, "uv", ui.uv);
            if (ui_obj.HasMember("order") && ui_obj["order"].IsInt()) ui.order = ui_obj["order"].GetInt();
            if (ui_obj.HasMember("visible") && ui_obj["visible"].IsBool()) ui.visible = ui_obj["visible"].GetBool();
            if (ui_obj.HasMember("scale") && ui_obj["scale"].IsNumber()) ui.scale = ui_obj["scale"].GetFloat();
            if (ui_obj.HasMember("hover_scale") && ui_obj["hover_scale"].IsNumber()) ui.hover_scale = ui_obj["hover_scale"].GetFloat();
            if (ui_obj.HasMember("pressed_scale") && ui_obj["pressed_scale"].IsNumber()) ui.pressed_scale = ui_obj["pressed_scale"].GetFloat();
            if (ui_obj.HasMember("scale_lerp_speed") && ui_obj["scale_lerp_speed"].IsNumber()) ui.scale_lerp_speed = ui_obj["scale_lerp_speed"].GetFloat();
            ReadVec2(ui_obj, "position", ui.position);
            ReadVec2(ui_obj, "size", ui.size);
            ReadVec2(ui_obj, "anchor_min", ui.anchor_min);
            ReadVec2(ui_obj, "anchor_max", ui.anchor_max);
            ReadVec2(ui_obj, "pivot", ui.pivot);
            if (ui_obj.HasMember("interactable") && ui_obj["interactable"].IsBool()) ui.interactable = ui_obj["interactable"].GetBool();
        }

        if (v.HasMember("ui_label") && v["ui_label"].IsObject()) {
            auto& label_obj = v["ui_label"];
            auto& label = registry.emplace<UILabelComponent>(entity);
            if (label_obj.HasMember("text") && label_obj["text"].IsString()) label.text = label_obj["text"].GetString();
            if (label_obj.HasMember("use_localization") && label_obj["use_localization"].IsBool()) label.use_localization = label_obj["use_localization"].GetBool();
            if (label_obj.HasMember("localization_key") && label_obj["localization_key"].IsString()) label.localization_key = label_obj["localization_key"].GetString();
            if (label_obj.HasMember("fallback_text") && label_obj["fallback_text"].IsString()) label.fallback_text = label_obj["fallback_text"].GetString();
            if (label_obj.HasMember("localization_params") && label_obj["localization_params"].IsObject()) {
                for (auto it = label_obj["localization_params"].MemberBegin(); it != label_obj["localization_params"].MemberEnd(); ++it) {
                    if (it->name.IsString() && it->value.IsString()) {
                        label.localization_params[it->name.GetString()] = it->value.GetString();
                    }
                }
            }
            if (label_obj.HasMember("number_value") && label_obj["number_value"].IsInt64()) label.number_value = label_obj["number_value"].GetInt64();
            if (label_obj.HasMember("numeric_mode") && label_obj["numeric_mode"].IsBool()) label.numeric_mode = label_obj["numeric_mode"].GetBool();
            if (label_obj.HasMember("font_texture_handle") && label_obj["font_texture_handle"].IsUint()) label.font_texture_handle = label_obj["font_texture_handle"].GetUint();
            ReadVec2(label_obj, "glyph_size", label.glyph_size);
            ReadVec2(label_obj, "offset", label.offset);
            if (label_obj.HasMember("spacing") && label_obj["spacing"].IsNumber()) label.spacing = label_obj["spacing"].GetFloat();
            if (label_obj.HasMember("atlas_cols") && label_obj["atlas_cols"].IsInt()) label.atlas_cols = label_obj["atlas_cols"].GetInt();
            if (label_obj.HasMember("atlas_rows") && label_obj["atlas_rows"].IsInt()) label.atlas_rows = label_obj["atlas_rows"].GetInt();
            if (label_obj.HasMember("ascii_start") && label_obj["ascii_start"].IsInt()) label.ascii_start = label_obj["ascii_start"].GetInt();
            ReadVec4(label_obj, "color", label.color);
            label.runtime_glyph_entities.clear();
            label.dirty = true;
        }

        if (v.HasMember("rigidbody2d") && v["rigidbody2d"].IsObject()) {
            auto& rb_obj = v["rigidbody2d"];
            auto& rb = registry.emplace<RigidBody2DComponent>(entity);
            if (rb_obj.HasMember("type") && rb_obj["type"].IsInt()) rb.type = static_cast<RigidBody2DType>(rb_obj["type"].GetInt());
            ReadVec2(rb_obj, "velocity", rb.velocity);
            if (rb_obj.HasMember("gravity_scale") && rb_obj["gravity_scale"].IsNumber()) rb.gravity_scale = rb_obj["gravity_scale"].GetFloat();
            if (rb_obj.HasMember("fixed_rotation") && rb_obj["fixed_rotation"].IsBool()) rb.fixed_rotation = rb_obj["fixed_rotation"].GetBool();
            rb.runtime_body = nullptr;
        }

        if (v.HasMember("particle_emitter") && v["particle_emitter"].IsObject()) {
            auto& emitter_obj = v["particle_emitter"];
            auto& emitter = registry.emplace<ParticleEmitterComponent>(entity);
            if (emitter_obj.HasMember("texture_handle") && emitter_obj["texture_handle"].IsUint()) emitter.texture_handle = emitter_obj["texture_handle"].GetUint();
            if (emitter_obj.HasMember("max_particles") && emitter_obj["max_particles"].IsInt()) emitter.max_particles = emitter_obj["max_particles"].GetInt();
            if (emitter_obj.HasMember("emit_rate") && emitter_obj["emit_rate"].IsNumber()) emitter.emit_rate = emitter_obj["emit_rate"].GetFloat();
            if (emitter_obj.HasMember("emit_rate_scale") && emitter_obj["emit_rate_scale"].IsNumber()) emitter.emit_rate_scale = emitter_obj["emit_rate_scale"].GetFloat();
            if (emitter_obj.HasMember("emitting") && emitter_obj["emitting"].IsBool()) emitter.emitting = emitter_obj["emitting"].GetBool();
            if (emitter_obj.HasMember("start_life_time") && emitter_obj["start_life_time"].IsNumber()) emitter.start_life_time = emitter_obj["start_life_time"].GetFloat();
            if (emitter_obj.HasMember("start_size") && emitter_obj["start_size"].IsNumber()) emitter.start_size = emitter_obj["start_size"].GetFloat();
            ReadVec4(emitter_obj, "start_color", emitter.start_color);
            ReadVec3(emitter_obj, "velocity_min", emitter.velocity_min);
            ReadVec3(emitter_obj, "velocity_max", emitter.velocity_max);
            if (emitter_obj.HasMember("life_time_min") && emitter_obj["life_time_min"].IsNumber()) emitter.life_time_min = emitter_obj["life_time_min"].GetFloat();
            if (emitter_obj.HasMember("life_time_max") && emitter_obj["life_time_max"].IsNumber()) emitter.life_time_max = emitter_obj["life_time_max"].GetFloat();
            if (emitter_obj.HasMember("size_min") && emitter_obj["size_min"].IsNumber()) emitter.size_min = emitter_obj["size_min"].GetFloat();
            if (emitter_obj.HasMember("size_max") && emitter_obj["size_max"].IsNumber()) emitter.size_max = emitter_obj["size_max"].GetFloat();
            if (emitter_obj.HasMember("rotation_min") && emitter_obj["rotation_min"].IsNumber()) emitter.rotation_min = emitter_obj["rotation_min"].GetFloat();
            if (emitter_obj.HasMember("rotation_max") && emitter_obj["rotation_max"].IsNumber()) emitter.rotation_max = emitter_obj["rotation_max"].GetFloat();
            if (emitter_obj.HasMember("angular_velocity_min") && emitter_obj["angular_velocity_min"].IsNumber()) emitter.angular_velocity_min = emitter_obj["angular_velocity_min"].GetFloat();
            if (emitter_obj.HasMember("angular_velocity_max") && emitter_obj["angular_velocity_max"].IsNumber()) emitter.angular_velocity_max = emitter_obj["angular_velocity_max"].GetFloat();
            if (emitter_obj.HasMember("use_random_params") && emitter_obj["use_random_params"].IsBool()) emitter.use_random_params = emitter_obj["use_random_params"].GetBool();
            if (emitter_obj.HasMember("use_size_curve") && emitter_obj["use_size_curve"].IsBool()) emitter.use_size_curve = emitter_obj["use_size_curve"].GetBool();
            if (emitter_obj.HasMember("size_curve_end") && emitter_obj["size_curve_end"].IsNumber()) emitter.size_curve_end = emitter_obj["size_curve_end"].GetFloat();
            if (emitter_obj.HasMember("use_alpha_curve") && emitter_obj["use_alpha_curve"].IsBool()) emitter.use_alpha_curve = emitter_obj["use_alpha_curve"].GetBool();
            if (emitter_obj.HasMember("alpha_curve_end") && emitter_obj["alpha_curve_end"].IsNumber()) emitter.alpha_curve_end = emitter_obj["alpha_curve_end"].GetFloat();
            if (emitter_obj.HasMember("use_color_curve") && emitter_obj["use_color_curve"].IsBool()) emitter.use_color_curve = emitter_obj["use_color_curve"].GetBool();
            ReadVec4(emitter_obj, "color_curve_end", emitter.color_curve_end);
            if (emitter_obj.HasMember("use_speed_curve") && emitter_obj["use_speed_curve"].IsBool()) emitter.use_speed_curve = emitter_obj["use_speed_curve"].GetBool();
            if (emitter_obj.HasMember("speed_curve_end_scale") && emitter_obj["speed_curve_end_scale"].IsNumber()) emitter.speed_curve_end_scale = emitter_obj["speed_curve_end_scale"].GetFloat();
            auto read_curve = [&](const char* name, ParticleCurve& curve) {
                if (emitter_obj.HasMember(name) && emitter_obj[name].IsObject()) {
                    const auto& curve_obj = emitter_obj[name];
                    if (curve_obj.HasMember("enabled") && curve_obj["enabled"].IsBool()) curve.enabled = curve_obj["enabled"].GetBool();
                    if (curve_obj.HasMember("type") && curve_obj["type"].IsInt()) curve.type = static_cast<ParticleCurveType>(curve_obj["type"].GetInt());
                    if (curve_obj.HasMember("start_value") && curve_obj["start_value"].IsNumber()) curve.start_value = curve_obj["start_value"].GetFloat();
                    if (curve_obj.HasMember("end_value") && curve_obj["end_value"].IsNumber()) curve.end_value = curve_obj["end_value"].GetFloat();
                }
            };
            read_curve("size_curve", emitter.size_curve);
            read_curve("alpha_curve", emitter.alpha_curve);
            read_curve("speed_curve", emitter.speed_curve);
            ReadVec3(emitter_obj, "gravity", emitter.gravity);
            if (emitter_obj.HasMember("enable_collision") && emitter_obj["enable_collision"].IsBool()) emitter.enable_collision = emitter_obj["enable_collision"].GetBool();
            if (emitter_obj.HasMember("collision_mode") && emitter_obj["collision_mode"].IsInt()) emitter.collision_mode = static_cast<ParticleCollisionMode>(emitter_obj["collision_mode"].GetInt());
            if (emitter_obj.HasMember("collision_bounce") && emitter_obj["collision_bounce"].IsNumber()) emitter.collision_bounce = emitter_obj["collision_bounce"].GetFloat();
            if (emitter_obj.HasMember("collision_friction") && emitter_obj["collision_friction"].IsNumber()) emitter.collision_friction = emitter_obj["collision_friction"].GetFloat();
            if (emitter_obj.HasMember("collision_life_loss") && emitter_obj["collision_life_loss"].IsNumber()) emitter.collision_life_loss = emitter_obj["collision_life_loss"].GetFloat();
            if (emitter_obj.HasMember("ground_y") && emitter_obj["ground_y"].IsNumber()) emitter.ground_y = emitter_obj["ground_y"].GetFloat();
            if (emitter_obj.HasMember("use_ground_collision") && emitter_obj["use_ground_collision"].IsBool()) emitter.use_ground_collision = emitter_obj["use_ground_collision"].GetBool();
            emitter.particles.clear();
            emitter.emit_accumulator = 0.0f;
            emitter.pending_burst = 0;
        }

        if (v.HasMember("ui_anchor") && v["ui_anchor"].IsObject()) {
            auto& anchor_obj = v["ui_anchor"];
            auto& anchor = registry.emplace<UIAnchorComponent>(entity);
            if (anchor_obj.HasMember("anchor") && anchor_obj["anchor"].IsInt()) anchor.anchor = anchor_obj["anchor"].GetInt();
            ReadVec2(anchor_obj, "offset", anchor.offset);
        }

        if (v.HasMember("ui_grid_layout") && v["ui_grid_layout"].IsObject()) {
            auto& grid_obj = v["ui_grid_layout"];
            auto& grid = registry.emplace<UIGridLayoutComponent>(entity);
            if (grid_obj.HasMember("columns") && grid_obj["columns"].IsInt()) grid.columns = grid_obj["columns"].GetInt();
            if (grid_obj.HasMember("rows") && grid_obj["rows"].IsInt()) grid.rows = grid_obj["rows"].GetInt();
            ReadVec2(grid_obj, "cell_size", grid.cell_size);
            ReadVec2(grid_obj, "spacing", grid.spacing);
            if (grid_obj.HasMember("alignment") && grid_obj["alignment"].IsInt()) grid.alignment = grid_obj["alignment"].GetInt();
        }

        if (v.HasMember("ui_canvas_scaler") && v["ui_canvas_scaler"].IsObject()) {
            auto& scaler_obj = v["ui_canvas_scaler"];
            auto& scaler = registry.emplace<UICanvasScalerComponent>(entity);
            ReadVec2(scaler_obj, "reference_resolution", scaler.reference_resolution);
            if (scaler_obj.HasMember("scale_factor") && scaler_obj["scale_factor"].IsNumber()) scaler.scale_factor = scaler_obj["scale_factor"].GetFloat();
            if (scaler_obj.HasMember("match_width_or_height") && scaler_obj["match_width_or_height"].IsBool()) scaler.match_width_or_height = scaler_obj["match_width_or_height"].GetBool();
        }

        if (v.HasMember("ui_animation") && v["ui_animation"].IsObject()) {
            auto& anim_obj = v["ui_animation"];
            auto& anim = registry.emplace<UIAnimationComponent>(entity);
            ReadVec2(anim_obj, "target_position", anim.target_position);
            ReadVec2(anim_obj, "target_scale", anim.target_scale);
            if (anim_obj.HasMember("target_alpha") && anim_obj["target_alpha"].IsNumber()) anim.target_alpha = anim_obj["target_alpha"].GetFloat();
            ReadVec4(anim_obj, "target_color", anim.target_color);
            if (anim_obj.HasMember("animate_position") && anim_obj["animate_position"].IsBool()) anim.animate_position = anim_obj["animate_position"].GetBool();
            if (anim_obj.HasMember("animate_scale") && anim_obj["animate_scale"].IsBool()) anim.animate_scale = anim_obj["animate_scale"].GetBool();
            if (anim_obj.HasMember("animate_alpha") && anim_obj["animate_alpha"].IsBool()) anim.animate_alpha = anim_obj["animate_alpha"].GetBool();
            if (anim_obj.HasMember("animate_color") && anim_obj["animate_color"].IsBool()) anim.animate_color = anim_obj["animate_color"].GetBool();
            if (anim_obj.HasMember("duration") && anim_obj["duration"].IsNumber()) anim.duration = anim_obj["duration"].GetFloat();
            if (anim_obj.HasMember("elapsed") && anim_obj["elapsed"].IsNumber()) anim.elapsed = anim_obj["elapsed"].GetFloat();
            if (anim_obj.HasMember("delay") && anim_obj["delay"].IsNumber()) anim.delay = anim_obj["delay"].GetFloat();
            if (anim_obj.HasMember("delay_remaining") && anim_obj["delay_remaining"].IsNumber()) anim.delay_remaining = anim_obj["delay_remaining"].GetFloat();
            if (anim_obj.HasMember("loop") && anim_obj["loop"].IsBool()) anim.loop = anim_obj["loop"].GetBool();
            if (anim_obj.HasMember("ping_pong") && anim_obj["ping_pong"].IsBool()) anim.ping_pong = anim_obj["ping_pong"].GetBool();
            if (anim_obj.HasMember("playing") && anim_obj["playing"].IsBool()) anim.playing = anim_obj["playing"].GetBool();
            if (anim_obj.HasMember("reverse") && anim_obj["reverse"].IsBool()) anim.reverse = anim_obj["reverse"].GetBool();
            if (anim_obj.HasMember("easing") && anim_obj["easing"].IsInt()) anim.easing = anim_obj["easing"].GetInt();
            ReadVec2(anim_obj, "start_position", anim.start_position);
            ReadVec2(anim_obj, "start_scale", anim.start_scale);
            if (anim_obj.HasMember("start_alpha") && anim_obj["start_alpha"].IsNumber()) anim.start_alpha = anim_obj["start_alpha"].GetFloat();
            ReadVec4(anim_obj, "start_color", anim.start_color);
        }

        if (v.HasMember("camera3d") && v["camera3d"].IsObject()) {
            auto& cam_obj = v["camera3d"];
            auto& cam = registry.emplace<dse::Camera3DComponent>(entity);
            if (cam_obj.HasMember("enabled") && cam_obj["enabled"].IsBool()) cam.enabled = cam_obj["enabled"].GetBool();
            if (cam_obj.HasMember("priority") && cam_obj["priority"].IsInt()) cam.priority = cam_obj["priority"].GetInt();
            if (cam_obj.HasMember("fov") && cam_obj["fov"].IsNumber()) cam.fov = cam_obj["fov"].GetFloat();
            if (cam_obj.HasMember("near_clip") && cam_obj["near_clip"].IsNumber()) cam.near_clip = cam_obj["near_clip"].GetFloat();
            if (cam_obj.HasMember("far_clip") && cam_obj["far_clip"].IsNumber()) cam.far_clip = cam_obj["far_clip"].GetFloat();
        }

        if (v.HasMember("directional_light3d") && v["directional_light3d"].IsObject()) {
            auto& light_obj = v["directional_light3d"];
            auto& light = registry.emplace<dse::DirectionalLight3DComponent>(entity);
            if (light_obj.HasMember("enabled") && light_obj["enabled"].IsBool()) light.enabled = light_obj["enabled"].GetBool();
            ReadVec3(light_obj, "color", light.color);
            if (light_obj.HasMember("intensity") && light_obj["intensity"].IsNumber()) light.intensity = light_obj["intensity"].GetFloat();
            if (light_obj.HasMember("cast_shadow") && light_obj["cast_shadow"].IsBool()) light.cast_shadow = light_obj["cast_shadow"].GetBool();
            if (light_obj.HasMember("shadow_strength") && light_obj["shadow_strength"].IsNumber()) light.shadow_strength = light_obj["shadow_strength"].GetFloat();
        }

        if (v.HasMember("mesh_renderer") && v["mesh_renderer"].IsObject()) {
            auto& mesh_obj = v["mesh_renderer"];
            auto& mesh = registry.emplace<dse::MeshRendererComponent>(entity);
            if (mesh_obj.HasMember("visible") && mesh_obj["visible"].IsBool()) mesh.visible = mesh_obj["visible"].GetBool();
            if (mesh_obj.HasMember("receive_shadow") && mesh_obj["receive_shadow"].IsBool()) mesh.receive_shadow = mesh_obj["receive_shadow"].GetBool();
            if (mesh_obj.HasMember("mesh_path") && mesh_obj["mesh_path"].IsString()) mesh.mesh_path = mesh_obj["mesh_path"].GetString();
            if (mesh_obj.HasMember("material_instance_id") && mesh_obj["material_instance_id"].IsUint()) mesh.material_instance_id = mesh_obj["material_instance_id"].GetUint();
            if (mesh_obj.HasMember("shader_variant") && mesh_obj["shader_variant"].IsString()) mesh.shader_variant = mesh_obj["shader_variant"].GetString();
            if (mesh_obj.HasMember("material_data_source") && mesh_obj["material_data_source"].IsInt()) mesh.material_data_source = static_cast<dse::MeshRendererComponent::MaterialDataSource>(mesh_obj["material_data_source"].GetInt());
            ReadVec4(mesh_obj, "color", mesh.color);
            ReadVec3(mesh_obj, "emissive", mesh.emissive);
            if (mesh_obj.HasMember("metallic") && mesh_obj["metallic"].IsNumber()) mesh.metallic = mesh_obj["metallic"].GetFloat();
            if (mesh_obj.HasMember("roughness") && mesh_obj["roughness"].IsNumber()) mesh.roughness = mesh_obj["roughness"].GetFloat();
            if (mesh_obj.HasMember("ao") && mesh_obj["ao"].IsNumber()) mesh.ao = mesh_obj["ao"].GetFloat();
            if (mesh_obj.HasMember("normal_strength") && mesh_obj["normal_strength"].IsNumber()) mesh.normal_strength = mesh_obj["normal_strength"].GetFloat();
            if (mesh_obj.HasMember("material_alpha_cutoff") && mesh_obj["material_alpha_cutoff"].IsNumber()) mesh.material_alpha_cutoff = mesh_obj["material_alpha_cutoff"].GetFloat();
            if (mesh_obj.HasMember("material_alpha_test") && mesh_obj["material_alpha_test"].IsBool()) mesh.material_alpha_test = mesh_obj["material_alpha_test"].GetBool();
            if (mesh_obj.HasMember("material_double_sided") && mesh_obj["material_double_sided"].IsBool()) mesh.material_double_sided = mesh_obj["material_double_sided"].GetBool();
        }

        if (v.HasMember("point_light") && v["point_light"].IsObject()) {
            auto& light_obj = v["point_light"];
            auto& light = registry.emplace<dse::PointLightComponent>(entity);
            if (light_obj.HasMember("enabled") && light_obj["enabled"].IsBool()) light.enabled = light_obj["enabled"].GetBool();
            ReadVec3(light_obj, "color", light.color);
            if (light_obj.HasMember("intensity") && light_obj["intensity"].IsNumber()) light.intensity = light_obj["intensity"].GetFloat();
            if (light_obj.HasMember("radius") && light_obj["radius"].IsNumber()) light.radius = light_obj["radius"].GetFloat();
            if (light_obj.HasMember("falloff") && light_obj["falloff"].IsNumber()) light.falloff = light_obj["falloff"].GetFloat();
            if (light_obj.HasMember("cast_shadow") && light_obj["cast_shadow"].IsBool()) light.cast_shadow = light_obj["cast_shadow"].GetBool();
        }

        if (v.HasMember("spot_light") && v["spot_light"].IsObject()) {
            auto& light_obj = v["spot_light"];
            auto& light = registry.emplace<dse::SpotLightComponent>(entity);
            if (light_obj.HasMember("enabled") && light_obj["enabled"].IsBool()) light.enabled = light_obj["enabled"].GetBool();
            ReadVec3(light_obj, "color", light.color);
            ReadVec3(light_obj, "direction", light.direction);
            if (light_obj.HasMember("intensity") && light_obj["intensity"].IsNumber()) light.intensity = light_obj["intensity"].GetFloat();
            if (light_obj.HasMember("radius") && light_obj["radius"].IsNumber()) light.radius = light_obj["radius"].GetFloat();
            if (light_obj.HasMember("falloff") && light_obj["falloff"].IsNumber()) light.falloff = light_obj["falloff"].GetFloat();
            if (light_obj.HasMember("inner_cone_angle") && light_obj["inner_cone_angle"].IsNumber()) light.inner_cone_angle = light_obj["inner_cone_angle"].GetFloat();
            if (light_obj.HasMember("outer_cone_angle") && light_obj["outer_cone_angle"].IsNumber()) light.outer_cone_angle = light_obj["outer_cone_angle"].GetFloat();
            if (light_obj.HasMember("cast_shadow") && light_obj["cast_shadow"].IsBool()) light.cast_shadow = light_obj["cast_shadow"].GetBool();
        }

        if (v.HasMember("sky_light") && v["sky_light"].IsObject()) {
            auto& light_obj = v["sky_light"];
            auto& light = registry.emplace<dse::SkyLightComponent>(entity);
            if (light_obj.HasMember("enabled") && light_obj["enabled"].IsBool()) light.enabled = light_obj["enabled"].GetBool();
            ReadVec3(light_obj, "up_color", light.up_color);
            ReadVec3(light_obj, "down_color", light.down_color);
            if (light_obj.HasMember("intensity") && light_obj["intensity"].IsNumber()) light.intensity = light_obj["intensity"].GetFloat();
        }

        if (v.HasMember("skybox") && v["skybox"].IsObject()) {
            auto& skybox_obj = v["skybox"];
            auto& skybox = registry.emplace<dse::SkyboxComponent>(entity);
            if (skybox_obj.HasMember("enabled") && skybox_obj["enabled"].IsBool()) skybox.enabled = skybox_obj["enabled"].GetBool();
            if (skybox_obj.HasMember("cubemap_handle") && skybox_obj["cubemap_handle"].IsUint()) skybox.cubemap_handle = skybox_obj["cubemap_handle"].GetUint();
            if (skybox_obj.HasMember("cubemap_path") && skybox_obj["cubemap_path"].IsString()) skybox.cubemap_path = skybox_obj["cubemap_path"].GetString();
        }

        if (v.HasMember("animator3d") && v["animator3d"].IsObject()) {
            auto& animator_obj = v["animator3d"];
            auto& animator = registry.emplace<dse::Animator3DComponent>(entity);
            if (animator_obj.HasMember("enabled") && animator_obj["enabled"].IsBool()) animator.enabled = animator_obj["enabled"].GetBool();
            if (animator_obj.HasMember("dskel_path") && animator_obj["dskel_path"].IsString()) animator.dskel_path = animator_obj["dskel_path"].GetString();
            if (animator_obj.HasMember("danim_path") && animator_obj["danim_path"].IsString()) animator.danim_path = animator_obj["danim_path"].GetString();
            if (animator_obj.HasMember("speed") && animator_obj["speed"].IsNumber()) animator.speed = animator_obj["speed"].GetFloat();
            if (animator_obj.HasMember("loop") && animator_obj["loop"].IsBool()) animator.loop = animator_obj["loop"].GetBool();
            if (animator_obj.HasMember("use_anim_tree") && animator_obj["use_anim_tree"].IsBool()) animator.use_anim_tree = animator_obj["use_anim_tree"].GetBool();
            if (animator_obj.HasMember("blend_parameter") && animator_obj["blend_parameter"].IsString()) animator.blend_parameter = animator_obj["blend_parameter"].GetString();
            if (animator_obj.HasMember("blend_parameter_value") && animator_obj["blend_parameter_value"].IsNumber()) animator.blend_parameter_value = animator_obj["blend_parameter_value"].GetFloat();
            if (animator_obj.HasMember("blend_nodes") && animator_obj["blend_nodes"].IsArray()) {
                for (const auto& node_obj : animator_obj["blend_nodes"].GetArray()) {
                    if (!node_obj.IsObject()) {
                        continue;
                    }
                    dse::AnimBlendNode node;
                    if (node_obj.HasMember("name") && node_obj["name"].IsString()) node.name = node_obj["name"].GetString();
                    if (node_obj.HasMember("danim_path") && node_obj["danim_path"].IsString()) node.danim_path = node_obj["danim_path"].GetString();
                    if (node_obj.HasMember("current_time") && node_obj["current_time"].IsNumber()) node.current_time = node_obj["current_time"].GetFloat();
                    if (node_obj.HasMember("speed") && node_obj["speed"].IsNumber()) node.speed = node_obj["speed"].GetFloat();
                    if (node_obj.HasMember("loop") && node_obj["loop"].IsBool()) node.loop = node_obj["loop"].GetBool();
                    if (node_obj.HasMember("weight") && node_obj["weight"].IsNumber()) node.weight = node_obj["weight"].GetFloat();
                    if (node_obj.HasMember("threshold") && node_obj["threshold"].IsNumber()) node.threshold = node_obj["threshold"].GetFloat();
                    animator.blend_nodes.push_back(std::move(node));
                }
            }
            animator.state_machine.reset();
            animator.final_bone_matrices.clear();
        }

        if (v.HasMember("terrain") && v["terrain"].IsObject()) {
            auto& terrain_obj = v["terrain"];
            auto& terrain = registry.emplace<dse::TerrainComponent>(entity);
            if (terrain_obj.HasMember("enabled") && terrain_obj["enabled"].IsBool()) terrain.enabled = terrain_obj["enabled"].GetBool();
            if (terrain_obj.HasMember("heightmap_path") && terrain_obj["heightmap_path"].IsString()) terrain.heightmap_path = terrain_obj["heightmap_path"].GetString();
            if (terrain_obj.HasMember("texture_handle") && terrain_obj["texture_handle"].IsUint()) terrain.texture_handle = terrain_obj["texture_handle"].GetUint();
            if (terrain_obj.HasMember("width") && terrain_obj["width"].IsNumber()) terrain.width = terrain_obj["width"].GetFloat();
            if (terrain_obj.HasMember("depth") && terrain_obj["depth"].IsNumber()) terrain.depth = terrain_obj["depth"].GetFloat();
            if (terrain_obj.HasMember("max_height") && terrain_obj["max_height"].IsNumber()) terrain.max_height = terrain_obj["max_height"].GetFloat();
            if (terrain_obj.HasMember("resolution_x") && terrain_obj["resolution_x"].IsInt()) terrain.resolution_x = terrain_obj["resolution_x"].GetInt();
            if (terrain_obj.HasMember("resolution_z") && terrain_obj["resolution_z"].IsInt()) terrain.resolution_z = terrain_obj["resolution_z"].GetInt();
            if (terrain_obj.HasMember("use_dynamic_lod") && terrain_obj["use_dynamic_lod"].IsBool()) terrain.use_dynamic_lod = terrain_obj["use_dynamic_lod"].GetBool();
            if (terrain_obj.HasMember("max_lod_levels") && terrain_obj["max_lod_levels"].IsInt()) terrain.max_lod_levels = terrain_obj["max_lod_levels"].GetInt();
            if (terrain_obj.HasMember("lod_distance_factor") && terrain_obj["lod_distance_factor"].IsNumber()) terrain.lod_distance_factor = terrain_obj["lod_distance_factor"].GetFloat();
            if (terrain_obj.HasMember("visible") && terrain_obj["visible"].IsBool()) terrain.visible = terrain_obj["visible"].GetBool();
            terrain.is_dirty = true;
        }

        if (v.HasMember("rigidbody3d") && v["rigidbody3d"].IsObject()) {
            auto& rb_obj = v["rigidbody3d"];
            auto& rb = registry.emplace<dse::RigidBody3DComponent>(entity);
            if (rb_obj.HasMember("type") && rb_obj["type"].IsInt()) rb.type = static_cast<dse::RigidBody3DType>(rb_obj["type"].GetInt());
            if (rb_obj.HasMember("mass") && rb_obj["mass"].IsNumber()) rb.mass = rb_obj["mass"].GetFloat();
            if (rb_obj.HasMember("drag") && rb_obj["drag"].IsNumber()) rb.drag = rb_obj["drag"].GetFloat();
            if (rb_obj.HasMember("angular_drag") && rb_obj["angular_drag"].IsNumber()) rb.angular_drag = rb_obj["angular_drag"].GetFloat();
            if (rb_obj.HasMember("gravity_scale") && rb_obj["gravity_scale"].IsNumber()) rb.gravity_scale = rb_obj["gravity_scale"].GetFloat();
            if (rb_obj.HasMember("use_gravity") && rb_obj["use_gravity"].IsBool()) rb.use_gravity = rb_obj["use_gravity"].GetBool();
            if (rb_obj.HasMember("is_kinematic") && rb_obj["is_kinematic"].IsBool()) rb.is_kinematic = rb_obj["is_kinematic"].GetBool();
        }
    }
}

} // namespace dse::editor
