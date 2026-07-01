/**
 * @file lua_binding_ecs_free_camera.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * FreeCameraControllerComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_free_camera_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_free_camera_get_enabled(e));
    return 1;
}
int L_Set_free_camera_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_free_camera_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_free_camera_move_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_free_camera_get_move_speed(e));
    return 1;
}
int L_Set_free_camera_move_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_free_camera_set_move_speed(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_free_camera_mouse_sensitivity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_free_camera_get_mouse_sensitivity(e));
    return 1;
}
int L_Set_free_camera_mouse_sensitivity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_free_camera_set_mouse_sensitivity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_free_camera_pitch(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_free_camera_get_pitch(e));
    return 1;
}
int L_Set_free_camera_pitch(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_free_camera_set_pitch(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_free_camera_yaw(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_free_camera_get_yaw(e));
    return 1;
}
int L_Set_free_camera_yaw(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_free_camera_set_yaw(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterFreeCameraControllerComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_free_camera_enabled", L_Get_free_camera_enabled},
        {"set_free_camera_enabled", L_Set_free_camera_enabled},
        {"get_free_camera_move_speed", L_Get_free_camera_move_speed},
        {"set_free_camera_move_speed", L_Set_free_camera_move_speed},
        {"get_free_camera_mouse_sensitivity", L_Get_free_camera_mouse_sensitivity},
        {"set_free_camera_mouse_sensitivity", L_Set_free_camera_mouse_sensitivity},
        {"get_free_camera_pitch", L_Get_free_camera_pitch},
        {"set_free_camera_pitch", L_Set_free_camera_pitch},
        {"get_free_camera_yaw", L_Get_free_camera_yaw},
        {"set_free_camera_yaw", L_Set_free_camera_yaw},
    });
}

} // namespace dse::runtime::lua_binding
