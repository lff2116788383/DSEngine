/**
 * @file vulkan_rhi_smoke_test.cpp
 * @brief Vulkan RHI 冒烟测试 — 验证真实 GPU 上的最小渲染帧不崩溃
 *
 * 覆盖场景：
 *   1. 创建隐藏 Win32 窗口 → InitVulkan 成功
 *   2. BeginFrame → 创建空 CommandBuffer → Submit → EndFrame 不崩溃
 *   3. 多帧循环（3 帧）稳定
 *   4. Shutdown 后再次 Init 不崩溃
 *   5. 资源创建/销毁基本路径（纹理、缓冲区）
 *
 * 编译条件：仅在 DSE_ENABLE_VULKAN 宏启用时编译
 */

#ifdef DSE_ENABLE_VULKAN

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    return DefWindowProcA(hwnd, msg, w, l);
}

HWND CreateHiddenWindow(int width, int height) {
    static bool registered = false;
    static const char* kClassName = "DSE_VulkanSmokeTest";
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = DummyWndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = kClassName;
        RegisterClassA(&wc);
        registered = true;
    }
    HWND hwnd = CreateWindowExA(
        0, kClassName, "VulkanSmokeTest",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    // 不调用 ShowWindow — 保持隐藏
    return hwnd;
}
#endif

} // namespace

class VulkanRhiSmokeTest : public ::testing::Test {
protected:
    static constexpr int kWidth = 320;
    static constexpr int kHeight = 240;

    dse::render::VulkanRhiDevice device_;
    HWND hwnd_ = nullptr;

    void SetUp() override {
#ifdef _WIN32
        hwnd_ = CreateHiddenWindow(kWidth, kHeight);
        ASSERT_NE(hwnd_, nullptr) << "Failed to create hidden Win32 window";
#else
        GTEST_SKIP() << "Vulkan smoke test requires Win32";
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

TEST_F(VulkanRhiSmokeTest, InitVulkanSucceeds) {
    bool ok = device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true);
    // 如果机器没有 Vulkan 驱动，跳过而非失败
    if (!ok) {
        GTEST_SKIP() << "Vulkan init failed (no driver/GPU?), skipping";
    }
    SUCCEED();
}

TEST_F(VulkanRhiSmokeTest, SingleFrameEmptyDoesNotCrash) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
    }
    device_.BeginFrame();
    auto cmd = device_.CreateCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    device_.Submit(cmd);
    device_.EndFrame();
    SUCCEED();
}

TEST_F(VulkanRhiSmokeTest, MultiFramecycleStable) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
    }
    for (int i = 0; i < 3; ++i) {
        device_.BeginFrame();
        auto cmd = device_.CreateCommandBuffer();
        device_.Submit(cmd);
        device_.EndFrame();
    }
    SUCCEED();
}

TEST_F(VulkanRhiSmokeTest, ShutdownAfterReInitDoesNotCrash) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
    }
    device_.BeginFrame();
    auto cmd = device_.CreateCommandBuffer();
    device_.Submit(cmd);
    device_.EndFrame();
    device_.Shutdown();

    // 重新初始化
    ASSERT_TRUE(device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true));
    device_.BeginFrame();
    auto cmd2 = device_.CreateCommandBuffer();
    device_.Submit(cmd2);
    device_.EndFrame();
    SUCCEED();
}

TEST_F(VulkanRhiSmokeTest, CreateAndDestroyWithoutCrashing) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
    }
    // 创建 2x2 白色纹理
    unsigned char pixels[] = {
        255,255,255,255, 255,255,255,255,
        255,255,255,255, 255,255,255,255
    };
    unsigned int tex = device_.CreateTexture2D(2, 2, pixels, true);
    EXPECT_NE(tex, 0u);
    device_.DeleteTexture(tex);
    SUCCEED();
}

TEST_F(VulkanRhiSmokeTest, Case10ContinuousFrameSubmitStable) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
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

TEST_F(VulkanRhiSmokeTest, RenderTargetCreateAndDestroyWithoutCrashing) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
    }
    RenderTargetDesc desc;
    desc.width = 64;
    desc.height = 64;
    desc.has_depth = true;
    unsigned int rt = device_.CreateRenderTarget(desc);
    EXPECT_NE(rt, 0u);
    device_.DeleteRenderTarget(rt);
    SUCCEED();
}

TEST_F(VulkanRhiSmokeTest, BufferCreateAndDestroyWithoutCrashing) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
    }
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    unsigned int buf = device_.CreateBuffer(sizeof(data), data, false, false);
    EXPECT_NE(buf, 0u);
    device_.DeleteBuffer(buf);
    SUCCEED();
}

#endif // DSE_ENABLE_VULKAN
