/**
 * @file lua_binding_core.cpp
 * @brief Lua 脚本绑定与运行时管理，处理 C++ 与 Lua 的交互边界。薄包装委托至 C ABI。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

// 与 engine/input/key_code.h 的 MOUSE_BUTTON_* 对应
constexpr int kMouseButtonLeft = 0;
constexpr int kMouseButtonRight = 1;
constexpr int kMouseButtonMiddle = 2;

int L_AssetsLoadTexture(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(dse_assets_load_texture(path)));
    return 1;
}

int L_AppSetDataRoot(lua_State* L) {
    dse_assets_set_data_root(luaL_checkstring(L, 1));
    return 0;
}

int L_AppSetWindowTitle(lua_State* L) {
    dse_app_set_window_title(luaL_checkstring(L, 1));
    return 0;
}

int L_AppGetMouseX(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_input_get_mouse_x()));
    return 1;
}

int L_AppGetMouseY(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_input_get_mouse_y()));
    return 1;
}

int L_AppGetMouseLeft(lua_State* L) {
    lua_pushboolean(L, dse_input_get_mouse_button(kMouseButtonLeft));
    return 1;
}

int L_AppGetMouseLeftDown(lua_State* L) {
    lua_pushboolean(L, dse_input_get_mouse_button_down(kMouseButtonLeft));
    return 1;
}

int L_AppGetMouseLeftUp(lua_State* L) {
    lua_pushboolean(L, dse_input_get_mouse_button_up(kMouseButtonLeft));
    return 1;
}

int L_AppGetMouseRight(lua_State* L) {
    lua_pushboolean(L, dse_input_get_mouse_button(kMouseButtonRight));
    return 1;
}

int L_AppGetMouseRightDown(lua_State* L) {
    lua_pushboolean(L, dse_input_get_mouse_button_down(kMouseButtonRight));
    return 1;
}

int L_AppGetMouseRightUp(lua_State* L) {
    lua_pushboolean(L, dse_input_get_mouse_button_up(kMouseButtonRight));
    return 1;
}

int L_AppGetKey(lua_State* L) {
    lua_pushboolean(L, dse_input_get_key(static_cast<int>(luaL_checkinteger(L, 1))));
    return 1;
}

int L_AppGetKeyDown(lua_State* L) {
    lua_pushboolean(L, dse_input_get_key_down(static_cast<int>(luaL_checkinteger(L, 1))));
    return 1;
}

int L_AppGetKeyUp(lua_State* L) {
    lua_pushboolean(L, dse_input_get_key_up(static_cast<int>(luaL_checkinteger(L, 1))));
    return 1;
}

/**
 * @brief Lua 绑定：获取鼠标左键当前帧是否触发了双击
 * @example if app.get_mouse_left_double_click() then print("double click") end
 */
int L_AppGetMouseLeftDoubleClick(lua_State* L) {
    lua_pushboolean(L, dse_input_get_mouse_left_double_click());
    return 1;
}

/**
 * @brief Lua 绑定：获取鼠标左键是否长按超过指定时间（默认 0.5 秒）
 * @example if app.get_mouse_left_long_press(1.0) then print("long press") end
 */
int L_AppGetMouseLeftLongPress(lua_State* L) {
    float duration = static_cast<float>(luaL_optnumber(L, 1, 0.5));
    lua_pushboolean(L, dse_input_get_mouse_left_long_press(duration));
    return 1;
}

/**
 * @brief Lua 绑定：获取当前帧滑动/拖拽在 X 轴上的增量（像素）
 * @example local dx = app.get_mouse_swipe_dx()
 */
int L_AppGetMouseSwipeDeltaX(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_input_get_mouse_swipe_dx()));
    return 1;
}

/**
 * @brief Lua 绑定：获取当前帧滑动/拖拽在 Y 轴上的增量（像素）
 * @example local dy = app.get_mouse_swipe_dy()
 */
int L_AppGetMouseSwipeDeltaY(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_input_get_mouse_swipe_dy()));
    return 1;
}

/**
 * @brief Lua 绑定：检测设备是否处于摇晃状态
 * @example if app.get_device_shake() then print("shake") end
 */
int L_AppGetShake(lua_State* L) {
    lua_pushboolean(L, dse_input_get_device_shake());
    return 1;
}

