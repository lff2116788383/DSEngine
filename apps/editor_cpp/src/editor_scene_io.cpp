#include "editor_scene_io.h"

#include <fstream>
#include <iterator>
#include <utility>
#include <filesystem>
#include <cstdint>
#include <vector>

#include <glm/gtc/type_ptr.hpp>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/transform.h"

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

// ============================================================
// Scene binary cache  (.dscene.bin)
// Format: magic(4) + version(4) + entity_count(4) +
//   per entity: id(4) + comp_flags(8) + component data
// Strings: uint32 length + raw bytes (no null terminator)
// ============================================================

static constexpr uint32_t kSceneBinMagic   = 0x42435344u; // 'DSCB'
static constexpr uint32_t kSceneBinVersion = 4u;

enum CompFlags : uint64_t {
    CF_NAME             = 1ull << 0,
    CF_SIBLING_INDEX    = 1ull << 1,
    CF_TRANSFORM        = 1ull << 2,
    CF_SPRITE           = 1ull << 3,
    CF_UI_RENDERER      = 1ull << 4,
    CF_UI_LABEL         = 1ull << 5,
    CF_UI_ANCHOR        = 1ull << 6,
    CF_UI_GRID          = 1ull << 7,
    CF_UI_CANVAS_SCALER = 1ull << 8,
    CF_UI_ANIMATION     = 1ull << 9,
    CF_RIGIDBODY2D      = 1ull << 10,
    CF_PARTICLE_EMITTER = 1ull << 11,
    CF_CAMERA3D         = 1ull << 12,
    CF_DIR_LIGHT3D      = 1ull << 13,
    CF_MESH_RENDERER    = 1ull << 14,
    CF_POINT_LIGHT      = 1ull << 15,
    CF_SPOT_LIGHT       = 1ull << 16,
    CF_SKY_LIGHT        = 1ull << 17,
    CF_SKYBOX           = 1ull << 18,
    CF_ANIMATOR3D       = 1ull << 19,
    CF_TERRAIN          = 1ull << 20,
    CF_RIGIDBODY3D      = 1ull << 21,
    CF_BOX_COLLIDER3D   = 1ull << 22,
    CF_SPHERE_COLLIDER3D= 1ull << 23,
    CF_CAPSULE_COLLIDER3D=1ull << 24,
    CF_MESH_COLLIDER3D  = 1ull << 25,
    CF_AUDIO_SOURCE     = 1ull << 26,
    CF_AUDIO_LISTENER   = 1ull << 27,
};

template<typename T>
static void WPod(std::ofstream& f, const T& v) {
    f.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
template<typename T>
static bool RPod(std::ifstream& f, T& v) {
    return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), sizeof(v)));
}
static void WStr(std::ofstream& f, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    WPod(f, len);
    if (len > 0) f.write(s.data(), len);
}
static bool RStr(std::ifstream& f, std::string& s) {
    uint32_t len = 0;
    if (!RPod(f, len)) return false;
    if (len > 4u * 1024u * 1024u) return false;
    s.resize(len);
    if (len > 0 && !f.read(s.data(), len)) return false;
    return true;
}

static bool SceneBinaryIsValid(const std::string& json_path,
                               const std::string& bin_path) {
    std::error_code ec;
    auto jt = std::filesystem::last_write_time(json_path, ec);
    if (ec) return false;
    auto bt = std::filesystem::last_write_time(bin_path, ec);
    if (ec) return false;
    return bt >= jt;
}

