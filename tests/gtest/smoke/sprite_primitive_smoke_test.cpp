/**
 * @file sprite_primitive_smoke_test.cpp
 * @brief B0 通用绘制原语「活体」像素冒烟 — 用 SpriteRenderer 在真实 GPU 上画带纹理 quad，
 *        离屏回读像素校验。覆盖新原语全栈：BindShaderProgram / BindUniformBuffer /
 *        BindTexture(2D) / BindVertexBuffer / BindIndexBuffer / DrawIndexed。
 *
 * 与 gl/dx11/vulkan_rhi_smoke_test.cpp 同构（各后端自建 GPU 上下文，无驱动则优雅跳过）。
 * 这是 RHI_PRIMITIVE_CONTRACT.md §7 要求的「每个原语随活体消费者 + 像素测试落地」闸门。
 */

#include <gtest/gtest.h>

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/sprite_renderer.h"

#include <glm/glm.hpp>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 在已初始化的 device 上：建 256² RT → 清黑 → 用 SpriteRenderer 画居中红色纹理 quad
// （裁剪空间半边长 0.5，覆盖屏幕中央一半）→ 回读像素。
RenderTargetReadback RenderCenteredSpriteQuad(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    // 2×2 纯红纹理（采样到 quad，验证纹理路径而非仅顶点色）
    const unsigned char red[] = {
        255, 0, 0, 255,  255, 0, 0, 255,
        255, 0, 0, 255,  255, 0, 0, 255,
    };
    unsigned int tex = device.CreateTexture2D(2, 2, red, false);

    // SpriteRenderer 必须存活到帧提交完成后才能 Shutdown：其 VBO/IBO/UBO 被命令缓冲引用，
    // 帧内删除会使 Vulkan 命令缓冲失效（GL/DX11 容忍，Vulkan 严格）。
    SpriteRenderer sprite;
    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);

        sprite.Draw(*cmd, device, tex, glm::mat4(1.0f), 0.5f, glm::vec4(1.0f));

        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();
    sprite.Shutdown(device);  // 帧已提交 + fence 等待完成，删除安全

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    device.DeleteTexture(tex);
    device.DeleteRenderTarget(rt);
    return rb;
}

// 断言：中央像素为红（纹理被采样到 quad），四角为黑（quad 外为清屏色）。
void VerifyCenteredRedQuad(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    auto at = [&](int x, int y) {
        return rb.pixels.data() + (static_cast<size_t>(y) * kRtSize + x) * 4;
    };

    const unsigned char* center = at(kRtSize / 2, kRtSize / 2);
    EXPECT_GT(center[0], 128) << backend << " center R should be high (red quad)";
    EXPECT_LT(center[1], 64) << backend << " center G should be low";
    EXPECT_LT(center[2], 64) << backend << " center B should be low";

    // 四角应是清屏黑（quad 仅覆盖中央一半，角落在 quad 外）
    const int margin = 8;
    const unsigned char* corner = at(margin, margin);
    EXPECT_LT(corner[0], 24) << backend << " corner should be clear-black R";
    EXPECT_LT(corner[1], 24) << backend << " corner should be clear-black G";
    EXPECT_LT(corner[2], 24) << backend << " corner should be clear-black B";
}

} // namespace

// ============================================================
// OpenGL
// ============================================================
#ifdef _WIN32

#include "engine/render/rhi/opengl/gl_rhi_device.h"
#include "engine/render/rhi/opengl/gl_loader.h"

namespace {

LRESULT CALLBACK SpriteGLWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    return DefWindowProcA(hwnd, msg, w, l);
}

struct SpriteGLContextGuard {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC hglrc = nullptr;
    ~SpriteGLContextGuard() { Destroy(); }
    void Destroy() {
        if (hglrc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(hglrc); hglrc = nullptr; }
        if (hdc && hwnd) { ReleaseDC(hwnd, hdc); hdc = nullptr; }
        if (hwnd) { DestroyWindow(hwnd); hwnd = nullptr; }
    }
};

