/**
 * @file lua_binding_free_navigation.gen.cpp
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

int L_dse_nav_is_ready(lua_State* L) {
    int _ret = dse_nav_is_ready();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_nav_load(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_nav_load(path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_nav_save(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_nav_save(path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_nav_agent_set(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float speed = static_cast<float>(luaL_checknumber(L, 2));
    float acceleration = static_cast<float>(luaL_checknumber(L, 3));
    float stopping_dist = static_cast<float>(luaL_checknumber(L, 4));
    float radius = static_cast<float>(luaL_checknumber(L, 5));
    float height = static_cast<float>(luaL_checknumber(L, 6));
    dse_nav_agent_set(e, speed, acceleration, stopping_dist, radius, height);
    return 0;
}

int L_dse_nav_agent_set_destination(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    dse_nav_agent_set_destination(e, x, y, z);
    return 0;
}

int L_dse_nav_agent_has_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_nav_agent_has_path(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_nav_agent_arrived(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_nav_agent_arrived(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_nav_find_path(lua_State* L) {
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));
    float sz = static_cast<float>(luaL_checknumber(L, 3));
    float ex = static_cast<float>(luaL_checknumber(L, 4));
    float ey = static_cast<float>(luaL_checknumber(L, 5));
    float ez = static_cast<float>(luaL_checknumber(L, 6));
    float _buf[768];
    int _count = dse_nav_find_path(sx, sy, sz, ex, ey, ez, _buf, 256);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_newtable(L);
        lua_pushnumber(L, _buf[_i * 3 + 0]); lua_setfield(L, -2, "x");
        lua_pushnumber(L, _buf[_i * 3 + 1]); lua_setfield(L, -2, "y");
        lua_pushnumber(L, _buf[_i * 3 + 2]); lua_setfield(L, -2, "z");
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

int L_dse_nav_bake(lua_State* L) {
    float verts = static_cast<float>(luaL_checknumber(L, 1));
    int nverts = static_cast<int>(luaL_checkinteger(L, 2));
    int tris = static_cast<int>(luaL_checkinteger(L, 3));
    int ntris = static_cast<int>(luaL_checkinteger(L, 4));
    float cell_size = static_cast<float>(luaL_checknumber(L, 5));
    float cell_height = static_cast<float>(luaL_checknumber(L, 6));
    float agent_height = static_cast<float>(luaL_checknumber(L, 7));
    float agent_radius = static_cast<float>(luaL_checknumber(L, 8));
    float agent_max_climb = static_cast<float>(luaL_checknumber(L, 9));
    float agent_max_slope = static_cast<float>(luaL_checknumber(L, 10));
    int _ret = dse_nav_bake(verts, nverts, tris, ntris, cell_size, cell_height, agent_height, agent_radius, agent_max_climb, agent_max_slope);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterNavigationBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "nav");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "nav");
    }
    helper::RegisterBindings(L, {
        {"navisready", L_dse_nav_is_ready},
        {"navload", L_dse_nav_load},
        {"navsave", L_dse_nav_save},
        {"ecssetnavagent", L_dse_nav_agent_set},
        {"ecssetnavdestination", L_dse_nav_agent_set_destination},
        {"ecsnavagenthaspath", L_dse_nav_agent_has_path},
        {"ecsnavagentarrived", L_dse_nav_agent_arrived},
        {"find_path", L_dse_nav_find_path},
        {"nav_bake", L_dse_nav_bake},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
