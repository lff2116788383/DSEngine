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
#include "engine/ecs/components_3d_weather.h"
#include "engine/ecs/components_3d_cloth.h"
#include <cstring>
#include "engine/ecs/components_3d_fluid.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/physics/physics3d/i_physics3d_system.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

#ifdef DSE_HAS_PHYSICS3D
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

#endif // DSE_HAS_PHYSICS3D

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

#ifdef DSE_HAS_PHYSICS3D
// ============================================================
// RagdollComponent 绑定（Phase 2 — Task 1）
// ============================================================

/// add_ragdoll(entity, [total_mass, auto_setup, joint_stiffness, joint_damping])
int L_EcsAddRagdoll(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& rd = world->registry().emplace_or_replace<RagdollComponent>(e);
    rd.total_mass = helper::OptFloat(L, 2, 10.0f);
    rd.auto_setup = helper::OptBool(L, 3, true);
    rd.joint_stiffness = helper::OptFloat(L, 4, 0.0f);
    rd.joint_damping = helper::OptFloat(L, 5, 50.0f);
    return 0;
}

/// ragdoll_activate(entity, [impulse_x, impulse_y, impulse_z, point_x, point_y, point_z])
int L_EcsRagdollActivate(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* rd = helper::TryGetComponent<RagdollComponent>(*world, e);
    if (!rd) return 0;
    rd->active = true;
    // 冲量参数记录到组件上，由 RagdollSystem 在下一帧处理
    // （实际激活由 RagdollSystem::Activate 完成，这里只设标志）
    return 0;
}

/// ragdoll_deactivate(entity)
int L_EcsRagdollDeactivate(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* rd = helper::TryGetComponent<RagdollComponent>(*world, e);
    if (!rd) return 0;
    rd->active = false;
    return 0;
}

/// ragdoll_is_active(entity) -> bool
int L_EcsRagdollIsActive(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* rd = helper::TryGetComponentConst<RagdollComponent>(*world, e);
    lua_pushboolean(L, (rd && rd->active) ? 1 : 0);
    return 1;
}

/// set_ragdoll_collision_layer(entity, layer, mask)
int L_EcsSetRagdollCollisionLayer(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* rd = helper::TryGetComponent<RagdollComponent>(*world, e);
    if (!rd) return 0;
    rd->collision_layer = static_cast<uint16_t>(helper::CheckInt(L, 2));
    rd->collision_mask = static_cast<uint16_t>(helper::CheckInt(L, 3));
    return 0;
}

#endif // DSE_HAS_PHYSICS3D

// ============================================================
// SoftBodyComponent 绑定（Phase 2 — Task 2）
// ============================================================

/// add_softbody(entity, [stiffness, iterations, damping, volume_stiffness])
int L_EcsAddSoftBody(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& sb = world->registry().emplace_or_replace<SoftBodyComponent>(e);
    sb.stiffness = helper::OptFloat(L, 2, 0.5f);
    sb.solver_iterations = helper::OptInt(L, 3, 4);
    sb.damping = helper::OptFloat(L, 4, 0.99f);
    sb.volume_stiffness = helper::OptFloat(L, 5, 0.5f);
    return 0;
}

/// softbody_set_gravity(entity, use_gravity, [gravity_scale])
int L_EcsSoftBodySetGravity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* sb = helper::TryGetComponent<SoftBodyComponent>(*world, e);
    if (!sb) return 0;
    sb->use_gravity = helper::CheckBool(L, 2);
    sb->gravity_scale = helper::OptFloat(L, 3, sb->gravity_scale);
    return 0;
}

/// softbody_pin_vertex(entity, vertex_index)
int L_EcsSoftBodyPinVertex(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* sb = helper::TryGetComponent<SoftBodyComponent>(*world, e);
    if (!sb) return 0;
    int idx = helper::CheckInt(L, 2);
    if (idx >= 0 && idx < static_cast<int>(sb->inv_masses.size())) {
        sb->inv_masses[idx] = 0.0f; // 固定点
    }
    return 0;
}

