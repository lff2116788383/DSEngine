/**
 * @file web_app.cpp
 * @brief WebApp 实现 — 封装 Emscripten GLFW3 + WebGL2 上下文与触屏映射
 */

#define GLFW_INCLUDE_NONE
#include "engine/platform/web/web_app.h"

#include <GLFW/glfw3.h>
#include <GLES3/gl3.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace dse::platform {

// --- 静态回调中继 ---
PlatformApp::KeyCallback         WebApp::s_key_cb_       = nullptr;
PlatformApp::MouseButtonCallback WebApp::s_mouse_btn_cb_ = nullptr;
PlatformApp::ScrollCallback      WebApp::s_scroll_cb_    = nullptr;
PlatformApp::CursorPosCallback   WebApp::s_cursor_pos_cb_= nullptr;
double                           WebApp::s_dpr_          = 1.0;

void WebApp::GlfwKeyCallback(GLFWwindow*, int key, int, int action, int) {
    if (s_key_cb_) s_key_cb_(key, action);
}

void WebApp::GlfwMouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (s_mouse_btn_cb_) s_mouse_btn_cb_(button, action);
}

void WebApp::GlfwScrollCallback(GLFWwindow*, double, double yoffset) {
    if (s_scroll_cb_) s_scroll_cb_(static_cast<float>(yoffset));
}

void WebApp::GlfwCursorPosCallback(GLFWwindow*, double xpos, double ypos) {
    // Emscripten GLFW reports cursor in CSS pixels; scale to drawing-buffer
    // pixels so coords match the (DPI-aware) framebuffer / Screen size.
    if (s_cursor_pos_cb_) s_cursor_pos_cb_(static_cast<float>(xpos * s_dpr_),
                                           static_cast<float>(ypos * s_dpr_));
}

// --- Canvas size / DPI sync ---
// The canvas CSS size (shell.html sets it to 100vw x 100vh) follows the
// window, but the drawing buffer does not auto-track it. Set the drawing
// buffer to CSS size * devicePixelRatio so rendering is crisp on HiDPI.
// GetFramebufferSize reports the real canvas size, and the per-frame poll
// in EngineInstance::RunOneFrame turns a size change into OnWindowResize.
static const char* const kCanvasTarget = "#canvas";

static void UpdateCanvasSize() {
    double css_w = 0.0, css_h = 0.0;
    if (emscripten_get_element_css_size(kCanvasTarget, &css_w, &css_h) != EMSCRIPTEN_RESULT_SUCCESS) {
        return;
    }
    double dpr = emscripten_get_device_pixel_ratio();
    if (dpr <= 0.0) dpr = 1.0;
    WebApp::s_dpr_ = dpr;
    int fb_w = static_cast<int>(std::lround(css_w * dpr));
    int fb_h = static_cast<int>(std::lround(css_h * dpr));
    if (fb_w < 1) fb_w = 1;
    if (fb_h < 1) fb_h = 1;
    emscripten_set_canvas_element_size(kCanvasTarget, fb_w, fb_h);
}

static EM_BOOL OnCanvasResize(int /*event_type*/, const EmscriptenUiEvent* /*e*/, void* /*user*/) {
    UpdateCanvasSize();
    return EM_FALSE;
}

// --- 触屏 → 鼠标左键映射（对齐 Android：单指作为 button 0 + 光标位置） ---
static EM_BOOL OnTouch(int event_type, const EmscriptenTouchEvent* e, void*) {
    if (e->numTouches <= 0) return EM_FALSE;
    const EmscriptenTouchPoint& t = e->touches[0];
    if (WebApp::s_cursor_pos_cb_) {
        WebApp::s_cursor_pos_cb_(static_cast<float>(t.targetX * WebApp::s_dpr_),
                                 static_cast<float>(t.targetY * WebApp::s_dpr_));
    }
    if (WebApp::s_mouse_btn_cb_) {
        const int action = (event_type == EMSCRIPTEN_EVENT_TOUCHSTART) ? 1 /*press*/
                         : (event_type == EMSCRIPTEN_EVENT_TOUCHEND ||
                            event_type == EMSCRIPTEN_EVENT_TOUCHCANCEL) ? 0 /*release*/
                         : -1; /*move: no button change*/
        if (action >= 0) WebApp::s_mouse_btn_cb_(0 /*left*/, action);
    }
    return EM_TRUE;
}

WebApp::~WebApp() {
    Shutdown();
}

