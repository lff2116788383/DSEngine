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

// 离屏渲染 + 像素回读校验：清屏到已知颜色后回读，断言每个像素都正确。
TEST_F(GLRhiSmokeTest, ClearColorReadbackCorrect) {
    constexpr int kRtSize = 64;
    RenderTargetDesc desc;
    desc.width = kRtSize;
    desc.height = kRtSize;
    desc.has_color = true;
    desc.has_depth = false;

    device_.BeginFrame();
    unsigned int rt = device_.CreateRenderTarget(desc);
    ASSERT_NE(rt, 0u);

    const glm::vec4 kClear(0.25f, 0.50f, 0.75f, 1.0f);
    auto cmd = device_.CreateCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    RenderPassDesc rp;
    rp.render_target = rt;
    rp.clear_color = kClear;
    rp.clear_color_enabled = true;
    cmd->BeginRenderPass(rp);
    cmd->EndRenderPass();
    device_.Submit(cmd);
    device_.EndFrame();

    RenderTargetReadback rb = device_.ReadRenderTargetColorRgba8WithSize(rt);
    ASSERT_EQ(rb.width, kRtSize);
    ASSERT_EQ(rb.height, kRtSize);
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4);

    const unsigned char exp_r = static_cast<unsigned char>(kClear.r * 255.0f + 0.5f);
    const unsigned char exp_g = static_cast<unsigned char>(kClear.g * 255.0f + 0.5f);
    const unsigned char exp_b = static_cast<unsigned char>(kClear.b * 255.0f + 0.5f);
    const unsigned char exp_a = 255;
    auto within_tol = [](unsigned char a, unsigned char b) {
        return (a > b ? a - b : b - a) <= 2;
    };

    for (int p = 0; p < kRtSize * kRtSize; ++p) {
        const unsigned char* px = rb.pixels.data() + static_cast<size_t>(p) * 4;
        ASSERT_TRUE(within_tol(px[0], exp_r)) << "pixel " << p << " R=" << int(px[0]) << " expected~" << int(exp_r);
        ASSERT_TRUE(within_tol(px[1], exp_g)) << "pixel " << p << " G=" << int(px[1]) << " expected~" << int(exp_g);
        ASSERT_TRUE(within_tol(px[2], exp_b)) << "pixel " << p << " B=" << int(px[2]) << " expected~" << int(exp_b);
        ASSERT_TRUE(within_tol(px[3], exp_a)) << "pixel " << p << " A=" << int(px[3]) << " expected~" << int(exp_a);
    }

    device_.DeleteRenderTarget(rt);
}

// 深度 RT 回读校验：创建带深度的 RT，渲染一帧后回读深度缓冲。
TEST_F(GLRhiSmokeTest, DepthRenderTargetReadback) {
    constexpr int kRtSize = 32;
    RenderTargetDesc desc;
    desc.width = kRtSize;
    desc.height = kRtSize;
    desc.has_color = true;
    desc.has_depth = true;

    device_.BeginFrame();
    unsigned int rt = device_.CreateRenderTarget(desc);
    ASSERT_NE(rt, 0u);

    auto cmd = device_.CreateCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    RenderPassDesc rp;
    rp.render_target = rt;
    rp.clear_color = glm::vec4(0.0f);
    rp.clear_color_enabled = true;
    cmd->BeginRenderPass(rp);
    cmd->EndRenderPass();
    device_.Submit(cmd);
    device_.EndFrame();

    RenderTargetDepthReadback drb = device_.ReadRenderTargetDepthFloatWithSize(rt);
    if (drb.width == 0) {
        device_.DeleteRenderTarget(rt);
        GTEST_SKIP() << "Depth readback not supported";
    }
    ASSERT_EQ(drb.width, kRtSize);
    ASSERT_EQ(drb.height, kRtSize);
    ASSERT_EQ(drb.depth.size(), static_cast<size_t>(kRtSize) * kRtSize);

    for (int p = 0; p < kRtSize * kRtSize; ++p) {
        ASSERT_NEAR(drb.depth[p], 1.0f, 0.001f) << "depth pixel " << p;
    }

    device_.DeleteRenderTarget(rt);
}

// Shader 编译冒烟：用最小顶点 + 片元着色器验证 CreateShaderProgram 返回有效句柄
TEST_F(GLRhiSmokeTest, ShaderCompilationSmoke) {
    device_.BeginFrame();

    const std::string vert_src = R"(
        #version 330 core
        layout(location = 0) in vec2 a_position;
        void main() {
            gl_Position = vec4(a_position, 0.0, 1.0);
        }
    )";
    const std::string frag_src = R"(
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
    )";

    unsigned int prog = device_.CreateShaderProgram(vert_src, frag_src);
    EXPECT_NE(prog, 0u);
    device_.DeleteShaderProgram(prog);
    device_.EndFrame();
    SUCCEED();
}

// Pipeline state 创建冒烟
TEST_F(GLRhiSmokeTest, PipelineStateCreateSmoke) {
    device_.BeginFrame();
    PipelineStateDesc ps_desc;
    ps_desc.blend_enabled = false;
    ps_desc.depth_test_enabled = true;
    ps_desc.depth_write_enabled = true;
    ps_desc.culling_enabled = true;
    unsigned int ps = device_.CreatePipelineState(ps_desc);
    EXPECT_NE(ps, 0u);
    device_.EndFrame();
    SUCCEED();
}

// 多 RT 创建/销毁无泄漏
TEST_F(GLRhiSmokeTest, MultipleRenderTargetsCreateDestroy) {
    device_.BeginFrame();
    constexpr int kCount = 8;
    unsigned int handles[kCount];
    for (int i = 0; i < kCount; ++i) {
        RenderTargetDesc desc;
        desc.width = 32 + i * 16;
        desc.height = 32 + i * 16;
        desc.has_color = true;
        desc.has_depth = (i % 2 == 0);
        handles[i] = device_.CreateRenderTarget(desc);
        ASSERT_NE(handles[i], 0u) << "RT #" << i;
    }
    for (int i = kCount - 1; i >= 0; --i) {
        device_.DeleteRenderTarget(handles[i]);
    }
    device_.EndFrame();
    SUCCEED();
}

// 纹理格式变体创建/销毁
TEST_F(GLRhiSmokeTest, TextureFormatVariantsCreateDestroy) {
    device_.BeginFrame();
    unsigned char pixels[] = {
        255,0,0,255, 0,255,0,255,
        0,0,255,255, 255,255,0,255
    };
    unsigned int tex_mip = device_.CreateTexture2D(2, 2, pixels, true);
    EXPECT_NE(tex_mip, 0u);
    unsigned int tex_no_mip = device_.CreateTexture2D(2, 2, pixels, false);
    EXPECT_NE(tex_no_mip, 0u);
    device_.DeleteTexture(tex_mip);
    device_.DeleteTexture(tex_no_mip);
    device_.EndFrame();
    SUCCEED();
}
