/**
 * @file lua_binding_free_meshlet.gen.cpp
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

int L_dse_meshlet_serialize(lua_State* L) {
    uint32_t handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    int _ret = dse_meshlet_serialize(handle, path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_meshlet_deserialize(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint32_t _ret = dse_meshlet_deserialize(path);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_meshlet_destroy(lua_State* L) {
    uint32_t handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_meshlet_destroy(handle);
    return 0;
}

int L_dse_meshlet_cull_create(lua_State* L) {
    uint32_t _ret = dse_meshlet_cull_create();
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_meshlet_cull_destroy(lua_State* L) {
    uint32_t cull_handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_meshlet_cull_destroy(cull_handle);
    return 0;
}

int L_dse_meshlet_cull_register(lua_State* L) {
    uint32_t cull_handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t meshlet_handle = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t _ret = dse_meshlet_cull_register(cull_handle, meshlet_handle);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_meshlet_cull_unregister(lua_State* L) {
    uint32_t cull_handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t reg_handle = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_meshlet_cull_unregister(cull_handle, reg_handle);
    return 0;
}

int L_dse_meshlet_cull_begin_frame(lua_State* L) {
    uint32_t cull_handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_meshlet_cull_begin_frame(cull_handle);
    return 0;
}

int L_dse_meshlet_build(lua_State* L) {
    float positions = static_cast<float>(luaL_checknumber(L, 1));
    int pos_count = static_cast<int>(luaL_checkinteger(L, 2));
    int indices = static_cast<int>(luaL_checkinteger(L, 3));
    int idx_count = static_cast<int>(luaL_checkinteger(L, 4));
    int max_vertices = static_cast<int>(luaL_checkinteger(L, 5));
    int max_triangles = static_cast<int>(luaL_checkinteger(L, 6));
    uint32_t _ret = dse_meshlet_build(positions, pos_count, indices, idx_count, max_vertices, max_triangles);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_meshlet_cull_add_instance(lua_State* L) {
    int cull_handle = static_cast<int>(luaL_checkinteger(L, 1));
    int reg_handle = static_cast<int>(luaL_checkinteger(L, 2));
    float matrix16 = static_cast<float>(luaL_checknumber(L, 3));
    dse_meshlet_cull_add_instance(cull_handle, reg_handle, matrix16);
    return 0;
}

int L_dse_meshlet_cull_execute_cpu(lua_State* L) {
    int cull_handle = static_cast<int>(luaL_checkinteger(L, 1));
    float vp_matrix16 = static_cast<float>(luaL_checknumber(L, 2));
    float cam_x = static_cast<float>(luaL_checknumber(L, 3));
    float cam_y = static_cast<float>(luaL_checknumber(L, 4));
    float cam_z = static_cast<float>(luaL_checknumber(L, 5));
    int flags = static_cast<int>(luaL_checkinteger(L, 6));
    uint32_t _ret = dse_meshlet_cull_execute_cpu(cull_handle, vp_matrix16, cam_x, cam_y, cam_z, flags);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_meshlet_cull_prepare(lua_State* L) {
    int cull_handle = static_cast<int>(luaL_checkinteger(L, 1));
    float vp_matrix16 = static_cast<float>(luaL_checknumber(L, 2));
    float cam_x = static_cast<float>(luaL_checknumber(L, 3));
    float cam_y = static_cast<float>(luaL_checknumber(L, 4));
    float cam_z = static_cast<float>(luaL_checknumber(L, 5));
    uint32_t _ret = dse_meshlet_cull_prepare(cull_handle, vp_matrix16, cam_x, cam_y, cam_z);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

} // namespace

void RegisterMeshletBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "meshlet");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "meshlet");
    }
    helper::RegisterBindings(L, {
        {"serialize", L_dse_meshlet_serialize},
        {"deserialize", L_dse_meshlet_deserialize},
        {"destroy", L_dse_meshlet_destroy},
        {"cull_create", L_dse_meshlet_cull_create},
        {"cull_destroy", L_dse_meshlet_cull_destroy},
        {"cull_register", L_dse_meshlet_cull_register},
        {"cull_unregister", L_dse_meshlet_cull_unregister},
        {"cull_begin_frame", L_dse_meshlet_cull_begin_frame},
        {"meshlet_build", L_dse_meshlet_build},
        {"meshlet_cull_add_instance", L_dse_meshlet_cull_add_instance},
        {"meshlet_cull_execute_cpu", L_dse_meshlet_cull_execute_cpu},
        {"meshlet_cull_prepare", L_dse_meshlet_cull_prepare},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
