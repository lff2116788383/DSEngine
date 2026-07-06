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
#include "engine/scene/world_partition.h"
#include "engine/render/hlod/hlod_system.h"
#include "engine/render/virtual_texture/virtual_texture.h"
#include "engine/terrain/geometry_clipmap.h"
#include "engine/render/sdf/global_sdf.h"
#include "engine/ai/ai_lod_scheduler.h"
#include "engine/render/particles/gpu_particle_system.h"
#include "engine/scene/world_state_persistence.h"
#include "engine/procedural/procedural_generator.h"
#include "engine/assets/streaming_manager.h"
#include "engine/render/material/dssl_material_loader.h"
#include "engine/render/material/dssl_material_instance.h"

#ifdef DSE_ENABLE_HTTP
#include "engine/http/http_client.h"
#endif
#include "engine/video/video_player.h"
#include "engine/render/meshlet/meshlet_builder.h"
#include "engine/render/meshlet/meshlet_cull_pass.h"
#include "engine/terrain/spline_system.h"
#include "engine/render/ocean_system.h"
#include "engine/terrain/world_editor_tools.h"
#include "engine/render/virtual_shadow_map.h"
#include "engine/ai/eqs_system.h"
#include "engine/assets/asset_distribution.h"

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
// Open World — 使用引擎子系统实现
// ============================================================

extern "C" int dse_wp_get_loaded_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldPartitionSystem>();
    return sys ? static_cast<int>(sys->LoadedCellCount()) : 0;
}
extern "C" int dse_wp_force_load(int cx, int cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldPartitionSystem>();
    if (sys) sys->ForceLoadCell({cx, cz});
    return 0;
}
extern "C" int dse_wp_force_unload(int cx, int cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldPartitionSystem>();
    if (sys) sys->ForceUnloadCell({cx, cz});
    return 0;
}
extern "C" void dse_wp_world_to_cell(float x, float y, float z, float cell_size, int* out_cx, int* out_cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldPartitionSystem>();
    if (!sys || !out_cx || !out_cz) { if (out_cx) *out_cx = 0; if (out_cz) *out_cz = 0; return; }
    auto coord = sys->WorldToCell(glm::vec3(x, y, z), cell_size);
    *out_cx = coord.x; *out_cz = coord.y;
}
extern "C" void dse_wp_cell_to_world(int cx, int cz, float cell_size, float* out_x, float* out_y, float* out_z) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldPartitionSystem>();
    if (!sys) { if (out_x) *out_x = 0; if (out_y) *out_y = 0; if (out_z) *out_z = 0; return; }
    auto w = sys->CellToWorld({cx, cz}, cell_size);
    if (out_x) *out_x = w.x; if (out_y) *out_y = w.y; if (out_z) *out_z = w.z;
}

extern "C" int dse_hlod_get_cluster_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::render::HLODSystem>();
    return sys ? static_cast<int>(sys->GetClusters().size()) : 0;
}
extern "C" int dse_hlod_get_active_proxy_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::render::HLODSystem>();
    return sys ? static_cast<int>(sys->ActiveProxyCount()) : 0;
}

extern "C" float dse_vt_get_cache_hit_rate(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::vt::VirtualTextureSystem>();
    return sys ? sys->CacheHitRate() : 0.0f;
}
extern "C" int dse_vt_get_page_table_size(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::vt::VirtualTextureSystem>();
    return sys ? static_cast<int>(sys->PageTableSize()) : 0;
}
extern "C" int dse_vt_get_physical_atlas_size(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::vt::VirtualTextureSystem>();
    return sys ? static_cast<int>(sys->PhysicalAtlasSize()) : 0;
}
extern "C" int dse_vt_get_occupied_pages(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::vt::VirtualTextureSystem>();
    return sys ? static_cast<int>(sys->GetCache().OccupiedCount()) : 0;
}

extern "C" int dse_clipmap_get_level_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::terrain::GeometryClipmapSystem>();
    return sys ? sys->LevelCount() : 0;
}
extern "C" int dse_clipmap_sample_height(float x, float z, float* out_y) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::terrain::GeometryClipmapSystem>();
    if (!sys || !out_y) { if (out_y) *out_y = 0.0f; return 0; }
    *out_y = sys->SampleHeight(x, z);
    return 1;
}
extern "C" void dse_clipmap_get_config(float* out_cell_size, int* out_levels) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::terrain::GeometryClipmapSystem>();
    if (!sys) { if (out_cell_size) *out_cell_size = 1.0f; if (out_levels) *out_levels = 0; return; }
    const auto& cfg = sys->GetConfig();
    if (out_cell_size) *out_cell_size = cfg.base_cell_size;
    if (out_levels) *out_levels = cfg.num_levels;
}

extern "C" float dse_sdf_query_distance(float x, float y, float z) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::render::GlobalSDFSystem>();
    return sys ? sys->QueryDistance(glm::vec3(x, y, z)) : 9999.0f;
}
extern "C" int dse_sdf_get_cascade_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::render::GlobalSDFSystem>();
    return sys ? sys->CascadeCount() : 0;
}
extern "C" int dse_sdf_rebuild(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::render::GlobalSDFSystem>();
    if (sys) sys->RebuildAll();
    return 1;
}

extern "C" void dse_ai_lod_register(uint32_t e, float importance) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    if (sys) sys->Register(e, importance);
}
extern "C" void dse_ai_lod_unregister(uint32_t e) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    if (sys) sys->Unregister(e);
}
extern "C" int dse_ai_lod_should_tick(uint32_t e) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    return sys ? (sys->ShouldTick(e) ? 1 : 0) : 1;
}
extern "C" int dse_ai_lod_get_level(uint32_t e) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    return sys ? static_cast<int>(sys->GetLevel(e)) : 0;
}
extern "C" void dse_ai_lod_set_force_active(uint32_t e, int force) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    if (sys) sys->SetForceActive(e, force != 0);
}
extern "C" int dse_ai_lod_get_registered_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    return sys ? static_cast<int>(sys->RegisteredCount()) : 0;
}
extern "C" void dse_ai_lod_get_config(float* out_near_dist, float* out_far_dist, int* out_max_level) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    if (!sys) { if (out_near_dist) *out_near_dist = 100.0f; if (out_far_dist) *out_far_dist = 1000.0f; if (out_max_level) *out_max_level = 3; return; }
    const auto& cfg = sys->GetConfig();
    if (out_near_dist) *out_near_dist = cfg.near_distance;
    if (out_far_dist) *out_far_dist = cfg.far_distance;
    if (out_max_level) *out_max_level = 3;
}

