/**
 * @file gbuffer_mesh_pixel_smoke_test.cpp
 * @brief 阶段4-M3 MeshRenderer GBuffer 几何通道（MRT）「活体」像素冒烟 ——
 *        用高层 MeshRenderer::DrawGBuffer（BuiltinProgram::GBufferMesh = forward_pbr.vert + gbuffer.frag）
 *        向一个 3 颜色附件 MRT RenderTarget 绘制一块面片，逐个 attachment 回读校验：
 *          - attachment0 gAlbedo   = texColor × vColor（顶点色 (0.8,0.2,0.1) → 约 (204,51,26)）
 *          - attachment1 gNormal   = normalize(N)×0.5+0.5（法线 +Z → (0.5,0.5,1) → (128,128,255)）
 *          - attachment2 gPosition = 世界坐标（面片 z=0.5 常量 → B≈128；x 随屏幕左→右增大）
 *        证明 MRT 三附件各自写入了正确且互不相同的几何数据（取代 DrawMeshBatch 的延迟几何输出，
 *        供 ShadowRSMPass→DDGIUpdatePass 生成 VPL）。
 *
 * 各 attachment 经 PostProcessRenderer "copy" 全屏拷贝到单附件 dst RT 后回读（与 dx11_rhi_smoke_test
 * 的 draw-call 级回读同构）—— 因 ReadRenderTargetColorRgba8WithSize 仅回读 attachment0。
 *
 * 设计要点：
 *  - 面片**法线 +Z**、顶点色 (0.8,0.2,0.1,1)、**z=0.5 常量**（gPosition.z 直接取世界 z，与投影无关）。
 *  - proj 含各后端裁剪修正（GetProjectionCorrection）。
 *  - 本机仅 D3D11(WARP) 实跑；GL/Vulkan 无驱动 skip（既有状况，非回归）。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/mesh_renderer.h"
#include "engine/render/post_process_renderer.h"
#include "engine/render/rhi/postprocess_common.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 一块矩形面片（局部=世界空间，**z=0.5 常量**，CCW 朝 +Z 相机）。法线 +Z，顶点色 (0.8,0.2,0.1,1)。
void BuildQuad(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const float z = 0.5f;
    const glm::vec3 n(0.0f, 0.0f, 1.0f);
    const glm::vec4 col(0.8f, 0.2f, 0.1f, 1.0f);
    MeshVertex v;
    v.color = col;
    v.normal = n;
    v.tangent = glm::vec3(0.0f, 1.0f, 0.0f);
    v.position = {-0.6f, -0.6f, z}; v.uv = {0.0f, 0.0f}; verts.push_back(v);
    v.position = { 0.6f, -0.6f, z}; v.uv = {1.0f, 0.0f}; verts.push_back(v);
    v.position = { 0.6f,  0.6f, z}; v.uv = {1.0f, 1.0f}; verts.push_back(v);
    v.position = {-0.6f,  0.6f, z}; v.uv = {0.0f, 1.0f}; verts.push_back(v);
    indices = {0, 1, 2, 0, 2, 3};
}

std::vector<MeshVertex> g_verts;
std::vector<uint16_t> g_indices;
void EnsureGeom() {
    if (g_verts.empty()) BuildQuad(g_verts, g_indices);
}

// 绘制 GBuffer 到 3 附件 MRT，再用 "copy" 把指定 attachment 拷到单附件 dst 后回读。
RenderTargetReadback RenderGBufferAttachment(RhiDevice& device, int attachment) {
    EnsureGeom();

    RenderTargetDesc mrt_desc;
    mrt_desc.width = kRtSize;
    mrt_desc.height = kRtSize;
    mrt_desc.has_color = true;
    mrt_desc.has_depth = true;
    mrt_desc.color_attachment_count = 3;  // gAlbedo / gNormal / gPosition
    unsigned int mrt = device.CreateRenderTarget(mrt_desc);

    RenderTargetDesc dst_desc;
    dst_desc.width = kRtSize;
    dst_desc.height = kRtSize;
    dst_desc.has_color = true;
    dst_desc.has_depth = false;
    unsigned int dst = device.CreateRenderTarget(dst_desc);

    if (mrt == 0 || dst == 0 ||
        device.GetBuiltinProgram(BuiltinProgram::GBufferMesh) == 0) {
        if (mrt) device.DeleteRenderTarget(mrt);
        if (dst) device.DeleteRenderTarget(dst);
        return {};
    }

    const glm::mat4 proj = device.GetProjectionCorrection();
    const glm::mat4 view(1.0f);
    const glm::mat4 model(1.0f);

    MeshRenderer renderer;
    PostProcessRenderer pp;
    device.BeginFrame();
    pp.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        // pass 1：GBuffer 几何 → MRT（3 附件 + 深度）
        RenderPassDesc rp;
        rp.render_target = mrt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        renderer.DrawGBuffer(*cmd, device, g_verts, g_indices, model, view, proj, /*albedo_tex=*/0);
        cmd->EndRenderPass();

        // pass 2：把目标 attachment 全屏拷到 dst（单附件）后回读
        RenderPassDesc rp2;
        rp2.render_target = dst;
        rp2.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp2.clear_color_enabled = true;
        cmd->BeginRenderPass(rp2);
        pp.Draw(*cmd, device,
                PostProcessRequest("copy", device.GetRenderTargetColorTexture(mrt, attachment)));
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(dst);
    renderer.Shutdown(device);
    pp.Shutdown(device);
    device.DeleteRenderTarget(mrt);
    device.DeleteRenderTarget(dst);
    return rb;
}

