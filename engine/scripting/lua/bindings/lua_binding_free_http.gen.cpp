/**
 * @file lua_binding_free_http.gen.cpp
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

int L_dse_http_update(lua_State* L) {
    dse_http_update();
    return 0;
}

int L_dse_http_available(lua_State* L) {
    int _ret = dse_http_available();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_http_poll(lua_State* L) {
    uint32_t _buf[64];
    int _count = dse_http_poll(_buf, 64);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_pushinteger(L, static_cast<lua_Integer>(_buf[_i]));
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

} // namespace

void RegisterHttpBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "http");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "http");
    }
    helper::RegisterBindings(L, {
        {"update", L_dse_http_update},
        {"available", L_dse_http_available},
        {"poll", L_dse_http_poll},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
