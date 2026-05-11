/**
 * @file lua_binding_ecs_gameplay3d.cpp
 * @brief ECS Lua 绑定 — Gameplay3D 物理系统（破碎、布料、流体）
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_fracture.h"
#include "engine/ecs/components_3d_cloth.h"
#include "engine/ecs/components_3d_fluid.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

// ============================================================
// FractureComponent 绑定
// ============================================================

/// add_fracture(entity, [source, fragment_count, break_force, health])
/// source: 0=Prefractured, 1=RuntimeVoronoi
int L_EcsAddFracture(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& fc = world->registry().emplace_or_replace<FractureComponent>(e);
    fc.source = static_cast<FractureSource>(helper::OptInt(L, 2, 1));
    fc.runtime_fragment_count = static_cast<uint32_t>(helper::OptInt(L, 3, 8));
    fc.break_force = helper::OptFloat(L, 4, 1000.0f);
    fc.health = helper::OptFloat(L, 5, 100.0f);
    fc.max_health = fc.health;
    return 0;
}

/// set_fracture_params(entity, explosion_force, fragment_lifetime, fade_duration, fragment_mass_scale)
int L_EcsSetFractureParams(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fc = helper::TryGetComponent<FractureComponent>(*world, e);
    if (!fc) return 0;
    fc->explosion_force = helper::OptFloat(L, 2, fc->explosion_force);
    fc->fragment_lifetime = helper::OptFloat(L, 3, fc->fragment_lifetime);
    fc->fragment_fade_duration = helper::OptFloat(L, 4, fc->fragment_fade_duration);
    fc->fragment_mass_scale = helper::OptFloat(L, 5, fc->fragment_mass_scale);
    return 0;
}

/// fracture_apply_damage(entity, damage, [impact_x, impact_y, impact_z])
int L_EcsFractureApplyDamage(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float damage = helper::CheckFloat(L, 2);
    auto* fc = helper::TryGetComponent<FractureComponent>(*world, e);
    if (!fc) return 0;
    fc->health -= damage;
    if (fc->health <= 0.0f && !fc->is_fractured) {
        fc->fracture_requested = true;
        fc->impact_point = glm::vec3(
            helper::OptFloat(L, 3, 0.0f),
            helper::OptFloat(L, 4, 0.0f),
            helper::OptFloat(L, 5, 0.0f));
    }
    return 0;
}

/// fracture_trigger(entity, [impact_x, impact_y, impact_z])
int L_EcsFractureTrigger(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fc = helper::TryGetComponent<FractureComponent>(*world, e);
    if (!fc || fc->is_fractured) return 0;
    fc->fracture_requested = true;
    fc->impact_point = glm::vec3(
        helper::OptFloat(L, 2, 0.0f),
        helper::OptFloat(L, 3, 0.0f),
        helper::OptFloat(L, 4, 0.0f));
    return 0;
}

/// fracture_is_fractured(entity) -> bool
int L_EcsFractureIsFractured(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* fc = helper::TryGetComponentConst<FractureComponent>(*world, e);
    lua_pushboolean(L, (fc && fc->is_fractured) ? 1 : 0);
    return 1;
}

// ============================================================
// ClothComponent 绑定
// ============================================================

/// add_cloth(entity, [solver_iterations, stiffness, damping, bend_stiffness])
int L_EcsAddCloth(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& cloth = world->registry().emplace_or_replace<ClothComponent>(e);
    cloth.enabled = true;
    cloth.solver_iterations = static_cast<uint32_t>(helper::OptInt(L, 2, 8));
    cloth.stiffness = helper::OptFloat(L, 3, 1.0f);
    cloth.damping = helper::OptFloat(L, 4, 0.01f);
    cloth.bend_stiffness = helper::OptFloat(L, 5, 0.5f);
    return 0;
}

/// set_cloth_wind(entity, wx, wy, wz, [turbulence])
int L_EcsSetClothWind(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* cloth = helper::TryGetComponent<ClothComponent>(*world, e);
    if (!cloth) return 0;
    cloth->wind = glm::vec3(
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4));
    cloth->wind_turbulence = helper::OptFloat(L, 5, cloth->wind_turbulence);
    return 0;
}

/// set_cloth_gravity(entity, gx, gy, gz)
int L_EcsSetClothGravity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* cloth = helper::TryGetComponent<ClothComponent>(*world, e);
    if (!cloth) return 0;
    cloth->gravity = glm::vec3(
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4));
    return 0;
}

/// cloth_pin_vertices(entity, {v1, v2, v3, ...})
int L_EcsClothPinVertices(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* cloth = helper::TryGetComponent<ClothComponent>(*world, e);
    if (!cloth) return 0;
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = static_cast<int>(lua_rawlen(L, 2));
    cloth->pinned_vertices.clear();
    cloth->pinned_vertices.reserve(n);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 2, i);
        cloth->pinned_vertices.push_back(static_cast<uint32_t>(lua_tointeger(L, -1)));
        lua_pop(L, 1);
    }
    return 0;
}

/// cloth_add_sphere_collider(entity, collider_entity, radius)
int L_EcsClothAddSphereCollider(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* cloth = helper::TryGetComponent<ClothComponent>(*world, e);
    if (!cloth) return 0;
    ClothSphereCollider col;
    col.entity_id = static_cast<uint32_t>(helper::CheckEntity(L, 2));
    col.radius = helper::OptFloat(L, 3, 0.5f);
    cloth->sphere_colliders.push_back(col);
    return 0;
}

// ============================================================
// FluidEmitterComponent 绑定
// ============================================================

/// add_fluid_emitter(entity, [shape, emission_rate, particle_lifetime, emit_speed])
/// shape: 0=Point, 1=Sphere, 2=Box
int L_EcsAddFluidEmitter(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& fluid = world->registry().emplace_or_replace<FluidEmitterComponent>(e);
    fluid.enabled = true;
    fluid.shape = static_cast<FluidEmitterShape>(helper::OptInt(L, 2, 0));
    fluid.emission_rate = helper::OptFloat(L, 3, 500.0f);
    fluid.particle_lifetime = helper::OptFloat(L, 4, 3.0f);
    fluid.emit_speed = helper::OptFloat(L, 5, 2.0f);
    return 0;
}

/// set_fluid_physics(entity, viscosity, surface_tension, rest_density, gas_stiffness)
int L_EcsSetFluidPhysics(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fluid = helper::TryGetComponent<FluidEmitterComponent>(*world, e);
    if (!fluid) return 0;
    fluid->viscosity = helper::OptFloat(L, 2, fluid->viscosity);
    fluid->surface_tension = helper::OptFloat(L, 3, fluid->surface_tension);
    fluid->rest_density = helper::OptFloat(L, 4, fluid->rest_density);
    fluid->gas_stiffness = helper::OptFloat(L, 5, fluid->gas_stiffness);
    return 0;
}

/// set_fluid_rendering(entity, r, g, b, a, [refraction, fresnel, specular])
int L_EcsSetFluidRendering(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fluid = helper::TryGetComponent<FluidEmitterComponent>(*world, e);
    if (!fluid) return 0;
    fluid->color = glm::vec4(
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4),
        helper::OptFloat(L, 5, 0.8f));
    fluid->refraction_strength = helper::OptFloat(L, 6, fluid->refraction_strength);
    fluid->fresnel_power = helper::OptFloat(L, 7, fluid->fresnel_power);
    fluid->specular_intensity = helper::OptFloat(L, 8, fluid->specular_intensity);
    return 0;
}

/// set_fluid_emit_direction(entity, dx, dy, dz, [spread])
int L_EcsSetFluidEmitDirection(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fluid = helper::TryGetComponent<FluidEmitterComponent>(*world, e);
    if (!fluid) return 0;
    fluid->emit_direction = glm::normalize(glm::vec3(
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4)));
    fluid->emit_spread = helper::OptFloat(L, 5, fluid->emit_spread);
    return 0;
}

/// set_fluid_floor(entity, floor_y, restitution)
int L_EcsSetFluidFloor(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fluid = helper::TryGetComponent<FluidEmitterComponent>(*world, e);
    if (!fluid) return 0;
    fluid->floor_y = helper::OptFloat(L, 2, fluid->floor_y);
    fluid->collision_restitution = helper::OptFloat(L, 3, fluid->collision_restitution);
    return 0;
}

/// get_fluid_particle_count(entity) -> count
int L_EcsGetFluidParticleCount(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushinteger(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* fluid = helper::TryGetComponentConst<FluidEmitterComponent>(*world, e);
    lua_pushinteger(L, fluid ? static_cast<lua_Integer>(fluid->active_count) : 0);
    return 1;
}

} // namespace

void RegisterEcsGameplay3DBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        // 破碎
        {"add_fracture",              L_EcsAddFracture},
        {"set_fracture_params",       L_EcsSetFractureParams},
        {"fracture_apply_damage",     L_EcsFractureApplyDamage},
        {"fracture_trigger",          L_EcsFractureTrigger},
        {"fracture_is_fractured",     L_EcsFractureIsFractured},
        // 布料
        {"add_cloth",                 L_EcsAddCloth},
        {"set_cloth_wind",            L_EcsSetClothWind},
        {"set_cloth_gravity",         L_EcsSetClothGravity},
        {"cloth_pin_vertices",        L_EcsClothPinVertices},
        {"cloth_add_sphere_collider", L_EcsClothAddSphereCollider},
        // 流体
        {"add_fluid_emitter",         L_EcsAddFluidEmitter},
        {"set_fluid_physics",         L_EcsSetFluidPhysics},
        {"set_fluid_rendering",       L_EcsSetFluidRendering},
        {"set_fluid_emit_direction",  L_EcsSetFluidEmitDirection},
        {"set_fluid_floor",           L_EcsSetFluidFloor},
        {"get_fluid_particle_count",  L_EcsGetFluidParticleCount},
    });
}

} // namespace dse::runtime::lua_binding
