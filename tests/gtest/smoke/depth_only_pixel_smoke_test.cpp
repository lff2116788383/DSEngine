/**
 * @file depth_only_pixel_smoke_test.cpp
 * @brief B2b-4 shadow / depth-only「活体」像素冒烟 — 用高层 MeshRenderer::DrawDepthOnly
 *        （消费内建程序 BuiltinProgram::ForwardPbrDepth：forward_pbr.vert + 空 shadow.frag）
 *        向 has_color=false / has_depth=true 的渲染目标只写深度，再经新增原语
 *        RhiDevice::ReadRenderTargetDepthFloatWithSize 回读深度图，断言「非平凡」：
 *          - 居中面片覆盖区深度 ≈ 0.5（近清屏值 1.0）→ 明显小于背景。
 *          - 背景（未覆盖）深度 = 清屏 1.0。
 *          - 深度图 min ≪ max（确有几何写入，非全 0 / 全 1）。
 *        三后端各自验证，且跨后端深度图（编码为灰度）RMSE 在阈值内。
 *
 * 设计要点（与 forward_pbr / instanced smoke 同构）：
 *  - 把回读到的归一化深度 [0,1] 编码成灰度 RGBA8（d*255），复用 rhi_pixel_harness 的
 *    PixelAt / ComputeRmse；RenderFn 返回 ::RenderTargetReadback。
 *  - 面片居中（x,y 皆对称）→ 沿 y=128 对称，DX11 垂直翻转不影响整帧 RMSE。
 *  - vp = GetProjectionCorrection() * I：世界 z=0 经各后端裁剪修正后深度落 ≈ 0.5，
 *    与背景清屏 1.0 区分；proj 须含修正，否则 Vulkan 背面剔除 / 深度方向错。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/mesh_renderer.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 居中竖直矩形面片（XY 平面 z=0，绕序 CCW 朝 +Z 相机；法线 +Z）。
void MakeCenteredQuad(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    const glm::vec3 n(0.0f, 0.0f, 1.0f);
    const float a = 0.5f;  // 半边长 → NDC [-0.5,0.5] → 屏幕 [64,192]
    verts.push_back({{-a, -a, 0.0f}, col, {0.0f, 0.0f}, n, {1.0f, 0.0f, 0.0f}});
    verts.push_back({{ a, -a, 0.0f}, col, {1.0f, 0.0f}, n, {1.0f, 0.0f, 0.0f}});
    verts.push_back({{ a,  a, 0.0f}, col, {1.0f, 1.0f}, n, {1.0f, 0.0f, 0.0f}});
    verts.push_back({{-a,  a, 0.0f}, col, {0.0f, 1.0f}, n, {1.0f, 0.0f, 0.0f}});
    indices = {0, 1, 2, 0, 2, 3};
}

// 归一化深度 [0,1] → 灰度 RGBA8（便于复用 PixelAt / ComputeRmse）。
::RenderTargetReadback DepthToGray(const ::RenderTargetDepthReadback& d) {
    ::RenderTargetReadback rb;
    if (d.depth.empty()) return rb;  // 该后端不支持深度回读 → 空，调用方跳过
    rb.width = d.width;
    rb.height = d.height;
    rb.pixels.resize(static_cast<size_t>(d.width) * d.height * 4, 0u);
    for (size_t i = 0; i < d.depth.size(); ++i) {
        const float c = std::clamp(d.depth[i], 0.0f, 1.0f);
        const unsigned char g = static_cast<unsigned char>(c * 255.0f + 0.5f);
        rb.pixels[i * 4 + 0] = g;
        rb.pixels[i * 4 + 1] = g;
        rb.pixels[i * 4 + 2] = g;
        rb.pixels[i * 4 + 3] = 255u;
    }
    return rb;
}

::RenderTargetReadback RenderDepthOnly(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = false;  // 仅深度 pass：无颜色附件
    rt_desc.has_depth = true;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    // depth-only 内建程序不可用（该后端未提供）→ 返回空，由调用方跳过。
    if (device.GetBuiltinProgram(BuiltinProgram::ForwardPbrDepth) == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeCenteredQuad(verts, indices);

    const glm::mat4 I(1.0f);
    // proj 须含各后端裁剪修正（Vulkan: Y-flip + Z remap；DX11: Z remap；GL: 单位）。
    const glm::mat4 proj = device.GetProjectionCorrection();

    MeshRenderer renderer;

    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        // 三后端约定：clear_color_enabled 同时驱动深度清屏（GL glClear(DEPTH_BIT)、
        // Vulkan 选 loadOp=CLEAR 的 render pass）。depth-only RT 无颜色附件，颜色清屏为
        // no-op（GL drawBuffer=NONE / Vulkan num_color=0），但深度据此清到 1.0。
        // 若为 false：GL/Vulkan 深度残留 0，面片 0.5 过不了 GL_LESS → 全 0。
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        renderer.DrawDepthOnly(*cmd, device, verts, indices, I, I, proj);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();  // fence 等待完成，删资源才安全（Vulkan 严格）

    ::RenderTargetDepthReadback depth = device.ReadRenderTargetDepthFloatWithSize(rt);
    ::RenderTargetReadback rb = DepthToGray(depth);

    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    return rb;
}

// 断言：面片中心深度明显小于背景（写入了更近几何），背景为清屏 1.0，深度图非平凡。
void VerifyNonTrivialDepth(const ::RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cx = kRtSize / 2;  // 128
    const int cy = kRtSize / 2;  // 128（y 对称轴）

    const unsigned char* center = dse::test::PixelAt(rb, cx, cy);  // 面片覆盖区
    const unsigned char* corner = dse::test::PixelAt(rb, 6, 6);    // 背景
    ASSERT_NE(center, nullptr) << backend << " center";
    ASSERT_NE(corner, nullptr) << backend << " corner";

    // 背景 = 清屏深度 1.0 → 灰度 255（远）。
    EXPECT_GT(corner[0], 250) << backend << " background depth ~1.0";

    // 面片区深度 ≈ 0.5（z=0 经裁剪修正）→ 明显小于背景，且非 0、非 1。
    EXPECT_LT(center[0], 200) << backend << " geometry depth < background";
    EXPECT_GT(center[0], 40) << backend << " geometry depth nonzero";
    EXPECT_LT(center[0], corner[0] - 30) << backend << " geometry closer than background";

    // 深度图非平凡：min ≪ max（确有几何写入，而非全清屏）。
    unsigned char mn = 255, mx = 0;
    for (int i = 0; i < kRtSize * kRtSize; ++i) {
        const unsigned char g = rb.pixels[static_cast<size_t>(i) * 4];
        mn = std::min(mn, g);
        mx = std::max(mx, g);
    }
    EXPECT_GE(mx - mn, 40) << backend << " depth map must be non-trivial (min<<max)";
    EXPECT_GT(mx, 250) << backend << " has cleared-far region";
}

}  // namespace

// ============================================================
// 三后端单独深度回读验证（活体消费 MeshRenderer::DrawDepthOnly + ForwardPbrDepth）。
// ============================================================

TEST(DepthOnlyPixelSmokeTest, OpenGLNonTrivialDepth) {
    auto r = dse::test::RunOpenGL(RenderDepthOnly);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbrDepth/depth readback unavailable (OpenGL)";
    VerifyNonTrivialDepth(r.readback, "OpenGL");
}

TEST(DepthOnlyPixelSmokeTest, D3D11NonTrivialDepth) {
    auto r = dse::test::RunD3D11(RenderDepthOnly);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbrDepth/depth readback unavailable (D3D11)";
    VerifyNonTrivialDepth(r.readback, "D3D11");
}

TEST(DepthOnlyPixelSmokeTest, VulkanNonTrivialDepth) {
    auto r = dse::test::RunVulkan(RenderDepthOnly);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbrDepth/depth readback unavailable (Vulkan)";
    VerifyNonTrivialDepth(r.readback, "Vulkan");
}

// ============================================================
// 跨后端一致性：深度图（灰度）沿 y=128 对称，DX11 垂直翻转不影响整帧 RMSE。
// ============================================================

TEST(DepthOnlyPixelSmokeTest, CrossBackendConsistent) {
    auto gl = dse::test::RunOpenGL(RenderDepthOnly);
    auto dx = dse::test::RunD3D11(RenderDepthOnly);
    auto vk = dse::test::RunVulkan(RenderDepthOnly);
    int available = (gl.available && !gl.readback.pixels.empty() ? 1 : 0) +
                    (dx.available && !dx.readback.pixels.empty() ? 1 : 0) +
                    (vk.available && !vk.readback.pixels.empty() ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "需要至少两个后端可用以比较深度 RMSE";

    constexpr double kRmseThreshold = 16.0;  // 软渲三实现深度量化微差容限
    if (gl.available && !gl.readback.pixels.empty() && dx.available && !dx.readback.pixels.empty()) {
        double rmse = dse::test::ComputeRmse(gl.readback, dx.readback);
        EXPECT_GE(rmse, 0.0);
        EXPECT_LT(rmse, kRmseThreshold) << "GL vs DX11 depth RMSE";
    }
    if (gl.available && !gl.readback.pixels.empty() && vk.available && !vk.readback.pixels.empty()) {
        double rmse = dse::test::ComputeRmse(gl.readback, vk.readback);
        EXPECT_GE(rmse, 0.0);
        EXPECT_LT(rmse, kRmseThreshold) << "GL vs Vulkan depth RMSE";
    }
    if (dx.available && !dx.readback.pixels.empty() && vk.available && !vk.readback.pixels.empty()) {
        double rmse = dse::test::ComputeRmse(dx.readback, vk.readback);
        EXPECT_GE(rmse, 0.0);
        EXPECT_LT(rmse, kRmseThreshold) << "DX11 vs Vulkan depth RMSE";
    }
}
