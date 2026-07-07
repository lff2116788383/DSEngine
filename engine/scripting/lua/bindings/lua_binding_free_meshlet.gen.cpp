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
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