// GPU Particles — per-entity
extern "C" void dse_gpu_particle_set_enabled(uint32_t e, int enabled) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = static_cast<entt::entity>(e);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.enabled = (enabled != 0);
    }
}
extern "C" void dse_gpu_particle_set_emission_rate(uint32_t e, float rate) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = static_cast<entt::entity>(e);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.emission_rate = rate;
    }
}
extern "C" void dse_gpu_particle_set_gravity(uint32_t e, float gx, float gy, float gz) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = static_cast<entt::entity>(e);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.gravity = glm::vec3(gx, gy, gz);
    }
}
extern "C" void dse_gpu_particle_set_wind(uint32_t e, float wx, float wy, float wz) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = static_cast<entt::entity>(e);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.wind = glm::vec3(wx, wy, wz);
    }
}
extern "C" void dse_gpu_particle_set_color(uint32_t e, float r1, float g1, float b1, float a1,
                                           float r2, float g2, float b2, float a2) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = static_cast<entt::entity>(e);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) {
            comp->config.color_start = glm::vec4(r1, g1, b1, a1);
            comp->config.color_end = glm::vec4(r2, g2, b2, a2);
        }
    }
}

extern "C" int dse_wsp_save_all(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    if (sys) sys->SaveAll();
    return 1;
}
extern "C" int dse_wsp_save_cell(int cx, int cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    return sys ? (sys->SaveCell(cx, cz) ? 1 : 0) : 0;
}
extern "C" int dse_wsp_load_cell(int cx, int cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    return sys ? (sys->LoadCell(cx, cz) ? 1 : 0) : 0;
}
extern "C" int dse_wsp_reset_cell(int cx, int cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    if (sys) sys->ResetCell(cx, cz);
    return 1;
}
extern "C" int dse_wsp_get_dirty_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    return sys ? static_cast<int>(sys->DirtyCellCount()) : 0;
}
extern "C" int dse_wsp_get_total_modifications(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    return sys ? static_cast<int>(sys->TotalModificationCount()) : 0;
}
extern "C" void dse_wsp_record_destruction(int cx, int cz, uint64_t entity_id) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    if (sys) sys->RecordDestruction(cx, cz, entity_id);
}

// Procedural noise — 使用引擎 procedural 命名空间
extern "C" float dse_procedural_perlin2d(float x, float y, uint32_t seed) {
    return dse::procedural::PerlinNoise2D(x, y, seed);
}
extern "C" float dse_procedural_simplex2d(float x, float y, uint32_t seed) {
    return dse::procedural::SimplexNoise2D(x, y, seed);
}
extern "C" float dse_procedural_worley2d(float x, float y, uint32_t seed) {
    return dse::procedural::WorleyNoise2D(x, y, seed);
}
extern "C" float dse_procedural_fbm2d(float x, float y, int octaves, float frequency,
                                     float lacunarity, float persistence, uint32_t seed) {
    dse::procedural::FBMParams params;
    params.octaves = octaves;
    params.frequency = frequency;
    params.lacunarity = lacunarity;
    params.persistence = persistence;
    params.seed = seed;
    return dse::procedural::FBM2D(x, y, params);
}

static dse::procedural::PCGRandom g_pcg(42);
extern "C" void dse_procedural_random_seed(uint64_t seed) {
    g_pcg = dse::procedural::PCGRandom(seed);
}
extern "C" float dse_procedural_random_float(float min_val, float max_val) {
    return g_pcg.Range(min_val, max_val);
}

// ============================================================
// Streaming — 使用 StreamingManager
// ============================================================

static dse::streaming::AssetType ParseAssetType(const char* type_str) {
    if (!type_str) return dse::streaming::AssetType::Texture;
    if (strcmp(type_str, "texture") == 0) return dse::streaming::AssetType::Texture;
    if (strcmp(type_str, "mesh") == 0) return dse::streaming::AssetType::Mesh;
    if (strcmp(type_str, "animation") == 0) return dse::streaming::AssetType::Animation;
    if (strcmp(type_str, "skeleton") == 0) return dse::streaming::AssetType::Skeleton;
    if (strcmp(type_str, "audio") == 0) return dse::streaming::AssetType::Audio;
    if (strcmp(type_str, "material") == 0) return dse::streaming::AssetType::Material;
    return dse::streaming::AssetType::Texture;
}

extern "C" uint32_t dse_streaming_create_zone(const char* name, float x, float y, float z,
                                       float load_r, float unload_r) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (!mgr) return 0;
    return mgr->CreateZone(name ? name : "", glm::vec3(x, y, z), load_r, unload_r);
}
extern "C" void dse_streaming_destroy_zone(uint32_t zone) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (mgr) mgr->DestroyZone(zone);
}
extern "C" void dse_streaming_add_asset(uint32_t zone, const char* path, const char* type_str) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (mgr) mgr->AddAsset(zone, path ? path : "", ParseAssetType(type_str));
}
extern "C" void dse_streaming_add_assets(uint32_t zone, const char* const* paths, int count,
                                        const char* type_str) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (!mgr || !paths) return;
    std::vector<std::string> v;
    v.reserve(count);
    for (int i = 0; i < count; ++i) if (paths[i]) v.emplace_back(paths[i]);
    mgr->AddAssets(zone, v, ParseAssetType(type_str));
}
extern "C" void dse_streaming_set_zone_center(uint32_t zone, float x, float y, float z) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (mgr) mgr->SetZoneCenter(zone, glm::vec3(x, y, z));
}
extern "C" void dse_streaming_force_load(uint32_t zone) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (mgr) mgr->ForceLoadZone(zone);
}
extern "C" void dse_streaming_force_unload(uint32_t zone) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (mgr) mgr->ForceUnloadZone(zone);
}
extern "C" int dse_streaming_get_zone_state(uint32_t zone) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (!mgr) return 0;
    return static_cast<int>(mgr->GetZoneState(zone));
}
extern "C" float dse_streaming_get_zone_progress(uint32_t zone) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    return mgr ? mgr->GetZoneProgress(zone) : 0.0f;
}
extern "C" void dse_streaming_set_budget(int max_loads_per_frame, int max_concurrent) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (!mgr) return;
    mgr->SetLoadBudgetPerFrame(max_loads_per_frame);
    mgr->SetMaxConcurrentLoads(max_concurrent);
}
extern "C" int dse_streaming_get_active_loads(void) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    return mgr ? mgr->GetActiveLoadCount() : 0;
}
extern "C" int dse_streaming_get_zone_count(void) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    return mgr ? static_cast<int>(mgr->GetZoneCount()) : 0;
}

// ============================================================
// HTTP — 异步完成队列模型（使用 HttpClient）
// ============================================================

#ifdef DSE_ENABLE_HTTP

struct HttpCompletion {
    uint32_t request_id;
    dse::http::Response response;
};
static std::vector<HttpCompletion> g_http_completed;
static uint32_t g_http_next_id = 1;

