/**
 * @file lua_binding_ui.cpp
 * @brief Lua UI 绑定 — C ABI 薄包装
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t Ent(lua_State* L, int idx) {
    return static_cast<uint32_t>(luaL_checkinteger(L, idx));
}

inline float F(lua_State* L, int idx) {
    return static_cast<float>(luaL_checknumber(L, idx));
}

inline float OptF(lua_State* L, int idx, float def) {
    return static_cast<float>(luaL_optnumber(L, idx, def));
}

inline int OptI(lua_State* L, int idx, int def) {
    return static_cast<int>(luaL_optinteger(L, idx, def));
}

// ============================================================
// Basic UI (existing C ABI)
// ============================================================

int L_UiAddRenderer(lua_State* L) {
    dse_ui_add_renderer(Ent(L,1), static_cast<uint32_t>(OptI(L,2,0)),
        OptF(L,3,1.0f), OptF(L,4,1.0f), OptF(L,5,1.0f), OptF(L,6,1.0f),
        OptI(L,7,0), OptF(L,8,640.0f), OptF(L,9,160.0f));
    return 0;
}

int L_UiAddLabel(lua_State* L) {
    dse_ui_add_label(Ent(L,1), luaL_checkstring(L,2),
        static_cast<uint32_t>(OptI(L,3,0)),
        OptF(L,4,1.0f), OptF(L,5,1.0f), OptF(L,6,1.0f), OptF(L,7,1.0f),
        OptF(L,8,0.0f), OptF(L,9,0.0f), OptF(L,10,0.0f),
        OptI(L,11,0), OptI(L,12,0), OptI(L,13,0),
        OptF(L,14,0.0f), OptF(L,15,0.0f));
    return 0;
}

int L_UiAddTtfLabel(lua_State* L) {
    dse_ui_add_ttf_label(Ent(L,1), luaL_checkstring(L,2), luaL_checkstring(L,3),
        OptF(L,4,32.0f), OptF(L,5,1.0f), OptF(L,6,1.0f), OptF(L,7,1.0f), OptF(L,8,1.0f));
    return 0;
}

int L_UiSetLabelText(lua_State* L) {
    dse_ui_set_label_text(Ent(L,1), luaL_checkstring(L,2));
    return 0;
}

int L_UiSetLabelFont(lua_State* L) {
    dse_ui_set_label_font(Ent(L,1), luaL_checkstring(L,2), OptF(L,3,0.0f));
    return 0;
}

int L_UiSetLabelLayout(lua_State* L) {
    dse_ui_set_label_layout(Ent(L,1), F(L,2), OptI(L,3,0), OptI(L,4,0), OptI(L,5,0), OptF(L,6,0.0f));
    return 0;
}

int L_UiSetLabelNumber(lua_State* L) {
    dse_ui_set_label_number(Ent(L,1), static_cast<long long>(luaL_checkinteger(L,2)));
    return 0;
}

int L_UiAddPanel(lua_State* L) {
    dse_ui_add_panel(Ent(L,1), lua_toboolean(L,2) ? 1 : 0);
    return 0;
}

int L_UiAddButton(lua_State* L) {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    if (lua_gettop(L) >= 5) {
        r = OptF(L,2,1.0f); g = OptF(L,3,1.0f); b = OptF(L,4,1.0f); a = OptF(L,5,1.0f);
    }
    dse_ui_add_button(Ent(L,1), r, g, b, a);
    return 0;
}

int L_UiSetButtonScale(lua_State* L) {
    dse_ui_set_button_scale(Ent(L,1), OptF(L,2,1.08f), OptF(L,3,0.94f), OptF(L,4,12.0f));
    return 0;
}

int L_UiAddMask(lua_State* L) {
    int block = (lua_gettop(L) >= 6) ? (lua_toboolean(L,6) ? 1 : 0) : 1;
    dse_ui_add_mask(Ent(L,1), OptF(L,2,0.0f), OptF(L,3,0.0f), OptF(L,4,0.0f), OptF(L,5,0.0f), block);
    return 0;
}

int L_UiAddRichText(lua_State* L) {
    int shadow = (lua_gettop(L) >= 7) ? (lua_toboolean(L,7) ? 1 : 0) : 0;
    int outline = (lua_gettop(L) >= 8) ? (lua_toboolean(L,8) ? 1 : 0) : 0;
    dse_ui_add_rich_text(Ent(L,1), luaL_optstring(L,2,""),
        OptF(L,3,1.0f), OptF(L,4,1.0f), OptF(L,5,1.0f), OptF(L,6,1.0f), shadow, outline);
    return 0;
}

int L_UiSetRichText(lua_State* L) {
    dse_ui_set_rich_text(Ent(L,1), luaL_optstring(L,2,""));
    return 0;
}

int L_UiAddJoystick(lua_State* L) {
    int follow = (lua_gettop(L) >= 3) ? (lua_toboolean(L,3) ? 1 : 0) : 1;
    int reset = (lua_gettop(L) >= 4) ? (lua_toboolean(L,4) ? 1 : 0) : 1;
    dse_ui_add_joystick(Ent(L,1), OptF(L,2,64.0f), follow, reset);
    return 0;
}

int L_UiGetJoystickX(lua_State* L) {
    lua_pushnumber(L, dse_ui_get_joystick_x(Ent(L,1)));
    return 1;
}

int L_UiGetJoystickY(lua_State* L) {
    lua_pushnumber(L, dse_ui_get_joystick_y(Ent(L,1)));
    return 1;
}

int L_UiSetPosition(lua_State* L) {
    dse_ui_set_position(Ent(L,1), F(L,2), F(L,3));
    return 0;
}

int L_UiSetSize(lua_State* L) {
    dse_ui_set_size(Ent(L,1), F(L,2), F(L,3));
    return 0;
}

int L_UiSetAnchor(lua_State* L) {
    dse_ui_set_anchor(Ent(L,1), F(L,2), F(L,3));
    return 0;
}

int L_UiSetColor(lua_State* L) {
    dse_ui_set_color(Ent(L,1), F(L,2), F(L,3), F(L,4), OptF(L,5,1.0f));
    return 0;
}

int L_UiSetVisible(lua_State* L) {
    dse_ui_set_visible(Ent(L,1), lua_toboolean(L,2) ? 1 : 0);
    return 0;
}

int L_UiSetUv(lua_State* L) {
    dse_ui_set_uv(Ent(L,1), F(L,2), F(L,3), OptF(L,4,1.0f), OptF(L,5,1.0f));
    return 0;
}

int L_UiSetNineSlice(lua_State* L) {
    dse_ui_set_nine_slice(Ent(L,1), lua_toboolean(L,2)?1:0,
        OptF(L,3,0.0f), OptF(L,4,0.0f), OptF(L,5,0.0f), OptF(L,6,0.0f),
        OptF(L,7,0.0f), OptF(L,8,0.0f));
    return 0;
}

// UIAnchorComponent
int L_UiAddAnchor(lua_State* L) {
    dse_ui_add_anchor(Ent(L,1), OptI(L,2,5), OptF(L,3,0.0f), OptF(L,4,0.0f));
    return 0;
}

int L_UiSetAnchorType(lua_State* L) {
    dse_ui_set_anchor_type(Ent(L,1), static_cast<int>(luaL_checkinteger(L,2)));
    return 0;
}

int L_UiSetAnchorOffset(lua_State* L) {
    dse_ui_set_anchor_offset(Ent(L,1), F(L,2), F(L,3));
    return 0;
}

// UIGridLayoutComponent
int L_UiAddGridLayout(lua_State* L) {
    dse_ui_add_grid_layout(Ent(L,1), OptI(L,2,1), OptF(L,3,100.0f), OptF(L,4,100.0f),
                           OptF(L,5,10.0f), OptF(L,6,10.0f));
    return 0;
}

int L_UiSetGridLayout(lua_State* L) {
    int cols = lua_isnoneornil(L,2) ? 0 : static_cast<int>(luaL_checkinteger(L,2));
    int rows = lua_isnoneornil(L,3) ? 0 : static_cast<int>(luaL_checkinteger(L,3));
    float cw = lua_isnoneornil(L,4) ? 0 : F(L,4);
    float ch = lua_isnoneornil(L,5) ? 0 : F(L,5);
    float sx = lua_isnoneornil(L,6) ? 0 : F(L,6);
    float sy = lua_isnoneornil(L,7) ? 0 : F(L,7);
    int align = lua_isnoneornil(L,8) ? 0 : static_cast<int>(luaL_checkinteger(L,8));
    dse_ui_set_grid_layout(Ent(L,1), cols, rows, cw, ch, sx, sy, align);
    return 0;
}

// UICanvasScalerComponent
int L_UiAddCanvasScaler(lua_State* L) {
    int match = lua_isnoneornil(L,4) ? 1 : (lua_toboolean(L,4) ? 1 : 0);
    dse_ui_add_canvas_scaler(Ent(L,1), OptF(L,2,1920.0f), OptF(L,3,1080.0f), match);
    return 0;
}

int L_UiSetCanvasScaler(lua_State* L) {
    float rw = lua_isnoneornil(L,2) ? 0 : F(L,2);
    float rh = lua_isnoneornil(L,3) ? 0 : F(L,3);
    float sf = lua_isnoneornil(L,4) ? 0 : F(L,4);
    int mwh = lua_isnoneornil(L,5) ? 0 : (lua_toboolean(L,5) ? 1 : 0);
    float match = lua_isnoneornil(L,6) ? 0 : F(L,6);
    int psnap = lua_isnoneornil(L,7) ? 0 : (lua_toboolean(L,7) ? 1 : 0);
    dse_ui_set_canvas_scaler(Ent(L,1), rw, rh, sf, mwh, match, psnap);
    return 0;
}

// UIBoxLayoutComponent
int L_UiAddBoxLayout(lua_State* L) {
    int reverse = (lua_gettop(L) >= 8) ? (lua_toboolean(L,8) ? 1 : 0) : 0;
    dse_ui_add_box_layout(Ent(L,1), lua_toboolean(L,2)?1:0, OptF(L,3,0.0f),
        OptF(L,4,0.0f), OptF(L,5,0.0f), OptI(L,6,0), OptI(L,7,0), reverse);
    return 0;
}

int L_UiSetBoxLayout(lua_State* L) {
    int vert = lua_isnoneornil(L,2) ? 0 : (lua_toboolean(L,2) ? 1 : 0);
    float spacing = lua_isnoneornil(L,3) ? 0 : F(L,3);
    float px = lua_isnoneornil(L,4) ? 0 : F(L,4);
    float py = lua_isnoneornil(L,5) ? 0 : F(L,5);
    int am = lua_isnoneornil(L,6) ? 0 : static_cast<int>(luaL_checkinteger(L,6));
    int ac = lua_isnoneornil(L,7) ? 0 : static_cast<int>(luaL_checkinteger(L,7));
    int rev = lua_isnoneornil(L,8) ? 0 : (lua_toboolean(L,8) ? 1 : 0);
    dse_ui_set_box_layout(Ent(L,1), vert, spacing, px, py, am, ac, rev);
    return 0;
}

// UIContentSizeFitterComponent
int L_UiAddContentSizeFitter(lua_State* L) {
    dse_ui_add_content_size_fitter(Ent(L,1), OptI(L,2,0), OptI(L,3,0),
        OptF(L,4,0.0f), OptF(L,5,0.0f), OptF(L,6,0.0f), OptF(L,7,0.0f));
    return 0;
}

// UIAnimationComponent
int L_UiAddAnimation(lua_State* L) {
    int loop = lua_isnoneornil(L,4) ? 0 : (lua_toboolean(L,4) ? 1 : 0);
    int pp = lua_isnoneornil(L,5) ? 0 : (lua_toboolean(L,5) ? 1 : 0);
    dse_ui_add_animation(Ent(L,1), OptF(L,2,0.3f), OptI(L,3,0), loop, pp, OptF(L,6,0.0f));
    return 0;
}

int L_UiAnimatePosition(lua_State* L) {
    dse_ui_animate_position(Ent(L,1), F(L,2), F(L,3));
    return 0;
}

int L_UiAnimateScale(lua_State* L) {
    dse_ui_animate_scale(Ent(L,1), F(L,2), F(L,3));
    return 0;
}

int L_UiAnimateAlpha(lua_State* L) {
    dse_ui_animate_alpha(Ent(L,1), F(L,2));
    return 0;
}

int L_UiAnimateColor(lua_State* L) {
    dse_ui_animate_color(Ent(L,1), F(L,2), F(L,3), F(L,4), OptF(L,5,1.0f));
    return 0;
}

int L_UiStopAnimation(lua_State* L) {
    dse_ui_stop_animation(Ent(L,1));
    return 0;
}

// UITextInputComponent
int L_UiAddTextInput(lua_State* L) {
    int pw = (lua_gettop(L) >= 4) ? (lua_toboolean(L,4) ? 1 : 0) : 0;
    dse_ui_add_text_input(Ent(L,1), luaL_optstring(L,2,""), OptI(L,3,0), pw);
    return 0;
}

int L_UiSetTextInputText(lua_State* L) {
    dse_ui_set_text_input_text(Ent(L,1), luaL_checkstring(L,2));
    return 0;
}

int L_UiGetTextInputText(lua_State* L) {
    char buf[4096];
    int n = dse_ui_get_text_input_text(Ent(L,1), buf, sizeof(buf));
    lua_pushlstring(L, buf, n);
    return 1;
}

int L_UiSetTextInputPlaceholder(lua_State* L) {
    dse_ui_set_text_input_placeholder(Ent(L,1), luaL_checkstring(L,2));
    return 0;
}

int L_UiSetTextInputFocus(lua_State* L) {
    dse_ui_set_text_input_focus(Ent(L,1), lua_toboolean(L,2) ? 1 : 0);
    return 0;
}

// UIScrollViewComponent
int L_UiAddScrollView(lua_State* L) {
    int horiz = (lua_gettop(L) >= 4) ? (lua_toboolean(L,4) ? 1 : 0) : 0;
    int vert = (lua_gettop(L) >= 5) ? (lua_toboolean(L,5) ? 1 : 0) : 1;
    dse_ui_add_scroll_view(Ent(L,1), OptF(L,2,0.0f), OptF(L,3,0.0f), horiz, vert);
    return 0;
}

int L_UiSetScrollOffset(lua_State* L) {
    dse_ui_set_scroll_offset(Ent(L,1), F(L,2), F(L,3));
    return 0;
}

int L_UiGetScrollOffset(lua_State* L) {
    float x, y;
    dse_ui_get_scroll_offset(Ent(L,1), &x, &y);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

int L_UiSetScrollContentSize(lua_State* L) {
    dse_ui_set_scroll_content_size(Ent(L,1), F(L,2), F(L,3));
    return 0;
}

// UISliderComponent
int L_UiAddSlider(lua_State* L) {
    int whole = (lua_gettop(L) >= 5) ? (lua_toboolean(L,5) ? 1 : 0) : 0;
    dse_ui_add_slider(Ent(L,1), OptF(L,2,0.0f), OptF(L,3,1.0f), OptF(L,4,0.0f), whole);
    return 0;
}

int L_UiSetSliderValue(lua_State* L) {
    dse_ui_set_slider_value(Ent(L,1), F(L,2));
    return 0;
}

int L_UiGetSliderValue(lua_State* L) {
    lua_pushnumber(L, dse_ui_get_slider_value(Ent(L,1)));
    return 1;
}

int L_UiSetSliderColors(lua_State* L) {
    dse_ui_set_slider_colors(Ent(L,1),
        F(L,2), F(L,3), F(L,4), F(L,5),
        F(L,6), F(L,7), F(L,8), F(L,9),
        F(L,10), F(L,11), F(L,12), F(L,13));
    return 0;
}

int L_UiSetSliderHandleSize(lua_State* L) {
    dse_ui_set_slider_handle_size(Ent(L,1), F(L,2));
    return 0;
}

int L_UiSetSliderVertical(lua_State* L) {
    dse_ui_set_slider_vertical(Ent(L,1), lua_toboolean(L,2) ? 1 : 0);
    return 0;
}

int L_UiSetSliderRange(lua_State* L) {
    dse_ui_set_slider_range(Ent(L,1), F(L,2), F(L,3));
    return 0;
}

// UIToggleComponent
int L_UiAddToggle(lua_State* L) {
    int is_on = (lua_gettop(L) >= 2) ? (lua_toboolean(L,2) ? 1 : 0) : 0;
    dse_ui_add_toggle(Ent(L,1), is_on, OptI(L,3,-1));
    return 0;
}

int L_UiSetToggle(lua_State* L) {
    dse_ui_set_toggle(Ent(L,1), lua_toboolean(L,2) ? 1 : 0);
    return 0;
}

int L_UiGetToggle(lua_State* L) {
    lua_pushboolean(L, dse_ui_get_toggle(Ent(L,1)));
    return 1;
}

// UIProgressBarComponent
int L_UiAddProgressBar(lua_State* L) {
    dse_ui_add_progress_bar(Ent(L,1), OptF(L,2,0.0f), OptF(L,3,1.0f));
    return 0;
}

int L_UiSetProgress(lua_State* L) {
    dse_ui_set_progress(Ent(L,1), F(L,2));
    return 0;
}

int L_UiGetProgress(lua_State* L) {
    lua_pushnumber(L, dse_ui_get_progress(Ent(L,1)));
    return 1;
}

// UIDropdownComponent
int L_UiAddDropdown(lua_State* L) {
    dse_ui_add_dropdown(Ent(L,1), OptF(L,2,40.0f), OptI(L,3,5));
    return 0;
}

int L_UiDropdownAddOption(lua_State* L) {
    const char* text = luaL_checkstring(L,2);
    const char* value = luaL_optstring(L,3, text);
    dse_ui_dropdown_add_option(Ent(L,1), text, value);
    return 0;
}

int L_UiDropdownClearOptions(lua_State* L) {
    dse_ui_dropdown_clear_options(Ent(L,1));
    return 0;
}

int L_UiSetDropdownIndex(lua_State* L) {
    dse_ui_set_dropdown_index(Ent(L,1), static_cast<int>(luaL_checkinteger(L,2)));
    return 0;
}

int L_UiGetDropdownIndex(lua_State* L) {
    lua_pushinteger(L, dse_ui_get_dropdown_index(Ent(L,1)));
    return 1;
}

int L_UiGetDropdownValue(lua_State* L) {
    char buf[1024];
    int n = dse_ui_get_dropdown_value(Ent(L,1), buf, sizeof(buf));
    lua_pushlstring(L, buf, n);
    return 1;
}

int L_UiSetDropdownOpen(lua_State* L) {
    dse_ui_set_dropdown_open(Ent(L,1), lua_toboolean(L,2) ? 1 : 0);
    return 0;
}

// UIFilledImageComponent
int L_UiAddFilledImage(lua_State* L) {
    int clockwise = (lua_gettop(L) >= 5) ? (lua_toboolean(L,5) ? 1 : 0) : 1;
    dse_ui_add_filled_image(Ent(L,1), OptF(L,2,1.0f), OptI(L,3,0), OptI(L,4,0), clockwise);
    return 0;
}

int L_UiSetFillAmount(lua_State* L) {
    dse_ui_set_fill_amount(Ent(L,1), F(L,2));
    return 0;
}

int L_UiGetFillAmount(lua_State* L) {
    lua_pushnumber(L, dse_ui_get_fill_amount(Ent(L,1)));
    return 1;
}

int L_UiSetFillMethod(lua_State* L) {
    int m = static_cast<int>(luaL_checkinteger(L,2));
    int o = lua_isnoneornil(L,3) ? -1 : static_cast<int>(luaL_checkinteger(L,3));
    int cw = lua_isnoneornil(L,4) ? -1 : (lua_toboolean(L,4) ? 1 : 0);
    dse_ui_set_fill_method(Ent(L,1), m, o, cw);
    return 0;
}

// UIFocusNavigableComponent
int L_UiAddFocusNavigable(lua_State* L) {
    dse_ui_add_focus_navigable(Ent(L,1), OptI(L,2,0));
    return 0;
}

int L_UiSetFocusNav(lua_State* L) {
    uint32_t up = 0, down = 0, left = 0, right = 0;
    if (!lua_isnoneornil(L,2)) up = static_cast<uint32_t>(luaL_checkinteger(L,2));
    if (!lua_isnoneornil(L,3)) down = static_cast<uint32_t>(luaL_checkinteger(L,3));
    if (!lua_isnoneornil(L,4)) left = static_cast<uint32_t>(luaL_checkinteger(L,4));
    if (!lua_isnoneornil(L,5)) right = static_cast<uint32_t>(luaL_checkinteger(L,5));
    dse_ui_set_focus_nav(Ent(L,1), up, down, left, right);
    return 0;
}

int L_UiIsFocused(lua_State* L) {
    lua_pushboolean(L, dse_ui_is_focused(Ent(L,1)));
    return 1;
}

int L_UiSetFocusTint(lua_State* L) {
    dse_ui_set_focus_tint(Ent(L,1), F(L,2), F(L,3), F(L,4), OptF(L,5,1.0f));
    return 0;
}

// UI 序列化
int L_UiLoadFromJson(lua_State* L) {
    const char* json = luaL_checkstring(L,1);
    uint32_t entities[1024];
    int n = dse_ui_load_from_json(json, entities, 1024);
    lua_newtable(L);
    for (int i = 0; i < n; ++i) {
        lua_pushinteger(L, static_cast<lua_Integer>(entities[i]));
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

int L_UiLoadFromFile(lua_State* L) {
    const char* path = luaL_checkstring(L,1);
    uint32_t entities[1024];
    int n = dse_ui_load_from_file(path, entities, 1024);
    lua_newtable(L);
    for (int i = 0; i < n; ++i) {
        lua_pushinteger(L, static_cast<lua_Integer>(entities[i]));
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// UIEventPropagationComponent
int L_UiAddEventPropagation(lua_State* L) {
    int click = (lua_gettop(L) >= 2) ? (lua_toboolean(L,2) ? 1 : 0) : 1;
    int hover = (lua_gettop(L) >= 3) ? (lua_toboolean(L,3) ? 1 : 0) : 0;
    dse_ui_add_event_propagation(Ent(L,1), click, hover);
    return 0;
}

int L_UiStopPropagation(lua_State* L) {
    dse_ui_stop_propagation(Ent(L,1));
    return 0;
}

// UIVisualEffectComponent
int L_UiAddVisualEffect(lua_State* L) {
    dse_ui_add_visual_effect(Ent(L,1));
    return 0;
}

int L_UiSetCornerRadius(lua_State* L) {
    dse_ui_set_corner_radius(Ent(L,1), F(L,2));
    return 0;
}

int L_UiSetGradient(lua_State* L) {
    dse_ui_set_gradient(Ent(L,1),
        F(L,2), F(L,3), F(L,4), OptF(L,5,1.0f),
        F(L,6), F(L,7), F(L,8), OptF(L,9,1.0f),
        OptI(L,10,1));
    return 0;
}

int L_UiSetBlur(lua_State* L) {
    dse_ui_set_blur(Ent(L,1), F(L,2), OptF(L,3,1.0f));
    return 0;
}

// UIVirtualScrollComponent
int L_UiAddVirtualScroll(lua_State* L) {
    dse_ui_add_virtual_scroll(Ent(L,1), static_cast<int>(luaL_checkinteger(L,2)), OptF(L,3,50.0f));
    return 0;
}

int L_UiSetVirtualScrollCount(lua_State* L) {
    dse_ui_set_virtual_scroll_count(Ent(L,1), static_cast<int>(luaL_checkinteger(L,2)));
    return 0;
}

int L_UiDestroyVirtualScroll(lua_State* L) {
    dse_ui_destroy_virtual_scroll(Ent(L,1));
    return 0;
}

int L_UiGetVirtualScrollRange(lua_State* L) {
    int start, end;
    dse_ui_get_virtual_scroll_range(Ent(L,1), &start, &end);
    lua_pushinteger(L, start);
    lua_pushinteger(L, end);
    return 2;
}

} // namespace

void RegisterUiBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };

    lua_newtable(L);
    set_fn("add_renderer", L_UiAddRenderer);
    set_fn("add_label", L_UiAddLabel);
    set_fn("add_ttf_label", L_UiAddTtfLabel);
    set_fn("set_label_text", L_UiSetLabelText);
    set_fn("set_label_font", L_UiSetLabelFont);
    set_fn("set_label_layout", L_UiSetLabelLayout);
    set_fn("set_label_number", L_UiSetLabelNumber);
    set_fn("add_panel", L_UiAddPanel);
    set_fn("add_button", L_UiAddButton);
    set_fn("set_button_scale", L_UiSetButtonScale);
    set_fn("add_mask", L_UiAddMask);
    set_fn("add_rich_text", L_UiAddRichText);
    set_fn("set_rich_text", L_UiSetRichText);
    set_fn("add_joystick", L_UiAddJoystick);
    set_fn("get_joystick_x", L_UiGetJoystickX);
    set_fn("get_joystick_y", L_UiGetJoystickY);
    set_fn("set_position", L_UiSetPosition);
    set_fn("set_size", L_UiSetSize);
    set_fn("set_anchor", L_UiSetAnchor);
    set_fn("set_color", L_UiSetColor);
    set_fn("set_visible", L_UiSetVisible);
    set_fn("set_uv", L_UiSetUv);
    set_fn("set_nine_slice", L_UiSetNineSlice);
    // UIAnchorComponent
    set_fn("add_anchor", L_UiAddAnchor);
    set_fn("set_anchor_type", L_UiSetAnchorType);
    set_fn("set_anchor_offset", L_UiSetAnchorOffset);
    // UIGridLayoutComponent
    set_fn("add_grid_layout", L_UiAddGridLayout);
    set_fn("set_grid_layout", L_UiSetGridLayout);
    // UICanvasScalerComponent
    set_fn("add_canvas_scaler", L_UiAddCanvasScaler);
    set_fn("set_canvas_scaler", L_UiSetCanvasScaler);
    // UIBoxLayoutComponent
    set_fn("add_box_layout", L_UiAddBoxLayout);
    set_fn("set_box_layout", L_UiSetBoxLayout);
    // UIContentSizeFitterComponent
    set_fn("add_content_size_fitter", L_UiAddContentSizeFitter);
    // UIAnimationComponent
    set_fn("add_ui_animation", L_UiAddAnimation);
    set_fn("animate_position", L_UiAnimatePosition);
    set_fn("animate_scale", L_UiAnimateScale);
    set_fn("animate_alpha", L_UiAnimateAlpha);
    set_fn("animate_color", L_UiAnimateColor);
    set_fn("stop_ui_animation", L_UiStopAnimation);
    // UITextInputComponent
    set_fn("add_text_input", L_UiAddTextInput);
    set_fn("set_text_input_text", L_UiSetTextInputText);
    set_fn("get_text_input_text", L_UiGetTextInputText);
    set_fn("set_text_input_placeholder", L_UiSetTextInputPlaceholder);
    set_fn("set_text_input_focus", L_UiSetTextInputFocus);
    // UIScrollViewComponent
    set_fn("add_scroll_view", L_UiAddScrollView);
    set_fn("set_scroll_offset", L_UiSetScrollOffset);
    set_fn("get_scroll_offset", L_UiGetScrollOffset);
    set_fn("set_scroll_content_size", L_UiSetScrollContentSize);
    // UISliderComponent
    set_fn("add_slider", L_UiAddSlider);
    set_fn("set_slider_value", L_UiSetSliderValue);
    set_fn("get_slider_value", L_UiGetSliderValue);
    set_fn("set_slider_colors", L_UiSetSliderColors);
    set_fn("set_slider_handle_size", L_UiSetSliderHandleSize);
    set_fn("set_slider_vertical", L_UiSetSliderVertical);
    set_fn("set_slider_range", L_UiSetSliderRange);
    // UIToggleComponent
    set_fn("add_toggle", L_UiAddToggle);
    set_fn("set_toggle", L_UiSetToggle);
    set_fn("get_toggle", L_UiGetToggle);
    // UIProgressBarComponent
    set_fn("add_progress_bar", L_UiAddProgressBar);
    set_fn("set_progress", L_UiSetProgress);
    set_fn("get_progress", L_UiGetProgress);
    // UIDropdownComponent
    set_fn("add_dropdown", L_UiAddDropdown);
    set_fn("dropdown_add_option", L_UiDropdownAddOption);
    set_fn("dropdown_clear_options", L_UiDropdownClearOptions);
    set_fn("set_dropdown_index", L_UiSetDropdownIndex);
    set_fn("get_dropdown_index", L_UiGetDropdownIndex);
    set_fn("get_dropdown_value", L_UiGetDropdownValue);
    set_fn("set_dropdown_open", L_UiSetDropdownOpen);
    // UIFilledImageComponent
    set_fn("add_filled_image", L_UiAddFilledImage);
    set_fn("set_fill_amount", L_UiSetFillAmount);
    set_fn("get_fill_amount", L_UiGetFillAmount);
    set_fn("set_fill_method", L_UiSetFillMethod);
    // UIFocusNavigableComponent
    set_fn("add_focus_navigable", L_UiAddFocusNavigable);
    set_fn("set_focus_nav", L_UiSetFocusNav);
    set_fn("is_focused", L_UiIsFocused);
    set_fn("set_focus_tint", L_UiSetFocusTint);
    // UI 序列化
    set_fn("load_from_json", L_UiLoadFromJson);
    set_fn("load_from_file", L_UiLoadFromFile);
    // UIEventPropagationComponent
    set_fn("add_event_propagation", L_UiAddEventPropagation);
    set_fn("stop_propagation", L_UiStopPropagation);
    // UIVisualEffectComponent
    set_fn("add_visual_effect", L_UiAddVisualEffect);
    set_fn("set_corner_radius", L_UiSetCornerRadius);
    set_fn("set_gradient", L_UiSetGradient);
    set_fn("set_blur", L_UiSetBlur);
    // UIVirtualScrollComponent
    set_fn("add_virtual_scroll", L_UiAddVirtualScroll);
    set_fn("set_virtual_scroll_count", L_UiSetVirtualScrollCount);
    set_fn("get_virtual_scroll_range", L_UiGetVirtualScrollRange);
    set_fn("destroy_virtual_scroll", L_UiDestroyVirtualScroll);
}

}