/// softbody_get_particle_count(entity) -> count
int L_EcsSoftBodyGetParticleCount(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushinteger(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* sb = helper::TryGetComponentConst<SoftBodyComponent>(*world, e);
    lua_pushinteger(L, sb ? static_cast<lua_Integer>(sb->positions.size()) : 0);
    return 1;
}

#ifdef DSE_HAS_PHYSICS3D
// ============================================================
// VehicleComponent 绑定（Phase 2 — Task 3）
// ============================================================

/// add_vehicle(entity, [max_engine_force, max_brake_force, max_steer_angle])
int L_EcsAddVehicle(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& v = world->registry().emplace_or_replace<VehicleComponent>(e);
    v.max_engine_force = helper::OptFloat(L, 2, 5000.0f);
    v.max_brake_force = helper::OptFloat(L, 3, 3000.0f);
    v.max_steer_angle = helper::OptFloat(L, 4, 35.0f);
    return 0;
}

/// vehicle_add_wheel(entity, pos_x, pos_y, pos_z, [radius, is_drive, is_steer, susp_stiffness, susp_damping])
int L_EcsVehicleAddWheel(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* v = helper::TryGetComponent<VehicleComponent>(*world, e);
    if (!v) return 0;
    VehicleWheelConfig wheel;
    wheel.position = glm::vec3(
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4));
    wheel.radius = helper::OptFloat(L, 5, 0.3f);
    wheel.is_drive_wheel = helper::OptBool(L, 6, true);
    wheel.is_steer_wheel = helper::OptBool(L, 7, false);
    wheel.suspension_stiffness = helper::OptFloat(L, 8, 30000.0f);
    wheel.suspension_damping = helper::OptFloat(L, 9, 4500.0f);
    v->wheels.push_back(wheel);
    v->initialized = false; // 重新初始化
    return 0;
}

/// vehicle_set_input(entity, throttle, brake, steering)
int L_EcsVehicleSetInput(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* v = helper::TryGetComponent<VehicleComponent>(*world, e);
    if (!v) return 0;
    v->throttle = std::clamp(helper::CheckFloat(L, 2), -1.0f, 1.0f);
    v->brake = std::clamp(helper::CheckFloat(L, 3), 0.0f, 1.0f);
    v->steering = std::clamp(helper::CheckFloat(L, 4), -1.0f, 1.0f);
    return 0;
}

