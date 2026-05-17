/**
 * @file dx11_rhi_smoke_test.cpp
 * @brief D3D11 RHI 冒烟测试 — 验证真实 GPU 上的最小渲染帧不崩溃
 *
 * 覆盖场景（与 vulkan_rhi_smoke_test.cpp 对等）：
 *   1. 创建隐藏 Win32 窗口 → InitD3D11 成功
 *   2. BeginFrame → 创建 CommandBuffer → Submit → EndFrame 不崩溃
 *   3. 多帧循环（3 帧）稳定
 *   4. Shutdown 后再次 Init 不崩溃
 *   5. 纹理创建/销毁基本路径
 *   6. 10帧连续提交稳定
 *   7. RenderTarget 创建/销毁
 *   8. Buffer 创建/销毁
 *
 * 编译条件：仅在 DSE_ENABLE_D3D11 宏启用时编译（Windows 默认启用）
 */

#ifdef DSE_ENABLE_D3D11

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/render/rhi/dx11/dx11_rhi_device.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
LRESULT CALLBACK DX11DummyWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    return DefWindowProcA(hwnd, msg, w, l);
}

HWND CreateDX11HiddenWindow(int width, int height) {
    static bool registered = false;
    static const char* kClassName = "DSE_DX11SmokeTest";
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = DX11DummyWndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = kClassName;
        RegisterClassA(&wc);
        registered = true;
    }
    HWND hwnd = CreateWindowExA(
        0, kClassName, "DX11SmokeTest",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    return hwnd;
}
#endif

} // namespace

class DX11RhiSmokeTest : public ::testing::Test {
protected:
    static constexpr int kWidth = 320;
    static constexpr int kHeight = 240;

    dse::render::DX11RhiDevice device_;
    HWND hwnd_ = nullptr;

    void SetUp() override {
#ifdef _WIN32
        hwnd_ = CreateDX11HiddenWindow(kWidth, kHeight);
        ASSERT_NE(hwnd_, nullptr) << "Failed to create hidden Win32 window";
#else
        GTEST_SKIP() << "D3D11 smoke test requires Win32";
#endif
    }

    void TearDown() override {
        device_.Shutdown();
#ifdef _WIN32
        if (hwnd_) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
#endif
    }
};

TEST_F(DX11RhiSmokeTest, InitD3D11成功) {
    bool ok = device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true);
    if (!ok) {
        GTEST_SKIP() << "D3D11 init failed (no GPU/driver?), skipping";
    }
    SUCCEED();
}

TEST_F(DX11RhiSmokeTest, 单帧空提交不崩溃) {
    if (!device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No D3D11";
    }
    device_.BeginFrame();
    auto cmd = device_.CreateCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    device_.Submit(cmd);
    device_.EndFrame();
    SUCCEED();
}

TEST_F(DX11RhiSmokeTest, 多帧循环稳定) {
    if (!device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No D3D11";
    }
    for (int i = 0; i < 3; ++i) {
        device_.BeginFrame();
        auto cmd = device_.CreateCommandBuffer();
        device_.Submit(cmd);
        device_.EndFrame();
    }
    SUCCEED();
}

TEST_F(DX11RhiSmokeTest, Shutdown后重新Init不崩溃) {
    if (!device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No D3D11";
    }
    device_.BeginFrame();
    auto cmd = device_.CreateCommandBuffer();
    device_.Submit(cmd);
    device_.EndFrame();
    device_.Shutdown();

    ASSERT_TRUE(device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true));
    device_.BeginFrame();
    auto cmd2 = device_.CreateCommandBuffer();
    device_.Submit(cmd2);
    device_.EndFrame();
    SUCCEED();
}

TEST_F(DX11RhiSmokeTest, 纹理创建销毁不崩溃) {
    if (!device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No D3D11";
    }
    unsigned char pixels[] = {
        255,255,255,255, 255,255,255,255,
        255,255,255,255, 255,255,255,255
    };
    unsigned int tex = device_.CreateTexture2D(2, 2, pixels, true);
    EXPECT_NE(tex, 0u);
    device_.DeleteTexture(tex);
    SUCCEED();
}

TEST_F(DX11RhiSmokeTest, 10帧连续提交稳定) {
    if (!device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No D3D11";
    }
    for (int i = 0; i < 10; ++i) {
        device_.BeginFrame();
        auto cmd = device_.CreateCommandBuffer();
        cmd->ClearColor(glm::vec4(0.1f * i, 0.0f, 0.0f, 1.0f));
        device_.Submit(cmd);
        device_.EndFrame();
    }
    SUCCEED();
}

TEST_F(DX11RhiSmokeTest, RenderTarget创建销毁不崩溃) {
    if (!device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No D3D11";
    }
    RenderTargetDesc desc;
    desc.width = 64;
    desc.height = 64;
    desc.has_depth = true;
    unsigned int rt = device_.CreateRenderTarget(desc);
    EXPECT_NE(rt, 0u);
    device_.resource_mgr().DeleteRenderTarget(rt);
    SUCCEED();
}

TEST_F(DX11RhiSmokeTest, Buffer创建销毁不崩溃) {
    if (!device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No D3D11";
    }
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    unsigned int buf = device_.CreateBuffer(sizeof(data), data, false, false);
    EXPECT_NE(buf, 0u);
    device_.DeleteBuffer(buf);
    SUCCEED();
}

#endif // DSE_ENABLE_D3D11
