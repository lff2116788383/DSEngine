/**
 * @file lua_binding_free_world_ocean.gen.cpp
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

int L_dse_ocean_init(lua_State* L) {
    int fft_res = 256;
    float tile_size = 512.0f;
    float wind_speed = 8.0f;
    float choppiness = 1.0f;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "fft_resolution");
        if (!lua_isnil(L, -1)) fft_res = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "tile_size");
        if (!lua_isnil(L, -1)) tile_size = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "wind_speed");
        if (!lua_isnil(L, -1)) wind_speed = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "choppiness");
        if (!lua_isnil(L, -1)) choppiness = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    int _ret = dse_ocean_init(fft_res, tile_size, wind_speed, choppiness);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ocean_shutdown(lua_State* L) {
    dse_ocean_shutdown();
    return 0;
}

int L_dse_ocean_update(lua_State* L) {
    float dt = static_cast<float>(luaL_checknumber(L, 1));
    float cam_x = static_cast<float>(luaL_checknumber(L, 2));
    float cam_y = static_cast<float>(luaL_checknumber(L, 3));
    float cam_z = static_cast<float>(luaL_checknumber(L, 4));
    dse_ocean_update(dt, cam_x, cam_y, cam_z);
    return 0;
}

int L_dse_ocean_get_height(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    float _ret = dse_ocean_get_height(x, z);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_ocean_get_normal(lua_State* L) {
    float xyz[3] = {0, 0, 0};
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    dse_ocean_get_normal(x, z, xyz);
    lua_pushnumber(L, xyz[0]);
    lua_pushnumber(L, xyz[1]);
    lua_pushnumber(L, xyz[2]);
    return 3;
}

int L_dse_ocean_get_foam(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    float _ret = dse_ocean_get_foam(x, z);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_ocean_set_wind(lua_State* L) {
    float dx = static_cast<float>(luaL_checknumber(L, 1));
    float dz = static_cast<float>(luaL_checknumber(L, 2));
    float speed = static_cast<float>(luaL_checknumber(L, 3));
    dse_ocean_set_wind(dx, dz, speed);
    return 0;
}

int L_dse_ocean_set_choppiness(lua_State* L) {
    float c = static_cast<float>(luaL_checknumber(L, 1));
    dse_ocean_set_choppiness(c);
    return 0;
}

int L_dse_ocean_get_stats(lua_State* L) {
    int total = 0;
    int visible = 0;
    int fft_res = 0;
    float max_height = 0;
    dse_ocean_get_stats(&total, &visible, &fft_res, &max_height);
    lua_newtable(L);
    lua_pushinteger(L, total);
    lua_setfield(L, -2, "total_tiles");
    lua_pushinteger(L, visible);
    lua_setfield(L, -2, "visible_tiles");
    lua_pushinteger(L, fft_res);
    lua_setfield(L, -2, "fft_resolution");
    lua_pushnumber(L, max_height);
    lua_setfield(L, -2, "max_height");
    return 1;
}

int L_dse_ocean_get_lod_count(lua_State* L) {
    int _ret = dse_ocean_get_lod_count();
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeWorldOceanBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ocean");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ocean");
    }
    helper::RegisterBindings(L, {
        {"init", L_dse_ocean_init},
        {"shutdown", L_dse_ocean_shutdown},
        {"update", L_dse_ocean_update},
        {"get_height", L_dse_ocean_get_height},
        {"get_normal", L_dse_ocean_get_normal},
        {"get_foam", L_dse_ocean_get_foam},
        {"set_wind", L_dse_ocean_set_wind},
        {"set_choppiness", L_dse_ocean_set_choppiness},
        {"get_stats", L_dse_ocean_get_stats},
        {"get_lod_count", L_dse_ocean_get_lod_count},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
