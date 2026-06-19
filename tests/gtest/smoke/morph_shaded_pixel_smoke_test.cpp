/**
 * @file morph_shaded_pixel_smoke_test.cpp
 * @brief Final-Feat-5 Morph target + 高级 shading 组合「活体」像素冒烟 —— 用高层
 *        MeshRenderer::DrawMorphShaded（消费内建程序 BuiltinProgram::ForwardMorphShaded：
 *        forward_shaded_morph.vert 按 gl_VertexIndex 从 morph 增量 SSBO\@slot 0[set7.b0] 取本顶点
 *        增量，按 morph 权重 UBO\@slot 8[set7.b3] 加权求和后施 vp，复用高级 shading frag
 *        forward_shaded.frag —— shading_mode 0/2-6 + clustered 点光 + GI 等）。
 *
 * 设计要点（验证「形变实际位移顶点」+ 高级 shading + 双绑定 SSBO/UBO 正确）：
 *  - 基网格为居中偏左的窄竖直矩形面片（x∈[-0.7,-0.3], y∈[-0.7,0.7]），法线 +X（朝光源）。
 *    单独绘制（无形变）时面片落在左半（中心 x=64px）。
 *  - 单个 morph target：每顶点位置增量 = (+1.0, 0, 0)，weight=1.0 → 形变后面片整体右移到
 *    x∈[0.3,0.7]（中心 x=192px）。
 *  - 形变正确（SSBO\@slot0 取到增量 + UBO\@slot8 取到 weight）→ 右侧 x=192 出现亮面片、
 *    左侧 x=64 清屏黑；中缝 x=128 与四角亦黑。
 *  - 若 morph 失效（增量 SSBO 或权重 UBO 任一未绑定/读零）→ 面片停在左半 x=64，右侧 x=192 为黑，
 *    断言失败；若高级 shading frag 未生效 → 面片不亮，亦失败。
 *  - 法线恒 +X，左右位置受光等同 → 该用例纯验证「位置形变」，不受光照位置耦合干扰。
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

// 居中偏左的窄竖直矩形面片（局部空间，z=0，CCW 朝 +Z 相机），法线 +X 朝光源。
void MakeLeftQuad(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    const glm::vec3 n(1.0f, 0.0f, 0.0f);   // 法线朝光源 → 亮
    const glm::vec3 tan(0.0f, 1.0f, 0.0f);
    const float x0 = -0.7f, x1 = -0.3f, y0 = -0.7f, y1 = 0.7f;
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

RenderTargetReadback RenderMorphShaded(RhiDevice& device, int shading_mode) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    if (device.GetBuiltinProgram(BuiltinProgram::ForwardMorphShaded) == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeLeftQuad(verts, indices);

    // 单个 morph target：把所有顶点沿 +X 平移 1.0，weight=1.0 → 左半面片右移到右半。
    MeshMorphTarget target;
    target.position_deltas.assign(verts.size(), glm::vec3(1.0f, 0.0f, 0.0f));
    target.weight = 1.0f;
    std::vector<MeshMorphTarget> morph_targets = {target};

    ShadedMaterial material;
    material.albedo = glm::vec3(0.8f);
    material.metallic = 0.0f;
    material.roughness = 0.5f;
    material.ao = 1.0f;
    material.shading_mode = shading_mode;

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
        renderer.DrawMorphShaded(*cmd, device, verts, indices, morph_targets, I, I, proj, cam_pos,
                                 material, MakeLightAlongX());
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    return rb;
}

RenderTargetReadback RenderPbr(RhiDevice& device) { return RenderMorphShaded(device, 0); }
RenderTargetReadback RenderHalfLambert(RhiDevice& device) { return RenderMorphShaded(device, 3); }

// 右半亮、左半与四角清屏黑 —— 证明 morph 增量（SSBO\@slot0）+ 权重（UBO\@slot8）实际位移顶点。
void VerifyMorphedRight(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cy = kRtSize / 2;
    // 形变后面片落在右半（中心 x=192）且被照亮。
    const unsigned char* right = dse::test::PixelAt(rb, 192, cy);
    ASSERT_NE(right, nullptr) << backend << " right";
    EXPECT_GT(right[0], 100) << backend << " right R";

    // 左半（基网格原位 x=64）应为空 —— 顶点已被形变搬走。
    const unsigned char* left = dse::test::PixelAt(rb, 64, cy);
    ASSERT_NE(left, nullptr) << backend << " left";
    EXPECT_LT(left[0], 32) << backend << " left R";
    EXPECT_LT(left[1], 32) << backend << " left G";
    EXPECT_LT(left[2], 32) << backend << " left B";

    const unsigned char* gap = dse::test::PixelAt(rb, 128, cy);
    ASSERT_NE(gap, nullptr) << backend;
    EXPECT_LT(gap[0], 32) << backend << " gap R";

    const unsigned char* corner = dse::test::PixelAt(rb, 6, 6);
    ASSERT_NE(corner, nullptr) << backend;
    EXPECT_LT(corner[0], 32) << backend << " corner R";
    EXPECT_LT(corner[1], 32) << backend << " corner G";
    EXPECT_LT(corner[2], 32) << backend << " corner B";
}

void RunSingleBackend(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardMorphShaded builtin program unavailable (" << backend << ")";
    VerifyMorphedRight(r.readback, backend);
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
// 三后端单独像素验证（活体消费 MeshRenderer::DrawMorphShaded + ForwardMorphShaded）。
// ============================================================

// ── PBR(0)：morph 位移 + 标准 PBR shading ──
TEST(MorphShadedPixelSmokeTest, OpenGLPbr) { RunSingleBackend(dse::test::RunOpenGL(RenderPbr), "OpenGL"); }
TEST(MorphShadedPixelSmokeTest, D3D11Pbr) { RunSingleBackend(dse::test::RunD3D11(RenderPbr), "D3D11"); }
TEST(MorphShadedPixelSmokeTest, VulkanPbr) { RunSingleBackend(dse::test::RunVulkan(RenderPbr), "Vulkan"); }
TEST(MorphShadedPixelSmokeTest, PbrCrossBackend) { CheckCrossBackend(RenderPbr, 12.0); }

// ── HalfLambert-Static(3)：morph 位移 + 高级 shading 分支 ──
TEST(MorphShadedPixelSmokeTest, OpenGLHalfLambert) { RunSingleBackend(dse::test::RunOpenGL(RenderHalfLambert), "OpenGL"); }
TEST(MorphShadedPixelSmokeTest, D3D11HalfLambert) { RunSingleBackend(dse::test::RunD3D11(RenderHalfLambert), "D3D11"); }
TEST(MorphShadedPixelSmokeTest, VulkanHalfLambert) { RunSingleBackend(dse::test::RunVulkan(RenderHalfLambert), "Vulkan"); }
TEST(MorphShadedPixelSmokeTest, HalfLambertCrossBackend) { CheckCrossBackend(RenderHalfLambert, 12.0); }
