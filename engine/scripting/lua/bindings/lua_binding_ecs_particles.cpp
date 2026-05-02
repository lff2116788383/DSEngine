/**
 * @file lua_binding_ecs_particles.cpp
 * @brief ECS Lua 绑定 — 粒子系统（2D/3D）+ GameplayTuning 调参
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_particle.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

// ============================================================
// 3D 粒子系统
// ============================================================

int L_EcsAddParticleSystem3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int max_particles = helper::OptInt(L, 2, 1000);
    float emission_rate = helper::OptFloat(L, 3, 100.0f);

    auto& ps = world->registry().emplace_or_replace<ParticleSystem3DComponent>(e);
    ps.max_particles = max_particles;
    ps.emission_rate = emission_rate;
    return 0;
}

int L_EcsSetParticleSystem3DParams(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* ps = helper::TryGetComponent<ParticleSystem3DComponent>(*world, e);
    if (!ps) return 0;
    ps->start_life_min = helper::OptFloat(L, 2, ps->start_life_min);
    ps->start_life_max = helper::OptFloat(L, 3, ps->start_life_max);
    ps->start_size_min = helper::OptFloat(L, 4, ps->start_size_min);
    ps->start_size_max = helper::OptFloat(L, 5, ps->start_size_max);
    ps->start_speed_min = helper::OptFloat(L, 6, ps->start_speed_min);
    ps->start_speed_max = helper::OptFloat(L, 7, ps->start_speed_max);
    ps->start_color = glm::vec4(
        helper::OptFloat(L, 8, ps->start_color.r),
        helper::OptFloat(L, 9, ps->start_color.g),
        helper::OptFloat(L, 10, ps->start_color.b),
        helper::OptFloat(L, 11, ps->start_color.a));
    ps->gravity = glm::vec3(
        helper::OptFloat(L, 12, ps->gravity.x),
        helper::OptFloat(L, 13, ps->gravity.y),
        helper::OptFloat(L, 14, ps->gravity.z));
    ps->texture_path = luaL_optstring(L, 15, ps->texture_path.c_str());
    return 0;
}

int L_EcsGetParticleSystem3DState(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    const auto* ps = helper::TryGetComponentConst<ParticleSystem3DComponent>(*world, e);
    if (!ps) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    helper::PushInt(L, static_cast<int>(ps->active_particle_count));
    helper::PushInt(L, ps->max_particles);
    helper::PushFloat(L, ps->emission_rate);
    helper::PushFloat(L, ps->start_life_min);
    helper::PushFloat(L, ps->start_life_max);
    helper::PushFloat(L, ps->start_size_min);
    helper::PushFloat(L, ps->start_size_max);
    helper::PushFloat(L, ps->start_speed_min);
    helper::PushFloat(L, ps->start_speed_max);
    helper::PushVec3(L, ps->gravity);
    helper::PushVec4(L, ps->start_color);
    lua_pushstring(L, ps->texture_path.c_str());
    helper::PushBool(L, ps->enabled);
    helper::PushBool(L, ps->initialized);
    helper::PushInt(L, static_cast<int>(ps->texture_handle));
    return 21;
}

// ============================================================
// 2D 粒子发射器
// ============================================================

int L_EcsAddParticleEmitter(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    unsigned int texture_handle = static_cast<unsigned int>(helper::OptInt(L, 2, 0));
    int max_particles = helper::OptInt(L, 3, 100);
    float emit_rate = helper::OptFloat(L, 4, 10.0f);
    auto& emitter = world->registry().emplace_or_replace<ParticleEmitterComponent>(e);
    emitter.texture_handle = texture_handle;
    emitter.max_particles = max_particles;
    emitter.emit_rate = emit_rate;
    return 0;
}

int L_EcsSetParticleDensity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float density_scale = helper::CheckFloat(L, 2);
    if (density_scale < 0.0f) density_scale = 0.0f;
    auto* emitter = helper::TryGetComponent<ParticleEmitterComponent>(*world, e);
    if (!emitter) return 0;
    emitter->emit_rate_scale = density_scale;
    return 0;
}

int L_EcsParticleBurst(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int burst_count = helper::CheckInt(L, 2);
    if (burst_count < 0) burst_count = 0;
    auto* emitter = helper::TryGetComponent<ParticleEmitterComponent>(*world, e);
    if (!emitter) return 0;
    emitter->pending_burst += burst_count;
    return 0;
}

// ============================================================
// GameplayTuning 调参
// ============================================================

int L_EcsAddGameplayTuning(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    world->registry().emplace_or_replace<GameplayTuningComponent>(e);
    return 0;
}

int L_EcsSetGameplayTuning(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* tuning = helper::TryGetComponent<GameplayTuningComponent>(*world, e);
    if (!tuning) return 0;
    tuning->leaf_min_distance = helper::OptFloat(L, 2, tuning->leaf_min_distance);
    tuning->leaf_move_left = helper::OptFloat(L, 3, tuning->leaf_move_left);
    tuning->leaf_move_right = helper::OptFloat(L, 4, tuning->leaf_move_right);
    tuning->jump_speed_scale = helper::OptFloat(L, 5, tuning->jump_speed_scale);
    tuning->jump_speed_max = helper::OptFloat(L, 6, tuning->jump_speed_max);
    tuning->camera_follow_damping = helper::OptFloat(L, 7, tuning->camera_follow_damping);
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
        // GameplayTuning
        {"add_gameplay_tuning",          L_EcsAddGameplayTuning},
        {"set_gameplay_tuning",          L_EcsSetGameplayTuning},
    });
}

} // namespace dse::runtime::lua_binding
