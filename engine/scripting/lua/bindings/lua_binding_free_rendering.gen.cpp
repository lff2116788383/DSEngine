/**
 * @file lua_binding_free_rendering.gen.cpp
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

int L_dse_line_renderer_set_points(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    float points = static_cast<float>(luaL_checknumber(L, 2));
    int count = static_cast<int>(luaL_checkinteger(L, 3));
    dse_line_renderer_set_points(e, points, count);
    return 0;
}

int L_dse_mesh_renderer_add_procedural(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_checknumber(L, 5));
    float vertices = static_cast<float>(luaL_checknumber(L, 6));
    int vertex_float_count = static_cast<int>(luaL_checkinteger(L, 7));
    int indices = static_cast<int>(luaL_checkinteger(L, 8));
    int index_count = static_cast<int>(luaL_checkinteger(L, 9));
    dse_mesh_renderer_add_procedural(e, r, g, b, a, vertices, vertex_float_count, indices, index_count);
    return 0;
}

int L_dse_mesh_renderer_set_normals(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    float normals = static_cast<float>(luaL_checknumber(L, 2));
    int count = static_cast<int>(luaL_checkinteger(L, 3));
    int out_attr_count = static_cast<int>(luaL_checkinteger(L, 4));
    int out_vertex_count = static_cast<int>(luaL_checkinteger(L, 5));
    int _ret = dse_mesh_renderer_set_normals(e, normals, count, out_attr_count, out_vertex_count);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_mesh_renderer_set_tangents(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    float tangents = static_cast<float>(luaL_checknumber(L, 2));
    int count = static_cast<int>(luaL_checkinteger(L, 3));
    int out_attr_count = static_cast<int>(luaL_checkinteger(L, 4));
    int out_vertex_count = static_cast<int>(luaL_checkinteger(L, 5));
    int _ret = dse_mesh_renderer_set_tangents(e, tangents, count, out_attr_count, out_vertex_count);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_mesh_renderer_set_uvs(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    float uvs = static_cast<float>(luaL_checknumber(L, 2));
    int count = static_cast<int>(luaL_checkinteger(L, 3));
    int out_attr_count = static_cast<int>(luaL_checkinteger(L, 4));
    int out_vertex_count = static_cast<int>(luaL_checkinteger(L, 5));
    int _ret = dse_mesh_renderer_set_uvs(e, uvs, count, out_attr_count, out_vertex_count);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeFn_rendering(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "rendering");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "rendering");
    }
    helper::RegisterBindings(L, {
        {"line_renderer_set_points", L_dse_line_renderer_set_points},
        {"mesh_renderer_add_procedural", L_dse_mesh_renderer_add_procedural},
        {"mesh_renderer_set_normals", L_dse_mesh_renderer_set_normals},
        {"mesh_renderer_set_tangents", L_dse_mesh_renderer_set_tangents},
        {"mesh_renderer_set_uvs", L_dse_mesh_renderer_set_uvs},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
