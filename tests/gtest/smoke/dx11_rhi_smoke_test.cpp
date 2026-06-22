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
#include "engine/render/post_process_renderer.h"

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

// 测试 DX 11 RHI冒烟：初始化D 3D 11成功
TEST_F(DX11RhiSmokeTest, InitD3D11Succeeds) {
    bool ok = device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true);
    if (!ok) {
        GTEST_SKIP() << "D3D11 init failed (no GPU/driver?), skipping";
    }
    SUCCEED();
}

// 测试 DX 11 RHI冒烟：单一帧空不崩溃
TEST_F(DX11RhiSmokeTest, SingleFrameEmptyDoesNotCrash) {
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

// 测试 DX 11 RHI冒烟：多帧周期稳定
TEST_F(DX11RhiSmokeTest, MultiFramecycleStable) {
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

// 测试 DX 11 RHI冒烟：关闭之后初始化不崩溃
TEST_F(DX11RhiSmokeTest, ShutdownAfterReInitDoesNotCrash) {
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

// 测试 DX 11 RHI冒烟：创建且销毁无崩溃
TEST_F(DX11RhiSmokeTest, CreateAndDestroyWithoutCrashing) {
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

// 测试 DX 11 RHI冒烟：情形10连续帧提交稳定
TEST_F(DX11RhiSmokeTest, Case10ContinuousFrameSubmitStable) {
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

// 测试 DX 11 RHI冒烟：渲染目标创建且销毁无崩溃
TEST_F(DX11RhiSmokeTest, RenderTargetCreateAndDestroyWithoutCrashing) {
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

// 测试 DX 11 RHI冒烟：缓冲区创建且销毁无崩溃
TEST_F(DX11RhiSmokeTest, BufferCreateAndDestroyWithoutCrashing) {
    if (!device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No D3D11";
    }
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    unsigned int buf = device_.CreateBuffer(sizeof(data), data, false, false);
    EXPECT_NE(buf, 0u);
    device_.DeleteBuffer(buf);
    SUCCEED();
}

// 离屏渲染 + 像素回读校验：清屏到已知颜色后回读，断言每个像素都正确。
// WARP 是参考级（conformant）软件光栅器，结果可信，可在无 GPU 环境验证
// D3D11 实际渲染输出的正确性（而不仅是"不崩溃"）。
// 测试 DX 11 RHI冒烟：正确
TEST_F(DX11RhiSmokeTest, Correct) {
    if (!device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No D3D11";
    }
    constexpr int kRtSize = 64;
    RenderTargetDesc desc;
    desc.width = kRtSize;
    desc.height = kRtSize;
    desc.has_color = true;
    desc.has_depth = false;
    unsigned int rt = device_.CreateRenderTarget(desc);
    ASSERT_NE(rt, 0u);

    // 清屏到已知颜色（SDR/WARP 下 RT 为 R8G8B8A8_UNORM，无 sRGB / tonemap 转换）
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

    // 期望 8-bit UNORM 值（四舍五入）：约 (64,128,191,255)。
    // WARP/驱动舍入策略可能略有差异，留 ±2 容差。
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

    device_.resource_mgr().DeleteRenderTarget(rt);
}

// Draw-call 级回读校验：用全屏四边形 + passthrough 着色器把 source RT 拷到 dest RT，
// 再回读 dest 断言像素正确。比清屏更进一步——覆盖顶点装配 + 光栅化 + 片元采样 +
// passthrough 片元着色器（"copy" effect 走 DX11 的标准全屏路径，与引擎最终 present
// 拷贝同一代码路）。source 为纯色，故采样/过滤不影响结果。
// 测试 DX 11 RHI冒烟：全部正确
TEST_F(DX11RhiSmokeTest, AllCorrect) {
    if (!device_.InitD3D11(static_cast<void*>(hwnd_), kWidth, kHeight, true)) {
        GTEST_SKIP() << "No D3D11";
    }
    constexpr int kRtSize = 64;
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    unsigned int src = device_.CreateRenderTarget(rt_desc);
    unsigned int dst = device_.CreateRenderTarget(rt_desc);
    ASSERT_NE(src, 0u);
    ASSERT_NE(dst, 0u);

    const glm::vec4 kSrcColor(0.75f, 0.25f, 0.50f, 1.0f); // ~ (191,64,128,255)

    // 全屏拷贝需关深度测试 / 背面剔除 / 混合（与引擎 present 用的 composite 状态一致），
    // 否则全屏四边形会被剔除或深度测试丢弃。
    PipelineStateDesc ps_desc;
    ps_desc.blend_enabled = false;
    ps_desc.depth_test_enabled = false;
    ps_desc.depth_write_enabled = false;
    ps_desc.culling_enabled = false;
    unsigned int ps = device_.CreatePipelineState(ps_desc);
    ASSERT_NE(ps, 0u);

    dse::render::PostProcessRenderer pp_renderer;
    device_.BeginFrame();
    pp_renderer.BeginFrame();
    auto cmd = device_.CreateCommandBuffer();
    ASSERT_NE(cmd, nullptr);

    // pass 1：清 source 到已知颜色
    {
        RenderPassDesc rp;
        rp.render_target = src;
        rp.clear_color = kSrcColor;
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        cmd->EndRenderPass();
    }
    // pass 2：dest 先清黑，再用全屏 passthrough 把 source 拷过来（必须覆盖掉黑色）
    {
        RenderPassDesc rp;
        rp.render_target = dst;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        cmd->BindPipeline(device_.GetGraphicsPipeline(ps, 0));
        pp_renderer.Draw(*cmd, device_,
            dse::render::PostProcessRequest("copy", device_.GetRenderTargetColorTexture(src)));
        cmd->EndRenderPass();
    }
    device_.Submit(cmd);
    device_.EndFrame();

    RenderTargetReadback rb = device_.ReadRenderTargetColorRgba8WithSize(dst);
    ASSERT_EQ(rb.width, kRtSize);
    ASSERT_EQ(rb.height, kRtSize);
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4);

    const unsigned char exp_r = static_cast<unsigned char>(kSrcColor.r * 255.0f + 0.5f);
    const unsigned char exp_g = static_cast<unsigned char>(kSrcColor.g * 255.0f + 0.5f);
    const unsigned char exp_b = static_cast<unsigned char>(kSrcColor.b * 255.0f + 0.5f);
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

    pp_renderer.Shutdown(device_);
    device_.resource_mgr().DeleteRenderTarget(src);
    device_.resource_mgr().DeleteRenderTarget(dst);
}

#endif // DSE_ENABLE_D3D11
