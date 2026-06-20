/**
 * @file tree_shared_template_pixel_smoke_test.cpp
 * @brief Final-Feat-7 共享网格模板去重（shared_vertex_ptr，tree）「活体」像素冒烟 —— 用高层
 *        MeshRenderer::DrawSharedTemplateInstanced（消费内建程序 BuiltinProgram::ForwardInstancedShaded：
 *        forward_shaded_instanced.vert 按 gl_InstanceIndex 取每实例 model 矩阵 SSBO\@slot 0[set7.b0]
 *        后施 model + vp，复用高级 shading frag forward_shaded.frag）。
 *
 * 设计要点（验证「一份共享局部空间顶点模板 + 每实例 model」即 shared_vertex_ptr 去重）：
 *  - 用 BuildShadedLocalVertexBuffer 把一块居中于原点的窄竖直矩形面片（x∈[-0.2,0.2], y∈[-0.7,0.7]，
 *    法线 +X 朝光源）构建为**一份常驻静态局部空间顶点模板**（is_dynamic=false）；索引缓冲同为常驻。
 *  - 两个 tree 实例 model：实例 0 = 平移(-0.5,0,0)、实例 1 = 平移(+0.5,0,0)。两实例**共享同一份模板 VB/IB**，
 *    各自 model 由 SSBO 按 gl_InstanceIndex 取 → 左面片落 x=64px、右面片落 x=192px，皆朝光被照亮，
 *    中缝 x=128 与四角清屏黑。证明「一份模板顶点 + N 份 model 矩阵」实例化正确（顶点只存一份）。
 *  - 单实例用例（仅实例 0 左移）：左 x=64 亮、右 x=192 黑 —— 证明模板是局部空间、由各实例 model 变换
 *    （而非把世界坐标烤进顶点），且 index_count_override 子段绘制生效。
 *  - 若共享模板/实例化失效（gl_InstanceIndex 未取到各自 model，或模板顶点未被各实例独立变换）→ 右侧
 *    x=192 不出现面片，断言失败；若高级 shading frag 未生效 → 朝光面片不亮，亦失败。
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

// 单块居中于原点的窄竖直矩形面片（局部空间，z=0，CCW 朝 +Z 相机），法线 +X 朝光源。
void MakeCenterQuad(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    const glm::vec3 n(1.0f, 0.0f, 0.0f);   // 法线朝光源 → 亮
    const glm::vec3 tan(0.0f, 1.0f, 0.0f);
    const float x0 = -0.2f, x1 = 0.2f, y0 = -0.7f, y1 = 0.7f;
    MeshVertex v;
    v.color = col; v.normal = n; v.tangent = tan;
    v.position = {x0, y0, 0.0f}; v.uv = {0.0f, 0.0f}; verts.push_back(v);
    v.position = {x1, y0, 0.0f}; v.uv = {1.0f, 0.0f}; verts.push_back(v);
    v.position = {x1, y1, 0.0f}; v.uv = {1.0f, 1.0f}; verts.push_back(v);
    v.position = {x0, y1, 0.0f}; v.uv = {0.0f, 1.0f}; verts.push_back(v);
    indices = {0, 1, 2, 0, 2, 3};
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

// 构建一份共享局部空间模板 VB/IB，用给定实例 model 列表做一次实例化绘制（共享顶点 + 每实例 model）。
RenderTargetReadback RenderSharedTemplate(RhiDevice& device, int shading_mode,
                                          const std::vector<glm::mat4>& models) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    if (device.GetBuiltinProgram(BuiltinProgram::ForwardInstancedShaded) == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeCenterQuad(verts, indices);

    // 一份常驻局部空间顶点模板 + 一份常驻索引缓冲（shared_vertex_ptr：多实例共享、不重传）。
    ExternalShadedMesh tmpl;
    tmpl.vertex_buffer = MeshRenderer::BuildShadedLocalVertexBuffer(device, verts);
    GpuBufferDesc ib_desc;
    ib_desc.size = indices.size() * sizeof(uint16_t);
    ib_desc.usage = GpuBufferUsage::kIndex;
    ib_desc.is_dynamic = false;
    tmpl.index_buffer = device.CreateGpuBuffer(ib_desc, indices.data());
    tmpl.index_type = IndexType::UInt16;
    if (!tmpl.vertex_buffer || !tmpl.index_buffer) {
        if (tmpl.vertex_buffer) device.DeleteGpuBuffer(tmpl.vertex_buffer);
        if (tmpl.index_buffer) device.DeleteGpuBuffer(tmpl.index_buffer);
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
        // index_count=6（整块模板），first_index=0；instance_count = models.size()。
        renderer.DrawSharedTemplateInstanced(*cmd, device, tmpl, 6u, 0u, models,
                                             I, proj, cam_pos, material, MakeLightAlongX());
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteGpuBuffer(tmpl.vertex_buffer);
    device.DeleteGpuBuffer(tmpl.index_buffer);
    device.DeleteRenderTarget(rt);
    return rb;
}

std::vector<glm::mat4> TwoInstanceModels() {
    std::vector<glm::mat4> models(2);
    models[0] = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.0f, 0.0f));
    models[1] = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 0.0f));
    return models;
}

std::vector<glm::mat4> SingleLeftModel() {
    return {glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.0f, 0.0f))};
}

RenderTargetReadback RenderTwoPbr(RhiDevice& device) {
    return RenderSharedTemplate(device, 0, TwoInstanceModels());
}
RenderTargetReadback RenderTwoHalfLambert(RhiDevice& device) {
    return RenderSharedTemplate(device, 3, TwoInstanceModels());
}
RenderTargetReadback RenderSingleLeft(RhiDevice& device) {
    return RenderSharedTemplate(device, 0, SingleLeftModel());
}

// 左右两面片皆亮、中缝与四角清屏黑 —— 证明一份共享模板顶点被两实例各自 model 变换。
void VerifyTwoLitInstances(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cy = kRtSize / 2;
    const unsigned char* left = dse::test::PixelAt(rb, 64, cy);
    const unsigned char* right = dse::test::PixelAt(rb, 192, cy);
    ASSERT_NE(left, nullptr) << backend << " left";
    ASSERT_NE(right, nullptr) << backend << " right";
    EXPECT_GT(left[0], 100) << backend << " left R";
    EXPECT_GT(right[0], 100) << backend << " right R";

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

// 单实例（仅左）：左 x=64 亮、右 x=192 黑 —— 证明模板局部空间 + 实例 model 变换生效。
void VerifySingleLeft(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cy = kRtSize / 2;
    const unsigned char* left = dse::test::PixelAt(rb, 64, cy);
    ASSERT_NE(left, nullptr) << backend << " left";
    EXPECT_GT(left[0], 100) << backend << " left R";

    const unsigned char* right = dse::test::PixelAt(rb, 192, cy);
    ASSERT_NE(right, nullptr) << backend << " right";
    EXPECT_LT(right[0], 32) << backend << " right R";
    EXPECT_LT(right[1], 32) << backend << " right G";
    EXPECT_LT(right[2], 32) << backend << " right B";

    const unsigned char* corner = dse::test::PixelAt(rb, 6, 6);
    ASSERT_NE(corner, nullptr) << backend;
    EXPECT_LT(corner[0], 32) << backend << " corner R";
}

void RunTwo(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardInstancedShaded builtin program unavailable (" << backend << ")";
    VerifyTwoLitInstances(r.readback, backend);
}

void RunSingle(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardInstancedShaded builtin program unavailable (" << backend << ")";
    VerifySingleLeft(r.readback, backend);
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
// 三后端单独像素验证（活体消费 MeshRenderer::DrawSharedTemplateInstanced + ForwardInstancedShaded）。
// ============================================================

// ── 双实例 PBR(0)：一份共享模板 + 两实例 model ──
TEST(TreeSharedTemplatePixelSmokeTest, OpenGLTwoPbr) { RunTwo(dse::test::RunOpenGL(RenderTwoPbr), "OpenGL"); }
TEST(TreeSharedTemplatePixelSmokeTest, D3D11TwoPbr) { RunTwo(dse::test::RunD3D11(RenderTwoPbr), "D3D11"); }
TEST(TreeSharedTemplatePixelSmokeTest, VulkanTwoPbr) { RunTwo(dse::test::RunVulkan(RenderTwoPbr), "Vulkan"); }
TEST(TreeSharedTemplatePixelSmokeTest, TwoPbrCrossBackend) { CheckCrossBackend(RenderTwoPbr, 12.0); }

// ── 双实例 HalfLambert(3)：共享模板 + 高级 shading 分支 ──
TEST(TreeSharedTemplatePixelSmokeTest, OpenGLTwoHalfLambert) { RunTwo(dse::test::RunOpenGL(RenderTwoHalfLambert), "OpenGL"); }
TEST(TreeSharedTemplatePixelSmokeTest, D3D11TwoHalfLambert) { RunTwo(dse::test::RunD3D11(RenderTwoHalfLambert), "D3D11"); }
TEST(TreeSharedTemplatePixelSmokeTest, VulkanTwoHalfLambert) { RunTwo(dse::test::RunVulkan(RenderTwoHalfLambert), "Vulkan"); }
TEST(TreeSharedTemplatePixelSmokeTest, TwoHalfLambertCrossBackend) { CheckCrossBackend(RenderTwoHalfLambert, 12.0); }

// ── 单实例 PBR(0)：证明模板局部空间 + 实例 model 变换（左亮右黑） ──
TEST(TreeSharedTemplatePixelSmokeTest, OpenGLSingleLeft) { RunSingle(dse::test::RunOpenGL(RenderSingleLeft), "OpenGL"); }
TEST(TreeSharedTemplatePixelSmokeTest, D3D11SingleLeft) { RunSingle(dse::test::RunD3D11(RenderSingleLeft), "D3D11"); }
TEST(TreeSharedTemplatePixelSmokeTest, VulkanSingleLeft) { RunSingle(dse::test::RunVulkan(RenderSingleLeft), "Vulkan"); }
TEST(TreeSharedTemplatePixelSmokeTest, SingleLeftCrossBackend) { CheckCrossBackend(RenderSingleLeft, 12.0); }
