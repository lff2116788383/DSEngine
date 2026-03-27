/**
 * @file lua_binding_core.cpp
 * @brief Lua 脚本绑定与运行时管理，处理 C++ 与 Lua 的交互边界
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"
#include "engine/base/time.h"
#include "engine/input/input.h"
#include "engine/input/key_code.h"
#include "engine/platform/screen.h"
extern "C" {
#include <lauxlib.h>
}

namespace dse::runtime::lua_binding {
namespace {
int L_AssetsLoadTexture(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    auto texture = GetAssetManager().LoadTexture(path);
    lua_pushinteger(L, texture ? static_cast<lua_Integer>(texture->GetHandle()) : 0);
    return 1;
}

int L_AppSetDataRoot(lua_State* L) {
    const char* data_root = luaL_checkstring(L, 1);
    auto& asset_manager = GetAssetManager();
    asset_manager.ConfigureDataRoot(data_root);
    DEBUG_LOG_INFO("Data root updated from lua: {}", asset_manager.GetDataRoot());
    return 0;
}

int L_AppSetWindowTitle(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    const auto& setter = GetBindingContext().set_window_title;
    if (setter) {
        setter(title);
    }
    return 0;
}

int L_AppGetMouseX(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(Input::mousePosition().x));
    return 1;
}

int L_AppGetMouseY(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(Input::mousePosition().y));
    return 1;
}

int L_AppGetMouseLeft(lua_State* L) {
    lua_pushboolean(L, Input::GetMouseButton(MOUSE_BUTTON_LEFT));
    return 1;
}

int L_AppGetMouseLeftDown(lua_State* L) {
    lua_pushboolean(L, Input::GetMouseButtonDown(MOUSE_BUTTON_LEFT));
    return 1;
}

int L_AppGetMouseLeftUp(lua_State* L) {
    lua_pushboolean(L, Input::GetMouseButtonUp(MOUSE_BUTTON_LEFT));
    return 1;
}

int L_AppGetMouseRight(lua_State* L) {
    lua_pushboolean(L, Input::GetMouseButton(MOUSE_BUTTON_RIGHT));
    return 1;
}

int L_AppGetMouseRightDown(lua_State* L) {
    lua_pushboolean(L, Input::GetMouseButtonDown(MOUSE_BUTTON_RIGHT));
    return 1;
}

int L_AppGetMouseRightUp(lua_State* L) {
    lua_pushboolean(L, Input::GetMouseButtonUp(MOUSE_BUTTON_RIGHT));
    return 1;
}

/**
 * @brief Lua 绑定：获取鼠标左键当前帧是否触发了双击
 * @param L Lua 状态机
 * @return 压入一个布尔值，表示是否双击
 * @example if app.get_mouse_left_double_click() then print("double click") end
 */
int L_AppGetMouseLeftDoubleClick(lua_State* L) {
    lua_pushboolean(L, Input::GetDoubleClick(MOUSE_BUTTON_LEFT));
    return 1;
}

/**
 * @brief Lua 绑定：获取鼠标左键是否长按超过指定时间
 * @param L Lua 状态机，参数 1 可选为长按判定时间（默认 0.5 秒）
 * @return 压入一个布尔值，表示是否处于长按状态
 * @example if app.get_mouse_left_long_press(1.0) then print("long press") end
 */
int L_AppGetMouseLeftLongPress(lua_State* L) {
    float duration = static_cast<float>(luaL_optnumber(L, 1, 0.5));
    lua_pushboolean(L, Input::GetLongPress(MOUSE_BUTTON_LEFT, duration));
    return 1;
}

/**
 * @brief Lua 绑定：获取当前帧滑动/拖拽在 X 轴上的增量
 * @param L Lua 状态机
 * @return 压入一个数字，表示滑动 X 轴增量（像素）
 * @example local dx = app.get_mouse_swipe_dx()
 */
int L_AppGetMouseSwipeDeltaX(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(Input::GetSwipeDelta().x));
    return 1;
}

/**
 * @brief Lua 绑定：获取当前帧滑动/拖拽在 Y 轴上的增量
 * @param L Lua 状态机
 * @return 压入一个数字，表示滑动 Y 轴增量（像素）
 * @example local dy = app.get_mouse_swipe_dy()
 */
int L_AppGetMouseSwipeDeltaY(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(Input::GetSwipeDelta().y));
    return 1;
}

/**
 * @brief Lua 绑定：检测设备是否处于摇晃状态
 * @param L Lua 状态机
 * @return 压入一个布尔值，表示是否在摇晃
 * @example if app.get_device_shake() then print("shake") end
 */
int L_AppGetShake(lua_State* L) {
    lua_pushboolean(L, Input::IsDeviceShaking());
    return 1;
}

int L_AppGetTimeSinceStartup(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(Time::TimeSinceStartup()));
    return 1;
}

int L_AppGetScreenWidth(lua_State* L) {
    lua_pushinteger(L, Screen::width());
    return 1;
}

int L_AppGetScreenHeight(lua_State* L) {
    lua_pushinteger(L, Screen::height());
    return 1;
}

int L_MetricsGetDrawCalls(lua_State* L) {
    const auto& fn = GetBindingContext().get_draw_calls;
    lua_pushinteger(L, fn ? fn() : 0);
    return 1;
}

int L_MetricsGetMaxBatchSprites(lua_State* L) {
    const auto& fn = GetBindingContext().get_max_batch_sprites;
    lua_pushinteger(L, fn ? fn() : 0);
    return 1;
}

int L_MetricsGetSpriteCount(lua_State* L) {
    const auto& fn = GetBindingContext().get_sprite_count;
    lua_pushinteger(L, fn ? fn() : 0);
    return 1;
}
}

void RegisterAssetsBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };
    lua_newtable(L);
    set_fn("load_texture", L_AssetsLoadTexture);
}

void RegisterAppBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };
    lua_newtable(L);
    set_fn("set_data_root", L_AppSetDataRoot);
    set_fn("set_window_title", L_AppSetWindowTitle);
    set_fn("get_mouse_x", L_AppGetMouseX);
    set_fn("get_mouse_y", L_AppGetMouseY);
    set_fn("get_mouse_left", L_AppGetMouseLeft);
    set_fn("get_mouse_left_down", L_AppGetMouseLeftDown);
    set_fn("get_mouse_left_up", L_AppGetMouseLeftUp);
    set_fn("get_mouse_right", L_AppGetMouseRight);
    set_fn("get_mouse_right_down", L_AppGetMouseRightDown);
    set_fn("get_mouse_right_up", L_AppGetMouseRightUp);
    set_fn("get_mouse_left_double_click", L_AppGetMouseLeftDoubleClick);
    set_fn("get_mouse_left_long_press", L_AppGetMouseLeftLongPress);
    set_fn("get_mouse_swipe_dx", L_AppGetMouseSwipeDeltaX);
    set_fn("get_mouse_swipe_dy", L_AppGetMouseSwipeDeltaY);
    set_fn("get_device_shake", L_AppGetShake);
    set_fn("time_since_startup", L_AppGetTimeSinceStartup);
    set_fn("get_screen_width", L_AppGetScreenWidth);
    set_fn("get_screen_height", L_AppGetScreenHeight);
}

void RegisterMetricsBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };
    lua_newtable(L);
    set_fn("get_draw_calls", L_MetricsGetDrawCalls);
    set_fn("get_max_batch_sprites", L_MetricsGetMaxBatchSprites);
    set_fn("get_sprite_count", L_MetricsGetSpriteCount);
}

}
