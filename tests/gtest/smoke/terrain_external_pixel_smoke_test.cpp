/**
 * @file terrain_external_pixel_smoke_test.cpp
 * @brief Final-Feat-6「外部常驻 VAO/EBO + index_count_override」（tiled terrain）「活体」像素冒烟 ——
 *        用高层 MeshRenderer::DrawShadedExternal 消费内建程序 BuiltinProgram::ForwardShaded：
 *        顶点/索引来自**调用方持有的常驻 GPU 缓冲**（MeshRenderer::BuildShadedWorldVertexBuffer 构建
 *        的世界空间 GpuMeshVertex 顶点 + 调用方建的索引缓冲），按 [first_index, first_index+index_count)
 *        子段 DrawIndexed —— 即一份共享地形缓冲、每 tile 画各自索引子段、零每帧顶点重传。
 *
 * 设计要点（验证「index_count_override 真按子段绘制」+「多 tile 共享同一对 VB/IB」+ 高级 shading）：
 *  - 一份共享顶点缓冲含两片竖直矩形面片（局部=世界，model=I）：
 *      左片 verts 0-3，x∈[-0.7,-0.3]；右片 verts 4-7，x∈[0.3,0.7]；皆 y∈[-0.7,0.7]，法线 +X 朝光源。
 *  - 一份共享索引缓冲含两片三角形（12 个 uint16）：左片 [0..5]={0,1,2,0,2,3}，右片 [6..11]={4,5,6,4,6,7}。
 *  - 左 tile：DrawShadedExternal(index_count=6, first_index=0) → 仅左片亮（x=64），右半 x=192 清屏黑。
 *  - 右 tile：DrawShadedExternal(index_count=6, first_index=6) → 仅右片亮（x=192），左半 x=64 黑。
 *    （若 first_index/index_count 被忽略而画了整缓冲 → 两侧都亮，断言失败 → 证伪 override。）
 *  - 双 tile：同一对 VB/IB 上连画两次（offsets 0 与 6）→ 左右皆亮、中缝 x=128 黑 → 证多 tile 共享缓冲。
 *  - 法线恒 +X，左右受光等同 → 纯验证「子段几何选择」，不受光照位置耦合干扰。
 *  - 场景沿 y=128 对称（仅沿 X 变化），DX11 回读垂直翻转不影响整帧 RMSE。
 *  - proj 含各后端裁剪修正（GetProjectionCorrection），否则 Vulkan 背面剔除致全黑。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/mesh_renderer.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <utility>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 两片竖直矩形面片（世界空间，z=0，CCW 朝 +Z 相机），法线 +X 朝光源。
// 左片 verts 0-3，右片 verts 4-7；索引左片 {0,1,2,0,2,3}、右片 {4,5,6,4,6,7}。
void MakeTwoTiles(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    const glm::vec3 n(1.0f, 0.0f, 0.0f);   // 法线朝光源 → 亮
    const glm::vec3 tan(0.0f, 1.0f, 0.0f);
    const float y0 = -0.7f, y1 = 0.7f;
    auto push_quad = [&](float x0, float x1) {
        MeshVertex v;
        v.color = col; v.normal = n; v.tangent = tan;
        v.position = {x0, y0, 0.0f}; v.uv = {0.0f, 0.0f}; verts.push_back(v);
        v.position = {x1, y0, 0.0f}; v.uv = {1.0f, 0.0f}; verts.push_back(v);
        v.position = {x1, y1, 0.0f}; v.uv = {1.0f, 1.0f}; verts.push_back(v);
        v.position = {x0, y1, 0.0f}; v.uv = {0.0f, 1.0f}; verts.push_back(v);
    };
    push_quad(-0.7f, -0.3f);   // 左片 verts 0-3
    push_quad(0.3f, 0.7f);     // 右片 verts 4-7
    indices = {0, 1, 2, 0, 2, 3,    // 左 tile：first_index=0
               4, 5, 6, 4, 6, 7};   // 右 tile：first_index=6
}

DirectionalLight MakeLightAlongX() {
    DirectionalLight light;
    light.direction = glm::vec3(-1.0f, 0.0f, 0.0f);  // to_light = +X
    light.color = glm::vec3(1.0f);
    light.intensity = 3.0f;
    light.ambient = 0.05f;
    light.enabled = true;
    return light;
}

// 在共享的外部 VB/IB 上按给定 (first_index, index_count) 子段列表逐个绘制。
RenderTargetReadback RenderTiles(RhiDevice& device, int shading_mode,
                                 const std::vector<std::pair<uint32_t, uint32_t>>& draws) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    if (device.GetBuiltinProgram(BuiltinProgram::ForwardShaded) == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeTwoTiles(verts, indices);

    // 调用方持有的常驻外部缓冲：世界空间顶点（model=I）+ 索引缓冲（皆 is_dynamic=false 静态）。
    ExternalShadedMesh mesh;
    mesh.vertex_buffer = MeshRenderer::BuildShadedWorldVertexBuffer(device, verts, glm::mat4(1.0f));
    GpuBufferDesc ib_desc;
    ib_desc.size = indices.size() * sizeof(uint16_t);
    ib_desc.usage = GpuBufferUsage::kIndex;
    ib_desc.is_dynamic = false;
    mesh.index_buffer = device.CreateGpuBuffer(ib_desc, indices.data());
    mesh.index_type = IndexType::UInt16;
    if (!mesh.vertex_buffer || !mesh.index_buffer) {
        if (mesh.vertex_buffer) device.DeleteGpuBuffer(mesh.vertex_buffer);
        if (mesh.index_buffer) device.DeleteGpuBuffer(mesh.index_buffer);
        device.DeleteRenderTarget(rt);
        return {};
    }

    ShadedMaterial material;
    material.albedo = glm::vec3(0.8f);
    material.metallic = 0.0f;
    material.roughness = 0.5f;
    material.ao = 1.0f;
    material.shading_mode = shading_mode;

    const glm::mat4 proj = device.GetProjectionCorrection();
    const glm::mat4 I(1.0f);
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
        for (const auto& d : draws) {
            renderer.DrawShadedExternal(*cmd, device, mesh, d.second, d.first,
                                        I, proj, cam_pos, material, MakeLightAlongX());
        }
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteGpuBuffer(mesh.vertex_buffer);
    device.DeleteGpuBuffer(mesh.index_buffer);
    device.DeleteRenderTarget(rt);
    return rb;
}

RenderTargetReadback RenderLeftTile(RhiDevice& device) {
    return RenderTiles(device, 0, {{0u, 6u}});
}
RenderTargetReadback RenderRightTile(RhiDevice& device) {
    return RenderTiles(device, 0, {{6u, 6u}});
}
RenderTargetReadback RenderBothTiles(RhiDevice& device) {
    return RenderTiles(device, 0, {{0u, 6u}, {6u, 6u}});
}
RenderTargetReadback RenderBothTilesHalfLambert(RhiDevice& device) {
    return RenderTiles(device, 3, {{0u, 6u}, {6u, 6u}});
}

// 左片亮（x=64）、右半（x=192）清屏黑 —— index_count_override 仅画 first_index=0 子段。
void VerifyLeftOnly(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;
    const int cy = kRtSize / 2;
    const unsigned char* left = dse::test::PixelAt(rb, 64, cy);
    ASSERT_NE(left, nullptr) << backend;
    EXPECT_GT(left[0], 100) << backend << " left R";
    const unsigned char* right = dse::test::PixelAt(rb, 192, cy);
    ASSERT_NE(right, nullptr) << backend;
    EXPECT_LT(right[0], 32) << backend << " right R";
    EXPECT_LT(right[1], 32) << backend << " right G";
    EXPECT_LT(right[2], 32) << backend << " right B";
}

// 右片亮（x=192）、左半（x=64）黑 —— 仅画 first_index=6 子段。
void VerifyRightOnly(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;
    const int cy = kRtSize / 2;
    const unsigned char* right = dse::test::PixelAt(rb, 192, cy);
    ASSERT_NE(right, nullptr) << backend;
    EXPECT_GT(right[0], 100) << backend << " right R";
    const unsigned char* left = dse::test::PixelAt(rb, 64, cy);
    ASSERT_NE(left, nullptr) << backend;
    EXPECT_LT(left[0], 32) << backend << " left R";
    EXPECT_LT(left[1], 32) << backend << " left G";
    EXPECT_LT(left[2], 32) << backend << " left B";
}

// 双 tile：左右皆亮、中缝（x=128）与四角黑 —— 同一对外部 VB/IB 连画两个子段。
void VerifyBoth(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;
    const int cy = kRtSize / 2;
    const unsigned char* left = dse::test::PixelAt(rb, 64, cy);
    ASSERT_NE(left, nullptr) << backend;
    EXPECT_GT(left[0], 100) << backend << " left R";
    const unsigned char* right = dse::test::PixelAt(rb, 192, cy);
    ASSERT_NE(right, nullptr) << backend;
    EXPECT_GT(right[0], 100) << backend << " right R";
    const unsigned char* gap = dse::test::PixelAt(rb, 128, cy);
    ASSERT_NE(gap, nullptr) << backend;
    EXPECT_LT(gap[0], 32) << backend << " gap R";
    const unsigned char* corner = dse::test::PixelAt(rb, 6, 6);
    ASSERT_NE(corner, nullptr) << backend;
    EXPECT_LT(corner[0], 32) << backend << " corner R";
    EXPECT_LT(corner[1], 32) << backend << " corner G";
    EXPECT_LT(corner[2], 32) << backend << " corner B";
}

void RunSingleBackend(dse::test::BackendResult r, const char* backend,
                      void (*verify)(const RenderTargetReadback&, const char*)) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardShaded builtin program unavailable (" << backend << ")";
    verify(r.readback, backend);
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
// 三后端单独像素验证（活体消费 MeshRenderer::DrawShadedExternal + ForwardShaded）。
// ============================================================

// ── 左 tile：index_count=6, first_index=0 → 仅左片亮 ──
TEST(TerrainExternalPixelSmokeTest, OpenGLLeftTile) { RunSingleBackend(dse::test::RunOpenGL(RenderLeftTile), "OpenGL", VerifyLeftOnly); }
TEST(TerrainExternalPixelSmokeTest, D3D11LeftTile) { RunSingleBackend(dse::test::RunD3D11(RenderLeftTile), "D3D11", VerifyLeftOnly); }
TEST(TerrainExternalPixelSmokeTest, VulkanLeftTile) { RunSingleBackend(dse::test::RunVulkan(RenderLeftTile), "Vulkan", VerifyLeftOnly); }
TEST(TerrainExternalPixelSmokeTest, LeftTileCrossBackend) { CheckCrossBackend(RenderLeftTile, 12.0); }

// ── 右 tile：index_count=6, first_index=6 → 仅右片亮（证 first_index 偏移生效） ──
TEST(TerrainExternalPixelSmokeTest, OpenGLRightTile) { RunSingleBackend(dse::test::RunOpenGL(RenderRightTile), "OpenGL", VerifyRightOnly); }
TEST(TerrainExternalPixelSmokeTest, D3D11RightTile) { RunSingleBackend(dse::test::RunD3D11(RenderRightTile), "D3D11", VerifyRightOnly); }
TEST(TerrainExternalPixelSmokeTest, VulkanRightTile) { RunSingleBackend(dse::test::RunVulkan(RenderRightTile), "Vulkan", VerifyRightOnly); }
TEST(TerrainExternalPixelSmokeTest, RightTileCrossBackend) { CheckCrossBackend(RenderRightTile, 12.0); }

// ── 双 tile：同一对外部 VB/IB 连画两子段 → 左右皆亮、中缝黑（证多 tile 共享缓冲） ──
TEST(TerrainExternalPixelSmokeTest, OpenGLBothTiles) { RunSingleBackend(dse::test::RunOpenGL(RenderBothTiles), "OpenGL", VerifyBoth); }
TEST(TerrainExternalPixelSmokeTest, D3D11BothTiles) { RunSingleBackend(dse::test::RunD3D11(RenderBothTiles), "D3D11", VerifyBoth); }
TEST(TerrainExternalPixelSmokeTest, VulkanBothTiles) { RunSingleBackend(dse::test::RunVulkan(RenderBothTiles), "Vulkan", VerifyBoth); }
TEST(TerrainExternalPixelSmokeTest, BothTilesCrossBackend) { CheckCrossBackend(RenderBothTiles, 12.0); }

// ── 双 tile + HalfLambert-Static(3)：外部缓冲子段绘制 + 高级 shading 分支 ──
TEST(TerrainExternalPixelSmokeTest, OpenGLBothHalfLambert) { RunSingleBackend(dse::test::RunOpenGL(RenderBothTilesHalfLambert), "OpenGL", VerifyBoth); }
TEST(TerrainExternalPixelSmokeTest, D3D11BothHalfLambert) { RunSingleBackend(dse::test::RunD3D11(RenderBothTilesHalfLambert), "D3D11", VerifyBoth); }
TEST(TerrainExternalPixelSmokeTest, VulkanBothHalfLambert) { RunSingleBackend(dse::test::RunVulkan(RenderBothTilesHalfLambert), "Vulkan", VerifyBoth); }
TEST(TerrainExternalPixelSmokeTest, BothHalfLambertCrossBackend) { CheckCrossBackend(RenderBothTilesHalfLambert, 12.0); }
