/**
 * @file lua_binding_font.cpp
 * @brief Lua 字体服务绑定 — dse.font API (C ABI 薄包装)
 *
 * Lua 用法示例:
 *   dse.font.load("main", "data/fonts/NotoSansSC-Regular.ttf")
 *   dse.font.load_cjk("main", "data/fonts/NotoSansSC-Regular.ttf")
 *   dse.font.set_default("main")
 *   local w = dse.font.measure("Hello World", "main", 24)
 *   local h = dse.font.line_height("main", 24)
 *   local tex = dse.font.get_texture("main")
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

// dse.font.load(font_id, ttf_path) → bool
int L_FontLoad(lua_State* L) {
    const char* font_id = luaL_checkstring(L, 1);
    const char* ttf_path = luaL_checkstring(L, 2);
    lua_pushboolean(L, dse_font_load(font_id, ttf_path));
    return 1;
}

// dse.font.load_cjk(font_id, ttf_path) → bool
int L_FontLoadCJK(lua_State* L) {
    const char* font_id = luaL_checkstring(L, 1);
    const char* ttf_path = luaL_checkstring(L, 2);
    lua_pushboolean(L, dse_font_load_cjk(font_id, ttf_path));
    return 1;
}

// dse.font.unload(font_id)
int L_FontUnload(lua_State* L) {
    dse_font_unload(luaL_checkstring(L, 1));
    return 0;
}

// dse.font.set_default(font_id) → bool
int L_FontSetDefault(lua_State* L) {
    lua_pushboolean(L, dse_font_set_default(luaL_checkstring(L, 1)));
    return 1;
}

// dse.font.measure(text [, font_id [, font_size]]) → width
int L_FontMeasure(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    const char* font_id = luaL_optstring(L, 2, "");
    float font_size = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    lua_pushnumber(L, static_cast<lua_Number>(dse_font_measure(text, font_id, font_size)));
    return 1;
}

// dse.font.line_height([font_id [, font_size]]) → height
int L_FontLineHeight(lua_State* L) {
    const char* font_id = luaL_optstring(L, 1, "");
    float font_size = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    lua_pushnumber(L, static_cast<lua_Number>(dse_font_line_height(font_id, font_size)));
    return 1;
}

// dse.font.get_texture([font_id]) → texture_handle (int)
int L_FontGetTexture(lua_State* L) {
    const char* font_id = luaL_optstring(L, 1, "");
    lua_pushinteger(L, static_cast<lua_Integer>(dse_font_get_texture(font_id)));
    return 1;
}

} // anonymous namespace

void RegisterFontBindings(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, L_FontLoad);
    lua_setfield(L, -2, "load");

    lua_pushcfunction(L, L_FontLoadCJK);
    lua_setfield(L, -2, "load_cjk");

    lua_pushcfunction(L, L_FontUnload);
    lua_setfield(L, -2, "unload");

    lua_pushcfunction(L, L_FontSetDefault);
    lua_setfield(L, -2, "set_default");

    lua_pushcfunction(L, L_FontMeasure);
    lua_setfield(L, -2, "measure");

    lua_pushcfunction(L, L_FontLineHeight);
    lua_setfield(L, -2, "line_height");

    lua_pushcfunction(L, L_FontGetTexture);
    lua_setfield(L, -2, "get_texture");
}

} // namespace dse::runtime::lua_binding
