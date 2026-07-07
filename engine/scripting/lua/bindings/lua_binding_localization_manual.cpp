/**
 * @file lua_binding_localization.cpp
 * @brief Lua 绑定：本地化系统 (dse.localization) — C ABI 薄包装
 *
 * API:
 *   dse.localization.load(locale, json_string)   → bool
 *   dse.localization.set_locale(locale)           → bool
 *   dse.localization.get_locale()                 → string
 *   dse.localization.get(key [, default])         → string
 *   dse.localization.has_key(key)                 → bool
 *   dse.localization.get_locales()                → {string,...}
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"

namespace dse::runtime::lua_binding {
namespace {

// dse.localization.load(locale, json_string) → bool
int L_L10nLoad(lua_State* L) {
    const char* locale = luaL_checkstring(L, 1);
    const char* json = luaL_checkstring(L, 2);
    lua_pushboolean(L, dse_l10n_load_string(json, locale));
    return 1;
}

// dse.localization.set_locale(locale) → bool
int L_L10nSetLocale(lua_State* L) {
    dse_l10n_set_locale(luaL_checkstring(L, 1));
    lua_pushboolean(L, 1);
    return 1;
}

// dse.localization.get_locale() → string
int L_L10nGetLocale(lua_State* L) {
    char buf[256];
    int n = dse_l10n_get_locale(buf, sizeof(buf));
    lua_pushlstring(L, buf, n);
    return 1;
}

// dse.localization.get(key [, default]) → string
int L_L10nGet(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* def = luaL_optstring(L, 2, key);
    char buf[4096];
    int n = dse_l10n_get(key, buf, sizeof(buf));
    // C ABI returns the key itself if not found; use default if different
    std::string result(buf, n);
    if (result == key && def != key) {
        lua_pushstring(L, def);
    } else {
        lua_pushlstring(L, buf, n);
    }
    return 1;
}

// dse.localization.has_key(key) → bool
int L_L10nHasKey(lua_State* L) {
    lua_pushboolean(L, dse_l10n_has_key(luaL_checkstring(L, 1)));
    return 1;
}

// dse.localization.get_locales() → {string,...}
int L_L10nGetLocales(lua_State* L) {
    // C ABI packs locales as null-separated string; we need to split them
    char buf[4096];
    int count = dse_l10n_get_locales(buf, sizeof(buf));
    lua_createtable(L, count, 0);
    if (count <= 0) return 1;

    // Parse null-separated strings
    int idx = 1;
    const char* start = buf;
    const char* end = buf + sizeof(buf);
    while (start < end && idx <= count) {
        const char* next = start;
        while (next < end && *next != '\0') ++next;
        lua_pushlstring(L, start, static_cast<size_t>(next - start));
        lua_rawseti(L, -2, idx++);
        if (next >= end) break;
        start = next + 1;
    }
    return 1;
}

} // anonymous namespace

void RegisterLocalizationBindings(lua_State* L) {
    lua_newtable(L);
    const luaL_Reg funcs[] = {
        {"load",        L_L10nLoad},
        {"set_locale",  L_L10nSetLocale},
        {"get_locale",  L_L10nGetLocale},
        {"get",         L_L10nGet},
        {"has_key",     L_L10nHasKey},
        {"get_locales", L_L10nGetLocales},
        {nullptr, nullptr}
    };
    luaL_setfuncs(L, funcs, 0);
}

} // namespace dse::runtime::lua_binding
