/**
 * @file foliage_wind_pixel_smoke_test.cpp
 * @brief B2b-6 植被风弯曲（foliage wind）「活体」像素冒烟 —— 验证 MeshRenderer::DrawInstancedShaded
 *        在 material.foliage 开启时，从 device 全局渲染状态取 foliage_wind/foliage_push 喂入
 *        forward_shaded_instanced.vert（与 pbr.vert:112-138 同算的顶点风弯曲）施加顶点位移。
 *
 * 设计要点（验证「风弯曲随高度递增 + 每绘制 foliage 开关门控」）：
 *  - 一根**竖直窄条**面片（局部空间 x∈[-0.06,0.06], y∈[0,1]，z=0；几何朝 +Z 相机可见，
 *    法线 +X 朝光被照亮 —— 与 tree 测试同解耦：几何面向相机、法线面向光），单实例 model=I。
 *  - VS 风弯曲按 height_factor=clamp(aPos.y,0,1) 递增：条底(y=0)不动、条顶(y=1)沿 +X 最大位移。
 *    强风(strength=6, dir=+X) 下条顶世界位移 ≈0.51 → 落到屏幕右侧约 col 185-201；条底仍在 col~128。
 *  - device 全局风**两种用例都设**，差异仅 material.foliage：
 *      · 关(foliage=false)：门控关 → VS 整段跳过 → 竖直条不偏（证明非植被绘制不受全局风影响、不回归）；
 *      · 开(foliage=true) ：门控开 → 条顶右偏成斜带。
 *  - 断言（与 y 翻转无关，按列扫描全部行）：
 *      1) RMSE(关,开) > 4 —— 风确实改变了画面；
 *      2) col 170 处亮像素行数：关≈0、开>8 —— 证明条顶右偏（高处位移大），且偏移随高度递增（斜带）；
 *      3) 三后端「开」图 RMSE<12 —— 风位移跨后端一致。
 *  - 若 VS 未施风 / CPU 侧未喂 foliage_wind / material.foliage 门控失效 → 关图与开图相同，断言 1/2 失败。
 *  - proj 含各后端裁剪修正（GetProjectionCorrection），否则 Vulkan 背面剔除致全黑。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/mesh_renderer.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 竖直窄条面片（局部空间，z=0，几何朝 +Z 相机；法线 +X 朝光源 → 亮）。y∈[0,1] 使风弯曲随高度递增。
void MakeVerticalStrip(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    const glm::vec3 n(1.0f, 0.0f, 0.0f);   // 法线朝光源（与几何朝向解耦）→ 亮
    const glm::vec3 tan(0.0f, 1.0f, 0.0f);
    const float x0 = -0.06f, x1 = 0.06f, y0 = 0.0f, y1 = 1.0f;
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

// 单实例（model=I）竖直条；device 全局风始终设为强风(+X)，foliage 控制门控开关。
RenderTargetReadback RenderStrip(RhiDevice& device, bool foliage) {
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
    MakeVerticalStrip(verts, indices);

    std::vector<glm::mat4> models = {glm::mat4(1.0f)};  // 单实例，世界位 = 局部位

    ShadedMaterial material;
    material.albedo = glm::vec3(0.8f);
    material.metallic = 0.0f;
    material.roughness = 0.5f;
    material.ao = 1.0f;
    material.shading_mode = 0;
    material.double_sided = true;   // 斜变形后放宽剔除，避免翻面误剔
    material.foliage = foliage;     // 风弯曲门控（被测开关）

    // 全局风：t=π/4 → sway≈0.567（正），强风 +X 方向；两用例都设以验证 foliage 门控。
    device.SetGlobalFoliageWind(glm::vec4(0.7853982f, 6.0f, 1.0f, 0.0f));
    device.SetGlobalFoliagePush(glm::vec4(0.0f));

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
        renderer.DrawInstancedShaded(*cmd, device, verts, indices, models,
                                     I, proj, cam_pos, material, MakeLightAlongX());
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    return rb;
}

RenderTargetReadback RenderWindOff(RhiDevice& device) { return RenderStrip(device, false); }
RenderTargetReadback RenderWindOn(RhiDevice& device) { return RenderStrip(device, true); }

// 某列上「亮」(R>100) 像素的行数（按全部行扫描，与 DX11 垂直翻转无关）。
int LitRowsAtColumn(const RenderTargetReadback& rb, int x) {
    int n = 0;
    for (int y = 0; y < rb.height; ++y) {
        const unsigned char* p = dse::test::PixelAt(rb, x, y);
        if (p && p[0] > 100) ++n;
    }
    return n;
}

// 逐列亮像素行数剖面（长度=width）。垂直翻转只改变行顺序、不改变每列计数 → 跨后端可直接比较，
// 规避 DX11 回读垂直翻转对 y 非对称场景（植被风弯随高度递增）的整帧 RMSE 干扰。
std::vector<int> ColumnLitProfile(const RenderTargetReadback& rb) {
    std::vector<int> prof(rb.width, 0);
    for (int x = 0; x < rb.width; ++x) prof[x] = LitRowsAtColumn(rb, x);
    return prof;
}

// 两剖面的 RMS 差（逐列计数差的均方根）。
double ColumnProfileRmse(const std::vector<int>& a, const std::vector<int>& b) {
    if (a.size() != b.size() || a.empty()) return 1e9;
    double acc = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        const double d = static_cast<double>(a[i] - b[i]);
        acc += d * d;
    }
    return std::sqrt(acc / static_cast<double>(a.size()));
}

bool Ok(const dse::test::BackendResult& r) {
    return r.available && !r.readback.pixels.empty();
}

// 单后端：分别渲染「关/开」两图，验证风位移（关图竖直、开图条顶右偏成斜带）。
void RunWindShift(dse::test::BackendResult off, dse::test::BackendResult on, const char* backend) {
    if (!off.available || !on.available) GTEST_SKIP() << off.skip_reason;
    if (off.readback.pixels.empty() || on.readback.pixels.empty())
        GTEST_SKIP() << "ForwardInstancedShaded builtin program unavailable (" << backend << ")";

    ASSERT_EQ(off.readback.width, kRtSize) << backend;
    ASSERT_EQ(on.readback.width, kRtSize) << backend;

    // 1) 风改变了画面。
    const double rmse = dse::test::ComputeRmse(off.readback, on.readback);
    EXPECT_GT(rmse, 4.0) << backend << " RMSE(off,on)=" << rmse;

    // 2) col 170（中心右侧）：关图≈0（竖直条在 col~128）、开图>8（条顶右偏经过此列）。
    const int off_lit = LitRowsAtColumn(off.readback, 170);
    const int on_lit = LitRowsAtColumn(on.readback, 170);
    EXPECT_LT(off_lit, 4) << backend << " off lit@170=" << off_lit;
    EXPECT_GT(on_lit, 8) << backend << " on lit@170=" << on_lit;
}

}  // namespace

// ============================================================
// 三后端单独像素验证（活体消费 MeshRenderer::DrawInstancedShaded + foliage wind VS）。
// ============================================================

TEST(FoliageWindPixelSmokeTest, OpenGLWindShift) {
    RunWindShift(dse::test::RunOpenGL(RenderWindOff), dse::test::RunOpenGL(RenderWindOn), "OpenGL");
}
TEST(FoliageWindPixelSmokeTest, D3D11WindShift) {
    RunWindShift(dse::test::RunD3D11(RenderWindOff), dse::test::RunD3D11(RenderWindOn), "D3D11");
}
TEST(FoliageWindPixelSmokeTest, VulkanWindShift) {
    RunWindShift(dse::test::RunVulkan(RenderWindOff), dse::test::RunVulkan(RenderWindOn), "Vulkan");
}

// ── 三后端「风开」图跨后端一致性：风位移（条带斜形）在 GL/DX11/Vulkan 一致 ──
// 用逐列亮像素剖面比较（翻转不变），规避 DX11 回读垂直翻转对 y 非对称场景的整帧 RMSE 干扰。
TEST(FoliageWindPixelSmokeTest, WindOnCrossBackend) {
    auto gl = dse::test::RunOpenGL(RenderWindOn);
    auto dx = dse::test::RunD3D11(RenderWindOn);
    auto vk = dse::test::RunVulkan(RenderWindOn);
    int available = (Ok(gl) ? 1 : 0) + (Ok(dx) ? 1 : 0) + (Ok(vk) ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "需要至少两个后端可用以比较剖面";
    const double kColTol = 5.0;  // 逐列亮像素行数 RMS 差容限（边缘抗锯齿/亚像素差异）
    if (Ok(gl) && Ok(dx)) EXPECT_LT(ColumnProfileRmse(ColumnLitProfile(gl.readback), ColumnLitProfile(dx.readback)), kColTol) << "GL vs DX11";
    if (Ok(gl) && Ok(vk)) EXPECT_LT(ColumnProfileRmse(ColumnLitProfile(gl.readback), ColumnLitProfile(vk.readback)), kColTol) << "GL vs Vulkan";
    if (Ok(dx) && Ok(vk)) EXPECT_LT(ColumnProfileRmse(ColumnLitProfile(dx.readback), ColumnLitProfile(vk.readback)), kColTol) << "DX11 vs Vulkan";
}
