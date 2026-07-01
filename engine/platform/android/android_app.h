/**
 * @file android_app.h
 * @brief Android 平台实现 — PlatformApp 的 ANativeActivity/ANativeWindow 具现
 *
 * 仅在 __ANDROID__ 下编译。
 * 生命周期由 ANativeActivity 回调驱动，调用方须在 Init() 前注入 ANativeWindow*。
 * 支持 AppState 生命周期状态机（与 HarmonyApp 对齐）。
 */

#pragma once
#ifdef __ANDROID__

#include "engine/platform/platform_app.h"
#include "engine/platform/shared/egl_helper.h"

#include <android/native_window.h>
#include <android/input.h>
#include <android/looper.h>

#include <atomic>
#include <chrono>

struct AAssetManager;

namespace dse::platform {

class AndroidApp final : public PlatformApp {
public:
    /// 应用生命周期状态（前后台 + Surface 有效性）
    enum class AppState {
        Active,            // Surface 有效，前台渲染中
        Paused,            // Surface 有效，后台暂停帧循环
        SurfaceLost,       // Surface 已销毁，等待重建
        PausedNoSurface,   // 后台 + Surface 已销毁
        Destroyed          // 已关闭
    };

    AndroidApp() = default;
    ~AndroidApp() override;

    // ─── Android 特定注入 ───────────────────────────────────────
    // 必须在 Init() 之前调用
    void SetNativeWindow(ANativeWindow* window) { native_window_ = window; }
    void SetAssetManager(AAssetManager* mgr)    { asset_manager_ = mgr; }
    void SetInputQueue(AInputQueue* queue);

    // ─── 生命周期回调（ANativeActivity 回调层调用）────────
    void OnPause();                                   // Activity::onPause
    void OnResume();                                  // Activity::onResume
    void OnSurfaceLost();                             // SurfaceHolder::surfaceDestroyed
    void OnSurfaceRegained(ANativeWindow* window);    // SurfaceHolder::surfaceCreated
    void OnSurfaceChanged(int width, int height);     // SurfaceHolder::surfaceChanged

    AppState GetAppState() const;
    bool IsRenderable() const;

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

    // EGL state（GLES 模式）— 使用共享 EGLState 结构
    EGLState egl_{};

    std::atomic<AppState> state_{AppState::Destroyed};
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
