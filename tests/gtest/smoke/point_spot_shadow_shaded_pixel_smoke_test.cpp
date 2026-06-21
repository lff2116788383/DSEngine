/**
 * @file point_spot_shadow_shaded_pixel_smoke_test.cpp
 * @brief Final-Feat-8 点光/聚光「阴影接收」跨后端像素冒烟 —— 验证 MeshRenderer 着色路径
 *        新增的点/聚光阴影采样能力（forward_shaded.frag 新增 SpotShadow/PointShadow +
 *        8 个阴影采样器 binding 21-28[flat unit 12-19] + PerScene.u_spot_light_space_matrices[4]，
 *        mesh_renderer.cpp 在全部 shaded 方法绑 spot 2D(12-15)/point cube(16-19) 阴影图，
 *        未用槽位回退默认白 2D / 白 cube 深度纹理）。
 *
 * 设计要点（聚焦「新绑定 + cast_shadow 代码路径」的三后端 descriptor 兼容性，这是本特性唯一未被
 * 生产 shadow.glsl 既有测试覆盖的风险面；采样数学与 shadow.glsl 逐行一致，不再重复验证遮挡数值）：
 *  - 一块铺满视口、居中原点的正方形面片（x,y∈[-0.9,0.9], z=0），法线 +Z 朝相机/光源。
 *  - 关方向光 + ambient=0：整帧仅由「开启 cast_shadow 的」点光 + 聚光灯贡献。
 *  - GlobalRenderState 未设任何点/聚光阴影图（spot_shadow_map/point_shadow_map 全 0）→ 绑定默认
 *    白 2D / 白 cube：SpotShadow 采样深度=1 → (cur-bias)>1 恒假 → 0 遮挡；PointShadow 采样 .r=1 →
 *    closest=radius，(cur-bias)>radius 恒假 → 0 遮挡。故「cast_shadow=true + 白默认」等价于无阴影。
 *  - 验证：中心亮（点光全向 + 聚光锥内同时照亮）；聚光锥外四角仅余点光 → 仍应被点光照亮（区别于
 *    纯聚光的四角全暗）。中心不亮 → 8 个采样器任一在某后端 reflection 错位 / cube descriptor 维度
 *    不匹配致整帧异常 → center 断言失败；任一后端因 560B PerScene UBO 越界 → 全帧异常。
 *  - 跨后端 RMSE：三后端在新绑定下输出一致（默认白阴影路径不引入后端差异）。
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

// 铺满视口、居中原点的正方形面片（局部空间 z=0，CCW 朝 +Z 相机），法线 +Z 朝光源。
void MakeFullscreenQuad(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    const glm::vec3 n(0.0f, 0.0f, 1.0f);
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

DirectionalLight MakeNoDirectional() {
    DirectionalLight light;
    light.direction = glm::vec3(0.0f, 0.0f, -1.0f);
    light.color = glm::vec3(0.0f);
    light.intensity = 0.0f;
    light.ambient = 0.0f;
    light.enabled = false;
    return light;
}

// 沿 Z 轴俯照面片的聚光灯，开启 cast_shadow（shadow_index=0）—— 走 SpotShadow 采样分支。
ShadedSpotLight MakeShadowSpot() {
    ShadedSpotLight spot;
    spot.position = glm::vec3(0.0f, 0.0f, 1.5f);
    spot.direction = glm::vec3(0.0f, 0.0f, -1.0f);
    spot.color = glm::vec3(1.0f);
    spot.intensity = 5.0f;
    spot.radius = 10.0f;
    spot.inner_cone = 15.0f;
    spot.outer_cone = 25.0f;
    spot.cast_shadow = true;
    spot.shadow_index = 0;
    return spot;
}

// 面片正前方的点光，开启 cast_shadow（shadow_index=0）—— 走 PointShadow cube 采样分支。
// 全向衰减：中心 + 四角都被照亮（与聚光锥角形成区分）。
ShadedPointLight MakeShadowPoint() {
    ShadedPointLight pl;
    pl.position = glm::vec3(0.0f, 0.0f, 1.2f);
    pl.color = glm::vec3(1.0f);
    pl.intensity = 4.0f;
    pl.radius = 12.0f;
    pl.cast_shadow = true;
    pl.shadow_index = 0;
    return pl;
}

RenderTargetReadback RenderShadowLit(RhiDevice& device, int shading_mode) {
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
    material.receive_shadow = true;
    material.shadow_strength = 1.0f;

    const glm::mat4 I(1.0f);
    const glm::mat4 proj = device.GetProjectionCorrection();
    const glm::vec3 cam_pos(0.0f, 0.0f, 1.0f);

    std::vector<ShadedPointLight> points = {MakeShadowPoint()};
    std::vector<ShadedSpotLight> spots = {MakeShadowSpot()};

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
                            material, MakeNoDirectional(), points, ShadedGI{}, spots);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    return rb;
}

RenderTargetReadback RenderPbr(RhiDevice& device) { return RenderShadowLit(device, 0); }
RenderTargetReadback RenderHalfLambert(RhiDevice& device) { return RenderShadowLit(device, 3); }

// 中心亮（点光 + 聚光同时照亮）+ 四角亮（点光全向，证明点光 cast_shadow 路径未误杀贡献）。
void VerifyShadowLit(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int c = kRtSize / 2;
    const unsigned char* center = dse::test::PixelAt(rb, c, c);
    ASSERT_NE(center, nullptr) << backend << " center";
    EXPECT_GT(center[0], 80) << backend << " center R";  // 点光 + 聚光内锥全亮

    // 离轴边中点（像素 28/228，落在面片内 [~13,~243]，但离聚光轴 ~27° > 外锥 25° → 锥外）：
    // 仅余点光（全向）贡献。默认白 cube → 点光不被遮挡 → 应被照亮（区别于纯聚光此处全暗）。
    const int offaxis[4][2] = {{28, c}, {kRtSize - 28, c}, {c, 28}, {c, kRtSize - 28}};
    for (auto& cc : offaxis) {
        const unsigned char* p = dse::test::PixelAt(rb, cc[0], cc[1]);
        ASSERT_NE(p, nullptr) << backend << " offaxis";
        EXPECT_GT(p[0], 20) << backend << " offaxis R @(" << cc[0] << "," << cc[1]
                            << ") — 点光 cast_shadow + 白默认 cube 不应误杀点光贡献";
    }
}

void RunSingleBackend(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardShaded builtin program unavailable (" << backend << ")";
    VerifyShadowLit(r.readback, backend);
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
// 三后端单独像素验证（活体消费 MeshRenderer 点/聚光 cast_shadow 采样路径 + 8 个新阴影绑定）。
// ============================================================

// ── PBR(0)：点光 cube + 聚光 2D 阴影采样器绑定 + cast_shadow 分支（默认白 → 无遮挡）──
TEST(PointSpotShadowShadedPixelSmokeTest, OpenGLPbr) { RunSingleBackend(dse::test::RunOpenGL(RenderPbr), "OpenGL"); }
TEST(PointSpotShadowShadedPixelSmokeTest, D3D11Pbr) { RunSingleBackend(dse::test::RunD3D11(RenderPbr), "D3D11"); }
TEST(PointSpotShadowShadedPixelSmokeTest, VulkanPbr) { RunSingleBackend(dse::test::RunVulkan(RenderPbr), "Vulkan"); }
TEST(PointSpotShadowShadedPixelSmokeTest, PbrCrossBackend) { CheckCrossBackend(RenderPbr, 12.0); }

// ── HalfLambert-Static(3)：阴影绑定 + 高级 shading 分支共存 ──
TEST(PointSpotShadowShadedPixelSmokeTest, OpenGLHalfLambert) { RunSingleBackend(dse::test::RunOpenGL(RenderHalfLambert), "OpenGL"); }
TEST(PointSpotShadowShadedPixelSmokeTest, D3D11HalfLambert) { RunSingleBackend(dse::test::RunD3D11(RenderHalfLambert), "D3D11"); }
TEST(PointSpotShadowShadedPixelSmokeTest, VulkanHalfLambert) { RunSingleBackend(dse::test::RunVulkan(RenderHalfLambert), "Vulkan"); }
TEST(PointSpotShadowShadedPixelSmokeTest, HalfLambertCrossBackend) { CheckCrossBackend(RenderHalfLambert, 12.0); }
