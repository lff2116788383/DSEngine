/**
 * @file lua_binding_free_ecs_phys3d.gen.cpp
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

int L_dse_rigidbody3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int type = static_cast<int>(luaL_checkinteger(L, 2));
    float mass = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    dse_rigidbody3d_add(e, type, mass);
    return 0;
}

int L_dse_box_collider3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    dse_box_collider3d_add(e, x, y, z);
    return 0;
}

int L_dse_sphere_collider3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float radius = static_cast<float>(luaL_checknumber(L, 2));
    dse_sphere_collider3d_add(e, radius);
    return 0;
}

int L_dse_rigidbody3d_add_force(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float fx = static_cast<float>(luaL_checknumber(L, 2));
    float fy = static_cast<float>(luaL_checknumber(L, 3));
    float fz = static_cast<float>(luaL_checknumber(L, 4));
    dse_rigidbody3d_add_force(e, fx, fy, fz);
    return 0;
}

int L_dse_rigidbody3d_add_impulse(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float ix = static_cast<float>(luaL_checknumber(L, 2));
    float iy = static_cast<float>(luaL_checknumber(L, 3));
    float iz = static_cast<float>(luaL_checknumber(L, 4));
    dse_rigidbody3d_add_impulse(e, ix, iy, iz);
    return 0;
}

int L_dse_rigidbody3d_add_torque(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float tx = static_cast<float>(luaL_checknumber(L, 2));
    float ty = static_cast<float>(luaL_checknumber(L, 3));
    float tz = static_cast<float>(luaL_checknumber(L, 4));
    dse_rigidbody3d_add_torque(e, tx, ty, tz);
    return 0;
}

int L_dse_rigidbody3d_set_angular_velocity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float ax = static_cast<float>(luaL_checknumber(L, 2));
    float ay = static_cast<float>(luaL_checknumber(L, 3));
    float az = static_cast<float>(luaL_checknumber(L, 4));
    dse_rigidbody3d_set_angular_velocity(e, ax, ay, az);
    return 0;
}

int L_dse_rigidbody3d_set_velocity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float vx = static_cast<float>(luaL_checknumber(L, 2));
    float vy = static_cast<float>(luaL_checknumber(L, 3));
    float vz = static_cast<float>(luaL_checknumber(L, 4));
    dse_rigidbody3d_set_velocity(e, vx, vy, vz);
    return 0;
}

int L_dse_rigidbody3d_set_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_rigidbody3d_set_gravity(e, enabled);
    return 0;
}

int L_dse_character_controller3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float radius = static_cast<float>(luaL_checknumber(L, 2));
    float height = static_cast<float>(luaL_checknumber(L, 3));
    float slope_limit = static_cast<float>(luaL_optnumber(L, 4, 45.0));
    float step_offset = static_cast<float>(luaL_optnumber(L, 5, 0.3));
    dse_character_controller3d_add(e, radius, height, slope_limit, step_offset);
    return 0;
}

int L_dse_character_controller3d_jump(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float jump_speed = static_cast<float>(luaL_checknumber(L, 2));
    int _ret = dse_character_controller3d_jump(e, jump_speed);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_character_controller3d_is_grounded(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_character_controller3d_is_grounded(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_terrain_heightmap_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float origin_x = static_cast<float>(luaL_checknumber(L, 2));
    float origin_z = static_cast<float>(luaL_checknumber(L, 3));
    float block_size = static_cast<float>(luaL_checknumber(L, 4));
    int cols = static_cast<int>(luaL_checkinteger(L, 5));
    int rows = static_cast<int>(luaL_checkinteger(L, 6));
    float scale = static_cast<float>(luaL_checknumber(L, 7));
    int flip_z = helper::CheckBool(L, 8) ? 1 : 0;
    dse_terrain_heightmap_add(e, origin_x, origin_z, block_size, cols, rows, scale, flip_z);
    return 0;
}

int L_dse_mesh_collider3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int convex = helper::CheckBool(L, 2) ? 1 : 0;
    int is_trigger = helper::OptBool(L, 3, false) ? 1 : 0;
    dse_mesh_collider3d_add(e, convex, is_trigger);
    return 0;
}

int L_dse_capsule_collider3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float radius = static_cast<float>(luaL_checknumber(L, 2));
    float height = static_cast<float>(luaL_checknumber(L, 3));
    int direction = static_cast<int>(luaL_checkinteger(L, 4));
    int is_trigger = static_cast<int>(luaL_optinteger(L, 5, 0));
    dse_capsule_collider3d_add(e, radius, height, direction, is_trigger);
    return 0;
}

int L_dse_joint3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t connected_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    int type = static_cast<int>(luaL_checkinteger(L, 3));
    float ax = static_cast<float>(luaL_checknumber(L, 4));
    float ay = static_cast<float>(luaL_checknumber(L, 5));
    float az = static_cast<float>(luaL_checknumber(L, 6));
    float bx = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float by = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float bz = static_cast<float>(luaL_optnumber(L, 9, 0.0));
    float break_force = static_cast<float>(luaL_optnumber(L, 10, 0.0));
    float break_torque = static_cast<float>(luaL_optnumber(L, 11, 0.0));
    dse_joint3d_add(e, connected_id, type, ax, ay, az, bx, by, bz, break_force, break_torque);
    return 0;
}

int L_dse_joint3d_set_hinge_limits(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float lower_deg = static_cast<float>(luaL_checknumber(L, 2));
    float upper_deg = static_cast<float>(luaL_checknumber(L, 3));
    dse_joint3d_set_hinge_limits(e, lower_deg, upper_deg);
    return 0;
}

int L_dse_joint3d_set_spring(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float stiffness = static_cast<float>(luaL_checknumber(L, 2));
    float damping = static_cast<float>(luaL_checknumber(L, 3));
    dse_joint3d_set_spring(e, stiffness, damping);
    return 0;
}

int L_dse_joint3d_set_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float min_dist = static_cast<float>(luaL_checknumber(L, 2));
    float max_dist = static_cast<float>(luaL_checknumber(L, 3));
    dse_joint3d_set_distance(e, min_dist, max_dist);
    return 0;
}

int L_dse_joint3d_is_broken(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_joint3d_is_broken(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_collision_set_layer(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int layer = static_cast<int>(luaL_checkinteger(L, 2));
    int mask = static_cast<int>(luaL_checkinteger(L, 3));
    dse_collision_set_layer(e, layer, mask);
    return 0;
}

int L_dse_collider_set_trigger(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int is_trigger = helper::CheckBool(L, 2) ? 1 : 0;
    dse_collider_set_trigger(e, is_trigger);
    return 0;
}

int L_dse_collider_set_material(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float friction = static_cast<float>(luaL_checknumber(L, 2));
    float bounciness = static_cast<float>(luaL_checknumber(L, 3));
    dse_collider_set_material(e, friction, bounciness);
    return 0;
}

int L_dse_physics3d_overlap_sphere(lua_State* L) {
    float cx = static_cast<float>(luaL_checknumber(L, 1));
    float cy = static_cast<float>(luaL_checknumber(L, 2));
    float cz = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));
    uint32_t _buf[256];
    int _count = dse_physics3d_overlap_sphere(cx, cy, cz, radius, _buf, 256);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_pushinteger(L, static_cast<lua_Integer>(_buf[_i]));
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

int L_dse_physics3d_overlap_box(lua_State* L) {
    float min_x = static_cast<float>(luaL_checknumber(L, 1));
    float min_y = static_cast<float>(luaL_checknumber(L, 2));
    float min_z = static_cast<float>(luaL_checknumber(L, 3));
    float max_x = static_cast<float>(luaL_checknumber(L, 4));
    float max_y = static_cast<float>(luaL_checknumber(L, 5));
    float max_z = static_cast<float>(luaL_checknumber(L, 6));
    uint32_t _buf[256];
    int _count = dse_physics3d_overlap_box(min_x, min_y, min_z, max_x, max_y, max_z, _buf, 256);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_pushinteger(L, static_cast<lua_Integer>(_buf[_i]));
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

int L_dse_physics3d_get_collision_events(lua_State* L) {
    float _buf[2816];
    int _count = dse_physics3d_get_collision_events(_buf, 256);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        float* _p = _buf + _i * 11;
        lua_newtable(L);
        lua_pushinteger(L, static_cast<lua_Integer>(_p[0])); lua_setfield(L, -2, "type");
        lua_pushinteger(L, static_cast<lua_Integer>(_p[1])); lua_setfield(L, -2, "entity_a");
        lua_pushinteger(L, static_cast<lua_Integer>(_p[2])); lua_setfield(L, -2, "entity_b");
        lua_pushnumber(L, _p[3]); lua_setfield(L, -2, "cx");
        lua_pushnumber(L, _p[4]); lua_setfield(L, -2, "cy");
        lua_pushnumber(L, _p[5]); lua_setfield(L, -2, "cz");
        lua_pushnumber(L, _p[6]); lua_setfield(L, -2, "nx");
        lua_pushnumber(L, _p[7]); lua_setfield(L, -2, "ny");
        lua_pushnumber(L, _p[8]); lua_setfield(L, -2, "nz");
        lua_pushnumber(L, _p[9]); lua_setfield(L, -2, "impulse");
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

int L_dse_physics3d_get_trigger_events(lua_State* L) {
    uint32_t _ent_buf[512];
    int _type_buf[256];
    int _count = dse_physics3d_get_trigger_events(_ent_buf, _type_buf, 256);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_newtable(L);
        lua_pushinteger(L, static_cast<lua_Integer>(_ent_buf[_i * 2])); lua_setfield(L, -2, "trigger_entity");
        lua_pushinteger(L, static_cast<lua_Integer>(_ent_buf[_i * 2 + 1])); lua_setfield(L, -2, "other_entity");
        lua_pushinteger(L, _type_buf[_i]); lua_setfield(L, -2, "type");
        lua_rawseti(L, -2, _i + 1);
    }
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
    lua_pushboolean(L, _ret);
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

int L_dse_rigidbody3d_get_velocity(lua_State* L) {
    float out_vel[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_get_velocity(e, out_vel);
    lua_pushnumber(L, out_vel[0]);
    lua_pushnumber(L, out_vel[1]);
    lua_pushnumber(L, out_vel[2]);
    return 3;
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
    lua_pushboolean(L, _ret);
    lua_pushnumber(L, out_velocity[0]);
    lua_pushnumber(L, out_velocity[1]);
    lua_pushnumber(L, out_velocity[2]);
    lua_pushinteger(L, static_cast<lua_Integer>(out_flags));
    return 5;
}

} // namespace

void RegisterEcsPhysics3DBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"add_rigidbody_3d", L_dse_rigidbody3d_add},
        {"add_box_collider_3d", L_dse_box_collider3d_add},
        {"add_sphere_collider_3d", L_dse_sphere_collider3d_add},
        {"rigidbody_3d_add_force", L_dse_rigidbody3d_add_force},
        {"rigidbody_3d_add_impulse", L_dse_rigidbody3d_add_impulse},
        {"rigidbody_3d_add_torque", L_dse_rigidbody3d_add_torque},
        {"rigidbody_3d_set_angular_velocity", L_dse_rigidbody3d_set_angular_velocity},
        {"rigidbody_3d_set_velocity", L_dse_rigidbody3d_set_velocity},
        {"rigidbody_3d_set_gravity", L_dse_rigidbody3d_set_gravity},
        {"add_character_controller_3d", L_dse_character_controller3d_add},
        {"character_controller_3d_jump", L_dse_character_controller3d_jump},
        {"character_controller_3d_is_grounded", L_dse_character_controller3d_is_grounded},
        {"add_terrain_heightmap", L_dse_terrain_heightmap_add},
        {"add_mesh_collider_3d", L_dse_mesh_collider3d_add},
        {"add_capsule_collider_3d", L_dse_capsule_collider3d_add},
        {"add_joint_3d", L_dse_joint3d_add},
        {"set_joint_3d_hinge_limits", L_dse_joint3d_set_hinge_limits},
        {"set_joint_3d_spring", L_dse_joint3d_set_spring},
        {"set_joint_3d_distance", L_dse_joint3d_set_distance},
        {"is_joint_3d_broken", L_dse_joint3d_is_broken},
        {"set_collision_layer", L_dse_collision_set_layer},
        {"set_collider_trigger", L_dse_collider_set_trigger},
        {"set_collider_material", L_dse_collider_set_material},
        {"physics_3d_overlap_sphere", L_dse_physics3d_overlap_sphere},
        {"physics3d_overlap_box", L_dse_physics3d_overlap_box},
        {"physics3d_get_collision_events", L_dse_physics3d_get_collision_events},
        {"physics3d_get_trigger_events", L_dse_physics3d_get_trigger_events},
        {"physics_3d_raycast", L_dse_physics3d_raycast},
        {"rigidbody_3d_get_velocity", L_dse_rigidbody3d_get_velocity},
        {"character_controller_3d_move", L_dse_character_controller3d_move},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