RenderTargetReadback RenderGBufferAlbedo(RhiDevice& device) { return RenderGBufferAttachment(device, 0); }
RenderTargetReadback RenderGBufferNormal(RhiDevice& device) { return RenderGBufferAttachment(device, 1); }
RenderTargetReadback RenderGBufferPosition(RhiDevice& device) { return RenderGBufferAttachment(device, 2); }

bool Near(int a, int b, int tol) { return (a > b ? a - b : b - a) <= tol; }

}  // namespace

// ============================================================
// attachment0 gAlbedo = texColor × vColor：面片区 ≈ (204,51,26)，背景 (0,0,0)。
// ============================================================
void RunAlbedo(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "GBufferMesh builtin program unavailable (" << backend << ")";
    const unsigned char* c = dse::test::PixelAt(r.readback, kRtSize / 2, kRtSize / 2);
    const unsigned char* bg = dse::test::PixelAt(r.readback, 6, 6);
    ASSERT_NE(c, nullptr) << backend;
    ASSERT_NE(bg, nullptr) << backend;
    EXPECT_TRUE(Near(c[0], 204, 12)) << backend << " gAlbedo.r≈204 got " << int(c[0]);
    EXPECT_TRUE(Near(c[1], 51, 12)) << backend << " gAlbedo.g≈51 got " << int(c[1]);
    EXPECT_TRUE(Near(c[2], 26, 12)) << backend << " gAlbedo.b≈26 got " << int(c[2]);
    EXPECT_LT(bg[0] + bg[1] + bg[2], 12) << backend << " background dark";
}
TEST(GBufferMeshPixelSmokeTest, OpenGLAlbedo) { RunAlbedo(dse::test::RunOpenGL(RenderGBufferAlbedo), "OpenGL"); }
TEST(GBufferMeshPixelSmokeTest, D3D11Albedo) { RunAlbedo(dse::test::RunD3D11(RenderGBufferAlbedo), "D3D11"); }
TEST(GBufferMeshPixelSmokeTest, VulkanAlbedo) { RunAlbedo(dse::test::RunVulkan(RenderGBufferAlbedo), "Vulkan"); }

// ============================================================
// attachment1 gNormal = normalize(N)×0.5+0.5：法线 +Z → (128,128,255)，背景 (0,0,0)。
// ============================================================
void RunNormal(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "GBufferMesh builtin program unavailable (" << backend << ")";
    const unsigned char* c = dse::test::PixelAt(r.readback, kRtSize / 2, kRtSize / 2);
    const unsigned char* bg = dse::test::PixelAt(r.readback, 6, 6);
    ASSERT_NE(c, nullptr) << backend;
    ASSERT_NE(bg, nullptr) << backend;
    EXPECT_TRUE(Near(c[0], 128, 12)) << backend << " gNormal.r≈128 got " << int(c[0]);
    EXPECT_TRUE(Near(c[1], 128, 12)) << backend << " gNormal.g≈128 got " << int(c[1]);
    EXPECT_GT(c[2], 235) << backend << " gNormal.b≈255 (N=+Z) got " << int(c[2]);
    EXPECT_LT(bg[0] + bg[1] + bg[2], 12) << backend << " background dark";
}
TEST(GBufferMeshPixelSmokeTest, OpenGLNormal) { RunNormal(dse::test::RunOpenGL(RenderGBufferNormal), "OpenGL"); }
TEST(GBufferMeshPixelSmokeTest, D3D11Normal) { RunNormal(dse::test::RunD3D11(RenderGBufferNormal), "D3D11"); }
TEST(GBufferMeshPixelSmokeTest, VulkanNormal) { RunNormal(dse::test::RunVulkan(RenderGBufferNormal), "Vulkan"); }

// ============================================================
// attachment2 gPosition = 世界坐标：z=0.5 常量 → 中心 B≈128；x 随屏幕左→右增大（R 梯度），背景 0。
// ============================================================
void RunPosition(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "GBufferMesh builtin program unavailable (" << backend << ")";
    const unsigned char* center = dse::test::PixelAt(r.readback, kRtSize / 2, kRtSize / 2);
    const unsigned char* left = dse::test::PixelAt(r.readback, 56, kRtSize / 2);   // 世界 x<0 → R clamp 0
    const unsigned char* right = dse::test::PixelAt(r.readback, 200, kRtSize / 2); // 世界 x>0 → R>0
    const unsigned char* bg = dse::test::PixelAt(r.readback, 6, 6);
    ASSERT_NE(center, nullptr) << backend;
    ASSERT_NE(left, nullptr) << backend;
    ASSERT_NE(right, nullptr) << backend;
    ASSERT_NE(bg, nullptr) << backend;
    EXPECT_TRUE(Near(center[2], 128, 16)) << backend << " gPosition.z=0.5 → B≈128 got " << int(center[2]);
    EXPECT_GT(static_cast<int>(right[0]) - static_cast<int>(left[0]), 40)
        << backend << " gPosition.x grows left→right (world-space x)";
    EXPECT_LT(bg[2], 8) << backend << " background z=0";
}
TEST(GBufferMeshPixelSmokeTest, OpenGLPosition) { RunPosition(dse::test::RunOpenGL(RenderGBufferPosition), "OpenGL"); }
TEST(GBufferMeshPixelSmokeTest, D3D11Position) { RunPosition(dse::test::RunD3D11(RenderGBufferPosition), "D3D11"); }
TEST(GBufferMeshPixelSmokeTest, VulkanPosition) { RunPosition(dse::test::RunVulkan(RenderGBufferPosition), "Vulkan"); }