int L_AppGetTimeSinceStartup(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_app_get_time_since_startup()));
    return 1;
}

int L_AppGetScreenWidth(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(dse_input_get_screen_width()));
    return 1;
}

int L_AppGetScreenHeight(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(dse_input_get_screen_height()));
    return 1;
}

int L_AppQuit(lua_State*) {
    dse_app_quit();
    return 0;
}

int L_AppSetTargetFps(lua_State* L) {
    dse_app_set_target_fps(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

// app.set_time_scale(s)：全局时间缩放（0=暂停, 1=正常, 0.5=半速, >1=快进），s 钳制为 >=0
int L_AppSetTimeScale(lua_State* L) {
    dse_app_set_time_scale(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

// app.get_time_scale() -> number：当前全局时间缩放
int L_AppGetTimeScale(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_app_get_time_scale()));
    return 1;
}

int L_AppGetMouseMiddle(lua_State* L) {
    lua_pushboolean(L, dse_input_get_mouse_middle());
    return 1;
}

int L_AppGetMouseMiddleDown(lua_State* L) {
    lua_pushboolean(L, dse_input_get_mouse_middle_down());
    return 1;
}

int L_AppGetMouseScrollDx(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_input_get_mouse_scroll_dx()));
    return 1;
}

int L_AppGetMouseScrollDy(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_input_get_mouse_scroll_dy()));
    return 1;
}

// app.get_gamepad_axis([gamepad_id=0], axis) -> number  （已应用死区）
int L_AppGetGamepadAxis(lua_State* L) {
    int gamepad_id = 0;
    int axis = 0;
    if (lua_gettop(L) >= 2) {
        gamepad_id = static_cast<int>(luaL_checkinteger(L, 1));
        axis = static_cast<int>(luaL_checkinteger(L, 2));
    } else {
        axis = static_cast<int>(luaL_checkinteger(L, 1));
    }
    lua_pushnumber(L, static_cast<lua_Number>(dse_input_get_gamepad_axis(gamepad_id, axis)));
    return 1;
}

// app.is_gamepad_connected([gamepad_id=0]) -> bool
int L_AppIsGamepadConnected(lua_State* L) {
    int gamepad_id = static_cast<int>(luaL_optinteger(L, 1, 0));
    lua_pushboolean(L, dse_input_is_gamepad_connected(gamepad_id));
    return 1;
}

// app.set_gamepad_dead_zone(dead_zone)
int L_AppSetGamepadDeadZone(lua_State* L) {
    dse_input_set_gamepad_dead_zone(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

// app.get_gamepad_dead_zone() -> number
int L_AppGetGamepadDeadZone(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_input_get_gamepad_dead_zone()));
    return 1;
}

int L_MetricsGetFps(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_metrics_get_fps()));
    return 1;
}

int L_MetricsGetFrameTimeMs(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_metrics_get_frame_time_ms()));
    return 1;
}

int L_MetricsGetDrawCalls(lua_State* L) {
    lua_pushinteger(L, dse_metrics_get_draw_calls());
    return 1;
}

int L_MetricsGetMaxBatchSprites(lua_State* L) {
    lua_pushinteger(L, dse_metrics_get_max_batch_sprites());
    return 1;
}

int L_MetricsGetSpriteCount(lua_State* L) {
    lua_pushinteger(L, dse_metrics_get_sprite_count());
    return 1;
}

int L_MetricsGetGpuDrivenActive(lua_State* L) {
    lua_pushboolean(L, dse_metrics_get_gpu_driven_active());
    return 1;
}

int L_MetricsGetGpuIndirectDrawCount(lua_State* L) {
    lua_pushinteger(L, dse_metrics_get_gpu_indirect_draw_count());
    return 1;
}