bool WebApp::Init(const WindowConfig& config) {
    if (initialized_) return true;

    if (!glfwInit()) {
        fprintf(stderr, "[WebApp] glfwInit failed\n");
        return false;
    }

    const bool needs_gl = !config.no_graphics_api;
    if (needs_gl) {
        // WebGL2 = OpenGL ES 3.0
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    } else {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    window_ = glfwCreateWindow(config.width, config.height,
                               config.title.c_str(), nullptr, nullptr);
    if (!window_) {
        fprintf(stderr, "[WebApp] glfwCreateWindow failed\n");
        glfwTerminate();
        return false;
    }

    if (needs_gl) {
        glfwMakeContextCurrent(window_);
        has_gl_context_ = true;
    }

    InstallTouchHandlers();

    // Canvas size / DPI: listen for window resize and do an initial sync.
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, OnCanvasResize);
    UpdateCanvasSize();

    initialized_ = true;
    return true;
}

void WebApp::InstallTouchHandlers() {
    // Emscripten 触屏事件挂到默认 canvas（"#canvas"）。
    emscripten_set_touchstart_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, OnTouch);
    emscripten_set_touchend_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, OnTouch);
    emscripten_set_touchmove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, OnTouch);
    emscripten_set_touchcancel_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, OnTouch);
}

void WebApp::Shutdown() {
    if (!initialized_) return;
    if (window_) {
        glfwSetKeyCallback(window_, nullptr);
        glfwSetMouseButtonCallback(window_, nullptr);
        glfwSetScrollCallback(window_, nullptr);
        glfwSetCursorPosCallback(window_, nullptr);
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
    s_key_cb_ = nullptr;
    s_mouse_btn_cb_ = nullptr;
    s_scroll_cb_ = nullptr;
    s_cursor_pos_cb_ = nullptr;
    window_ = nullptr;
    has_gl_context_ = false;
    initialized_ = false;
}

bool WebApp::ShouldClose() const {
    // Web：主循环由 emscripten_set_main_loop 驱动，窗口不可关闭。
    return false;
}

void WebApp::PollEvents() {
    glfwPollEvents();
}

void WebApp::SwapBuffers() {
    if (window_ && glfwGetCurrentContext() != nullptr) {
        glfwSwapBuffers(window_);
    }
}

double WebApp::GetTime() const {
    return glfwGetTime();
}

void WebApp::GetFramebufferSize(int& w, int& h) const {
    // Real drawing-buffer size (already includes devicePixelRatio),
    // matching what UpdateCanvasSize() applied.
    int cw = 0, ch = 0;
    emscripten_get_canvas_element_size(kCanvasTarget, &cw, &ch);
    if (cw > 0 && ch > 0) {
        w = cw;
        h = ch;
        return;
    }
    if (window_) {
        glfwGetFramebufferSize(window_, &w, &h);
    }
}

void WebApp::SetWindowTitle(const std::string& title) {
    if (window_) {
        glfwSetWindowTitle(window_, title.c_str());
    }
}

void WebApp::RequestClose() {
    // Web 无窗口关闭语义；no-op。
}

void* WebApp::GetNativeWindowHandle() const {
    return nullptr;
}

bool WebApp::HasGLContext() const {
    return has_gl_context_;
}

bool WebApp::LoadGLFunctions() {
    // WebGL2 函数由 Emscripten 静态链接提供，无需 glad 动态加载。
    return has_gl_context_;
}

uint64_t WebApp::CreateVulkanSurface(void* /*vk_instance*/) {
    return 0;  // Web 无 Vulkan
}

void WebApp::SetInputCallbacks(KeyCallback key_cb, MouseButtonCallback mb_cb,
                               ScrollCallback scroll_cb, CursorPosCallback cursor_cb) {
    s_key_cb_ = key_cb;
    s_mouse_btn_cb_ = mb_cb;
    s_scroll_cb_ = scroll_cb;
    s_cursor_pos_cb_ = cursor_cb;
    if (window_) {
        glfwSetKeyCallback(window_, GlfwKeyCallback);
        glfwSetMouseButtonCallback(window_, GlfwMouseButtonCallback);
        glfwSetScrollCallback(window_, GlfwScrollCallback);
        glfwSetCursorPosCallback(window_, GlfwCursorPosCallback);
    }
}

bool WebApp::AttachExternal(void* /*existing_window*/) {
    return false;  // Web 不支持编辑器外部窗口注入
}

void WebApp::MakeContextCurrent() {
    if (window_) glfwMakeContextCurrent(window_);
}

void WebApp::ReleaseContext() {
    glfwMakeContextCurrent(nullptr);
}

std::unique_ptr<PlatformApp> CreateDefaultPlatformApp() {
    return std::make_unique<WebApp>();
}

} // namespace dse::platform
