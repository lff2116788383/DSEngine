/**
 * @file lua_binding_free_localization.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/function_defs.json
 *
 * localization 组自由函数的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_l10n_load(lua_State* L) {
    const char* locale = luaL_checkstring(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int _ret = dse_l10n_load(locale, path);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeFn_localization(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "localization");
    helper::RegisterBindings(L, {
        {"load", L_dse_l10n_load},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
