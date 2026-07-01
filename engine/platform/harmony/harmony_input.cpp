/**
 * @file harmony_input.cpp
 * @brief XComponent 触屏回调 — 统一使用 dse::input::TouchPhase 枚举
 *
 * 仅在 __OHOS__ 下编译。
 * OH_NATIVEXCOMPONENT_DOWN/MOVE/UP/CANCEL 映射到引擎 TouchPhase 枚举。
 */

#ifdef __OHOS__

#include "engine/platform/harmony/harmony_app.h"
#include "engine/input/touch.h"

#include <ace/xcomponent/native_interface_xcomponent.h>

namespace dse::platform {

void HarmonyDispatchTouchEvent(HarmonyApp* app,
                                OH_NativeXComponent* component,
                                void* window) {
    OH_NativeXComponent_TouchEvent touch_event;
    if (OH_NativeXComponent_GetTouchEvent(component, window, &touch_event)
        != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }

    auto cursor_pos_cb = app->GetCursorPosCallback();
    auto mouse_btn_cb  = app->GetMouseBtnCallback();
    auto touch_cb      = app->GetTouchCallback();

    // 主触点驱动鼠标兼容接口（与 Android 后端一致）
    if (touch_event.numPoints > 0) {
        auto& p0 = touch_event.touchPoints[0];
        if (cursor_pos_cb) cursor_pos_cb(p0.x, p0.y);
        if (mouse_btn_cb) {
            int btn_action = 0;
            if (p0.type == OH_NATIVEXCOMPONENT_DOWN ||
                p0.type == OH_NATIVEXCOMPONENT_MOVE)
                btn_action = 1;
            else if (p0.type == OH_NATIVEXCOMPONENT_UP)
                btn_action = 0;
            mouse_btn_cb(0, btn_action);
        }
    }

    if (!touch_cb) return;

    for (int i = 0; i < touch_event.numPoints; ++i) {
        auto& point = touch_event.touchPoints[i];
        int phase = 0;
        switch (point.type) {
            case OH_NATIVEXCOMPONENT_DOWN:
                phase = static_cast<int>(dse::input::TouchPhase::Began);
                break;
            case OH_NATIVEXCOMPONENT_MOVE:
                phase = static_cast<int>(dse::input::TouchPhase::Moved);
                break;
            case OH_NATIVEXCOMPONENT_UP:
                phase = static_cast<int>(dse::input::TouchPhase::Ended);
                break;
            case OH_NATIVEXCOMPONENT_CANCEL:
                phase = static_cast<int>(dse::input::TouchPhase::Cancelled);
                break;
            default:
                continue;
        }
        touch_cb(point.id, point.x, point.y, phase);
    }
}

} // namespace dse::platform

#endif // __OHOS__
