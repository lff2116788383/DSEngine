/**
 * @file android_app.h
 * @brief Android 平台实现 — PlatformApp 的 ANativeActivity/ANativeWindow 具现
 *
 * 仅在 __ANDROID__ 下编译。
 * 生命周期由 ANativeActivity 回调驱动，调用方须在 Init() 前注入 ANativeWindow*。
 */

#pragma once
#ifdef __ANDROID__

#include "engine/platform/platform_app.h"

#include <android/native_window.h>
#include <android/input.h>
#include <android/looper.h>
#include <EGL/egl.h>

#include <atomic>
#include <chrono>

struct AAssetManager;

namespace dse::platform {

class AndroidApp final : public PlatformApp {
public:
    AndroidApp() = default;
    ~AndroidApp() override;

    // ─── Android 特定注入 ───────────────────────────────────────
    // 必须在 Init() 之前调用
    void SetNativeWindow(ANativeWindow* window) { native_window_ = window; }
    void SetAssetManager(AAssetManager* mgr)    { asset_manager_ = mgr; }
    void SetInputQueue(AInputQueue* queue);

    // ─── PlatformApp 接口 ───────────────────────────────────────

    bool Init(const WindowConfig& config) override;
    void Shutdown() override;

    bool   ShouldClose() const override;
    void   PollEvents() override;
    void   SwapBuffers() override;
    double GetTime() const override;

    void GetFramebufferSize(int& w, int& h) const override;
    void SetWindowTitle(const std::string& title) override;
    void RequestClose() override;

    void*    GetNativeWindowHandle() const override;
    bool     HasGLContext() const override;
    bool     LoadGLFunctions() override;
    uint64_t CreateVulkanSurface(void* vk_instance) override;

    void SetInputCallbacks(KeyCallback, MouseButtonCallback,
                           ScrollCallback, CursorPosCallback) override;

    void SetTouchCallback(TouchCallback) override;

    bool AttachExternal(void* existing_window) override;

private:
    ANativeWindow* native_window_        = nullptr;
    AAssetManager* asset_manager_        = nullptr;
    AInputQueue*   input_queue_          = nullptr;

    // EGL state（GLES 模式）
    EGLDisplay egl_display_ = EGL_NO_DISPLAY;
    EGLContext egl_context_ = EGL_NO_CONTEXT;
    EGLSurface egl_surface_ = EGL_NO_SURFACE;

    bool has_gl_context_  = false;
    bool initialized_     = false;
    std::atomic<bool> should_close_{false};
    std::chrono::steady_clock::time_point start_time_;

    KeyCallback         key_cb_       = nullptr;
    MouseButtonCallback mouse_btn_cb_ = nullptr;
    ScrollCallback      scroll_cb_    = nullptr;
    CursorPosCallback   cursor_pos_cb_= nullptr;
    TouchCallback       touch_cb_     = nullptr;

    bool InitEGL(const WindowConfig& config);
    void DestroyEGL();
    void ProcessInputEvent(AInputEvent* event);
};

} // namespace dse::platform

#endif // __ANDROID__
