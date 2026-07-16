/**
 * @file lua_binding_free_ecs_2d.gen.cpp
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

int L_dse_parallax_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_parallax_add(e);
    return 0;
}

int L_dse_parallax_add_layer(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float scroll_x = static_cast<float>(luaL_checknumber(L, 2));
    float scroll_y = static_cast<float>(luaL_checknumber(L, 3));
    int _ret = dse_parallax_add_layer(e, scroll_x, scroll_y);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_parallax_set_layer_scroll(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int layer = static_cast<int>(luaL_checkinteger(L, 2));
    float sx = static_cast<float>(luaL_checknumber(L, 3));
    float sy = static_cast<float>(luaL_checknumber(L, 4));
    dse_parallax_set_layer_scroll(e, layer, sx, sy);
    return 0;
}

int L_dse_parallax_set_layer_auto_scroll(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int layer = static_cast<int>(luaL_checkinteger(L, 2));
    float sx = static_cast<float>(luaL_checknumber(L, 3));
    float sy = static_cast<float>(luaL_checknumber(L, 4));
    dse_parallax_set_layer_auto_scroll(e, layer, sx, sy);
    return 0;
}

int L_dse_parallax_set_layer_opacity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int layer = static_cast<int>(luaL_checkinteger(L, 2));
    float opacity = static_cast<float>(luaL_checknumber(L, 3));
    dse_parallax_set_layer_opacity(e, layer, opacity);
    return 0;
}

int L_dse_parallax_get_layer_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_parallax_get_layer_count(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_light2d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int type = static_cast<int>(luaL_checkinteger(L, 2));
    dse_light2d_add(e, type);
    return 0;
}

int L_dse_light2d_set_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    dse_light2d_set_color(e, r, g, b);
    return 0;
}

int L_dse_light2d_set_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float intensity = static_cast<float>(luaL_checknumber(L, 2));
    dse_light2d_set_intensity(e, intensity);
    return 0;
}

int L_dse_light2d_set_range(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float range = static_cast<float>(luaL_checknumber(L, 2));
    dse_light2d_set_range(e, range);
    return 0;
}

int L_dse_light2d_set_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int mode = static_cast<int>(luaL_checkinteger(L, 2));
    dse_light2d_set_shadow(e, mode);
    return 0;
}

int L_dse_light2d_set_ambient(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float intensity = static_cast<float>(luaL_checknumber(L, 5));
    dse_light2d_set_ambient(e, r, g, b, intensity);
    return 0;
}

int L_dse_normal_map_2d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float strength = static_cast<float>(luaL_checknumber(L, 2));
    dse_normal_map_2d_add(e, strength);
    return 0;
}

int L_dse_sprite_sheet_load(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_sprite_sheet_load(path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_sprite_sheet_frame_count(lua_State* L) {
    int sheet = static_cast<int>(luaL_checkinteger(L, 1));
    int _ret = dse_sprite_sheet_frame_count(sheet);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_atlas_load(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_atlas_load(path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_atlas_entry_count(lua_State* L) {
    int atlas = static_cast<int>(luaL_checkinteger(L, 1));
    int _ret = dse_atlas_entry_count(atlas);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_camera_controller_2d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_camera_controller_2d_add(e);
    return 0;
}

int L_dse_camera_2d_shake(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float trauma = static_cast<float>(luaL_checknumber(L, 2));
    dse_camera_2d_shake(e, trauma);
    return 0;
}

int L_dse_camera_2d_set_zoom(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float zoom = static_cast<float>(luaL_checknumber(L, 2));
    dse_camera_2d_set_zoom(e, zoom);
    return 0;
}

int L_dse_camera_2d_set_bounds(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float min_x = static_cast<float>(luaL_checknumber(L, 2));
    float min_y = static_cast<float>(luaL_checknumber(L, 3));
    float max_x = static_cast<float>(luaL_checknumber(L, 4));
    float max_y = static_cast<float>(luaL_checknumber(L, 5));
    dse_camera_2d_set_bounds(e, min_x, min_y, max_x, max_y);
    return 0;
}

int L_dse_camera_2d_set_look_ahead(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float lax = static_cast<float>(luaL_checknumber(L, 2));
    float lay = static_cast<float>(luaL_checknumber(L, 3));
    dse_camera_2d_set_look_ahead(e, lax, lay);
    return 0;
}

int L_dse_trail_renderer_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float lifetime = static_cast<float>(luaL_optnumber(L, 2, 0.5));
    float start_width = static_cast<float>(luaL_optnumber(L, 3, 0.5));
    float end_width = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    dse_trail_renderer_add(e, lifetime, start_width, end_width);
    return 0;
}

int L_dse_trail_set_emitting(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int emitting = helper::CheckBool(L, 2) ? 1 : 0;
    dse_trail_set_emitting(e, emitting);
    return 0;
}

int L_dse_trail_set_colors(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r1 = static_cast<float>(luaL_checknumber(L, 2));
    float g1 = static_cast<float>(luaL_checknumber(L, 3));
    float b1 = static_cast<float>(luaL_checknumber(L, 4));
    float a1 = static_cast<float>(luaL_checknumber(L, 5));
    float r2 = static_cast<float>(luaL_checknumber(L, 6));
    float g2 = static_cast<float>(luaL_checknumber(L, 7));
    float b2 = static_cast<float>(luaL_checknumber(L, 8));
    float a2 = static_cast<float>(luaL_optnumber(L, 9, 0.0));
    dse_trail_set_colors(e, r1, g1, b1, a1, r2, g2, b2, a2);
    return 0;
}

int L_dse_trail_clear(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_trail_clear(e);
    return 0;
}

int L_dse_line_renderer_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float width = static_cast<float>(luaL_checknumber(L, 2));
    dse_line_renderer_add(e, width);
    return 0;
}

int L_dse_line_renderer_set_width(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float width = static_cast<float>(luaL_checknumber(L, 2));
    dse_line_renderer_set_width(e, width);
    return 0;
}

int L_dse_line_renderer_set_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_checknumber(L, 5));
    dse_line_renderer_set_color(e, r, g, b, a);
    return 0;
}

int L_dse_line_renderer_set_closed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int closed = static_cast<int>(luaL_checkinteger(L, 2));
    dse_line_renderer_set_closed(e, closed);
    return 0;
}

int L_dse_audio_spatial_2d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float min_dist = static_cast<float>(luaL_checknumber(L, 2));
    float max_dist = static_cast<float>(luaL_checknumber(L, 3));
    dse_audio_spatial_2d_add(e, min_dist, max_dist);
    return 0;
}

int L_dse_audio_spatial_2d_set_range(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float min_dist = static_cast<float>(luaL_checknumber(L, 2));
    float max_dist = static_cast<float>(luaL_checknumber(L, 3));
    dse_audio_spatial_2d_set_range(e, min_dist, max_dist);
    return 0;
}

int L_dse_audio_spatial_2d_set_attenuation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int model = static_cast<int>(luaL_checkinteger(L, 2));
    float rolloff = static_cast<float>(luaL_checknumber(L, 3));
    dse_audio_spatial_2d_set_attenuation(e, model, rolloff);
    return 0;
}

int L_dse_audio_listener_2d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float global_volume = static_cast<float>(luaL_checknumber(L, 2));
    dse_audio_listener_2d_add(e, global_volume);
    return 0;
}

} // namespace

void Register2DSystemsBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"add_parallax", L_dse_parallax_add},
        {"parallax_add_layer", L_dse_parallax_add_layer},
        {"parallax_set_layer_scroll", L_dse_parallax_set_layer_scroll},
        {"parallax_set_layer_auto_scroll", L_dse_parallax_set_layer_auto_scroll},
        {"parallax_set_layer_opacity", L_dse_parallax_set_layer_opacity},
        {"parallax_get_layer_count", L_dse_parallax_get_layer_count},
        {"add_light_2d", L_dse_light2d_add},
        {"set_light_2d_color", L_dse_light2d_set_color},
        {"set_light_2d_intensity", L_dse_light2d_set_intensity},
        {"set_light_2d_range", L_dse_light2d_set_range},
        {"set_light_2d_shadow", L_dse_light2d_set_shadow},
        {"set_ambient_2d", L_dse_light2d_set_ambient},
        {"add_normal_map_2d", L_dse_normal_map_2d_add},
        {"load_sprite_sheet", L_dse_sprite_sheet_load},
        {"sprite_sheet_frame_count", L_dse_sprite_sheet_frame_count},
        {"load_atlas", L_dse_atlas_load},
        {"atlas_entry_count", L_dse_atlas_entry_count},
        {"add_camera_controller_2d", L_dse_camera_controller_2d_add},
        {"camera_shake", L_dse_camera_2d_shake},
        {"camera_set_zoom", L_dse_camera_2d_set_zoom},
        {"camera_set_bounds", L_dse_camera_2d_set_bounds},
        {"camera_set_look_ahead", L_dse_camera_2d_set_look_ahead},
        {"add_trail_renderer", L_dse_trail_renderer_add},
        {"set_trail_emitting", L_dse_trail_set_emitting},
        {"set_trail_colors", L_dse_trail_set_colors},
        {"clear_trail", L_dse_trail_clear},
        {"add_line_renderer", L_dse_line_renderer_add},
        {"line_renderer_set_width", L_dse_line_renderer_set_width},
        {"line_renderer_set_color", L_dse_line_renderer_set_color},
        {"line_renderer_set_closed", L_dse_line_renderer_set_closed},
        {"add_audio_spatial_2d", L_dse_audio_spatial_2d_add},
        {"set_audio_spatial_2d_range", L_dse_audio_spatial_2d_set_range},
        {"set_audio_spatial_2d_attenuation", L_dse_audio_spatial_2d_set_attenuation},
        {"add_audio_listener_2d", L_dse_audio_listener_2d_add},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
