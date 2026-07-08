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
    std::vector<float> points;
    if (lua_istable(L, 2)) {
        lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2));
        for (lua_Integer _i = 1; _i <= _n; ++_i) {
            lua_rawgeti(L, 2, _i);
            if (lua_isnumber(L, -1)) points.push_back(static_cast<float>(lua_tonumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    dse_line_renderer_set_points(e, points.data(), static_cast<int>(points.size()));
    return 0;
}

int L_dse_mesh_renderer_add_procedural(lua_State* L) {
    std::vector<float> vertices; if (lua_istable(L, 6)) { lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 6)); for (lua_Integer _i = 1; _i <= _n; ++_i) { lua_rawgeti(L, 6, _i); if (lua_isnumber(L, -1)) vertices.push_back(static_cast<float>(lua_tonumber(L, -1))); lua_pop(L, 1); } }
    std::vector<int> indices; if (lua_istable(L, 7)) { lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 7)); for (lua_Integer _i = 1; _i <= _n; ++_i) { lua_rawgeti(L, 7, _i); if (lua_isnumber(L, -1)) indices.push_back(static_cast<int>(lua_tointeger(L, -1))); lua_pop(L, 1); } }
    dse_mesh_renderer_add_procedural(static_cast<uint32_t>(luaL_checkinteger(L, 1)), static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)), static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5)), vertices.data(), static_cast<int>(vertices.size()), indices.data(), static_cast<int>(indices.size()));
    return 0;
}

int L_dse_mesh_renderer_set_normals(lua_State* L) {
    int out_attr_count = 0;
    int out_vertex_count = 0;
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    std::vector<float> normals;
    if (lua_istable(L, 2)) {
        lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2));
        for (lua_Integer _i = 1; _i <= _n; ++_i) {
            lua_rawgeti(L, 2, _i);
            if (lua_isnumber(L, -1)) normals.push_back(static_cast<float>(lua_tonumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    int _ret = dse_mesh_renderer_set_normals(e, normals.data(), static_cast<int>(normals.size()), &out_attr_count, &out_vertex_count);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_attr_count);
    lua_pushinteger(L, out_vertex_count);
    return 3;
}

int L_dse_mesh_renderer_set_tangents(lua_State* L) {
    int out_attr_count = 0;
    int out_vertex_count = 0;
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    std::vector<float> tangents;
    if (lua_istable(L, 2)) {
        lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2));
        for (lua_Integer _i = 1; _i <= _n; ++_i) {
            lua_rawgeti(L, 2, _i);
            if (lua_isnumber(L, -1)) tangents.push_back(static_cast<float>(lua_tonumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    int _ret = dse_mesh_renderer_set_tangents(e, tangents.data(), static_cast<int>(tangents.size()), &out_attr_count, &out_vertex_count);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_attr_count);
    lua_pushinteger(L, out_vertex_count);
    return 3;
}

int L_dse_mesh_renderer_set_uvs(lua_State* L) {
    int out_attr_count = 0;
    int out_vertex_count = 0;
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    std::vector<float> uvs;
    if (lua_istable(L, 2)) {
        lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2));
        for (lua_Integer _i = 1; _i <= _n; ++_i) {
            lua_rawgeti(L, 2, _i);
            if (lua_isnumber(L, -1)) uvs.push_back(static_cast<float>(lua_tonumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    int _ret = dse_mesh_renderer_set_uvs(e, uvs.data(), static_cast<int>(uvs.size()), &out_attr_count, &out_vertex_count);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_attr_count);
    lua_pushinteger(L, out_vertex_count);
    return 3;
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
