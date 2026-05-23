/**
 * @file lua_binding_localization.cpp
 * @brief Lua 绑定：本地化系统 (dse.localization)
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
#include "engine/assets/localization_manager.h"
#include "engine/core/service_locator.h"

namespace dse::runtime::lua_binding {
namespace {

dse::assets::LocalizationManager* GetL10n() {
    return dse::core::ServiceLocator::Instance().Get<dse::assets::LocalizationManager>();
}

int L_L10nLoad(lua_State* L) {
    const char* locale = luaL_checkstring(L, 1);
    const char* json = luaL_checkstring(L, 2);
    auto* mgr = GetL10n();
    if (!mgr) { lua_pushboolean(L, 0); return 1; }
    bool ok = mgr->LoadLocaleFromString(json, locale);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int L_L10nSetLocale(lua_State* L) {
    const char* locale = luaL_checkstring(L, 1);
    auto* mgr = GetL10n();
    if (!mgr) { lua_pushboolean(L, 0); return 1; }
    mgr->SetCurrentLocale(locale);
    lua_pushboolean(L, 1);
    return 1;
}

int L_L10nGetLocale(lua_State* L) {
    auto* mgr = GetL10n();
    if (!mgr) { lua_pushstring(L, ""); return 1; }
    lua_pushstring(L, mgr->GetCurrentLocale().c_str());
    return 1;
}

int L_L10nGet(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* def = luaL_optstring(L, 2, key);
    auto* mgr = GetL10n();
    if (!mgr) { lua_pushstring(L, def); return 1; }
    std::string result = mgr->Get(key);
    if (result == key && def != key) {
        result = def;
    }
    lua_pushstring(L, result.c_str());
    return 1;
}

int L_L10nHasKey(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    auto* mgr = GetL10n();
    if (!mgr) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, mgr->HasKey(key) ? 1 : 0);
    return 1;
}

int L_L10nGetLocales(lua_State* L) {
    auto* mgr = GetL10n();
    if (!mgr) { lua_newtable(L); return 1; }
    auto locales = mgr->GetAvailableLocales();
    lua_createtable(L, static_cast<int>(locales.size()), 0);
    for (int i = 0; i < static_cast<int>(locales.size()); ++i) {
        lua_pushstring(L, locales[i].c_str());
        lua_rawseti(L, -2, i + 1);
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
