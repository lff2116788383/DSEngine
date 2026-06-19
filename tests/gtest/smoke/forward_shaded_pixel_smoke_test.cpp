/**
 * @file forward_shaded_pixel_smoke_test.cpp
 * @brief B2c-1 高级 shading「活体」像素冒烟 — 用高层 MeshRenderer::DrawShaded（消费内建程序
 *        BuiltinProgram::ForwardShaded：shading_mode 0/2-6 + SSS/clearcoat/anisotropy/POM/
 *        alpha-test/double-sided + 扩展 PerMaterial UBO）经通用原语绘制。
 *
 *        覆盖：
 *          - HalfLambert-Static(3)：左面片法线朝光 → 亮；右面片背光 → 暗。
 *          - Toon(4)：同布局，验证 cel 分段（左亮带、右阴影带）。
 *          - double-sided：背向绕序面片，单面剔除被剔（中心黑），双面则可见（中心亮）。
 *        断言三后端各自像素正确，且每模式跨后端 RMSE 在阈值内。
 *
 * 设计要点（与 forward_pbr smoke 同构）：
 *  - 场景沿 y=128 对称，DX11 回读垂直翻转不影响整帧 RMSE。
 *  - proj = device.GetProjectionCorrection()（Vulkan Y-flip/Z-remap、DX11 Z-remap），否则背面剔除全黑。
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

// 两块竖直矩形面片（XY 平面 z=0）。左面片法线 +X（朝光）→ 亮；右面片法线 -X（背光）→ 暗。
// winding_ccw=true 朝 +Z 相机（front-facing）。
void MakeTwoQuads(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    auto add_quad = [&](float x0, float x1, const glm::vec3& n) {
        const uint16_t base = static_cast<uint16_t>(verts.size());
        const float y0 = -0.7f, y1 = 0.7f;
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
    add_quad(-0.9f, -0.1f, glm::vec3(1.0f, 0.0f, 0.0f));   // 左：朝光 → 亮
    add_quad(0.1f, 0.9f, glm::vec3(-1.0f, 0.0f, 0.0f));    // 右：背光 → 暗
}

// 单块居中面片；winding_ccw=false 时为背向绕序（front-facing 朝 -Z，对 +Z 相机为背面）。
void MakeCenterQuad(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices, bool winding_ccw) {
    const glm::vec4 col(1.0f);
    const glm::vec3 n(0.0f, 0.0f, 1.0f);
    const float x0 = -0.6f, x1 = 0.6f, y0 = -0.6f, y1 = 0.6f;
    verts.push_back({{x0, y0, 0.0f}, col, {0.0f, 0.0f}, n, {1.0f, 0.0f, 0.0f}});
    verts.push_back({{x1, y0, 0.0f}, col, {1.0f, 0.0f}, n, {1.0f, 0.0f, 0.0f}});
    verts.push_back({{x1, y1, 0.0f}, col, {1.0f, 1.0f}, n, {1.0f, 0.0f, 0.0f}});
    verts.push_back({{x0, y1, 0.0f}, col, {0.0f, 1.0f}, n, {1.0f, 0.0f, 0.0f}});
    if (winding_ccw) {
        indices = {0, 1, 2, 0, 2, 3};
    } else {
        indices = {0, 2, 1, 0, 3, 2};  // CW → 对 +Z 相机为背面
    }
}

RenderTargetReadback RenderShadedScene(RhiDevice& device, const ShadedMaterial& material,
                                       const DirectionalLight& light,
                                       const std::vector<MeshVertex>& verts,
                                       const std::vector<uint16_t>& indices,
                                       const std::vector<ShadedPointLight>& point_lights = {}) {
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
        renderer.DrawShaded(*cmd, device, verts, indices, I, I, proj, cam_pos, material, light, point_lights);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    return rb;
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

RenderTargetReadback RenderMode(RhiDevice& device, int shading_mode) {
    ShadedMaterial m;
    m.albedo = glm::vec3(0.8f);
    m.roughness = 0.5f;
    m.shading_mode = shading_mode;
    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeTwoQuads(verts, indices);
    return RenderShadedScene(device, m, MakeLightAlongX(), verts, indices);
}

RenderTargetReadback RenderHalfLambert(RhiDevice& device) { return RenderMode(device, 3); }
RenderTargetReadback RenderToon(RhiDevice& device) { return RenderMode(device, 4); }

// double-sided：背向绕序居中面片，unlit（纯 albedo），单面剔除→中心黑、双面→中心亮。
RenderTargetReadback RenderDoubleSided(RhiDevice& device, bool double_sided) {
    ShadedMaterial m;
    m.albedo = glm::vec3(0.8f);
    m.shading_mode = 0;
    m.double_sided = double_sided;
    DirectionalLight light;
    light.enabled = false;  // unlit：输出 ~albedo，独立于翻转后的法线
    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeCenterQuad(verts, indices, /*winding_ccw=*/false);
    return RenderShadedScene(device, m, light, verts, indices);
}

