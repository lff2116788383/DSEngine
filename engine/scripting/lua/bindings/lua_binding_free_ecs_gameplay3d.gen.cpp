/**
 * @file lua_binding_free_ecs_gameplay3d.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}
#include <cmath>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

namespace dse::runtime::lua_binding {
namespace {

int L_dse_fracture_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int source = static_cast<int>(luaL_checkinteger(L, 2));
    uint32_t fragment_count = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    float break_force = static_cast<float>(luaL_checknumber(L, 4));
    float health = static_cast<float>(luaL_checknumber(L, 5));
    dse_fracture_add(e, source, fragment_count, break_force, health);
    return 0;
}

int L_dse_fracture_set_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float explosion_force = static_cast<float>(luaL_checknumber(L, 2));
    float fragment_lifetime = static_cast<float>(luaL_checknumber(L, 3));
    float fade_duration = static_cast<float>(luaL_checknumber(L, 4));
    float mass_scale = static_cast<float>(luaL_checknumber(L, 5));
    dse_fracture_set_params(e, explosion_force, fragment_lifetime, fade_duration, mass_scale);
    return 0;
}

int L_dse_fracture_apply_damage(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float damage = static_cast<float>(luaL_checknumber(L, 2));
    float ix = static_cast<float>(luaL_checknumber(L, 3));
    float iy = static_cast<float>(luaL_checknumber(L, 4));
    float iz = static_cast<float>(luaL_checknumber(L, 5));
    dse_fracture_apply_damage(e, damage, ix, iy, iz);
    return 0;
}

int L_dse_fracture_trigger(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float ix = static_cast<float>(luaL_checknumber(L, 2));
    float iy = static_cast<float>(luaL_checknumber(L, 3));
    float iz = static_cast<float>(luaL_checknumber(L, 4));
    dse_fracture_trigger(e, ix, iy, iz);
    return 0;
}

int L_dse_fracture_is_fractured(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_fracture_is_fractured(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_cloth_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t solver_iterations = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    float stiffness = static_cast<float>(luaL_checknumber(L, 3));
    float damping = static_cast<float>(luaL_checknumber(L, 4));
    float bend_stiffness = static_cast<float>(luaL_checknumber(L, 5));
    dse_cloth_add(e, solver_iterations, stiffness, damping, bend_stiffness);
    return 0;
}

int L_dse_cloth_set_wind(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float wx = static_cast<float>(luaL_checknumber(L, 2));
    float wy = static_cast<float>(luaL_checknumber(L, 3));
    float wz = static_cast<float>(luaL_checknumber(L, 4));
    float turbulence = lua_isnoneornil(L, 5) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 5));
    dse_cloth_set_wind(e, wx, wy, wz, turbulence);
    return 0;
}

int L_dse_cloth_set_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float gx = static_cast<float>(luaL_checknumber(L, 2));
    float gy = static_cast<float>(luaL_checknumber(L, 3));
    float gz = static_cast<float>(luaL_checknumber(L, 4));
    dse_cloth_set_gravity(e, gx, gy, gz);
    return 0;
}

int L_dse_cloth_add_sphere_collider(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t collider_entity = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    float radius = static_cast<float>(luaL_checknumber(L, 3));
    dse_cloth_add_sphere_collider(e, collider_entity, radius);
    return 0;
}

int L_dse_fluid_add_emitter(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int shape = static_cast<int>(luaL_checkinteger(L, 2));
    float emission_rate = static_cast<float>(luaL_checknumber(L, 3));
    float particle_lifetime = static_cast<float>(luaL_checknumber(L, 4));
    float emit_speed = static_cast<float>(luaL_checknumber(L, 5));
    dse_fluid_add_emitter(e, shape, emission_rate, particle_lifetime, emit_speed);
    return 0;
}

int L_dse_fluid_set_physics(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float viscosity = static_cast<float>(luaL_checknumber(L, 2));
    float surface_tension = static_cast<float>(luaL_checknumber(L, 3));
    float rest_density = static_cast<float>(luaL_checknumber(L, 4));
    float gas_stiffness = static_cast<float>(luaL_checknumber(L, 5));
    dse_fluid_set_physics(e, viscosity, surface_tension, rest_density, gas_stiffness);
    return 0;
}

int L_dse_fluid_set_rendering(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_checknumber(L, 5));
    float refraction = static_cast<float>(luaL_checknumber(L, 6));
    float fresnel = static_cast<float>(luaL_checknumber(L, 7));
    float specular = static_cast<float>(luaL_checknumber(L, 8));
    dse_fluid_set_rendering(e, r, g, b, a, refraction, fresnel, specular);
    return 0;
}

int L_dse_fluid_set_emit_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float dx = static_cast<float>(luaL_checknumber(L, 2));
    float dy = static_cast<float>(luaL_checknumber(L, 3));
    float dz = static_cast<float>(luaL_checknumber(L, 4));
    float spread = static_cast<float>(luaL_checknumber(L, 5));
    dse_fluid_set_emit_direction(e, dx, dy, dz, spread);
    return 0;
}

int L_dse_fluid_set_floor(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float floor_y = static_cast<float>(luaL_checknumber(L, 2));
    float restitution = static_cast<float>(luaL_checknumber(L, 3));
    dse_fluid_set_floor(e, floor_y, restitution);
    return 0;
}

int L_dse_fluid_get_particle_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t _ret = dse_fluid_get_particle_count(e);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_ragdoll_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float total_mass = static_cast<float>(luaL_checknumber(L, 2));
    int auto_setup = helper::CheckBool(L, 3) ? 1 : 0;
    float joint_stiffness = static_cast<float>(luaL_checknumber(L, 4));
    float joint_damping = static_cast<float>(luaL_checknumber(L, 5));
    dse_ragdoll_add(e, total_mass, auto_setup, joint_stiffness, joint_damping);
    return 0;
}

int L_dse_ragdoll_activate(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ragdoll_activate(e);
    return 0;
}

int L_dse_ragdoll_deactivate(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ragdoll_deactivate(e);
    return 0;
}

int L_dse_ragdoll_is_active(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ragdoll_is_active(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ragdoll_set_collision_layer_mask(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t layer = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t mask = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    dse_ragdoll_set_collision_layer_mask(e, layer, mask);
    return 0;
}

int L_dse_softbody_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float stiffness = static_cast<float>(luaL_checknumber(L, 2));
    int iterations = static_cast<int>(luaL_checkinteger(L, 3));
    float damping = static_cast<float>(luaL_checknumber(L, 4));
    float volume_stiffness = static_cast<float>(luaL_checknumber(L, 5));
    dse_softbody_add(e, stiffness, iterations, damping, volume_stiffness);
    return 0;
}

int L_dse_softbody_set_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int use_gravity = helper::CheckBool(L, 2) ? 1 : 0;
    float gravity_scale = lua_isnoneornil(L, 3) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 3));
    dse_softbody_set_gravity(e, use_gravity, gravity_scale);
    return 0;
}

int L_dse_softbody_pin_vertex(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int vertex_index = static_cast<int>(luaL_checkinteger(L, 2));
    dse_softbody_pin_vertex(e, vertex_index);
    return 0;
}

int L_dse_softbody_get_particle_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t _ret = dse_softbody_get_particle_count(e);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_vehicle_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float max_engine_force = static_cast<float>(luaL_checknumber(L, 2));
    float max_brake_force = static_cast<float>(luaL_checknumber(L, 3));
    float max_steer_angle = static_cast<float>(luaL_checknumber(L, 4));
    dse_vehicle_add(e, max_engine_force, max_brake_force, max_steer_angle);
    return 0;
}

int L_dse_vehicle_add_wheel(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float px = static_cast<float>(luaL_checknumber(L, 2));
    float py = static_cast<float>(luaL_checknumber(L, 3));
    float pz = static_cast<float>(luaL_checknumber(L, 4));
    float radius = static_cast<float>(luaL_checknumber(L, 5));
    int is_drive = helper::CheckBool(L, 6) ? 1 : 0;
    int is_steer = helper::CheckBool(L, 7) ? 1 : 0;
    float susp_stiffness = static_cast<float>(luaL_checknumber(L, 8));
    float susp_damping = static_cast<float>(luaL_checknumber(L, 9));
    dse_vehicle_add_wheel(e, px, py, pz, radius, is_drive, is_steer, susp_stiffness, susp_damping);
    return 0;
}

int L_dse_vehicle_set_input(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float throttle = static_cast<float>(luaL_checknumber(L, 2));
    float brake = static_cast<float>(luaL_checknumber(L, 3));
    float steering = static_cast<float>(luaL_checknumber(L, 4));
    dse_vehicle_set_input(e, throttle, brake, steering);
    return 0;
}

int L_dse_vehicle_get_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_vehicle_get_speed(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_vehicle_get_wheel_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t _ret = dse_vehicle_get_wheel_count(e);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_rope_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int segment_count = static_cast<int>(luaL_checkinteger(L, 2));
    float segment_length = static_cast<float>(luaL_checknumber(L, 3));
    float damping = static_cast<float>(luaL_checknumber(L, 4));
    int iterations = static_cast<int>(luaL_checkinteger(L, 5));
    dse_rope_add(e, segment_count, segment_length, damping, iterations);
    return 0;
}

int L_dse_rope_set_anchors(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t anchor_a = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t anchor_b = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    float oax = static_cast<float>(luaL_checknumber(L, 4));
    float oay = static_cast<float>(luaL_checknumber(L, 5));
    float oaz = static_cast<float>(luaL_checknumber(L, 6));
    float obx = static_cast<float>(luaL_checknumber(L, 7));
    float oby = static_cast<float>(luaL_checknumber(L, 8));
    float obz = static_cast<float>(luaL_checknumber(L, 9));
    dse_rope_set_anchors(e, anchor_a, anchor_b, oax, oay, oaz, obx, oby, obz);
    return 0;
}

int L_dse_rope_set_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int use_gravity = helper::CheckBool(L, 2) ? 1 : 0;
    float gravity_scale = static_cast<float>(luaL_checknumber(L, 3));
    dse_rope_set_gravity(e, use_gravity, gravity_scale);
    return 0;
}

int L_dse_buoyancy_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float water_level = static_cast<float>(luaL_checknumber(L, 2));
    float buoyancy_force = static_cast<float>(luaL_checknumber(L, 3));
    float water_drag = static_cast<float>(luaL_optnumber(L, 4, 3.0));
    float angular_drag = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float submerge_depth = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    dse_buoyancy_add(e, water_level, buoyancy_force, water_drag, angular_drag, submerge_depth);
    return 0;
}

int L_dse_buoyancy_add_sample_point(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float ox = static_cast<float>(luaL_checknumber(L, 2));
    float oy = static_cast<float>(luaL_checknumber(L, 3));
    float oz = static_cast<float>(luaL_checknumber(L, 4));
    float force_scale = static_cast<float>(luaL_checknumber(L, 5));
    dse_buoyancy_add_sample_point(e, ox, oy, oz, force_scale);
    return 0;
}

int L_dse_buoyancy_set_water_level(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float water_level = static_cast<float>(luaL_checknumber(L, 2));
    dse_buoyancy_set_water_level(e, water_level);
    return 0;
}

int L_dse_buoyancy_get_submerge_ratio(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_buoyancy_get_submerge_ratio(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_buoyancy_set_use_fluid(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int use_fluid = helper::CheckBool(L, 2) ? 1 : 0;
    dse_buoyancy_set_use_fluid(e, use_fluid);
    return 0;
}

int L_dse_compat_weather_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* type = luaL_checkstring(L, 2);
    float intensity = static_cast<float>(luaL_checknumber(L, 3));
    dse_compat_weather_add(e, type, intensity);
    return 0;
}

int L_dse_weather_set(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int type = lua_isnoneornil(L, 2) ? -1 : static_cast<int>(luaL_checkinteger(L, 2));
    float intensity = lua_isnoneornil(L, 3) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 3));
    float wind_x = lua_isnoneornil(L, 4) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 4));
    float wind_z = lua_isnoneornil(L, 5) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 5));
    dse_weather_set(e, type, intensity, wind_x, wind_z);
    return 0;
}

int L_dse_weather_set_spawn(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float radius = lua_isnoneornil(L, 2) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 2));
    float height = lua_isnoneornil(L, 3) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 3));
    int max_particles = lua_isnoneornil(L, 4) ? -1 : static_cast<int>(luaL_checkinteger(L, 4));
    dse_weather_set_spawn(e, radius, height, max_particles);
    return 0;
}

int L_dse_snow_cover_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_snow_cover_add(e);
    return 0;
}

int L_dse_snow_cover_set(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float target_coverage = lua_isnoneornil(L, 2) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 2));
    float accumulation_rate = lua_isnoneornil(L, 3) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 3));
    float melt_rate = lua_isnoneornil(L, 4) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 4));
    dse_snow_cover_set(e, target_coverage, accumulation_rate, melt_rate);
    return 0;
}

int L_dse_snow_set_appearance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float albedo_r = static_cast<float>(luaL_checknumber(L, 2));
    float albedo_g = static_cast<float>(luaL_checknumber(L, 3));
    float albedo_b = static_cast<float>(luaL_checknumber(L, 4));
    float roughness = static_cast<float>(luaL_checknumber(L, 5));
    float metallic = static_cast<float>(luaL_checknumber(L, 6));
    float threshold = static_cast<float>(luaL_checknumber(L, 7));
    float sharpness = static_cast<float>(luaL_checknumber(L, 8));
    dse_snow_set_appearance(e, albedo_r, albedo_g, albedo_b, roughness, metallic, threshold, sharpness);
    return 0;
}

int L_dse_snow_cover_set_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    dse_snow_cover_set_enabled(e, enabled);
    return 0;
}

int L_dse_snow_set_texture(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    float tiling = static_cast<float>(luaL_checknumber(L, 3));
    dse_snow_set_texture(e, path, tiling);
    return 0;
}

int L_dse_snow_set_displacement(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float displacement_height = static_cast<float>(luaL_checknumber(L, 2));
    float deformation_strength = static_cast<float>(luaL_checknumber(L, 3));
    dse_snow_set_displacement(e, displacement_height, deformation_strength);
    return 0;
}

int L_dse_snow_cover_remove(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_snow_cover_remove(e);
    return 0;
}

int L_dse_atmosphere_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_add(e);
    return 0;
}

int L_dse_atmosphere_set_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float planet_radius = static_cast<float>(luaL_checknumber(L, 2));
    float atmosphere_height = static_cast<float>(luaL_checknumber(L, 3));
    float sun_disk_angle = static_cast<float>(luaL_checknumber(L, 4));
    dse_atmosphere_set_params(e, planet_radius, atmosphere_height, sun_disk_angle);
    return 0;
}

int L_dse_atmosphere_set_rayleigh(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float coeff_r = static_cast<float>(luaL_checknumber(L, 2));
    float coeff_g = static_cast<float>(luaL_checknumber(L, 3));
    float coeff_b = static_cast<float>(luaL_checknumber(L, 4));
    float scale_height = static_cast<float>(luaL_checknumber(L, 5));
    dse_atmosphere_set_rayleigh(e, coeff_r, coeff_g, coeff_b, scale_height);
    return 0;
}

int L_dse_atmosphere_set_mie(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float coeff = static_cast<float>(luaL_checknumber(L, 2));
    float scale_height = static_cast<float>(luaL_checknumber(L, 3));
    float g = static_cast<float>(luaL_checknumber(L, 4));
    dse_atmosphere_set_mie(e, coeff, scale_height, g);
    return 0;
}

int L_dse_atmosphere_set_sun_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    dse_atmosphere_set_sun_intensity(e, x, y, z);
    return 0;
}

int L_dse_day_night_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float time_of_day = static_cast<float>(luaL_optnumber(L, 2, 12.0));
    int auto_advance = helper::OptBool(L, 3, false) ? 1 : 0;
    float time_speed = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    dse_day_night_add(e, time_of_day, auto_advance, time_speed);
    return 0;
}

int L_dse_day_night_set_time(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float time_of_day = static_cast<float>(luaL_checknumber(L, 2));
    dse_day_night_set_time(e, time_of_day);
    return 0;
}

int L_dse_day_night_get_time(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_day_night_get_time(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_day_night_set_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float speed = static_cast<float>(luaL_checknumber(L, 2));
    dse_day_night_set_speed(e, speed);
    return 0;
}

int L_dse_day_night_set_auto_advance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_day_night_set_auto_advance(e, enabled);
    return 0;
}

int L_dse_day_night_set_location(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float latitude = lua_isnoneornil(L, 2) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 2));
    float longitude = lua_isnoneornil(L, 3) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 3));
    int day_of_year = lua_isnoneornil(L, 4) ? -1 : static_cast<int>(luaL_checkinteger(L, 4));
    dse_day_night_set_location(e, latitude, longitude, day_of_year);
    return 0;
}

int L_dse_day_night_get_sun_elevation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_day_night_get_sun_elevation(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_volumetric_cloud_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_add(e);
    return 0;
}

int L_dse_cloud_set_layer(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float bottom = lua_isnoneornil(L, 2) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 2));
    float top = lua_isnoneornil(L, 3) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 3));
    float coverage = lua_isnoneornil(L, 4) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 4));
    float density = lua_isnoneornil(L, 5) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 5));
    dse_cloud_set_layer(e, bottom, top, coverage, density);
    return 0;
}

int L_dse_cloud_set_wind(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float dir_x = lua_isnoneornil(L, 2) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 2));
    float dir_y = lua_isnoneornil(L, 3) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 3));
    float speed = lua_isnoneornil(L, 4) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 4));
    dse_cloud_set_wind(e, dir_x, dir_y, speed);
    return 0;
}

int L_dse_rope_get_positions(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _buf[768];
    int _count = dse_rope_get_positions(e, _buf, 256);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_newtable(L);
        lua_pushnumber(L, _buf[_i * 3 + 0]); lua_setfield(L, -2, "x");
        lua_pushnumber(L, _buf[_i * 3 + 1]); lua_setfield(L, -2, "y");
        lua_pushnumber(L, _buf[_i * 3 + 2]); lua_setfield(L, -2, "z");
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

int L_dse_cloth_pin_vertices(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    std::vector<uint32_t> verts;
    if (lua_istable(L, 2)) {
        lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2));
        for (lua_Integer _i = 1; _i <= _n; ++_i) {
            lua_rawgeti(L, 2, _i);
            if (lua_isnumber(L, -1)) verts.push_back(static_cast<uint32_t>(lua_tointeger(L, -1)));
            lua_pop(L, 1);
        }
    }
    dse_cloth_pin_vertices(e, verts.data(), static_cast<int>(verts.size()));
    return 0;
}

} // namespace

void RegisterEcsGameplay3DBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"add_fracture", L_dse_fracture_add},
        {"set_fracture_params", L_dse_fracture_set_params},
        {"fracture_apply_damage", L_dse_fracture_apply_damage},
        {"fracture_trigger", L_dse_fracture_trigger},
        {"fracture_is_fractured", L_dse_fracture_is_fractured},
        {"add_cloth", L_dse_cloth_add},
        {"set_cloth_wind", L_dse_cloth_set_wind},
        {"set_cloth_gravity", L_dse_cloth_set_gravity},
        {"cloth_add_sphere_collider", L_dse_cloth_add_sphere_collider},
        {"add_fluid_emitter", L_dse_fluid_add_emitter},
        {"set_fluid_physics", L_dse_fluid_set_physics},
        {"set_fluid_rendering", L_dse_fluid_set_rendering},
        {"set_fluid_emit_direction", L_dse_fluid_set_emit_direction},
        {"set_fluid_floor", L_dse_fluid_set_floor},
        {"get_fluid_particle_count", L_dse_fluid_get_particle_count},
        {"add_ragdoll", L_dse_ragdoll_add},
        {"ragdoll_activate", L_dse_ragdoll_activate},
        {"ragdoll_deactivate", L_dse_ragdoll_deactivate},
        {"ragdoll_is_active", L_dse_ragdoll_is_active},
        {"set_ragdoll_collision_layer", L_dse_ragdoll_set_collision_layer_mask},
        {"add_softbody", L_dse_softbody_add},
        {"softbody_set_gravity", L_dse_softbody_set_gravity},
        {"softbody_pin_vertex", L_dse_softbody_pin_vertex},
        {"softbody_get_particle_count", L_dse_softbody_get_particle_count},
        {"add_vehicle", L_dse_vehicle_add},
        {"vehicle_add_wheel", L_dse_vehicle_add_wheel},
        {"vehicle_set_input", L_dse_vehicle_set_input},
        {"vehicle_get_speed", L_dse_vehicle_get_speed},
        {"vehicle_get_wheel_count", L_dse_vehicle_get_wheel_count},
        {"add_rope", L_dse_rope_add},
        {"rope_set_anchors", L_dse_rope_set_anchors},
        {"rope_set_gravity", L_dse_rope_set_gravity},
        {"add_buoyancy", L_dse_buoyancy_add},
        {"buoyancy_add_sample_point", L_dse_buoyancy_add_sample_point},
        {"buoyancy_set_water_level", L_dse_buoyancy_set_water_level},
        {"buoyancy_get_submerge_ratio", L_dse_buoyancy_get_submerge_ratio},
        {"buoyancy_set_use_fluid", L_dse_buoyancy_set_use_fluid},
        {"add_weather", L_dse_compat_weather_add},
        {"set_weather", L_dse_weather_set},
        {"set_weather_spawn", L_dse_weather_set_spawn},
        {"add_snow_cover", L_dse_snow_cover_add},
        {"set_snow_cover", L_dse_snow_cover_set},
        {"set_snow_appearance", L_dse_snow_set_appearance},
        {"set_snow_cover_enabled", L_dse_snow_cover_set_enabled},
        {"set_snow_texture", L_dse_snow_set_texture},
        {"set_snow_displacement", L_dse_snow_set_displacement},
        {"remove_snow_cover", L_dse_snow_cover_remove},
        {"add_atmosphere", L_dse_atmosphere_add},
        {"set_atmosphere_params", L_dse_atmosphere_set_params},
        {"set_atmosphere_rayleigh", L_dse_atmosphere_set_rayleigh},
        {"set_atmosphere_mie", L_dse_atmosphere_set_mie},
        {"set_atmosphere_sun_intensity", L_dse_atmosphere_set_sun_intensity},
        {"add_day_night_cycle", L_dse_day_night_add},
        {"set_day_night_time", L_dse_day_night_set_time},
        {"get_day_night_time", L_dse_day_night_get_time},
        {"set_day_night_speed", L_dse_day_night_set_speed},
        {"set_day_night_auto_advance", L_dse_day_night_set_auto_advance},
        {"set_day_night_location", L_dse_day_night_set_location},
        {"get_sun_elevation", L_dse_day_night_get_sun_elevation},
        {"add_volumetric_cloud", L_dse_volumetric_cloud_add},
        {"set_cloud_layer", L_dse_cloud_set_layer},
        {"set_cloud_wind", L_dse_cloud_set_wind},
        {"rope_get_positions", L_dse_rope_get_positions},
        {"cloth_pin_vertices", L_dse_cloth_pin_vertices},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
