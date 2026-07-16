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
    std::vector<float> _positions; if (lua_istable(L, 1)) { lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 1)); for (lua_Integer _i = 1; _i <= _n; ++_i) { lua_rawgeti(L, 1, _i); if (lua_isnumber(L, -1)) _positions.push_back(static_cast<float>(lua_tonumber(L, -1))); lua_pop(L, 1); } }
    std::vector<uint32_t> _indices; if (lua_istable(L, 2)) { lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2)); for (lua_Integer _i = 1; _i <= _n; ++_i) { lua_rawgeti(L, 2, _i); if (lua_isnumber(L, -1)) _indices.push_back(static_cast<uint32_t>(lua_tointeger(L, -1))); lua_pop(L, 1); } }
    dse_meshlet_build(_positions.data(), static_cast<int>(_positions.size()), _indices.data(), static_cast<int>(_indices.size()), static_cast<uint32_t>(luaL_checkinteger(L, 3)), static_cast<uint32_t>(luaL_checkinteger(L, 4)));
    return 0;
}

int L_dse_meshlet_cull_add_instance(lua_State* L) {
    std::vector<float> _matrix; if (lua_istable(L, 3)) { lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 3)); for (lua_Integer _i = 1; _i <= _n; ++_i) { lua_rawgeti(L, 3, _i); if (lua_isnumber(L, -1)) _matrix.push_back(static_cast<float>(lua_tonumber(L, -1))); lua_pop(L, 1); } }
    dse_meshlet_cull_add_instance(static_cast<uint32_t>(luaL_checkinteger(L, 1)), static_cast<uint32_t>(luaL_checkinteger(L, 2)), _matrix.data());
    return 0;
}

int L_dse_meshlet_cull_execute_cpu(lua_State* L) {
    std::vector<float> _vp; if (lua_istable(L, 2)) { lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2)); for (lua_Integer _i = 1; _i <= _n; ++_i) { lua_rawgeti(L, 2, _i); if (lua_isnumber(L, -1)) _vp.push_back(static_cast<float>(lua_tonumber(L, -1))); lua_pop(L, 1); } }
    dse_meshlet_cull_execute_cpu(static_cast<uint32_t>(luaL_checkinteger(L, 1)), _vp.data(), static_cast<float>(luaL_checknumber(L, 3)), static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5)), static_cast<uint32_t>(luaL_checkinteger(L, 6)));
    return 0;
}

int L_dse_meshlet_cull_prepare(lua_State* L) {
    std::vector<float> _vp; if (lua_istable(L, 2)) { lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2)); for (lua_Integer _i = 1; _i <= _n; ++_i) { lua_rawgeti(L, 2, _i); if (lua_isnumber(L, -1)) _vp.push_back(static_cast<float>(lua_tonumber(L, -1))); lua_pop(L, 1); } }
    dse_meshlet_cull_prepare(static_cast<uint32_t>(luaL_checkinteger(L, 1)), _vp.data(), static_cast<float>(luaL_checknumber(L, 3)), static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5)));
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
        {"build", L_dse_meshlet_build},
        {"cull_add_instance", L_dse_meshlet_cull_add_instance},
        {"cull_execute_cpu", L_dse_meshlet_cull_execute_cpu},
        {"cull_prepare", L_dse_meshlet_cull_prepare},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
