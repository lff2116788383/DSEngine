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

// ParticleEmitter 密度缩放 setter — extract 中内嵌 clamp
DSE_LUA_COMPONENT_SETTER(ParticleDensity, ParticleEmitterComponent, emit_rate_scale, float, std::max(0.0f, helper::CheckFloat(L, 2)))

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

// set_particle_random(entity, velocity_min_xyz, velocity_max_xyz, life_min, life_max, size_min, size_max)
int L_EcsSetParticleRandom(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* pe = helper::TryGetComponent<ParticleEmitterComponent>(*world, e);
    if (!pe) return 0;
    pe->use_random_params = true;
    pe->velocity_min = glm::vec3(helper::CheckFloat(L,2), helper::CheckFloat(L,3), helper::CheckFloat(L,4));
    pe->velocity_max = glm::vec3(helper::CheckFloat(L,5), helper::CheckFloat(L,6), helper::CheckFloat(L,7));
    pe->life_time_min = helper::OptFloat(L, 8, pe->life_time_min);
    pe->life_time_max = helper::OptFloat(L, 9, pe->life_time_max);
    pe->size_min = helper::OptFloat(L, 10, pe->size_min);
    pe->size_max = helper::OptFloat(L, 11, pe->size_max);
    return 0;
}

// set_particle_size_curve(entity, enabled, start_value, end_value)
int L_EcsSetParticleSizeCurve(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* pe = helper::TryGetComponent<ParticleEmitterComponent>(*world, e);
    if (!pe) return 0;
    pe->size_curve.enabled = helper::CheckBool(L, 2);
    pe->size_curve.start_value = helper::OptFloat(L, 3, pe->size_curve.start_value);
    pe->size_curve.end_value = helper::OptFloat(L, 4, pe->size_curve.end_value);
    return 0;
}

// set_particle_alpha_curve(entity, enabled, start_value, end_value)
int L_EcsSetParticleAlphaCurve(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* pe = helper::TryGetComponent<ParticleEmitterComponent>(*world, e);
    if (!pe) return 0;
    pe->alpha_curve.enabled = helper::CheckBool(L, 2);
    pe->alpha_curve.start_value = helper::OptFloat(L, 3, pe->alpha_curve.start_value);
    pe->alpha_curve.end_value = helper::OptFloat(L, 4, pe->alpha_curve.end_value);
    return 0;
}

// set_particle_speed_curve(entity, enabled, start_value, end_value)
int L_EcsSetParticleSpeedCurve(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* pe = helper::TryGetComponent<ParticleEmitterComponent>(*world, e);
    if (!pe) return 0;
    pe->speed_curve.enabled = helper::CheckBool(L, 2);
    pe->speed_curve.start_value = helper::OptFloat(L, 3, pe->speed_curve.start_value);
    pe->speed_curve.end_value = helper::OptFloat(L, 4, pe->speed_curve.end_value);
    return 0;
}

// set_particle_gravity(entity, gx, gy, gz)
int L_EcsSetParticleGravity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* pe = helper::TryGetComponent<ParticleEmitterComponent>(*world, e);
    if (!pe) return 0;
    pe->gravity = glm::vec3(
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4));
    return 0;
}

// set_particle_collision(entity, enabled, [mode, bounce, friction, life_loss, ground_y])
// mode: 0=None, 1=GroundPlane, 2=Box2D
int L_EcsSetParticleCollision(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* pe = helper::TryGetComponent<ParticleEmitterComponent>(*world, e);
    if (!pe) return 0;
    pe->enable_collision = helper::CheckBool(L, 2);
    pe->collision_mode = static_cast<ParticleCollisionMode>(helper::OptInt(L, 3, static_cast<int>(pe->collision_mode)));
    pe->collision_bounce = helper::OptFloat(L, 4, pe->collision_bounce);
    pe->collision_friction = helper::OptFloat(L, 5, pe->collision_friction);
    pe->collision_life_loss = helper::OptFloat(L, 6, pe->collision_life_loss);
    pe->ground_y = helper::OptFloat(L, 7, pe->ground_y);
    return 0;
}

// set_particle_color_curve(entity, enabled, end_r, end_g, end_b, end_a)
int L_EcsSetParticleColorCurve(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* pe = helper::TryGetComponent<ParticleEmitterComponent>(*world, e);
    if (!pe) return 0;
    pe->use_color_curve = helper::CheckBool(L, 2);
    pe->color_curve_end = glm::vec4(
        helper::OptFloat(L, 3, pe->color_curve_end.r),
        helper::OptFloat(L, 4, pe->color_curve_end.g),
        helper::OptFloat(L, 5, pe->color_curve_end.b),
        helper::OptFloat(L, 6, pe->color_curve_end.a));
    return 0;
}

// set_particle_rotation(entity, rotation_min, rotation_max, angular_velocity_min, angular_velocity_max)
int L_EcsSetParticleRotation(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* pe = helper::TryGetComponent<ParticleEmitterComponent>(*world, e);
    if (!pe) return 0;
    pe->rotation_min = helper::OptFloat(L, 2, pe->rotation_min);
    pe->rotation_max = helper::OptFloat(L, 3, pe->rotation_max);
    pe->angular_velocity_min = helper::OptFloat(L, 4, pe->angular_velocity_min);
    pe->angular_velocity_max = helper::OptFloat(L, 5, pe->angular_velocity_max);
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
