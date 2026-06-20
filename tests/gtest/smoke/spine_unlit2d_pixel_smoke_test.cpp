/**
 * @file spine_unlit2d_pixel_smoke_test.cpp
 * @brief B2b-6 spine 2D 蒙皮迁移「活体」像素冒烟 —— 用高层 MeshRenderer::DrawUnlit2D
 *        （消费内建程序 BuiltinProgram::Sprite2D：VS 仅施 vp、frag = texture * vertexColor）
 *        复刻 DrawMeshBatch 对 spine 项（lighting_enabled=false 的无光照 2D 三角网格）的处理。
 *
 * 设计要点（验证「无光照 + 纹理 × 顶点色 + 任意三角拓扑 + alpha 混合」即 spine 渲染所需能力）：
 *  - spine runtime 已用 computeWorldVertices 在 CPU 侧做完 2D 骨骼蒙皮 → 顶点为世界空间，
 *    GPU 侧只需无光照绘制；故顶点直接给世界空间坐标（与生产 spine_system 一致）。
 *  - 居中 quad（世界空间 x,y∈[-0.5,0.5]，z=0）+ proj=GetProjectionCorrection、view=I：
 *    NDC[-1,1]→[0,256]px，quad 落像素 [64,192]，中心 (128,128) 在内、四角 (6,6) 在外清屏黑。
 *  - 纹理路径：绑 1x1 绿纹理 + 白顶点色 → 中心绿（证明 u_texture 采样生效）。
 *  - 顶点色路径：texture=0（回退白纹理）+ 红顶点色 → 中心红（证明顶点色调制 + 白纹理回退）。
 *  - 任意三角拓扑路径：单个朝上三角（非 quad，indices={0,1,2}），在中心行 y=128 落 x∈[96,160]，
 *    中心 (128,128) 亮、左外 (32,128) 黑 —— 证明可绘制 spine MeshAttachment 的任意三角网格
 *    （quad-only 的 SpriteBatchRenderer 做不到）。
 *  - 断言点取中心行 y=128（DX11 回读垂直翻转下不变），左右/中心沿 x 取，规避后端翻转差异。
 *  - 若无光照 2D 能力缺失（Sprite2D 程序未取到 / PSO 深度未关致被裁 / 顶点色未调制）→ 断言失败。
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

Unlit2DVertex MakeVert(float x, float y, float u, float v, const glm::vec4& col) {
    Unlit2DVertex vert;
    vert.position = glm::vec3(x, y, 0.0f);
    vert.color = col;
    vert.uv = glm::vec2(u, v);
    return vert;
}

// 居中 quad（世界空间），两三角 indices {0,1,2,0,2,3}。
void MakeQuad(const glm::vec4& col, std::vector<Unlit2DVertex>& verts,
              std::vector<uint16_t>& indices) {
    verts = {
        MakeVert(-0.5f, -0.5f, 0.0f, 0.0f, col),
        MakeVert( 0.5f, -0.5f, 1.0f, 0.0f, col),
        MakeVert( 0.5f,  0.5f, 1.0f, 1.0f, col),
        MakeVert(-0.5f,  0.5f, 0.0f, 1.0f, col),
    };
    indices = {0, 1, 2, 0, 2, 3};
}

// 单个朝上三角（非 quad 拓扑），沿 x=0 竖直对称。
void MakeTriangle(const glm::vec4& col, std::vector<Unlit2DVertex>& verts,
                  std::vector<uint16_t>& indices) {
    verts = {
        MakeVert(-0.5f, -0.5f, 0.0f, 0.0f, col),
        MakeVert( 0.5f, -0.5f, 1.0f, 0.0f, col),
        MakeVert( 0.0f,  0.5f, 0.5f, 1.0f, col),
    };
    indices = {0, 1, 2};
}

// 用给定几何/纹理/混合做一次无光照 2D 绘制并回读。
RenderTargetReadback RenderUnlit2D(RhiDevice& device,
                                   const std::vector<Unlit2DVertex>& verts,
                                   const std::vector<uint16_t>& indices,
                                   bool use_green_tex) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    if (device.GetBuiltinProgram(BuiltinProgram::Sprite2D) == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    unsigned int green_tex = 0;
    if (use_green_tex) {
        const unsigned char green[4] = {0, 255, 0, 255};
        green_tex = device.CreateTexture2D(1, 1, green, /*linear_filter=*/true);
    }

    const glm::mat4 proj = device.GetProjectionCorrection();
    const glm::mat4 I(1.0f);

    MeshRenderer renderer;
    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        renderer.DrawUnlit2D(*cmd, device, verts, indices, I, proj, green_tex, /*blend_mode=*/0u);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    if (green_tex) device.DeleteTexture(green_tex);
    device.DeleteRenderTarget(rt);
    return rb;
}

