/**
 * @file lua_binding_free_open_world_gap.gen.cpp
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

int L_dse_clipmap_get_config(lua_State* L) {
    float out_cell_size = 0;
    int out_levels = 0;
    dse_clipmap_get_config(&out_cell_size, &out_levels);
    lua_pushnumber(L, out_cell_size);
    lua_pushinteger(L, out_levels);
    return 2;
}

int L_dse_clipmap_sample_height(lua_State* L) {
    float out_y = 0;
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    int _ret = dse_clipmap_sample_height(x, z, &out_y);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_y);
    return 2;
}

int L_dse_wp_cell_to_world(lua_State* L) {
    float out_x = 0;
    float out_y = 0;
    float out_z = 0;
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    float cell_size = static_cast<float>(luaL_checknumber(L, 3));
    dse_wp_cell_to_world(cx, cz, cell_size, &out_x, &out_y, &out_z);
    lua_pushnumber(L, out_x);
    lua_pushnumber(L, out_y);
    lua_pushnumber(L, out_z);
    return 3;
}

int L_dse_wp_world_to_cell(lua_State* L) {
    int out_cx = 0;
    int out_cz = 0;
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float cell_size = static_cast<float>(luaL_checknumber(L, 4));
    dse_wp_world_to_cell(x, y, z, cell_size, &out_cx, &out_cz);
    lua_pushinteger(L, out_cx);
    lua_pushinteger(L, out_cz);
    return 2;
}

} // namespace

void RegisterFreeOpenWorldGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"clipmap_get_config", L_dse_clipmap_get_config},
        {"clipmap_sample_height", L_dse_clipmap_sample_height},
        {"wp_cell_to_world", L_dse_wp_cell_to_world},
        {"wp_world_to_cell", L_dse_wp_world_to_cell},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