/// vehicle_get_speed(entity) -> speed (m/s)
int L_EcsVehicleGetSpeed(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnumber(L, 0.0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* v = helper::TryGetComponentConst<VehicleComponent>(*world, e);
    lua_pushnumber(L, v ? static_cast<lua_Number>(v->current_speed) : 0.0);
    return 1;
}

/// vehicle_get_wheel_count(entity) -> count
int L_EcsVehicleGetWheelCount(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushinteger(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* v = helper::TryGetComponentConst<VehicleComponent>(*world, e);
    lua_pushinteger(L, v ? static_cast<lua_Integer>(v->wheels.size()) : 0);
    return 1;
}

#endif // DSE_HAS_PHYSICS3D

// ============================================================
// RopeComponent 绑定（Phase 2 — Task 4）
// ============================================================

/// add_rope(entity, [segment_count, segment_length, damping, iterations])
int L_EcsAddRope(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& rope = world->registry().emplace_or_replace<RopeComponent>(e);
    rope.segment_count = helper::OptInt(L, 2, 10);
    rope.segment_length = helper::OptFloat(L, 3, 0.2f);
    rope.damping = helper::OptFloat(L, 4, 0.99f);
    rope.solver_iterations = helper::OptInt(L, 5, 8);
    return 0;
}

/// rope_set_anchors(entity, anchor_a_entity, anchor_b_entity, [off_ax, off_ay, off_az, off_bx, off_by, off_bz])
int L_EcsRopeSetAnchors(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* rope = helper::TryGetComponent<RopeComponent>(*world, e);
    if (!rope) return 0;
    rope->anchor_entity_a = static_cast<uint32_t>(helper::OptInt(L, 2, 0));
    rope->anchor_entity_b = static_cast<uint32_t>(helper::OptInt(L, 3, 0));
    rope->anchor_offset_a = glm::vec3(
        helper::OptFloat(L, 4, 0.0f),
        helper::OptFloat(L, 5, 0.0f),
        helper::OptFloat(L, 6, 0.0f));
    rope->anchor_offset_b = glm::vec3(
        helper::OptFloat(L, 7, 0.0f),
        helper::OptFloat(L, 8, 0.0f),
        helper::OptFloat(L, 9, 0.0f));
    rope->initialized = false; // 重新初始化
    return 0;
}

/// rope_get_positions(entity) -> { {x1,y1,z1}, {x2,y2,z2}, ... }
int L_EcsRopeGetPositions(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_newtable(L); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* rope = helper::TryGetComponentConst<RopeComponent>(*world, e);
    lua_newtable(L);
    if (!rope) return 1;
    for (size_t i = 0; i < rope->positions.size(); ++i) {
        lua_newtable(L);
        lua_pushnumber(L, static_cast<lua_Number>(rope->positions[i].x));
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, static_cast<lua_Number>(rope->positions[i].y));
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, static_cast<lua_Number>(rope->positions[i].z));
        lua_rawseti(L, -2, 3);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

/// rope_set_gravity(entity, use_gravity, [gravity_scale])
int L_EcsRopeSetGravity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* rope = helper::TryGetComponent<RopeComponent>(*world, e);
    if (!rope) return 0;
    rope->use_gravity = helper::CheckBool(L, 2);
    rope->gravity_scale = helper::OptFloat(L, 3, rope->gravity_scale);
    return 0;
}

#ifdef DSE_HAS_PHYSICS3D
// ============================================================
// BuoyancyComponent 绑定（Phase 2 — Task 5）
// ============================================================

/// add_buoyancy(entity, [water_level, buoyancy_force, water_drag, angular_drag, submerge_depth])
int L_EcsAddBuoyancy(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& b = world->registry().emplace_or_replace<BuoyancyComponent>(e);
    b.water_level = helper::OptFloat(L, 2, 0.0f);
    b.buoyancy_force = helper::OptFloat(L, 3, 10.0f);
    b.water_drag = helper::OptFloat(L, 4, 3.0f);
    b.water_angular_drag = helper::OptFloat(L, 5, 1.0f);
    b.submerge_depth = helper::OptFloat(L, 6, 1.0f);
    return 0;
}

/// buoyancy_add_sample_point(entity, offset_x, offset_y, offset_z, [force_scale])
int L_EcsBuoyancyAddSamplePoint(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* b = helper::TryGetComponent<BuoyancyComponent>(*world, e);
    if (!b) return 0;
    BuoyancySamplePoint sp;
    sp.offset = glm::vec3(
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4));
    sp.force_scale = helper::OptFloat(L, 5, 1.0f);
    b->sample_points.push_back(sp);
    return 0;
}

/// buoyancy_set_water_level(entity, water_level)
int L_EcsBuoyancySetWaterLevel(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* b = helper::TryGetComponent<BuoyancyComponent>(*world, e);
    if (!b) return 0;
    b->water_level = helper::CheckFloat(L, 2);
    return 0;
}

