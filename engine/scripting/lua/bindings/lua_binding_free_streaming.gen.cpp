/**
 * @file lua_binding_free_streaming.gen.cpp
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

int L_dse_streaming_create_zone(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float load_r = static_cast<float>(luaL_checknumber(L, 5));
    float unload_r = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    uint32_t _ret = dse_streaming_create_zone(name, x, y, z, load_r, unload_r);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_streaming_destroy_zone(lua_State* L) {
    uint32_t zone = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_streaming_destroy_zone(zone);
    return 0;
}

int L_dse_streaming_add_asset(lua_State* L) {
    uint32_t zone = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    const char* type_str = luaL_checkstring(L, 3);
    dse_streaming_add_asset(zone, path, type_str);
    return 0;
}

int L_dse_streaming_set_zone_center(lua_State* L) {
    uint32_t zone = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    dse_streaming_set_zone_center(zone, x, y, z);
    return 0;
}

int L_dse_streaming_force_load(lua_State* L) {
    uint32_t zone = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_streaming_force_load(zone);
    return 0;
}

int L_dse_streaming_force_unload(lua_State* L) {
    uint32_t zone = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_streaming_force_unload(zone);
    return 0;
}

int L_dse_streaming_get_zone_state(lua_State* L) {
    uint32_t zone = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_streaming_get_zone_state(zone);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_streaming_get_zone_progress(lua_State* L) {
    uint32_t zone = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_streaming_get_zone_progress(zone);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_streaming_set_budget(lua_State* L) {
    int max_loads_per_frame = static_cast<int>(luaL_optinteger(L, 1, 8));
    int max_concurrent = static_cast<int>(luaL_optinteger(L, 2, 32));
    dse_streaming_set_budget(max_loads_per_frame, max_concurrent);
    return 0;
}

int L_dse_streaming_get_active_loads(lua_State* L) {
    int _ret = dse_streaming_get_active_loads();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_streaming_get_zone_count(lua_State* L) {
    int _ret = dse_streaming_get_zone_count();
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterStreamingBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "streaming");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "streaming");
    }
    helper::RegisterBindings(L, {
        {"streamingcreatezone", L_dse_streaming_create_zone},
        {"streamingdestroyzone", L_dse_streaming_destroy_zone},
        {"streamingaddasset", L_dse_streaming_add_asset},
        {"streamingsetzonecenter", L_dse_streaming_set_zone_center},
        {"streamingforceload", L_dse_streaming_force_load},
        {"streamingforceunload", L_dse_streaming_force_unload},
        {"streaminggetzonestate", L_dse_streaming_get_zone_state},
        {"streaminggetzoneprogress", L_dse_streaming_get_zone_progress},
        {"streamingsetbudget", L_dse_streaming_set_budget},
        {"streaminggetactiveloads", L_dse_streaming_get_active_loads},
        {"streaminggetzonecount", L_dse_streaming_get_zone_count},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