static void SaveSceneBinary(entt::registry& registry,
                            const std::string& bin_path) {
    std::ofstream f(bin_path, std::ios::binary);
    if (!f.is_open()) return;
    WPod(f, kSceneBinMagic);
    WPod(f, kSceneBinVersion);
    uint32_t count = 0;
    for (auto e : registry.storage<entt::entity>())
        if (registry.valid(e)) ++count;
    WPod(f, count);
    for (auto entity : registry.storage<entt::entity>()) {
        if (!registry.valid(entity)) continue;
        uint64_t flags = 0;
        if (registry.all_of<dse::editor::EditorNameComponent>(entity))          flags |= CF_NAME;
        if (registry.all_of<dse::editor::SiblingIndexComponent>(entity))        flags |= CF_SIBLING_INDEX;
        if (registry.all_of<TransformComponent>(entity))                        flags |= CF_TRANSFORM;
        if (registry.all_of<SpriteRendererComponent>(entity))                   flags |= CF_SPRITE;
        if (registry.all_of<UIRendererComponent>(entity))                       flags |= CF_UI_RENDERER;
        if (registry.all_of<UILabelComponent>(entity))                          flags |= CF_UI_LABEL;
        if (registry.all_of<UIAnchorComponent>(entity))                         flags |= CF_UI_ANCHOR;
        if (registry.all_of<UIGridLayoutComponent>(entity))                     flags |= CF_UI_GRID;
        if (registry.all_of<UICanvasScalerComponent>(entity))                   flags |= CF_UI_CANVAS_SCALER;
        if (registry.all_of<UIAnimationComponent>(entity))                      flags |= CF_UI_ANIMATION;
        if (registry.all_of<RigidBody2DComponent>(entity))                      flags |= CF_RIGIDBODY2D;
        if (registry.all_of<ParticleEmitterComponent>(entity))                  flags |= CF_PARTICLE_EMITTER;
        if (registry.all_of<dse::Camera3DComponent>(entity))                    flags |= CF_CAMERA3D;
        if (registry.all_of<dse::DirectionalLight3DComponent>(entity))          flags |= CF_DIR_LIGHT3D;
        if (registry.all_of<dse::MeshRendererComponent>(entity))                flags |= CF_MESH_RENDERER;
        if (registry.all_of<dse::PointLightComponent>(entity))                  flags |= CF_POINT_LIGHT;
        if (registry.all_of<dse::SpotLightComponent>(entity))                   flags |= CF_SPOT_LIGHT;
        if (registry.all_of<dse::SkyLightComponent>(entity))                    flags |= CF_SKY_LIGHT;
        if (registry.all_of<dse::SkyboxComponent>(entity))                      flags |= CF_SKYBOX;
        if (registry.all_of<dse::Animator3DComponent>(entity))                  flags |= CF_ANIMATOR3D;
        if (registry.all_of<dse::TerrainComponent>(entity))                     flags |= CF_TERRAIN;
        if (registry.all_of<dse::RigidBody3DComponent>(entity))                 flags |= CF_RIGIDBODY3D;
        if (registry.all_of<dse::BoxCollider3DComponent>(entity))              flags |= CF_BOX_COLLIDER3D;
        if (registry.all_of<dse::SphereCollider3DComponent>(entity))           flags |= CF_SPHERE_COLLIDER3D;
        if (registry.all_of<dse::CapsuleCollider3DComponent>(entity))          flags |= CF_CAPSULE_COLLIDER3D;
        if (registry.all_of<dse::MeshCollider3DComponent>(entity))             flags |= CF_MESH_COLLIDER3D;
        if (registry.all_of<AudioSourceComponent>(entity))                     flags |= CF_AUDIO_SOURCE;
        if (registry.all_of<AudioListenerComponent>(entity))                   flags |= CF_AUDIO_LISTENER;
        WPod(f, static_cast<uint32_t>(entity));
        WPod(f, flags);
        if (flags & CF_NAME)             WStr(f, registry.get<dse::editor::EditorNameComponent>(entity).name);
        if (flags & CF_SIBLING_INDEX)    WPod(f, registry.get<dse::editor::SiblingIndexComponent>(entity).index);
        if (flags & CF_TRANSFORM) {
            auto& t = registry.get<TransformComponent>(entity);
            WPod(f, t.position); WPod(f, t.rotation); WPod(f, t.scale); WPod(f, t.dirty);
        }
        if (flags & CF_SPRITE) {
            auto& s = registry.get<SpriteRendererComponent>(entity);
            WStr(f, s.shader_variant); WPod(f, s.color); WPod(f, s.uv);
            WPod(f, s.uv_offset); WPod(f, s.uv_scroll_speed);
            WPod(f, s.sorting_layer); WPod(f, s.order_in_layer); WPod(f, s.visible);
        }
        if (flags & CF_UI_RENDERER) {
            auto& ui = registry.get<UIRendererComponent>(entity);
            WPod(f, ui.texture_handle); WPod(f, ui.color); WPod(f, ui.uv);
            WPod(f, ui.order); WPod(f, ui.visible); WPod(f, ui.scale);
            WPod(f, ui.hover_scale); WPod(f, ui.pressed_scale); WPod(f, ui.scale_lerp_speed);
            WPod(f, ui.position); WPod(f, ui.size);
            WPod(f, ui.anchor_min); WPod(f, ui.anchor_max); WPod(f, ui.pivot);
            WPod(f, ui.interactable);
        }
        if (flags & CF_UI_LABEL) {
            auto& lb = registry.get<UILabelComponent>(entity);
            WStr(f, lb.text); WPod(f, lb.use_localization);
            WStr(f, lb.localization_key); WStr(f, lb.fallback_text);
            uint32_t pc = static_cast<uint32_t>(lb.localization_params.size()); WPod(f, pc);
            for (auto& [k, v] : lb.localization_params) { WStr(f, k); WStr(f, v); }
            WPod(f, lb.number_value); WPod(f, lb.numeric_mode);
            WPod(f, lb.font_texture_handle); WPod(f, lb.glyph_size); WPod(f, lb.offset);
            WPod(f, lb.spacing); WPod(f, lb.atlas_cols); WPod(f, lb.atlas_rows);
            WPod(f, lb.ascii_start); WPod(f, lb.color);
        }
        if (flags & CF_UI_ANCHOR) {
            auto& a = registry.get<UIAnchorComponent>(entity);
            WPod(f, a.anchor); WPod(f, a.offset);
        }
        if (flags & CF_UI_GRID) {
            auto& g = registry.get<UIGridLayoutComponent>(entity);
            WPod(f, g.columns); WPod(f, g.rows); WPod(f, g.cell_size);
            WPod(f, g.spacing); WPod(f, g.alignment);
        }
        if (flags & CF_UI_CANVAS_SCALER) {
            auto& cs = registry.get<UICanvasScalerComponent>(entity);
            WPod(f, cs.reference_resolution); WPod(f, cs.scale_factor);
            WPod(f, cs.match_width_or_height);
        }
        if (flags & CF_UI_ANIMATION) {
            auto& an = registry.get<UIAnimationComponent>(entity);
            WPod(f, an.target_position); WPod(f, an.target_scale); WPod(f, an.target_alpha);
            WPod(f, an.target_color); WPod(f, an.animate_position); WPod(f, an.animate_scale);
            WPod(f, an.animate_alpha); WPod(f, an.animate_color); WPod(f, an.duration);
            WPod(f, an.elapsed); WPod(f, an.delay); WPod(f, an.delay_remaining);
            WPod(f, an.loop); WPod(f, an.ping_pong); WPod(f, an.playing); WPod(f, an.reverse);
            WPod(f, an.easing); WPod(f, an.start_position); WPod(f, an.start_scale);
            WPod(f, an.start_alpha); WPod(f, an.start_color);
        }
        if (flags & CF_RIGIDBODY2D) {
            auto& rb = registry.get<RigidBody2DComponent>(entity);
            WPod(f, static_cast<int>(rb.type)); WPod(f, rb.velocity);
            WPod(f, rb.gravity_scale); WPod(f, rb.fixed_rotation);
        }
        if (flags & CF_PARTICLE_EMITTER) {
            auto& pe = registry.get<ParticleEmitterComponent>(entity);
            WPod(f, pe.texture_handle); WPod(f, pe.max_particles);
            WPod(f, pe.emit_rate); WPod(f, pe.emit_rate_scale); WPod(f, pe.emitting);
            WPod(f, pe.start_life_time); WPod(f, pe.start_size); WPod(f, pe.start_color);
            WPod(f, pe.velocity_min); WPod(f, pe.velocity_max);
            WPod(f, pe.life_time_min); WPod(f, pe.life_time_max);
            WPod(f, pe.size_min); WPod(f, pe.size_max);
            WPod(f, pe.rotation_min); WPod(f, pe.rotation_max);
            WPod(f, pe.angular_velocity_min); WPod(f, pe.angular_velocity_max);
            WPod(f, pe.use_random_params); WPod(f, pe.use_size_curve); WPod(f, pe.size_curve_end);
            WPod(f, pe.use_alpha_curve); WPod(f, pe.alpha_curve_end);
            WPod(f, pe.use_color_curve); WPod(f, pe.color_curve_end);
            WPod(f, pe.use_speed_curve); WPod(f, pe.speed_curve_end_scale);
            auto wc = [&](const ParticleCurve& c) {
                WPod(f, c.enabled); WPod(f, static_cast<int>(c.type));
                WPod(f, c.start_value); WPod(f, c.end_value);
            };
            wc(pe.size_curve); wc(pe.alpha_curve); wc(pe.speed_curve);
            WPod(f, pe.gravity); WPod(f, pe.enable_collision);
            WPod(f, static_cast<int>(pe.collision_mode));
            WPod(f, pe.collision_bounce); WPod(f, pe.collision_friction);
            WPod(f, pe.collision_life_loss); WPod(f, pe.ground_y);
            WPod(f, pe.use_ground_collision);
        }
        if (flags & CF_CAMERA3D) {
            auto& cam = registry.get<dse::Camera3DComponent>(entity);
            WPod(f, cam.enabled); WPod(f, cam.priority); WPod(f, cam.fov);
            WPod(f, cam.near_clip); WPod(f, cam.far_clip);
        }
        if (flags & CF_DIR_LIGHT3D) {
            auto& l = registry.get<dse::DirectionalLight3DComponent>(entity);
            WPod(f, l.enabled); WPod(f, l.color); WPod(f, l.intensity);
            WPod(f, l.cast_shadow); WPod(f, l.shadow_strength);
        }
        if (flags & CF_MESH_RENDERER) {
            auto& m = registry.get<dse::MeshRendererComponent>(entity);
            WPod(f, m.visible); WPod(f, m.receive_shadow);
            WStr(f, m.mesh_path); WPod(f, m.material_instance_id);
            WStr(f, m.shader_variant); WPod(f, static_cast<int>(m.material_data_source));
            WPod(f, m.color); WPod(f, m.emissive); WPod(f, m.metallic);
            WPod(f, m.roughness); WPod(f, m.ao); WPod(f, m.normal_strength);
            WPod(f, m.material_alpha_cutoff); WPod(f, m.material_alpha_test);
            WPod(f, m.material_double_sided);
        }
        if (flags & CF_POINT_LIGHT) {
            auto& l = registry.get<dse::PointLightComponent>(entity);
            WPod(f, l.enabled); WPod(f, l.color); WPod(f, l.intensity);
            WPod(f, l.radius); WPod(f, l.falloff); WPod(f, l.cast_shadow);
        }
        if (flags & CF_SPOT_LIGHT) {
            auto& l = registry.get<dse::SpotLightComponent>(entity);
            WPod(f, l.enabled); WPod(f, l.color); WPod(f, l.direction);
            WPod(f, l.intensity); WPod(f, l.radius); WPod(f, l.falloff);
            WPod(f, l.inner_cone_angle); WPod(f, l.outer_cone_angle); WPod(f, l.cast_shadow);
        }
        if (flags & CF_SKY_LIGHT) {
            auto& l = registry.get<dse::SkyLightComponent>(entity);
            WPod(f, l.enabled); WPod(f, l.up_color); WPod(f, l.down_color); WPod(f, l.intensity);
        }
        if (flags & CF_SKYBOX) {
            auto& s = registry.get<dse::SkyboxComponent>(entity);
            WPod(f, s.enabled); WPod(f, s.cubemap_handle); WStr(f, s.cubemap_path);
        }
        if (flags & CF_ANIMATOR3D) {
            auto& a = registry.get<dse::Animator3DComponent>(entity);
            WPod(f, a.enabled); WStr(f, a.dskel_path); WStr(f, a.danim_path);
            WPod(f, a.speed); WPod(f, a.loop); WPod(f, a.use_anim_tree);
            WStr(f, a.blend_parameter); WPod(f, a.blend_parameter_value);
            uint32_t nc = static_cast<uint32_t>(a.blend_nodes.size()); WPod(f, nc);
            for (auto& nd : a.blend_nodes) {
                WStr(f, nd.name); WStr(f, nd.danim_path);
                WPod(f, nd.current_time); WPod(f, nd.speed); WPod(f, nd.loop);
                WPod(f, nd.weight); WPod(f, nd.threshold);
            }
        }
        if (flags & CF_TERRAIN) {
            auto& t = registry.get<dse::TerrainComponent>(entity);
            WPod(f, t.enabled); WStr(f, t.heightmap_path); WPod(f, t.texture_handle);
            WPod(f, t.width); WPod(f, t.depth); WPod(f, t.max_height);
            WPod(f, t.resolution_x); WPod(f, t.resolution_z);
            WPod(f, t.use_dynamic_lod); WPod(f, t.max_lod_levels);
            WPod(f, t.lod_distance_factor); WPod(f, t.visible);
        }
        if (flags & CF_RIGIDBODY3D) {
            auto& rb = registry.get<dse::RigidBody3DComponent>(entity);
            WPod(f, static_cast<int>(rb.type)); WPod(f, rb.mass);
            WPod(f, rb.drag); WPod(f, rb.angular_drag); WPod(f, rb.gravity_scale);
            WPod(f, rb.use_gravity); WPod(f, rb.is_kinematic);
        }
        if (flags & CF_BOX_COLLIDER3D) {
            auto& c = registry.get<dse::BoxCollider3DComponent>(entity);
            WPod(f, c.size); WPod(f, c.center); WPod(f, c.is_trigger);
            WPod(f, c.bounciness); WPod(f, c.friction);
        }
        if (flags & CF_SPHERE_COLLIDER3D) {
            auto& c = registry.get<dse::SphereCollider3DComponent>(entity);
            WPod(f, c.radius); WPod(f, c.center); WPod(f, c.is_trigger);
            WPod(f, c.bounciness); WPod(f, c.friction);
        }
        if (flags & CF_CAPSULE_COLLIDER3D) {
            auto& c = registry.get<dse::CapsuleCollider3DComponent>(entity);
            WPod(f, c.radius); WPod(f, c.height); WPod(f, c.center);
            WPod(f, c.direction); WPod(f, c.is_trigger);
            WPod(f, c.bounciness); WPod(f, c.friction);
        }
        if (flags & CF_MESH_COLLIDER3D) {
            auto& c = registry.get<dse::MeshCollider3DComponent>(entity);
            WPod(f, c.convex); WPod(f, c.is_trigger);
            WPod(f, c.bounciness); WPod(f, c.friction);
        }
        if (flags & CF_AUDIO_SOURCE) {
            auto& a = registry.get<AudioSourceComponent>(entity);
            WPod(f, a.play_on_awake); WPod(f, a.loop); WPod(f, a.volume);
            WPod(f, a.pitch); WPod(f, a.spatial_enabled);
            WPod(f, a.min_distance); WPod(f, a.max_distance); WPod(f, a.rolloff);
            WPod(f, static_cast<int>(a.attenuation_model));
            WPod(f, a.occlusion_enabled); WPod(f, a.occlusion_factor);
        }
        if (flags & CF_AUDIO_LISTENER) {
            auto& a = registry.get<AudioListenerComponent>(entity);
            WPod(f, a.enabled); WPod(f, a.listener_index);
        }
    }
}

