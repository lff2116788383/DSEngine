/**
 * @file lua_binding_ecs_player_controller.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * PlayerControllerComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_player_controller_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_player_controller_get_enabled(e));
    return 1;
}
int L_Set_player_controller_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_player_controller_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_player_controller_mouse_sensitivity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_player_controller_get_mouse_sensitivity(e));
    return 1;
}
int L_Set_player_controller_mouse_sensitivity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_player_controller_set_mouse_sensitivity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_player_controller_gamepad_sensitivity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_player_controller_get_gamepad_sensitivity(e));
    return 1;
}
int L_Set_player_controller_gamepad_sensitivity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_player_controller_set_gamepad_sensitivity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_player_controller_invert_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_player_controller_get_invert_y(e));
    return 1;
}
int L_Set_player_controller_invert_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_player_controller_set_invert_y(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_player_controller_stick_dead_zone(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_player_controller_get_stick_dead_zone(e));
    return 1;
}
int L_Set_player_controller_stick_dead_zone(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_player_controller_set_stick_dead_zone(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_player_controller_move_response_curve(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_player_controller_get_move_response_curve(e));
    return 1;
}
int L_Set_player_controller_move_response_curve(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_player_controller_set_move_response_curve(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_player_controller_look_response_curve(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_player_controller_get_look_response_curve(e));
    return 1;
}
int L_Set_player_controller_look_response_curve(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_player_controller_set_look_response_curve(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterPlayerControllerComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_player_controller_enabled", L_Get_player_controller_enabled},
        {"set_player_controller_enabled", L_Set_player_controller_enabled},
        {"get_player_controller_mouse_sensitivity", L_Get_player_controller_mouse_sensitivity},
        {"set_player_controller_mouse_sensitivity", L_Set_player_controller_mouse_sensitivity},
        {"get_player_controller_gamepad_sensitivity", L_Get_player_controller_gamepad_sensitivity},
        {"set_player_controller_gamepad_sensitivity", L_Set_player_controller_gamepad_sensitivity},
        {"get_player_controller_invert_y", L_Get_player_controller_invert_y},
        {"set_player_controller_invert_y", L_Set_player_controller_invert_y},
        {"get_player_controller_stick_dead_zone", L_Get_player_controller_stick_dead_zone},
        {"set_player_controller_stick_dead_zone", L_Set_player_controller_stick_dead_zone},
        {"get_player_controller_move_response_curve", L_Get_player_controller_move_response_curve},
        {"set_player_controller_move_response_curve", L_Set_player_controller_move_response_curve},
        {"get_player_controller_look_response_curve", L_Get_player_controller_look_response_curve},
        {"set_player_controller_look_response_curve", L_Set_player_controller_look_response_curve},
    });
}

} // namespace dse::runtime::lua_binding
