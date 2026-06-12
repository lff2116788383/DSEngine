/**
 * @file glfw_app.h
 * @brief GLFW 平台实现 — PlatformApp 的桌面端（Windows/Linux/macOS）具现
 */

#ifndef DSE_PLATFORM_GLFW_APP_H
#define DSE_PLATFORM_GLFW_APP_H

#include "engine/platform/platform_app.h"

struct GLFWwindow;

namespace dse::platform {

class GlfwApp final : public PlatformApp {
public:
    GlfwApp() = default;
    ~GlfwApp() override;

    // --- 生命周期 ---
    bool Init(const WindowConfig& config) override;
    void Shutdown() override;

    // --- 主循环驱动 ---
    bool ShouldClose() const override;
    void PollEvents() override;
    void SwapBuffers() override;
    double GetTime() const override;

    // --- 窗口信息 ---
    void GetFramebufferSize(int& w, int& h) const override;
    void SetWindowTitle(const std::string& title) override;
    void RequestClose() override;
    void Show() override;

    // --- 平台桥接 ---
    void* GetNativeWindowHandle() const override;
    bool HasGLContext() const override;
    bool LoadGLFunctions() override;

    // --- Vulkan Surface ---
    uint64_t CreateVulkanSurface(void* vk_instance) override;

    // --- 输入回调 ---
    void SetInputCallbacks(KeyCallback, MouseButtonCallback,
                           ScrollCallback, CursorPosCallback) override;

    // --- 编辑器外部窗口注入 ---
    bool AttachExternal(void* existing_window) override;

    // --- GL Context 线程管理 ---
    void MakeContextCurrent() override;
    void ReleaseContext() override;

private:
    GLFWwindow* window_ = nullptr;
    bool owns_window_ = false;  // false = 编辑器模式 (AttachExternal)
    bool has_gl_context_ = false;
    bool initialized_ = false;

    // 输入回调中继（GLFW 回调签名固定，通过静态转发到用户回调）
    static KeyCallback s_key_cb_;
    static MouseButtonCallback s_mouse_btn_cb_;
    static ScrollCallback s_scroll_cb_;
    static CursorPosCallback s_cursor_pos_cb_;

    static void GlfwKeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void GlfwMouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void GlfwScrollCallback(GLFWwindow* w, double xoffset, double yoffset);
    static void GlfwCursorPosCallback(GLFWwindow* w, double xpos, double ypos);
};

} // namespace dse::platform

#endif // DSE_PLATFORM_GLFW_APP_H