bool CreateSpriteGLWindow(SpriteGLContextGuard& guard) {
    static bool registered = false;
    static const char* kClassName = "DSE_SpritePrimSmokeTest";
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = SpriteGLWndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = kClassName;
        RegisterClassA(&wc);
        registered = true;
    }
    guard.hwnd = CreateWindowExA(0, kClassName, "SpritePrimSmoke", WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
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

GLADapiproc SpriteGLGladLoad(const char* name) {
    PROC proc = wglGetProcAddress(name);
    if (proc == nullptr) {
        static HMODULE gl_module = LoadLibraryA("opengl32.dll");
        if (gl_module) proc = GetProcAddress(gl_module, name);
    }
    return reinterpret_cast<GLADapiproc>(proc);
}

} // namespace

// 测试 Sprite 原语冒烟：OpenGL 居中红 quad
TEST(SpritePrimitiveSmokeTest, OpenGLDrawsCenteredTexturedQuad) {
    SpriteGLContextGuard guard;
    if (!CreateSpriteGLWindow(guard)) {
        GTEST_SKIP() << "No GL window/driver";
    }
    const int gl_version = gladLoadGL(SpriteGLGladLoad);
    if (gl_version == 0 || !GLAD_GL_VERSION_3_3) {
        GTEST_SKIP() << "Requires OpenGL 3.3+ context";
    }
    OpenGLRhiDevice device;
    RenderTargetReadback rb = RenderCenteredSpriteQuad(device);
    VerifyCenteredRedQuad(rb, "OpenGL");
    device.Shutdown();
}

// ============================================================
// Direct3D 11
// ============================================================
#ifdef DSE_ENABLE_D3D11

#include "engine/render/rhi/dx11/dx11_rhi_device.h"

namespace {

HWND CreateSpriteDX11Window() {
    static bool registered = false;
    static const char* kClassName = "DSE_SpritePrimSmoke_DX11";
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = SpriteGLWndProc;  // 通用 DefWindowProc
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = kClassName;
        RegisterClassA(&wc);
        registered = true;
    }
    return CreateWindowExA(0, kClassName, "SpritePrimSmokeDX11", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
                           nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
}

} // namespace

// 测试 Sprite 原语冒烟：D3D11 居中红 quad
TEST(SpritePrimitiveSmokeTest, D3D11DrawsCenteredTexturedQuad) {
    HWND hwnd = CreateSpriteDX11Window();
    if (!hwnd) {
        GTEST_SKIP() << "No Win32 window";
    }
    DX11RhiDevice device;
    if (!device.InitD3D11(static_cast<void*>(hwnd), 320, 240, true)) {
        DestroyWindow(hwnd);
        GTEST_SKIP() << "No D3D11 device/driver";
    }
    RenderTargetReadback rb = RenderCenteredSpriteQuad(device);
    VerifyCenteredRedQuad(rb, "D3D11");
    device.Shutdown();
    DestroyWindow(hwnd);
}

#endif // DSE_ENABLE_D3D11

// ============================================================
// Vulkan
// ============================================================
#ifdef DSE_ENABLE_VULKAN

#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"

namespace {

HWND CreateSpriteVulkanWindow() {
    static bool registered = false;
    static const char* kClassName = "DSE_SpritePrimSmoke_VK";
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = SpriteGLWndProc;  // 通用 DefWindowProc
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = kClassName;
        RegisterClassA(&wc);
        registered = true;
    }
    return CreateWindowExA(0, kClassName, "SpritePrimSmokeVK", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
                           nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
}

} // namespace

// 测试 Sprite 原语冒烟：Vulkan 居中红 quad
TEST(SpritePrimitiveSmokeTest, VulkanDrawsCenteredTexturedQuad) {
    HWND hwnd = CreateSpriteVulkanWindow();
    if (!hwnd) {
        GTEST_SKIP() << "No Win32 window";
    }
    VulkanRhiDevice device;
    if (!device.InitVulkan(static_cast<void*>(hwnd), 320, 240, true)) {
        DestroyWindow(hwnd);
        GTEST_SKIP() << "No Vulkan device/driver";
    }
    RenderTargetReadback rb = RenderCenteredSpriteQuad(device);
    VerifyCenteredRedQuad(rb, "Vulkan");
    device.Shutdown();
    DestroyWindow(hwnd);
}

#endif // DSE_ENABLE_VULKAN

#endif // _WIN32
