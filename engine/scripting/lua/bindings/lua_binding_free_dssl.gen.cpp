/**
 * @file lua_binding_free_dssl.gen.cpp
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

int L_dse_dssl_load_material(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint32_t _ret = dse_dssl_load_material(path);
    if (_ret == 0) { lua_pushnil(L); } else { lua_pushinteger(L, static_cast<lua_Integer>(_ret)); }
    return 1;
}

int L_dse_dssl_create_instance(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint32_t _ret = dse_dssl_create_instance(path);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_dssl_set_float(lua_State* L) {
    uint32_t instance = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float value = static_cast<float>(luaL_checknumber(L, 3));
    dse_dssl_set_float(instance, name, value);
    return 0;
}

int L_dse_dssl_set_color(lua_State* L) {
    uint32_t instance = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float r = static_cast<float>(luaL_checknumber(L, 3));
    float g = static_cast<float>(luaL_checknumber(L, 4));
    float b = static_cast<float>(luaL_checknumber(L, 5));
    float a = static_cast<float>(luaL_checknumber(L, 6));
    dse_dssl_set_color(instance, name, r, g, b, a);
    return 0;
}

int L_dse_dssl_set_vec3(lua_State* L) {
    uint32_t instance = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));
    dse_dssl_set_vec3(instance, name, x, y, z);
    return 0;
}

int L_dse_dssl_set_texture(lua_State* L) {
    uint32_t instance = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    const char* path = luaL_checkstring(L, 3);
    dse_dssl_set_texture(instance, name, path);
    return 0;
}

int L_dse_dssl_set_texture_handle(lua_State* L) {
    uint32_t instance = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    uint32_t handle = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    dse_dssl_set_texture_handle(instance, name, handle);
    return 0;
}

int L_dse_dssl_apply_material(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t instance = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_dssl_apply_material(e, instance);
    return 0;
}

int L_dse_dssl_get_float(lua_State* L) {
    uint32_t instance = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float _ret = dse_dssl_get_float(instance, name);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_dssl_get_color(lua_State* L) {
    float out_rgba[4] = {0, 0, 0, 0};
    uint32_t instance = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    dse_dssl_get_color(instance, name, out_rgba);
    lua_pushnumber(L, out_rgba[0]);
    lua_pushnumber(L, out_rgba[1]);
    lua_pushnumber(L, out_rgba[2]);
    lua_pushnumber(L, out_rgba[3]);
    return 4;
}

} // namespace

void RegisterDSSLBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "dssl");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "dssl");
    }
    helper::RegisterBindings(L, {
        {"load_material", L_dse_dssl_load_material},
        {"create_instance", L_dse_dssl_create_instance},
        {"set_float", L_dse_dssl_set_float},
        {"set_color", L_dse_dssl_set_color},
        {"set_vec3", L_dse_dssl_set_vec3},
        {"set_texture", L_dse_dssl_set_texture},
        {"set_texture_handle", L_dse_dssl_set_texture_handle},
        {"apply_material", L_dse_dssl_apply_material},
        {"get_float", L_dse_dssl_get_float},
        {"get_color", L_dse_dssl_get_color},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
