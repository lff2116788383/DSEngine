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

} // namespace

void RegisterHttpBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "http");
    helper::RegisterBindings(L, {
        {"update", L_dse_http_update},
        {"available", L_dse_http_available},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
