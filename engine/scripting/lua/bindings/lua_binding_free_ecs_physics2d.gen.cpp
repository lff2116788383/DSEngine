/**
 * @file lua_binding_free_ecs_physics2d.gen.cpp
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

int L_dse_physics2d_add_rigidbody(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int type = static_cast<int>(luaL_checkinteger(L, 2));
    float gravity_scale = static_cast<float>(luaL_checknumber(L, 3));
    int fixed_rotation = static_cast<int>(luaL_checkinteger(L, 4));
    dse_physics2d_add_rigidbody(e, type, gravity_scale, fixed_rotation);
    return 0;
}

int L_dse_physics2d_set_rigidbody_velocity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float vx = static_cast<float>(luaL_checknumber(L, 2));
    float vy = static_cast<float>(luaL_checknumber(L, 3));
    dse_physics2d_set_rigidbody_velocity(e, vx, vy);
    return 0;
}

int L_dse_physics2d_add_box_collider(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float w = static_cast<float>(luaL_checknumber(L, 2));
    float h = static_cast<float>(luaL_checknumber(L, 3));
    float density = static_cast<float>(luaL_checknumber(L, 4));
    float friction = static_cast<float>(luaL_checknumber(L, 5));
    float restitution = static_cast<float>(luaL_checknumber(L, 6));
    dse_physics2d_add_box_collider(e, w, h, density, friction, restitution);
    return 0;
}

int L_dse_physics2d_set_box_collider_trigger(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int is_trigger = static_cast<int>(luaL_checkinteger(L, 2));
    dse_physics2d_set_box_collider_trigger(e, is_trigger);
    return 0;
}

int L_dse_physics2d_add_circle_collider(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float radius = static_cast<float>(luaL_checknumber(L, 2));
    float density = static_cast<float>(luaL_checknumber(L, 3));
    float friction = static_cast<float>(luaL_checknumber(L, 4));
    float restitution = static_cast<float>(luaL_checknumber(L, 5));
    dse_physics2d_add_circle_collider(e, radius, density, friction, restitution);
    return 0;
}

int L_dse_physics2d_set_circle_collider_trigger(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int is_trigger = static_cast<int>(luaL_checkinteger(L, 2));
    dse_physics2d_set_circle_collider_trigger(e, is_trigger);
    return 0;
}

int L_dse_physics2d_set_polygon_collider_trigger(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int is_trigger = static_cast<int>(luaL_checkinteger(L, 2));
    dse_physics2d_set_polygon_collider_trigger(e, is_trigger);
    return 0;
}

int L_dse_physics2d_add_joint(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int type = static_cast<int>(luaL_checkinteger(L, 2));
    uint32_t entity_a = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    uint32_t entity_b = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    float ax = static_cast<float>(luaL_checknumber(L, 5));
    float ay = static_cast<float>(luaL_checknumber(L, 6));
    float bx = static_cast<float>(luaL_checknumber(L, 7));
    float by = static_cast<float>(luaL_checknumber(L, 8));
    int collide_connected = static_cast<int>(luaL_checkinteger(L, 9));
    dse_physics2d_add_joint(e, type, entity_a, entity_b, ax, ay, bx, by, collide_connected);
    return 0;
}

int L_dse_physics2d_set_joint_revolute(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enable_limit = static_cast<int>(luaL_checkinteger(L, 2));
    float lower_deg = static_cast<float>(luaL_checknumber(L, 3));
    float upper_deg = static_cast<float>(luaL_checknumber(L, 4));
    int enable_motor = static_cast<int>(luaL_checkinteger(L, 5));
    float motor_speed = static_cast<float>(luaL_checknumber(L, 6));
    float max_torque = static_cast<float>(luaL_checknumber(L, 7));
    dse_physics2d_set_joint_revolute(e, enable_limit, lower_deg, upper_deg, enable_motor, motor_speed, max_torque);
    return 0;
}

int L_dse_physics2d_set_joint_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float min_len = static_cast<float>(luaL_checknumber(L, 2));
    float max_len = static_cast<float>(luaL_checknumber(L, 3));
    float stiffness = static_cast<float>(luaL_checknumber(L, 4));
    float damping = static_cast<float>(luaL_checknumber(L, 5));
    dse_physics2d_set_joint_distance(e, min_len, max_len, stiffness, damping);
    return 0;
}

int L_dse_physics2d_set_joint_prismatic(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float axis_x = static_cast<float>(luaL_checknumber(L, 2));
    float axis_y = static_cast<float>(luaL_checknumber(L, 3));
    int enable_limit = static_cast<int>(luaL_checkinteger(L, 4));
    float lower = static_cast<float>(luaL_checknumber(L, 5));
    float upper = static_cast<float>(luaL_checknumber(L, 6));
    int enable_motor = static_cast<int>(luaL_checkinteger(L, 7));
    float motor_speed = static_cast<float>(luaL_checknumber(L, 8));
    float max_force = static_cast<float>(luaL_checknumber(L, 9));
    dse_physics2d_set_joint_prismatic(e, axis_x, axis_y, enable_limit, lower, upper, enable_motor, motor_speed, max_force);
    return 0;
}

int L_dse_physics2d_destroy_joint(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_physics2d_destroy_joint(e);
    return 0;
}

int L_dse_physics2d_add_tilemap(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int width = static_cast<int>(luaL_checkinteger(L, 2));
    int height = static_cast<int>(luaL_checkinteger(L, 3));
    float tile_size = static_cast<float>(luaL_checknumber(L, 4));
    uint32_t tex_handle = static_cast<uint32_t>(luaL_checkinteger(L, 5));
    dse_physics2d_add_tilemap(e, width, height, tile_size, tex_handle);
    return 0;
}

int L_dse_physics2d_set_tile(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int x = static_cast<int>(luaL_checkinteger(L, 2));
    int y = static_cast<int>(luaL_checkinteger(L, 3));
    int tile_id = static_cast<int>(luaL_checkinteger(L, 4));
    dse_physics2d_set_tile(e, x, y, tile_id);
    return 0;
}

} // namespace

void RegisterEcsPhysics2DBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    helper::RegisterBindings(L, {
        {"add_rigid_body", L_dse_physics2d_add_rigidbody},
        {"set_rigid_body_velocity", L_dse_physics2d_set_rigidbody_velocity},
        {"add_box_collider", L_dse_physics2d_add_box_collider},
        {"set_box_collider_trigger", L_dse_physics2d_set_box_collider_trigger},
        {"add_circle_collider", L_dse_physics2d_add_circle_collider},
        {"set_circle_collider_trigger", L_dse_physics2d_set_circle_collider_trigger},
        {"set_polygon_collider_trigger", L_dse_physics2d_set_polygon_collider_trigger},
        {"add_joint_2d", L_dse_physics2d_add_joint},
        {"set_joint_2d_revolute", L_dse_physics2d_set_joint_revolute},
        {"set_joint_2d_distance", L_dse_physics2d_set_joint_distance},
        {"set_joint_2d_prismatic", L_dse_physics2d_set_joint_prismatic},
        {"destroy_joint_2d", L_dse_physics2d_destroy_joint},
        {"add_tilemap", L_dse_physics2d_add_tilemap},
        {"set_tile", L_dse_physics2d_set_tile},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
