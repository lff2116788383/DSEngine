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
#include "engine/scripting/native_api/dse_api.h"
#include <cmath>
#include <vector>
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(Entity e) { return static_cast<uint32_t>(static_cast<entt::id_type>(e)); }

/// 天气类型字符串 → int 枚举（0=None,1=Rain,2=Snow）。
inline int WeatherTypeFromStr(const char* s) {
    if (!s) return -1;  // 保持
    if (std::strcmp(s, "rain") == 0) return 1;
    if (std::strcmp(s, "snow") == 0) return 2;
    return 0;
}

#ifdef DSE_HAS_PHYSICS3D
// ============================================================
// FractureComponent 绑定
// ============================================================

/// add_fracture(entity, [source, fragment_count, break_force, health])
/// source: 0=Prefractured, 1=RuntimeVoronoi
int L_EcsAddFracture(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_fracture_add(EID(e),
        helper::OptInt(L, 2, 1),
        static_cast<uint32_t>(helper::OptInt(L, 3, 8)),
        helper::OptFloat(L, 4, 1000.0f),
        helper::OptFloat(L, 5, 100.0f));
    return 0;
}

/// set_fracture_params(entity, explosion_force, fragment_lifetime, fade_duration, fragment_mass_scale)
int L_EcsSetFractureParams(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_fracture_set_params(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptFloat(L, 4, NAN),
        helper::OptFloat(L, 5, NAN));
    return 0;
}

/// fracture_apply_damage(entity, damage, [impact_x, impact_y, impact_z])
int L_EcsFractureApplyDamage(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float damage = helper::CheckFloat(L, 2);
    dse_fracture_apply_damage(EID(e), damage,
        helper::OptFloat(L, 3, 0.0f),
        helper::OptFloat(L, 4, 0.0f),
        helper::OptFloat(L, 5, 0.0f));
    return 0;
}

/// fracture_trigger(entity, [impact_x, impact_y, impact_z])
int L_EcsFractureTrigger(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_fracture_trigger(EID(e),
        helper::OptFloat(L, 2, 0.0f),
        helper::OptFloat(L, 3, 0.0f),
        helper::OptFloat(L, 4, 0.0f));
    return 0;
}

/// fracture_is_fractured(entity) -> bool
int L_EcsFractureIsFractured(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    lua_pushboolean(L, dse_fracture_is_fractured(EID(e)));
    return 1;
}

#endif // DSE_HAS_PHYSICS3D

// ============================================================
// ClothComponent 绑定
// ============================================================

/// add_cloth(entity, [solver_iterations, stiffness, damping, bend_stiffness])
int L_EcsAddCloth(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_cloth_add(EID(e),
        static_cast<uint32_t>(helper::OptInt(L, 2, 8)),
        helper::OptFloat(L, 3, 1.0f),
        helper::OptFloat(L, 4, 0.01f),
        helper::OptFloat(L, 5, 0.5f));
    return 0;
}

/// set_cloth_wind(entity, wx, wy, wz, [turbulence])
int L_EcsSetClothWind(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_cloth_set_wind(EID(e),
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4),
        helper::OptFloat(L, 5, NAN));
    return 0;
}

/// set_cloth_gravity(entity, gx, gy, gz)
int L_EcsSetClothGravity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_cloth_set_gravity(EID(e),
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4));
    return 0;
}

/// cloth_pin_vertices(entity, {v1, v2, v3, ...})
int L_EcsClothPinVertices(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = static_cast<int>(lua_rawlen(L, 2));
    std::vector<uint32_t> verts;
    verts.reserve(static_cast<size_t>(n));
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 2, i);
        verts.push_back(static_cast<uint32_t>(lua_tointeger(L, -1)));
        lua_pop(L, 1);
    }
    dse_cloth_pin_vertices(EID(e), verts.data(), static_cast<int>(verts.size()));
    return 0;
}

/// cloth_add_sphere_collider(entity, collider_entity, radius)
int L_EcsClothAddSphereCollider(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    uint32_t collider = EID(helper::CheckEntity(L, 2));
    dse_cloth_add_sphere_collider(EID(e), collider, helper::OptFloat(L, 3, 0.5f));
    return 0;
}

// ============================================================
// FluidEmitterComponent 绑定
// ============================================================

