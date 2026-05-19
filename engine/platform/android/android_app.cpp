/**
 * @file android_app.cpp
 * @brief AndroidApp 实现 — ANativeActivity + EGL + AInputQueue
 *
 * 仅在 __ANDROID__ 下编译（PC 构建中此文件被 CMake 过滤排除）。
 */

#ifdef __ANDROID__

#include "engine/platform/android/android_app.h"
#include "engine/base/debug.h"

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
    egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display_ == EGL_NO_DISPLAY) {
        ALOGE("eglGetDisplay failed");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(egl_display_, &major, &minor)) {
        ALOGE("eglInitialize failed");
        return false;
    }

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint num_cfg = 0;
    if (!eglChooseConfig(egl_display_, attribs, &cfg, 1, &num_cfg) || num_cfg == 0) {
        ALOGE("eglChooseConfig failed");
        return false;
    }

    // 窗口格式与 EGL config 对齐
    EGLint format;
    eglGetConfigAttrib(egl_display_, cfg, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(native_window_, 0, 0, format);

    egl_surface_ = eglCreateWindowSurface(egl_display_, cfg,
                                          static_cast<EGLNativeWindowType>(native_window_),
                                          nullptr);
    if (egl_surface_ == EGL_NO_SURFACE) {
        ALOGE("eglCreateWindowSurface failed");
        return false;
    }

    const EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    egl_context_ = eglCreateContext(egl_display_, cfg, EGL_NO_CONTEXT, ctx_attribs);
    if (egl_context_ == EGL_NO_CONTEXT) {
        ALOGE("eglCreateContext failed");
        return false;
    }

    if (!eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_)) {
        ALOGE("eglMakeCurrent failed");
        return false;
    }

    has_gl_context_ = true;
    ALOGI("EGL context created (OpenGL ES 3)");
    return true;
}

void AndroidApp::DestroyEGL() {
    if (egl_display_ == EGL_NO_DISPLAY) return;
    eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (egl_context_ != EGL_NO_CONTEXT) { eglDestroyContext(egl_display_, egl_context_); egl_context_ = EGL_NO_CONTEXT; }
    if (egl_surface_ != EGL_NO_SURFACE) { eglDestroySurface(egl_display_, egl_surface_); egl_surface_ = EGL_NO_SURFACE; }
    eglTerminate(egl_display_);
    egl_display_ = EGL_NO_DISPLAY;
    has_gl_context_ = false;
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
        const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        const float x = AMotionEvent_getX(event, 0);
        const float y = AMotionEvent_getY(event, 0);

        if (cursor_pos_cb_) cursor_pos_cb_(x, y);

        if (mouse_btn_cb_) {
            int btn_action = 0;
            if (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_MOVE)
                btn_action = 1;
            else if (action == AMOTION_EVENT_ACTION_UP)
                btn_action = 0;
            mouse_btn_cb_(0, btn_action);  // 左键 = touch
        }
    } else if (type == AINPUT_EVENT_TYPE_KEY) {
        const int32_t action  = AKeyEvent_getAction(event);
        const int32_t keycode = AKeyEvent_getKeyCode(event);
        if (key_cb_) key_cb_(keycode, (action == AKEY_EVENT_ACTION_DOWN) ? 1 : 0);
    }
}

void AndroidApp::SwapBuffers() {
    if (egl_display_ != EGL_NO_DISPLAY && egl_surface_ != EGL_NO_SURFACE) {
        eglSwapBuffers(egl_display_, egl_surface_);
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
    return has_gl_context_;
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
