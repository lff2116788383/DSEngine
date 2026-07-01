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

// 测试 Vulkan RHI冒烟：初始化Vulkan成功
TEST_F(VulkanRhiSmokeTest, InitVulkanSucceeds) {
    bool ok = device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true);
    // 如果机器没有 Vulkan 驱动，跳过而非失败
    if (!ok) {
        GTEST_SKIP() << "Vulkan init failed (no driver/GPU?), skipping";
    }
    SUCCEED();
}

// 测试 Vulkan RHI冒烟：单一帧空不崩溃
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

// 测试 Vulkan RHI冒烟：多帧周期稳定
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

// 测试 Vulkan RHI冒烟：关闭之后初始化不崩溃
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

// 测试 Vulkan RHI冒烟：创建且销毁无崩溃
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

// 测试 Vulkan RHI冒烟：情形10连续帧提交稳定
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

// 测试 Vulkan RHI冒烟：渲染目标创建且销毁无崩溃
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

// 测试 Vulkan RHI冒烟：缓冲区创建且销毁无崩溃
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

// 离屏渲染 + 像素回读校验：清屏到已知颜色后回读，断言每个像素都正确。
TEST_F(VulkanRhiSmokeTest, ClearColorReadbackCorrect) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
    }
    constexpr int kRtSize = 64;
    RenderTargetDesc desc;
    desc.width = kRtSize;
    desc.height = kRtSize;
    desc.has_color = true;
    desc.has_depth = false;
    unsigned int rt = device_.CreateRenderTarget(desc);
    ASSERT_NE(rt, 0u);

    const glm::vec4 kClear(0.25f, 0.50f, 0.75f, 1.0f);
    device_.BeginFrame();
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
TEST_F(VulkanRhiSmokeTest, DepthRenderTargetReadback) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
    }
    constexpr int kRtSize = 32;
    RenderTargetDesc desc;
    desc.width = kRtSize;
    desc.height = kRtSize;
    desc.has_color = true;
    desc.has_depth = true;
    unsigned int rt = device_.CreateRenderTarget(desc);
    ASSERT_NE(rt, 0u);

    device_.BeginFrame();
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

// Pipeline state 创建/销毁冒烟
TEST_F(VulkanRhiSmokeTest, PipelineStateCreateAndDestroy) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
    }
    PipelineStateDesc ps_desc;
    ps_desc.blend_enabled = false;
    ps_desc.depth_test_enabled = true;
    ps_desc.depth_write_enabled = true;
    ps_desc.culling_enabled = true;
    unsigned int ps = device_.CreatePipelineState(ps_desc);
    EXPECT_NE(ps, 0u);
}

// 多 RT 创建/销毁无泄漏
TEST_F(VulkanRhiSmokeTest, MultipleRenderTargetsCreateDestroy) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
    }
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
    SUCCEED();
}

// 纹理格式变体创建/销毁
TEST_F(VulkanRhiSmokeTest, TextureFormatVariantsCreateDestroy) {
    if (!device_.InitVulkan(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No Vulkan";
    }
    // RGBA8 with mips
    unsigned char pixels[] = {
        255,0,0,255, 0,255,0,255,
        0,0,255,255, 255,255,0,255
    };
    unsigned int tex_mip = device_.CreateTexture2D(2, 2, pixels, true);
    EXPECT_NE(tex_mip, 0u);
    // RGBA8 without mips
    unsigned int tex_no_mip = device_.CreateTexture2D(2, 2, pixels, false);
    EXPECT_NE(tex_no_mip, 0u);
    device_.DeleteTexture(tex_mip);
    device_.DeleteTexture(tex_no_mip);
    SUCCEED();
}

#endif // DSE_ENABLE_VULKAN