/// buoyancy_get_submerge_ratio(entity) -> ratio [0,1]
int L_EcsBuoyancyGetSubmergeRatio(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnumber(L, 0.0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* b = helper::TryGetComponentConst<BuoyancyComponent>(*world, e);
    lua_pushnumber(L, b ? static_cast<lua_Number>(b->submerge_ratio) : 0.0);
    return 1;
}

/// buoyancy_set_use_fluid(entity, use_fluid_system)
int L_EcsBuoyancySetUseFluid(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* b = helper::TryGetComponent<BuoyancyComponent>(*world, e);
    if (!b) return 0;
    b->use_fluid_system = helper::CheckBool(L, 2);
    return 0;
}

#endif // DSE_HAS_PHYSICS3D

// ============================================================
// WeatherComponent 绑定
// ============================================================

/// add_weather(entity, type_str, intensity)
/// type_str: "none" | "rain" | "snow"
int L_EcsAddWeather(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& wc = world->registry().emplace_or_replace<WeatherComponent>(e);
    const char* type_str = luaL_optstring(L, 2, "snow");
    if (std::strcmp(type_str, "rain") == 0)       wc.type = WeatherType::Rain;
    else if (std::strcmp(type_str, "snow") == 0)  wc.type = WeatherType::Snow;
    else                                           wc.type = WeatherType::None;
    wc.intensity = helper::OptFloat(L, 3, 0.5f);
    return 0;
}

/// set_weather(entity, type_str, intensity, wind_x, wind_z)
int L_EcsSetWeather(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* wc = helper::TryGetComponent<WeatherComponent>(*world, e);
    if (!wc) return 0;
    const char* type_str = luaL_optstring(L, 2, nullptr);
    if (type_str) {
        if (std::strcmp(type_str, "rain") == 0)       wc->type = WeatherType::Rain;
        else if (std::strcmp(type_str, "snow") == 0)  wc->type = WeatherType::Snow;
        else                                           wc->type = WeatherType::None;
    }
    wc->intensity   = helper::OptFloat(L, 3, wc->intensity);
    wc->wind_x      = helper::OptFloat(L, 4, wc->wind_x);
    wc->wind_z      = helper::OptFloat(L, 5, wc->wind_z);
    return 0;
}

/// set_weather_spawn(entity, radius, height, max_particles)
int L_EcsSetWeatherSpawn(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* wc = helper::TryGetComponent<WeatherComponent>(*world, e);
    if (!wc) return 0;
    wc->spawn_radius   = helper::OptFloat(L, 2, wc->spawn_radius);
    wc->spawn_height   = helper::OptFloat(L, 3, wc->spawn_height);
    wc->max_particles  = helper::OptInt(L,   4, wc->max_particles);
    return 0;
}

// ============================================================
// SnowCoverComponent 绑定
// ============================================================

/// add_snow_cover(entity)
int L_EcsAddSnowCover(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    world->registry().emplace_or_replace<SnowCoverComponent>(e);
    return 0;
}

/// set_snow_cover(entity, coverage, accumulation_rate, melt_rate)
int L_EcsSetSnowCover(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* sc = helper::TryGetComponent<SnowCoverComponent>(*world, e);
    if (!sc) return 0;
    sc->coverage          = helper::OptFloat(L, 2, sc->coverage);
    sc->accumulation_rate = helper::OptFloat(L, 3, sc->accumulation_rate);
    sc->melt_rate         = helper::OptFloat(L, 4, sc->melt_rate);
    return 0;
}

/// set_snow_appearance(entity, albedo_r, albedo_g, albedo_b, roughness, threshold, sharpness)
int L_EcsSetSnowAppearance(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* sc = helper::TryGetComponent<SnowCoverComponent>(*world, e);
    if (!sc) return 0;
    sc->snow_albedo.r    = helper::OptFloat(L, 2, sc->snow_albedo.r);
    sc->snow_albedo.g    = helper::OptFloat(L, 3, sc->snow_albedo.g);
    sc->snow_albedo.b    = helper::OptFloat(L, 4, sc->snow_albedo.b);
    sc->snow_roughness   = helper::OptFloat(L, 5, sc->snow_roughness);
    sc->normal_threshold = helper::OptFloat(L, 6, sc->normal_threshold);
    sc->edge_sharpness   = helper::OptFloat(L, 7, sc->edge_sharpness);
    return 0;
}

// ============================================================
// AtmosphereComponent 绑定
// ============================================================

/// add_atmosphere(entity)
int L_EcsAddAtmosphere(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    world->registry().emplace_or_replace<AtmosphereComponent>(e);
    return 0;
}

/// set_atmosphere_params(entity, planet_radius, atmosphere_height, sun_disk_angle)
int L_EcsSetAtmosphereParams(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* atm = helper::TryGetComponent<AtmosphereComponent>(*world, e);
    if (!atm) return 0;
    atm->planet_radius     = helper::OptFloat(L, 2, atm->planet_radius);
    atm->atmosphere_height = helper::OptFloat(L, 3, atm->atmosphere_height);
    atm->sun_disk_angle    = helper::OptFloat(L, 4, atm->sun_disk_angle);
    return 0;
}

/// set_atmosphere_rayleigh(entity, coeff_r, coeff_g, coeff_b, scale_height)
int L_EcsSetAtmosphereRayleigh(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* atm = helper::TryGetComponent<AtmosphereComponent>(*world, e);
    if (!atm) return 0;
    atm->rayleigh_coeff = glm::vec3(
        helper::OptFloat(L, 2, atm->rayleigh_coeff.x),
        helper::OptFloat(L, 3, atm->rayleigh_coeff.y),
        helper::OptFloat(L, 4, atm->rayleigh_coeff.z));
    atm->rayleigh_scale_height = helper::OptFloat(L, 5, atm->rayleigh_scale_height);
    return 0;
}

/// set_atmosphere_mie(entity, coeff, scale_height, g)
int L_EcsSetAtmosphereMie(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* atm = helper::TryGetComponent<AtmosphereComponent>(*world, e);
    if (!atm) return 0;
    atm->mie_coeff        = helper::OptFloat(L, 2, atm->mie_coeff);
    atm->mie_scale_height = helper::OptFloat(L, 3, atm->mie_scale_height);
    atm->mie_g            = helper::OptFloat(L, 4, atm->mie_g);
    return 0;
}

/// set_atmosphere_sun_intensity(entity, r, g, b)
int L_EcsSetAtmosphereSunIntensity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* atm = helper::TryGetComponent<AtmosphereComponent>(*world, e);
    if (!atm) return 0;
    atm->sun_intensity = glm::vec3(
        helper::OptFloat(L, 2, atm->sun_intensity.x),
        helper::OptFloat(L, 3, atm->sun_intensity.y),
        helper::OptFloat(L, 4, atm->sun_intensity.z));
    return 0;
}

