/**
 * @file web_app.h
 * @brief Web (Emscripten/WebAssembly) 平台实现 — PlatformApp 的浏览器端具现
 *
 * 渲染走 WebGL2（= OpenGL ES 3.0），窗口/输入复用 Emscripten 的 GLFW3 移植
 * （`-sUSE_GLFW=3`）。主循环由 apps/web_host 通过 emscripten_set_main_loop 驱动，
 * 故 ShouldClose 恒为 false。触屏事件映射为鼠标左键（对齐 Android 行为）。
 */

#ifndef DSE_PLATFORM_WEB_APP_H
#define DSE_PLATFORM_WEB_APP_H

#include "engine/platform/platform_app.h"

struct GLFWwindow;

namespace dse::platform {

class WebApp final : public PlatformApp {
public:
    WebApp() = default;
    ~WebApp() override;

    // --- 生命周期 ---
    bool Init(const WindowConfig& config) override;
    void Shutdown() override;

    // --- 主循环驱动（Web 由 emscripten_set_main_loop 驱动） ---
    bool ShouldClose() const override;
    void PollEvents() override;
    void SwapBuffers() override;
    double GetTime() const override;

    // --- 窗口信息 ---
    void GetFramebufferSize(int& w, int& h) const override;
    void SetWindowTitle(const std::string& title) override;
    void RequestClose() override;

    // --- 平台桥接 ---
    void* GetNativeWindowHandle() const override;
    bool HasGLContext() const override;
    bool LoadGLFunctions() override;

    // --- Vulkan Surface（Web 无 Vulkan） ---
    uint64_t CreateVulkanSurface(void* vk_instance) override;

    // --- 输入回调 ---
    void SetInputCallbacks(KeyCallback, MouseButtonCallback,
                           ScrollCallback, CursorPosCallback) override;

    // --- 编辑器外部窗口注入（Web 不支持） ---
    bool AttachExternal(void* existing_window) override;

    // --- GL Context 线程管理 ---
    void MakeContextCurrent() override;
    void ReleaseContext() override;

    // 输入回调中继（GLFW 回调签名固定，通过静态转发到用户回调）
    // public：触屏事件由文件内的自由函数 OnTouch 转发到这些回调。
    static KeyCallback s_key_cb_;
    static MouseButtonCallback s_mouse_btn_cb_;
    static ScrollCallback s_scroll_cb_;
    static CursorPosCallback s_cursor_pos_cb_;
    static double s_dpr_;  // devicePixelRatio; scales input coords to drawing-buffer space

private:
    GLFWwindow* window_ = nullptr;
    bool has_gl_context_ = false;
    bool initialized_ = false;

    static void GlfwKeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void GlfwMouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void GlfwScrollCallback(GLFWwindow* w, double xoffset, double yoffset);
    static void GlfwCursorPosCallback(GLFWwindow* w, double xpos, double ypos);

    void InstallTouchHandlers();
};

} // namespace dse::platform

#endif // DSE_PLATFORM_WEB_APP_H