/// add_fluid_emitter(entity, [shape, emission_rate, particle_lifetime, emit_speed])
/// shape: 0=Point, 1=Sphere, 2=Box
int L_EcsAddFluidEmitter(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_fluid_add_emitter(EID(e),
        helper::OptInt(L, 2, 0),
        helper::OptFloat(L, 3, 500.0f),
        helper::OptFloat(L, 4, 3.0f),
        helper::OptFloat(L, 5, 2.0f));
    return 0;
}

/// set_fluid_physics(entity, viscosity, surface_tension, rest_density, gas_stiffness)
int L_EcsSetFluidPhysics(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_fluid_set_physics(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptFloat(L, 4, NAN),
        helper::OptFloat(L, 5, NAN));
    return 0;
}

/// set_fluid_rendering(entity, r, g, b, a, [refraction, fresnel, specular])
int L_EcsSetFluidRendering(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_fluid_set_rendering(EID(e),
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4),
        helper::OptFloat(L, 5, 0.8f),
        helper::OptFloat(L, 6, NAN),
        helper::OptFloat(L, 7, NAN),
        helper::OptFloat(L, 8, NAN));
    return 0;
}

/// set_fluid_emit_direction(entity, dx, dy, dz, [spread])
int L_EcsSetFluidEmitDirection(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_fluid_set_emit_direction(EID(e),
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4),
        helper::OptFloat(L, 5, NAN));
    return 0;
}

/// set_fluid_floor(entity, floor_y, restitution)
int L_EcsSetFluidFloor(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_fluid_set_floor(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN));
    return 0;
}

/// get_fluid_particle_count(entity) -> count
int L_EcsGetFluidParticleCount(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(dse_fluid_get_particle_count(EID(e))));
    return 1;
}

#ifdef DSE_HAS_PHYSICS3D
// ============================================================
// RagdollComponent 绑定（Phase 2 — Task 1）
// ============================================================

/// add_ragdoll(entity, [total_mass, auto_setup, joint_stiffness, joint_damping])
int L_EcsAddRagdoll(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_ragdoll_add(EID(e),
        helper::OptFloat(L, 2, 10.0f),
        helper::OptBool(L, 3, true) ? 1 : 0,
        helper::OptFloat(L, 4, 0.0f),
        helper::OptFloat(L, 5, 50.0f));
    return 0;
}

/// ragdoll_activate(entity, [impulse_x, impulse_y, impulse_z, point_x, point_y, point_z])
int L_EcsRagdollActivate(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_ragdoll_activate(EID(e));
    return 0;
}

/// ragdoll_deactivate(entity)
int L_EcsRagdollDeactivate(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_ragdoll_deactivate(EID(e));
    return 0;
}

/// ragdoll_is_active(entity) -> bool
int L_EcsRagdollIsActive(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    lua_pushboolean(L, dse_ragdoll_is_active(EID(e)));
    return 1;
}

/// set_ragdoll_collision_layer(entity, layer, mask)
int L_EcsSetRagdollCollisionLayer(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_ragdoll_set_collision_layer(EID(e),
        static_cast<uint32_t>(helper::CheckInt(L, 2)),
        static_cast<uint32_t>(helper::CheckInt(L, 3)));
    return 0;
}

#endif // DSE_HAS_PHYSICS3D

// ============================================================
// SoftBodyComponent 绑定（Phase 2 — Task 2）
// ============================================================

/// add_softbody(entity, [stiffness, iterations, damping, volume_stiffness])
int L_EcsAddSoftBody(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_softbody_add(EID(e),
        helper::OptFloat(L, 2, 0.5f),
        helper::OptInt(L, 3, 4),
        helper::OptFloat(L, 4, 0.99f),
        helper::OptFloat(L, 5, 0.5f));
    return 0;
}

/// softbody_set_gravity(entity, use_gravity, [gravity_scale])
int L_EcsSoftBodySetGravity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_softbody_set_gravity(EID(e),
        helper::CheckBool(L, 2) ? 1 : 0,
        helper::OptFloat(L, 3, NAN));  // NaN=保持当前
    return 0;
}

/// softbody_pin_vertex(entity, vertex_index)
int L_EcsSoftBodyPinVertex(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_softbody_pin_vertex(EID(e), helper::CheckInt(L, 2));
    return 0;
}

/// softbody_get_particle_count(entity) -> count
int L_EcsSoftBodyGetParticleCount(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(dse_softbody_get_particle_count(EID(e))));
    return 1;
}

#ifdef DSE_HAS_PHYSICS3D
// ============================================================
// VehicleComponent 绑定（Phase 2 — Task 3）
// ============================================================