// ============================================================
// DayNightCycleComponent 绑定
// ============================================================

/// add_day_night_cycle(entity, [time_of_day, auto_advance, time_speed])
int L_EcsAddDayNightCycle(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& dnc = world->registry().emplace_or_replace<DayNightCycleComponent>(e);
    dnc.time_of_day  = helper::OptFloat(L, 2, 12.0f);
    dnc.auto_advance = helper::OptBool(L, 3, false);
    dnc.time_speed   = helper::OptFloat(L, 4, 1.0f);
    return 0;
}

/// set_day_night_time(entity, time_of_day)
int L_EcsSetDayNightTime(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* dnc = helper::TryGetComponent<DayNightCycleComponent>(*world, e);
    if (!dnc) return 0;
    dnc->time_of_day = helper::CheckFloat(L, 2);
    return 0;
}

/// get_day_night_time(entity) -> time_of_day
int L_EcsGetDayNightTime(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const auto* dnc = helper::TryGetComponentConst<DayNightCycleComponent>(*world, e);
    if (!dnc) { lua_pushnumber(L, 0); return 1; }
    lua_pushnumber(L, dnc->time_of_day);
    return 1;
}

/// set_day_night_speed(entity, speed)
int L_EcsSetDayNightSpeed(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* dnc = helper::TryGetComponent<DayNightCycleComponent>(*world, e);
    if (!dnc) return 0;
    dnc->time_speed = helper::CheckFloat(L, 2);
    return 0;
}

