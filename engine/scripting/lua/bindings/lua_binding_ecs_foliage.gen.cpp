/**
 * @file lua_binding_ecs_foliage.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * FoliageComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_foliage_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_foliage_get_enabled(e));
    return 1;
}
int L_Set_foliage_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_foliage_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_foliage_wind_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_foliage_get_wind_strength(e));
    return 1;
}
int L_Set_foliage_wind_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_foliage_set_wind_strength(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_foliage_stiffness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_foliage_get_stiffness(e));
    return 1;
}
int L_Set_foliage_stiffness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_foliage_set_stiffness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_foliage_phase_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_foliage_get_phase_offset(e));
    return 1;
}
int L_Set_foliage_phase_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_foliage_set_phase_offset(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_foliage_push_response(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_foliage_get_push_response(e));
    return 1;
}
int L_Set_foliage_push_response(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_foliage_set_push_response(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterFoliageComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_foliage_enabled", L_Get_foliage_enabled},
        {"set_foliage_enabled", L_Set_foliage_enabled},
        {"get_foliage_wind_strength", L_Get_foliage_wind_strength},
        {"set_foliage_wind_strength", L_Set_foliage_wind_strength},
        {"get_foliage_stiffness", L_Get_foliage_stiffness},
        {"set_foliage_stiffness", L_Set_foliage_stiffness},
        {"get_foliage_phase_offset", L_Get_foliage_phase_offset},
        {"set_foliage_phase_offset", L_Set_foliage_phase_offset},
        {"get_foliage_push_response", L_Get_foliage_push_response},
        {"set_foliage_push_response", L_Set_foliage_push_response},
    });
}

} // namespace dse::runtime::lua_binding