static bool LoadSceneBinary(entt::registry& registry,
                            const std::string& bin_path) {
    std::ifstream f(bin_path, std::ios::binary);
    if (!f.is_open()) return false;
    uint32_t magic = 0, ver = 0;
    if (!RPod(f, magic) || magic != kSceneBinMagic) return false;
    if (!RPod(f, ver)   || ver   != kSceneBinVersion) return false;
    uint32_t count = 0;
    if (!RPod(f, count)) return false;
    registry.clear();
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t eid = 0; uint64_t flags = 0;
        if (!RPod(f, eid) || !RPod(f, flags)) return false;
        auto entity = registry.create();
        if (flags & CF_NAME) {
            std::string nm; if (!RStr(f, nm)) return false;
            registry.emplace<dse::editor::EditorNameComponent>(entity, std::move(nm));
        }
        if (flags & CF_SIBLING_INDEX) {
            int idx = 0; if (!RPod(f, idx)) return false;
            registry.emplace<dse::editor::SiblingIndexComponent>(entity).index = idx;
        }
        if (flags & CF_TRANSFORM) {
            auto& t = registry.emplace<TransformComponent>(entity);
            if (!RPod(f, t.position)||!RPod(f, t.rotation)||!RPod(f, t.scale)||!RPod(f, t.dirty)) return false;
        }
        if (flags & CF_SPRITE) {
            auto& s = registry.emplace<SpriteRendererComponent>(entity);
            if (!RStr(f, s.shader_variant)||!RPod(f, s.color)||!RPod(f, s.uv)) return false;
            if (!RPod(f, s.uv_offset)||!RPod(f, s.uv_scroll_speed)) return false;
            if (!RPod(f, s.sorting_layer)||!RPod(f, s.order_in_layer)||!RPod(f, s.visible)) return false;
        }
        if (flags & CF_UI_RENDERER) {
            auto& ui = registry.emplace<UIRendererComponent>(entity);
            if (!RPod(f, ui.texture_handle)||!RPod(f, ui.color)||!RPod(f, ui.uv)) return false;
            if (!RPod(f, ui.order)||!RPod(f, ui.visible)||!RPod(f, ui.scale)) return false;
            if (!RPod(f, ui.hover_scale)||!RPod(f, ui.pressed_scale)||!RPod(f, ui.scale_lerp_speed)) return false;
            if (!RPod(f, ui.position)||!RPod(f, ui.size)) return false;
            if (!RPod(f, ui.anchor_min)||!RPod(f, ui.anchor_max)||!RPod(f, ui.pivot)) return false;
            if (!RPod(f, ui.interactable)) return false;
        }
        if (flags & CF_UI_LABEL) {
            auto& lb = registry.emplace<UILabelComponent>(entity);
            if (!RStr(f, lb.text)||!RPod(f, lb.use_localization)) return false;
            if (!RStr(f, lb.localization_key)||!RStr(f, lb.fallback_text)) return false;
            uint32_t pc = 0; if (!RPod(f, pc)) return false;
            for (uint32_t pi = 0; pi < pc; ++pi) {
                std::string k, v;
                if (!RStr(f, k)||!RStr(f, v)) return false;
                lb.localization_params[std::move(k)] = std::move(v);
            }
            if (!RPod(f, lb.number_value)||!RPod(f, lb.numeric_mode)) return false;
            if (!RPod(f, lb.font_texture_handle)||!RPod(f, lb.glyph_size)||!RPod(f, lb.offset)) return false;
            if (!RPod(f, lb.spacing)||!RPod(f, lb.atlas_cols)||!RPod(f, lb.atlas_rows)) return false;
            if (!RPod(f, lb.ascii_start)||!RPod(f, lb.color)) return false;
            lb.runtime_glyph_entities.clear(); lb.dirty = true;
        }
        if (flags & CF_UI_ANCHOR) {
            auto& a = registry.emplace<UIAnchorComponent>(entity);
            if (!RPod(f, a.anchor)||!RPod(f, a.offset)) return false;
        }
        if (flags & CF_UI_GRID) {
            auto& g = registry.emplace<UIGridLayoutComponent>(entity);
            if (!RPod(f, g.columns)||!RPod(f, g.rows)||!RPod(f, g.cell_size)) return false;
            if (!RPod(f, g.spacing)||!RPod(f, g.alignment)) return false;
        }
        if (flags & CF_UI_CANVAS_SCALER) {
            auto& cs = registry.emplace<UICanvasScalerComponent>(entity);
            if (!RPod(f, cs.reference_resolution)||!RPod(f, cs.scale_factor)) return false;
            if (!RPod(f, cs.match_width_or_height)) return false;
        }
        if (flags & CF_UI_ANIMATION) {
            auto& an = registry.emplace<UIAnimationComponent>(entity);
            if (!RPod(f, an.target_position)||!RPod(f, an.target_scale)||!RPod(f, an.target_alpha)) return false;
            if (!RPod(f, an.target_color)||!RPod(f, an.animate_position)||!RPod(f, an.animate_scale)) return false;
            if (!RPod(f, an.animate_alpha)||!RPod(f, an.animate_color)||!RPod(f, an.duration)) return false;
            if (!RPod(f, an.elapsed)||!RPod(f, an.delay)||!RPod(f, an.delay_remaining)) return false;
            if (!RPod(f, an.loop)||!RPod(f, an.ping_pong)||!RPod(f, an.playing)||!RPod(f, an.reverse)) return false;
            if (!RPod(f, an.easing)||!RPod(f, an.start_position)||!RPod(f, an.start_scale)) return false;
            if (!RPod(f, an.start_alpha)||!RPod(f, an.start_color)) return false;
        }
        if (flags & CF_RIGIDBODY2D) {
            auto& rb = registry.emplace<RigidBody2DComponent>(entity);
            int ti = 0; if (!RPod(f, ti)) return false;
            rb.type = static_cast<RigidBody2DType>(ti);
            if (!RPod(f, rb.velocity)||!RPod(f, rb.gravity_scale)||!RPod(f, rb.fixed_rotation)) return false;
            rb.runtime_body = nullptr;
        }
        if (flags & CF_PARTICLE_EMITTER) {
            auto& pe = registry.emplace<ParticleEmitterComponent>(entity);
            if (!RPod(f, pe.texture_handle)||!RPod(f, pe.max_particles)) return false;
            if (!RPod(f, pe.emit_rate)||!RPod(f, pe.emit_rate_scale)||!RPod(f, pe.emitting)) return false;
            if (!RPod(f, pe.start_life_time)||!RPod(f, pe.start_size)||!RPod(f, pe.start_color)) return false;
            if (!RPod(f, pe.velocity_min)||!RPod(f, pe.velocity_max)) return false;
            if (!RPod(f, pe.life_time_min)||!RPod(f, pe.life_time_max)) return false;
            if (!RPod(f, pe.size_min)||!RPod(f, pe.size_max)) return false;
            if (!RPod(f, pe.rotation_min)||!RPod(f, pe.rotation_max)) return false;
            if (!RPod(f, pe.angular_velocity_min)||!RPod(f, pe.angular_velocity_max)) return false;
            if (!RPod(f, pe.use_random_params)||!RPod(f, pe.use_size_curve)||!RPod(f, pe.size_curve_end)) return false;
            if (!RPod(f, pe.use_alpha_curve)||!RPod(f, pe.alpha_curve_end)) return false;
            if (!RPod(f, pe.use_color_curve)||!RPod(f, pe.color_curve_end)) return false;
            if (!RPod(f, pe.use_speed_curve)||!RPod(f, pe.speed_curve_end_scale)) return false;
            auto rc = [&](ParticleCurve& c) -> bool {
                int ti2 = 0;
                if (!RPod(f, c.enabled)||!RPod(f, ti2)) return false;
                c.type = static_cast<ParticleCurveType>(ti2);
                return RPod(f, c.start_value) && RPod(f, c.end_value);
            };
            if (!rc(pe.size_curve)||!rc(pe.alpha_curve)||!rc(pe.speed_curve)) return false;
            if (!RPod(f, pe.gravity)||!RPod(f, pe.enable_collision)) return false;
            int cm = 0; if (!RPod(f, cm)) return false;
            pe.collision_mode = static_cast<ParticleCollisionMode>(cm);
            if (!RPod(f, pe.collision_bounce)||!RPod(f, pe.collision_friction)) return false;
            if (!RPod(f, pe.collision_life_loss)||!RPod(f, pe.ground_y)||!RPod(f, pe.use_ground_collision)) return false;
            pe.particles.clear(); pe.emit_accumulator = 0.0f; pe.pending_burst = 0;
        }
        if (flags & CF_CAMERA3D) {
            auto& cam = registry.emplace<dse::Camera3DComponent>(entity);
            if (!RPod(f, cam.enabled)||!RPod(f, cam.priority)||!RPod(f, cam.fov)) return false;
            if (!RPod(f, cam.near_clip)||!RPod(f, cam.far_clip)) return false;
        }
        if (flags & CF_DIR_LIGHT3D) {
            auto& l = registry.emplace<dse::DirectionalLight3DComponent>(entity);
            if (!RPod(f, l.enabled)||!RPod(f, l.color)||!RPod(f, l.intensity)) return false;
            if (!RPod(f, l.cast_shadow)||!RPod(f, l.shadow_strength)) return false;
        }
        if (flags & CF_MESH_RENDERER) {
            auto& m = registry.emplace<dse::MeshRendererComponent>(entity);
            if (!RPod(f, m.visible)||!RPod(f, m.receive_shadow)) return false;
            if (!RStr(f, m.mesh_path)||!RPod(f, m.material_instance_id)) return false;
            if (!RStr(f, m.shader_variant)) return false;
            int mds = 0; if (!RPod(f, mds)) return false;
            m.material_data_source = static_cast<dse::MeshRendererComponent::MaterialDataSource>(mds);
            if (!RPod(f, m.color)||!RPod(f, m.emissive)||!RPod(f, m.metallic)) return false;
            if (!RPod(f, m.roughness)||!RPod(f, m.ao)||!RPod(f, m.normal_strength)) return false;
            if (!RPod(f, m.material_alpha_cutoff)||!RPod(f, m.material_alpha_test)) return false;
            if (!RPod(f, m.material_double_sided)) return false;
        }
        if (flags & CF_POINT_LIGHT) {
            auto& l = registry.emplace<dse::PointLightComponent>(entity);
            if (!RPod(f, l.enabled)||!RPod(f, l.color)||!RPod(f, l.intensity)) return false;
            if (!RPod(f, l.radius)||!RPod(f, l.falloff)||!RPod(f, l.cast_shadow)) return false;
        }
        if (flags & CF_SPOT_LIGHT) {
            auto& l = registry.emplace<dse::SpotLightComponent>(entity);
            if (!RPod(f, l.enabled)||!RPod(f, l.color)||!RPod(f, l.direction)) return false;
            if (!RPod(f, l.intensity)||!RPod(f, l.radius)||!RPod(f, l.falloff)) return false;
            if (!RPod(f, l.inner_cone_angle)||!RPod(f, l.outer_cone_angle)||!RPod(f, l.cast_shadow)) return false;
        }
        if (flags & CF_SKY_LIGHT) {
            auto& l = registry.emplace<dse::SkyLightComponent>(entity);
            if (!RPod(f, l.enabled)||!RPod(f, l.up_color)||!RPod(f, l.down_color)||!RPod(f, l.intensity)) return false;
        }
        if (flags & CF_SKYBOX) {
            auto& s = registry.emplace<dse::SkyboxComponent>(entity);
            if (!RPod(f, s.enabled)||!RPod(f, s.cubemap_handle)||!RStr(f, s.cubemap_path)) return false;
        }
        if (flags & CF_ANIMATOR3D) {
            auto& a = registry.emplace<dse::Animator3DComponent>(entity);
            if (!RPod(f, a.enabled)||!RStr(f, a.dskel_path)||!RStr(f, a.danim_path)) return false;
            if (!RPod(f, a.speed)||!RPod(f, a.loop)||!RPod(f, a.use_anim_tree)) return false;
            if (!RStr(f, a.blend_parameter)||!RPod(f, a.blend_parameter_value)) return false;
            uint32_t nc = 0; if (!RPod(f, nc)) return false;
            if (nc > 65536u) return false;  // 损坏场景可能给出近 4G 的 nc → resize 超大分配触发 bad_alloc
            a.blend_nodes.resize(nc);
            for (auto& nd : a.blend_nodes) {
                if (!RStr(f, nd.name)||!RStr(f, nd.danim_path)) return false;
                if (!RPod(f, nd.current_time)||!RPod(f, nd.speed)||!RPod(f, nd.loop)) return false;
                if (!RPod(f, nd.weight)||!RPod(f, nd.threshold)) return false;
            }
            a.state_machine.reset(); a.final_bone_matrices.clear();
        }
        if (flags & CF_TERRAIN) {
            auto& t = registry.emplace<dse::TerrainComponent>(entity);
            if (!RPod(f, t.enabled)||!RStr(f, t.heightmap_path)||!RPod(f, t.texture_handle)) return false;
            if (!RPod(f, t.width)||!RPod(f, t.depth)||!RPod(f, t.max_height)) return false;
            if (!RPod(f, t.resolution_x)||!RPod(f, t.resolution_z)) return false;
            if (!RPod(f, t.use_dynamic_lod)||!RPod(f, t.max_lod_levels)||!RPod(f, t.lod_distance_factor)) return false;
            if (!RPod(f, t.visible)) return false;
            t.is_dirty = true;
        }
        if (flags & CF_RIGIDBODY3D) {
            auto& rb = registry.emplace<dse::RigidBody3DComponent>(entity);
            int ti = 0; if (!RPod(f, ti)) return false;
            rb.type = static_cast<dse::RigidBody3DType>(ti);
            if (!RPod(f, rb.mass)||!RPod(f, rb.drag)||!RPod(f, rb.angular_drag)) return false;
            if (!RPod(f, rb.gravity_scale)||!RPod(f, rb.use_gravity)||!RPod(f, rb.is_kinematic)) return false;
            rb.runtime_body = nullptr;
        }
        if (flags & CF_BOX_COLLIDER3D) {
            auto& c = registry.emplace<dse::BoxCollider3DComponent>(entity);
            if (!RPod(f, c.size)||!RPod(f, c.center)||!RPod(f, c.is_trigger)) return false;
            if (!RPod(f, c.bounciness)||!RPod(f, c.friction)) return false;
            c.runtime_shape = nullptr;
        }
        if (flags & CF_SPHERE_COLLIDER3D) {
            auto& c = registry.emplace<dse::SphereCollider3DComponent>(entity);
            if (!RPod(f, c.radius)||!RPod(f, c.center)||!RPod(f, c.is_trigger)) return false;
            if (!RPod(f, c.bounciness)||!RPod(f, c.friction)) return false;
            c.runtime_shape = nullptr;
        }
        if (flags & CF_CAPSULE_COLLIDER3D) {
            auto& c = registry.emplace<dse::CapsuleCollider3DComponent>(entity);
            if (!RPod(f, c.radius)||!RPod(f, c.height)||!RPod(f, c.center)) return false;
            if (!RPod(f, c.direction)||!RPod(f, c.is_trigger)) return false;
            if (!RPod(f, c.bounciness)||!RPod(f, c.friction)) return false;
            c.runtime_shape = nullptr;
        }
        if (flags & CF_MESH_COLLIDER3D) {
            auto& c = registry.emplace<dse::MeshCollider3DComponent>(entity);
            if (!RPod(f, c.convex)||!RPod(f, c.is_trigger)) return false;
            if (!RPod(f, c.bounciness)||!RPod(f, c.friction)) return false;
            c.runtime_shape = nullptr;
        }
        if (flags & CF_AUDIO_SOURCE) {
            auto& a = registry.emplace<AudioSourceComponent>(entity);
            if (!RPod(f, a.play_on_awake)||!RPod(f, a.loop)||!RPod(f, a.volume)) return false;
            if (!RPod(f, a.pitch)||!RPod(f, a.spatial_enabled)) return false;
            if (!RPod(f, a.min_distance)||!RPod(f, a.max_distance)||!RPod(f, a.rolloff)) return false;
            int am = 0; if (!RPod(f, am)) return false;
            a.attenuation_model = static_cast<AudioAttenuationModel>(am);
            if (!RPod(f, a.occlusion_enabled)||!RPod(f, a.occlusion_factor)) return false;
            a.runtime_handle = 0;
        }
        if (flags & CF_AUDIO_LISTENER) {
            auto& a = registry.emplace<AudioListenerComponent>(entity);
            if (!RPod(f, a.enabled)||!RPod(f, a.listener_index)) return false;
        }
    }
    return true;
}