/// add_vehicle(entity, [max_engine_force, max_brake_force, max_steer_angle])
int L_EcsAddVehicle(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_vehicle_add(EID(e),
        helper::OptFloat(L, 2, 5000.0f),
        helper::OptFloat(L, 3, 3000.0f),
        helper::OptFloat(L, 4, 35.0f));
    return 0;
}

/// vehicle_add_wheel(entity, pos_x, pos_y, pos_z, [radius, is_drive, is_steer, susp_stiffness, susp_damping])
int L_EcsVehicleAddWheel(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_vehicle_add_wheel(EID(e),
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4),
        helper::OptFloat(L, 5, 0.3f),
        helper::OptBool(L, 6, true) ? 1 : 0,
        helper::OptBool(L, 7, false) ? 1 : 0,
        helper::OptFloat(L, 8, 30000.0f),
        helper::OptFloat(L, 9, 4500.0f));
    return 0;
}

/// vehicle_set_input(entity, throttle, brake, steering)
int L_EcsVehicleSetInput(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_vehicle_set_input(EID(e),
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4));
    return 0;
}

/// vehicle_get_speed(entity) -> speed (m/s)
int L_EcsVehicleGetSpeed(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    lua_pushnumber(L, static_cast<lua_Number>(dse_vehicle_get_speed(EID(e))));
    return 1;
}

/// vehicle_get_wheel_count(entity) -> count
int L_EcsVehicleGetWheelCount(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(dse_vehicle_get_wheel_count(EID(e))));
    return 1;
}

#endif // DSE_HAS_PHYSICS3D

// ============================================================
// RopeComponent 绑定（Phase 2 — Task 4）
// ============================================================

/// add_rope(entity, [segment_count, segment_length, damping, iterations])
int L_EcsAddRope(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_rope_add(EID(e),
        helper::OptInt(L, 2, 10),
        helper::OptFloat(L, 3, 0.2f),
        helper::OptFloat(L, 4, 0.99f),
        helper::OptInt(L, 5, 8));
    return 0;
}

/// rope_set_anchors(entity, anchor_a_entity, anchor_b_entity, [off_ax, off_ay, off_az, off_bx, off_by, off_bz])
int L_EcsRopeSetAnchors(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_rope_set_anchors(EID(e),
        static_cast<uint32_t>(helper::OptInt(L, 2, 0)),
        static_cast<uint32_t>(helper::OptInt(L, 3, 0)),
        helper::OptFloat(L, 4, 0.0f),
        helper::OptFloat(L, 5, 0.0f),
        helper::OptFloat(L, 6, 0.0f),
        helper::OptFloat(L, 7, 0.0f),
        helper::OptFloat(L, 8, 0.0f),
        helper::OptFloat(L, 9, 0.0f));
    return 0;
}

/// rope_get_positions(entity) -> { {x1,y1,z1}, {x2,y2,z2}, ... }
int L_EcsRopeGetPositions(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const int count = dse_rope_get_positions(EID(e), nullptr, 0);
    lua_newtable(L);
    if (count <= 0) return 1;
    std::vector<float> buf(static_cast<size_t>(count) * 3);
    dse_rope_get_positions(EID(e), buf.data(), count);
    for (int i = 0; i < count; ++i) {
        lua_newtable(L);
        lua_pushnumber(L, static_cast<lua_Number>(buf[i * 3 + 0]));
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, static_cast<lua_Number>(buf[i * 3 + 1]));
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, static_cast<lua_Number>(buf[i * 3 + 2]));
        lua_rawseti(L, -2, 3);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

/// rope_set_gravity(entity, use_gravity, [gravity_scale])
int L_EcsRopeSetGravity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_rope_set_gravity(EID(e),
        helper::CheckBool(L, 2) ? 1 : 0,
        helper::OptFloat(L, 3, NAN));  // NaN=保持当前
    return 0;
}

#ifdef DSE_HAS_PHYSICS3D
// ============================================================
// BuoyancyComponent 绑定（Phase 2 — Task 5）
// ============================================================

/// add_buoyancy(entity, [water_level, buoyancy_force, water_drag, angular_drag, submerge_depth])
int L_EcsAddBuoyancy(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_buoyancy_add(EID(e),
        helper::OptFloat(L, 2, 0.0f),
        helper::OptFloat(L, 3, 10.0f),
        helper::OptFloat(L, 4, 3.0f),
        helper::OptFloat(L, 5, 1.0f),
        helper::OptFloat(L, 6, 1.0f));
    return 0;
}

