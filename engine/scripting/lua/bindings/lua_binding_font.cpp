/**
 * @file lua_binding_font.cpp
 * @brief Lua 字体服务绑定 — dse.font API
 *
 * Lua 用法示例:
 *   dse.font.load("main", "data/fonts/NotoSansSC-Regular.ttf")
 *   dse.font.load_cjk("main", "data/fonts/NotoSansSC-Regular.ttf")  -- 自动追加常用汉字
 *   dse.font.set_default("main")
 *   local w = dse.font.measure("Hello World", "main", 24)
 *   local h = dse.font.line_height("main", 24)
 *   local tex = dse.font.get_texture("main")
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/core/service_locator.h"
#include "engine/render/font/font_service.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

static render::FontService* GetFontService() {
    return core::ServiceLocator::Instance().Get<render::FontService>();
}

// dse.font.load(font_id, ttf_path)
int L_FontLoad(lua_State* L) {
    auto* svc = GetFontService();
    if (!svc) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const char* font_id = luaL_checkstring(L, 1);
    const char* ttf_path = luaL_checkstring(L, 2);
    bool ok = svc->LoadFont(font_id, ttf_path);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// dse.font.load_cjk(font_id, ttf_path) — 自动追加常用汉字码点，临时增大图集
int L_FontLoadCJK(lua_State* L) {
    auto* svc = GetFontService();
    if (!svc) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const char* font_id = luaL_checkstring(L, 1);
    const char* ttf_path = luaL_checkstring(L, 2);

    // 常用汉字 (CJK Unified Ideographs 中取前 800 高频字)
    std::vector<int> cjk_codepoints;
    cjk_codepoints.reserve(900);
    for (int cp = 0x4E00; cp <= 0x9FA5 && static_cast<int>(cjk_codepoints.size()) < 800; ++cp) {
        cjk_codepoints.push_back(cp);
    }
    // 常用标点
    for (int cp = 0x3000; cp <= 0x303F; ++cp) cjk_codepoints.push_back(cp);
    for (int cp = 0xFF01; cp <= 0xFF5E; ++cp) cjk_codepoints.push_back(cp);

    // CJK 字形数较多，临时提升图集尺寸为 4096x4096
    auto& cfg = svc->GetConfig();
    int old_w = cfg.default_atlas_width;
    int old_h = cfg.default_atlas_height;
    cfg.default_atlas_width = 4096;
    cfg.default_atlas_height = 4096;

    bool ok = svc->LoadFont(font_id, ttf_path, cjk_codepoints);

    cfg.default_atlas_width = old_w;
    cfg.default_atlas_height = old_h;
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// dse.font.unload(font_id)
int L_FontUnload(lua_State* L) {
    auto* svc = GetFontService();
    if (svc) {
        svc->UnloadFont(luaL_checkstring(L, 1));
    }
    return 0;
}

// dse.font.set_default(font_id)
int L_FontSetDefault(lua_State* L) {
    auto* svc = GetFontService();
    if (!svc) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, svc->SetDefaultFont(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}

// dse.font.measure(text [, font_id [, font_size]]) → width
int L_FontMeasure(lua_State* L) {
    auto* svc = GetFontService();
    if (!svc) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    const char* text = luaL_checkstring(L, 1);
    const char* font_id = luaL_optstring(L, 2, "");
    float font_size = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    lua_pushnumber(L, static_cast<lua_Number>(svc->MeasureText(text, font_id, font_size)));
    return 1;
}

// dse.font.line_height([font_id [, font_size]]) → height
int L_FontLineHeight(lua_State* L) {
    auto* svc = GetFontService();
    if (!svc) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    const char* font_id = luaL_optstring(L, 1, "");
    float font_size = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    lua_pushnumber(L, static_cast<lua_Number>(svc->GetLineHeight(font_id, font_size)));
    return 1;
}

// dse.font.get_texture([font_id]) → texture_handle (int)
int L_FontGetTexture(lua_State* L) {
    auto* svc = GetFontService();
    if (!svc) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const char* font_id = luaL_optstring(L, 1, "");
    std::string fid = font_id[0] ? font_id : svc->GetDefaultFontId();
    auto* fi = svc->GetFont(fid);
    lua_pushinteger(L, fi ? static_cast<lua_Integer>(fi->gpu_texture_handle) : 0);
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
