/**
 * @file lua_binding_free_api_core.gen.cpp
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

int L_dse_api_version(lua_State* L) {
    uint32_t _ret = dse_api_version();
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_uuid_get(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    const char* out = luaL_checkstring(L, 2);
    int cap = static_cast<int>(luaL_checkinteger(L, 3));
    int _ret = dse_uuid_get(e, out, cap);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_uuid_set(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    const char* uuid_str = luaL_checkstring(L, 2);
    const char* out = luaL_checkstring(L, 3);
    int cap = static_cast<int>(luaL_checkinteger(L, 4));
    int _ret = dse_uuid_set(e, uuid_str, out, cap);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeFn_api_core(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"api_version", L_dse_api_version},
        {"uuid_get", L_dse_uuid_get},
        {"uuid_set", L_dse_uuid_set},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