extern "C" uint32_t dse_http_send(const char* method, const char* url, const char* body,
                                const char* headers_json, int timeout_sec, int verify_peer,
                                const char* ca_file) {
    auto* client = dse::core::ServiceLocator::Instance().Get<dse::http::HttpClient>();
    if (!client || !url) return 0;

    dse::http::Request req;
    req.url = url;
    req.method = method ? method : "GET";
    if (body) req.body = body;
    if (timeout_sec > 0) req.timeout_sec = timeout_sec;
    req.verify_peer = (verify_peer != 0);
    if (ca_file) req.ca_file = ca_file;

    // Parse simple JSON headers string {"K":"V",...}
    if (headers_json && headers_json[0]) {
        std::string hs(headers_json);
        size_t pos = 0;
        while ((pos = hs.find('"', pos)) != std::string::npos) {
            size_t k_end = hs.find('"', pos + 1);
            if (k_end == std::string::npos) break;
            std::string key = hs.substr(pos + 1, k_end - pos - 1);
            size_t colon = hs.find(':', k_end + 1);
            if (colon == std::string::npos) break;
            size_t v_start = hs.find('"', colon + 1);
            if (v_start == std::string::npos) break;
            size_t v_end = hs.find('"', v_start + 1);
            if (v_end == std::string::npos) break;
            std::string val = hs.substr(v_start + 1, v_end - v_start - 1);
            req.headers.emplace_back(key, val);
            pos = v_end + 1;
        }
    }

    uint32_t id = g_http_next_id++;
    client->Send(req, [id](const dse::http::Response& resp) {
        g_http_completed.push_back({id, resp});
    });
    return id;
}

extern "C" int dse_http_poll(uint32_t* out_ids, int max_ids) {
    int count = std::min(static_cast<int>(g_http_completed.size()), max_ids);
    for (int i = 0; i < count; ++i) {
        out_ids[i] = g_http_completed[i].request_id;
    }
    g_http_completed.erase(g_http_completed.begin(), g_http_completed.begin() + count);
    return count;
}

extern "C" int dse_http_get_response(uint32_t request_id, int* out_status,
                                     char* out_body, int body_cap,
                                     char* out_error, int error_cap) {
    // Response was already consumed by poll; we need to keep a map.
    // For simplicity, the Lua side should retrieve response during poll.
    // This is a fallback that searches the completion queue.
    if (out_status) *out_status = 0;
    if (out_body && body_cap > 0) out_body[0] = '\0';
    if (out_error && error_cap > 0) out_error[0] = '\0';
    return 0;
}

extern "C" void dse_http_update(void) {
    auto* client = dse::core::ServiceLocator::Instance().Get<dse::http::HttpClient>();
    if (client) client->Poll();
}

extern "C" int dse_http_available(void) {
    return dse::http::HttpClient::Available() ? 1 : 0;
}

#else

extern "C" uint32_t dse_http_send(const char*, const char*, const char*, const char*, int, int, const char*) { return 0; }
extern "C" int dse_http_poll(uint32_t*, int) { return 0; }
extern "C" int dse_http_get_response(uint32_t, int*, char*, int, char*, int) { return 0; }
extern "C" void dse_http_update(void) {}
extern "C" int dse_http_available(void) { return 0; }

#endif

// ============================================================
// Video — 使用 VideoPlayer + 句柄表
// ============================================================

static std::unordered_map<uint32_t, std::unique_ptr<dse::video::VideoPlayer>> g_video_players;
static uint32_t g_video_next_id = 1;

static dse::video::VideoPlayer* GetVideoPlayer(uint32_t id) {
    auto it = g_video_players.find(id);
    return it != g_video_players.end() ? it->second.get() : nullptr;
}

extern "C" uint32_t dse_video_create_player(void) {
    uint32_t id = g_video_next_id++;
    g_video_players[id] = std::make_unique<dse::video::VideoPlayer>();
    return id;
}

extern "C" void dse_video_destroy_player(uint32_t player) {
    g_video_players.erase(player);
}

extern "C" void dse_video_play(uint32_t player, const char* path, int loop, float playback_rate,
                             int decode_audio, int prefetch_frames, int backend) {
    auto* p = GetVideoPlayer(player);
    if (!p || !path) return;
    dse::video::VideoPlayConfig config{};
    config.loop = (loop != 0);
    config.playback_rate = playback_rate;
    config.decode_audio = (decode_audio != 0);
    config.prefetch_frames = prefetch_frames;
    config.backend = static_cast<dse::video::DecoderBackend>(backend);
    p->Play(path, config);
}

extern "C" void dse_video_pause(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    if (p) p->Pause();
}

extern "C" void dse_video_resume(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    if (p) p->Resume();
}

extern "C" void dse_video_stop(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    if (p) p->Stop();
}

extern "C" void dse_video_seek(uint32_t player, float time) {
    auto* p = GetVideoPlayer(player);
    if (p) p->Seek(time);
}

extern "C" void dse_video_set_loop(uint32_t player, int loop) {
    auto* p = GetVideoPlayer(player);
    if (p) p->SetLoop(loop != 0);
}

extern "C" void dse_video_set_playback_rate(uint32_t player, float rate) {
    auto* p = GetVideoPlayer(player);
    if (p) p->SetPlaybackRate(rate);
}

extern "C" uint32_t dse_video_update(uint32_t player, float delta_time) {
    auto* p = GetVideoPlayer(player);
    if (!p) return 0;
    return p->Update(delta_time);
}

extern "C" int dse_video_get_state(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    return p ? static_cast<int>(p->GetState()) : 0;
}

extern "C" float dse_video_get_time(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    return p ? p->GetCurrentTime() : 0.0f;
}

extern "C" float dse_video_get_duration(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    return p ? p->GetDuration() : 0.0f;
}

extern "C" void dse_video_get_info(uint32_t player, int* out_w, int* out_h, float* out_fps,
                                 float* out_duration, int* out_total_frames, int* out_has_audio,
                                 int* out_sample_rate, int* out_channels,
                                 char* out_codec, int codec_cap) {
    auto* p = GetVideoPlayer(player);
    if (!p) return;
    const auto& info = p->GetInfo();
    if (out_w) *out_w = info.width;
    if (out_h) *out_h = info.height;
    if (out_fps) *out_fps = info.fps;
    if (out_duration) *out_duration = info.duration;
    if (out_total_frames) *out_total_frames = info.total_frames;
    if (out_has_audio) *out_has_audio = info.has_audio ? 1 : 0;
    if (out_sample_rate) *out_sample_rate = info.audio_sample_rate;
    if (out_channels) *out_channels = info.audio_channels;
    if (out_codec && codec_cap > 0) {
        strncpy(out_codec, info.codec_name.c_str(), codec_cap - 1);
        out_codec[codec_cap - 1] = '\0';
    }
}

extern "C" uint32_t dse_video_get_texture(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    return p ? p->GetCurrentTexture() : 0;
}

// ============================================================
// DSSL — 使用 DSSLMaterialLoader
// ============================================================

using dse::render::DSSLMaterialLoader;
using dse::render::DSSLShaderType;

static DSSLMaterialLoader* GetDSSL() {
    return dse::core::ServiceLocator::Instance().Get<DSSLMaterialLoader>();
}

