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

namespace dse::runtime::lua_binding {
namespace {

int L_dse_rigidbody3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int type = static_cast<int>(luaL_checkinteger(L, 2));
    float mass = static_cast<float>(luaL_checknumber(L, 3));
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
    float slope_limit = static_cast<float>(luaL_checknumber(L, 4));
    float step_offset = static_cast<float>(luaL_checknumber(L, 5));
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
    int flip_z = static_cast<int>(luaL_checkinteger(L, 8));
    dse_terrain_heightmap_add(e, origin_x, origin_z, block_size, cols, rows, scale, flip_z);
    return 0;
}

int L_dse_mesh_collider3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int convex = static_cast<int>(luaL_checkinteger(L, 2));
    int is_trigger = static_cast<int>(luaL_checkinteger(L, 3));
    dse_mesh_collider3d_add(e, convex, is_trigger);
    return 0;
}

int L_dse_capsule_collider3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float radius = static_cast<float>(luaL_checknumber(L, 2));
    float height = static_cast<float>(luaL_checknumber(L, 3));
    int direction = static_cast<int>(luaL_checkinteger(L, 4));
    int is_trigger = static_cast<int>(luaL_checkinteger(L, 5));
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
    float bx = static_cast<float>(luaL_checknumber(L, 7));
    float by = static_cast<float>(luaL_checknumber(L, 8));
    float bz = static_cast<float>(luaL_checknumber(L, 9));
    float break_force = static_cast<float>(luaL_checknumber(L, 10));
    float break_torque = static_cast<float>(luaL_checknumber(L, 11));
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
    int is_trigger = static_cast<int>(luaL_checkinteger(L, 2));
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

} // namespace

void RegisterEcsPhysics3DBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
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
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
