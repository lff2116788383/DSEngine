/**
 * @file ios_app.h
 * @brief UIKitApp — iOS 平台 PlatformApp 实现（UIKit + MoltenVK）
 *
 * 持有 UIWindow + DSEViewController（含 MTKView）。
 * MTKView.layer 为 CAMetalLayer，供 MoltenVK 创建 VkSurfaceKHR。
 * 引擎帧循环由引擎自身驱动（MTKView.paused=YES），不使用 MTKView 的 drawInMTKView 回调。
 */

#ifndef DSE_PLATFORM_IOS_IOS_APP_H
#define DSE_PLATFORM_IOS_IOS_APP_H

#ifdef DSE_ENABLE_APPLE_PLATFORM

#include "engine/platform/platform_app.h"

namespace dse::platform {

class UIKitApp final : public PlatformApp {
public:
    ~UIKitApp() override;

    // --- 生命周期 ---
    bool Init(const WindowConfig& config) override;
    void Shutdown() override;

    // --- 主循环驱动 ---
    bool ShouldClose() const override;
    void PollEvents() override;
    void SwapBuffers() override;         // MoltenVK swapchain 处理，no-op
    double GetTime() const override;     // CACurrentMediaTime()

    // --- 窗口信息 ---
    void GetFramebufferSize(int& w, int& h) const override;
    void SetWindowTitle(const std::string& title) override; // iOS 无标题栏，no-op
    void RequestClose() override;
    void Show() override;               // iOS 全屏应用，Init 后即可见，no-op

    // --- 渲染 Context 线程管理 ---
    void MakeContextCurrent() override;  // Vulkan 模式，no-op
    void ReleaseContext() override;      // Vulkan 模式，no-op

    // --- 平台桥接 ---
    void* GetNativeWindowHandle() const override; // 返回 CAMetalLayer*
    bool HasGLContext() const override;            // false（Vulkan 模式）
    bool LoadGLFunctions() override;               // false

    // --- Vulkan Surface ---
    uint64_t CreateVulkanSurface(void* vk_instance) override;

    // --- 输入回调 ---
    void SetInputCallbacks(KeyCallback, MouseButtonCallback,
                           ScrollCallback, CursorPosCallback) override;
    void SetTouchCallback(TouchCallback cb) override;

    // --- 编辑器外部窗口注入 ---
    bool AttachExternal(void* existing_window) override;

private:
    void* ui_window_ = nullptr;        // UIWindow* (ARC bridge)
    void* view_controller_ = nullptr;  // DSEViewController*
    void* metal_view_ = nullptr;       // MTKView*
    TouchCallback touch_cb_ = nullptr;
    bool should_close_ = false;
    bool initialized_ = false;
};

/// iOS 平台工厂函数（替代桌面 GLFW 实现）
std::unique_ptr<PlatformApp> CreateDefaultPlatformApp();

} // namespace dse::platform

#endif // DSE_ENABLE_APPLE_PLATFORM
#endif // DSE_PLATFORM_IOS_IOS_APP_H