extern "C" uint32_t dse_dssl_load_material(const char* path) {
    if (!path) return 0;
    auto* dssl = GetDSSL();
    if (!dssl) return 0;
    auto& am = dse::core::ServiceLocator::Instance().Get<AssetManager>();
    std::string full = am.ResolveAssetPath(path);
    if (full.empty()) full = path;
    auto inst = dssl->LoadFromFile(full, &am);
    return inst ? inst->GetId() : 0;
}
extern "C" uint32_t dse_dssl_create_instance(const char* path) {
    if (!path) return 0;
    auto* dssl = GetDSSL();
    if (!dssl) return 0;
    auto& am = dse::core::ServiceLocator::Instance().Get<AssetManager>();
    std::string full = am.ResolveAssetPath(path);
    if (full.empty()) full = path;
    auto inst = dssl->CreateInstance(full, &am);
    return inst ? inst->GetId() : 0;
}
extern "C" void dse_dssl_set_float(uint32_t instance, const char* name, float value) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    if (inst) inst->SetFloat(name, value);
}
extern "C" void dse_dssl_set_color(uint32_t instance, const char* name, float r, float g, float b, float a) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    if (inst) inst->SetVec4(name, glm::vec4(r, g, b, a));
}
extern "C" void dse_dssl_set_vec3(uint32_t instance, const char* name, float x, float y, float z) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    if (inst) inst->SetVec3(name, glm::vec3(x, y, z));
}
extern "C" void dse_dssl_set_texture(uint32_t instance, const char* name, const char* path) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    if (!inst || !path) return;
    auto& am = dse::core::ServiceLocator::Instance().Get<AssetManager>();
    auto tex = am.LoadTexture(path);
    if (tex) inst->SetTexture(name, tex->GetHandle());
}
extern "C" void dse_dssl_set_texture_handle(uint32_t instance, const char* name, uint32_t handle) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    if (inst) inst->SetTexture(name, handle);
}
extern "C" void dse_dssl_apply_material(uint32_t e, uint32_t instance) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = TE(e);
    auto* mesh = world->registry().try_get<dse::MeshRendererComponent>(entity);
    if (!mesh) return;
    auto* loader = GetDSSL();
    if (!loader) return;
    auto inst = loader->GetInstance(instance);
    if (!inst) return;

    mesh->material_instance_id = instance;
    mesh->material_data_source = dse::MeshRendererComponent::MaterialDataSource::MaterialInstance;

    switch (inst->GetShaderType()) {
        case DSSLShaderType::Surface:
            if (inst->GetRenderModes().lighting_model == "toon")
                mesh->shader_variant = "MESH_TOON";
            else if (inst->GetRenderModes().lighting_model == "watercolor")
                mesh->shader_variant = "MESH_WATERCOLOR";
            else
                mesh->shader_variant = "MESH_PBR";
            break;
        case DSSLShaderType::Unlit:   mesh->shader_variant = "MESH_UNLIT"; break;
        default:                      mesh->shader_variant = "MESH_PBR"; break;
    }

    mesh->color = inst->GetBaseColor();
    mesh->emissive = inst->GetEmissiveColor();
    mesh->metallic = inst->GetMetallic();
    mesh->roughness = inst->GetRoughness();
    mesh->ao = inst->GetAO();
    mesh->normal_strength = inst->GetNormalStrength();
    mesh->material_alpha_cutoff = inst->GetAlphaCutoff();
    mesh->material_alpha_test = inst->GetAlphaTest();
    mesh->material_double_sided = inst->GetDoubleSided();

    unsigned int albedo_tex = inst->GetAlbedoTexture();
    if (albedo_tex) mesh->albedo_texture_handle = albedo_tex;
    unsigned int normal_tex = inst->GetNormalTexture();
    if (normal_tex) mesh->normal_texture_handle = normal_tex;
    unsigned int mr_tex = inst->GetMetallicRoughnessTexture();
    if (mr_tex) mesh->metallic_roughness_texture_handle = mr_tex;
    unsigned int emissive_tex = inst->GetEmissiveTexture();
    if (emissive_tex) mesh->emissive_texture_handle = emissive_tex;
    unsigned int occlusion_tex = inst->GetOcclusionTexture();
    if (occlusion_tex) mesh->occlusion_texture_handle = occlusion_tex;

    mesh->receive_shadow = inst->GetRenderModes().shadows_enabled;

    if (inst->GetRenderModes().lighting_model == "toon") {
        glm::vec4 sc = inst->GetVec4("shadow_color", glm::vec4(0.15f, 0.1f, 0.18f, 1.0f));
        mesh->toon_shadow_color = glm::vec3(sc);
        mesh->toon_shadow_threshold = inst->GetFloat("shadow_threshold", 0.35f);
        mesh->toon_shadow_softness = inst->GetFloat("shadow_softness", 0.05f);
        mesh->toon_specular_size = inst->GetFloat("specular_size", 0.6f);
        mesh->toon_specular_strength = inst->GetFloat("specular_strength", 0.8f);
        mesh->toon_rim_strength = inst->GetFloat("rim_strength", 0.3f);
    }

    if (inst->GetRenderModes().lighting_model == "watercolor") {
        mesh->watercolor_paper_strength = inst->GetFloat("paper_strength", 0.3f);
        mesh->watercolor_edge_darkening = inst->GetFloat("edge_darkening", 0.4f);
        mesh->watercolor_color_bleed = inst->GetFloat("color_bleed", 0.2f);
        mesh->watercolor_pigment_density = inst->GetFloat("pigment_density", 1.0f);
    }
}
extern "C" float dse_dssl_get_float(uint32_t instance, const char* name) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    return inst ? inst->GetFloat(name) : 0.0f;
}
extern "C" void dse_dssl_get_color(uint32_t instance, const char* name, float* out_rgba) {
    if (!out_rgba) return;
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    glm::vec4 v(0.0f);
    if (inst) v = inst->GetVec4(name);
    out_rgba[0] = v.r; out_rgba[1] = v.g; out_rgba[2] = v.b; out_rgba[3] = v.a;
}

// ============================================================
// Meshlet — 使用 MeshletBuilder / MeshletCullPass + 句柄表
// ============================================================

static std::unordered_map<uint32_t, std::unique_ptr<dse::render::MeshletMesh>> g_meshlet_meshes;
static std::unordered_map<uint32_t, std::unique_ptr<dse::render::MeshletCullPass>> g_meshlet_culls;
static uint32_t g_meshlet_next_mesh = 1;
static uint32_t g_meshlet_next_cull = 1;

extern "C" uint32_t dse_meshlet_build(const float* positions, int pos_count,
                                    const uint32_t* indices, int idx_count,
                                    uint32_t max_vertices, uint32_t max_triangles) {
    if (!positions || pos_count <= 0 || !indices || idx_count <= 0) return 0;
    std::vector<glm::vec3> verts(pos_count / 3);
    for (int i = 0; i < pos_count / 3; ++i)
        verts[i] = glm::vec3(positions[i*3], positions[i*3+1], positions[i*3+2]);
    std::vector<uint32_t> idx(indices, indices + idx_count);

    dse::render::MeshletBuildConfig config;
    config.max_vertices = max_vertices > 0 ? max_vertices : 64;
    config.max_triangles = max_triangles > 0 ? max_triangles : 124;

    dse::render::MeshletBuilder builder;
    auto result = builder.Build(verts, idx, config);

    uint32_t id = g_meshlet_next_mesh++;
    g_meshlet_meshes[id] = std::make_unique<dse::render::MeshletMesh>(std::move(result));
    return id;
}

