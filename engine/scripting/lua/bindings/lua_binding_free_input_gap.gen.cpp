/**
 * @file lua_binding_free_input_gap.gen.cpp
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

int L_dse_input_get_touch(lua_State* L) {
    float out_x = 0;
    float out_y = 0;
    int out_phase = 0;
    int index = static_cast<int>(luaL_checkinteger(L, 1));
    int _ret = dse_input_get_touch(index, &out_x, &out_y, &out_phase);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_x);
    lua_pushnumber(L, out_y);
    lua_pushinteger(L, out_phase);
    return 4;
}

} // namespace

void RegisterFreeInputGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"input_get_touch", L_dse_input_get_touch},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
