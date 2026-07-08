/**
 * @file lua_binding_free_ecs_camera.gen.cpp
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

int L_dse_camera_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float ortho_size = static_cast<float>(luaL_checknumber(L, 2));
    int priority = static_cast<int>(luaL_checkinteger(L, 3));
    dse_camera_add(e, ortho_size, priority);
    return 0;
}

int L_dse_camera_set_priority(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int priority = static_cast<int>(luaL_checkinteger(L, 2));
    dse_camera_set_priority(e, priority);
    return 0;
}

int L_dse_camera_set_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_camera_set_enabled(e, enabled);
    return 0;
}

int L_dse_camera_set_follow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t target = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    float damping = static_cast<float>(luaL_checknumber(L, 3));
    float dead_zone_x = static_cast<float>(luaL_checknumber(L, 4));
    float dead_zone_y = static_cast<float>(luaL_checknumber(L, 5));
    float offset_x = static_cast<float>(luaL_checknumber(L, 6));
    float offset_y = static_cast<float>(luaL_checknumber(L, 7));
    dse_camera_set_follow(e, target, damping, dead_zone_x, dead_zone_y, offset_x, offset_y);
    return 0;
}

int L_dse_free_camera_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float move_speed = static_cast<float>(luaL_checknumber(L, 2));
    float mouse_sensitivity = static_cast<float>(luaL_checknumber(L, 3));
    dse_free_camera_add(e, move_speed, mouse_sensitivity);
    return 0;
}

int L_dse_sprite_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_checknumber(L, 5));
    int order_in_layer = static_cast<int>(luaL_checkinteger(L, 6));
    uint32_t texture_handle = static_cast<uint32_t>(luaL_checkinteger(L, 7));
    dse_sprite_add(e, r, g, b, a, order_in_layer, texture_handle);
    return 0;
}

int L_dse_sprite_set_uv_scroll(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float sx = static_cast<float>(luaL_checknumber(L, 2));
    float sy = static_cast<float>(luaL_checknumber(L, 3));
    dse_sprite_set_uv_scroll(e, sx, sy);
    return 0;
}

int L_dse_sprite_set_uv_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float ox = static_cast<float>(luaL_checknumber(L, 2));
    float oy = static_cast<float>(luaL_checknumber(L, 3));
    dse_sprite_set_uv_offset(e, ox, oy);
    return 0;
}

int L_add_camera_3d(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float fov = static_cast<float>(luaL_checknumber(L, 2));
    int priority = static_cast<int>(luaL_checkinteger(L, 3));
    dse_camera3d_add(e, fov, 0.1f, 1000.0f);
    dse_camera3d_set_priority(e, priority);
    return 0;
}

int L_dse_compat_world_to_screen(lua_State* L) {
    float out_sx = 0;
    float out_sy = 0;
    int out_visible = 0;
    float wx = static_cast<float>(luaL_checknumber(L, 1));
    float wy = static_cast<float>(luaL_checknumber(L, 2));
    float wz = static_cast<float>(luaL_checknumber(L, 3));
    dse_compat_world_to_screen(wx, wy, wz, &out_sx, &out_sy, &out_visible);
    lua_pushnumber(L, out_sx);
    lua_pushnumber(L, out_sy);
    lua_pushboolean(L, out_visible);
    return 3;
}

} // namespace

void RegisterEcsRenderingCameraBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"add_camera", L_dse_camera_add},
        {"set_camera_priority", L_dse_camera_set_priority},
        {"set_camera_enabled", L_dse_camera_set_enabled},
        {"set_camera_follow", L_dse_camera_set_follow},
        {"add_free_camera_controller", L_dse_free_camera_add},
        {"add_sprite", L_dse_sprite_add},
        {"set_sprite_uv_scroll", L_dse_sprite_set_uv_scroll},
        {"set_sprite_uv_offset", L_dse_sprite_set_uv_offset},
        {"add_camera_3d", L_add_camera_3d},
        {"world_to_screen", L_dse_compat_world_to_screen},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