namespace dse::editor {

namespace {

using CopyComponentFn = void (*)(entt::registry&, entt::entity, entt::registry&, entt::entity);

struct ComponentCopyEntry {
    CopyComponentFn copy = nullptr;
    bool include_in_additive = true;
};

template <typename Component>
void CopyPlainComponent(entt::registry& dst,
                        entt::entity dst_entity,
                        entt::registry& src,
                        entt::entity src_entity) {
    if (src.all_of<Component>(src_entity)) {
        dst.emplace<Component>(dst_entity, src.get<Component>(src_entity));
    }
}

void CopyUIRendererComponent(entt::registry& dst,
                             entt::entity dst_entity,
                             entt::registry& src,
                             entt::entity src_entity) {
    if (src.all_of<UIRendererComponent>(src_entity)) {
        auto ui = src.get<UIRendererComponent>(src_entity);
        ui.is_hovered = false;
        ui.is_pressed = false;
        ui.runtime_model = glm::mat4(1.0f);
        dst.emplace<UIRendererComponent>(dst_entity, std::move(ui));
    }
}

void CopyUILabelComponent(entt::registry& dst,
                          entt::entity dst_entity,
                          entt::registry& src,
                          entt::entity src_entity) {
    if (src.all_of<UILabelComponent>(src_entity)) {
        auto label = src.get<UILabelComponent>(src_entity);
        label.runtime_glyph_entities.clear();
        label.dirty = true;
        dst.emplace<UILabelComponent>(dst_entity, std::move(label));
    }
}

void CopyUIRichTextComponent(entt::registry& dst,
                             entt::entity dst_entity,
                             entt::registry& src,
                             entt::entity src_entity) {
    if (src.all_of<UIRichTextComponent>(src_entity)) {
        auto rich = src.get<UIRichTextComponent>(src_entity);
        rich.dirty = true;
        dst.emplace<UIRichTextComponent>(dst_entity, std::move(rich));
    }
}

void CopyRigidBody2DComponent(entt::registry& dst,
                              entt::entity dst_entity,
                              entt::registry& src,
                              entt::entity src_entity) {
    if (src.all_of<RigidBody2DComponent>(src_entity)) {
        auto rigidbody = src.get<RigidBody2DComponent>(src_entity);
        rigidbody.runtime_body = nullptr;
        dst.emplace<RigidBody2DComponent>(dst_entity, std::move(rigidbody));
    }
}

void CopyParticleEmitterComponent(entt::registry& dst,
                                  entt::entity dst_entity,
                                  entt::registry& src,
                                  entt::entity src_entity) {
    if (src.all_of<ParticleEmitterComponent>(src_entity)) {
        auto emitter = src.get<ParticleEmitterComponent>(src_entity);
        emitter.particles.clear();
        emitter.emit_accumulator = 0.0f;
        emitter.pending_burst = 0;
        dst.emplace<ParticleEmitterComponent>(dst_entity, std::move(emitter));
    }
}

void CopyAnimator3DComponent(entt::registry& dst,
                             entt::entity dst_entity,
                             entt::registry& src,
                             entt::entity src_entity) {
    if (src.all_of<dse::Animator3DComponent>(src_entity)) {
        auto animator = src.get<dse::Animator3DComponent>(src_entity);
        animator.state_machine.reset();
        animator.final_bone_matrices.clear();
        animator.transition_progress = 0.0f;
        animator.current_time = 0.0f;
        dst.emplace<dse::Animator3DComponent>(dst_entity, std::move(animator));
    }
}

void CopyTerrainComponent(entt::registry& dst,
                          entt::entity dst_entity,
                          entt::registry& src,
                          entt::entity src_entity) {
    if (src.all_of<dse::TerrainComponent>(src_entity)) {
        auto terrain = src.get<dse::TerrainComponent>(src_entity);
        terrain.is_dirty = true;
        dst.emplace<dse::TerrainComponent>(dst_entity, std::move(terrain));
    }
}

void CopyRigidBody3DComponent(entt::registry& dst,
                              entt::entity dst_entity,
                              entt::registry& src,
                              entt::entity src_entity) {
    if (src.all_of<dse::RigidBody3DComponent>(src_entity)) {
        auto rb = src.get<dse::RigidBody3DComponent>(src_entity);
        rb.runtime_body = nullptr;
        dst.emplace<dse::RigidBody3DComponent>(dst_entity, std::move(rb));
    }
}

const std::vector<ComponentCopyEntry>& GetComponentCopyRegistry() {
    static const std::vector<ComponentCopyEntry> entries = {
        {&CopyPlainComponent<EditorNameComponent>, true},
        {&CopyPlainComponent<SiblingIndexComponent>, true},
        {&CopyPlainComponent<TransformComponent>, true},
        {&CopyPlainComponent<SpriteRendererComponent>, true},
        {&CopyUIRendererComponent, true},
        {&CopyUILabelComponent, true},
        {&CopyPlainComponent<UIAnchorComponent>, true},
        {&CopyPlainComponent<UIGridLayoutComponent>, true},
        {&CopyPlainComponent<UICanvasScalerComponent>, true},
        {&CopyPlainComponent<UIAnimationComponent>, true},
        {&CopyUIRichTextComponent, true},
        {&CopyRigidBody2DComponent, true},
        {&CopyParticleEmitterComponent, true},
        {&CopyPlainComponent<dse::MeshRendererComponent>, true},
        {&CopyPlainComponent<dse::Camera3DComponent>, true},
        {&CopyPlainComponent<dse::DirectionalLight3DComponent>, true},
        {&CopyPlainComponent<dse::PointLightComponent>, true},
        {&CopyPlainComponent<dse::SpotLightComponent>, true},
        {&CopyPlainComponent<dse::SkyLightComponent>, true},
        {&CopyPlainComponent<dse::SkyboxComponent>, true},
        {&CopyAnimator3DComponent, true},
        {&CopyTerrainComponent, true},
        {&CopyRigidBody3DComponent, true},
        {&CopyPlainComponent<dse::BoxCollider3DComponent>, true},
        {&CopyPlainComponent<dse::SphereCollider3DComponent>, true},
        {&CopyPlainComponent<dse::CapsuleCollider3DComponent>, true},
        {&CopyPlainComponent<dse::MeshCollider3DComponent>, true},
        {&CopyPlainComponent<AudioSourceComponent>, true},
        {&CopyPlainComponent<AudioListenerComponent>, true},
        {&CopyPlainComponent<dse::SubSceneComponent>, true},
        {&CopyPlainComponent<ParentComponent>, false},
    };
    return entries;
}

void CopyRegisteredComponents(entt::registry& dst,
                              entt::entity dst_entity,
                              entt::registry& src,
                              entt::entity src_entity,
                              bool additive) {
    for (const auto& entry : GetComponentCopyRegistry()) {
        if (!entry.copy || (additive && !entry.include_in_additive)) {
            continue;
        }
        entry.copy(dst, dst_entity, src, src_entity);
    }
}

}

void CopyRegistry(entt::registry& dst, entt::registry& src) {
    dst.clear();
    for (auto entity : src.storage<entt::entity>()) {
        if (!src.valid(entity)) continue;
        auto new_ent = dst.create(entity);
        CopyRegisteredComponents(dst, new_ent, src, entity, false);
    }
}


namespace {

using SceneAllocator = rapidjson::Document::AllocatorType;
using SaveComponentFn = void (*)(entt::registry&, entt::entity, rapidjson::Value&, SceneAllocator&);
using LoadComponentFn = void (*)(entt::registry&, entt::entity, const rapidjson::Value&);
using HasComponentFn = bool (*)(entt::registry&, entt::entity);

struct ComponentJsonIOEntry {
    SaveComponentFn save = nullptr;
    LoadComponentFn load = nullptr;
    HasComponentFn has = nullptr;  ///< 保护性检查：实体是否拥有该组件类型
};

static void AddStringMember(rapidjson::Value& parent,
                            const char* name,
                            const std::string& value,
                            SceneAllocator& allocator) {
    parent.AddMember(rapidjson::Value(name, allocator).Move(),
                     rapidjson::Value(value.c_str(), allocator).Move(),
                     allocator);
}

static void SaveEditorNameJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

