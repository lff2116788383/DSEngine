/**
 * @file lua_binding_ecs_character_movement.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * CharacterMovementState 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_character_movement_input_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_character_movement_get_input_direction(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_character_movement_input_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_set_input_direction(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_character_movement_input_jump(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_character_movement_get_input_jump(e));
    return 1;
}
int L_Set_character_movement_input_jump(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_set_input_jump(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_character_movement_input_sprint(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_character_movement_get_input_sprint(e));
    return 1;
}
int L_Set_character_movement_input_sprint(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_set_input_sprint(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_character_movement_input_crouch(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_character_movement_get_input_crouch(e));
    return 1;
}
int L_Set_character_movement_input_crouch(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_set_input_crouch(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_character_movement_velocity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_character_movement_get_velocity(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Get_character_movement_is_grounded(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_character_movement_get_is_grounded(e));
    return 1;
}
int L_Get_character_movement_is_jumping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_character_movement_get_is_jumping(e));
    return 1;
}
int L_Get_character_movement_jump_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_character_movement_get_jump_count(e));
    return 1;
}

} // namespace

void RegisterCharacterMovementStateGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_character_movement_input_direction", L_Get_character_movement_input_direction},
        {"set_character_movement_input_direction", L_Set_character_movement_input_direction},
        {"get_character_movement_input_jump", L_Get_character_movement_input_jump},
        {"set_character_movement_input_jump", L_Set_character_movement_input_jump},
        {"get_character_movement_input_sprint", L_Get_character_movement_input_sprint},
        {"set_character_movement_input_sprint", L_Set_character_movement_input_sprint},
        {"get_character_movement_input_crouch", L_Get_character_movement_input_crouch},
        {"set_character_movement_input_crouch", L_Set_character_movement_input_crouch},
        {"get_character_movement_velocity", L_Get_character_movement_velocity},
        {"get_character_movement_is_grounded", L_Get_character_movement_is_grounded},
        {"get_character_movement_is_jumping", L_Get_character_movement_is_jumping},
        {"get_character_movement_jump_count", L_Get_character_movement_jump_count},
    });
}

} // namespace dse::runtime::lua_binding
