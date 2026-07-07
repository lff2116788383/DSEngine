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
    float glyph_w = static_cast<float>(luaL_checknumber(L, 8));
    float glyph_h = static_cast<float>(luaL_checknumber(L, 9));
    float spacing = static_cast<float>(luaL_checknumber(L, 10));
    int atlas_cols = static_cast<int>(luaL_checkinteger(L, 11));
    int atlas_rows = static_cast<int>(luaL_checkinteger(L, 12));
    int ascii_start = static_cast<int>(luaL_checkinteger(L, 13));
    float offset_x = static_cast<float>(luaL_checknumber(L, 14));
    float offset_y = static_cast<float>(luaL_checknumber(L, 15));
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
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t number = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_ui_set_label_number(e, number);
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
    int is_password = static_cast<int>(luaL_checkinteger(L, 4));
    dse_ui_add_text_input(e, placeholder, max_length, is_password);
    return 0;
}

int L_dse_ui_set_text_input_text(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    dse_ui_set_text_input_text(e, text);
    return 0;
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
    int horizontal = static_cast<int>(luaL_checkinteger(L, 4));
    int vertical = static_cast<int>(luaL_checkinteger(L, 5));
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
    int whole_numbers = static_cast<int>(luaL_checkinteger(L, 5));
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
    int is_on = static_cast<int>(luaL_checkinteger(L, 2));
    int group = static_cast<int>(luaL_checkinteger(L, 3));
    dse_ui_add_toggle(e, is_on, group);
    return 0;
}

int L_dse_ui_set_toggle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int is_on = static_cast<int>(luaL_checkinteger(L, 2));
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

} // namespace

void RegisterUiBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ui");
    helper::RegisterBindings(L, {
        {"uiaddrenderer", L_dse_ui_add_renderer},
        {"uiaddlabel", L_dse_ui_add_label},
        {"uiaddttflabel", L_dse_ui_add_ttf_label},
        {"uisetlabeltext", L_dse_ui_set_label_text},
        {"uisetlabelfont", L_dse_ui_set_label_font},
        {"uisetlabellayout", L_dse_ui_set_label_layout},
        {"uisetlabelnumber", L_dse_ui_set_label_number},
        {"uiaddpanel", L_dse_ui_add_panel},
        {"uiaddbutton", L_dse_ui_add_button},
        {"uisetbuttonscale", L_dse_ui_set_button_scale},
        {"uiaddmask", L_dse_ui_add_mask},
        {"uiaddrichtext", L_dse_ui_add_rich_text},
        {"uisetrichtext", L_dse_ui_set_rich_text},
        {"uiaddjoystick", L_dse_ui_add_joystick},
        {"uigetjoystickx", L_dse_ui_get_joystick_x},
        {"uigetjoysticky", L_dse_ui_get_joystick_y},
        {"uisetposition", L_dse_ui_set_position},
        {"uisetsize", L_dse_ui_set_size},
        {"uisetanchor", L_dse_ui_set_anchor},
        {"uisetcolor", L_dse_ui_set_color},
        {"uisetvisible", L_dse_ui_set_visible},
        {"uisetuv", L_dse_ui_set_uv},
        {"uisetnineslice", L_dse_ui_set_nine_slice},
        {"uiaddanchor", L_dse_ui_add_anchor},
        {"uisetanchortype", L_dse_ui_set_anchor_type},
        {"uisetanchoroffset", L_dse_ui_set_anchor_offset},
        {"uiaddgridlayout", L_dse_ui_add_grid_layout},
        {"uisetgridlayout", L_dse_ui_set_grid_layout},
        {"uiaddcanvasscaler", L_dse_ui_add_canvas_scaler},
        {"uisetcanvasscaler", L_dse_ui_set_canvas_scaler},
        {"uiaddboxlayout", L_dse_ui_add_box_layout},
        {"uisetboxlayout", L_dse_ui_set_box_layout},
        {"uiaddcontentsizefitter", L_dse_ui_add_content_size_fitter},
        {"uiaddanimation", L_dse_ui_add_animation},
        {"uianimateposition", L_dse_ui_animate_position},
        {"uianimatescale", L_dse_ui_animate_scale},
        {"uianimatealpha", L_dse_ui_animate_alpha},
        {"uianimatecolor", L_dse_ui_animate_color},
        {"uistopanimation", L_dse_ui_stop_animation},
        {"uiaddtextinput", L_dse_ui_add_text_input},
        {"uisettextinputtext", L_dse_ui_set_text_input_text},
        {"uisettextinputplaceholder", L_dse_ui_set_text_input_placeholder},
        {"uisettextinputfocus", L_dse_ui_set_text_input_focus},
        {"uiaddscrollview", L_dse_ui_add_scroll_view},
        {"uisetscrolloffset", L_dse_ui_set_scroll_offset},
        {"uisetscrollcontentsize", L_dse_ui_set_scroll_content_size},
        {"uiaddslider", L_dse_ui_add_slider},
        {"uisetslidervalue", L_dse_ui_set_slider_value},
        {"uigetslidervalue", L_dse_ui_get_slider_value},
        {"uisetslidercolors", L_dse_ui_set_slider_colors},
        {"uisetsliderhandlesize", L_dse_ui_set_slider_handle_size},
        {"uisetslidervertical", L_dse_ui_set_slider_vertical},
        {"uisetsliderrange", L_dse_ui_set_slider_range},
        {"uiaddtoggle", L_dse_ui_add_toggle},
        {"uisettoggle", L_dse_ui_set_toggle},
        {"uigettoggle", L_dse_ui_get_toggle},
        {"uiaddprogressbar", L_dse_ui_add_progress_bar},
        {"uisetprogress", L_dse_ui_set_progress},
        {"uigetprogress", L_dse_ui_get_progress},
        {"uiadddropdown", L_dse_ui_add_dropdown},
        {"uidropdownaddoption", L_dse_ui_dropdown_add_option},
        {"uidropdownclearoptions", L_dse_ui_dropdown_clear_options},
        {"uisetdropdownindex", L_dse_ui_set_dropdown_index},
        {"uigetdropdownindex", L_dse_ui_get_dropdown_index},
        {"uisetdropdownopen", L_dse_ui_set_dropdown_open},
        {"uiaddfilledimage", L_dse_ui_add_filled_image},
        {"uisetfillamount", L_dse_ui_set_fill_amount},
        {"uigetfillamount", L_dse_ui_get_fill_amount},
        {"uisetfillmethod", L_dse_ui_set_fill_method},
        {"uiaddfocusnavigable", L_dse_ui_add_focus_navigable},
        {"uisetfocusnav", L_dse_ui_set_focus_nav},
        {"uiisfocused", L_dse_ui_is_focused},
        {"uisetfocustint", L_dse_ui_set_focus_tint},
        {"uiaddeventpropagation", L_dse_ui_add_event_propagation},
        {"uistoppropagation", L_dse_ui_stop_propagation},
        {"uiaddvisualeffect", L_dse_ui_add_visual_effect},
        {"uisetcornerradius", L_dse_ui_set_corner_radius},
        {"uisetgradient", L_dse_ui_set_gradient},
        {"uisetblur", L_dse_ui_set_blur},
        {"uiaddvirtualscroll", L_dse_ui_add_virtual_scroll},
        {"uisetvirtualscrollcount", L_dse_ui_set_virtual_scroll_count},
        {"uidestroyvirtualscroll", L_dse_ui_destroy_virtual_scroll},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
