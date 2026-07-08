/**
 * @file lua_binding_free_meshlet_gap.gen.cpp
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

int L_dse_meshlet_cull_stats(lua_State* L) {
    int out_total = 0;
    int out_visible = 0;
    int out_meshes = 0;
    int out_instances = 0;
    uint32_t cull_handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_meshlet_cull_stats(cull_handle, &out_total, &out_visible, &out_meshes, &out_instances);
    lua_pushinteger(L, out_total);
    lua_pushinteger(L, out_visible);
    lua_pushinteger(L, out_meshes);
    lua_pushinteger(L, out_instances);
    return 4;
}

int L_dse_meshlet_get_info(lua_State* L) {
    int out_meshlets = 0;
    int out_vertices = 0;
    int out_indices = 0;
    int out_meshlet_vertices = 0;
    uint32_t handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_meshlet_get_info(handle, &out_meshlets, &out_vertices, &out_indices, &out_meshlet_vertices);
    lua_pushinteger(L, out_meshlets);
    lua_pushinteger(L, out_vertices);
    lua_pushinteger(L, out_indices);
    lua_pushinteger(L, out_meshlet_vertices);
    return 4;
}

} // namespace

void RegisterFreeMeshletGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"meshlet_cull_stats", L_dse_meshlet_cull_stats},
        {"meshlet_get_info", L_dse_meshlet_get_info},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