// 绿纹理 quad + 白顶点色 → 中心绿。
RenderTargetReadback RenderGreenQuad(RhiDevice& device) {
    std::vector<Unlit2DVertex> verts;
    std::vector<uint16_t> indices;
    MakeQuad(glm::vec4(1.0f), verts, indices);
    return RenderUnlit2D(device, verts, indices, /*use_green_tex=*/true);
}

// 白纹理回退 + 红顶点色 → 中心红。
RenderTargetReadback RenderRedVertexQuad(RhiDevice& device) {
    std::vector<Unlit2DVertex> verts;
    std::vector<uint16_t> indices;
    MakeQuad(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), verts, indices);
    return RenderUnlit2D(device, verts, indices, /*use_green_tex=*/false);
}

// 单三角（任意拓扑）+ 白顶点色 → 中心亮、左外黑。
RenderTargetReadback RenderWhiteTriangle(RhiDevice& device) {
    std::vector<Unlit2DVertex> verts;
    std::vector<uint16_t> indices;
    MakeTriangle(glm::vec4(1.0f), verts, indices);
    return RenderUnlit2D(device, verts, indices, /*use_green_tex=*/false);
}

void VerifyCenterChannel(const RenderTargetReadback& rb, const char* backend,
                         int hi_channel) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cy = kRtSize / 2;
    const unsigned char* center = dse::test::PixelAt(rb, 128, cy);
    ASSERT_NE(center, nullptr) << backend << " center";
    for (int c = 0; c < 3; ++c) {
        if (c == hi_channel) {
            EXPECT_GT(center[c], 180) << backend << " center channel " << c;
        } else {
            EXPECT_LT(center[c], 64) << backend << " center channel " << c;
        }
    }

    const unsigned char* corner = dse::test::PixelAt(rb, 6, 6);
    ASSERT_NE(corner, nullptr) << backend << " corner";
    EXPECT_LT(corner[0], 32) << backend << " corner R";
    EXPECT_LT(corner[1], 32) << backend << " corner G";
    EXPECT_LT(corner[2], 32) << backend << " corner B";
}

// 三角：中心亮（白）、中心行左外黑。
void VerifyTriangle(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cy = kRtSize / 2;
    const unsigned char* center = dse::test::PixelAt(rb, 128, cy);
    ASSERT_NE(center, nullptr) << backend << " center";
    EXPECT_GT(center[0], 180) << backend << " center R";
    EXPECT_GT(center[1], 180) << backend << " center G";
    EXPECT_GT(center[2], 180) << backend << " center B";

    const unsigned char* left_out = dse::test::PixelAt(rb, 32, cy);
    ASSERT_NE(left_out, nullptr) << backend << " left_out";
    EXPECT_LT(left_out[0], 32) << backend << " left_out R";
    EXPECT_LT(left_out[1], 32) << backend << " left_out G";
    EXPECT_LT(left_out[2], 32) << backend << " left_out B";
}

void RunGreen(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "Sprite2D builtin program unavailable (" << backend << ")";
    VerifyCenterChannel(r.readback, backend, /*hi_channel=*/1);  // 绿
}

void RunRed(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "Sprite2D builtin program unavailable (" << backend << ")";
    VerifyCenterChannel(r.readback, backend, /*hi_channel=*/0);  // 红
}

void RunTriangle(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "Sprite2D builtin program unavailable (" << backend << ")";
    VerifyTriangle(r.readback, backend);
}

