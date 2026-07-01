/**
 * @file glfw_app.cpp
 * @brief GlfwApp 实现 — 封装所有 GLFW 调用
 */

#define GLFW_INCLUDE_NONE
#include "engine/platform/glfw/glfw_app.h"
#include "engine/base/debug.h"
#include "engine/input/key_code.h"        // GAMEPAD_BUTTON_* / GamepadAxis / KeyAction
#include "engine/render/rhi/ubo_types.h"  // kMaxUBOLights

#ifdef DSE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#endif
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if !TARGET_OS_IOS
      #define GLFW_EXPOSE_NATIVE_COCOA
      #include <GLFW/glfw3native.h>
    #endif
  #endif
#endif

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <stb/stb_image.h>

namespace dse::platform {

// --- 静态回调中继 ---
PlatformApp::KeyCallback         GlfwApp::s_key_cb_       = nullptr;
PlatformApp::MouseButtonCallback GlfwApp::s_mouse_btn_cb_ = nullptr;
PlatformApp::ScrollCallback      GlfwApp::s_scroll_cb_    = nullptr;
PlatformApp::CursorPosCallback   GlfwApp::s_cursor_pos_cb_= nullptr;

void GlfwApp::GlfwKeyCallback(GLFWwindow*, int key, int /*scancode*/, int action, int /*mods*/) {
    if (s_key_cb_) s_key_cb_(key, action);
}

void GlfwApp::GlfwMouseButtonCallback(GLFWwindow*, int button, int action, int /*mods*/) {
    if (s_mouse_btn_cb_) s_mouse_btn_cb_(button, action);
}

void GlfwApp::GlfwScrollCallback(GLFWwindow*, double /*xoffset*/, double yoffset) {
    if (s_scroll_cb_) s_scroll_cb_(static_cast<float>(yoffset));
}

void GlfwApp::GlfwCursorPosCallback(GLFWwindow*, double xpos, double ypos) {
    if (s_cursor_pos_cb_) s_cursor_pos_cb_(static_cast<float>(xpos), static_cast<float>(ypos));
}

GlfwApp::~GlfwApp() {
    Shutdown();
}

bool GlfwApp::Init(const WindowConfig& config) {
    if (initialized_) return true;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    const bool needs_gl = !config.no_graphics_api;
    if (needs_gl) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    } else {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    // splash 期间隐藏窗口：首帧渲染完成后再由上层调用 Show()，避免空白窗口闪烁。
    glfwWindowHint(GLFW_VISIBLE, config.start_hidden ? GLFW_FALSE : GLFW_TRUE);

    window_ = glfwCreateWindow(config.width, config.height,
                               config.title.c_str(), nullptr, nullptr);
    if (!window_ && needs_gl && config.gl_fallback_33) {
        fprintf(stderr, "[DSEngine] GL 4.3 unavailable, falling back to GL 3.3 (UBO light mode, max %d lights)\n",
                dse::render::kMaxUBOLights);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        window_ = glfwCreateWindow(config.width, config.height,
                                   config.title.c_str(), nullptr, nullptr);
    }
    if (!window_) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    if (needs_gl) {
        glfwMakeContextCurrent(window_);
        const char* vsync_env = std::getenv("DSE_VSYNC");
        int swap_interval = (vsync_env && std::string(vsync_env) == "0") ? 0 : 1;
        glfwSwapInterval(swap_interval);
        has_gl_context_ = true;
    }

    // 设置窗口图标（从 exe 所在目录向上查找 data/icon/dse_icon.png）
    {
        // 尝试常见路径：exe旁 → exe/../data → 工程根/data
        std::filesystem::path exe_dir;
#if defined(_WIN32)
        wchar_t module_buf[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, module_buf, MAX_PATH))
            exe_dir = std::filesystem::path(module_buf).parent_path();
#endif
        const char* candidates[] = {
            "data/icon/dse_icon.png",
            "../data/icon/dse_icon.png",
            "../../data/icon/dse_icon.png",
        };
        for (auto rel : candidates) {
            std::filesystem::path icon_path = exe_dir.empty()
                ? std::filesystem::path(rel)
                : exe_dir / rel;
            int iw, ih, ic;
            unsigned char* px = stbi_load(icon_path.string().c_str(), &iw, &ih, &ic, 4);
            if (px) {
                GLFWimage img{ iw, ih, px };
                glfwSetWindowIcon(window_, 1, &img);
                stbi_image_free(px);
                break;
            }
        }
    }

    // 显式设置箭头光标，避免 Windows 在加载期间显示忙碌光标
    GLFWcursor* arrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    if (arrow) {
        glfwSetCursor(window_, arrow);
    }

    owns_window_ = true;
    initialized_ = true;
    return true;
}