extern "C" int dse_meshlet_serialize(uint32_t handle, const char* path) {
    auto it = g_meshlet_meshes.find(handle);
    if (it == g_meshlet_meshes.end() || !path) return 0;
    return dse::render::MeshletBuilder::Serialize(*it->second, path) ? 1 : 0;
}

extern "C" uint32_t dse_meshlet_deserialize(const char* path) {
    if (!path) return 0;
    auto mesh = std::make_unique<dse::render::MeshletMesh>();
    if (!dse::render::MeshletBuilder::Deserialize(path, *mesh)) return 0;
    uint32_t id = g_meshlet_next_mesh++;
    g_meshlet_meshes[id] = std::move(mesh);
    return id;
}

extern "C" void dse_meshlet_destroy(uint32_t handle) {
    g_meshlet_meshes.erase(handle);
}

extern "C" void dse_meshlet_get_info(uint32_t handle, int* out_meshlets, int* out_vertices,
                                    int* out_indices, int* out_meshlet_vertices) {
    auto it = g_meshlet_meshes.find(handle);
    if (it == g_meshlet_meshes.end()) {
        if (out_meshlets) *out_meshlets = 0;
        if (out_vertices) *out_vertices = 0;
        if (out_indices) *out_indices = 0;
        if (out_meshlet_vertices) *out_meshlet_vertices = 0;
        return;
    }
    const auto& m = *it->second;
    if (out_meshlets) *out_meshlets = static_cast<int>(m.meshlets.size());
    if (out_vertices) *out_vertices = static_cast<int>(m.positions.size());
    if (out_indices) *out_indices = static_cast<int>(m.global_indices.size());
    if (out_meshlet_vertices) *out_meshlet_vertices = static_cast<int>(m.meshlet_vertices.size());
}

extern "C" uint32_t dse_meshlet_cull_create(void) {
    uint32_t id = g_meshlet_next_cull++;
    g_meshlet_culls[id] = std::make_unique<dse::render::MeshletCullPass>();
    return id;
}

extern "C" void dse_meshlet_cull_destroy(uint32_t cull_handle) {
    g_meshlet_culls.erase(cull_handle);
}

extern "C" uint32_t dse_meshlet_cull_register(uint32_t cull_handle, uint32_t meshlet_handle) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit == g_meshlet_culls.end()) return 0;
    auto mit = g_meshlet_meshes.find(meshlet_handle);
    if (mit == g_meshlet_meshes.end()) return 0;
    return cit->second->RegisterMesh(*mit->second);
}

extern "C" void dse_meshlet_cull_unregister(uint32_t cull_handle, uint32_t reg_handle) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit != g_meshlet_culls.end()) cit->second->UnregisterMesh(reg_handle);
}

extern "C" void dse_meshlet_cull_begin_frame(uint32_t cull_handle) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit != g_meshlet_culls.end()) cit->second->BeginFrame();
}

extern "C" void dse_meshlet_cull_add_instance(uint32_t cull_handle, uint32_t reg_handle,
                                            const float* matrix16) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit == g_meshlet_culls.end() || !matrix16) return;
    glm::mat4 m(1.0f);
    const float* p = matrix16;
    for (int i = 0; i < 16; ++i) m[i / 4][i % 4] = p[i];
    cit->second->AddInstance(reg_handle, m);
}

extern "C" uint32_t dse_meshlet_cull_prepare(uint32_t cull_handle, const float* vp_matrix16,
                                           float cam_x, float cam_y, float cam_z) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit == g_meshlet_culls.end() || !vp_matrix16) return 0;
    glm::mat4 vp(1.0f);
    for (int i = 0; i < 16; ++i) vp[i / 4][i % 4] = vp_matrix16[i];
    return cit->second->PrepareGPUData(glm::mat4(1.0f), vp, glm::vec3(cam_x, cam_y, cam_z));
}

extern "C" uint32_t dse_meshlet_cull_execute_cpu(uint32_t cull_handle, const float* vp_matrix16,
                                                float cam_x, float cam_y, float cam_z,
                                                uint32_t flags) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit == g_meshlet_culls.end() || !vp_matrix16) return 0;
    glm::mat4 vp(1.0f);
    for (int i = 0; i < 16; ++i) vp[i / 4][i % 4] = vp_matrix16[i];
    dse::render::MeshletCullConfig config;
    config.enable_frustum_cull = (flags & 1) != 0;
    config.enable_occlusion_cull = (flags & 2) != 0;
    config.enable_cone_cull = (flags & 4) != 0;
    cit->second->CullCPU(vp, glm::vec3(cam_x, cam_y, cam_z), config);
    return cit->second->GetVisibleMeshletCount();
}

extern "C" void dse_meshlet_cull_stats(uint32_t cull_handle, int* out_total, int* out_visible,
                                      int* out_meshes, int* out_instances) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit == g_meshlet_culls.end()) {
        if (out_total) *out_total = 0;
        if (out_visible) *out_visible = 0;
        if (out_meshes) *out_meshes = 0;
        if (out_instances) *out_instances = 0;
        return;
    }
    if (out_total) *out_total = cit->second->GetTotalMeshletCount();
    if (out_visible) *out_visible = cit->second->GetVisibleMeshletCount();
    if (out_meshes) *out_meshes = cit->second->GetRegisteredMeshCount();
    if (out_instances) *out_instances = cit->second->GetInstanceCount();
}

// ============================================================
// World Systems — Spline / Ocean / Editor / VSM / EQS / Distribution
// 使用引擎子系统单例
// ============================================================

static std::unique_ptr<dse::terrain::SplineSystem> g_spline_sys;
static std::unique_ptr<dse::render::OceanSystem> g_ocean_sys;
static std::unique_ptr<dse::terrain::WorldEditorTools> g_editor_sys;
static std::unique_ptr<dse::render::VirtualShadowMapSystem> g_vsm_sys;
static std::unique_ptr<dse::ai::EQSSystem> g_eqs_sys;
static std::unique_ptr<dse::assets::AssetDistribution> g_dist_sys;

// --- Spline ---

extern "C" int dse_spline_init(void) {
    g_spline_sys = std::make_unique<dse::terrain::SplineSystem>();
    return 1;
}

extern "C" void dse_spline_shutdown(void) {
    if (g_spline_sys) { g_spline_sys->Shutdown(); g_spline_sys.reset(); }
}

extern "C" uint32_t dse_spline_create(const char* name) {
    if (!g_spline_sys) return 0;
    return g_spline_sys->CreateSpline(name ? name : "");
}

extern "C" void dse_spline_destroy(uint32_t spline) {
    if (g_spline_sys) g_spline_sys->DestroySpline(spline);
}

extern "C" void dse_spline_add_point(uint32_t spline, float x, float y, float z, float width) {
    if (!g_spline_sys) return;
    dse::terrain::SplinePoint pt;
    pt.position = glm::vec3(x, y, z);
    pt.width = width;
    g_spline_sys->AddPoint(spline, pt);
}

extern "C" void dse_spline_set_point(uint32_t spline, int index, float x, float y, float z, float width) {
    if (!g_spline_sys) return;
    dse::terrain::SplinePoint pt;
    pt.position = glm::vec3(x, y, z);
    pt.width = width;
    g_spline_sys->SetPoint(spline, index, pt);
}

