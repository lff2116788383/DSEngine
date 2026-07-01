/**
 * @file lua_binding_ecs_character_ctrl3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * CharacterController3DComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_character_ctrl3d_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_ctrl3d_get_radius(e));
    return 1;
}
int L_Set_character_ctrl3d_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_ctrl3d_set_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_ctrl3d_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_ctrl3d_get_height(e));
    return 1;
}
int L_Set_character_ctrl3d_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_ctrl3d_set_height(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_ctrl3d_slope_limit(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_ctrl3d_get_slope_limit(e));
    return 1;
}
int L_Set_character_ctrl3d_slope_limit(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_ctrl3d_set_slope_limit(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_ctrl3d_step_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_ctrl3d_get_step_offset(e));
    return 1;
}
int L_Set_character_ctrl3d_step_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_ctrl3d_set_step_offset(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_ctrl3d_skin_width(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_ctrl3d_get_skin_width(e));
    return 1;
}
int L_Set_character_ctrl3d_skin_width(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_ctrl3d_set_skin_width(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_ctrl3d_min_move_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_ctrl3d_get_min_move_distance(e));
    return 1;
}
int L_Set_character_ctrl3d_min_move_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_ctrl3d_set_min_move_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterCharacterController3DComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_character_ctrl3d_radius", L_Get_character_ctrl3d_radius},
        {"set_character_ctrl3d_radius", L_Set_character_ctrl3d_radius},
        {"get_character_ctrl3d_height", L_Get_character_ctrl3d_height},
        {"set_character_ctrl3d_height", L_Set_character_ctrl3d_height},
        {"get_character_ctrl3d_slope_limit", L_Get_character_ctrl3d_slope_limit},
        {"set_character_ctrl3d_slope_limit", L_Set_character_ctrl3d_slope_limit},
        {"get_character_ctrl3d_step_offset", L_Get_character_ctrl3d_step_offset},
        {"set_character_ctrl3d_step_offset", L_Set_character_ctrl3d_step_offset},
        {"get_character_ctrl3d_skin_width", L_Get_character_ctrl3d_skin_width},
        {"set_character_ctrl3d_skin_width", L_Set_character_ctrl3d_skin_width},
        {"get_character_ctrl3d_min_move_distance", L_Get_character_ctrl3d_min_move_distance},
        {"set_character_ctrl3d_min_move_distance", L_Set_character_ctrl3d_min_move_distance},
    });
}

} // namespace dse::runtime::lua_binding
