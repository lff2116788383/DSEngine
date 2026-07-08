/**
 * @file lua_binding_free_dssl_gap.gen.cpp
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

void RegisterFreeDsslGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"dssl_get_color", L_dse_dssl_get_color},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
