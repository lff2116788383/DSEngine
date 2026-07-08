/**
 * @file lua_binding_free_localization.gen.cpp
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

int L_dse_compat_l10n_load(lua_State* L) {
    const char* locale = luaL_checkstring(L, 1);
    const char* json = luaL_checkstring(L, 2);
    int _ret = dse_compat_l10n_load(locale, json);
    lua_pushboolean(L, _ret);
    return 1;
}

int L_dse_l10n_set_locale(lua_State* L) {
    const char* locale = luaL_checkstring(L, 1);
    dse_l10n_set_locale(locale);
    return 0;
}

int L_dse_compat_l10n_get_locale(lua_State* L) {
    const char* _ret = dse_compat_l10n_get_locale();
    lua_pushstring(L, _ret ? _ret : "");
    return 1;
}

int L_dse_compat_l10n_get(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* fallback = luaL_optstring(L, 2, "");
    const char* _ret = dse_compat_l10n_get(key, fallback);
    lua_pushstring(L, _ret ? _ret : "");
    return 1;
}

int L_dse_l10n_has_key(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    int _ret = dse_l10n_has_key(key);
    lua_pushboolean(L, _ret);
    return 1;
}

int L_dse_l10n_get_locales(lua_State* L) {
    char _buf[1024];
    int _count = dse_l10n_get_locales(_buf, sizeof(_buf));
    lua_newtable(L);
    int _offset = 0;
    int _idx = 1;
    for (int _i = 0; _i < _count && _offset < static_cast<int>(sizeof(_buf)); ++_i) {
        const char* _name = _buf + _offset;
        lua_pushstring(L, _name);
        lua_rawseti(L, -2, _idx++);
        _offset += static_cast<int>(strlen(_name)) + 1;
    }
    return 1;
}

} // namespace

void RegisterFreeFn_localization(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "localization");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "localization");
    }
    helper::RegisterBindings(L, {
        {"load", L_dse_compat_l10n_load},
        {"set_locale", L_dse_l10n_set_locale},
        {"get_locale", L_dse_compat_l10n_get_locale},
        {"get", L_dse_compat_l10n_get},
        {"has_key", L_dse_l10n_has_key},
        {"get_locales", L_dse_l10n_get_locales},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
