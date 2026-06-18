/**
 * @file skinned_mesh_pixel_smoke_test.cpp
 * @brief B2b-2 蒙皮 forward PBR「活体」像素冒烟 — 用高层 MeshRenderer::DrawSkinned
 *        （消费内建程序 BuiltinProgram::ForwardPbrSkinned；VS 按 bone index/weight 混合
 *        骨骼矩阵 SSBO\@slot 0 后施 vp，复用静态 Cook-Torrance frag）。
 *
 * 设计要点（一根两骨骼网格，骨骼位移后顶点落位正确）：
 *  - 两块「绑定空间」竖直矩形面片，皆居中于原点（x∈[-0.4,0.4], y∈[-0.7,0.7]）。
 *    左面片全权重绑骨骼 0、法线 +X；右面片全权重绑骨骼 1、法线 -X。
 *  - 骨骼 0 = 平移(-0.5,0,0)，骨骼 1 = 平移(+0.5,0,0)。
 *    蒙皮正确 → 左面片落到 x∈[-0.9,-0.1]（被照亮）、右面片落到 x∈[0.1,0.9]（仅环境，暗）,
 *    与 forward_pbr 静态 smoke 同一目标图像 → 复用 VerifyLitDarkQuads + 跨后端 RMSE。
 *  - 若蒙皮失效（骨骼矩阵未被读/未施加）→ 两面片仍叠在原点中心，x=64/x=192 处为黑，断言失败。
 *  - 场景沿 y=128 对称（位移仅沿 X），DX11 垂直翻转不影响整帧 RMSE。
 *  - proj 含各后端裁剪修正（GetProjectionCorrection），否则 Vulkan 背面剔除致全黑。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/mesh_renderer.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 两块居中于原点的竖直矩形面片（绑定空间，z=0，CCW 朝 +Z 相机）。
// 左面片全权重绑骨骼 0、法线 +X；右面片全权重绑骨骼 1、法线 -X。
void MakeTwoSkinnedQuads(std::vector<SkinnedMeshVertex>& verts,
                         std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    auto add_quad = [&](const glm::vec3& n, float bone) {
        const uint16_t base = static_cast<uint16_t>(verts.size());
        const float x0 = -0.4f, x1 = 0.4f, y0 = -0.7f, y1 = 0.7f;
        const glm::vec4 bidx(bone, 0.0f, 0.0f, 0.0f);
        const glm::vec4 bw(1.0f, 0.0f, 0.0f, 0.0f);  // 全权重绑该骨骼
        const glm::vec3 tan(0.0f, 1.0f, 0.0f);
        SkinnedMeshVertex v;
        v.color = col; v.normal = n; v.tangent = tan;
        v.bone_indices = bidx; v.bone_weights = bw;
        v.position = {x0, y0, 0.0f}; v.uv = {0.0f, 0.0f}; verts.push_back(v);
        v.position = {x1, y0, 0.0f}; v.uv = {1.0f, 0.0f}; verts.push_back(v);
        v.position = {x1, y1, 0.0f}; v.uv = {1.0f, 1.0f}; verts.push_back(v);
        v.position = {x0, y1, 0.0f}; v.uv = {0.0f, 1.0f}; verts.push_back(v);
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    };
    add_quad(glm::vec3(1.0f, 0.0f, 0.0f), 0.0f);   // 左：骨骼 0，法线朝光源 → 亮
    add_quad(glm::vec3(-1.0f, 0.0f, 0.0f), 1.0f);  // 右：骨骼 1，法线背光源 → 暗
}

RenderTargetReadback RenderSkinnedMesh(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    if (device.GetBuiltinProgram(BuiltinProgram::ForwardPbrSkinned) == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    std::vector<SkinnedMeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeTwoSkinnedQuads(verts, indices);

    // 两根骨骼：平移把居中面片搬到左右目标位置（蒙皮位移）。
    std::vector<glm::mat4> bones(2);
    bones[0] = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.0f, 0.0f));
    bones[1] = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 0.0f));

    MeshMaterial material;
    material.albedo = glm::vec3(0.8f, 0.8f, 0.8f);
    material.metallic = 0.0f;
    material.roughness = 0.5f;
    material.ao = 1.0f;

    DirectionalLight light;
    light.direction = glm::vec3(-1.0f, 0.0f, 0.0f);  // to_light = +X
    light.color = glm::vec3(1.0f);
    light.intensity = 3.0f;
    light.ambient = 0.05f;
    light.enabled = true;

    const glm::mat4 I(1.0f);
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
        renderer.DrawSkinned(*cmd, device, verts, indices, I, bones, I, proj, cam_pos,
                             material, light);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);

    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    return rb;
}

// 与 forward_pbr smoke 同一断言：左亮右暗、左>右、间隙与四角清屏黑。
void VerifyLitDarkQuads(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cy = kRtSize / 2;
    const int lit_x = 64;
    const int dark_x = 192;

    const unsigned char* lit = dse::test::PixelAt(rb, lit_x, cy);
    const unsigned char* dark = dse::test::PixelAt(rb, dark_x, cy);
    ASSERT_NE(lit, nullptr) << backend << " lit";
    ASSERT_NE(dark, nullptr) << backend << " dark";

    EXPECT_GT(lit[0], 128) << backend << " lit R";
    EXPECT_GT(lit[1], 128) << backend << " lit G";
    EXPECT_GT(lit[2], 128) << backend << " lit B";

    EXPECT_LT(dark[0], 110) << backend << " dark R";
    EXPECT_GT(dark[0], 8) << backend << " dark R nonzero";
    EXPECT_LT(dark[0] + dark[1] + dark[2], lit[0] + lit[1] + lit[2])
        << backend << " dark < lit";

    const unsigned char* gap = dse::test::PixelAt(rb, 128, cy);
    ASSERT_NE(gap, nullptr) << backend;
    EXPECT_LT(gap[0], 32) << backend << " gap R";
    EXPECT_LT(gap[1], 32) << backend << " gap G";
    EXPECT_LT(gap[2], 32) << backend << " gap B";

    const unsigned char* corner = dse::test::PixelAt(rb, 6, 6);
    ASSERT_NE(corner, nullptr) << backend;
    EXPECT_LT(corner[0], 32) << backend << " corner R";
    EXPECT_LT(corner[1], 32) << backend << " corner G";
    EXPECT_LT(corner[2], 32) << backend << " corner B";
}

}  // namespace

// ============================================================
// 三后端单独像素验证（活体消费 MeshRenderer::DrawSkinned + ForwardPbrSkinned）。
// ============================================================

TEST(SkinnedMeshPixelSmokeTest, OpenGLSkinnedQuads) {
    auto r = dse::test::RunOpenGL(RenderSkinnedMesh);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbrSkinned builtin program unavailable (OpenGL)";
    VerifyLitDarkQuads(r.readback, "OpenGL");
}

TEST(SkinnedMeshPixelSmokeTest, D3D11SkinnedQuads) {
    auto r = dse::test::RunD3D11(RenderSkinnedMesh);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbrSkinned builtin program unavailable (D3D11)";
    VerifyLitDarkQuads(r.readback, "D3D11");
}

TEST(SkinnedMeshPixelSmokeTest, VulkanSkinnedQuads) {
    auto r = dse::test::RunVulkan(RenderSkinnedMesh);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbrSkinned builtin program unavailable (Vulkan)";
    VerifyLitDarkQuads(r.readback, "Vulkan");
}

// ============================================================
// 跨后端一致性：场景沿 y=128 对称，DX11 垂直翻转不影响整帧 RMSE。
// ============================================================

TEST(SkinnedMeshPixelSmokeTest, CrossBackendConsistent) {
    auto gl = dse::test::RunOpenGL(RenderSkinnedMesh);
    auto dx = dse::test::RunD3D11(RenderSkinnedMesh);
    auto vk = dse::test::RunVulkan(RenderSkinnedMesh);
    int available = (gl.available && !gl.readback.pixels.empty() ? 1 : 0) +
                    (dx.available && !dx.readback.pixels.empty() ? 1 : 0) +
                    (vk.available && !vk.readback.pixels.empty() ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "需要至少两个后端可用以比较 RMSE";

    constexpr double kRmseThreshold = 12.0;
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