void GlfwApp::Shutdown() {
    if (!initialized_) return;

    if (owns_window_ && window_) {
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
    owns_window_ = false;
    has_gl_context_ = false;
    initialized_ = false;
}

bool GlfwApp::ShouldClose() const {
    return window_ ? glfwWindowShouldClose(window_) != 0 : true;
}

void GlfwApp::PollEvents() {
    glfwPollEvents();
}

void GlfwApp::SwapBuffers() {
    if (glfwGetCurrentContext() != nullptr) {
        glfwSwapBuffers(window_);
    }
}

double GlfwApp::GetTime() const {
    return glfwGetTime();
}

void GlfwApp::GetFramebufferSize(int& w, int& h) const {
    if (window_) {
        glfwGetFramebufferSize(window_, &w, &h);
    }
}

void GlfwApp::SetWindowTitle(const std::string& title) {
    if (window_) {
        glfwSetWindowTitle(window_, title.c_str());
    }
}

void GlfwApp::RequestClose() {
    if (window_) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

void GlfwApp::Show() {
    if (window_) {
        glfwShowWindow(window_);
    }
}

void* GlfwApp::GetNativeWindowHandle() const {
#ifdef _WIN32
    return window_ ? (void*)glfwGetWin32Window(window_) : nullptr;
#elif defined(DSE_ENABLE_APPLE_PLATFORM) && defined(__APPLE__) && !TARGET_OS_IOS
    return window_ ? (void*)glfwGetCocoaWindow(window_) : nullptr;
#else
    return nullptr;
#endif
}

bool GlfwApp::HasGLContext() const {
    return has_gl_context_;
}

bool GlfwApp::LoadGLFunctions() {
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize OpenGL (glad)\n";
        return false;
    }

    // 立即清黑屏并 swap，消除初始化期间的白屏
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (window_) glfwSwapBuffers(window_);

    // 打印 GPU 信息
    const char* gl_renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* gl_vendor   = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* gl_version  = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    DEBUG_LOG_INFO("OpenGL GPU: {} | Vendor: {} | Version: {}",
                   gl_renderer ? gl_renderer : "unknown",
                   gl_vendor   ? gl_vendor   : "unknown",
                   gl_version  ? gl_version  : "unknown");
    if (FILE* f = fopen("gpu_info.txt", "w")) {
        fprintf(f, "Renderer: %s\nVendor: %s\nVersion: %s\n",
                gl_renderer ? gl_renderer : "unknown",
                gl_vendor   ? gl_vendor   : "unknown",
                gl_version  ? gl_version  : "unknown");
        fclose(f);
    }
    return true;
}

uint64_t GlfwApp::CreateVulkanSurface(void* vk_instance) {
#ifdef DSE_ENABLE_VULKAN
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = glfwCreateWindowSurface(
        static_cast<VkInstance>(vk_instance), window_, nullptr, &surface);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[GlfwApp] glfwCreateWindowSurface failed: {}", static_cast<int>(result));
        return 0;
    }
    return reinterpret_cast<uint64_t>(surface);
#else
    (void)vk_instance;
    return 0;
#endif
}

void GlfwApp::SetInputCallbacks(KeyCallback key_cb, MouseButtonCallback mouse_btn_cb,
                                ScrollCallback scroll_cb, CursorPosCallback cursor_pos_cb) {
    s_key_cb_       = key_cb;
    s_mouse_btn_cb_ = mouse_btn_cb;
    s_scroll_cb_    = scroll_cb;
    s_cursor_pos_cb_= cursor_pos_cb;

    if (window_) {
        glfwSetKeyCallback(window_, GlfwKeyCallback);
        glfwSetMouseButtonCallback(window_, GlfwMouseButtonCallback);
        glfwSetScrollCallback(window_, GlfwScrollCallback);
        glfwSetCursorPosCallback(window_, GlfwCursorPosCallback);
    }
}

