/**
 * @file lua_binding_free_ui.gen.cpp
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

int L_dse_ui_is_hovered(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ui_is_hovered(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ui_is_pressed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ui_is_pressed(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ui_get_dropdown_value(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    const char* out = luaL_checkstring(L, 2);
    int cap = static_cast<int>(luaL_checkinteger(L, 3));
    int _ret = dse_ui_get_dropdown_value(e, out, cap);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ui_get_text_input_text(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    const char* out = luaL_checkstring(L, 2);
    int cap = static_cast<int>(luaL_checkinteger(L, 3));
    int _ret = dse_ui_get_text_input_text(e, out, cap);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeFn_ui(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ui");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ui");
    }
    helper::RegisterBindings(L, {
        {"is_hovered", L_dse_ui_is_hovered},
        {"is_pressed", L_dse_ui_is_pressed},
        {"ui_get_dropdown_value", L_dse_ui_get_dropdown_value},
        {"ui_get_text_input_text", L_dse_ui_get_text_input_text},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