extern "C" void dse_spline_remove_point(uint32_t spline, int index) {
    if (g_spline_sys) g_spline_sys->RemovePoint(spline, index);
}

extern "C" int dse_spline_get_point_count(uint32_t spline) {
    return g_spline_sys ? g_spline_sys->GetPointCount(spline) : 0;
}

extern "C" float dse_spline_get_length(uint32_t spline) {
    return g_spline_sys ? g_spline_sys->GetSplineLength(spline) : 0.0f;
}

extern "C" void dse_spline_evaluate(uint32_t spline, float t, float* out_xyz) {
    if (!out_xyz) return;
    if (!g_spline_sys) { out_xyz[0] = out_xyz[1] = out_xyz[2] = 0; return; }
    auto s = g_spline_sys->EvaluateAtParam(spline, t);
    out_xyz[0] = s.position.x; out_xyz[1] = s.position.y; out_xyz[2] = s.position.z;
}

extern "C" void dse_spline_evaluate_distance(uint32_t spline, float dist, float* out_xyz) {
    if (!out_xyz) return;
    if (!g_spline_sys) { out_xyz[0] = out_xyz[1] = out_xyz[2] = 0; return; }
    auto s = g_spline_sys->EvaluateAtDistance(spline, dist);
    out_xyz[0] = s.position.x; out_xyz[1] = s.position.y; out_xyz[2] = s.position.z;
}

extern "C" float dse_spline_find_nearest(uint32_t spline, float x, float y, float z) {
    if (!g_spline_sys) return 0.0f;
    return g_spline_sys->FindNearestPoint(spline, glm::vec3(x, y, z));
}

extern "C" int dse_spline_gen_road(uint32_t spline, float segment_length, int width_segments) {
    if (!g_spline_sys) return 0;
    dse::terrain::RoadConfig cfg;
    cfg.segment_length = segment_length > 0 ? segment_length : 1.0f;
    cfg.width_segments = width_segments > 0 ? width_segments : 4;
    auto mesh = g_spline_sys->GenerateRoadMesh(spline, cfg);
    return static_cast<int>(mesh.vertices.size());
}

extern "C" int dse_spline_gen_river(uint32_t spline, float segment_length, float depth) {
    if (!g_spline_sys) return 0;
    dse::terrain::RiverConfig cfg;
    cfg.segment_length = segment_length > 0 ? segment_length : 2.0f;
    cfg.depth = depth;
    auto mesh = g_spline_sys->GenerateRiverMesh(spline, cfg);
    return static_cast<int>(mesh.vertices.size());
}

// --- Ocean ---

extern "C" int dse_ocean_init(int fft_resolution, float tile_size, float wind_speed, float choppiness) {
    dse::render::OceanConfig cfg;
    cfg.fft_resolution = fft_resolution > 0 ? fft_resolution : 256;
    cfg.tile_size = tile_size > 0 ? tile_size : 100.0f;
    cfg.wind_speed = wind_speed > 0 ? wind_speed : 10.0f;
    cfg.choppiness = choppiness;
    g_ocean_sys = std::make_unique<dse::render::OceanSystem>();
    g_ocean_sys->Init(cfg);
    return 1;
}

extern "C" void dse_ocean_shutdown(void) {
    if (g_ocean_sys) { g_ocean_sys->Shutdown(); g_ocean_sys.reset(); }
}

extern "C" void dse_ocean_update(float time, float cam_x, float cam_y, float cam_z) {
    if (g_ocean_sys) g_ocean_sys->Update(time, glm::vec3(cam_x, cam_y, cam_z));
}

extern "C" float dse_ocean_get_height(float x, float z) {
    return g_ocean_sys ? g_ocean_sys->GetHeightAt(x, z) : 0.0f;
}

extern "C" void dse_ocean_get_normal(float x, float z, float* out_xyz) {
    if (!out_xyz) return;
    if (!g_ocean_sys) { out_xyz[0] = 0; out_xyz[1] = 1; out_xyz[2] = 0; return; }
    auto n = g_ocean_sys->GetNormalAt(x, z);
    out_xyz[0] = n.x; out_xyz[1] = n.y; out_xyz[2] = n.z;
}

extern "C" float dse_ocean_get_foam(float x, float z) {
    return g_ocean_sys ? g_ocean_sys->GetFoamAt(x, z) : 0.0f;
}

extern "C" void dse_ocean_set_wind(float speed, float dx, float dz) {
    if (g_ocean_sys) g_ocean_sys->SetWind(speed, dx, dz);
}

extern "C" void dse_ocean_set_choppiness(float choppiness) {
    if (g_ocean_sys) g_ocean_sys->SetChoppiness(choppiness);
}

extern "C" void dse_ocean_get_stats(int* out_total_tiles, int* out_visible_tiles,
                                   int* out_fft_res, float* out_max_height) {
    if (!g_ocean_sys) {
        if (out_total_tiles) *out_total_tiles = 0;
        if (out_visible_tiles) *out_visible_tiles = 0;
        if (out_fft_res) *out_fft_res = 0;
        if (out_max_height) *out_max_height = 0;
        return;
    }
    auto stats = g_ocean_sys->GetStats();
    if (out_total_tiles) *out_total_tiles = stats.total_tiles;
    if (out_visible_tiles) *out_visible_tiles = stats.visible_tiles;
    if (out_fft_res) *out_fft_res = stats.fft_resolution;
    if (out_max_height) *out_max_height = stats.current_max_height;
}

extern "C" int dse_ocean_get_lod_count(void) {
    return g_ocean_sys ? g_ocean_sys->GetLODCount() : 0;
}

// --- Editor ---

extern "C" int dse_editor_init(void) {
    g_editor_sys = std::make_unique<dse::terrain::WorldEditorTools>();
    g_editor_sys->Init();
    return 1;
}

extern "C" void dse_editor_shutdown(void) {
    if (g_editor_sys) { g_editor_sys->Shutdown(); g_editor_sys.reset(); }
}

extern "C" int dse_editor_terrain_brush(int op, float x, float y, float z, float radius,
                                        float strength, float falloff) {
    if (!g_editor_sys) return 0;
    dse::terrain::BrushParams params;
    params.center = glm::vec3(x, y, z);
    params.radius = radius;
    params.strength = strength;
    params.falloff = falloff;
    return g_editor_sys->ApplyTerrainBrush(static_cast<dse::terrain::TerrainBrushOp>(op), params);
}

extern "C" void dse_editor_brush_preview(float x, float y, float z, float radius,
                                        float* out_min_x, float* out_min_y,
                                        float* out_max_x, float* out_max_y) {
    if (!g_editor_sys) return;
    dse::terrain::BrushParams params;
    params.center = glm::vec3(x, y, z);
    params.radius = radius;
    auto aabb = g_editor_sys->GetBrushPreview(params);
    if (out_min_x) *out_min_x = aabb.x;
    if (out_min_y) *out_min_y = aabb.y;
    if (out_max_x) *out_max_x = aabb.z;
    if (out_max_y) *out_max_y = aabb.w;
}

