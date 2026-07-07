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
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
