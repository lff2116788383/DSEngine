/**
 * @file dse_api_rendering_fx.cpp
 * @brief DSEngine C ABI - Rendering FX（Steering / LOD / Hair / Pick）
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/components_3d_character.h"

using namespace dse;
using namespace dse_api_internal;


extern "C" void dse_steering_add(uint32_t e, float max_velocity, float max_force, float mass) {
    World* world = GW();
    if (!world) return;
    auto& st = world->registry().emplace_or_replace<SteeringComponent>(TE(e));
    st.enabled = true;
    st.max_velocity = max_velocity;
    st.max_force = max_force;
    st.mass = mass;
}

extern "C" int dse_steering_set_target(uint32_t e, int behavior, float x, float y, float z) {
    World* world = GW();
    if (!world) return 0;
    auto* st = world->registry().try_get<SteeringComponent>(TE(e));
    if (!st) return 0;
    switch (behavior) {
        case 0:
            st->seek_enabled = true; st->flee_enabled = false; st->arrive_enabled = false;
            st->seek_target = glm::vec3(x, y, z);
            return 1;
        case 1:
            st->seek_enabled = false; st->flee_enabled = true; st->arrive_enabled = false;
            st->flee_target = glm::vec3(x, y, z);
            return 1;
        case 2:
            st->seek_enabled = false; st->flee_enabled = false; st->arrive_enabled = true;
            st->arrive_target = glm::vec3(x, y, z);
            return 1;
        default:
            return 0;
    }
}

extern "C" int dse_steering_get_state(uint32_t e, int* out_flags, float* out_velocity,
                                      float* out_params, float* out_targets) {
    World* world = GW();
    if (!world) return 0;
    const auto* st = world->registry().try_get<SteeringComponent>(TE(e));
    if (!st) return 0;
    if (out_flags) {
        out_flags[0] = st->enabled ? 1 : 0;
        out_flags[1] = st->seek_enabled ? 1 : 0;
        out_flags[2] = st->flee_enabled ? 1 : 0;
        out_flags[3] = st->arrive_enabled ? 1 : 0;
    }
    if (out_velocity) {
        out_velocity[0] = st->velocity.x; out_velocity[1] = st->velocity.y; out_velocity[2] = st->velocity.z;
    }
    if (out_params) {
        out_params[0] = st->max_velocity; out_params[1] = st->max_force;
        out_params[2] = st->mass; out_params[3] = st->arrive_deceleration_radius;
    }
    if (out_targets) {
        out_targets[0] = st->seek_target.x;   out_targets[1] = st->seek_target.y;   out_targets[2] = st->seek_target.z;
        out_targets[3] = st->flee_target.x;   out_targets[4] = st->flee_target.y;   out_targets[5] = st->flee_target.z;
        out_targets[6] = st->arrive_target.x; out_targets[7] = st->arrive_target.y; out_targets[8] = st->arrive_target.z;
    }
    return 1;
}

extern "C" void dse_lod_add_level(uint32_t e, const char* mesh_path, float screen_size_threshold) {
    World* world = GW();
    if (!world || !mesh_path || !world->registry().valid(TE(e))) return;
    auto& lod = world->registry().get_or_emplace<LODGroupComponent>(TE(e));
    LODLevelConfig level;
    level.mesh_path = mesh_path;
    level.screen_size_threshold = screen_size_threshold;
    lod.levels.push_back(std::move(level));
}

extern "C" void dse_lod_set_scale(uint32_t e, float scale) {
    World* world = GW();
    if (!world) return;
    auto* lod = world->registry().try_get<LODGroupComponent>(TE(e));
    if (lod) lod->global_scale = scale;
}

extern "C" void dse_lod_set_min_screen_size(uint32_t e, float min_size) {
    World* world = GW();
    if (!world || !world->registry().valid(TE(e))) return;
    auto& lod = world->registry().get_or_emplace<LODGroupComponent>(TE(e));
    lod.min_screen_size = min_size;
}

extern "C" void dse_lod_set_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* lod = world->registry().try_get<LODGroupComponent>(TE(e));
    if (lod) lod->enabled = (enabled != 0);
}

extern "C" void dse_hair_add(uint32_t e, const char* asset_path, int num_follow_per_guide) {
    World* world = GW();
    if (!world) return;
    auto& hair = world->registry().emplace_or_replace<HairComponent>(TE(e));
    hair.enabled = true;
    if (asset_path) hair.hair_asset_path = asset_path;
    if (num_follow_per_guide >= 0) hair.num_follow_per_guide = num_follow_per_guide;
}

extern "C" void dse_hair_set_physics(uint32_t e, float damping, float stiffness_local,
                                     float stiffness_global, float gravity) {
    World* world = GW();
    if (!world) return;
    auto* hair = world->registry().try_get<HairComponent>(TE(e));
    if (!hair) return;
    if (!Keep(damping)) hair->damping = damping;
    if (!Keep(stiffness_local)) hair->stiffness_local = stiffness_local;
    if (!Keep(stiffness_global)) hair->stiffness_global = stiffness_global;
    if (!Keep(gravity)) hair->gravity = gravity;
}

extern "C" void dse_hair_set_render(uint32_t e,
                                    float root_r, float root_g, float root_b, float root_a,
                                    float tip_r, float tip_g, float tip_b, float tip_a,
                                    float fiber_radius, float opacity) {
    World* world = GW();
    if (!world) return;
    auto* hair = world->registry().try_get<HairComponent>(TE(e));
    if (!hair) return;
    if (!Keep(root_r) && !Keep(root_g) && !Keep(root_b) && !Keep(root_a))
        hair->root_color = glm::vec4(root_r, root_g, root_b, root_a);
    if (!Keep(tip_r) && !Keep(tip_g) && !Keep(tip_b) && !Keep(tip_a))
        hair->tip_color = glm::vec4(tip_r, tip_g, tip_b, tip_a);
    if (!Keep(fiber_radius)) hair->fiber_radius = fiber_radius;
    if (!Keep(opacity)) hair->opacity = opacity;
}

extern "C" void dse_hair_set_wind_full(uint32_t e, float wx, float wy, float wz, float turbulence) {
    World* world = GW();
    if (!world) return;
    auto* hair = world->registry().try_get<HairComponent>(TE(e));
    if (!hair) return;
    hair->wind = glm::vec3(wx, wy, wz);
    if (!Keep(turbulence)) hair->wind_turbulence = turbulence;
}

// dse_hair_set_enabled 由 dse_api_hair.gen.cpp 提供（codegen 逐字段 setter）

extern "C" void dse_hair_set_lod(uint32_t e, float lod0_distance, float lod1_distance,
                                 float lod2_distance, float cull_distance) {
    World* world = GW();
    if (!world) return;
    auto* hair = world->registry().try_get<HairComponent>(TE(e));
    if (!hair) return;
    if (!Keep(lod0_distance)) hair->lod0_distance = lod0_distance;
    if (!Keep(lod1_distance)) hair->lod1_distance = lod1_distance;
    if (!Keep(lod2_distance)) hair->lod2_distance = lod2_distance;
    if (!Keep(cull_distance)) hair->cull_distance = cull_distance;
}