extern "C" int dse_editor_place_foliage(float x, float y, float z, float radius,
                                        float density, const char* mesh_path) {
    if (!g_editor_sys) return 0;
    dse::terrain::FoliageBrushParams params;
    params.center = glm::vec3(x, y, z);
    params.radius = radius;
    params.density = density;
    params.mesh_path = mesh_path ? mesh_path : "default_tree";
    return g_editor_sys->PlaceFoliage(params);
}

extern "C" int dse_editor_erase_foliage(float x, float y, float z, float radius) {
    if (!g_editor_sys) return 0;
    return g_editor_sys->EraseFoliage(glm::vec3(x, y, z), radius);
}

extern "C" int dse_editor_get_foliage_count(void) {
    return g_editor_sys ? g_editor_sys->GetFoliageCount() : 0;
}

extern "C" int dse_editor_begin_road(float width) {
    if (!g_editor_sys) return 0;
    return g_editor_sys->BeginRoadDraw(width);
}

extern "C" void dse_editor_add_road_point(uint32_t session, float x, float y, float z) {
    if (g_editor_sys) g_editor_sys->AddRoadPoint(session, glm::vec3(x, y, z));
}

extern "C" void dse_editor_end_road(uint32_t session) {
    if (g_editor_sys) g_editor_sys->EndRoadDraw(session);
}

extern "C" void dse_editor_update_partition_vis(float cam_x, float cam_y, float cam_z, float cell_size) {
    if (g_editor_sys) g_editor_sys->UpdatePartitionVisualization(glm::vec3(cam_x, cam_y, cam_z), cell_size);
}

extern "C" int dse_editor_get_cell_count(void) {
    return g_editor_sys ? g_editor_sys->GetVisibleCellCount() : 0;
}

extern "C" int dse_editor_undo(void) {
    return g_editor_sys ? (g_editor_sys->Undo() ? 1 : 0) : 0;
}

extern "C" int dse_editor_redo(void) {
    return g_editor_sys ? (g_editor_sys->Redo() ? 1 : 0) : 0;
}

// --- VSM ---

extern "C" int dse_vsm_init(uint32_t virtual_resolution, uint32_t page_size,
                            uint32_t pool_pages, uint32_t clipmap_levels) {
    dse::render::VSMConfig cfg;
    cfg.virtual_resolution = virtual_resolution > 0 ? virtual_resolution : 16384;
    cfg.page_size = page_size > 0 ? page_size : 128;
    cfg.physical_pool_pages = pool_pages > 0 ? pool_pages : 4096;
    cfg.clipmap_levels = clipmap_levels > 0 ? clipmap_levels : 5;
    g_vsm_sys = std::make_unique<dse::render::VirtualShadowMapSystem>();
    g_vsm_sys->Init(cfg);
    return 1;
}

extern "C" void dse_vsm_shutdown(void) {
    if (g_vsm_sys) { g_vsm_sys->Shutdown(); g_vsm_sys.reset(); }
}

extern "C" uint32_t dse_vsm_register_light(uint32_t light_id, int is_directional,
                                         float dx, float dy, float dz) {
    if (!g_vsm_sys) return 0;
    dse::render::ShadowLightInfo info;
    info.light_id = light_id;
    info.is_directional = (is_directional != 0);
    info.direction = glm::vec3(dx, dy, dz);
    return g_vsm_sys->RegisterLight(info);
}

extern "C" void dse_vsm_unregister_light(uint32_t light_id) {
    if (g_vsm_sys) g_vsm_sys->UnregisterLight(light_id);
}

extern "C" void dse_vsm_begin_frame(uint32_t frame, float cam_x, float cam_y, float cam_z) {
    if (g_vsm_sys) g_vsm_sys->BeginFrame(frame, glm::vec3(cam_x, cam_y, cam_z));
}

extern "C" void dse_vsm_end_frame(void) {
    if (g_vsm_sys) g_vsm_sys->EndFrame();
}

extern "C" void dse_vsm_invalidate(uint32_t light_id,
                                  float min_x, float min_y, float min_z,
                                  float max_x, float max_y, float max_z) {
    if (g_vsm_sys) g_vsm_sys->InvalidateRegion(light_id, glm::vec3(min_x, min_y, min_z),
                                               glm::vec3(max_x, max_y, max_z));
}

extern "C" void dse_vsm_mark_page_rendered(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id) {
    if (g_vsm_sys) g_vsm_sys->MarkPageRendered(vx, vy, mip, light_id);
}

extern "C" int dse_vsm_get_pages_to_render(void) {
    if (!g_vsm_sys) return 0;
    return static_cast<int>(g_vsm_sys->GetPagesToRender().size());
}

extern "C" int dse_vsm_lookup_page(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id,
                                   uint32_t* out_px, uint32_t* out_py) {
    if (!g_vsm_sys) return 0;
    uint32_t px, py;
    bool found = g_vsm_sys->LookupPage(vx, vy, mip, light_id, px, py);
    if (found) {
        if (out_px) *out_px = px;
        if (out_py) *out_py = py;
        return 1;
    }
    return 0;
}

extern "C" void dse_vsm_get_stats(int* out_total, int* out_mapped, int* out_dirty,
                                 int* out_rendered, int* out_cache_hit, int* out_pool_usage) {
    if (!g_vsm_sys) {
        if (out_total) *out_total = 0;
        if (out_mapped) *out_mapped = 0;
        if (out_dirty) *out_dirty = 0;
        if (out_rendered) *out_rendered = 0;
        if (out_cache_hit) *out_cache_hit = 0;
        if (out_pool_usage) *out_pool_usage = 0;
        return;
    }
    auto stats = g_vsm_sys->GetStats();
    if (out_total) *out_total = stats.total_pages;
    if (out_mapped) *out_mapped = stats.mapped_pages;
    if (out_dirty) *out_dirty = stats.dirty_pages;
    if (out_rendered) *out_rendered = stats.rendered_this_frame;
    if (out_cache_hit) *out_cache_hit = stats.cache_hit_rate_percent;
    if (out_pool_usage) *out_pool_usage = stats.physical_pool_usage_percent;
}

extern "C" int dse_vsm_get_clipmap_levels(void) {
    return g_vsm_sys ? g_vsm_sys->GetConfig().clipmap_levels : 0;
}

// --- EQS ---

extern "C" int dse_eqs_init(void) {
    g_eqs_sys = std::make_unique<dse::ai::EQSSystem>();
    g_eqs_sys->Init();
    return 1;
}

extern "C" void dse_eqs_shutdown(void) {
    if (g_eqs_sys) { g_eqs_sys->Shutdown(); g_eqs_sys.reset(); }
}

extern "C" uint32_t dse_eqs_create_template(const char* name) {
    if (!g_eqs_sys) return 0;
    return g_eqs_sys->CreateTemplate(name ? name : "");
}

extern "C" void dse_eqs_destroy_template(uint32_t tmpl) {
    if (g_eqs_sys) g_eqs_sys->DestroyTemplate(tmpl);
}