    rapidjson::Value name_val;
    name_val.SetString(registry.get<EditorNameComponent>(entity).name.c_str(), allocator);
    ent_obj.AddMember("name", name_val, allocator);
}

static void LoadEditorNameJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("name") || !v["name"].IsString()) return;

    registry.emplace<EditorNameComponent>(entity, v["name"].GetString());
}

static void SaveSiblingIndexJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

    rapidjson::Value si_obj(rapidjson::kObjectType);
    si_obj.AddMember("index", registry.get<SiblingIndexComponent>(entity).index, allocator);
    ent_obj.AddMember("sibling_index", si_obj, allocator);
}

static void LoadSiblingIndexJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("sibling_index") || !v["sibling_index"].IsObject()) return;

    const auto& si_obj = v["sibling_index"];
    auto& si = registry.emplace<SiblingIndexComponent>(entity);
    if (si_obj.HasMember("index") && si_obj["index"].IsInt()) si.index = si_obj["index"].GetInt();
}

static void SaveTransformJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadTransformJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("transform") || !v["transform"].IsObject()) return;

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

static void SaveSpriteRendererJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadSpriteRendererJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("sprite") || !v["sprite"].IsObject()) return;

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

static void SaveUIRendererJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadUIRendererJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("ui_renderer") || !v["ui_renderer"].IsObject()) return;

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

static void SaveUILabelJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadUILabelJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("ui_label") || !v["ui_label"].IsObject()) return;

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

static void SaveRigidBody2DJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

    auto& rb = registry.get<RigidBody2DComponent>(entity);
    rapidjson::Value rb_obj(rapidjson::kObjectType);
    rb_obj.AddMember("type", static_cast<int>(rb.type), allocator);
    WriteVec2(rb_obj, "velocity", rb.velocity, allocator);
    rb_obj.AddMember("gravity_scale", rb.gravity_scale, allocator);
    rb_obj.AddMember("fixed_rotation", rb.fixed_rotation, allocator);
    ent_obj.AddMember("rigidbody2d", rb_obj, allocator);
}

static void LoadRigidBody2DJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("rigidbody2d") || !v["rigidbody2d"].IsObject()) return;

    auto& rb_obj = v["rigidbody2d"];
    auto& rb = registry.emplace<RigidBody2DComponent>(entity);
    if (rb_obj.HasMember("type") && rb_obj["type"].IsInt()) rb.type = static_cast<RigidBody2DType>(rb_obj["type"].GetInt());
    ReadVec2(rb_obj, "velocity", rb.velocity);
    if (rb_obj.HasMember("gravity_scale") && rb_obj["gravity_scale"].IsNumber()) rb.gravity_scale = rb_obj["gravity_scale"].GetFloat();
    if (rb_obj.HasMember("fixed_rotation") && rb_obj["fixed_rotation"].IsBool()) rb.fixed_rotation = rb_obj["fixed_rotation"].GetBool();
    rb.runtime_body = nullptr;
}

static void SaveParticleEmitterJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadParticleEmitterJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("particle_emitter") || !v["particle_emitter"].IsObject()) return;

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

static void SaveUIAnchorJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

    auto& anchor = registry.get<UIAnchorComponent>(entity);
    rapidjson::Value anchor_obj(rapidjson::kObjectType);
    anchor_obj.AddMember("anchor", static_cast<int>(anchor.anchor), allocator);
    WriteVec2(anchor_obj, "offset", anchor.offset, allocator);
    ent_obj.AddMember("ui_anchor", anchor_obj, allocator);
}

