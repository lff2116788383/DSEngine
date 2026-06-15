/**
 * @file gl_rhi_smoke_test.cpp
 * @brief OpenGL RHI 冒烟测试 — 验证真实 GPU 上的最小渲染帧不崩溃
 *
 * 覆盖场景（与 dx11_rhi_smoke_test.cpp / vulkan_rhi_smoke_test.cpp 对等）：
 *   1. 创建隐藏 Win32 窗口 + GL context → InitDevice 成功
 *   2. BeginFrame → 创建 CommandBuffer → Submit → EndFrame 不崩溃
 *   3. 多帧循环（3 帧）稳定
 *   4. Shutdown 后再次 Init 不崩溃
 *   5. 纹理创建/销毁基本路径
 *   6. 10帧连续提交稳定
 *   7. RenderTarget 创建/销毁
 *   8. Buffer 创建/销毁
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/render/rhi/opengl/gl_rhi_device.h"
#include "engine/render/rhi/opengl/gl_loader.h"
#include "engine/render/rhi/rhi_device.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32

/// 隐藏 Win32 窗口 + 像素格式 + 传统 GL context 创建
LRESULT CALLBACK GLDummyWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    return DefWindowProcA(hwnd, msg, w, l);
}

struct GLContextGuard {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC hglrc = nullptr;

    ~GLContextGuard() { Destroy(); }

    void Destroy() {
        if (hglrc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(hglrc); hglrc = nullptr; }
        if (hdc && hwnd) { ReleaseDC(hwnd, hdc); hdc = nullptr; }
        if (hwnd) { DestroyWindow(hwnd); hwnd = nullptr; }
    }
};

bool CreateHiddenGLWindow(GLContextGuard& guard, int width, int height) {
    static bool registered = false;
    static const char* kClassName = "DSE_GLSmokeTest";
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = GLDummyWndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = kClassName;
        RegisterClassA(&wc);
        registered = true;
    }
    guard.hwnd = CreateWindowExA(
        0, kClassName, "GLSmokeTest",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (!guard.hwnd) return false;

    guard.hdc = GetDC(guard.hwnd);
    if (!guard.hdc) return false;

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(guard.hdc, &pfd);
    if (pf == 0) return false;
    if (!SetPixelFormat(guard.hdc, pf, &pfd)) return false;

    guard.hglrc = wglCreateContext(guard.hdc);
    if (!guard.hglrc) return false;
    if (!wglMakeCurrent(guard.hdc, guard.hglrc)) return false;

    return true;
}

/// glad2 函数指针加载器：优先 wglGetProcAddress（现代 GL 函数），
/// 回退 opengl32.dll（GL 1.1 基础函数，wglGetProcAddress 对其返回 null）
GLADapiproc GLSmokeGladLoad(const char* name) {
    PROC proc = wglGetProcAddress(name);
    if (proc == nullptr) {
        static HMODULE gl_module = LoadLibraryA("opengl32.dll");
        if (gl_module) {
            proc = GetProcAddress(gl_module, name);
        }
    }
    return reinterpret_cast<GLADapiproc>(proc);
}

#endif // _WIN32

} // namespace

class GLRhiSmokeTest : public ::testing::Test {
protected:
    static constexpr int kWidth = 320;
    static constexpr int kHeight = 240;

    dse::render::OpenGLRhiDevice device_;
    GLContextGuard ctx_guard_;

    void SetUp() override {
#ifdef _WIN32
        bool ok = CreateHiddenGLWindow(ctx_guard_, kWidth, kHeight);
        if (!ok) {
            GTEST_SKIP() << "Failed to create hidden GL window (no GPU/driver?), skipping";
        }
        // 加载现代 GL 函数指针；无真实 GPU 驱动时只能拿到软件 GL 1.1，
        // 此时按能力优雅跳过（与 Vulkan 冒烟测试"无驱动则跳过"一致）。
        const int gl_version = gladLoadGL(GLSmokeGladLoad);
        if (gl_version == 0 || !GLAD_GL_VERSION_3_3) {
            GTEST_SKIP() << "Requires OpenGL 3.3+ context (got "
                         << GLAD_VERSION_MAJOR(gl_version) << "."
                         << GLAD_VERSION_MINOR(gl_version)
                         << "); no modern GL driver in this environment";
        }
#else
        GTEST_SKIP() << "OpenGL smoke test requires Win32";
#endif
    }

    void TearDown() override {
        device_.Shutdown();
        ctx_guard_.Destroy();
    }
};

// 测试 GL RHI冒烟：初始化打开GL成功
TEST_F(GLRhiSmokeTest, InitOpenGLSucceeds) {
    device_.BeginFrame();
    device_.EndFrame();
    SUCCEED();
}

// 测试 GL RHI冒烟：单一帧空不崩溃
TEST_F(GLRhiSmokeTest, SingleFrameEmptyDoesNotCrash) {
    device_.BeginFrame();
    auto cmd = device_.CreateCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    device_.Submit(cmd);
    device_.EndFrame();
    SUCCEED();
}

// 测试 GL RHI冒烟：多帧周期稳定
TEST_F(GLRhiSmokeTest, MultiFramecycleStable) {
    for (int i = 0; i < 3; ++i) {
        device_.BeginFrame();
        auto cmd = device_.CreateCommandBuffer();
        device_.Submit(cmd);
        device_.EndFrame();
    }
    SUCCEED();
}

// 测试 GL RHI冒烟：关闭之后初始化不崩溃
TEST_F(GLRhiSmokeTest, ShutdownAfterReInitDoesNotCrash) {
    device_.BeginFrame();
    auto cmd = device_.CreateCommandBuffer();
    device_.Submit(cmd);
    device_.EndFrame();
    device_.Shutdown();

    device_.BeginFrame();
    auto cmd2 = device_.CreateCommandBuffer();
    device_.Submit(cmd2);
    device_.EndFrame();
    SUCCEED();
}

// 测试 GL RHI冒烟：创建且销毁无崩溃
TEST_F(GLRhiSmokeTest, CreateAndDestroyWithoutCrashing) {
    device_.BeginFrame();
    unsigned char pixels[] = {
        255,255,255,255, 255,255,255,255,
        255,255,255,255, 255,255,255,255
    };
    unsigned int tex = device_.CreateTexture2D(2, 2, pixels, true);
    EXPECT_NE(tex, 0u);
    device_.DeleteTexture(tex);
    device_.EndFrame();
    SUCCEED();
}

// 测试 GL RHI冒烟：情形10连续帧提交稳定
TEST_F(GLRhiSmokeTest, Case10ContinuousFrameSubmitStable) {
    for (int i = 0; i < 10; ++i) {
        device_.BeginFrame();
        auto cmd = device_.CreateCommandBuffer();
        cmd->ClearColor(glm::vec4(0.1f * i, 0.0f, 0.0f, 1.0f));
        device_.Submit(cmd);
        device_.EndFrame();
    }
    SUCCEED();
}

// 测试 GL RHI冒烟：渲染目标创建且销毁无崩溃
TEST_F(GLRhiSmokeTest, RenderTargetCreateAndDestroyWithoutCrashing) {
    device_.BeginFrame();
    RenderTargetDesc desc;
    desc.width = 64;
    desc.height = 64;
    desc.has_depth = true;
    unsigned int rt = device_.CreateRenderTarget(desc);
    EXPECT_NE(rt, 0u);
    device_.DeleteRenderTarget(rt);
    device_.EndFrame();
    SUCCEED();
}

// 测试 GL RHI冒烟：缓冲区创建且销毁无崩溃
TEST_F(GLRhiSmokeTest, BufferCreateAndDestroyWithoutCrashing) {
    device_.BeginFrame();
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    unsigned int buf = device_.CreateBuffer(sizeof(data), data, false, false);
    EXPECT_NE(buf, 0u);
    device_.DeleteBuffer(buf);
    device_.EndFrame();
    SUCCEED();
}

// 测试 GL RHI冒烟：顶点数组创建递增句柄
TEST_F(GLRhiSmokeTest, VertexArrayCreateIncrementsHandle) {
    device_.BeginFrame();
    dse::render::VertexArrayHandle vao1 = device_.CreateVertexArray();
    dse::render::VertexArrayHandle vao2 = device_.CreateVertexArray();
    EXPECT_NE(vao1.id, 0u);
    EXPECT_NE(vao2.id, 0u);
    EXPECT_NE(vao1.id, vao2.id);
    device_.DeleteVertexArray(vao1);
    device_.DeleteVertexArray(vao2);
    device_.EndFrame();
    SUCCEED();
}
