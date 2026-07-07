/**
 * @file lua_binding_free_navigation_gap.gen.cpp
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

int L_dse_nav_agent_get(lua_State* L) {
    float out_params = 0;
    int out_flags = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_nav_agent_get(e, &out_params, &out_flags);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_params);
    lua_pushinteger(L, out_flags);
    return 3;
}

int L_dse_nav_agent_get_destination(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_nav_agent_get_destination(e, out_xyz);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 3;
}

int L_dse_nav_find_nearest(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    int _ret = dse_nav_find_nearest(x, y, z, out_xyz);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 4;
}

int L_dse_nav_raycast(lua_State* L) {
    float out_hit_xyz[3] = {0, 0, 0};
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));
    float sz = static_cast<float>(luaL_checknumber(L, 3));
    float ex = static_cast<float>(luaL_checknumber(L, 4));
    float ey = static_cast<float>(luaL_checknumber(L, 5));
    float ez = static_cast<float>(luaL_checknumber(L, 6));
    int _ret = dse_nav_raycast(sx, sy, sz, ex, ey, ez, out_hit_xyz);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_hit_xyz[0]);
    lua_pushnumber(L, out_hit_xyz[1]);
    lua_pushnumber(L, out_hit_xyz[2]);
    return 4;
}

} // namespace

void RegisterFreeNavigationGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"nav_agent_get", L_dse_nav_agent_get},
        {"nav_agent_get_destination", L_dse_nav_agent_get_destination},
        {"nav_find_nearest", L_dse_nav_find_nearest},
        {"nav_raycast", L_dse_nav_raycast},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
