/**
 * @file forward_pbr_pixel_smoke_test.cpp
 * @brief B2b-1 静态 forward PBR「活体」像素冒烟 — 用高层 MeshRenderer（消费内建程序
 *        BuiltinProgram::ForwardPbr，真实 Cook-Torrance + 5 纹理槽 + PerFrame/PerScene/
 *        PerMaterial UBO）经通用原语绘制两块面片：
 *          - 左面片法线指向光源 → 被照亮（漫反射 + 环境）→ 明亮。
 *          - 右面片法线背向光源 → 仅环境项 → 暗（非零）。
 *        断言三后端各自「左亮右暗、左 > 右、背景黑」，且跨后端 RMSE 在阈值内。
 *
 * 这是 RHI_PRIMITIVE_CONTRACT.md §7「每效果随活体消费者 + 像素测试落地」闸门：
 *  - 端到端验证 MeshRenderer 抽象 + ForwardPbr 内建程序在 GL/DX11/Vulkan 上像素正确。
 *
 * 设计要点（与 instanced_ssbo smoke 同构）：
 *  - 场景沿 y=128 对称（两面片为竖直矩形，亮/暗沿 X 分布），故 DX11 垂直翻转不影响整帧 RMSE。
 *  - vp = 单位矩阵（view/proj 皆单位），顶点位置直接落在裁剪空间；camera 在 +Z。
 *  - 光照沿 X：to_light = +X；左面片法线 +X（N·L=1），右面片法线 -X（N·L≤0，仅环境）。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/mesh_renderer.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 构造两块竖直矩形面片（XY 平面 z=0，绕序 CCW 朝 +Z 相机）。
// 左面片法线 +X（朝光源）→ 亮；右面片法线 -X（背光源）→ 暗。
void MakeTwoQuads(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    auto add_quad = [&](float x0, float x1, const glm::vec3& n) {
        const uint16_t base = static_cast<uint16_t>(verts.size());
        const float y0 = -0.7f, y1 = 0.7f;
        // bl, br, tr, tl（CCW）
        verts.push_back({{x0, y0, 0.0f}, col, {0.0f, 0.0f}, n, {0.0f, 1.0f, 0.0f}});
        verts.push_back({{x1, y0, 0.0f}, col, {1.0f, 0.0f}, n, {0.0f, 1.0f, 0.0f}});
        verts.push_back({{x1, y1, 0.0f}, col, {1.0f, 1.0f}, n, {0.0f, 1.0f, 0.0f}});
        verts.push_back({{x0, y1, 0.0f}, col, {0.0f, 1.0f}, n, {0.0f, 1.0f, 0.0f}});
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    };
    add_quad(-0.9f, -0.1f, glm::vec3(1.0f, 0.0f, 0.0f));   // 左：法线朝光源 → 亮
    add_quad(0.1f, 0.9f, glm::vec3(-1.0f, 0.0f, 0.0f));    // 右：法线背光源 → 暗
}

RenderTargetReadback RenderForwardPbr(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;  // MeshRenderer PSO 写/测深度，需深度附件
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    // 内建 forward PBR 程序不可用（该后端未提供）→ 返回空 readback，由调用方跳过。
    if (device.GetBuiltinProgram(BuiltinProgram::ForwardPbr) == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeTwoQuads(verts, indices);

    MeshMaterial material;
    material.albedo = glm::vec3(0.8f, 0.8f, 0.8f);
    material.metallic = 0.0f;
    material.roughness = 0.5f;
    material.ao = 1.0f;

    DirectionalLight light;
    light.direction = glm::vec3(-1.0f, 0.0f, 0.0f);  // 光线沿 -X 传播 → to_light = +X
    light.color = glm::vec3(1.0f);
    light.intensity = 3.0f;
    light.ambient = 0.05f;
    light.enabled = true;

    const glm::mat4 I(1.0f);
    // proj 须含各后端裁剪修正（Vulkan: Y-flip + Z remap；DX11: Z remap；GL: 单位）。
    // 否则 Vulkan 下三角形绕序翻转被背面剔除，画面全黑。
    const glm::mat4 proj = device.GetProjectionCorrection();
    const glm::vec3 cam_pos(0.0f, 0.0f, 1.0f);

    MeshRenderer renderer;

    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        renderer.Draw(*cmd, device, verts, indices, I, I, proj, cam_pos, material, light);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();  // fence 等待完成，删资源才安全（Vulkan 严格）

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);

    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    return rb;
}

// 断言：左面片中心明亮、右面片中心暗（非零环境）、左明显亮于右、面片间隙与四角为清屏黑。
void VerifyLitDarkQuads(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cy = kRtSize / 2;  // 128，翻转轴（y 对称）
    const int lit_x = 64;        // 左面片中心（NDC x ≈ -0.5）
    const int dark_x = 192;      // 右面片中心（NDC x ≈ +0.5）

    const unsigned char* lit = dse::test::PixelAt(rb, lit_x, cy);
    const unsigned char* dark = dse::test::PixelAt(rb, dark_x, cy);
    ASSERT_NE(lit, nullptr) << backend << " lit";
    ASSERT_NE(dark, nullptr) << backend << " dark";

    // 左面片被照亮：明亮。
    EXPECT_GT(lit[0], 128) << backend << " lit R";
    EXPECT_GT(lit[1], 128) << backend << " lit G";
    EXPECT_GT(lit[2], 128) << backend << " lit B";

    // 右面片仅环境项：暗但非零（验证 ambient 路径），且明显暗于左。
    EXPECT_LT(dark[0], 110) << backend << " dark R";
    EXPECT_GT(dark[0], 8) << backend << " dark R nonzero";
    EXPECT_LT(dark[0] + dark[1] + dark[2], lit[0] + lit[1] + lit[2])
        << backend << " dark < lit";

    // 面片间隙（px 128，两面片之间）应为清屏黑。
    const unsigned char* gap = dse::test::PixelAt(rb, 128, cy);
    ASSERT_NE(gap, nullptr) << backend;
    EXPECT_LT(gap[0], 32) << backend << " gap R";
    EXPECT_LT(gap[1], 32) << backend << " gap G";
    EXPECT_LT(gap[2], 32) << backend << " gap B";

    // 四角为清屏黑。
    const unsigned char* corner = dse::test::PixelAt(rb, 6, 6);
    ASSERT_NE(corner, nullptr) << backend;
    EXPECT_LT(corner[0], 32) << backend << " corner R";
    EXPECT_LT(corner[1], 32) << backend << " corner G";
    EXPECT_LT(corner[2], 32) << backend << " corner B";
}

}  // namespace

// ============================================================
// 三后端单独像素验证（活体消费 MeshRenderer + ForwardPbr）。
// ============================================================

TEST(ForwardPbrPixelSmokeTest, OpenGLLitDarkQuads) {
    auto r = dse::test::RunOpenGL(RenderForwardPbr);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbr builtin program unavailable (OpenGL)";
    VerifyLitDarkQuads(r.readback, "OpenGL");
}

TEST(ForwardPbrPixelSmokeTest, D3D11LitDarkQuads) {
    auto r = dse::test::RunD3D11(RenderForwardPbr);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbr builtin program unavailable (D3D11)";
    VerifyLitDarkQuads(r.readback, "D3D11");
}

TEST(ForwardPbrPixelSmokeTest, VulkanLitDarkQuads) {
    auto r = dse::test::RunVulkan(RenderForwardPbr);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbr builtin program unavailable (Vulkan)";
    VerifyLitDarkQuads(r.readback, "Vulkan");
}

// ============================================================
// 跨后端一致性：场景沿 y=128 对称，DX11 垂直翻转不影响整帧 RMSE。
// ============================================================

TEST(ForwardPbrPixelSmokeTest, CrossBackendConsistent) {
    auto gl = dse::test::RunOpenGL(RenderForwardPbr);
    auto dx = dse::test::RunD3D11(RenderForwardPbr);
    auto vk = dse::test::RunVulkan(RenderForwardPbr);
    int available = (gl.available && !gl.readback.pixels.empty() ? 1 : 0) +
                    (dx.available && !dx.readback.pixels.empty() ? 1 : 0) +
                    (vk.available && !vk.readback.pixels.empty() ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "需要至少两个后端可用以比较 RMSE";

    constexpr double kRmseThreshold = 12.0;  // 软渲三实现着色微差容限
    if (gl.available && !gl.readback.pixels.empty() && dx.available && !dx.readback.pixels.empty()) {
        double rmse = dse::test::ComputeRmse(gl.readback, dx.readback);
        EXPECT_GE(rmse, 0.0);
        EXPECT_LT(rmse, kRmseThreshold) << "GL vs DX11 RMSE";
    }
    if (gl.available && !gl.readback.pixels.empty() && vk.available && !vk.readback.pixels.empty()) {
        double rmse = dse::test::ComputeRmse(gl.readback, vk.readback);
        EXPECT_GE(rmse, 0.0);
        EXPECT_LT(rmse, kRmseThreshold) << "GL vs Vulkan RMSE";
    }
    if (dx.available && !dx.readback.pixels.empty() && vk.available && !vk.readback.pixels.empty()) {
        double rmse = dse::test::ComputeRmse(dx.readback, vk.readback);
        EXPECT_GE(rmse, 0.0);
        EXPECT_LT(rmse, kRmseThreshold) << "DX11 vs Vulkan RMSE";
    }
}