// GLFW 标准手柄按钮索引 → engine/input 键码。
// 引擎枚举把 GUIDE 排在最后 (414)，与 GLFW 的索引 8 不同，故需显式映射表而非加偏移。
static constexpr unsigned short kGlfwButtonToKeyCode[15] = {  // GLFW_GAMEPAD_BUTTON_LAST + 1
    GAMEPAD_BUTTON_A,             // 0  GLFW_GAMEPAD_BUTTON_A
    GAMEPAD_BUTTON_B,             // 1  B
    GAMEPAD_BUTTON_X,             // 2  X
    GAMEPAD_BUTTON_Y,             // 3  Y
    GAMEPAD_BUTTON_LEFT_BUMPER,   // 4  LEFT_BUMPER
    GAMEPAD_BUTTON_RIGHT_BUMPER,  // 5  RIGHT_BUMPER
    GAMEPAD_BUTTON_BACK,          // 6  BACK
    GAMEPAD_BUTTON_START,         // 7  START
    GAMEPAD_BUTTON_GUIDE,         // 8  GUIDE
    GAMEPAD_BUTTON_LEFT_THUMB,    // 9  LEFT_THUMB
    GAMEPAD_BUTTON_RIGHT_THUMB,   // 10 RIGHT_THUMB
    GAMEPAD_BUTTON_DPAD_UP,       // 11 DPAD_UP
    GAMEPAD_BUTTON_DPAD_RIGHT,    // 12 DPAD_RIGHT
    GAMEPAD_BUTTON_DPAD_DOWN,     // 13 DPAD_DOWN
    GAMEPAD_BUTTON_DPAD_LEFT,     // 14 DPAD_LEFT
};

void GlfwApp::SetGamepadCallbacks(GamepadButtonCallback btn_cb, GamepadAxisCallback axis_cb,
                                  GamepadConnectionCallback conn_cb) {
    gamepad_btn_cb_  = btn_cb;
    gamepad_axis_cb_ = axis_cb;
    gamepad_conn_cb_ = conn_cb;
}

void GlfwApp::PollGamepads() {
    static_assert(sizeof(kGlfwButtonToKeyCode) / sizeof(kGlfwButtonToKeyCode[0]) ==
                      kGamepadButtonCount,
                  "GLFW 手柄按钮映射表长度须与 kGamepadButtonCount 一致");
    for (int gid = 0; gid < kMaxGamepads; ++gid) {
        const int jid = GLFW_JOYSTICK_1 + gid;
        const bool is_gamepad = glfwJoystickIsGamepad(jid) == GLFW_TRUE;

        if (is_gamepad != gamepad_prev_connected_[gid]) {
            gamepad_prev_connected_[gid] = is_gamepad;
            if (gamepad_conn_cb_) gamepad_conn_cb_(gid, is_gamepad);
            // 断开瞬间清零摇杆并释放仍按住的按钮，避免状态卡死。
            if (!is_gamepad) {
                if (gamepad_axis_cb_) {
                    for (int a = 0; a <= GAMEPAD_AXIS_LAST; ++a) gamepad_axis_cb_(gid, a, 0.0f);
                }
                for (int b = 0; b < kGamepadButtonCount; ++b) {
                    if (gamepad_prev_buttons_[gid][b]) {
                        gamepad_prev_buttons_[gid][b] = 0;
                        if (gamepad_btn_cb_) gamepad_btn_cb_(gid, kGlfwButtonToKeyCode[b], KEY_ACTION_UP);
                    }
                }
            }
        }

        if (!is_gamepad) continue;

        GLFWgamepadstate state;
        if (!glfwGetGamepadState(jid, &state)) continue;

        if (gamepad_axis_cb_) {
            for (int a = 0; a <= GAMEPAD_AXIS_LAST; ++a) {
                gamepad_axis_cb_(gid, a, state.axes[a]);
            }
        }
        // 仅在跳变沿上报，保持与键盘事件一致的 Down/Up 语义。
        for (int b = 0; b < kGamepadButtonCount; ++b) {
            const unsigned char pressed = state.buttons[b];
            if (pressed != gamepad_prev_buttons_[gid][b]) {
                gamepad_prev_buttons_[gid][b] = pressed;
                if (gamepad_btn_cb_) {
                    gamepad_btn_cb_(gid, kGlfwButtonToKeyCode[b],
                                    pressed ? KEY_ACTION_DOWN : KEY_ACTION_UP);
                }
            }
        }
    }
}

bool GlfwApp::AttachExternal(void* existing_window) {
    if (!initialized_) {
        // 编辑器模式可能传 nullptr（外部 GL context 已由编辑器创建）
        if (!glfwInit()) return false;
    }

    auto* glfw_win = static_cast<GLFWwindow*>(existing_window);
    window_ = glfw_win;  // 允许 nullptr（编辑器自管理窗口）
    owns_window_ = false;
    has_gl_context_ = (glfwGetCurrentContext() != nullptr);
    initialized_ = true;
    return true;
}

void GlfwApp::MakeContextCurrent() {
    if (window_ && has_gl_context_) {
        glfwMakeContextCurrent(window_);
    }
}

void GlfwApp::ReleaseContext() {
    if (has_gl_context_) {
        glfwMakeContextCurrent(nullptr);
    }
}

// --- 工厂函数 ---
std::unique_ptr<PlatformApp> CreateDefaultPlatformApp() {
    return std::make_unique<GlfwApp>();
}

} // namespace dse::platform