static void LoadUIAnchorJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("ui_anchor") || !v["ui_anchor"].IsObject()) return;

    auto& anchor_obj = v["ui_anchor"];
    auto& anchor = registry.emplace<UIAnchorComponent>(entity);
    if (anchor_obj.HasMember("anchor") && anchor_obj["anchor"].IsInt()) anchor.anchor = anchor_obj["anchor"].GetInt();
    ReadVec2(anchor_obj, "offset", anchor.offset);
}

static void SaveUIGridLayoutJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

    auto& grid = registry.get<UIGridLayoutComponent>(entity);
    rapidjson::Value grid_obj(rapidjson::kObjectType);
    grid_obj.AddMember("columns", grid.columns, allocator);
    grid_obj.AddMember("rows", grid.rows, allocator);
    WriteVec2(grid_obj, "cell_size", grid.cell_size, allocator);
    WriteVec2(grid_obj, "spacing", grid.spacing, allocator);
    grid_obj.AddMember("alignment", grid.alignment, allocator);
    ent_obj.AddMember("ui_grid_layout", grid_obj, allocator);
}

static void LoadUIGridLayoutJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("ui_grid_layout") || !v["ui_grid_layout"].IsObject()) return;

    auto& grid_obj = v["ui_grid_layout"];
    auto& grid = registry.emplace<UIGridLayoutComponent>(entity);
    if (grid_obj.HasMember("columns") && grid_obj["columns"].IsInt()) grid.columns = grid_obj["columns"].GetInt();
    if (grid_obj.HasMember("rows") && grid_obj["rows"].IsInt()) grid.rows = grid_obj["rows"].GetInt();
    ReadVec2(grid_obj, "cell_size", grid.cell_size);
    ReadVec2(grid_obj, "spacing", grid.spacing);
    if (grid_obj.HasMember("alignment") && grid_obj["alignment"].IsInt()) grid.alignment = grid_obj["alignment"].GetInt();
}

static void SaveUICanvasScalerJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

    auto& scaler = registry.get<UICanvasScalerComponent>(entity);
    rapidjson::Value scaler_obj(rapidjson::kObjectType);
    WriteVec2(scaler_obj, "reference_resolution", scaler.reference_resolution, allocator);
    scaler_obj.AddMember("scale_factor", scaler.scale_factor, allocator);
    scaler_obj.AddMember("match_width_or_height", scaler.match_width_or_height, allocator);
    ent_obj.AddMember("ui_canvas_scaler", scaler_obj, allocator);
}

static void LoadUICanvasScalerJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("ui_canvas_scaler") || !v["ui_canvas_scaler"].IsObject()) return;

    auto& scaler_obj = v["ui_canvas_scaler"];
    auto& scaler = registry.emplace<UICanvasScalerComponent>(entity);
    ReadVec2(scaler_obj, "reference_resolution", scaler.reference_resolution);
    if (scaler_obj.HasMember("scale_factor") && scaler_obj["scale_factor"].IsNumber()) scaler.scale_factor = scaler_obj["scale_factor"].GetFloat();
    if (scaler_obj.HasMember("match_width_or_height") && scaler_obj["match_width_or_height"].IsBool()) scaler.match_width_or_height = scaler_obj["match_width_or_height"].GetBool();
}

static void SaveUIAnimationJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadUIAnimationJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("ui_animation") || !v["ui_animation"].IsObject()) return;

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

static void SaveCamera3DJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

    auto& cam = registry.get<dse::Camera3DComponent>(entity);
    rapidjson::Value cam_obj(rapidjson::kObjectType);
    cam_obj.AddMember("enabled", cam.enabled, allocator);
    cam_obj.AddMember("priority", cam.priority, allocator);
    cam_obj.AddMember("fov", cam.fov, allocator);
    cam_obj.AddMember("near_clip", cam.near_clip, allocator);
    cam_obj.AddMember("far_clip", cam.far_clip, allocator);
    ent_obj.AddMember("camera3d", cam_obj, allocator);
}

static void LoadCamera3DJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("camera3d") || !v["camera3d"].IsObject()) return;

    auto& cam_obj = v["camera3d"];
    auto& cam = registry.emplace<dse::Camera3DComponent>(entity);
    if (cam_obj.HasMember("enabled") && cam_obj["enabled"].IsBool()) cam.enabled = cam_obj["enabled"].GetBool();
    if (cam_obj.HasMember("priority") && cam_obj["priority"].IsInt()) cam.priority = cam_obj["priority"].GetInt();
    if (cam_obj.HasMember("fov") && cam_obj["fov"].IsNumber()) cam.fov = cam_obj["fov"].GetFloat();
    if (cam_obj.HasMember("near_clip") && cam_obj["near_clip"].IsNumber()) cam.near_clip = cam_obj["near_clip"].GetFloat();
    if (cam_obj.HasMember("far_clip") && cam_obj["far_clip"].IsNumber()) cam.far_clip = cam_obj["far_clip"].GetFloat();
}

static void SaveDirectionalLight3DJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

    auto& light = registry.get<dse::DirectionalLight3DComponent>(entity);
    rapidjson::Value light_obj(rapidjson::kObjectType);
    light_obj.AddMember("enabled", light.enabled, allocator);
    WriteVec3(light_obj, "color", light.color, allocator);
    light_obj.AddMember("intensity", light.intensity, allocator);
    light_obj.AddMember("cast_shadow", light.cast_shadow, allocator);
    light_obj.AddMember("shadow_strength", light.shadow_strength, allocator);
    ent_obj.AddMember("directional_light3d", light_obj, allocator);
}

static void LoadDirectionalLight3DJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("directional_light3d") || !v["directional_light3d"].IsObject()) return;

    auto& light_obj = v["directional_light3d"];
    auto& light = registry.emplace<dse::DirectionalLight3DComponent>(entity);
    if (light_obj.HasMember("enabled") && light_obj["enabled"].IsBool()) light.enabled = light_obj["enabled"].GetBool();
    ReadVec3(light_obj, "color", light.color);
    if (light_obj.HasMember("intensity") && light_obj["intensity"].IsNumber()) light.intensity = light_obj["intensity"].GetFloat();
    if (light_obj.HasMember("cast_shadow") && light_obj["cast_shadow"].IsBool()) light.cast_shadow = light_obj["cast_shadow"].GetBool();
    if (light_obj.HasMember("shadow_strength") && light_obj["shadow_strength"].IsNumber()) light.shadow_strength = light_obj["shadow_strength"].GetFloat();
}

static void SaveMeshRendererJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadMeshRendererJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("mesh_renderer") || !v["mesh_renderer"].IsObject()) return;

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

static void SavePointLightJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadPointLightJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("point_light") || !v["point_light"].IsObject()) return;

    auto& light_obj = v["point_light"];
    auto& light = registry.emplace<dse::PointLightComponent>(entity);
    if (light_obj.HasMember("enabled") && light_obj["enabled"].IsBool()) light.enabled = light_obj["enabled"].GetBool();
    ReadVec3(light_obj, "color", light.color);
    if (light_obj.HasMember("intensity") && light_obj["intensity"].IsNumber()) light.intensity = light_obj["intensity"].GetFloat();
    if (light_obj.HasMember("radius") && light_obj["radius"].IsNumber()) light.radius = light_obj["radius"].GetFloat();
    if (light_obj.HasMember("falloff") && light_obj["falloff"].IsNumber()) light.falloff = light_obj["falloff"].GetFloat();
    if (light_obj.HasMember("cast_shadow") && light_obj["cast_shadow"].IsBool()) light.cast_shadow = light_obj["cast_shadow"].GetBool();
}

static void SaveSpotLightJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadSpotLightJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("spot_light") || !v["spot_light"].IsObject()) return;

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

static void SaveSkyLightJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

    auto& light = registry.get<dse::SkyLightComponent>(entity);
    rapidjson::Value light_obj(rapidjson::kObjectType);
    light_obj.AddMember("enabled", light.enabled, allocator);
    WriteVec3(light_obj, "up_color", light.up_color, allocator);
    WriteVec3(light_obj, "down_color", light.down_color, allocator);
    light_obj.AddMember("intensity", light.intensity, allocator);
    ent_obj.AddMember("sky_light", light_obj, allocator);
}

static void LoadSkyLightJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("sky_light") || !v["sky_light"].IsObject()) return;

    auto& light_obj = v["sky_light"];
    auto& light = registry.emplace<dse::SkyLightComponent>(entity);
    if (light_obj.HasMember("enabled") && light_obj["enabled"].IsBool()) light.enabled = light_obj["enabled"].GetBool();
    ReadVec3(light_obj, "up_color", light.up_color);
    ReadVec3(light_obj, "down_color", light.down_color);
    if (light_obj.HasMember("intensity") && light_obj["intensity"].IsNumber()) light.intensity = light_obj["intensity"].GetFloat();
}

static void SaveSkyboxJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

    auto& skybox = registry.get<dse::SkyboxComponent>(entity);
    rapidjson::Value skybox_obj(rapidjson::kObjectType);
    skybox_obj.AddMember("enabled", skybox.enabled, allocator);
    skybox_obj.AddMember("cubemap_handle", skybox.cubemap_handle, allocator);
    skybox_obj.AddMember("cubemap_path", rapidjson::Value(skybox.cubemap_path.c_str(), allocator).Move(), allocator);
    ent_obj.AddMember("skybox", skybox_obj, allocator);
}