/// buoyancy_add_sample_point(entity, offset_x, offset_y, offset_z, [force_scale])
int L_EcsBuoyancyAddSamplePoint(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_buoyancy_add_sample_point(EID(e),
        helper::CheckFloat(L, 2),
        helper::CheckFloat(L, 3),
        helper::CheckFloat(L, 4),
        helper::OptFloat(L, 5, 1.0f));
    return 0;
}

/// buoyancy_set_water_level(entity, water_level)
int L_EcsBuoyancySetWaterLevel(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_buoyancy_set_water_level(EID(e), helper::CheckFloat(L, 2));
    return 0;
}

/// buoyancy_get_submerge_ratio(entity) -> ratio [0,1]
int L_EcsBuoyancyGetSubmergeRatio(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    lua_pushnumber(L, static_cast<lua_Number>(dse_buoyancy_get_submerge_ratio(EID(e))));
    return 1;
}

/// buoyancy_set_use_fluid(entity, use_fluid_system)
int L_EcsBuoyancySetUseFluid(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_buoyancy_set_use_fluid(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}

#endif // DSE_HAS_PHYSICS3D

// ============================================================
// WeatherComponent 绑定
// ============================================================

/// add_weather(entity, type_str, intensity)
/// type_str: "none" | "rain" | "snow"
int L_EcsAddWeather(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int type = WeatherTypeFromStr(luaL_optstring(L, 2, "snow"));
    dse_weather_add(EID(e), type, helper::OptFloat(L, 3, 0.5f));
    return 0;
}

/// set_weather(entity, type_str, intensity, wind_x, wind_z)
int L_EcsSetWeather(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int type = WeatherTypeFromStr(luaL_optstring(L, 2, nullptr));  // -1=保持
    dse_weather_set(EID(e), type,
        helper::OptFloat(L, 3, NAN),
        helper::OptFloat(L, 4, NAN),
        helper::OptFloat(L, 5, NAN));
    return 0;
}

/// set_weather_spawn(entity, radius, height, max_particles)
int L_EcsSetWeatherSpawn(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_weather_set_spawn(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptInt(L, 4, -1));  // <0=保持
    return 0;
}

// ============================================================
// SnowCoverComponent 绑定
// ============================================================

/// add_snow_cover(entity)
int L_EcsAddSnowCover(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_snow_cover_add(EID(e));
    return 0;
}

/// set_snow_cover(entity, target_coverage, accumulation_rate, melt_rate)
int L_EcsSetSnowCover(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_snow_cover_set(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptFloat(L, 4, NAN));
    return 0;
}

/// set_snow_appearance(entity, albedo_r, albedo_g, albedo_b, roughness, metallic, threshold, sharpness)
int L_EcsSetSnowAppearance(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_snow_set_appearance(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptFloat(L, 4, NAN),
        helper::OptFloat(L, 5, NAN),
        helper::OptFloat(L, 6, NAN),
        helper::OptFloat(L, 7, NAN),
        helper::OptFloat(L, 8, NAN));
    return 0;
}

/// get_snow_cover(entity) -> coverage, target_coverage, enabled
int L_EcsGetSnowCover(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float coverage = 0.0f, target = 0.0f;
    int enabled = 0;
    dse_snow_cover_get(EID(e), &coverage, &target, &enabled);
    lua_pushnumber(L, static_cast<lua_Number>(coverage));
    lua_pushnumber(L, static_cast<lua_Number>(target));
    lua_pushboolean(L, enabled);
    return 3;
}

/// set_snow_cover_enabled(entity, enabled)
int L_EcsSetSnowCoverEnabled(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_snow_cover_set_enabled(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}

/// set_snow_texture(entity, texture_path, [tiling])
int L_EcsSetSnowTexture(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_snow_set_texture(EID(e),
        helper::OptString(L, 2, nullptr),
        helper::OptFloat(L, 3, NAN));
    return 0;
}

/// set_snow_displacement(entity, displacement_height, [deformation_strength])
int L_EcsSetSnowDisplacement(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_snow_set_displacement(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN));
    return 0;
}

/// remove_snow_cover(entity)
int L_EcsRemoveSnowCover(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_snow_cover_remove(EID(e));
    return 0;
}

// ============================================================
// AtmosphereComponent 绑定
// ============================================================

/// add_atmosphere(entity)
int L_EcsAddAtmosphere(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_atmosphere_add(EID(e));
    return 0;
}

/// set_atmosphere_params(entity, planet_radius, atmosphere_height, sun_disk_angle)
int L_EcsSetAtmosphereParams(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_atmosphere_set_params(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptFloat(L, 4, NAN));
    return 0;
}

/// set_atmosphere_rayleigh(entity, coeff_r, coeff_g, coeff_b, scale_height)
int L_EcsSetAtmosphereRayleigh(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_atmosphere_set_rayleigh(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptFloat(L, 4, NAN),
        helper::OptFloat(L, 5, NAN));
    return 0;
}

/// set_atmosphere_mie(entity, coeff, scale_height, g)
int L_EcsSetAtmosphereMie(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_atmosphere_set_mie(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptFloat(L, 4, NAN));
    return 0;
}

/// set_atmosphere_sun_intensity(entity, r, g, b)
int L_EcsSetAtmosphereSunIntensity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_atmosphere_set_sun_intensity(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptFloat(L, 4, NAN));
    return 0;
}