extern "C" void dse_eqs_set_generator(uint32_t tmpl, int type, float radius, float spacing, int max_points) {
    if (!g_eqs_sys) return;
    dse::ai::GeneratorConfig cfg;
    cfg.type = static_cast<dse::ai::GeneratorType>(type);
    cfg.radius = radius;
    cfg.spacing = spacing;
    cfg.max_points = max_points;
    g_eqs_sys->SetGenerator(tmpl, cfg);
}

extern "C" void dse_eqs_add_scorer(uint32_t tmpl, int type, float weight, int invert, float max_value) {
    if (!g_eqs_sys) return;
    dse::ai::ScorerConfig cfg;
    cfg.type = static_cast<dse::ai::ScorerType>(type);
    cfg.weight = weight;
    cfg.invert = (invert != 0);
    cfg.max_value = max_value;
    g_eqs_sys->AddScorer(tmpl, cfg);
}

extern "C" void dse_eqs_clear_scorers(uint32_t tmpl) {
    if (g_eqs_sys) g_eqs_sys->ClearScorers(tmpl);
}

extern "C" void dse_eqs_set_combine_mode(uint32_t tmpl, int mode) {
    if (g_eqs_sys) g_eqs_sys->SetCombineMode(tmpl, static_cast<dse::ai::CombineMode>(mode));
}

extern "C" void dse_eqs_set_max_results(uint32_t tmpl, uint32_t max_results) {
    if (g_eqs_sys) g_eqs_sys->SetMaxResults(tmpl, max_results);
}

extern "C" void dse_eqs_execute(uint32_t tmpl, float x, float y, float z, float* out_result) {
    if (!g_eqs_sys || !out_result) return;
    auto result = g_eqs_sys->Execute(tmpl, glm::vec3(x, y, z));
    out_result[0] = result.best_position.x;
    out_result[1] = result.best_position.y;
    out_result[2] = result.best_position.z;
    out_result[3] = result.best_score;
    out_result[4] = static_cast<float>(result.total_generated);
    out_result[5] = static_cast<float>(result.valid_count);
    out_result[6] = result.query_time_ms;
}

extern "C" int dse_eqs_get_template_count(void) {
    return g_eqs_sys ? g_eqs_sys->GetTemplateCount() : 0;
}

extern "C" void dse_eqs_execute_at(uint32_t tmpl, float px, float py, float pz,
                                  float cx, float cy, float cz, float* out_result) {
    if (!g_eqs_sys || !out_result) return;
    auto result = g_eqs_sys->ExecuteAt(tmpl, glm::vec3(px, py, pz), glm::vec3(cx, cy, cz));
    out_result[0] = result.best_position.x;
    out_result[1] = result.best_position.y;
    out_result[2] = result.best_position.z;
    out_result[3] = result.best_score;
    out_result[4] = static_cast<float>(result.valid_count);
}

// --- Distribution ---

extern "C" int dse_dist_init(float cell_size, int max_downloads, const char* cdn_url) {
    dse::assets::DistributionConfig cfg;
    cfg.cell_size = cell_size > 0 ? cell_size : 512.0f;
    cfg.max_concurrent_downloads = max_downloads > 0 ? max_downloads : 4;
    if (cdn_url) cfg.cdn_base_url = cdn_url;
    g_dist_sys = std::make_unique<dse::assets::AssetDistribution>();
    g_dist_sys->Init(cfg);
    return 1;
}

extern "C" void dse_dist_shutdown(void) {
    if (g_dist_sys) { g_dist_sys->Shutdown(); g_dist_sys.reset(); }
}

extern "C" int dse_dist_load_manifest(const char* path) {
    if (!g_dist_sys || !path) return 0;
    return g_dist_sys->LoadManifest(path) ? 1 : 0;
}

extern "C" int dse_dist_save_manifest(const char* path) {
    if (!g_dist_sys || !path) return 0;
    return g_dist_sys->SaveManifest(path) ? 1 : 0;
}

extern "C" int dse_dist_package_cell(int cx, int cz, int lod, const char* const* assets, int count) {
    if (!g_dist_sys) return 0;
    std::vector<std::string> v;
    for (int i = 0; i < count; ++i) if (assets[i]) v.emplace_back(assets[i]);
    return g_dist_sys->PackageCell(cx, cz, lod, v);
}

extern "C" void dse_dist_request_download(const char* package) {
    if (g_dist_sys && package) g_dist_sys->RequestDownload(package);
}

extern "C" void dse_dist_cancel_download(const char* package) {
    if (g_dist_sys && package) g_dist_sys->CancelDownload(package);
}

extern "C" void dse_dist_update_priorities(float x, float y, float z) {
    if (g_dist_sys) g_dist_sys->UpdatePriorities(glm::vec3(x, y, z));
}

extern "C" void dse_dist_tick(float dt) {
    if (g_dist_sys) g_dist_sys->Tick(dt);
}

extern "C" int dse_dist_is_installed(const char* package) {
    if (!g_dist_sys || !package) return 0;
    return g_dist_sys->IsPackageInstalled(package) ? 1 : 0;
}

extern "C" void dse_dist_get_stats(int* out_total, int* out_installed, int* out_downloading,
                                   int* out_pending, double* out_downloaded_bytes, double* out_speed_bps) {
    if (!g_dist_sys) {
        if (out_total) *out_total = 0;
        if (out_installed) *out_installed = 0;
        if (out_downloading) *out_downloading = 0;
        if (out_pending) *out_pending = 0;
        if (out_downloaded_bytes) *out_downloaded_bytes = 0;
        if (out_speed_bps) *out_speed_bps = 0;
        return;
    }
    auto stats = g_dist_sys->GetStats();
    if (out_total) *out_total = stats.total_packages;
    if (out_installed) *out_installed = stats.installed_packages;
    if (out_downloading) *out_downloading = stats.downloading_packages;
    if (out_pending) *out_pending = stats.pending_packages;
    if (out_downloaded_bytes) *out_downloaded_bytes = static_cast<double>(stats.total_downloaded_bytes);
    if (out_speed_bps) *out_speed_bps = stats.download_speed_bps;
}

extern "C" int dse_dist_get_missing(float x, float y, float z, float radius,
                                    char* out_buf, int buf_cap) {
    if (!g_dist_sys || !out_buf || buf_cap <= 0) return 0;
    auto missing = g_dist_sys->GetMissingPackages(glm::vec3(x, y, z), radius);
    int offset = 0;
    for (const auto& name : missing) {
        int len = static_cast<int>(name.size());
        if (offset + len + 1 >= buf_cap) break;
        memcpy(out_buf + offset, name.c_str(), len);
        offset += len;
        out_buf[offset++] = '\0';
    }
    if (offset < buf_cap) out_buf[offset] = '\0';
    return static_cast<int>(missing.size());
}

extern "C" int dse_dist_verify(const char* package) {
    if (!g_dist_sys || !package) return 0;
    return g_dist_sys->VerifyPackage(package) ? 1 : 0;
}

extern "C" uint64_t dse_dist_get_disk_usage(void) {
    return g_dist_sys ? g_dist_sys->GetDiskUsage() : 0;
}
