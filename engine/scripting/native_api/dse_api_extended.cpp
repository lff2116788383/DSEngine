/**
 * @file dse_api_extended.cpp
 * @brief DSEngine Native C ABI — 扩展模块实现
 *
 * 集中实现 Particles3D、Rendering 扩展、Animation 扩展、Gameplay3D 扩展、
 * Open World、Streaming、HTTP、Video、DSSL、Meshlet、World Systems 等
 * Lua 有而 C ABI 缺失的模块。
 *
 * 各模块在对应引擎子系统不可用时安全返回 0/无操作。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/components_3d_animation.h"
#include "engine/ecs/components_3d_character.h"
#include "engine/ecs/components_3d_sky.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <random>

using Entity = entt::entity;
using namespace dse;

namespace {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }
inline AssetManager* GAM() { return static_cast<AssetManager*>(dse_get_asset_manager_ptr()); }

// Handle-based resource registries for modules without ECS components
std::unordered_map<uint32_t, std::vector<glm::vec3>> g_spline_points;
std::unordered_map<uint32_t, int> g_spline_next_id;
uint32_t g_next_handle = 1;

std::mt19937 g_rng{std::random_device{}()};
std::uniform_real_distribution<float> g_uniform(0.0f, 1.0f);

float Hash2D(int x, int y) {
    int n = x * 374761393 + y * 668265263;
    n = (n ^ (n >> 13)) * 1274126177;
    return static_cast<float>((n & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF));
}

float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

} // namespace

// ============================================================
// Particles 3D
// ============================================================

extern "C" void dse_particle_system_3d_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<ParticleSystem3DComponent>(TE(e));
}

extern "C" void dse_particle_system_3d_set_params(uint32_t e, float duration, float start_speed,
                                                  float start_size, float start_rotation,
                                                  int max_particles, float gravity) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (!ps) return;
    ps->duration = duration;
    ps->start_speed = start_speed;
    ps->start_size = start_size;
    ps->start_rotation = start_rotation;
    ps->max_particles = max_particles;
    ps->gravity = gravity;
}

extern "C" int dse_particle_system_3d_get_state(uint32_t e, int* out_alive, int* out_emitted) {
    World* world = GW();
    if (!world) return 0;
    const auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (!ps) return 0;
    if (out_alive) *out_alive = ps->alive_count;
    if (out_emitted) *out_emitted = ps->emitted_count;
    return ps->is_playing ? 1 : 0;
}

extern "C" void dse_particle_emitter_add(uint32_t e, int shape, float rate, float lifetime, float speed) {
    World* world = GW();
    if (!world) return;
    auto& em = world->registry().emplace_or_replace<ParticleEmitterComponent>(TE(e));
    em.shape = static_cast<ParticleEmitterShape>(shape);
    em.emission_rate = rate;
    em.particle_lifetime = lifetime;
    em.emit_speed = speed;
}

extern "C" void dse_particle_set_density(uint32_t e, float density) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (ps) ps->density = density;
}

extern "C" void dse_particle_burst(uint32_t e, int count) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (ps) ps->burst_count += count;
}

extern "C" void dse_particle_set_random(uint32_t e, float pos_rand, float vel_rand, float size_rand, float rot_rand) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (!ps) return;
    ps->position_random = pos_rand;
    ps->velocity_random = vel_rand;
    ps->size_random = size_rand;
    ps->rotation_random = rot_rand;
}

extern "C" void dse_particle_set_size_curve(uint32_t e, float start_size, float end_size) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (ps) { ps->start_size = start_size; ps->end_size = end_size; }
}

extern "C" void dse_particle_set_alpha_curve(uint32_t e, float start_alpha, float end_alpha) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (ps) { ps->start_alpha = start_alpha; ps->end_alpha = end_alpha; }
}

extern "C" void dse_particle_set_speed_curve(uint32_t e, float start_speed, float end_speed) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (ps) { ps->start_speed = start_speed; ps->end_speed = end_speed; }
}

extern "C" void dse_particle_set_gravity(uint32_t e, float gx, float gy, float gz) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (ps) ps->gravity_vec = glm::vec3(gx, gy, gz);
}

extern "C" void dse_particle_set_collision(uint32_t e, int enabled, float bounce) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (ps) { ps->collision_enabled = (enabled != 0); ps->collision_bounce = bounce; }
}

extern "C" void dse_particle_set_color_curve(uint32_t e, float r1, float g1, float b1,
                                              float r2, float g2, float b2) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (ps) { ps->start_color = glm::vec3(r1, g1, b1); ps->end_color = glm::vec3(r2, g2, b2); }
}

extern "C" void dse_particle_set_rotation(uint32_t e, float start_rot, float end_rot) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (ps) { ps->start_rotation = start_rot; ps->end_rotation = end_rot; }
}

// ============================================================
// Rendering Light 扩展
// ============================================================

extern "C" void dse_rendering_add_skybox(uint32_t e, const char* cubemap_path) {
    World* world = GW();
    if (!world) return;
    auto& sb = world->registry().emplace_or_replace<SkyboxComponent>(TE(e));
    if (cubemap_path) sb.cubemap_path = cubemap_path;
}

extern "C" void dse_rendering_add_gi_probe(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<GIProbeComponent>(TE(e));
}

extern "C" void dse_rendering_set_gi_probe(uint32_t e, float intensity, float range, int resolution) {
    World* world = GW();
    if (!world) return;
    auto* gi = world->registry().try_get<GIProbeComponent>(TE(e));
    if (gi) { gi->intensity = intensity; gi->range = range; gi->resolution = resolution; }
}

extern "C" void dse_rendering_set_gi_probe_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* gi = world->registry().try_get<GIProbeComponent>(TE(e));
    if (gi) gi->enabled = (enabled != 0);
}

extern "C" void dse_rendering_get_gi_probe(uint32_t e, float* out_intensity, float* out_range, int* out_resolution) {
    World* world = GW();
    if (!world) return;
    const auto* gi = world->registry().try_get<GIProbeComponent>(TE(e));
    if (!gi) return;
    if (out_intensity) *out_intensity = gi->intensity;
    if (out_range) *out_range = gi->range;
    if (out_resolution) *out_resolution = gi->resolution;
}

extern "C" void dse_rendering_add_light_probe(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<LightProbeComponent>(TE(e));
}

extern "C" void dse_rendering_set_light_probe(uint32_t e, float intensity, float range) {
    World* world = GW();
    if (!world) return;
    auto* lp = world->registry().try_get<LightProbeComponent>(TE(e));
    if (lp) { lp->intensity = intensity; lp->range = range; }
}

extern "C" void dse_rendering_set_light_probe_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* lp = world->registry().try_get<LightProbeComponent>(TE(e));
    if (lp) lp->enabled = (enabled != 0);
}

extern "C" void dse_rendering_add_reflection_probe(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<ReflectionProbeComponent>(TE(e));
}

extern "C" void dse_rendering_set_reflection_probe(uint32_t e, float intensity, float range, int resolution) {
    World* world = GW();
    if (!world) return;
    auto* rp = world->registry().try_get<ReflectionProbeComponent>(TE(e));
    if (rp) { rp->intensity = intensity; rp->range = range; rp->resolution = resolution; }
}

extern "C" void dse_rendering_set_reflection_probe_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* rp = world->registry().try_get<ReflectionProbeComponent>(TE(e));
    if (rp) rp->enabled = (enabled != 0);
}

// ============================================================
// Rendering Camera 扩展
// ============================================================

extern "C" void dse_camera_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<CameraComponent>(TE(e));
}

extern "C" void dse_camera_set_priority(uint32_t e, int priority) {
    World* world = GW();
    if (!world) return;
    auto* cam = world->registry().try_get<CameraComponent>(TE(e));
    if (cam) cam->priority = priority;
}

extern "C" void dse_camera_set_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* cam = world->registry().try_get<CameraComponent>(TE(e));
    if (cam) cam->enabled = (enabled != 0);
}

extern "C" void dse_camera_set_follow(uint32_t e, uint32_t target, float lerp) {
    World* world = GW();
    if (!world) return;
    auto* cam = world->registry().try_get<CameraComponent>(TE(e));
    if (cam) { cam->follow_target = TE(target); cam->follow_lerp = lerp; }
}

extern "C" void dse_free_camera_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<FreeCameraControllerComponent>(TE(e));
}

extern "C" void dse_sprite_add(uint32_t e, uint32_t texture_handle, float w, float h) {
    World* world = GW();
    if (!world) return;
    auto& sp = world->registry().emplace_or_replace<SpriteComponent>(TE(e));
    sp.texture_handle = texture_handle;
    sp.size = glm::vec2(w, h);
}

extern "C" void dse_sprite_set_uv_scroll(uint32_t e, float sx, float sy) {
    World* world = GW();
    if (!world) return;
    auto* sp = world->registry().try_get<SpriteComponent>(TE(e));
    if (sp) sp->uv_scroll = glm::vec2(sx, sy);
}

extern "C" void dse_sprite_set_uv_offset(uint32_t e, float ox, float oy) {
    World* world = GW();
    if (!world) return;
    auto* sp = world->registry().try_get<SpriteComponent>(TE(e));
    if (sp) sp->uv_offset = glm::vec2(ox, oy);
}

// ============================================================
// Rendering Mesh 扩展
// ============================================================

extern "C" void dse_mesh_set_material(uint32_t e, const char* material_path) {
    World* world = GW();
    if (!world || !material_path) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (mr) mr->material_path = material_path;
}

extern "C" void dse_mesh_set_depth_state(uint32_t e, int depth_test, int depth_write) {
    World* world = GW();
    if (!world) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (mr) { mr->depth_test = (depth_test != 0); mr->depth_write = (depth_write != 0); }
}

extern "C" void dse_mesh_set_material_scalar(uint32_t e, const char* param_name, float value) {
    World* world = GW();
    if (!world || !param_name) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (mr) mr->material_params[param_name] = value;
}

extern "C" void dse_mesh_set_texture(uint32_t e, const char* slot, uint32_t texture_handle) {
    World* world = GW();
    if (!world || !slot) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (mr) mr->textures[slot] = texture_handle;
}

extern "C" void dse_mesh_set_emissive(uint32_t e, float r, float g, float b, float intensity) {
    World* world = GW();
    if (!world) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (mr) { mr->emissive = glm::vec3(r, g, b); mr->emissive_intensity = intensity; }
}

// ============================================================
// Rendering FX（Steering / LOD / Hair / Pick）
// ============================================================

extern "C" void dse_steering_add(uint32_t e, float max_speed, float max_force, float mass) {
    World* world = GW();
    if (!world) return;
    auto& st = world->registry().emplace_or_replace<SteeringComponent>(TE(e));
    st.max_speed = max_speed;
    st.max_force = max_force;
    st.mass = mass;
}

extern "C" void dse_steering_set_target(uint32_t e, float x, float y, float z) {
    World* world = GW();
    if (!world) return;
    auto* st = world->registry().try_get<SteeringComponent>(TE(e));
    if (st) st->target = glm::vec3(x, y, z);
}

extern "C" int dse_steering_get_state(uint32_t e, float* out_vel, float* out_accel) {
    World* world = GW();
    if (!world) return 0;
    const auto* st = world->registry().try_get<SteeringComponent>(TE(e));
    if (!st) return 0;
    if (out_vel) { out_vel[0] = st->velocity.x; out_vel[1] = st->velocity.y; out_vel[2] = st->velocity.z; }
    if (out_accel) { out_accel[0] = st->acceleration.x; out_accel[1] = st->acceleration.y; out_accel[2] = st->acceleration.z; }
    return 1;
}

extern "C" void dse_lod_add_level(uint32_t e, float distance, const char* mesh_path) {
    World* world = GW();
    if (!world) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (!mr || !mesh_path) return;
    mr->lod_distances.push_back(distance);
    mr->lod_mesh_paths.push_back(mesh_path);
}

extern "C" void dse_lod_set_scale(uint32_t e, float scale) {
    World* world = GW();
    if (!world) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (mr) mr->lod_scale = scale;
}

extern "C" void dse_lod_set_min_screen_size(uint32_t e, float min_size) {
    World* world = GW();
    if (!world) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (mr) mr->lod_min_screen_size = min_size;
}

extern "C" void dse_lod_set_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (mr) mr->lod_enabled = (enabled != 0);
}

extern "C" void dse_hair_add(uint32_t e, int strand_count, int segment_count, float length) {
    World* world = GW();
    if (!world) return;
    auto& hair = world->registry().emplace_or_replace<HairComponent>(TE(e));
    hair.strand_count = strand_count;
    hair.segment_count = segment_count;
    hair.length = length;
}

extern "C" void dse_hair_set_physics(uint32_t e, float stiffness, float damping, float gravity) {
    World* world = GW();
    if (!world) return;
    auto* hair = world->registry().try_get<HairComponent>(TE(e));
    if (hair) { hair->stiffness = stiffness; hair->damping = damping; hair->gravity = gravity; }
}

extern "C" void dse_hair_set_render(uint32_t e, float thickness, int enable_shadow) {
    World* world = GW();
    if (!world) return;
    auto* hair = world->registry().try_get<HairComponent>(TE(e));
    if (hair) { hair->thickness = thickness; hair->cast_shadow = (enable_shadow != 0); }
}

extern "C" void dse_hair_set_wind(uint32_t e, float wx, float wy, float wz, float strength) {
    World* world = GW();
    if (!world) return;
    auto* hair = world->registry().try_get<HairComponent>(TE(e));
    if (hair) { hair->wind_dir = glm::vec3(wx, wy, wz); hair->wind_strength = strength; }
}

extern "C" void dse_hair_set_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* hair = world->registry().try_get<HairComponent>(TE(e));
    if (hair) hair->enabled = (enabled != 0);
}

extern "C" void dse_hair_set_lod(uint32_t e, float close_dist, float far_dist, int min_strands) {
    World* world = GW();
    if (!world) return;
    auto* hair = world->registry().try_get<HairComponent>(TE(e));
    if (hair) { hair->lod_close_dist = close_dist; hair->lod_far_dist = far_dist; hair->lod_min_strands = min_strands; }
}

extern "C" int dse_render_pick_entity(float screen_x, float screen_y) {
    // Stub: requires render system integration
    return 0;
}

// ============================================================
// Rendering Post 扩展
// ============================================================

extern "C" void dse_decal_add(uint32_t e, uint32_t texture_handle, float w, float h, float d) {
    World* world = GW();
    if (!world) return;
    auto& decal = world->registry().emplace_or_replace<DecalComponent>(TE(e));
    decal.texture_handle = texture_handle;
    decal.size = glm::vec3(w, h, d);
}

extern "C" void dse_decal_set(uint32_t e, float r, float g, float b, float a, float opacity) {
    World* world = GW();
    if (!world) return;
    auto* decal = world->registry().try_get<DecalComponent>(TE(e));
    if (decal) { decal->color = glm::vec4(r, g, b, a); decal->opacity = opacity; }
}

extern "C" int dse_post_process_get_state(uint32_t e, int* out_enabled, int* out_bloom, int* out_ssao,
                                           int* out_ssr, int* out_fxaa, int* out_dof) {
    World* world = GW();
    if (!world) return 0;
    const auto* pp = world->registry().try_get<PostProcessComponent>(TE(e));
    if (!pp) return 0;
    if (out_enabled) *out_enabled = pp->enabled ? 1 : 0;
    if (out_bloom) *out_bloom = pp->bloom_enabled ? 1 : 0;
    if (out_ssao) *out_ssao = pp->ssao_enabled ? 1 : 0;
    if (out_ssr) *out_ssr = pp->ssr_enabled ? 1 : 0;
    if (out_fxaa) *out_fxaa = pp->fxaa_enabled ? 1 : 0;
    if (out_dof) *out_dof = pp->dof_enabled ? 1 : 0;
    return 1;
}

// ============================================================
// Animation 扩展
// ============================================================

extern "C" void dse_anim3d_set_blend_tree_1d(uint32_t e, const char* const* clips, const float* thresholds,
                                             const float* speeds, int count) {
    World* world = GW();
    if (!world || !clips || !thresholds) return;
    auto* anim = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!anim) return;
    anim->blend_tree_clips.clear();
    anim->blend_tree_thresholds.clear();
    anim->blend_tree_speeds.clear();
    for (int i = 0; i < count; ++i) {
        if (clips[i]) anim->blend_tree_clips.push_back(clips[i]);
        anim->blend_tree_thresholds.push_back(thresholds[i]);
        anim->blend_tree_speeds.push_back(speeds ? speeds[i] : 1.0f);
    }
}

extern "C" void dse_anim3d_set_blend_param(uint32_t e, float value) {
    World* world = GW();
    if (!world) return;
    auto* anim = world->registry().try_get<Animator3DComponent>(TE(e));
    if (anim) anim->blend_parameter = value;
}

extern "C" float dse_anim3d_get_blend_param(uint32_t e) {
    World* world = GW();
    if (!world) return 0.0f;
    const auto* anim = world->registry().try_get<Animator3DComponent>(TE(e));
    return anim ? anim->blend_parameter : 0.0f;
}

extern "C" void dse_anim3d_set_layer_weight(uint32_t e, int layer, float weight) {
    World* world = GW();
    if (!world) return;
    auto* anim = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!anim) return;
    if (layer >= 0 && layer < static_cast<int>(anim->layer_weights.size())) {
        anim->layer_weights[layer] = weight;
    }
}

extern "C" float dse_anim3d_get_layer_weight(uint32_t e, int layer) {
    World* world = GW();
    if (!world) return 0.0f;
    const auto* anim = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!anim || layer < 0 || layer >= static_cast<int>(anim->layer_weights.size())) return 0.0f;
    return anim->layer_weights[layer];
}

extern "C" void dse_anim3d_set_layer_mask(uint32_t e, int layer, const char* const* bones, int count) {
    World* world = GW();
    if (!world || !bones) return;
    auto* anim = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!anim) return;
    if (layer >= 0 && layer < static_cast<int>(anim->layer_masks.size())) {
        anim->layer_masks[layer].clear();
        for (int i = 0; i < count; ++i) {
            if (bones[i]) anim->layer_masks[layer].push_back(bones[i]);
        }
    }
}

// ============================================================
// Gameplay3D 扩展
// ============================================================

extern "C" void dse_character_set_slide_params(uint32_t e, float slide_speed, float slide_duration) {
    World* world = GW();
    if (!world) return;
    auto* cm = world->registry().try_get<CharacterMovementConfigComponent>(TE(e));
    if (cm) { cm->slide_speed = slide_speed; cm->slide_duration = slide_duration; }
}

extern "C" void dse_character_set_climb_params(uint32_t e, float climb_speed, float check_distance) {
    World* world = GW();
    if (!world) return;
    auto* cm = world->registry().try_get<CharacterMovementConfigComponent>(TE(e));
    if (cm) { cm->climb_speed = climb_speed; cm->climb_check_distance = check_distance; }
}

extern "C" void dse_character_set_swim_params(uint32_t e, float swim_speed, float water_level) {
    World* world = GW();
    if (!world) return;
    auto* cm = world->registry().try_get<CharacterMovementConfigComponent>(TE(e));
    if (cm) { cm->swim_speed = swim_speed; cm->water_level = water_level; }
}

extern "C" int dse_character_check_ground(uint32_t e, float* out_normal) {
    World* world = GW();
    if (!world) return 0;
    // Use character controller grounded state
    const auto* cc = world->registry().try_get<CharacterController3DComponent>(TE(e));
    if (!cc) return 0;
    if (out_normal) { out_normal[0] = 0.0f; out_normal[1] = 1.0f; out_normal[2] = 0.0f; }
    return cc->is_grounded ? 1 : 0;
}

extern "C" void dse_gameplay_set_interaction(uint32_t e, float range, float cooldown) {
    World* world = GW();
    if (!world) return;
    auto* gc = world->registry().try_get<GameplayTuningComponent>(TE(e));
    if (!gc) {
        auto& ngc = world->registry().emplace_or_replace<GameplayTuningComponent>(TE(e));
        ngc.interaction_range = range;
        ngc.interaction_cooldown = cooldown;
        return;
    }
    gc->interaction_range = range;
    gc->interaction_cooldown = cooldown;
}

extern "C" int dse_gameplay_find_interactable(uint32_t e, uint32_t* out_target, float max_range) {
    World* world = GW();
    if (!world || !out_target) return 0;
    const auto* tf = world->registry().try_get<TransformComponent>(TE(e));
    if (!tf) return 0;
    // Search for nearby entities with GameplayTuningComponent
    float best_dist = max_range * max_range;
    Entity best = entt::null;
    auto view = world->registry().view<TransformComponent, GameplayTuningComponent>();
    for (auto other : view) {
        if (other == TE(e)) continue;
        const auto& otf = view.get<TransformComponent>(other);
        float d2 = glm::dot(otf.position - tf->position, otf.position - tf->position);
        if (d2 < best_dist) { best_dist = d2; best = other; }
    }
    if (best == entt::null) return 0;
    *out_target = static_cast<uint32_t>(static_cast<entt::id_type>(best));
    return 1;
}

// ============================================================
// Open World（stubs — require engine subsystem integration）
// ============================================================

extern "C" int dse_wp_get_loaded_count(void) { return 0; }
extern "C" int dse_wp_force_load(float x, float z, float radius) { (void)x; (void)z; (void)radius; return 0; }
extern "C" int dse_wp_force_unload(float x, float z, float radius) { (void)x; (void)z; (void)radius; return 0; }
extern "C" void dse_wp_world_to_cell(float x, float z, int* out_cx, int* out_cz) {
    if (out_cx) *out_cx = static_cast<int>(x / 1000.0f);
    if (out_cz) *out_cz = static_cast<int>(z / 1000.0f);
}
extern "C" void dse_wp_cell_to_world(int cx, int cz, float* out_x, float* out_z) {
    if (out_x) *out_x = cx * 1000.0f;
    if (out_z) *out_z = cz * 1000.0f;
}

extern "C" int dse_hlod_get_cluster_count(void) { return 0; }
extern "C" int dse_hlod_get_active_proxy_count(void) { return 0; }

extern "C" float dse_vt_get_cache_hit_rate(void) { return 0.0f; }
extern "C" int dse_vt_get_page_table_size(void) { return 0; }
extern "C" int dse_vt_get_physical_atlas_size(void) { return 0; }
extern "C" int dse_vt_get_occupied_pages(void) { return 0; }

extern "C" int dse_clipmap_get_level_count(void) { return 0; }
extern "C" int dse_clipmap_sample_height(float x, float z, float* out_y) {
    if (out_y) *out_y = 0.0f;
    (void)x; (void)z;
    return 0;
}
extern "C" void dse_clipmap_get_config(float* out_cell_size, int* out_levels) {
    if (out_cell_size) *out_cell_size = 1.0f;
    if (out_levels) *out_levels = 0;
}

extern "C" float dse_sdf_query_distance(float x, float y, float z) { (void)x; (void)y; (void)z; return 0.0f; }
extern "C" int dse_sdf_get_cascade_count(void) { return 0; }
extern "C" int dse_sdf_rebuild(void) { return 0; }

extern "C" void dse_ai_lod_register(uint32_t e) { (void)e; }
extern "C" void dse_ai_lod_unregister(uint32_t e) { (void)e; }
extern "C" int dse_ai_lod_should_tick(uint32_t e) { (void)e; return 1; }
extern "C" int dse_ai_lod_get_level(uint32_t e) { (void)e; return 0; }
extern "C" void dse_ai_lod_set_force_active(uint32_t e, int force) { (void)e; (void)force; }
extern "C" int dse_ai_lod_get_registered_count(void) { return 0; }
extern "C" void dse_ai_lod_get_config(float* out_near_dist, float* out_far_dist, int* out_max_level) {
    if (out_near_dist) *out_near_dist = 100.0f;
    if (out_far_dist) *out_far_dist = 1000.0f;
    if (out_max_level) *out_max_level = 3;
}

extern "C" void dse_gpu_particle_set_enabled(int enabled) { (void)enabled; }
extern "C" void dse_gpu_particle_set_emission_rate(float rate) { (void)rate; }
extern "C" void dse_gpu_particle_set_gravity(float gx, float gy, float gz) { (void)gx; (void)gy; (void)gz; }
extern "C" void dse_gpu_particle_set_wind(float wx, float wy, float wz) { (void)wx; (void)wy; (void)wz; }
extern "C" void dse_gpu_particle_set_color(float r, float g, float b, float a) { (void)r; (void)g; (void)b; (void)a; }

extern "C" int dse_wsp_save_all(void) { return 0; }
extern "C" int dse_wsp_save_cell(int cx, int cz) { (void)cx; (void)cz; return 0; }
extern "C" int dse_wsp_load_cell(int cx, int cz) { (void)cx; (void)cz; return 0; }
extern "C" int dse_wsp_reset_cell(int cx, int cz) { (void)cx; (void)cz; return 0; }
extern "C" int dse_wsp_get_dirty_count(void) { return 0; }
extern "C" int dse_wsp_get_total_modifications(void) { return 0; }
extern "C" void dse_wsp_record_destruction(float x, float y, float z, float radius) { (void)x; (void)y; (void)z; (void)radius; }

// Procedural noise
extern "C" float dse_procedural_perlin2d(float x, float y, float scale) {
    if (scale <= 0.0f) scale = 1.0f;
    x /= scale; y /= scale;
    int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    float fx = x - x0, fy = y - y0;
    float v00 = Hash2D(x0, y0), v10 = Hash2D(x0 + 1, y0);
    float v01 = Hash2D(x0, y0 + 1), v11 = Hash2D(x0 + 1, y0 + 1);
    float sx = SmoothStep(fx), sy = SmoothStep(fy);
    return (v00 * (1 - sx) + v10 * sx) * (1 - sy) + (v01 * (1 - sx) + v11 * sx) * sy;
}

extern "C" float dse_procedural_simplex2d(float x, float y, float scale) {
    return dse_procedural_perlin2d(x, y, scale); // Approximation
}

extern "C" float dse_procedural_worley2d(float x, float y, float scale) {
    if (scale <= 0.0f) scale = 1.0f;
    x /= scale; y /= scale;
    int cx = static_cast<int>(std::floor(x)), cy = static_cast<int>(std::floor(y));
    float min_dist = 1.0f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            float hx = Hash2D(cx + dx, cy + dy) + cx + dx;
            float hy = Hash2D(cx + dx + 1000, cy + dy + 1000) + cy + dy;
            float d = (hx - x) * (hx - x) + (hy - y) * (hy - y);
            min_dist = std::min(min_dist, d);
        }
    }
    return std::sqrt(min_dist);
}

extern "C" float dse_procedural_fbm2d(float x, float y, float scale, int octaves) {
    float value = 0.0f, amplitude = 1.0f, total = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        value += dse_procedural_perlin2d(x, y, scale) * amplitude;
        total += amplitude;
        amplitude *= 0.5f;
        scale *= 2.0f;
    }
    return total > 0.0f ? value / total : 0.0f;
}

extern "C" void dse_procedural_random_seed(int seed) {
    g_rng.seed(static_cast<unsigned int>(seed));
}

extern "C" float dse_procedural_random_float(void) {
    return g_uniform(g_rng);
}

// ============================================================
// Streaming（stubs）
// ============================================================

extern "C" uint32_t dse_streaming_create_zone(float x, float y, float z, float radius) {
    (void)x; (void)y; (void)z; (void)radius;
    return g_next_handle++;
}
extern "C" void dse_streaming_destroy_zone(uint32_t zone) { (void)zone; }
extern "C" void dse_streaming_add_asset(uint32_t zone, const char* path) { (void)zone; (void)path; }
extern "C" void dse_streaming_add_assets(uint32_t zone, const char* const* paths, int count) { (void)zone; (void)paths; (void)count; }
extern "C" void dse_streaming_set_zone_center(uint32_t zone, float x, float y, float z) { (void)zone; (void)x; (void)y; (void)z; }
extern "C" void dse_streaming_force_load(uint32_t zone) { (void)zone; }
extern "C" void dse_streaming_force_unload(uint32_t zone) { (void)zone; }
extern "C" int dse_streaming_get_zone_state(uint32_t zone) { (void)zone; return 0; }
extern "C" float dse_streaming_get_zone_progress(uint32_t zone) { (void)zone; return 1.0f; }
extern "C" void dse_streaming_set_budget(int max_loads_per_frame) { (void)max_loads_per_frame; }
extern "C" int dse_streaming_get_active_loads(void) { return 0; }
extern "C" int dse_streaming_get_zone_count(void) { return 0; }

// ============================================================
// HTTP（stubs — require HTTP client integration）
// ============================================================

extern "C" int dse_http_request(const char* method, const char* url, const char* body,
                                const char* headers, int* out_status, char* out_body, int body_cap) {
    (void)method; (void)url; (void)body; (void)headers;
    if (out_status) *out_status = 0;
    if (out_body && body_cap > 0) out_body[0] = '\0';
    return 0;
}
extern "C" int dse_http_get(const char* url, char* out_body, int body_cap) {
    (void)url;
    if (out_body && body_cap > 0) out_body[0] = '\0';
    return 0;
}
extern "C" int dse_http_post(const char* url, const char* body, char* out_body, int body_cap) {
    (void)url; (void)body;
    if (out_body && body_cap > 0) out_body[0] = '\0';
    return 0;
}
extern "C" void dse_http_update(void) {}
extern "C" int dse_http_available(void) { return 0; }

// ============================================================
// Video（stubs — require video player integration）
// ============================================================

extern "C" uint32_t dse_video_create_player(const char* path) { (void)path; return 0; }
extern "C" void dse_video_destroy_player(uint32_t player) { (void)player; }
extern "C" void dse_video_play(uint32_t player) { (void)player; }
extern "C" void dse_video_pause(uint32_t player) { (void)player; }
extern "C" void dse_video_resume(uint32_t player) { (void)player; }
extern "C" void dse_video_stop(uint32_t player) { (void)player; }
extern "C" void dse_video_seek(uint32_t player, float time) { (void)player; (void)time; }
extern "C" void dse_video_set_loop(uint32_t player, int loop) { (void)player; (void)loop; }
extern "C" void dse_video_set_playback_rate(uint32_t player, float rate) { (void)player; (void)rate; }
extern "C" void dse_video_update(uint32_t player) { (void)player; }
extern "C" int dse_video_get_state(uint32_t player) { (void)player; return 0; }
extern "C" float dse_video_get_time(uint32_t player) { (void)player; return 0.0f; }
extern "C" float dse_video_get_duration(uint32_t player) { (void)player; return 0.0f; }
extern "C" int dse_video_get_texture(uint32_t player) { (void)player; return 0; }

// ============================================================
// DSSL（stubs — require shader language integration）
// ============================================================

extern "C" uint32_t dse_dssl_load_material(const char* path) { (void)path; return 0; }
extern "C" uint32_t dse_dssl_create_instance(uint32_t material) { (void)material; return 0; }
extern "C" void dse_dssl_set_float(uint32_t instance, const char* name, float value) { (void)instance; (void)name; (void)value; }
extern "C" void dse_dssl_set_color(uint32_t instance, const char* name, float r, float g, float b, float a) { (void)instance; (void)name; (void)r; (void)g; (void)b; (void)a; }
extern "C" void dse_dssl_set_vec3(uint32_t instance, const char* name, float x, float y, float z) { (void)instance; (void)name; (void)x; (void)y; (void)z; }
extern "C" void dse_dssl_set_texture(uint32_t instance, const char* name, const char* path) { (void)instance; (void)name; (void)path; }
extern "C" void dse_dssl_set_texture_handle(uint32_t instance, const char* name, uint32_t handle) { (void)instance; (void)name; (void)handle; }
extern "C" void dse_dssl_apply_material(uint32_t e, uint32_t instance) { (void)e; (void)instance; }
extern "C" float dse_dssl_get_float(uint32_t instance, const char* name) { (void)instance; (void)name; return 0.0f; }
extern "C" void dse_dssl_get_color(uint32_t instance, const char* name, float* out_rgba) {
    (void)instance; (void)name;
    if (out_rgba) { out_rgba[0] = 1.0f; out_rgba[1] = 1.0f; out_rgba[2] = 1.0f; out_rgba[3] = 1.0f; }
}

// ============================================================
// Meshlet（stubs — require GPU-driven rendering integration）
// ============================================================

extern "C" uint32_t dse_meshlet_build(uint32_t entity) { (void)entity; return 0; }
extern "C" int dse_meshlet_serialize(uint32_t handle, const char* path) { (void)handle; (void)path; return 0; }
extern "C" uint32_t dse_meshlet_deserialize(const char* path) { (void)path; return 0; }
extern "C" void dse_meshlet_destroy(uint32_t handle) { (void)handle; }
extern "C" void dse_meshlet_get_info(uint32_t handle, int* out_meshlets, int* out_triangles) {
    (void)handle;
    if (out_meshlets) *out_meshlets = 0;
    if (out_triangles) *out_triangles = 0;
}
extern "C" int dse_meshlet_cull_create(void) { return static_cast<int>(g_next_handle++); }
extern "C" void dse_meshlet_cull_destroy(int cull_handle) { (void)cull_handle; }
extern "C" void dse_meshlet_cull_register(int cull_handle, uint32_t meshlet_handle) { (void)cull_handle; (void)meshlet_handle; }
extern "C" void dse_meshlet_cull_unregister(int cull_handle, uint32_t meshlet_handle) { (void)cull_handle; (void)meshlet_handle; }
extern "C" void dse_meshlet_cull_begin_frame(int cull_handle, float cam_x, float cam_y, float cam_z) { (void)cull_handle; (void)cam_x; (void)cam_y; (void)cam_z; }
extern "C" void dse_meshlet_cull_add_instance(int cull_handle, uint32_t meshlet_handle, const float* matrix) { (void)cull_handle; (void)meshlet_handle; (void)matrix; }
extern "C" void dse_meshlet_cull_prepare(int cull_handle) { (void)cull_handle; }
extern "C" int dse_meshlet_cull_execute_cpu(int cull_handle) { (void)cull_handle; return 0; }
extern "C" void dse_meshlet_cull_stats(int cull_handle, int* out_total, int* out_visible) {
    (void)cull_handle;
    if (out_total) *out_total = 0;
    if (out_visible) *out_visible = 0;
}

// ============================================================
// World Systems — Spline
// ============================================================

extern "C" uint32_t dse_spline_create(void) {
    uint32_t id = g_next_handle++;
    g_spline_points[id] = {};
    return id;
}

extern "C" void dse_spline_destroy(uint32_t spline) {
    g_spline_points.erase(spline);
}

extern "C" void dse_spline_add_point(uint32_t spline, float x, float y, float z) {
    auto it = g_spline_points.find(spline);
    if (it != g_spline_points.end()) {
        it->second.push_back(glm::vec3(x, y, z));
    }
}

extern "C" void dse_spline_set_point(uint32_t spline, int index, float x, float y, float z) {
    auto it = g_spline_points.find(spline);
    if (it == g_spline_points.end()) return;
    if (index >= 0 && index < static_cast<int>(it->second.size())) {
        it->second[index] = glm::vec3(x, y, z);
    }
}

extern "C" int dse_spline_get_point_count(uint32_t spline) {
    auto it = g_spline_points.find(spline);
    return it != g_spline_points.end() ? static_cast<int>(it->second.size()) : 0;
}

extern "C" float dse_spline_get_length(uint32_t spline) {
    auto it = g_spline_points.find(spline);
    if (it == g_spline_points.end() || it->second.size() < 2) return 0.0f;
    float length = 0.0f;
    for (size_t i = 1; i < it->second.size(); ++i) {
        length += glm::length(it->second[i] - it->second[i - 1]);
    }
    return length;
}

extern "C" void dse_spline_evaluate(uint32_t spline, float t, float* out_xyz) {
    if (!out_xyz) return;
    auto it = g_spline_points.find(spline);
    if (it == g_spline_points.end() || it->second.empty()) {
        out_xyz[0] = out_xyz[1] = out_xyz[2] = 0.0f;
        return;
    }
    t = std::clamp(t, 0.0f, 1.0f);
    const auto& pts = it->second;
    float idx = t * (pts.size() - 1);
    int i0 = static_cast<int>(idx);
    int i1 = std::min(i0 + 1, static_cast<int>(pts.size()) - 1);
    float frac = idx - i0;
    glm::vec3 p = pts[i0] * (1.0f - frac) + pts[i1] * frac;
    out_xyz[0] = p.x; out_xyz[1] = p.y; out_xyz[2] = p.z;
}

extern "C" void dse_spline_evaluate_distance(uint32_t spline, float dist, float* out_xyz) {
    if (!out_xyz) return;
    auto it = g_spline_points.find(spline);
    if (it == g_spline_points.end() || it->second.size() < 2) {
        out_xyz[0] = out_xyz[1] = out_xyz[2] = 0.0f;
        return;
    }
    const auto& pts = it->second;
    float accum = 0.0f;
    for (size_t i = 1; i < pts.size(); ++i) {
        float seg_len = glm::length(pts[i] - pts[i - 1]);
        if (accum + seg_len >= dist) {
            float frac = seg_len > 0.0f ? (dist - accum) / seg_len : 0.0f;
            glm::vec3 p = pts[i - 1] * (1.0f - frac) + pts[i] * frac;
            out_xyz[0] = p.x; out_xyz[1] = p.y; out_xyz[2] = p.z;
            return;
        }
        accum += seg_len;
    }
    glm::vec3 p = pts.back();
    out_xyz[0] = p.x; out_xyz[1] = p.y; out_xyz[2] = p.z;
}

extern "C" void dse_spline_find_nearest(uint32_t spline, float x, float y, float z, float* out_t, float* out_xyz) {
    auto it = g_spline_points.find(spline);
    if (it == g_spline_points.end() || it->second.size() < 2) {
        if (out_t) *out_t = 0.0f;
        if (out_xyz) { out_xyz[0] = x; out_xyz[1] = y; out_xyz[2] = z; }
        return;
    }
    const auto& pts = it->second;
    glm::vec3 target(x, y, z);
    float best_dist = 1e30f, best_t = 0.0f;
    float total_len = 0.0f;
    for (size_t i = 1; i < pts.size(); ++i) {
        float seg_len = glm::length(pts[i] - pts[i - 1]);
        for (float f = 0.0f; f <= 1.0f; f += 0.01f) {
            glm::vec3 p = pts[i - 1] * (1.0f - f) + pts[i] * f;
            float d = glm::distance2(p, target);
            if (d < best_dist) {
                best_dist = d;
                best_t = (total_len + seg_len * f) / std::max(1.0f, dse_spline_get_length(spline));
            }
        }
        total_len += seg_len;
    }
    if (out_t) *out_t = best_t;
    if (out_xyz) dse_spline_evaluate(spline, best_t, out_xyz);
}

extern "C" int dse_spline_gen_road(uint32_t spline, float width, int segments) { (void)spline; (void)width; (void)segments; return 0; }
extern "C" int dse_spline_gen_river(uint32_t spline, float width, float depth, int segments) { (void)spline; (void)width; (void)depth; (void)segments; return 0; }

// ============================================================
// World Systems — Ocean (stubs)
// ============================================================

extern "C" void dse_ocean_update(float dt) { (void)dt; }
extern "C" float dse_ocean_get_height(float x, float z) { (void)x; (void)z; return 0.0f; }
extern "C" void dse_ocean_get_normal(float x, float z, float* out_xyz) { (void)x; (void)z; if (out_xyz) { out_xyz[0] = 0; out_xyz[1] = 1; out_xyz[2] = 0; } }
extern "C" float dse_ocean_get_foam(float x, float z) { (void)x; (void)z; return 0.0f; }
extern "C" void dse_ocean_set_wind(float wx, float wz, float speed) { (void)wx; (void)wz; (void)speed; }
extern "C" void dse_ocean_set_choppiness(float choppiness) { (void)choppiness; }
extern "C" int dse_ocean_get_lod_count(void) { return 0; }

// ============================================================
// World Systems — Editor helpers (stubs)
// ============================================================

extern "C" void dse_editor_terrain_brush(float x, float z, float radius, float strength, int mode) { (void)x; (void)z; (void)radius; (void)strength; (void)mode; }
extern "C" void dse_editor_place_foliage(float x, float z, float radius, int count, uint32_t type) { (void)x; (void)z; (void)radius; (void)count; (void)type; }
extern "C" void dse_editor_erase_foliage(float x, float z, float radius) { (void)x; (void)z; (void)radius; }
extern "C" int dse_editor_get_foliage_count(void) { return 0; }
extern "C" void dse_editor_begin_road(uint32_t spline) { (void)spline; }
extern "C" void dse_editor_add_road_point(float x, float y, float z) { (void)x; (void)y; (void)z; }
extern "C" void dse_editor_end_road(void) {}
extern "C" void dse_editor_undo(void) {}
extern "C" void dse_editor_redo(void) {}

// ============================================================
// World Systems — VSM (stubs)
// ============================================================

extern "C" void dse_vsm_register_light(uint32_t e) { (void)e; }
extern "C" void dse_vsm_unregister_light(uint32_t e) { (void)e; }
extern "C" void dse_vsm_invalidate(void) {}
extern "C" int dse_vsm_get_pages_to_render(void) { return 0; }
extern "C" int dse_vsm_get_clipmap_levels(void) { return 0; }

// ============================================================
// World Systems — EQS (stubs)
// ============================================================

extern "C" uint32_t dse_eqs_create_template(void) { return g_next_handle++; }
extern "C" void dse_eqs_destroy_template(uint32_t tmpl) { (void)tmpl; }
extern "C" void dse_eqs_set_generator(uint32_t tmpl, int type, float radius, float spacing) { (void)tmpl; (void)type; (void)radius; (void)spacing; }
extern "C" void dse_eqs_add_scorer(uint32_t tmpl, int type, float weight) { (void)tmpl; (void)type; (void)weight; }
extern "C" void dse_eqs_set_max_results(uint32_t tmpl, int max_results) { (void)tmpl; (void)max_results; }
extern "C" int dse_eqs_execute(uint32_t tmpl, float x, float y, float z, float* out_positions, int max_results) {
    (void)tmpl; (void)x; (void)y; (void)z; (void)out_positions; (void)max_results;
    return 0;
}
extern "C" int dse_eqs_get_template_count(void) { return 0; }

// ============================================================
// World Systems — Distribution (stubs)
// ============================================================

extern "C" int dse_dist_load_manifest(const char* path) { (void)path; return 0; }
extern "C" int dse_dist_save_manifest(const char* path) { (void)path; return 0; }
extern "C" int dse_dist_package_cell(int cx, int cz, const char* output_path) { (void)cx; (void)cz; (void)output_path; return 0; }
extern "C" int dse_dist_request_download(int cx, int cz) { (void)cx; (void)cz; return 0; }
extern "C" int dse_dist_cancel_download(int cx, int cz) { (void)cx; (void)cz; return 0; }
extern "C" void dse_dist_tick(void) {}
extern "C" int dse_dist_is_installed(int cx, int cz) { (void)cx; (void)cz; return 0; }
extern "C" int dse_dist_get_disk_usage(void) { return 0; }
extern "C" int dse_dist_verify(void) { return 0; }
