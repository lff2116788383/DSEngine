/**
 * @file dse_api_rendering_fx.cpp
 * @brief DSEngine C ABI - Rendering FX（Steering / LOD / Hair / Pick）
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/components_3d_character.h"
#include <cmath>

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

// get_steering_state 的 Lua 期望签名与 C ABI 不一致，这里按 CODEGEN_GUIDE §6 加零行为变更的薄包装：
// C ABI 用定长输出缓冲（flags[4] / velocity[3] / params[4] / targets[9]），而 Lua 侧契约是 22 个平铺
// 返回值（ok, enabled, seek, flee, arrive, vx, vy, vz, speed, max_vel, max_force, mass, decel_r,
// seek_t[3], flee_t[3], arrive_t[3]，见 samples/lua/3d/3d_steering_behavior.lua 的注释）。
// 旧绑定的缺陷：codegen 定义把 flags/params/targets 声明成标量 → 包装器只分配 1 个 int/float，
// 而 C ABI 会写 out_flags[0..3]、out_params[0..3]、out_targets[0..8] → **栈越界写**，调用即崩/挂死
// （实测 3d_character_third_person 卡死在 dse.ecs.get_steering_state）。这里把缓冲改由本函数持有，
// 调用方只拿标量，越界面消失。
// speed 不在 C ABI 里，由速度向量模长推导（Lua 侧契约要求第 9 个返回值是 speed）。
extern "C" int dse_compat_steering_get_state(uint32_t e,
                                            int* out_enabled, int* out_seek_enabled,
                                            int* out_flee_enabled, int* out_arrive_enabled,
                                            float* out_velocity, float* out_speed,
                                            float* out_max_velocity, float* out_max_force,
                                            float* out_mass, float* out_arrive_decel_radius,
                                            float* out_seek_target, float* out_flee_target,
                                            float* out_arrive_target) {
    int flags[4] = {0, 0, 0, 0};
    float velocity[3] = {0.0f, 0.0f, 0.0f};
    float params[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float targets[9] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    const int ok = dse_steering_get_state(e, flags, velocity, params, targets);
    if (out_enabled)            *out_enabled = flags[0];
    if (out_seek_enabled)       *out_seek_enabled = flags[1];
    if (out_flee_enabled)       *out_flee_enabled = flags[2];
    if (out_arrive_enabled)     *out_arrive_enabled = flags[3];
    if (out_velocity) {
        out_velocity[0] = velocity[0]; out_velocity[1] = velocity[1]; out_velocity[2] = velocity[2];
    }
    if (out_speed) {
        *out_speed = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] +
                               velocity[2] * velocity[2]);
    }
    if (out_max_velocity)       *out_max_velocity = params[0];
    if (out_max_force)          *out_max_force = params[1];
    if (out_mass)               *out_mass = params[2];
    if (out_arrive_decel_radius) *out_arrive_decel_radius = params[3];
    if (out_seek_target) {
        out_seek_target[0] = targets[0]; out_seek_target[1] = targets[1]; out_seek_target[2] = targets[2];
    }
    if (out_flee_target) {
        out_flee_target[0] = targets[3]; out_flee_target[1] = targets[4]; out_flee_target[2] = targets[5];
    }
    if (out_arrive_target) {
        out_arrive_target[0] = targets[6]; out_arrive_target[1] = targets[7]; out_arrive_target[2] = targets[8];
    }
    return ok;
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

