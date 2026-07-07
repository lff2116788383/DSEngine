/**
 * @file lua_binding_free_font.gen.cpp
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

int L_dse_font_load(lua_State* L) {
    const char* font_id = luaL_checkstring(L, 1);
    const char* ttf_path = luaL_checkstring(L, 2);
    int _ret = dse_font_load(font_id, ttf_path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_font_load_cjk(lua_State* L) {
    const char* font_id = luaL_checkstring(L, 1);
    const char* ttf_path = luaL_checkstring(L, 2);
    int _ret = dse_font_load_cjk(font_id, ttf_path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_font_unload(lua_State* L) {
    const char* font_id = luaL_checkstring(L, 1);
    dse_font_unload(font_id);
    return 0;
}

int L_dse_font_set_default(lua_State* L) {
    const char* font_id = luaL_checkstring(L, 1);
    int _ret = dse_font_set_default(font_id);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_font_measure(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    const char* font_id = luaL_checkstring(L, 2);
    float font_size = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float _ret = dse_font_measure(text, font_id, font_size);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_font_line_height(lua_State* L) {
    const char* font_id = luaL_checkstring(L, 1);
    float font_size = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float _ret = dse_font_line_height(font_id, font_size);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_font_get_texture(lua_State* L) {
    const char* font_id = luaL_checkstring(L, 1);
    uint32_t _ret = dse_font_get_texture(font_id);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

} // namespace

void RegisterFontBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "font");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "font");
    }
    helper::RegisterBindings(L, {
        {"fontload", L_dse_font_load},
        {"fontloadcjk", L_dse_font_load_cjk},
        {"fontunload", L_dse_font_unload},
        {"fontsetdefault", L_dse_font_set_default},
        {"fontmeasure", L_dse_font_measure},
        {"fontlineheight", L_dse_font_line_height},
        {"fontgettexture", L_dse_font_get_texture},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
