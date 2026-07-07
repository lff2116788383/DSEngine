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

} // namespace

void RegisterNavigationBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "nav");
    helper::RegisterBindings(L, {
        {"navisready", L_dse_nav_is_ready},
        {"navload", L_dse_nav_load},
        {"navsave", L_dse_nav_save},
        {"ecssetnavagent", L_dse_nav_agent_set},
        {"ecssetnavdestination", L_dse_nav_agent_set_destination},
        {"ecsnavagenthaspath", L_dse_nav_agent_has_path},
        {"ecsnavagentarrived", L_dse_nav_agent_arrived},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
