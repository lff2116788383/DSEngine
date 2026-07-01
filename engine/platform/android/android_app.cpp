/**
 * @file android_app.cpp
 * @brief AndroidApp 实现 — ANativeActivity + EGL + AInputQueue
 *
 * 仅在 __ANDROID__ 下编译（PC 构建中此文件被 CMake 过滤排除）。
 */

#ifdef __ANDROID__

#include "engine/platform/android/android_app.h"
#include "engine/base/debug.h"
#include "engine/input/touch.h"

#include <android/log.h>
#include <android/native_window.h>

#ifdef DSE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#endif

#include <cstring>

#define ALOG_TAG "DSEngine"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO,  ALOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, ALOG_TAG, __VA_ARGS__)

namespace dse::platform {

AndroidApp::~AndroidApp() {
    Shutdown();
}

// ─── 注入 ───────────────────────────────────────────────────────

void AndroidApp::SetInputQueue(AInputQueue* queue) {
    input_queue_ = queue;
    if (queue) {
        AInputQueue_attachLooper(queue, ALooper_forThread(), ALOOPER_POLL_CALLBACK,
                                 nullptr, nullptr);
    }
}

// ─── Init ───────────────────────────────────────────────────────

bool AndroidApp::Init(const WindowConfig& config) {
    if (!native_window_) {
        ALOGE("AndroidApp::Init: ANativeWindow not set before Init()");
        return false;
    }

    start_time_ = std::chrono::steady_clock::now();

    if (!config.no_graphics_api) {
        if (!InitEGL(config)) return false;
    }

    initialized_ = true;
    ALOGI("AndroidApp initialized (%dx%d)", ANativeWindow_getWidth(native_window_),
          ANativeWindow_getHeight(native_window_));
    return true;
}

bool AndroidApp::InitEGL(const WindowConfig& /*config*/) {
    if (!dse::platform::InitEGL(egl_, native_window_)) {
        ALOGE("EGL initialization failed");
        return false;
    }
    ALOGI("EGL context created (OpenGL ES 3)");
    return true;
}

void AndroidApp::DestroyEGL() {
    dse::platform::DestroyEGL(egl_);
}

// ─── Shutdown ──────────────────────────────────────────────────

void AndroidApp::Shutdown() {
    if (!initialized_) return;
    DestroyEGL();
    if (input_queue_) {
        AInputQueue_detachLooper(input_queue_);
        input_queue_ = nullptr;
    }
    initialized_ = false;
    ALOGI("AndroidApp shutdown");
}

// ─── 主循环 ───────────────────────────────────────────────────

bool AndroidApp::ShouldClose() const {
    return should_close_.load(std::memory_order_relaxed);
}

void AndroidApp::PollEvents() {
    if (!input_queue_) return;

    AInputEvent* event = nullptr;
    while (AInputQueue_getEvent(input_queue_, &event) >= 0) {
        if (AInputQueue_preDispatchEvent(input_queue_, event)) continue;
        ProcessInputEvent(event);
        AInputQueue_finishEvent(input_queue_, event, 1);
    }
}

void AndroidApp::ProcessInputEvent(AInputEvent* event) {
    const int32_t type = AInputEvent_getType(event);

    if (type == AINPUT_EVENT_TYPE_MOTION) {
        const int32_t raw_action = AMotionEvent_getAction(event);
        const int32_t action     = raw_action & AMOTION_EVENT_ACTION_MASK;
        const int32_t ptr_index  = (raw_action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                                   >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        // 主指针仍驱动鼠标兼容接口（保留既有单点行为）。
        const float x0 = AMotionEvent_getX(event, 0);
        const float y0 = AMotionEvent_getY(event, 0);
        if (cursor_pos_cb_) cursor_pos_cb_(x0, y0);
        if (mouse_btn_cb_) {
            int btn_action = 0;
            if (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_MOVE)
                btn_action = 1;
            else if (action == AMOTION_EVENT_ACTION_UP)
                btn_action = 0;
            mouse_btn_cb_(0, btn_action);  // 左键 = touch
        }

        // 多点触摸：喂入触摸抽象层。
        if (touch_cb_) {
            const size_t ptr_count = AMotionEvent_getPointerCount(event);
            switch (action) {
                case AMOTION_EVENT_ACTION_DOWN:
                case AMOTION_EVENT_ACTION_POINTER_DOWN: {
                    const int32_t id = AMotionEvent_getPointerId(event, ptr_index);
                    touch_cb_(id, AMotionEvent_getX(event, ptr_index),
                              AMotionEvent_getY(event, ptr_index),
                              static_cast<int>(dse::input::TouchPhase::Began));
                    break;
                }
                case AMOTION_EVENT_ACTION_MOVE: {
                    for (size_t i = 0; i < ptr_count; ++i) {
                        touch_cb_(AMotionEvent_getPointerId(event, i),
                                  AMotionEvent_getX(event, i),
                                  AMotionEvent_getY(event, i),
                                  static_cast<int>(dse::input::TouchPhase::Moved));
                    }
                    break;
                }
                case AMOTION_EVENT_ACTION_UP:
                case AMOTION_EVENT_ACTION_POINTER_UP: {
                    const int32_t id = AMotionEvent_getPointerId(event, ptr_index);
                    touch_cb_(id, AMotionEvent_getX(event, ptr_index),
                              AMotionEvent_getY(event, ptr_index),
                              static_cast<int>(dse::input::TouchPhase::Ended));
                    break;
                }
                case AMOTION_EVENT_ACTION_CANCEL: {
                    for (size_t i = 0; i < ptr_count; ++i) {
                        touch_cb_(AMotionEvent_getPointerId(event, i),
                                  AMotionEvent_getX(event, i),
                                  AMotionEvent_getY(event, i),
                                  static_cast<int>(dse::input::TouchPhase::Cancelled));
                    }
                    break;
                }
                default:
                    break;
            }
        }
    } else if (type == AINPUT_EVENT_TYPE_KEY) {
        const int32_t action  = AKeyEvent_getAction(event);
        const int32_t keycode = AKeyEvent_getKeyCode(event);
        if (key_cb_) key_cb_(keycode, (action == AKEY_EVENT_ACTION_DOWN) ? 1 : 0);
    }
}

void AndroidApp::SwapBuffers() {
    if (egl_.display != EGL_NO_DISPLAY && egl_.surface != EGL_NO_SURFACE) {
        eglSwapBuffers(egl_.display, egl_.surface);
    }
}

double AndroidApp::GetTime() const {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - start_time_).count();
}

// ─── 窗口信息 ─────────────────────────────────────────────────

void AndroidApp::GetFramebufferSize(int& w, int& h) const {
    if (native_window_) {
        w = ANativeWindow_getWidth(native_window_);
        h = ANativeWindow_getHeight(native_window_);
    } else {
        w = h = 0;
    }
}

void AndroidApp::SetWindowTitle(const std::string& /*title*/) {
    // Android 无窗口标题栏，忽略
}

void AndroidApp::RequestClose() {
    should_close_.store(true, std::memory_order_relaxed);
}

// ─── 平台桥接 ─────────────────────────────────────────────────

void* AndroidApp::GetNativeWindowHandle() const {
    return static_cast<void*>(native_window_);
}

bool AndroidApp::HasGLContext() const {
    return egl_.has_context;
}

bool AndroidApp::LoadGLFunctions() {
    // GLES 3 函数在 Android 上直接可用（动态链接 libGLESv3）
    return true;
}

uint64_t AndroidApp::CreateVulkanSurface(void* vk_instance) {
#ifdef DSE_ENABLE_VULKAN
    VkAndroidSurfaceCreateInfoKHR info{};
    info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    info.window = native_window_;

    auto fn = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
        vkGetInstanceProcAddr(static_cast<VkInstance>(vk_instance),
                              "vkCreateAndroidSurfaceKHR"));
    if (!fn) {
        ALOGE("vkCreateAndroidSurfaceKHR not available");
        return 0;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (fn(static_cast<VkInstance>(vk_instance), &info, nullptr, &surface) != VK_SUCCESS) {
        ALOGE("vkCreateAndroidSurfaceKHR failed");
        return 0;
    }
    return reinterpret_cast<uint64_t>(surface);
#else
    (void)vk_instance;
    return 0;
#endif
}

void AndroidApp::SetInputCallbacks(KeyCallback key, MouseButtonCallback mouse,
                                    ScrollCallback scroll, CursorPosCallback cursor) {
    key_cb_        = key;
    mouse_btn_cb_  = mouse;
    scroll_cb_     = scroll;
    cursor_pos_cb_ = cursor;
}

void AndroidApp::SetTouchCallback(TouchCallback touch) {
    touch_cb_ = touch;
}

bool AndroidApp::AttachExternal(void* existing_window) {
    native_window_ = static_cast<ANativeWindow*>(existing_window);
    return native_window_ != nullptr;
}

// ─── 工厂函数（Android 版） ────────────────────────────────────

std::unique_ptr<PlatformApp> CreateDefaultPlatformApp() {
    return std::make_unique<AndroidApp>();
}

} // namespace dse::platform

#endif // __ANDROID__
