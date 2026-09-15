/**
 * @file lua_binding_compat.cpp
 * @brief Lua 绑定兼容层：修正「生成绑定」中过严/命名不一致的参数约定。
 *
 * 背景：UI/音频等绑定由 codegen 生成，形参多为整数，导致
 *   - ui.set_visible(e, true/false) 报 "number expected, got boolean"
 *   - audio.play_sfx/play_bgm 的 loop 传 false/true 报错
 * 本文件在生成绑定注册之后覆盖同名函数，做 bool/number 双兼容（不改生成物、不改 ABI）。
 */
#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api_core.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

bool ToBoolish(lua_State* L, int idx, bool def) {
    if (lua_isboolean(L, idx)) return lua_toboolean(L, idx) != 0;
    if (lua_isnumber(L, idx)) return lua_tonumber(L, idx) != 0.0;
    if (lua_isnoneornil(L, idx)) return def;
    return lua_toboolean(L, idx) != 0;
}

int L_SetVisibleCompat(lua_State* L) {
    const uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ui_set_visible(e, ToBoolish(L, 2, true) ? 1 : 0);
    return 0;
}

int L_PlaySfxCompat(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const float vol = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    dse_audio_play_sfx(path, vol, ToBoolish(L, 3, false) ? 1 : 0);
    return 0;
}

int L_PlayBgmCompat(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const float vol = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    const bool loop = ToBoolish(L, 3, true);
    lua_pushboolean(L, dse_audio_play_bgm(path, vol, loop ? 1 : 0) != 0);
    return 1;
}

void Override(lua_State* L, const char* table, const char* name, lua_CFunction fn) {
    lua_getglobal(L, "dse");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
    lua_getfield(L, -1, table);
    if (lua_istable(L, -1)) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    }
    lua_pop(L, 2);
}

}  // namespace

void RegisterCompatBindings(lua_State* L) {
    Override(L, "ui", "set_visible", L_SetVisibleCompat);
    Override(L, "audio", "play_sfx", L_PlaySfxCompat);
    Override(L, "audio", "play_bgm", L_PlayBgmCompat);
}

}  // namespace dse::runtime::lua_binding