int L_MetricsGetGpuTotalInstances(lua_State* L) {
    lua_pushinteger(L, dse_metrics_get_gpu_total_instances());
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
    set_fn("get_key", L_AppGetKey);
    set_fn("get_key_down", L_AppGetKeyDown);
    set_fn("get_key_up", L_AppGetKeyUp);
    set_fn("get_mouse_left_double_click", L_AppGetMouseLeftDoubleClick);

    set_fn("get_mouse_left_long_press", L_AppGetMouseLeftLongPress);
    set_fn("get_mouse_swipe_dx", L_AppGetMouseSwipeDeltaX);
    set_fn("get_mouse_swipe_dy", L_AppGetMouseSwipeDeltaY);
    set_fn("get_device_shake", L_AppGetShake);
    set_fn("time_since_startup", L_AppGetTimeSinceStartup);
    set_fn("get_screen_width", L_AppGetScreenWidth);
    set_fn("get_screen_height", L_AppGetScreenHeight);
    set_fn("quit", L_AppQuit);
    set_fn("set_target_fps", L_AppSetTargetFps);
    set_fn("set_time_scale", L_AppSetTimeScale);
    set_fn("get_time_scale", L_AppGetTimeScale);
    set_fn("get_mouse_middle", L_AppGetMouseMiddle);
    set_fn("get_mouse_middle_down", L_AppGetMouseMiddleDown);
    set_fn("get_mouse_scroll_dx", L_AppGetMouseScrollDx);
    set_fn("get_mouse_scroll_dy", L_AppGetMouseScrollDy);
    set_fn("get_gamepad_axis", L_AppGetGamepadAxis);
    set_fn("is_gamepad_connected", L_AppIsGamepadConnected);
    set_fn("set_gamepad_dead_zone", L_AppSetGamepadDeadZone);
    set_fn("get_gamepad_dead_zone", L_AppGetGamepadDeadZone);
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
    set_fn("get_gpu_driven_active", L_MetricsGetGpuDrivenActive);
    set_fn("get_gpu_indirect_draw_count", L_MetricsGetGpuIndirectDrawCount);
    set_fn("get_gpu_total_instances", L_MetricsGetGpuTotalInstances);
    set_fn("get_fps", L_MetricsGetFps);
    set_fn("get_frame_time_ms", L_MetricsGetFrameTimeMs);
}

// ============================================================
// Floating Origin Lua API
// ============================================================

namespace {

// origin.get_accumulated() -> x, y, z
int L_OriginGetAccumulated(lua_State* L) {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    dse_origin_get_accumulated(&x, &y, &z);
    lua_pushnumber(L, static_cast<lua_Number>(x));
    lua_pushnumber(L, static_cast<lua_Number>(y));
    lua_pushnumber(L, static_cast<lua_Number>(z));
    return 3;
}

// origin.to_absolute(lx, ly, lz) -> ax, ay, az
int L_OriginToAbsolute(lua_State* L) {
    float lx = static_cast<float>(luaL_checknumber(L, 1));
    float ly = static_cast<float>(luaL_checknumber(L, 2));
    float lz = static_cast<float>(luaL_checknumber(L, 3));
    float x = lx, y = ly, z = lz;
    dse_origin_to_absolute(lx, ly, lz, &x, &y, &z);
    lua_pushnumber(L, static_cast<lua_Number>(x));
    lua_pushnumber(L, static_cast<lua_Number>(y));
    lua_pushnumber(L, static_cast<lua_Number>(z));
    return 3;
}

// origin.to_local(ax, ay, az) -> lx, ly, lz
int L_OriginToLocal(lua_State* L) {
    float ax = static_cast<float>(luaL_checknumber(L, 1));
    float ay = static_cast<float>(luaL_checknumber(L, 2));
    float az = static_cast<float>(luaL_checknumber(L, 3));
    float x = ax, y = ay, z = az;
    dse_origin_to_local(ax, ay, az, &x, &y, &z);
    lua_pushnumber(L, static_cast<lua_Number>(x));
    lua_pushnumber(L, static_cast<lua_Number>(y));
    lua_pushnumber(L, static_cast<lua_Number>(z));
    return 3;
}

// origin.set_rebase_threshold(threshold)
int L_OriginSetRebaseThreshold(lua_State* L) {
    dse_origin_set_rebase_threshold(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

// origin.get_rebase_threshold() -> threshold
int L_OriginGetRebaseThreshold(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(dse_origin_get_rebase_threshold()));
    return 1;
}

} // namespace

void RegisterFloatingOriginBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };
    lua_newtable(L);
    set_fn("get_accumulated", L_OriginGetAccumulated);
    set_fn("to_absolute", L_OriginToAbsolute);
    set_fn("to_local", L_OriginToLocal);
    set_fn("set_rebase_threshold", L_OriginSetRebaseThreshold);
    set_fn("get_rebase_threshold", L_OriginGetRebaseThreshold);
}

}