static void LoadSkyboxJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("skybox") || !v["skybox"].IsObject()) return;

    auto& skybox_obj = v["skybox"];
    auto& skybox = registry.emplace<dse::SkyboxComponent>(entity);
    if (skybox_obj.HasMember("enabled") && skybox_obj["enabled"].IsBool()) skybox.enabled = skybox_obj["enabled"].GetBool();
    if (skybox_obj.HasMember("cubemap_handle") && skybox_obj["cubemap_handle"].IsUint()) skybox.cubemap_handle = skybox_obj["cubemap_handle"].GetUint();
    if (skybox_obj.HasMember("cubemap_path") && skybox_obj["cubemap_path"].IsString()) skybox.cubemap_path = skybox_obj["cubemap_path"].GetString();
}

static void SaveSubSceneJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

    auto& sub = registry.get<dse::SubSceneComponent>(entity);
    rapidjson::Value sub_obj(rapidjson::kObjectType);
    sub_obj.AddMember("enabled", sub.enabled, allocator);
    sub_obj.AddMember("scene_path", rapidjson::Value(sub.scene_path.c_str(), allocator).Move(), allocator);
    ent_obj.AddMember("sub_scene", sub_obj, allocator);
}

static void LoadSubSceneJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("sub_scene") || !v["sub_scene"].IsObject()) return;

    const auto& sub_obj = v["sub_scene"];
    auto& sub = registry.emplace<dse::SubSceneComponent>(entity);
    if (sub_obj.HasMember("enabled") && sub_obj["enabled"].IsBool()) sub.enabled = sub_obj["enabled"].GetBool();
    if (sub_obj.HasMember("scene_path") && sub_obj["scene_path"].IsString()) sub.scene_path = sub_obj["scene_path"].GetString();
}

static void SaveAnimator3DJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadAnimator3DJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("animator3d") || !v["animator3d"].IsObject()) return;

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

static void SaveTerrainJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadTerrainJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("terrain") || !v["terrain"].IsObject()) return;

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

static void SaveRigidBody3DJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        rapidjson::Value& ent_obj,
        SceneAllocator& allocator) {

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

static void LoadRigidBody3DJsonComponent(
        entt::registry& registry,
        entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("rigidbody3d") || !v["rigidbody3d"].IsObject()) return;

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

// ─── BoxCollider3D ──────────────────────────────────────────────────────────

static void SaveBoxCollider3DJsonComponent(
        entt::registry& registry, entt::entity entity,
        rapidjson::Value& ent_obj, SceneAllocator& allocator) {
    auto& c = registry.get<dse::BoxCollider3DComponent>(entity);
    rapidjson::Value obj(rapidjson::kObjectType);
    WriteVec3(obj, "size", c.size, allocator);
    WriteVec3(obj, "center", c.center, allocator);
    obj.AddMember("is_trigger", c.is_trigger, allocator);
    obj.AddMember("bounciness", c.bounciness, allocator);
    obj.AddMember("friction", c.friction, allocator);
    ent_obj.AddMember("box_collider3d", obj, allocator);
}

static void LoadBoxCollider3DJsonComponent(
        entt::registry& registry, entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("box_collider3d") || !v["box_collider3d"].IsObject()) return;
    auto& o = v["box_collider3d"];
    auto& c = registry.emplace<dse::BoxCollider3DComponent>(entity);
    ReadVec3(o, "size", c.size);
    ReadVec3(o, "center", c.center);
    if (o.HasMember("is_trigger") && o["is_trigger"].IsBool()) c.is_trigger = o["is_trigger"].GetBool();
    if (o.HasMember("bounciness") && o["bounciness"].IsNumber()) c.bounciness = o["bounciness"].GetFloat();
    if (o.HasMember("friction") && o["friction"].IsNumber()) c.friction = o["friction"].GetFloat();
}

// ─── SphereCollider3D ───────────────────────────────────────────────────────

static void SaveSphereCollider3DJsonComponent(
        entt::registry& registry, entt::entity entity,
        rapidjson::Value& ent_obj, SceneAllocator& allocator) {
    auto& c = registry.get<dse::SphereCollider3DComponent>(entity);
    rapidjson::Value obj(rapidjson::kObjectType);
    obj.AddMember("radius", c.radius, allocator);
    WriteVec3(obj, "center", c.center, allocator);
    obj.AddMember("is_trigger", c.is_trigger, allocator);
    obj.AddMember("bounciness", c.bounciness, allocator);
    obj.AddMember("friction", c.friction, allocator);
    ent_obj.AddMember("sphere_collider3d", obj, allocator);
}

static void LoadSphereCollider3DJsonComponent(
        entt::registry& registry, entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("sphere_collider3d") || !v["sphere_collider3d"].IsObject()) return;
    auto& o = v["sphere_collider3d"];
    auto& c = registry.emplace<dse::SphereCollider3DComponent>(entity);
    if (o.HasMember("radius") && o["radius"].IsNumber()) c.radius = o["radius"].GetFloat();
    ReadVec3(o, "center", c.center);
    if (o.HasMember("is_trigger") && o["is_trigger"].IsBool()) c.is_trigger = o["is_trigger"].GetBool();
    if (o.HasMember("bounciness") && o["bounciness"].IsNumber()) c.bounciness = o["bounciness"].GetFloat();
    if (o.HasMember("friction") && o["friction"].IsNumber()) c.friction = o["friction"].GetFloat();
}

// ─── CapsuleCollider3D ──────────────────────────────────────────────────────

static void SaveCapsuleCollider3DJsonComponent(
        entt::registry& registry, entt::entity entity,
        rapidjson::Value& ent_obj, SceneAllocator& allocator) {
    auto& c = registry.get<dse::CapsuleCollider3DComponent>(entity);
    rapidjson::Value obj(rapidjson::kObjectType);
    obj.AddMember("radius", c.radius, allocator);
    obj.AddMember("height", c.height, allocator);
    WriteVec3(obj, "center", c.center, allocator);
    obj.AddMember("direction", c.direction, allocator);
    obj.AddMember("is_trigger", c.is_trigger, allocator);
    obj.AddMember("bounciness", c.bounciness, allocator);
    obj.AddMember("friction", c.friction, allocator);
    ent_obj.AddMember("capsule_collider3d", obj, allocator);
}

static void LoadCapsuleCollider3DJsonComponent(
        entt::registry& registry, entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("capsule_collider3d") || !v["capsule_collider3d"].IsObject()) return;
    auto& o = v["capsule_collider3d"];
    auto& c = registry.emplace<dse::CapsuleCollider3DComponent>(entity);
    if (o.HasMember("radius") && o["radius"].IsNumber()) c.radius = o["radius"].GetFloat();
    if (o.HasMember("height") && o["height"].IsNumber()) c.height = o["height"].GetFloat();
    ReadVec3(o, "center", c.center);
    if (o.HasMember("direction") && o["direction"].IsInt()) c.direction = o["direction"].GetInt();
    if (o.HasMember("is_trigger") && o["is_trigger"].IsBool()) c.is_trigger = o["is_trigger"].GetBool();
    if (o.HasMember("bounciness") && o["bounciness"].IsNumber()) c.bounciness = o["bounciness"].GetFloat();
    if (o.HasMember("friction") && o["friction"].IsNumber()) c.friction = o["friction"].GetFloat();
}

// ─── MeshCollider3D ─────────────────────────────────────────────────────────

static void SaveMeshCollider3DJsonComponent(
        entt::registry& registry, entt::entity entity,
        rapidjson::Value& ent_obj, SceneAllocator& allocator) {
    auto& c = registry.get<dse::MeshCollider3DComponent>(entity);
    rapidjson::Value obj(rapidjson::kObjectType);
    obj.AddMember("convex", c.convex, allocator);
    obj.AddMember("is_trigger", c.is_trigger, allocator);
    obj.AddMember("bounciness", c.bounciness, allocator);
    obj.AddMember("friction", c.friction, allocator);
    ent_obj.AddMember("mesh_collider3d", obj, allocator);
}

static void LoadMeshCollider3DJsonComponent(
        entt::registry& registry, entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("mesh_collider3d") || !v["mesh_collider3d"].IsObject()) return;
    auto& o = v["mesh_collider3d"];
    auto& c = registry.emplace<dse::MeshCollider3DComponent>(entity);
    if (o.HasMember("convex") && o["convex"].IsBool()) c.convex = o["convex"].GetBool();
    if (o.HasMember("is_trigger") && o["is_trigger"].IsBool()) c.is_trigger = o["is_trigger"].GetBool();
    if (o.HasMember("bounciness") && o["bounciness"].IsNumber()) c.bounciness = o["bounciness"].GetFloat();
    if (o.HasMember("friction") && o["friction"].IsNumber()) c.friction = o["friction"].GetFloat();
}

// ─── AudioSource ────────────────────────────────────────────────────────────

static void SaveAudioSourceJsonComponent(
        entt::registry& registry, entt::entity entity,
        rapidjson::Value& ent_obj, SceneAllocator& allocator) {
    auto& a = registry.get<AudioSourceComponent>(entity);
    rapidjson::Value obj(rapidjson::kObjectType);
    obj.AddMember("play_on_awake", a.play_on_awake, allocator);
    obj.AddMember("loop", a.loop, allocator);
    obj.AddMember("volume", a.volume, allocator);
    obj.AddMember("pitch", a.pitch, allocator);
    obj.AddMember("spatial_enabled", a.spatial_enabled, allocator);
    obj.AddMember("min_distance", a.min_distance, allocator);
    obj.AddMember("max_distance", a.max_distance, allocator);
    obj.AddMember("rolloff", a.rolloff, allocator);
    obj.AddMember("attenuation_model", static_cast<int>(a.attenuation_model), allocator);
    obj.AddMember("occlusion_enabled", a.occlusion_enabled, allocator);
    obj.AddMember("occlusion_factor", a.occlusion_factor, allocator);
    ent_obj.AddMember("audio_source", obj, allocator);
}