void VerifyLitDarkQuads(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cy = kRtSize / 2;
    const unsigned char* lit = dse::test::PixelAt(rb, 64, cy);
    const unsigned char* dark = dse::test::PixelAt(rb, 192, cy);
    ASSERT_NE(lit, nullptr) << backend;
    ASSERT_NE(dark, nullptr) << backend;

    EXPECT_GT(lit[0], 100) << backend << " lit R";
    EXPECT_LT(dark[0] + dark[1] + dark[2], lit[0] + lit[1] + lit[2]) << backend << " dark < lit";

    const unsigned char* corner = dse::test::PixelAt(rb, 6, 6);
    ASSERT_NE(corner, nullptr) << backend;
    EXPECT_LT(corner[0], 32) << backend << " corner R";
    EXPECT_LT(corner[1], 32) << backend << " corner G";
    EXPECT_LT(corner[2], 32) << backend << " corner B";
}

void RunSingleBackendLitDark(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardShaded builtin program unavailable (" << backend << ")";
    VerifyLitDarkQuads(r.readback, backend);
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

// ── HalfLambert-Static(3) ──
TEST(ForwardShadedPixelSmokeTest, OpenGLHalfLambert) { RunSingleBackendLitDark(dse::test::RunOpenGL(RenderHalfLambert), "OpenGL"); }
TEST(ForwardShadedPixelSmokeTest, D3D11HalfLambert) { RunSingleBackendLitDark(dse::test::RunD3D11(RenderHalfLambert), "D3D11"); }
TEST(ForwardShadedPixelSmokeTest, VulkanHalfLambert) { RunSingleBackendLitDark(dse::test::RunVulkan(RenderHalfLambert), "Vulkan"); }
TEST(ForwardShadedPixelSmokeTest, HalfLambertCrossBackend) { CheckCrossBackend(RenderHalfLambert, 12.0); }

// ── Toon(4) ──
TEST(ForwardShadedPixelSmokeTest, OpenGLToon) { RunSingleBackendLitDark(dse::test::RunOpenGL(RenderToon), "OpenGL"); }
TEST(ForwardShadedPixelSmokeTest, D3D11Toon) { RunSingleBackendLitDark(dse::test::RunD3D11(RenderToon), "D3D11"); }
TEST(ForwardShadedPixelSmokeTest, VulkanToon) { RunSingleBackendLitDark(dse::test::RunVulkan(RenderToon), "Vulkan"); }
TEST(ForwardShadedPixelSmokeTest, ToonCrossBackend) { CheckCrossBackend(RenderToon, 12.0); }

// ── double-sided ──
TEST(ForwardShadedPixelSmokeTest, DoubleSidedRendersBackface) {
    auto fn_on = [](RhiDevice& d) { return RenderDoubleSided(d, true); };
    auto fn_off = [](RhiDevice& d) { return RenderDoubleSided(d, false); };
    auto on_gl = dse::test::RunOpenGL(fn_on);
    if (!on_gl.available) GTEST_SKIP() << on_gl.skip_reason;
    if (on_gl.readback.pixels.empty()) GTEST_SKIP() << "ForwardShaded unavailable (OpenGL)";
    auto off_gl = dse::test::RunOpenGL(fn_off);

    const int c = kRtSize / 2;
    const unsigned char* on_center = dse::test::PixelAt(on_gl.readback, c, c);
    const unsigned char* off_center = dse::test::PixelAt(off_gl.readback, c, c);
    ASSERT_NE(on_center, nullptr);
    ASSERT_NE(off_center, nullptr);
    // 双面：背面可见 → 中心亮。单面：背面被剔 → 中心清屏黑。
    EXPECT_GT(on_center[0], 100) << "double-sided center should be lit";
    EXPECT_LT(off_center[0], 32) << "single-sided backface should be culled (black)";
}

TEST(ForwardShadedPixelSmokeTest, DoubleSidedCrossBackend) {
    auto fn_on = [](RhiDevice& d) { return RenderDoubleSided(d, true); };
    CheckCrossBackend(fn_on, 12.0);
}

// ── clustered 点光（B2c-2） ──
// 正面朝 +Z 的居中面片，低强度方向光作基底；一盞亮点光位于轴上正前方（x=y=0），
// 场景沿 y=128 对称。with_point=true 时中心显著变亮。
RenderTargetReadback RenderPointLit(RhiDevice& device, bool with_point) {
    ShadedMaterial m;
    m.albedo = glm::vec3(0.8f);
    m.roughness = 0.5f;
    m.shading_mode = 0;  // PBR
    DirectionalLight light;
    light.direction = glm::vec3(0.0f, 0.0f, -1.0f);  // to_light = +Z
    light.color = glm::vec3(1.0f);
    light.intensity = 0.2f;  // 低基底光
    light.ambient = 0.02f;
    light.enabled = true;
    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeCenterQuad(verts, indices, /*winding_ccw=*/true);  // front-facing 不被剔
    std::vector<ShadedPointLight> pls;
    if (with_point) {
        ShadedPointLight pl;
        pl.color = glm::vec3(1.0f);
        pl.intensity = 5.0f;
        pl.position = glm::vec3(0.0f, 0.0f, 0.5f);  // 轴上正前方
        pl.radius = 3.0f;
        pls.push_back(pl);
    }
    return RenderShadedScene(device, m, light, verts, indices, pls);
}

TEST(ForwardShadedPixelSmokeTest, PointLightBrightensCenter) {
    auto on = dse::test::RunOpenGL([](RhiDevice& d) { return RenderPointLit(d, true); });
    if (!on.available) GTEST_SKIP() << on.skip_reason;
    if (on.readback.pixels.empty()) GTEST_SKIP() << "ForwardShaded unavailable (OpenGL)";
    auto off = dse::test::RunOpenGL([](RhiDevice& d) { return RenderPointLit(d, false); });

    const int c = kRtSize / 2;
    const unsigned char* on_c = dse::test::PixelAt(on.readback, c, c);
    const unsigned char* off_c = dse::test::PixelAt(off.readback, c, c);
    ASSERT_NE(on_c, nullptr);
    ASSERT_NE(off_c, nullptr);
    // 点光显著抬高中心亮度。
    EXPECT_GT(on_c[0], off_c[0] + 30) << "point light should brighten center R";
    EXPECT_GT(on_c[0], 100) << "point-lit center should be bright";
}

TEST(ForwardShadedPixelSmokeTest, PointLightCrossBackend) {
    auto fn = [](RhiDevice& d) { return RenderPointLit(d, true); };
    CheckCrossBackend(fn, 12.0);
}

// ── 地形 splatmap（B2c-3） ──
// 居中正面片，unlit（输出 = texColor*albedo）。权重图 100% 落 layer0（纯绿），splat 开启时
// 中心应为绿（G≫R,B）；关闭时 u_texture 为白默认 → 中心近白。
RenderTargetReadback RenderSplat(RhiDevice& device, bool splat_enabled) {
    auto solid = [&](unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
        const unsigned char px[16] = { r,g,b,a, r,g,b,a, r,g,b,a, r,g,b,a };  // 2x2 RGBA
        return device.CreateTexture2D(2, 2, px, false);
    };
    unsigned int w_map = solid(255, 0, 0, 0);    // 权重：100% layer0
    unsigned int l0 = solid(0, 255, 0, 255);     // layer0 纯绿
    unsigned int l1 = solid(255, 0, 0, 255);     // layer1 纯红（权重 0，不显）
    unsigned int l2 = solid(0, 0, 255, 255);     // layer2 纯蓝（权重 0）
    unsigned int l3 = solid(255, 255, 0, 255);   // layer3 纯黄（权重 0）

    ShadedMaterial m;
    m.albedo = glm::vec3(1.0f);
    m.shading_mode = 0;
    m.splat_enabled = splat_enabled;
    m.splat_weight_map = w_map;
    m.splat_layers[0] = l0;
    m.splat_layers[1] = l1;
    m.splat_layers[2] = l2;
    m.splat_layers[3] = l3;
    m.splat_tiling = glm::vec4(1.0f);

    DirectionalLight light;
    light.enabled = false;  // unlit：输出 ~ texColor * albedo，便于直接比对 splat 混合色
    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeCenterQuad(verts, indices, /*winding_ccw=*/true);
    RenderTargetReadback rb = RenderShadedScene(device, m, light, verts, indices);

    device.DeleteTexture(w_map);
    device.DeleteTexture(l0);
    device.DeleteTexture(l1);
    device.DeleteTexture(l2);
    device.DeleteTexture(l3);
    return rb;
}

TEST(ForwardShadedPixelSmokeTest, SplatBlendsLayers) {
    auto on = dse::test::RunOpenGL([](RhiDevice& d) { return RenderSplat(d, true); });
    if (!on.available) GTEST_SKIP() << on.skip_reason;
    if (on.readback.pixels.empty()) GTEST_SKIP() << "ForwardShaded unavailable (OpenGL)";
    auto off = dse::test::RunOpenGL([](RhiDevice& d) { return RenderSplat(d, false); });

    const int c = kRtSize / 2;
    const unsigned char* on_c = dse::test::PixelAt(on.readback, c, c);
    const unsigned char* off_c = dse::test::PixelAt(off.readback, c, c);
    ASSERT_NE(on_c, nullptr);
    ASSERT_NE(off_c, nullptr);
    // splat 开启：layer0 纯绿主导 → G 远大于 R/B。
    EXPECT_GT(on_c[1], 100) << "splat center G";
    EXPECT_GT(on_c[1], on_c[0] + 40) << "splat center G > R";
    EXPECT_GT(on_c[1], on_c[2] + 40) << "splat center G > B";
    // splat 关闭：默认白纹理 → 中心近白（R 高）。
    EXPECT_GT(off_c[0], 100) << "non-splat center R (white default)";
}

TEST(ForwardShadedPixelSmokeTest, SplatCrossBackend) {
    auto fn = [](RhiDevice& d) { return RenderSplat(d, true); };
    CheckCrossBackend(fn, 12.0);
}

// ── 积雪（B2c-3） ──
// 居中正面片，法线作 +Y（朝上）使积雪 mask 命中；PBR + 沿 +Y 方向光。
// 雪覆盖开启时表面反照率→白 → 中心显著变亮。
RenderTargetReadback RenderSnow(RhiDevice& device, bool snow) {
    ShadedMaterial m;
    m.albedo = glm::vec3(0.12f, 0.09f, 0.05f);  // 暗褐基色
    m.roughness = 0.8f;
    m.shading_mode = 0;  // PBR
    if (snow) {
        m.snow_coverage = 1.0f;
        m.snow_albedo = glm::vec3(0.95f);
        m.snow_roughness = 0.7f;
        m.snow_normal_threshold = 0.2f;
        m.snow_edge_sharpness = 2.0f;
    }
    DirectionalLight light;
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);  // to_light = +Y
    light.color = glm::vec3(1.0f);
    light.intensity = 2.0f;
    light.ambient = 0.05f;
    light.enabled = true;

    // 居中面片，法线改为 +Y（朝上）以触发积雪；几何仍在 XY 平面朝 +Z 相机可见。
    const glm::vec4 col(1.0f);
    const glm::vec3 n(0.0f, 1.0f, 0.0f);
    const float x0 = -0.6f, x1 = 0.6f, y0 = -0.6f, y1 = 0.6f;
    std::vector<MeshVertex> verts = {
        {{x0, y0, 0.0f}, col, {0.0f, 0.0f}, n, {1.0f, 0.0f, 0.0f}},
        {{x1, y0, 0.0f}, col, {1.0f, 0.0f}, n, {1.0f, 0.0f, 0.0f}},
        {{x1, y1, 0.0f}, col, {1.0f, 1.0f}, n, {1.0f, 0.0f, 0.0f}},
        {{x0, y1, 0.0f}, col, {0.0f, 1.0f}, n, {1.0f, 0.0f, 0.0f}},
    };
    std::vector<uint16_t> indices = {0, 1, 2, 0, 2, 3};
    return RenderShadedScene(device, m, light, verts, indices);
}