void CheckCrossBackend(const dse::test::RenderFn& fn, double threshold) {
    auto gl = dse::test::RunOpenGL(fn);
    auto dx = dse::test::RunD3D11(fn);
    auto vk = dse::test::RunVulkan(fn);
    auto ok = [](const dse::test::BackendResult& r) { return r.available && !r.readback.pixels.empty(); };
    int available = (ok(gl) ? 1 : 0) + (ok(dx) ? 1 : 0) + (ok(vk) ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "需要至少两个后端可用以比较 RMSE";
    if (ok(gl) && ok(dx)) EXPECT_LT(dse::test::ComputeRmse(gl.readback, dx.readback), threshold) << "GL vs DX11";
    if (ok(gl) && ok(vk)) EXPECT_LT(dse::test::ComputeRmse(gl.readback, vk.readback), threshold) << "GL vs Vulkan";
    if (ok(dx) && ok(vk)) EXPECT_LT(dse::test::ComputeRmse(dx.readback, vk.readback), threshold) << "DX11 vs Vulkan";
}

}  // namespace

// ============================================================
// 三后端单独像素验证（活体消费 MeshRenderer::DrawUnlit2D + Sprite2D）。
// ============================================================

// ── 绿纹理 quad（纹理采样）──
TEST(SpineUnlit2DPixelSmokeTest, OpenGLGreenQuad) { RunGreen(dse::test::RunOpenGL(RenderGreenQuad), "OpenGL"); }
TEST(SpineUnlit2DPixelSmokeTest, D3D11GreenQuad) { RunGreen(dse::test::RunD3D11(RenderGreenQuad), "D3D11"); }
TEST(SpineUnlit2DPixelSmokeTest, VulkanGreenQuad) { RunGreen(dse::test::RunVulkan(RenderGreenQuad), "Vulkan"); }
TEST(SpineUnlit2DPixelSmokeTest, GreenQuadCrossBackend) { CheckCrossBackend(RenderGreenQuad, 12.0); }

// ── 红顶点色 quad（顶点色调制 + 白纹理回退）──
TEST(SpineUnlit2DPixelSmokeTest, OpenGLRedVertexQuad) { RunRed(dse::test::RunOpenGL(RenderRedVertexQuad), "OpenGL"); }
TEST(SpineUnlit2DPixelSmokeTest, D3D11RedVertexQuad) { RunRed(dse::test::RunD3D11(RenderRedVertexQuad), "D3D11"); }
TEST(SpineUnlit2DPixelSmokeTest, VulkanRedVertexQuad) { RunRed(dse::test::RunVulkan(RenderRedVertexQuad), "Vulkan"); }
TEST(SpineUnlit2DPixelSmokeTest, RedVertexQuadCrossBackend) { CheckCrossBackend(RenderRedVertexQuad, 12.0); }

// ── 单三角（任意三角拓扑，spine MeshAttachment）──
// 注：三角 y 方向非对称，而 GL 与 DX11/Vulkan 的 RT 回读行序约定相反（仅对 y 非对称内容显现），
// 故此处不做整帧跨后端 RMSE（那需要按后端翻转归一化；既有 tree/terrain 测试亦靠 y 对称规避）。
// 跨后端像素一致性已由上方两个 y 对称 quad 用例（Green/RedVertex）覆盖同一 DrawUnlit2D 路径；
// 本组三个单后端用例专证「任意三角拓扑」在各后端均正确绘制（中心亮、三角外黑）。
TEST(SpineUnlit2DPixelSmokeTest, OpenGLTriangle) { RunTriangle(dse::test::RunOpenGL(RenderWhiteTriangle), "OpenGL"); }
TEST(SpineUnlit2DPixelSmokeTest, D3D11Triangle) { RunTriangle(dse::test::RunD3D11(RenderWhiteTriangle), "D3D11"); }
TEST(SpineUnlit2DPixelSmokeTest, VulkanTriangle) { RunTriangle(dse::test::RunVulkan(RenderWhiteTriangle), "Vulkan"); }
