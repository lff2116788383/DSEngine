/**
 * @file platform_app.h
 * @brief 平台应用抽象接口 — 窗口创建、事件泵送、GL 加载、Vulkan Surface 等
 *
 * EngineInstance 仅通过此接口与平台交互，不直接引用 GLFW / SDL / ANativeWindow。
 */

#ifndef DSE_PLATFORM_APP_H
#define DSE_PLATFORM_APP_H

#include <memory>
#include <string>
#include <cstdint>

namespace dse::platform {

struct WindowConfig {
    int width = 800;
    int height = 600;
    std::string title = "DSEngine";
    bool no_graphics_api = false;  // D3D11/Vulkan：不创建 GL context
    bool gl_fallback_33 = true;    // GL 4.3 失败时降级到 3.3
    bool start_hidden = false;     // 创建时隐藏窗口（splash 期间），首帧后再 Show()
};

class PlatformApp {
public:
    virtual ~PlatformApp() = default;

    // --- 生命周期 ---
    virtual bool Init(const WindowConfig& config) = 0;
    virtual void Shutdown() = 0;

    // --- 主循环驱动 ---
    virtual bool ShouldClose() const = 0;
    virtual void PollEvents() = 0;
    virtual void SwapBuffers() = 0;        // GL swap；非 GL no-op
    virtual double GetTime() const = 0;

    // --- 窗口信息 ---
    virtual void GetFramebufferSize(int& w, int& h) const = 0;
    virtual void SetWindowTitle(const std::string& title) = 0;
    virtual void RequestClose() = 0;
    virtual void Show() {}                  // 显示之前以 start_hidden 创建的窗口；非桌面端 no-op

    // --- 渲染 Context 线程管理 ---
    virtual void MakeContextCurrent() {}    // GL: glfwMakeContextCurrent(window); 非 GL no-op
    virtual void ReleaseContext() {}        // GL: glfwMakeContextCurrent(nullptr); 非 GL no-op

    // --- 平台桥接 ---
    virtual void* GetNativeWindowHandle() const = 0;  // HWND / X11 Window / ANativeWindow*
    virtual bool HasGLContext() const = 0;
    virtual bool LoadGLFunctions() = 0;

    // --- Vulkan Surface（避免 vulkan.h 头文件依赖，用 uint64_t 传递） ---
    virtual uint64_t CreateVulkanSurface(void* vk_instance) = 0;

    // --- 输入回调 ---
    using KeyCallback = void(*)(int key, int action);
    using MouseButtonCallback = void(*)(int button, int action);
    using ScrollCallback = void(*)(float yoffset);
    using CursorPosCallback = void(*)(float x, float y);
    virtual void SetInputCallbacks(KeyCallback, MouseButtonCallback,
                                   ScrollCallback, CursorPosCallback) = 0;

    // --- 手柄回调（事件驱动平台无原生手柄事件，需每帧 PollGamepads 主动轮询） ---
    // button 为 engine/input 的 GAMEPAD_BUTTON_* 键码；action 为 KeyAction（0 松 / 1 按）。
    using GamepadButtonCallback = void(*)(int gamepad_id, int button, int action);
    // axis 为 GamepadAxis；value 为原始 [-1,1]（死区由 Input 层过滤）。
    using GamepadAxisCallback = void(*)(int gamepad_id, int axis, float value);
    using GamepadConnectionCallback = void(*)(int gamepad_id, bool connected);
    virtual void SetGamepadCallbacks(GamepadButtonCallback, GamepadAxisCallback,
                                     GamepadConnectionCallback) {}
    /// 轮询所有手柄并通过回调上报状态变化；无手柄能力的平台为 no-op。
    virtual void PollGamepads() {}

    // --- 触摸回调 ---
    // finger_id 为触点标识；phase 取值对应 dse::input::TouchPhase 整型
    // (1=Began 2=Moved 3=Stationary 4=Ended 5=Cancelled)。
    using TouchCallback = void(*)(int finger_id, float x, float y, int phase);
    virtual void SetTouchCallback(TouchCallback) {}

    // --- 编辑器外部窗口注入 ---
    virtual bool AttachExternal(void* existing_window) = 0;
};

/// 创建当前平台的默认 PlatformApp 实现
std::unique_ptr<PlatformApp> CreateDefaultPlatformApp();

} // namespace dse::platform

#endif // DSE_PLATFORM_APP_H