/// set_day_night_auto_advance(entity, enabled)
int L_EcsSetDayNightAutoAdvance(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* dnc = helper::TryGetComponent<DayNightCycleComponent>(*world, e);
    if (!dnc) return 0;
    dnc->auto_advance = helper::CheckBool(L, 2);
    return 0;
}

/// set_day_night_location(entity, latitude, longitude, day_of_year)
int L_EcsSetDayNightLocation(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* dnc = helper::TryGetComponent<DayNightCycleComponent>(*world, e);
    if (!dnc) return 0;
    dnc->latitude    = helper::OptFloat(L, 2, dnc->latitude);
    dnc->longitude   = helper::OptFloat(L, 3, dnc->longitude);
    dnc->day_of_year = helper::OptInt(L, 4, dnc->day_of_year);
    return 0;
}

/// get_sun_elevation(entity) -> degrees
int L_EcsGetSunElevation(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const auto* dnc = helper::TryGetComponentConst<DayNightCycleComponent>(*world, e);
    if (!dnc) { lua_pushnumber(L, 0); return 1; }
    lua_pushnumber(L, dnc->sun_elevation_);
    return 1;
}

/// get_sun_direction(entity) -> x, y, z
int L_EcsGetSunDirection(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const auto* dnc = helper::TryGetComponentConst<DayNightCycleComponent>(*world, e);
    if (!dnc) { lua_pushnumber(L, 0); lua_pushnumber(L, -1); lua_pushnumber(L, 0); return 3; }
    lua_pushnumber(L, dnc->sun_direction_.x);
    lua_pushnumber(L, dnc->sun_direction_.y);
    lua_pushnumber(L, dnc->sun_direction_.z);
    return 3;
}

// ============================================================
// VolumetricCloudComponent 绑定
// ============================================================

/// add_volumetric_cloud(entity)
int L_EcsAddVolumetricCloud(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    world->registry().emplace_or_replace<VolumetricCloudComponent>(e);
    return 0;
}

/// set_cloud_layer(entity, bottom, top, coverage, density)
int L_EcsSetCloudLayer(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* vc = helper::TryGetComponent<VolumetricCloudComponent>(*world, e);
    if (!vc) return 0;
    vc->cloud_bottom = helper::OptFloat(L, 2, vc->cloud_bottom);
    vc->cloud_top    = helper::OptFloat(L, 3, vc->cloud_top);
    vc->coverage     = helper::OptFloat(L, 4, vc->coverage);
    vc->density      = helper::OptFloat(L, 5, vc->density);
    return 0;
}

/// set_cloud_wind(entity, dir_x, dir_y, speed)
int L_EcsSetCloudWind(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* vc = helper::TryGetComponent<VolumetricCloudComponent>(*world, e);
    if (!vc) return 0;
    vc->wind_direction = glm::vec2(
        helper::OptFloat(L, 2, vc->wind_direction.x),
        helper::OptFloat(L, 3, vc->wind_direction.y));
    vc->wind_speed = helper::OptFloat(L, 4, vc->wind_speed);
    return 0;
}

} // namespace

void RegisterEcsGameplay3DBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
#ifdef DSE_HAS_PHYSICS3D
        // 破碎
        {"add_fracture",              L_EcsAddFracture},
        {"set_fracture_params",       L_EcsSetFractureParams},
        {"fracture_apply_damage",     L_EcsFractureApplyDamage},
        {"fracture_trigger",          L_EcsFractureTrigger},
        {"fracture_is_fractured",     L_EcsFractureIsFractured},
