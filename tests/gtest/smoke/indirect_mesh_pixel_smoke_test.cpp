/**
 * @file indirect_mesh_pixel_smoke_test.cpp
 * @brief B2b-5 GPU-driven 间接绘制 forward PBR「活体」像素冒烟 — 用高层 MeshRenderer::DrawIndirect
 *        （消费 CommandBuffer::DrawIndexedIndirect 通用原语；绘制参数 count/instance_count 来自 GPU 端
 *        indirect buffer 内的 DrawElementsIndirectCommand，而非 CPU 立即数。三后端原语对齐：
 *        GL→glMultiDrawElementsIndirect / Vulkan→vkCmdDrawIndexedIndirect / DX11→DrawIndexedInstancedIndirect。
 *        着色器复用 BuiltinProgram::ForwardPbrInstanced：VS 按 gl_InstanceIndex 从 instance model SSBO\@slot 0
 *        取出后施 model + vp，复用静态 Cook-Torrance frag）。
 *
 * 设计要点（与 B2b-3 实例化 smoke 同一对称场景，N=2 个实例不同 transform 互不重叠）：
 *  - 单块「局部空间」竖直矩形面片，居中于原点（x∈[-0.4,0.4], y∈[-0.7,0.7]），法线 +X（朝光源）。
 *  - 两个实例 model 矩阵：实例 0 = 平移(-0.5,0,0)，实例 1 = 平移(+0.5,0,0)。
 *    间接绘制正确（从 indirect buffer 读出 instance_count=2）→ 左实例落 x≈64、右实例落 x≈192，
 *    皆被照亮、中间 x=128 有暗隙 → 证明「间接绘制参数被三后端正确消费且每实例 transform 生效」。
 *  - 若间接绘制失效：
 *      · indirect buffer 未读 / count 取错 → 面片缺失或退化 → 断言失败；
 *      · instance_count 读成 1 → 仅左实例（gl_InstanceIndex 0）亮、右侧 x=192 黑 → 断言失败。
 *  - 场景沿 y=128 对称（位移仅沿 X），DX11 回读垂直翻转不影响整帧 RMSE。
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

// 单块居中于原点的竖直矩形面片（局部空间，z=0，CCW 朝 +Z 相机），法线 +X 朝光源。
void MakeCenteredQuad(std::vector<MeshVertex>& verts,
                      std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    const glm::vec3 n(1.0f, 0.0f, 0.0f);
    const glm::vec3 tan(0.0f, 1.0f, 0.0f);
    const float x0 = -0.4f, x1 = 0.4f, y0 = -0.7f, y1 = 0.7f;
    MeshVertex v;
    v.color = col; v.normal = n; v.tangent = tan;
    v.position = {x0, y0, 0.0f}; v.uv = {0.0f, 0.0f}; verts.push_back(v);
    v.position = {x1, y0, 0.0f}; v.uv = {1.0f, 0.0f}; verts.push_back(v);
    v.position = {x1, y1, 0.0f}; v.uv = {1.0f, 1.0f}; verts.push_back(v);
    v.position = {x0, y1, 0.0f}; v.uv = {0.0f, 1.0f}; verts.push_back(v);
    indices = {0, 1, 2, 0, 2, 3};
}

RenderTargetReadback RenderIndirectMesh(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    if (device.GetBuiltinProgram(BuiltinProgram::ForwardPbrInstanced) == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeCenteredQuad(verts, indices);

    // 两个实例：平移把居中面片搬到左右目标位置（每实例 transform，instance_count 经 indirect buffer 表达）。
    std::vector<glm::mat4> instances(2);
    instances[0] = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.0f, 0.0f));
    instances[1] = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 0.0f));

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
        renderer.DrawIndirect(*cmd, device, verts, indices, instances, I, proj, cam_pos,
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

// 两个实例皆亮、互不重叠：左(x=64)亮、右(x=192)亮、中隙(x=128)暗、四角清屏黑。
void VerifyTwoInstances(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cy = kRtSize / 2;
    const int left_x = 64;
    const int right_x = 192;

    const unsigned char* left = dse::test::PixelAt(rb, left_x, cy);
    const unsigned char* right = dse::test::PixelAt(rb, right_x, cy);
    ASSERT_NE(left, nullptr) << backend << " left";
    ASSERT_NE(right, nullptr) << backend << " right";

    // 两实例皆被照亮（法线朝光源）—— 证明 indirect buffer 内 instance_count=2 被正确消费。
    EXPECT_GT(left[0], 128) << backend << " left R";
    EXPECT_GT(left[1], 128) << backend << " left G";
    EXPECT_GT(left[2], 128) << backend << " left B";

    EXPECT_GT(right[0], 128) << backend << " right R";
    EXPECT_GT(right[1], 128) << backend << " right G";
    EXPECT_GT(right[2], 128) << backend << " right B";

    // 中间暗隙：证明两实例分居左右、互不重叠。
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
// 三后端单独像素验证（活体消费 MeshRenderer::DrawIndirect + CommandBuffer::DrawIndexedIndirect）。
// ============================================================

TEST(IndirectMeshPixelSmokeTest, OpenGLIndirectQuads) {
    auto r = dse::test::RunOpenGL(RenderIndirectMesh);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbrInstanced builtin program unavailable (OpenGL)";
    VerifyTwoInstances(r.readback, "OpenGL");
}

TEST(IndirectMeshPixelSmokeTest, D3D11IndirectQuads) {
    auto r = dse::test::RunD3D11(RenderIndirectMesh);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbrInstanced builtin program unavailable (D3D11)";
    VerifyTwoInstances(r.readback, "D3D11");
}

TEST(IndirectMeshPixelSmokeTest, VulkanIndirectQuads) {
    auto r = dse::test::RunVulkan(RenderIndirectMesh);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardPbrInstanced builtin program unavailable (Vulkan)";
    VerifyTwoInstances(r.readback, "Vulkan");
}

// ============================================================
// 跨后端一致性：场景沿 y=128 对称，DX11 垂直翻转不影响整帧 RMSE。
// ============================================================

TEST(IndirectMeshPixelSmokeTest, CrossBackendConsistent) {
    auto gl = dse::test::RunOpenGL(RenderIndirectMesh);
    auto dx = dse::test::RunD3D11(RenderIndirectMesh);
    auto vk = dse::test::RunVulkan(RenderIndirectMesh);
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