// ============================================================
// DayNightCycleComponent 绑定
// ============================================================

/// add_day_night_cycle(entity, [time_of_day, auto_advance, time_speed])
int L_EcsAddDayNightCycle(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_day_night_add(EID(e),
        helper::OptFloat(L, 2, 12.0f),
        helper::OptBool(L, 3, false) ? 1 : 0,
        helper::OptFloat(L, 4, 1.0f));
    return 0;
}

/// set_day_night_time(entity, time_of_day)
int L_EcsSetDayNightTime(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_day_night_set_time(EID(e), helper::CheckFloat(L, 2));
    return 0;
}

/// get_day_night_time(entity) -> time_of_day
int L_EcsGetDayNightTime(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    lua_pushnumber(L, static_cast<lua_Number>(dse_day_night_get_time(EID(e))));
    return 1;
}

/// set_day_night_speed(entity, speed)
int L_EcsSetDayNightSpeed(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_day_night_set_speed(EID(e), helper::CheckFloat(L, 2));
    return 0;
}

/// set_day_night_auto_advance(entity, enabled)
int L_EcsSetDayNightAutoAdvance(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_day_night_set_auto_advance(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}

/// set_day_night_location(entity, latitude, longitude, day_of_year)
int L_EcsSetDayNightLocation(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_day_night_set_location(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptInt(L, 4, -1));  // <=0=保持
    return 0;
}

/// get_sun_elevation(entity) -> degrees
int L_EcsGetSunElevation(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    lua_pushnumber(L, static_cast<lua_Number>(dse_day_night_get_sun_elevation(EID(e))));
    return 1;
}

/// get_sun_direction(entity) -> x, y, z
int L_EcsGetSunDirection(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float dir[3] = {0.0f, -1.0f, 0.0f};
    dse_day_night_get_sun_direction(EID(e), dir);
    lua_pushnumber(L, static_cast<lua_Number>(dir[0]));
    lua_pushnumber(L, static_cast<lua_Number>(dir[1]));
    lua_pushnumber(L, static_cast<lua_Number>(dir[2]));
    return 3;
}

// ============================================================
// VolumetricCloudComponent 绑定
// ============================================================

/// add_volumetric_cloud(entity)
int L_EcsAddVolumetricCloud(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_volumetric_cloud_add(EID(e));
    return 0;
}

/// set_cloud_layer(entity, bottom, top, coverage, density)
int L_EcsSetCloudLayer(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_cloud_set_layer(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptFloat(L, 4, NAN),
        helper::OptFloat(L, 5, NAN));
    return 0;
}

/// set_cloud_wind(entity, dir_x, dir_y, speed)
int L_EcsSetCloudWind(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_cloud_set_wind(EID(e),
        helper::OptFloat(L, 2, NAN),
        helper::OptFloat(L, 3, NAN),
        helper::OptFloat(L, 4, NAN));
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
        // 体积云（由 VolumetricCloudPass 渲染）
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
        {"get_snow_cover",             L_EcsGetSnowCover},
        {"set_snow_cover_enabled",     L_EcsSetSnowCoverEnabled},
        {"set_snow_texture",           L_EcsSetSnowTexture},
        {"set_snow_displacement",      L_EcsSetSnowDisplacement},
        {"remove_snow_cover",          L_EcsRemoveSnowCover},
    });
}

} // namespace dse::runtime::lua_binding