#endif
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
#ifdef DSE_HAS_PHYSICS3D
        // 布娃娃（Phase 2）
        {"add_ragdoll",               L_EcsAddRagdoll},
        {"ragdoll_activate",          L_EcsRagdollActivate},
        {"ragdoll_deactivate",        L_EcsRagdollDeactivate},
        {"ragdoll_is_active",         L_EcsRagdollIsActive},
        {"set_ragdoll_collision_layer", L_EcsSetRagdollCollisionLayer},
#endif
        // 软体（Phase 2）
        {"add_softbody",              L_EcsAddSoftBody},
        {"softbody_set_gravity",      L_EcsSoftBodySetGravity},
        {"softbody_pin_vertex",       L_EcsSoftBodyPinVertex},
        {"softbody_get_particle_count", L_EcsSoftBodyGetParticleCount},
#ifdef DSE_HAS_PHYSICS3D
        // 车辆（Phase 2）
        {"add_vehicle",               L_EcsAddVehicle},
        {"vehicle_add_wheel",         L_EcsVehicleAddWheel},
        {"vehicle_set_input",         L_EcsVehicleSetInput},
        {"vehicle_get_speed",         L_EcsVehicleGetSpeed},
        {"vehicle_get_wheel_count",   L_EcsVehicleGetWheelCount},
#endif
        // 绳索（Phase 2）
        {"add_rope",                  L_EcsAddRope},
        {"rope_set_anchors",          L_EcsRopeSetAnchors},
        {"rope_get_positions",        L_EcsRopeGetPositions},
        {"rope_set_gravity",          L_EcsRopeSetGravity},
#ifdef DSE_HAS_PHYSICS3D
        // 浮力（Phase 2）
        {"add_buoyancy",              L_EcsAddBuoyancy},
        {"buoyancy_add_sample_point", L_EcsBuoyancyAddSamplePoint},
        {"buoyancy_set_water_level",  L_EcsBuoyancySetWaterLevel},
        {"buoyancy_get_submerge_ratio", L_EcsBuoyancyGetSubmergeRatio},
        {"buoyancy_set_use_fluid",    L_EcsBuoyancySetUseFluid},
#endif
        // 大气天空
        {"add_atmosphere",             L_EcsAddAtmosphere},
        {"set_atmosphere_params",      L_EcsSetAtmosphereParams},
        {"set_atmosphere_rayleigh",    L_EcsSetAtmosphereRayleigh},
        {"set_atmosphere_mie",         L_EcsSetAtmosphereMie},
        {"set_atmosphere_sun_intensity", L_EcsSetAtmosphereSunIntensity},
        // 昼夜循环
        {"add_day_night_cycle",        L_EcsAddDayNightCycle},
        {"set_day_night_time",         L_EcsSetDayNightTime},
        {"get_day_night_time",         L_EcsGetDayNightTime},
        {"set_day_night_speed",        L_EcsSetDayNightSpeed},
        {"set_day_night_auto_advance", L_EcsSetDayNightAutoAdvance},
        {"set_day_night_location",     L_EcsSetDayNightLocation},
        {"get_sun_elevation",          L_EcsGetSunElevation},
        {"get_sun_direction",          L_EcsGetSunDirection},
        // 体积云（占位，VolumetricCloudPass 后续实现）
        {"add_volumetric_cloud",       L_EcsAddVolumetricCloud},
        {"set_cloud_layer",            L_EcsSetCloudLayer},
        {"set_cloud_wind",             L_EcsSetCloudWind},
        // 天气系统
        {"add_weather",                L_EcsAddWeather},
        {"set_weather",                L_EcsSetWeather},
        {"set_weather_spawn",          L_EcsSetWeatherSpawn},
        // 雪地系统
        {"add_snow_cover",             L_EcsAddSnowCover},
        {"set_snow_cover",             L_EcsSetSnowCover},
        {"set_snow_appearance",        L_EcsSetSnowAppearance},
    });
}

} // namespace dse::runtime::lua_binding
