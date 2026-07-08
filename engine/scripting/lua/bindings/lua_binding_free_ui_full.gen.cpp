/**
 * @file lua_binding_free_ui_full.gen.cpp
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

int L_dse_ui_add_renderer(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t texture_handle = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    float r = static_cast<float>(luaL_checknumber(L, 3));
    float g = static_cast<float>(luaL_checknumber(L, 4));
    float b = static_cast<float>(luaL_checknumber(L, 5));
    float a = static_cast<float>(luaL_checknumber(L, 6));
    int order = static_cast<int>(luaL_checkinteger(L, 7));
    float w = static_cast<float>(luaL_checknumber(L, 8));
    float h = static_cast<float>(luaL_checknumber(L, 9));
    dse_ui_add_renderer(e, texture_handle, r, g, b, a, order, w, h);
    return 0;
}

int L_dse_ui_add_label(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    uint32_t font_tex_handle = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    float r = static_cast<float>(luaL_checknumber(L, 4));
    float g = static_cast<float>(luaL_checknumber(L, 5));
    float b = static_cast<float>(luaL_checknumber(L, 6));
    float a = static_cast<float>(luaL_checknumber(L, 7));
    float glyph_w = static_cast<float>(luaL_optnumber(L, 8, 8.0));
    float glyph_h = static_cast<float>(luaL_optnumber(L, 9, 16.0));
    float spacing = static_cast<float>(luaL_optnumber(L, 10, 0.0));
    int atlas_cols = static_cast<int>(luaL_optinteger(L, 11, 16));
    int atlas_rows = static_cast<int>(luaL_optinteger(L, 12, 16));
    int ascii_start = static_cast<int>(luaL_optinteger(L, 13, 32));
    float offset_x = static_cast<float>(luaL_optnumber(L, 14, 0.0));
    float offset_y = static_cast<float>(luaL_optnumber(L, 15, 0.0));
    dse_ui_add_label(e, text, font_tex_handle, r, g, b, a, glyph_w, glyph_h, spacing, atlas_cols, atlas_rows, ascii_start, offset_x, offset_y);
    return 0;
}

int L_dse_ui_add_ttf_label(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    const char* font_id = luaL_checkstring(L, 3);
    float font_size = static_cast<float>(luaL_checknumber(L, 4));
    float r = static_cast<float>(luaL_checknumber(L, 5));
    float g = static_cast<float>(luaL_checknumber(L, 6));
    float b = static_cast<float>(luaL_checknumber(L, 7));
    float a = static_cast<float>(luaL_checknumber(L, 8));
    dse_ui_add_ttf_label(e, text, font_id, font_size, r, g, b, a);
    return 0;
}

int L_dse_ui_set_label_text(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    dse_ui_set_label_text(e, text);
    return 0;
}

int L_dse_ui_set_label_font(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* font_id = luaL_checkstring(L, 2);
    float font_size = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_set_label_font(e, font_id, font_size);
    return 0;
}

int L_dse_ui_set_label_layout(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float max_width = static_cast<float>(luaL_checknumber(L, 2));
    int align = static_cast<int>(luaL_checkinteger(L, 3));
    int overflow = static_cast<int>(luaL_checkinteger(L, 4));
    int max_lines = static_cast<int>(luaL_checkinteger(L, 5));
    float line_spacing = static_cast<float>(luaL_checknumber(L, 6));
    dse_ui_set_label_layout(e, max_width, align, overflow, max_lines, line_spacing);
    return 0;
}

int L_dse_ui_set_label_number(lua_State* L) {
    dse_ui_set_label_number(static_cast<uint32_t>(luaL_checkinteger(L, 1)), static_cast<long long>(luaL_checkinteger(L, 2)));
    return 0;
}

int L_dse_ui_add_panel(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int blocks_input = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ui_add_panel(e, blocks_input);
    return 0;
}

int L_dse_ui_add_button(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_checknumber(L, 5));
    dse_ui_add_button(e, r, g, b, a);
    return 0;
}

int L_dse_ui_set_button_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float hover = static_cast<float>(luaL_checknumber(L, 2));
    float pressed = static_cast<float>(luaL_checknumber(L, 3));
    float lerp_speed = static_cast<float>(luaL_checknumber(L, 4));
    dse_ui_set_button_scale(e, hover, pressed, lerp_speed);
    return 0;
}

int L_dse_ui_add_mask(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float w = static_cast<float>(luaL_checknumber(L, 2));
    float h = static_cast<float>(luaL_checknumber(L, 3));
    float ox = static_cast<float>(luaL_checknumber(L, 4));
    float oy = static_cast<float>(luaL_checknumber(L, 5));
    int block_outside = static_cast<int>(luaL_checkinteger(L, 6));
    dse_ui_add_mask(e, w, h, ox, oy, block_outside);
    return 0;
}

int L_dse_ui_add_rich_text(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    float r = static_cast<float>(luaL_checknumber(L, 3));
    float g = static_cast<float>(luaL_checknumber(L, 4));
    float b = static_cast<float>(luaL_checknumber(L, 5));
    float a = static_cast<float>(luaL_checknumber(L, 6));
    int shadow = static_cast<int>(luaL_checkinteger(L, 7));
    int outline = static_cast<int>(luaL_checkinteger(L, 8));
    dse_ui_add_rich_text(e, text, r, g, b, a, shadow, outline);
    return 0;
}

int L_dse_ui_set_rich_text(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    dse_ui_set_rich_text(e, text);
    return 0;
}

int L_dse_ui_add_joystick(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float max_radius = static_cast<float>(luaL_checknumber(L, 2));
    int follow_pointer = static_cast<int>(luaL_checkinteger(L, 3));
    int reset_on_release = static_cast<int>(luaL_checkinteger(L, 4));
    dse_ui_add_joystick(e, max_radius, follow_pointer, reset_on_release);
    return 0;
}

int L_dse_ui_get_joystick_x(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_ui_get_joystick_x(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_ui_get_joystick_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_ui_get_joystick_y(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_ui_set_position(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_set_position(e, x, y);
    return 0;
}

int L_dse_ui_set_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float w = static_cast<float>(luaL_checknumber(L, 2));
    float h = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_set_size(e, w, h);
    return 0;
}

int L_dse_ui_set_anchor(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float ax = static_cast<float>(luaL_checknumber(L, 2));
    float ay = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_set_anchor(e, ax, ay);
    return 0;
}

int L_dse_ui_set_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_checknumber(L, 5));
    dse_ui_set_color(e, r, g, b, a);
    return 0;
}

int L_dse_ui_set_visible(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int visible = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ui_set_visible(e, visible);
    return 0;
}

int L_dse_ui_set_uv(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float u = static_cast<float>(luaL_checknumber(L, 2));
    float v = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_checknumber(L, 4));
    float h = static_cast<float>(luaL_checknumber(L, 5));
    dse_ui_set_uv(e, u, v, w, h);
    return 0;
}

int L_dse_ui_set_nine_slice(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    float l = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float r = static_cast<float>(luaL_checknumber(L, 5));
    float t = static_cast<float>(luaL_checknumber(L, 6));
    float sw = static_cast<float>(luaL_checknumber(L, 7));
    float sh = static_cast<float>(luaL_checknumber(L, 8));
    dse_ui_set_nine_slice(e, enabled, l, b, r, t, sw, sh);
    return 0;
}

int L_dse_ui_add_anchor(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int anchor_type = static_cast<int>(luaL_checkinteger(L, 2));
    float ox = static_cast<float>(luaL_checknumber(L, 3));
    float oy = static_cast<float>(luaL_checknumber(L, 4));
    dse_ui_add_anchor(e, anchor_type, ox, oy);
    return 0;
}

int L_dse_ui_set_anchor_type(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int anchor_type = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ui_set_anchor_type(e, anchor_type);
    return 0;
}

int L_dse_ui_set_anchor_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float ox = static_cast<float>(luaL_checknumber(L, 2));
    float oy = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_set_anchor_offset(e, ox, oy);
    return 0;
}

int L_dse_ui_add_grid_layout(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int columns = static_cast<int>(luaL_checkinteger(L, 2));
    float cw = static_cast<float>(luaL_checknumber(L, 3));
    float ch = static_cast<float>(luaL_checknumber(L, 4));
    float sx = static_cast<float>(luaL_checknumber(L, 5));
    float sy = static_cast<float>(luaL_checknumber(L, 6));
    dse_ui_add_grid_layout(e, columns, cw, ch, sx, sy);
    return 0;
}

int L_dse_ui_set_grid_layout(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int columns = static_cast<int>(luaL_checkinteger(L, 2));
    int rows = static_cast<int>(luaL_checkinteger(L, 3));
    float cw = static_cast<float>(luaL_checknumber(L, 4));
    float ch = static_cast<float>(luaL_checknumber(L, 5));
    float sx = static_cast<float>(luaL_checknumber(L, 6));
    float sy = static_cast<float>(luaL_checknumber(L, 7));
    int alignment = static_cast<int>(luaL_checkinteger(L, 8));
    dse_ui_set_grid_layout(e, columns, rows, cw, ch, sx, sy, alignment);
    return 0;
}

int L_dse_ui_add_canvas_scaler(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float ref_w = static_cast<float>(luaL_checknumber(L, 2));
    float ref_h = static_cast<float>(luaL_checknumber(L, 3));
    int match = static_cast<int>(luaL_checkinteger(L, 4));
    dse_ui_add_canvas_scaler(e, ref_w, ref_h, match);
    return 0;
}

int L_dse_ui_set_canvas_scaler(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float ref_w = static_cast<float>(luaL_checknumber(L, 2));
    float ref_h = static_cast<float>(luaL_checknumber(L, 3));
    float scale_factor = static_cast<float>(luaL_checknumber(L, 4));
    int match_wh = static_cast<int>(luaL_checkinteger(L, 5));
    float match = static_cast<float>(luaL_checknumber(L, 6));
    int pixel_snap = static_cast<int>(luaL_checkinteger(L, 7));
    dse_ui_set_canvas_scaler(e, ref_w, ref_h, scale_factor, match_wh, match, pixel_snap);
    return 0;
}

int L_dse_ui_add_box_layout(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int vertical = static_cast<int>(luaL_checkinteger(L, 2));
    float spacing = static_cast<float>(luaL_checknumber(L, 3));
    float pad_x = static_cast<float>(luaL_checknumber(L, 4));
    float pad_y = static_cast<float>(luaL_checknumber(L, 5));
    int align_main = static_cast<int>(luaL_checkinteger(L, 6));
    int align_cross = static_cast<int>(luaL_checkinteger(L, 7));
    int reverse = static_cast<int>(luaL_checkinteger(L, 8));
    dse_ui_add_box_layout(e, vertical, spacing, pad_x, pad_y, align_main, align_cross, reverse);
    return 0;
}

int L_dse_ui_set_box_layout(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int vertical = static_cast<int>(luaL_checkinteger(L, 2));
    float spacing = static_cast<float>(luaL_checknumber(L, 3));
    float pad_x = static_cast<float>(luaL_checknumber(L, 4));
    float pad_y = static_cast<float>(luaL_checknumber(L, 5));
    int align_main = static_cast<int>(luaL_checkinteger(L, 6));
    int align_cross = static_cast<int>(luaL_checkinteger(L, 7));
    int reverse = static_cast<int>(luaL_checkinteger(L, 8));
    dse_ui_set_box_layout(e, vertical, spacing, pad_x, pad_y, align_main, align_cross, reverse);
    return 0;
}

int L_dse_ui_add_content_size_fitter(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int fit_w = static_cast<int>(luaL_checkinteger(L, 2));
    int fit_h = static_cast<int>(luaL_checkinteger(L, 3));
    float min_w = static_cast<float>(luaL_checknumber(L, 4));
    float min_h = static_cast<float>(luaL_checknumber(L, 5));
    float max_w = static_cast<float>(luaL_checknumber(L, 6));
    float max_h = static_cast<float>(luaL_checknumber(L, 7));
    dse_ui_add_content_size_fitter(e, fit_w, fit_h, min_w, min_h, max_w, max_h);
    return 0;
}

int L_dse_ui_add_animation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float duration = static_cast<float>(luaL_checknumber(L, 2));
    int easing = static_cast<int>(luaL_checkinteger(L, 3));
    int loop = static_cast<int>(luaL_checkinteger(L, 4));
    int ping_pong = static_cast<int>(luaL_checkinteger(L, 5));
    float delay = static_cast<float>(luaL_checknumber(L, 6));
    dse_ui_add_animation(e, duration, easing, loop, ping_pong, delay);
    return 0;
}

int L_dse_ui_animate_position(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float tx = static_cast<float>(luaL_checknumber(L, 2));
    float ty = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_animate_position(e, tx, ty);
    return 0;
}

int L_dse_ui_animate_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float sx = static_cast<float>(luaL_checknumber(L, 2));
    float sy = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_animate_scale(e, sx, sy);
    return 0;
}

int L_dse_ui_animate_alpha(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float alpha = static_cast<float>(luaL_checknumber(L, 2));
    dse_ui_animate_alpha(e, alpha);
    return 0;
}

int L_dse_ui_animate_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_checknumber(L, 5));
    dse_ui_animate_color(e, r, g, b, a);
    return 0;
}

int L_dse_ui_stop_animation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ui_stop_animation(e);
    return 0;
}

int L_dse_ui_add_text_input(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* placeholder = luaL_checkstring(L, 2);
    int max_length = static_cast<int>(luaL_checkinteger(L, 3));
    int is_password = helper::CheckBool(L, 4) ? 1 : 0;
    dse_ui_add_text_input(e, placeholder, max_length, is_password);
    return 0;
}

int L_dse_ui_set_text_input_text(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    dse_ui_set_text_input_text(e, text);
    return 0;
}

int L_dse_compat_ui_get_text_input_text(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* _ret = dse_compat_ui_get_text_input_text(e);
    lua_pushstring(L, _ret ? _ret : "");
    return 1;
}

int L_dse_ui_set_text_input_placeholder(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* placeholder = luaL_checkstring(L, 2);
    dse_ui_set_text_input_placeholder(e, placeholder);
    return 0;
}

int L_dse_ui_set_text_input_focus(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int focused = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ui_set_text_input_focus(e, focused);
    return 0;
}

int L_dse_ui_add_scroll_view(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float cw = static_cast<float>(luaL_checknumber(L, 2));
    float ch = static_cast<float>(luaL_checknumber(L, 3));
    int horizontal = helper::CheckBool(L, 4) ? 1 : 0;
    int vertical = helper::CheckBool(L, 5) ? 1 : 0;
    dse_ui_add_scroll_view(e, cw, ch, horizontal, vertical);
    return 0;
}

int L_dse_ui_set_scroll_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_set_scroll_offset(e, x, y);
    return 0;
}

int L_dse_ui_set_scroll_content_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float w = static_cast<float>(luaL_checknumber(L, 2));
    float h = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_set_scroll_content_size(e, w, h);
    return 0;
}

int L_dse_ui_add_slider(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float min_value = static_cast<float>(luaL_checknumber(L, 2));
    float max_value = static_cast<float>(luaL_checknumber(L, 3));
    float value = static_cast<float>(luaL_checknumber(L, 4));
    int whole_numbers = helper::CheckBool(L, 5) ? 1 : 0;
    dse_ui_add_slider(e, min_value, max_value, value, whole_numbers);
    return 0;
}

int L_dse_ui_set_slider_value(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float value = static_cast<float>(luaL_checknumber(L, 2));
    dse_ui_set_slider_value(e, value);
    return 0;
}

int L_dse_ui_get_slider_value(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_ui_get_slider_value(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_ui_set_slider_colors(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float tr = static_cast<float>(luaL_checknumber(L, 2));
    float tg = static_cast<float>(luaL_checknumber(L, 3));
    float tb = static_cast<float>(luaL_checknumber(L, 4));
    float ta = static_cast<float>(luaL_checknumber(L, 5));
    float fr = static_cast<float>(luaL_checknumber(L, 6));
    float fg = static_cast<float>(luaL_checknumber(L, 7));
    float fb = static_cast<float>(luaL_checknumber(L, 8));
    float fa = static_cast<float>(luaL_checknumber(L, 9));
    float hr = static_cast<float>(luaL_checknumber(L, 10));
    float hg = static_cast<float>(luaL_checknumber(L, 11));
    float hb = static_cast<float>(luaL_checknumber(L, 12));
    float ha = static_cast<float>(luaL_checknumber(L, 13));
    dse_ui_set_slider_colors(e, tr, tg, tb, ta, fr, fg, fb, fa, hr, hg, hb, ha);
    return 0;
}

int L_dse_ui_set_slider_handle_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float size = static_cast<float>(luaL_checknumber(L, 2));
    dse_ui_set_slider_handle_size(e, size);
    return 0;
}

int L_dse_ui_set_slider_vertical(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int vertical = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ui_set_slider_vertical(e, vertical);
    return 0;
}

int L_dse_ui_set_slider_range(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float min_val = static_cast<float>(luaL_checknumber(L, 2));
    float max_val = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_set_slider_range(e, min_val, max_val);
    return 0;
}

int L_dse_ui_add_toggle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int is_on = helper::CheckBool(L, 2) ? 1 : 0;
    int group = static_cast<int>(luaL_checkinteger(L, 3));
    dse_ui_add_toggle(e, is_on, group);
    return 0;
}

int L_dse_ui_set_toggle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int is_on = helper::CheckBool(L, 2) ? 1 : 0;
    dse_ui_set_toggle(e, is_on);
    return 0;
}

int L_dse_ui_get_toggle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ui_get_toggle(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ui_add_progress_bar(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float value = static_cast<float>(luaL_checknumber(L, 2));
    float max_value = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_add_progress_bar(e, value, max_value);
    return 0;
}

int L_dse_ui_set_progress(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float value = static_cast<float>(luaL_checknumber(L, 2));
    dse_ui_set_progress(e, value);
    return 0;
}

int L_dse_ui_get_progress(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_ui_get_progress(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_ui_add_dropdown(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float item_height = static_cast<float>(luaL_checknumber(L, 2));
    int max_visible = static_cast<int>(luaL_checkinteger(L, 3));
    dse_ui_add_dropdown(e, item_height, max_visible);
    return 0;
}

int L_dse_ui_dropdown_add_option(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    const char* value = luaL_checkstring(L, 3);
    dse_ui_dropdown_add_option(e, text, value);
    return 0;
}

int L_dse_ui_dropdown_clear_options(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ui_dropdown_clear_options(e);
    return 0;
}

int L_dse_ui_set_dropdown_index(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int index = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ui_set_dropdown_index(e, index);
    return 0;
}

int L_dse_ui_get_dropdown_index(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ui_get_dropdown_index(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ui_set_dropdown_open(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int open = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ui_set_dropdown_open(e, open);
    return 0;
}

int L_dse_ui_add_filled_image(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float fill_amount = static_cast<float>(luaL_checknumber(L, 2));
    int method = static_cast<int>(luaL_checkinteger(L, 3));
    int origin = static_cast<int>(luaL_checkinteger(L, 4));
    int clockwise = static_cast<int>(luaL_checkinteger(L, 5));
    dse_ui_add_filled_image(e, fill_amount, method, origin, clockwise);
    return 0;
}

int L_dse_ui_set_fill_amount(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float amount = static_cast<float>(luaL_checknumber(L, 2));
    dse_ui_set_fill_amount(e, amount);
    return 0;
}

int L_dse_ui_get_fill_amount(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_ui_get_fill_amount(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_ui_set_fill_method(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int method = static_cast<int>(luaL_checkinteger(L, 2));
    int origin = static_cast<int>(luaL_checkinteger(L, 3));
    int clockwise = static_cast<int>(luaL_checkinteger(L, 4));
    dse_ui_set_fill_method(e, method, origin, clockwise);
    return 0;
}

int L_dse_ui_add_focus_navigable(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int tab_index = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ui_add_focus_navigable(e, tab_index);
    return 0;
}

int L_dse_ui_set_focus_nav(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t up = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t down = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    uint32_t left = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    uint32_t right = static_cast<uint32_t>(luaL_checkinteger(L, 5));
    dse_ui_set_focus_nav(e, up, down, left, right);
    return 0;
}

int L_dse_ui_is_focused(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ui_is_focused(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ui_set_focus_tint(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_checknumber(L, 5));
    dse_ui_set_focus_tint(e, r, g, b, a);
    return 0;
}

int L_dse_ui_add_event_propagation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int bubbles_click = static_cast<int>(luaL_checkinteger(L, 2));
    int bubbles_hover = static_cast<int>(luaL_checkinteger(L, 3));
    dse_ui_add_event_propagation(e, bubbles_click, bubbles_hover);
    return 0;
}

int L_dse_ui_stop_propagation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ui_stop_propagation(e);
    return 0;
}

int L_dse_ui_add_visual_effect(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ui_add_visual_effect(e);
    return 0;
}

int L_dse_ui_set_corner_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float radius = static_cast<float>(luaL_checknumber(L, 2));
    dse_ui_set_corner_radius(e, radius);
    return 0;
}

int L_dse_ui_set_gradient(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float sr = static_cast<float>(luaL_checknumber(L, 2));
    float sg = static_cast<float>(luaL_checknumber(L, 3));
    float sb = static_cast<float>(luaL_checknumber(L, 4));
    float sa = static_cast<float>(luaL_checknumber(L, 5));
    float er = static_cast<float>(luaL_checknumber(L, 6));
    float eg = static_cast<float>(luaL_checknumber(L, 7));
    float eb = static_cast<float>(luaL_checknumber(L, 8));
    float ea = static_cast<float>(luaL_checknumber(L, 9));
    int direction = static_cast<int>(luaL_checkinteger(L, 10));
    dse_ui_set_gradient(e, sr, sg, sb, sa, er, eg, eb, ea, direction);
    return 0;
}

int L_dse_ui_set_blur(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float radius = static_cast<float>(luaL_checknumber(L, 2));
    float intensity = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_set_blur(e, radius, intensity);
    return 0;
}

int L_dse_ui_add_virtual_scroll(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int total_items = static_cast<int>(luaL_checkinteger(L, 2));
    float item_height = static_cast<float>(luaL_checknumber(L, 3));
    dse_ui_add_virtual_scroll(e, total_items, item_height);
    return 0;
}

int L_dse_ui_set_virtual_scroll_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int count = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ui_set_virtual_scroll_count(e, count);
    return 0;
}

int L_dse_ui_destroy_virtual_scroll(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ui_destroy_virtual_scroll(e);
    return 0;
}

int L_dse_ui_load_from_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint32_t _buf[256];
    int _count = dse_ui_load_from_file(path, _buf, 256);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_pushinteger(L, static_cast<lua_Integer>(_buf[_i]));
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

int L_dse_ui_load_from_json(lua_State* L) {
    const char* json_str = luaL_checkstring(L, 1);
    uint32_t _buf[256];
    int _count = dse_ui_load_from_json(json_str, _buf, 256);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_pushinteger(L, static_cast<lua_Integer>(_buf[_i]));
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

} // namespace

void RegisterUiBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ui");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ui");
    }
    helper::RegisterBindings(L, {
        {"add_renderer", L_dse_ui_add_renderer},
        {"add_label", L_dse_ui_add_label},
        {"add_ttf_label", L_dse_ui_add_ttf_label},
        {"set_label_text", L_dse_ui_set_label_text},
        {"set_label_font", L_dse_ui_set_label_font},
        {"set_label_layout", L_dse_ui_set_label_layout},
        {"set_label_number", L_dse_ui_set_label_number},
        {"add_panel", L_dse_ui_add_panel},
        {"add_button", L_dse_ui_add_button},
        {"set_button_scale", L_dse_ui_set_button_scale},
        {"add_mask", L_dse_ui_add_mask},
        {"add_rich_text", L_dse_ui_add_rich_text},
        {"set_rich_text", L_dse_ui_set_rich_text},
        {"add_joystick", L_dse_ui_add_joystick},
        {"get_joystick_x", L_dse_ui_get_joystick_x},
        {"get_joystick_y", L_dse_ui_get_joystick_y},
        {"set_position", L_dse_ui_set_position},
        {"set_size", L_dse_ui_set_size},
        {"set_anchor", L_dse_ui_set_anchor},
        {"set_color", L_dse_ui_set_color},
        {"set_visible", L_dse_ui_set_visible},
        {"set_uv", L_dse_ui_set_uv},
        {"set_nine_slice", L_dse_ui_set_nine_slice},
        {"add_anchor", L_dse_ui_add_anchor},
        {"set_anchor_type", L_dse_ui_set_anchor_type},
        {"set_anchor_offset", L_dse_ui_set_anchor_offset},
        {"add_grid_layout", L_dse_ui_add_grid_layout},
        {"set_grid_layout", L_dse_ui_set_grid_layout},
        {"add_canvas_scaler", L_dse_ui_add_canvas_scaler},
        {"set_canvas_scaler", L_dse_ui_set_canvas_scaler},
        {"add_box_layout", L_dse_ui_add_box_layout},
        {"set_box_layout", L_dse_ui_set_box_layout},
        {"add_content_size_fitter", L_dse_ui_add_content_size_fitter},
        {"add_animation", L_dse_ui_add_animation},
        {"animate_position", L_dse_ui_animate_position},
        {"animate_scale", L_dse_ui_animate_scale},
        {"animate_alpha", L_dse_ui_animate_alpha},
        {"animate_color", L_dse_ui_animate_color},
        {"stop_animation", L_dse_ui_stop_animation},
        {"add_text_input", L_dse_ui_add_text_input},
        {"set_text_input_text", L_dse_ui_set_text_input_text},
        {"get_text_input_text", L_dse_compat_ui_get_text_input_text},
        {"set_text_input_placeholder", L_dse_ui_set_text_input_placeholder},
        {"set_text_input_focus", L_dse_ui_set_text_input_focus},
        {"add_scroll_view", L_dse_ui_add_scroll_view},
        {"set_scroll_offset", L_dse_ui_set_scroll_offset},
        {"set_scroll_content_size", L_dse_ui_set_scroll_content_size},
        {"add_slider", L_dse_ui_add_slider},
        {"set_slider_value", L_dse_ui_set_slider_value},
        {"get_slider_value", L_dse_ui_get_slider_value},
        {"set_slider_colors", L_dse_ui_set_slider_colors},
        {"set_slider_handle_size", L_dse_ui_set_slider_handle_size},
        {"set_slider_vertical", L_dse_ui_set_slider_vertical},
        {"set_slider_range", L_dse_ui_set_slider_range},
        {"add_toggle", L_dse_ui_add_toggle},
        {"set_toggle", L_dse_ui_set_toggle},
        {"get_toggle", L_dse_ui_get_toggle},
        {"add_progress_bar", L_dse_ui_add_progress_bar},
        {"set_progress", L_dse_ui_set_progress},
        {"get_progress", L_dse_ui_get_progress},
        {"add_dropdown", L_dse_ui_add_dropdown},
        {"dropdown_add_option", L_dse_ui_dropdown_add_option},
        {"dropdown_clear_options", L_dse_ui_dropdown_clear_options},
        {"set_dropdown_index", L_dse_ui_set_dropdown_index},
        {"get_dropdown_index", L_dse_ui_get_dropdown_index},
        {"set_dropdown_open", L_dse_ui_set_dropdown_open},
        {"add_filled_image", L_dse_ui_add_filled_image},
        {"set_fill_amount", L_dse_ui_set_fill_amount},
        {"get_fill_amount", L_dse_ui_get_fill_amount},
        {"set_fill_method", L_dse_ui_set_fill_method},
        {"add_focus_navigable", L_dse_ui_add_focus_navigable},
        {"set_focus_nav", L_dse_ui_set_focus_nav},
        {"is_focused", L_dse_ui_is_focused},
        {"set_focus_tint", L_dse_ui_set_focus_tint},
        {"add_event_propagation", L_dse_ui_add_event_propagation},
        {"stop_propagation", L_dse_ui_stop_propagation},
        {"add_visual_effect", L_dse_ui_add_visual_effect},
        {"set_corner_radius", L_dse_ui_set_corner_radius},
        {"set_gradient", L_dse_ui_set_gradient},
        {"set_blur", L_dse_ui_set_blur},
        {"add_virtual_scroll", L_dse_ui_add_virtual_scroll},
        {"set_virtual_scroll_count", L_dse_ui_set_virtual_scroll_count},
        {"destroy_virtual_scroll", L_dse_ui_destroy_virtual_scroll},
        {"load_from_file", L_dse_ui_load_from_file},
        {"load_from_json", L_dse_ui_load_from_json},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