static void LoadAudioSourceJsonComponent(
        entt::registry& registry, entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("audio_source") || !v["audio_source"].IsObject()) return;
    auto& o = v["audio_source"];
    auto& a = registry.emplace<AudioSourceComponent>(entity);
    if (o.HasMember("play_on_awake") && o["play_on_awake"].IsBool()) a.play_on_awake = o["play_on_awake"].GetBool();
    if (o.HasMember("loop") && o["loop"].IsBool()) a.loop = o["loop"].GetBool();
    if (o.HasMember("volume") && o["volume"].IsNumber()) a.volume = o["volume"].GetFloat();
    if (o.HasMember("pitch") && o["pitch"].IsNumber()) a.pitch = o["pitch"].GetFloat();
    if (o.HasMember("spatial_enabled") && o["spatial_enabled"].IsBool()) a.spatial_enabled = o["spatial_enabled"].GetBool();
    if (o.HasMember("min_distance") && o["min_distance"].IsNumber()) a.min_distance = o["min_distance"].GetFloat();
    if (o.HasMember("max_distance") && o["max_distance"].IsNumber()) a.max_distance = o["max_distance"].GetFloat();
    if (o.HasMember("rolloff") && o["rolloff"].IsNumber()) a.rolloff = o["rolloff"].GetFloat();
    if (o.HasMember("attenuation_model") && o["attenuation_model"].IsInt()) a.attenuation_model = static_cast<AudioAttenuationModel>(o["attenuation_model"].GetInt());
    if (o.HasMember("occlusion_enabled") && o["occlusion_enabled"].IsBool()) a.occlusion_enabled = o["occlusion_enabled"].GetBool();
    if (o.HasMember("occlusion_factor") && o["occlusion_factor"].IsNumber()) a.occlusion_factor = o["occlusion_factor"].GetFloat();
}

// ─── AudioListener ──────────────────────────────────────────────────────────

static void SaveAudioListenerJsonComponent(
        entt::registry& registry, entt::entity entity,
        rapidjson::Value& ent_obj, SceneAllocator& allocator) {
    auto& a = registry.get<AudioListenerComponent>(entity);
    rapidjson::Value obj(rapidjson::kObjectType);
    obj.AddMember("enabled", a.enabled, allocator);
    obj.AddMember("listener_index", a.listener_index, allocator);
    ent_obj.AddMember("audio_listener", obj, allocator);
}

static void LoadAudioListenerJsonComponent(
        entt::registry& registry, entt::entity entity,
        const rapidjson::Value& v) {
    if (!v.HasMember("audio_listener") || !v["audio_listener"].IsObject()) return;
    auto& o = v["audio_listener"];
    auto& a = registry.emplace<AudioListenerComponent>(entity);
    if (o.HasMember("enabled") && o["enabled"].IsBool()) a.enabled = o["enabled"].GetBool();
    if (o.HasMember("listener_index") && o["listener_index"].IsUint()) a.listener_index = o["listener_index"].GetUint();
}

const std::vector<ComponentJsonIOEntry>& GetComponentJsonIORegistry() {
    static const std::vector<ComponentJsonIOEntry> entries = {
        {&SaveEditorNameJsonComponent, &LoadEditorNameJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::editor::EditorNameComponent>(e); }},
        {&SaveSiblingIndexJsonComponent, &LoadSiblingIndexJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::editor::SiblingIndexComponent>(e); }},
        {&SaveTransformJsonComponent, &LoadTransformJsonComponent,
         [](auto& r, auto e) { return r.all_of<TransformComponent>(e); }},
        {&SaveSpriteRendererJsonComponent, &LoadSpriteRendererJsonComponent,
         [](auto& r, auto e) { return r.all_of<SpriteRendererComponent>(e); }},
        {&SaveUIRendererJsonComponent, &LoadUIRendererJsonComponent,
         [](auto& r, auto e) { return r.all_of<UIRendererComponent>(e); }},
        {&SaveUILabelJsonComponent, &LoadUILabelJsonComponent,
         [](auto& r, auto e) { return r.all_of<UILabelComponent>(e); }},
        {&SaveRigidBody2DJsonComponent, &LoadRigidBody2DJsonComponent,
         [](auto& r, auto e) { return r.all_of<RigidBody2DComponent>(e); }},
        {&SaveParticleEmitterJsonComponent, &LoadParticleEmitterJsonComponent,
         [](auto& r, auto e) { return r.all_of<ParticleEmitterComponent>(e); }},
        {&SaveUIAnchorJsonComponent, &LoadUIAnchorJsonComponent,
         [](auto& r, auto e) { return r.all_of<UIAnchorComponent>(e); }},
        {&SaveUIGridLayoutJsonComponent, &LoadUIGridLayoutJsonComponent,
         [](auto& r, auto e) { return r.all_of<UIGridLayoutComponent>(e); }},
        {&SaveUICanvasScalerJsonComponent, &LoadUICanvasScalerJsonComponent,
         [](auto& r, auto e) { return r.all_of<UICanvasScalerComponent>(e); }},
        {&SaveUIAnimationJsonComponent, &LoadUIAnimationJsonComponent,
         [](auto& r, auto e) { return r.all_of<UIAnimationComponent>(e); }},
        {&SaveCamera3DJsonComponent, &LoadCamera3DJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::Camera3DComponent>(e); }},
        {&SaveDirectionalLight3DJsonComponent, &LoadDirectionalLight3DJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::DirectionalLight3DComponent>(e); }},
        {&SaveMeshRendererJsonComponent, &LoadMeshRendererJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::MeshRendererComponent>(e); }},
        {&SavePointLightJsonComponent, &LoadPointLightJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::PointLightComponent>(e); }},
        {&SaveSpotLightJsonComponent, &LoadSpotLightJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::SpotLightComponent>(e); }},
        {&SaveSkyLightJsonComponent, &LoadSkyLightJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::SkyLightComponent>(e); }},
        {&SaveSkyboxJsonComponent, &LoadSkyboxJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::SkyboxComponent>(e); }},
        {&SaveSubSceneJsonComponent, &LoadSubSceneJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::SubSceneComponent>(e); }},
        {&SaveAnimator3DJsonComponent, &LoadAnimator3DJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::Animator3DComponent>(e); }},
        {&SaveTerrainJsonComponent, &LoadTerrainJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::TerrainComponent>(e); }},
        {&SaveRigidBody3DJsonComponent, &LoadRigidBody3DJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::RigidBody3DComponent>(e); }},
        {&SaveBoxCollider3DJsonComponent, &LoadBoxCollider3DJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::BoxCollider3DComponent>(e); }},
        {&SaveSphereCollider3DJsonComponent, &LoadSphereCollider3DJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::SphereCollider3DComponent>(e); }},
        {&SaveCapsuleCollider3DJsonComponent, &LoadCapsuleCollider3DJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::CapsuleCollider3DComponent>(e); }},
        {&SaveMeshCollider3DJsonComponent, &LoadMeshCollider3DJsonComponent,
         [](auto& r, auto e) { return r.all_of<dse::MeshCollider3DComponent>(e); }},
        {&SaveAudioSourceJsonComponent, &LoadAudioSourceJsonComponent,
         [](auto& r, auto e) { return r.all_of<AudioSourceComponent>(e); }},
        {&SaveAudioListenerJsonComponent, &LoadAudioListenerJsonComponent,
         [](auto& r, auto e) { return r.all_of<AudioListenerComponent>(e); }},
    };
    return entries;
}

} // namespace (json io)

void SaveScene(entt::registry& registry, const std::string& filepath) {

    rapidjson::Document doc;
    doc.SetArray();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    for (auto entity : registry.storage<entt::entity>()) {
        if (!registry.valid(entity)) continue;
        rapidjson::Value ent_obj(rapidjson::kObjectType);
        ent_obj.AddMember("id", static_cast<uint32_t>(entity), allocator);

        for (const auto& io_entry : GetComponentJsonIORegistry()) {
            if (io_entry.save && io_entry.has && io_entry.has(registry, entity)) {
                io_entry.save(registry, entity, ent_obj, allocator);
            }
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
    SaveSceneBinary(registry, filepath + ".bin");

}

void LoadScene(entt::registry& registry, const std::string& filepath) {

    const std::string bin_path = filepath + ".bin";
    if (SceneBinaryIsValid(filepath, bin_path) && LoadSceneBinary(registry, bin_path)) return;

    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    rapidjson::Document doc;
    doc.Parse(content.c_str());

    if (!doc.IsArray()) return;

    registry.clear();
    for (auto& v : doc.GetArray()) {
        auto entity = registry.create();

        for (const auto& io_entry : GetComponentJsonIORegistry()) {
            if (io_entry.load) {
                io_entry.load(registry, entity, v);
            }
        }
    }
    SaveSceneBinary(registry, bin_path);
}

void LoadSceneAdditive(entt::registry& dst, const std::string& filepath, entt::entity parent) {
    entt::registry temp;
    LoadScene(temp, filepath);

    for (auto e : temp.storage<entt::entity>()) {
        if (!temp.valid(e)) continue;
        auto ne = dst.create();

        if (parent != entt::null)
            dst.emplace<ParentComponent>(ne, parent);

        CopyRegisteredComponents(dst, ne, temp, e, true);
    }
}

} // namespace dse::editor
