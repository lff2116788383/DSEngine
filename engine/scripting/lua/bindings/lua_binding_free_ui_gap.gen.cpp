/**
 * @file lua_binding_free_ui_gap.gen.cpp
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

int L_dse_ui_get_scroll_offset(lua_State* L) {
    float out_x = 0;
    float out_y = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ui_get_scroll_offset(e, &out_x, &out_y);
    lua_pushnumber(L, out_x);
    lua_pushnumber(L, out_y);
    return 2;
}

int L_dse_ui_get_virtual_scroll_range(lua_State* L) {
    int out_start = 0;
    int out_end = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ui_get_virtual_scroll_range(e, &out_start, &out_end);
    lua_pushinteger(L, out_start);
    lua_pushinteger(L, out_end);
    return 2;
}

} // namespace

void RegisterFreeUiGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"ui_get_scroll_offset", L_dse_ui_get_scroll_offset},
        {"ui_get_virtual_scroll_range", L_dse_ui_get_virtual_scroll_range},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
