/**
 * @file lua_binding_ecs_particles.cpp
 * @brief ECS Lua 绑定 — 粒子系统（2D/3D）+ GameplayTuning 调参。薄包装委托至 C ABI（dse_particle*）。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
#include <cmath>
#include <algorithm>
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(Entity e) { return static_cast<uint32_t>(static_cast<entt::id_type>(e)); }

/// 可选浮点参数：缺省时返回 NaN（C ABI 哨兵 = 保持当前值）。
inline float OptNan(lua_State* L, int i) {
    return lua_isnoneornil(L, i) ? NAN : helper::CheckFloat(L, i);
}

// ============================================================
// 3D 粒子系统
// ============================================================

int L_EcsAddParticleSystem3D(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int max_particles = helper::OptInt(L, 2, 1000);
    float emission_rate = helper::OptFloat(L, 3, 100.0f);
    dse_particle_system_3d_add(EID(e), max_particles, emission_rate);
    return 0;
}

int L_EcsSetParticleSystem3DParams(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* tex = (lua_gettop(L) >= 15 && lua_isstring(L, 15)) ? lua_tostring(L, 15) : nullptr;
    dse_particle_system_3d_set_params(EID(e),
        OptNan(L, 2), OptNan(L, 3),
        OptNan(L, 4), OptNan(L, 5),
        OptNan(L, 6), OptNan(L, 7),
        OptNan(L, 8), OptNan(L, 9), OptNan(L, 10), OptNan(L, 11),
        OptNan(L, 12), OptNan(L, 13), OptNan(L, 14),
        tex);
    return 0;
}

int L_EcsGetParticleSystem3DState(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int active = 0, max_particles = 0, enabled = 0, initialized = 0;
    float emission_rate = 0.0f;
    float life[2] = {0.0f, 0.0f}, size[2] = {0.0f, 0.0f}, speed[2] = {0.0f, 0.0f};
    float gravity[3] = {0.0f, 0.0f, 0.0f}, color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    char tex[512] = {0};
    uint32_t texture_handle = 0;
    int found = dse_particle_system_3d_get_state(EID(e), &active, &max_particles,
                                                 &emission_rate, life, size, speed,
                                                 gravity, color, tex, static_cast<int>(sizeof(tex)),
                                                 &enabled, &initialized, &texture_handle);
    if (!found) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    helper::PushInt(L, active);
    helper::PushInt(L, max_particles);
    helper::PushFloat(L, emission_rate);
    helper::PushFloat(L, life[0]);
    helper::PushFloat(L, life[1]);
    helper::PushFloat(L, size[0]);
    helper::PushFloat(L, size[1]);
    helper::PushFloat(L, speed[0]);
    helper::PushFloat(L, speed[1]);
    helper::PushVec3(L, glm::vec3(gravity[0], gravity[1], gravity[2]));
    helper::PushVec4(L, glm::vec4(color[0], color[1], color[2], color[3]));
    lua_pushstring(L, tex);
    helper::PushBool(L, enabled != 0);
    helper::PushBool(L, initialized != 0);
    helper::PushInt(L, static_cast<int>(texture_handle));
    return 21;
}

// ============================================================
// 2D 粒子发射器
// ============================================================

int L_EcsAddParticleEmitter(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    uint32_t texture_handle = static_cast<uint32_t>(helper::OptInt(L, 2, 0));
    int max_particles = helper::OptInt(L, 3, 100);
    float emit_rate = helper::OptFloat(L, 4, 10.0f);
    dse_particle_emitter_add(EID(e), texture_handle, max_particles, emit_rate);
    return 0;
}

int L_EcsSetParticleDensity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_particle_set_density(EID(e), std::max(0.0f, helper::CheckFloat(L, 2)));
    return 0;
}

int L_EcsParticleBurst(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int burst_count = helper::CheckInt(L, 2);
    if (burst_count < 0) burst_count = 0;
    dse_particle_burst(EID(e), burst_count);
    return 0;
}

// ============================================================
// GameplayTuning 调参
// ============================================================

int L_EcsAddGameplayTuning(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_gameplay_tuning_add(EID(e));
    return 0;
}

int L_EcsSetGameplayTuning(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_gameplay_tuning_set(EID(e), OptNan(L, 2), OptNan(L, 3), OptNan(L, 4),
                            OptNan(L, 5), OptNan(L, 6), OptNan(L, 7));
    return 0;
}

// set_particle_random(entity, velocity_min_xyz, velocity_max_xyz, life_min, life_max, size_min, size_max)
int L_EcsSetParticleRandom(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_particle_set_random(EID(e),
        helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4),
        helper::CheckFloat(L, 5), helper::CheckFloat(L, 6), helper::CheckFloat(L, 7),
        OptNan(L, 8), OptNan(L, 9), OptNan(L, 10), OptNan(L, 11));
    return 0;
}

// set_particle_size_curve(entity, enabled, start_value, end_value)
int L_EcsSetParticleSizeCurve(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_particle_set_size_curve(EID(e), helper::CheckBool(L, 2) ? 1 : 0, OptNan(L, 3), OptNan(L, 4));
    return 0;
}

// set_particle_alpha_curve(entity, enabled, start_value, end_value)
int L_EcsSetParticleAlphaCurve(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_particle_set_alpha_curve(EID(e), helper::CheckBool(L, 2) ? 1 : 0, OptNan(L, 3), OptNan(L, 4));
    return 0;
}

// set_particle_speed_curve(entity, enabled, start_value, end_value)
int L_EcsSetParticleSpeedCurve(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_particle_set_speed_curve(EID(e), helper::CheckBool(L, 2) ? 1 : 0, OptNan(L, 3), OptNan(L, 4));
    return 0;
}

// set_particle_gravity(entity, gx, gy, gz)
int L_EcsSetParticleGravity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_particle_set_gravity(EID(e), helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4));
    return 0;
}

// set_particle_collision(entity, enabled, [mode, bounce, friction, life_loss, ground_y])
// mode: 0=None, 1=GroundPlane, 2=Box2D；mode<0=保持当前值
int L_EcsSetParticleCollision(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_particle_set_collision(EID(e), helper::CheckBool(L, 2) ? 1 : 0,
                               helper::OptInt(L, 3, -1),
                               OptNan(L, 4), OptNan(L, 5), OptNan(L, 6), OptNan(L, 7));
    return 0;
}

// set_particle_color_curve(entity, enabled, end_r, end_g, end_b, end_a)
int L_EcsSetParticleColorCurve(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_particle_set_color_curve(EID(e), helper::CheckBool(L, 2) ? 1 : 0,
                                 OptNan(L, 3), OptNan(L, 4), OptNan(L, 5), OptNan(L, 6));
    return 0;
}

// set_particle_rotation(entity, rotation_min, rotation_max, angular_velocity_min, angular_velocity_max)
int L_EcsSetParticleRotation(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_particle_set_rotation(EID(e), OptNan(L, 2), OptNan(L, 3), OptNan(L, 4), OptNan(L, 5));
    return 0;
}

} // namespace

void RegisterEcsParticlesBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        // 3D 粒子系统
        {"add_particle_system_3d",       L_EcsAddParticleSystem3D},
        {"set_particle_system_3d_params", L_EcsSetParticleSystem3DParams},
        {"get_particle_system_3d_state", L_EcsGetParticleSystem3DState},
        // 2D 粒子发射器
        {"add_particle_emitter",         L_EcsAddParticleEmitter},
        {"set_particle_density",         L_EcsSetParticleDensity},
        {"particle_burst",               L_EcsParticleBurst},
        {"set_particle_random",           L_EcsSetParticleRandom},
        {"set_particle_size_curve",       L_EcsSetParticleSizeCurve},
        {"set_particle_alpha_curve",      L_EcsSetParticleAlphaCurve},
        {"set_particle_speed_curve",      L_EcsSetParticleSpeedCurve},
        {"set_particle_gravity",          L_EcsSetParticleGravity},
        {"set_particle_collision",        L_EcsSetParticleCollision},
        {"set_particle_color_curve",      L_EcsSetParticleColorCurve},
        {"set_particle_rotation",         L_EcsSetParticleRotation},
        // GameplayTuning
        {"add_gameplay_tuning",          L_EcsAddGameplayTuning},
        {"set_gameplay_tuning",          L_EcsSetGameplayTuning},
    });
}

} // namespace dse::runtime::lua_binding
