/**
 * @file lua_binding_free_http_gap.gen.cpp
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

int L_dse_http_send(lua_State* L) {
    const char* method = luaL_checkstring(L, 1);
    const char* url = luaL_checkstring(L, 2);
    const char* body = luaL_checkstring(L, 3);
    const char* headers_json = luaL_checkstring(L, 4);
    int timeout_sec = static_cast<int>(luaL_checkinteger(L, 5));
    int verify_peer = static_cast<int>(luaL_checkinteger(L, 6));
    const char* ca_file = luaL_checkstring(L, 7);
    uint32_t _ret = dse_http_send(method, url, body, headers_json, timeout_sec, verify_peer, ca_file);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

} // namespace

void RegisterFreeHttpGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"http_send", L_dse_http_send},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