TEST(ForwardShadedPixelSmokeTest, SnowBrightensSurface) {
    auto on = dse::test::RunOpenGL([](RhiDevice& d) { return RenderSnow(d, true); });
    if (!on.available) GTEST_SKIP() << on.skip_reason;
    if (on.readback.pixels.empty()) GTEST_SKIP() << "ForwardShaded unavailable (OpenGL)";
    auto off = dse::test::RunOpenGL([](RhiDevice& d) { return RenderSnow(d, false); });

    const int c = kRtSize / 2;
    const unsigned char* on_c = dse::test::PixelAt(on.readback, c, c);
    const unsigned char* off_c = dse::test::PixelAt(off.readback, c, c);
    ASSERT_NE(on_c, nullptr);
    ASSERT_NE(off_c, nullptr);
    const int on_lum = on_c[0] + on_c[1] + on_c[2];
    const int off_lum = off_c[0] + off_c[1] + off_c[2];
    // 积雪显著抬高表面亮度。
    EXPECT_GT(on_lum, off_lum + 120) << "snow should brighten surface";
    EXPECT_GT(on_c[0], 120) << "snow-covered center should be near white";
}

TEST(ForwardShadedPixelSmokeTest, SnowCrossBackend) {
    auto fn = [](RhiDevice& d) { return RenderSnow(d, true); };
    CheckCrossBackend(fn, 12.0);
}
