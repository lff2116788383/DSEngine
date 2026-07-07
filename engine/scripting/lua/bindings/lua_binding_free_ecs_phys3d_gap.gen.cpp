/**
 * @file lua_binding_free_ecs_phys3d_gap.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_character_check_ground(lua_State* L) {
    float out_normal[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_character_check_ground(e, out_normal);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_normal[0]);
    lua_pushnumber(L, out_normal[1]);
    lua_pushnumber(L, out_normal[2]);
    return 4;
}

int L_dse_character_controller3d_get_position(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_character_controller3d_get_position(e, out_xyz);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 4;
}

int L_dse_character_controller3d_move(lua_State* L) {
    float out_velocity[3] = {0, 0, 0};
    uint32_t out_flags = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float dx = static_cast<float>(luaL_checknumber(L, 2));
    float dy = static_cast<float>(luaL_checknumber(L, 3));
    float dz = static_cast<float>(luaL_checknumber(L, 4));
    float min_dist = static_cast<float>(luaL_checknumber(L, 5));
    float dt = static_cast<float>(luaL_checknumber(L, 6));
    int _ret = dse_character_controller3d_move(e, dx, dy, dz, min_dist, dt, out_velocity, &out_flags);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_velocity[0]);
    lua_pushnumber(L, out_velocity[1]);
    lua_pushnumber(L, out_velocity[2]);
    lua_pushinteger(L, static_cast<lua_Integer>(out_flags));
    return 5;
}

int L_dse_physics3d_boxcast(lua_State* L) {
    uint32_t out_entity = 0;
    float out_point[3] = {0, 0, 0};
    float out_normal[3] = {0, 0, 0};
    float out_distance = 0;
    float ox = static_cast<float>(luaL_checknumber(L, 1));
    float oy = static_cast<float>(luaL_checknumber(L, 2));
    float oz = static_cast<float>(luaL_checknumber(L, 3));
    float dx = static_cast<float>(luaL_checknumber(L, 4));
    float dy = static_cast<float>(luaL_checknumber(L, 5));
    float dz = static_cast<float>(luaL_checknumber(L, 6));
    float hx = static_cast<float>(luaL_checknumber(L, 7));
    float hy = static_cast<float>(luaL_checknumber(L, 8));
    float hz = static_cast<float>(luaL_checknumber(L, 9));
    float max_dist = static_cast<float>(luaL_checknumber(L, 10));
    int _ret = dse_physics3d_boxcast(ox, oy, oz, dx, dy, dz, hx, hy, hz, max_dist, &out_entity, out_point, out_normal, &out_distance);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_entity));
    lua_pushnumber(L, out_point[0]);
    lua_pushnumber(L, out_point[1]);
    lua_pushnumber(L, out_point[2]);
    lua_pushnumber(L, out_normal[0]);
    lua_pushnumber(L, out_normal[1]);
    lua_pushnumber(L, out_normal[2]);
    lua_pushnumber(L, out_distance);
    return 9;
}

int L_dse_physics3d_get_collision_count(lua_State* L) {
    int _ret = dse_physics3d_get_collision_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_physics3d_get_trigger_count(lua_State* L) {
    int _ret = dse_physics3d_get_trigger_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_physics3d_raycast(lua_State* L) {
    uint32_t out_entity = 0;
    float out_point[3] = {0, 0, 0};
    float out_normal[3] = {0, 0, 0};
    float out_distance = 0;
    float ox = static_cast<float>(luaL_checknumber(L, 1));
    float oy = static_cast<float>(luaL_checknumber(L, 2));
    float oz = static_cast<float>(luaL_checknumber(L, 3));
    float dx = static_cast<float>(luaL_checknumber(L, 4));
    float dy = static_cast<float>(luaL_checknumber(L, 5));
    float dz = static_cast<float>(luaL_checknumber(L, 6));
    float max_dist = static_cast<float>(luaL_checknumber(L, 7));
    int _ret = dse_physics3d_raycast(ox, oy, oz, dx, dy, dz, max_dist, &out_entity, out_point, out_normal, &out_distance);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_entity));
    lua_pushnumber(L, out_point[0]);
    lua_pushnumber(L, out_point[1]);
    lua_pushnumber(L, out_point[2]);
    lua_pushnumber(L, out_normal[0]);
    lua_pushnumber(L, out_normal[1]);
    lua_pushnumber(L, out_normal[2]);
    lua_pushnumber(L, out_distance);
    return 9;
}

int L_dse_physics3d_spherecast(lua_State* L) {
    uint32_t out_entity = 0;
    float out_point[3] = {0, 0, 0};
    float out_normal[3] = {0, 0, 0};
    float out_distance = 0;
    float ox = static_cast<float>(luaL_checknumber(L, 1));
    float oy = static_cast<float>(luaL_checknumber(L, 2));
    float oz = static_cast<float>(luaL_checknumber(L, 3));
    float dx = static_cast<float>(luaL_checknumber(L, 4));
    float dy = static_cast<float>(luaL_checknumber(L, 5));
    float dz = static_cast<float>(luaL_checknumber(L, 6));
    float radius = static_cast<float>(luaL_checknumber(L, 7));
    float max_dist = static_cast<float>(luaL_checknumber(L, 8));
    int _ret = dse_physics3d_spherecast(ox, oy, oz, dx, dy, dz, radius, max_dist, &out_entity, out_point, out_normal, &out_distance);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_entity));
    lua_pushnumber(L, out_point[0]);
    lua_pushnumber(L, out_point[1]);
    lua_pushnumber(L, out_point[2]);
    lua_pushnumber(L, out_normal[0]);
    lua_pushnumber(L, out_normal[1]);
    lua_pushnumber(L, out_normal[2]);
    lua_pushnumber(L, out_distance);
    return 9;
}

int L_dse_physics_lod_get_stats(lua_State* L) {
    int out_stats = 0;
    int _ret = dse_physics_lod_get_stats(&out_stats);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_stats);
    return 2;
}

int L_dse_rigidbody3d_get_angular_velocity(lua_State* L) {
    float out_vel[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_get_angular_velocity(e, out_vel);
    lua_pushnumber(L, out_vel[0]);
    lua_pushnumber(L, out_vel[1]);
    lua_pushnumber(L, out_vel[2]);
    return 3;
}

int L_dse_rigidbody3d_get_velocity(lua_State* L) {
    float out_vel[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_get_velocity(e, out_vel);
    lua_pushnumber(L, out_vel[0]);
    lua_pushnumber(L, out_vel[1]);
    lua_pushnumber(L, out_vel[2]);
    return 3;
}

} // namespace

void RegisterFreePhys3dGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"character_check_ground", L_dse_character_check_ground},
        {"character_controller3d_get_position", L_dse_character_controller3d_get_position},
        {"character_controller3d_move", L_dse_character_controller3d_move},
        {"physics3d_boxcast", L_dse_physics3d_boxcast},
        {"physics3d_get_collision_count", L_dse_physics3d_get_collision_count},
        {"physics3d_get_trigger_count", L_dse_physics3d_get_trigger_count},
        {"physics3d_raycast", L_dse_physics3d_raycast},
        {"physics3d_spherecast", L_dse_physics3d_spherecast},
        {"physics_lod_get_stats", L_dse_physics_lod_get_stats},
        {"rigidbody3d_get_angular_velocity", L_dse_rigidbody3d_get_angular_velocity},
        {"rigidbody3d_get_velocity", L_dse_rigidbody3d_get_velocity},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
