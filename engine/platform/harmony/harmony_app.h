/**
 * @file harmony_app.h
 * @brief HarmonyOS 平台实现 — PlatformApp 的 OHNativeWindow/XComponent 具现
 *
 * 仅在 __OHOS__ 下编译。
 * 生命周期由 XComponent + UIAbility 回调双轨驱动。
 * 支持 Vulkan（VK_OHOS_surface）和 GLES3（EGL on OHOS）双路径。
 */

#pragma once
#ifdef __OHOS__

#include "engine/platform/platform_app.h"
#include "engine/platform/shared/egl_helper.h"

#include <native_window/external_window.h>

#include <atomic>
#include <chrono>

struct NativeResourceManager;

namespace dse::platform {

class HarmonyApp final : public PlatformApp {
public:
    /// 应用生命周期状态（前后台 + Surface 有效性）
    enum class AppState {
        Active,            // Surface 有效，前台渲染中
        Paused,            // Surface 有效，后台暂停帧循环
        SurfaceLost,       // Surface 已销毁，等待重建
        PausedNoSurface,   // 后台 + Surface 已销毁
        Destroyed          // 已关闭
    };

    HarmonyApp() = default;
    ~HarmonyApp() override;

    // ─── OHOS 特有注入（Init 前调用） ───────────────────────────
    void SetNativeWindow(OHNativeWindow* window);
    void SetResourceManager(NativeResourceManager* mgr);

    // ─── 生命周期回调（NAPI 桥接层调用） ────────────────────────
    void OnPause();                                  // UIAbility::onBackground
    void OnResume();                                 // UIAbility::onForeground
    void OnSurfaceLost();                            // XComponent::OnSurfaceDestroyed
    void OnSurfaceRegained(OHNativeWindow* window);  // XComponent::OnSurfaceCreated
    void OnSurfaceChanged(int width, int height);    // 旋转/分辨率变化
    void OnMemoryLevel(int level);                   // 内存压力回调

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
    void SetTouchCallback(TouchCallback cb) override;

    bool AttachExternal(void* existing_window) override;

    // ─── 触屏回调访问（供 harmony_input.cpp 使用） ──────────────
    TouchCallback GetTouchCallback() const { return touch_cb_; }
    CursorPosCallback GetCursorPosCallback() const { return cursor_pos_cb_; }
    MouseButtonCallback GetMouseBtnCallback() const { return mouse_btn_cb_; }

private:
    OHNativeWindow* native_window_        = nullptr;
    NativeResourceManager* resource_mgr_  = nullptr;

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
};

} // namespace dse::platform

#endif // __OHOS__
