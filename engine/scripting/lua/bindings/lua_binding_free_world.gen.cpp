/**
 * @file lua_binding_free_world.gen.cpp
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

int L_dse_weather_add(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    int type = static_cast<int>(luaL_checkinteger(L, 2));
    float intensity = static_cast<float>(luaL_checknumber(L, 3));
    dse_weather_add(e, type, intensity);
    return 0;
}

} // namespace

void RegisterFreeFn_world(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "world");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "world");
    }
    helper::RegisterBindings(L, {
        {"weather_add", L_dse_weather_add},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
