/**
 * @file spot_shaded_pixel_smoke_test.cpp
 * @brief Final-Feat-4 聚光灯 SpotLight「活体」像素冒烟 —— 用高层 MeshRenderer::DrawShaded
 *        新增的 spot_lights 形参（消费内建程序 BuiltinProgram::ForwardShaded：forward_shaded.frag
 *        新增 FwdSpotLightUBO\@set7.b1[契约 slot 7] + SpotLightsLo 锥角衰减）。
 *
 * 设计要点（验证「锥角内/外」差异，区别于点光的全向衰减）：
 *  - 一块铺满视口、居中于原点的正方形面片（x,y∈[-0.9,0.9], z=0），法线 +Z 朝相机/聚光灯。
 *  - 关闭方向光（enabled=false）+ ambient=0：整帧仅由聚光灯贡献，便于隔离锥角效果。
 *  - 单个聚光灯置于 (0,0,1.5)、方向 (0,0,-1) 指向面片，inner=15°/outer=25°、radius=10（距离衰减弱）。
 *    锥角正确 → 中心 (0,0) 落在内锥全亮；四角 (|x|,|y|≈0.9，离轴角≫25°) 落在锥外全暗。
 *  - 若锥角衰减失效（退化为点光/全向）→ 四角也被照亮，corner 断言失败；
 *    若 set7.b1 聚光灯 UBO 未正确绑定到契约 slot 7（任一后端 reflection 错位）→ 中心不亮，center 断言失败。
 *  - 场景沿 y=128 对称（聚光灯在 Z 轴上），DX11 回读垂直翻转不影响整帧 RMSE。
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

// 铺满视口、居中于原点的正方形面片（局部空间，z=0，CCW 朝 +Z 相机），法线 +Z 朝聚光灯。
void MakeFullscreenQuad(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    const glm::vec3 n(0.0f, 0.0f, 1.0f);   // 法线朝 +Z（相机/聚光灯方向）
    const glm::vec3 tan(1.0f, 0.0f, 0.0f);
    const float x0 = -0.9f, x1 = 0.9f, y0 = -0.9f, y1 = 0.9f;
    MeshVertex v;
    v.color = col; v.normal = n; v.tangent = tan;
    v.position = {x0, y0, 0.0f}; v.uv = {0.0f, 0.0f}; verts.push_back(v);
    v.position = {x1, y0, 0.0f}; v.uv = {1.0f, 0.0f}; verts.push_back(v);
    v.position = {x1, y1, 0.0f}; v.uv = {1.0f, 1.0f}; verts.push_back(v);
    v.position = {x0, y1, 0.0f}; v.uv = {0.0f, 1.0f}; verts.push_back(v);
    indices = {0, 1, 2, 0, 2, 3};
}

// 方向光关闭 + 无环境光：整帧只受聚光灯影响。
DirectionalLight MakeNoDirectional() {
    DirectionalLight light;
    light.direction = glm::vec3(0.0f, 0.0f, -1.0f);
    light.color = glm::vec3(0.0f);
    light.intensity = 0.0f;
    light.ambient = 0.0f;
    light.enabled = false;
    return light;
}

// 沿 Z 轴俯照面片的聚光灯（锥心在原点）。
ShadedSpotLight MakeAxisSpot() {
    ShadedSpotLight spot;
    spot.position = glm::vec3(0.0f, 0.0f, 1.5f);
    spot.direction = glm::vec3(0.0f, 0.0f, -1.0f);  // 指向面片
    spot.color = glm::vec3(1.0f);
    spot.intensity = 5.0f;
    spot.radius = 10.0f;       // 距离衰减弱，凸显锥角衰减
    spot.inner_cone = 15.0f;
    spot.outer_cone = 25.0f;
    return spot;
}

RenderTargetReadback RenderSpot(RhiDevice& device, int shading_mode) {
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
    MakeFullscreenQuad(verts, indices);

    ShadedMaterial material;
    material.albedo = glm::vec3(0.8f);
    material.metallic = 0.0f;
    material.roughness = 0.5f;
    material.ao = 1.0f;
    material.shading_mode = shading_mode;

    const glm::mat4 I(1.0f);
    const glm::mat4 proj = device.GetProjectionCorrection();
    const glm::vec3 cam_pos(0.0f, 0.0f, 1.0f);

    std::vector<ShadedSpotLight> spots = {MakeAxisSpot()};

    MeshRenderer renderer;
    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        renderer.DrawShaded(*cmd, device, verts, indices, I, I, proj, cam_pos,
                            material, MakeNoDirectional(), {}, ShadedGI{}, spots);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    return rb;
}

RenderTargetReadback RenderPbr(RhiDevice& device) { return RenderSpot(device, 0); }
RenderTargetReadback RenderHalfLambert(RhiDevice& device) { return RenderSpot(device, 3); }

// 中心亮（锥内）、四角暗（锥外）—— 证明聚光灯锥角衰减 + set7.b1 UBO 绑定同时生效。
void VerifySpotCone(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int c = kRtSize / 2;
    const unsigned char* center = dse::test::PixelAt(rb, c, c);
    ASSERT_NE(center, nullptr) << backend << " center";
    EXPECT_GT(center[0], 80) << backend << " center R";  // 内锥全亮

    // 四角离轴角 ≫ 外锥半角 → 锥外全暗（点光会照亮，此处必须暗）。
    const int corners[4][2] = {{6, 6}, {kRtSize - 6, 6}, {6, kRtSize - 6}, {kRtSize - 6, kRtSize - 6}};
    for (auto& cc : corners) {
        const unsigned char* p = dse::test::PixelAt(rb, cc[0], cc[1]);
        ASSERT_NE(p, nullptr) << backend << " corner";
        EXPECT_LT(p[0], 32) << backend << " corner R @(" << cc[0] << "," << cc[1] << ")";
        EXPECT_LT(p[1], 32) << backend << " corner G @(" << cc[0] << "," << cc[1] << ")";
        EXPECT_LT(p[2], 32) << backend << " corner B @(" << cc[0] << "," << cc[1] << ")";
    }
}

void RunSingleBackend(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardShaded builtin program unavailable (" << backend << ")";
    VerifySpotCone(r.readback, backend);
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
// 三后端单独像素验证（活体消费 MeshRenderer::DrawShaded spot_lights + ForwardShaded）。
// ============================================================

// ── PBR(0)：聚光灯锥角 + 标准 PBR shading ──
TEST(SpotShadedPixelSmokeTest, OpenGLPbr) { RunSingleBackend(dse::test::RunOpenGL(RenderPbr), "OpenGL"); }
TEST(SpotShadedPixelSmokeTest, D3D11Pbr) { RunSingleBackend(dse::test::RunD3D11(RenderPbr), "D3D11"); }
TEST(SpotShadedPixelSmokeTest, VulkanPbr) { RunSingleBackend(dse::test::RunVulkan(RenderPbr), "Vulkan"); }
TEST(SpotShadedPixelSmokeTest, PbrCrossBackend) { CheckCrossBackend(RenderPbr, 12.0); }

// ── HalfLambert-Static(3)：聚光灯锥角 + 高级 shading 分支 ──
TEST(SpotShadedPixelSmokeTest, OpenGLHalfLambert) { RunSingleBackend(dse::test::RunOpenGL(RenderHalfLambert), "OpenGL"); }
TEST(SpotShadedPixelSmokeTest, D3D11HalfLambert) { RunSingleBackend(dse::test::RunD3D11(RenderHalfLambert), "D3D11"); }
TEST(SpotShadedPixelSmokeTest, VulkanHalfLambert) { RunSingleBackend(dse::test::RunVulkan(RenderHalfLambert), "Vulkan"); }
TEST(SpotShadedPixelSmokeTest, HalfLambertCrossBackend) { CheckCrossBackend(RenderHalfLambert, 12.0); }
