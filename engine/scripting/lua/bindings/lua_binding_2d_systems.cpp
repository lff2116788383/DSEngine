/**
 * @file lua_binding_2d_systems.cpp
 * @brief Lua 绑定 — 2D 扩展系统 (Parallax, Light2D, Trail, Line, CameraController, AudioSpatial, SpriteSheet, Atlas)
 *
 * 薄包装：仅做 Lua 参数读取与结果入栈，所有逻辑委托 C ABI（dse_api_2d_systems.cpp）。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
#include <vector>
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(Entity e) { return static_cast<uint32_t>(static_cast<entt::id_type>(e)); }

// ============================================================
// #1 Parallax
// ============================================================

int L_AddParallax(lua_State* L) {
    dse_parallax_add(EID(helper::CheckEntity(L, 1)));
    return 0;
}

int L_ParallaxAddLayer(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float sx = helper::OptFloat(L, 2, 1.0f);
    float sy = helper::OptFloat(L, 3, 1.0f);
    lua_pushinteger(L, dse_parallax_add_layer(e, sx, sy));
    return 1;
}

int L_ParallaxSetLayerScroll(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int idx = helper::CheckInt(L, 2);
    float sx = helper::CheckFloat(L, 3);
    float sy = helper::CheckFloat(L, 4);
    dse_parallax_set_layer_scroll(e, idx, sx, sy);
    return 0;
}

int L_ParallaxSetLayerAutoScroll(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int idx = helper::CheckInt(L, 2);
    float sx = helper::CheckFloat(L, 3);
    float sy = helper::OptFloat(L, 4, 0.0f);
    dse_parallax_set_layer_auto_scroll(e, idx, sx, sy);
    return 0;
}

int L_ParallaxSetLayerOpacity(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int idx = helper::CheckInt(L, 2);
    float opacity = helper::CheckFloat(L, 3);
    dse_parallax_set_layer_opacity(e, idx, opacity);
    return 0;
}

int L_ParallaxGetLayerCount(lua_State* L) {
    lua_pushinteger(L, dse_parallax_get_layer_count(EID(helper::CheckEntity(L, 1))));
    return 1;
}

// ============================================================
// #2 Light2D
// ============================================================

int L_AddLight2D(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int type = helper::OptInt(L, 2, 0);
    dse_light2d_add(e, type);
    return 0;
}

int L_SetLight2DColor(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float r = helper::CheckFloat(L, 2);
    float g = helper::CheckFloat(L, 3);
    float b = helper::CheckFloat(L, 4);
    dse_light2d_set_color(e, r, g, b);
    return 0;
}

int L_SetLight2DIntensity(lua_State* L) {
    dse_light2d_set_intensity(EID(helper::CheckEntity(L, 1)), helper::CheckFloat(L, 2));
    return 0;
}

int L_SetLight2DRange(lua_State* L) {
    dse_light2d_set_range(EID(helper::CheckEntity(L, 1)), helper::CheckFloat(L, 2));
    return 0;
}

int L_SetLight2DShadow(lua_State* L) {
    dse_light2d_set_shadow(EID(helper::CheckEntity(L, 1)), helper::CheckInt(L, 2));
    return 0;
}

int L_SetAmbient2D(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float r = helper::CheckFloat(L, 2);
    float g = helper::CheckFloat(L, 3);
    float b = helper::CheckFloat(L, 4);
    float intensity = helper::OptFloat(L, 5, 0.5f);
    dse_light2d_set_ambient(e, r, g, b, intensity);
    return 0;
}

int L_AddNormalMap2D(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float strength = helper::OptFloat(L, 2, 1.0f);
    dse_normal_map_2d_add(e, strength);
    return 0;
}

// ============================================================
// #3 SpriteSheet
// ============================================================

int L_LoadSpriteSheet(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    lua_pushinteger(L, dse_sprite_sheet_load(path));
    return 1;
}

int L_SpriteSheetFrameCount(lua_State* L) {
    lua_pushinteger(L, dse_sprite_sheet_frame_count(helper::CheckInt(L, 1)));
    return 1;
}

int L_SpriteSheetGetFrameUV(lua_State* L) {
    int sheet = helper::CheckInt(L, 1);
    int frame = helper::CheckInt(L, 2);
    float uv[4] = {0, 0, 1, 1};
    dse_sprite_sheet_get_frame_uv(sheet, frame, uv);
    lua_pushnumber(L, uv[0]);
    lua_pushnumber(L, uv[1]);
    lua_pushnumber(L, uv[2]);
    lua_pushnumber(L, uv[3]);
    return 4;
}

// ============================================================
// #4 Atlas
// ============================================================

int L_LoadAtlas(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    lua_pushinteger(L, dse_atlas_load(path));
    return 1;
}

int L_AtlasEntryCount(lua_State* L) {
    lua_pushinteger(L, dse_atlas_entry_count(helper::CheckInt(L, 1)));
    return 1;
}

int L_AtlasGetEntryUV(lua_State* L) {
    int atlas = helper::CheckInt(L, 1);
    const char* name = luaL_checkstring(L, 2);
    float uv[4] = {0, 0, 1, 1};
    dse_atlas_get_entry_uv(atlas, name, uv);
    lua_pushnumber(L, uv[0]);
    lua_pushnumber(L, uv[1]);
    lua_pushnumber(L, uv[2]);
    lua_pushnumber(L, uv[3]);
    return 4;
}

// ============================================================
// #5 Camera Controller 2D
// ============================================================

int L_AddCameraController2D(lua_State* L) {
    dse_camera_controller_2d_add(EID(helper::CheckEntity(L, 1)));
    return 0;
}

int L_CameraShake(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float trauma = helper::CheckFloat(L, 2);
    dse_camera_2d_shake(e, trauma);
    return 0;
}

int L_CameraSetZoom(lua_State* L) {
    dse_camera_2d_set_zoom(EID(helper::CheckEntity(L, 1)), helper::CheckFloat(L, 2));
    return 0;
}

int L_CameraSetBounds(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float min_x = helper::CheckFloat(L, 2);
    float min_y = helper::CheckFloat(L, 3);
    float max_x = helper::CheckFloat(L, 4);
    float max_y = helper::CheckFloat(L, 5);
    dse_camera_2d_set_bounds(e, min_x, min_y, max_x, max_y);
    return 0;
}

int L_CameraSetLookAhead(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float lax = helper::CheckFloat(L, 2);
    float lay = helper::CheckFloat(L, 3);
    dse_camera_2d_set_look_ahead(e, lax, lay);
    return 0;
}

// ============================================================
// #6 Trail Renderer
// ============================================================

int L_AddTrailRenderer(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float lifetime = helper::OptFloat(L, 2, 0.5f);
    float start_width = helper::OptFloat(L, 3, 0.5f);
    float end_width = helper::OptFloat(L, 4, 0.0f);
    dse_trail_renderer_add(e, lifetime, start_width, end_width);
    return 0;
}

int L_SetTrailEmitting(lua_State* L) {
    dse_trail_set_emitting(EID(helper::CheckEntity(L, 1)), lua_toboolean(L, 2) != 0 ? 1 : 0);
    return 0;
}

int L_SetTrailColors(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float sr = helper::CheckFloat(L, 2);
    float sg = helper::CheckFloat(L, 3);
    float sb = helper::CheckFloat(L, 4);
    float sa = helper::OptFloat(L, 5, 1.0f);
    float er = helper::OptFloat(L, 6, sr);
    float eg = helper::OptFloat(L, 7, sg);
    float eb = helper::OptFloat(L, 8, sb);
    float ea = helper::OptFloat(L, 9, 0.0f);
    dse_trail_set_colors(e, sr, sg, sb, sa, er, eg, eb, ea);
    return 0;
}

int L_ClearTrail(lua_State* L) {
    dse_trail_clear(EID(helper::CheckEntity(L, 1)));
    return 0;
}

// ============================================================
// #7 Line Renderer
// ============================================================

int L_AddLineRenderer(lua_State* L) {
    dse_line_renderer_add(EID(helper::CheckEntity(L, 1)), helper::OptFloat(L, 2, 0.1f));
    return 0;
}

int L_LineRendererSetPoints(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = static_cast<int>(lua_rawlen(L, 2));
    std::vector<float> pts(n * 2);
    for (int i = 0; i < n; ++i) {
        lua_rawgeti(L, 2, i + 1);
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1); pts[i * 2]     = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, -1, 2); pts[i * 2 + 1] = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    dse_line_renderer_set_points(e, pts.data(), n);
    return 0;
}

int L_LineRendererSetWidth(lua_State* L) {
    dse_line_renderer_set_width(EID(helper::CheckEntity(L, 1)), helper::CheckFloat(L, 2));
    return 0;
}

int L_LineRendererSetColor(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float r = helper::CheckFloat(L, 2);
    float g = helper::CheckFloat(L, 3);
    float b = helper::CheckFloat(L, 4);
    float a = helper::OptFloat(L, 5, 1.0f);
    dse_line_renderer_set_color(e, r, g, b, a);
    return 0;
}

int L_LineRendererSetClosed(lua_State* L) {
    dse_line_renderer_set_closed(EID(helper::CheckEntity(L, 1)), lua_toboolean(L, 2) != 0 ? 1 : 0);
    return 0;
}

// ============================================================
// #8 Audio Spatial 2D
// ============================================================

int L_AddAudioSpatial2D(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float min_d = helper::OptFloat(L, 2, 1.0f);
    float max_d = helper::OptFloat(L, 3, 20.0f);
    dse_audio_spatial_2d_add(e, min_d, max_d);
    return 0;
}

int L_SetAudioSpatial2DRange(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float min_d = helper::CheckFloat(L, 2);
    float max_d = helper::CheckFloat(L, 3);
    dse_audio_spatial_2d_set_range(e, min_d, max_d);
    return 0;
}

int L_SetAudioSpatial2DAttenuation(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int model = helper::CheckInt(L, 2);
    float rolloff = helper::OptFloat(L, 3, 1.0f);
    dse_audio_spatial_2d_set_attenuation(e, model, rolloff);
    return 0;
}

int L_AddAudioListener2D(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float vol = helper::OptFloat(L, 2, 1.0f);
    dse_audio_listener_2d_add(e, vol);
    return 0;
}

} // namespace

void Register2DSystemsBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        // Parallax
        {"add_parallax",                L_AddParallax},
        {"parallax_add_layer",          L_ParallaxAddLayer},
        {"parallax_set_layer_scroll",   L_ParallaxSetLayerScroll},
        {"parallax_set_layer_auto_scroll", L_ParallaxSetLayerAutoScroll},
        {"parallax_set_layer_opacity",  L_ParallaxSetLayerOpacity},
        {"parallax_get_layer_count",    L_ParallaxGetLayerCount},
        // Light2D
        {"add_light_2d",                L_AddLight2D},
        {"set_light_2d_color",          L_SetLight2DColor},
        {"set_light_2d_intensity",      L_SetLight2DIntensity},
        {"set_light_2d_range",          L_SetLight2DRange},
        {"set_light_2d_shadow",         L_SetLight2DShadow},
        {"set_ambient_2d",              L_SetAmbient2D},
        {"add_normal_map_2d",           L_AddNormalMap2D},
        // SpriteSheet
        {"load_sprite_sheet",           L_LoadSpriteSheet},
        {"sprite_sheet_frame_count",    L_SpriteSheetFrameCount},
        {"sprite_sheet_get_frame_uv",   L_SpriteSheetGetFrameUV},
        // Atlas
        {"load_atlas",                  L_LoadAtlas},
        {"atlas_entry_count",           L_AtlasEntryCount},
        {"atlas_get_entry_uv",          L_AtlasGetEntryUV},
        // Camera Controller
        {"add_camera_controller_2d",    L_AddCameraController2D},
        {"camera_shake",                L_CameraShake},
        {"camera_set_zoom",             L_CameraSetZoom},
        {"camera_set_bounds",           L_CameraSetBounds},
        {"camera_set_look_ahead",       L_CameraSetLookAhead},
        // Trail Renderer
        {"add_trail_renderer",          L_AddTrailRenderer},
        {"set_trail_emitting",          L_SetTrailEmitting},
        {"set_trail_colors",            L_SetTrailColors},
        {"clear_trail",                 L_ClearTrail},
        // Line Renderer
        {"add_line_renderer",           L_AddLineRenderer},
        {"line_renderer_set_points",    L_LineRendererSetPoints},
        {"line_renderer_set_width",     L_LineRendererSetWidth},
        {"line_renderer_set_color",     L_LineRendererSetColor},
        {"line_renderer_set_closed",    L_LineRendererSetClosed},
        // Audio Spatial 2D
        {"add_audio_spatial_2d",        L_AddAudioSpatial2D},
        {"set_audio_spatial_2d_range",  L_SetAudioSpatial2DRange},
        {"set_audio_spatial_2d_attenuation", L_SetAudioSpatial2DAttenuation},
        {"add_audio_listener_2d",       L_AddAudioListener2D},
    });
}

} // namespace dse::runtime::lua_binding
