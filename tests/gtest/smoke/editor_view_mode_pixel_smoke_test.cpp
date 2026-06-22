/**
 * @file editor_view_mode_pixel_smoke_test.cpp
 * @brief 阶段4-M2 编辑器场景视图模式（wireframe / overdraw / force_unlit）「活体」像素冒烟 ——
 *        用高层 MeshRenderer::DrawShaded（高级 shading forward 路径，BuiltinProgram::ForwardShaded）
 *        分别在 device.SetWireframeMode / SetOverdrawMode / SetForceUnlit 作用域内绘制，验证三种
 *        视图模式经 GetGlobalRenderState() 的 wireframe_mode/overdraw_mode/force_unlit 标志传入
 *        MeshRenderer 后逐个生效（wireframe→line-fill PSO；overdraw→加性混合 PSO + 固定低强度材质；
 *        force_unlit→PerScene 关方向光走 forward_shaded.frag Unlit 分支）。
 *
 * 设计要点（各模式可与「无模式基线」对照，差异即模式生效的证据）：
 *  - force_unlit：居中面片，**法线 +Z**（垂直于光方向 to_light=+X → 基线 NdotL=0 仅环境光，暗）。
 *      关方向光后走 Unlit 分支 color = albedo(0.8) → tonemap+gamma 后明显变亮。
 *      → 基线中心暗（<90）、force_unlit 中心亮（>120）。
 *  - wireframe：居中**大**面片（覆盖大片），法线 +X（NdotL=1，亮）。实心填充点亮整片；线框仅描边。
 *      → 线框「亮像素数」远小于实心（<实心/4）且 >0（确有描边）。
 *  - overdraw：两块**部分重叠**面片（法线 +X，亮），加性混合下重叠区亮度叠加。
 *      → 重叠中心 R 显著大于单覆盖左区 R，二者皆 > 背景。
 *  - proj 含各后端裁剪修正（GetProjectionCorrection），否则 Vulkan 背面剔除/翻转致异常。
 *  - 本机仅 D3D11(WARP) 实跑；GL/Vulkan 无驱动 skip（既有状况，非回归）。
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

// 一块矩形面片（局部=世界空间，z=0，CCW 朝 +Z 相机）。法线由调用方指定（独立于几何朝向，
// 仅用于光照），全白顶点色。
void AppendQuad(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices,
                float x0, float x1, float y0, float y1, const glm::vec3& n) {
    const uint16_t base = static_cast<uint16_t>(verts.size());
    MeshVertex v;
    v.color = glm::vec4(1.0f);
    v.normal = n;
    v.tangent = glm::vec3(0.0f, 1.0f, 0.0f);
    v.position = {x0, y0, 0.0f}; v.uv = {0.0f, 0.0f}; verts.push_back(v);
    v.position = {x1, y0, 0.0f}; v.uv = {1.0f, 0.0f}; verts.push_back(v);
    v.position = {x1, y1, 0.0f}; v.uv = {1.0f, 1.0f}; verts.push_back(v);
    v.position = {x0, y1, 0.0f}; v.uv = {0.0f, 1.0f}; verts.push_back(v);
    indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
    indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
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

ShadedMaterial MakeMaterial() {
    ShadedMaterial material;
    material.albedo = glm::vec3(0.8f);
    material.metallic = 0.0f;
    material.roughness = 0.5f;
    material.ao = 1.0f;
    material.shading_mode = 0;  // PBR
    return material;
}

enum class ViewMode { None, ForceUnlit, Wireframe, Overdraw };

// 在指定视图模式作用域内用 DrawShaded 绘制给定几何，回读颜色。
RenderTargetReadback RenderWithMode(RhiDevice& device, ViewMode mode,
                                    const std::vector<MeshVertex>& verts,
                                    const std::vector<uint16_t>& indices) {
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

    const ShadedMaterial material = MakeMaterial();
    const glm::mat4 proj = device.GetProjectionCorrection();
    const glm::mat4 view(1.0f);
    const glm::mat4 model(1.0f);
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

        if (mode == ViewMode::Wireframe) device.SetWireframeMode(true);
        else if (mode == ViewMode::Overdraw) device.SetOverdrawMode(true);
        else if (mode == ViewMode::ForceUnlit) device.SetForceUnlit(true);

        renderer.DrawShaded(*cmd, device, verts, indices, model, view, proj, cam_pos,
                            material, MakeLightAlongX());

        if (mode == ViewMode::Wireframe) device.SetWireframeMode(false);
        else if (mode == ViewMode::Overdraw) device.SetOverdrawMode(false);
        else if (mode == ViewMode::ForceUnlit) device.SetForceUnlit(false);

        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    return rb;
}

// --- force_unlit：法线 +Z 居中面片，基线（无模式）暗、force_unlit 亮 ---
std::vector<MeshVertex> g_unlit_verts;
std::vector<uint16_t> g_unlit_indices;
void EnsureUnlitGeom() {
    if (!g_unlit_verts.empty()) return;
    AppendQuad(g_unlit_verts, g_unlit_indices, -0.6f, 0.6f, -0.6f, 0.6f, glm::vec3(0.0f, 0.0f, 1.0f));
}
RenderTargetReadback RenderUnlitBaseline(RhiDevice& device) {
    EnsureUnlitGeom();
    return RenderWithMode(device, ViewMode::None, g_unlit_verts, g_unlit_indices);
}
RenderTargetReadback RenderForceUnlit(RhiDevice& device) {
    EnsureUnlitGeom();
    return RenderWithMode(device, ViewMode::ForceUnlit, g_unlit_verts, g_unlit_indices);
}

// --- wireframe：法线 +X 大面片，实心 vs 线框 ---
std::vector<MeshVertex> g_wire_verts;
std::vector<uint16_t> g_wire_indices;
void EnsureWireGeom() {
    if (!g_wire_verts.empty()) return;
    AppendQuad(g_wire_verts, g_wire_indices, -0.7f, 0.7f, -0.7f, 0.7f, glm::vec3(1.0f, 0.0f, 0.0f));
}
RenderTargetReadback RenderWireSolid(RhiDevice& device) {
    EnsureWireGeom();
    return RenderWithMode(device, ViewMode::None, g_wire_verts, g_wire_indices);
}
RenderTargetReadback RenderWireframe(RhiDevice& device) {
    EnsureWireGeom();
    return RenderWithMode(device, ViewMode::Wireframe, g_wire_verts, g_wire_indices);
}

// --- overdraw：分层重叠面片（法线 +X），加性叠加 ---
// 固定低强度 overdraw 材质单层很暗，故堆 3 层放大对照：全宽块铺满 1 层，中央窄块再叠 2 层。
// 采样左区（x≈60 → ndc≈-0.53，仅全宽块=1 层）vs 中央（x=128 → ndc 0，三块=3 层）。
std::vector<MeshVertex> g_over_verts;
std::vector<uint16_t> g_over_indices;
void EnsureOverGeom() {
    if (!g_over_verts.empty()) return;
    const glm::vec3 nx(1.0f, 0.0f, 0.0f);
    AppendQuad(g_over_verts, g_over_indices, -0.7f, 0.7f, -0.5f, 0.5f, nx);  // 全宽（1 层铺满）
    AppendQuad(g_over_verts, g_over_indices, -0.3f, 0.3f, -0.5f, 0.5f, nx);  // 中央窄块（+1 层）
    AppendQuad(g_over_verts, g_over_indices, -0.3f, 0.3f, -0.5f, 0.5f, nx);  // 中央窄块（+1 层）
}
RenderTargetReadback RenderOverdraw(RhiDevice& device) {
    EnsureOverGeom();
    return RenderWithMode(device, ViewMode::Overdraw, g_over_verts, g_over_indices);
}

int CountLit(const RenderTargetReadback& rb, int threshold) {
    int n = 0;
    for (size_t i = 0; i + 3 < rb.pixels.size(); i += 4) {
        if (rb.pixels[i] > static_cast<unsigned char>(threshold)) ++n;
    }
    return n;
}

}  // namespace

// ============================================================
// force_unlit：基线（法线⊥光）中心仅环境光偏暗；force_unlit 走 Unlit 分支输出纯 albedo 明显变亮。
// ============================================================
void RunForceUnlit(dse::test::BackendResult baseline, dse::test::BackendResult unlit, const char* backend) {
    if (!unlit.available) GTEST_SKIP() << unlit.skip_reason;
    if (baseline.readback.pixels.empty() || unlit.readback.pixels.empty())
        GTEST_SKIP() << "ForwardShaded builtin program unavailable (" << backend << ")";
    const int cx = kRtSize / 2, cy = kRtSize / 2;
    const unsigned char* base_c = dse::test::PixelAt(baseline.readback, cx, cy);
    const unsigned char* unlit_c = dse::test::PixelAt(unlit.readback, cx, cy);
    ASSERT_NE(base_c, nullptr) << backend;
    ASSERT_NE(unlit_c, nullptr) << backend;
    EXPECT_LT(base_c[0], 90) << backend << " baseline center (lit, N⊥L → dim ambient)";
    EXPECT_GT(unlit_c[0], 120) << backend << " force_unlit center (flat albedo)";
    EXPECT_GT(static_cast<int>(unlit_c[0]) - static_cast<int>(base_c[0]), 40) << backend << " unlit brighter";
}
TEST(EditorViewModePixelSmokeTest, OpenGLForceUnlit) {
    RunForceUnlit(dse::test::RunOpenGL(RenderUnlitBaseline), dse::test::RunOpenGL(RenderForceUnlit), "OpenGL");
}
TEST(EditorViewModePixelSmokeTest, D3D11ForceUnlit) {
    RunForceUnlit(dse::test::RunD3D11(RenderUnlitBaseline), dse::test::RunD3D11(RenderForceUnlit), "D3D11");
}
TEST(EditorViewModePixelSmokeTest, VulkanForceUnlit) {
    RunForceUnlit(dse::test::RunVulkan(RenderUnlitBaseline), dse::test::RunVulkan(RenderForceUnlit), "Vulkan");
}

// ============================================================
// wireframe：实心填满大片；线框仅描边 → 亮像素数远少于实心，且 >0。
// ============================================================
void RunWireframe(dse::test::BackendResult solid, dse::test::BackendResult wire, const char* backend) {
    if (!wire.available) GTEST_SKIP() << wire.skip_reason;
    if (solid.readback.pixels.empty() || wire.readback.pixels.empty())
        GTEST_SKIP() << "ForwardShaded builtin program unavailable (" << backend << ")";
    const int solid_lit = CountLit(solid.readback, 80);
    const int wire_lit = CountLit(wire.readback, 80);
    EXPECT_GT(solid_lit, 4000) << backend << " solid fills a large area";
    EXPECT_GT(wire_lit, 50) << backend << " wireframe draws edges";
    EXPECT_LT(wire_lit, solid_lit / 4) << backend << " wireframe << solid (only edges)";
}
TEST(EditorViewModePixelSmokeTest, OpenGLWireframe) {
    RunWireframe(dse::test::RunOpenGL(RenderWireSolid), dse::test::RunOpenGL(RenderWireframe), "OpenGL");
}
TEST(EditorViewModePixelSmokeTest, D3D11Wireframe) {
    RunWireframe(dse::test::RunD3D11(RenderWireSolid), dse::test::RunD3D11(RenderWireframe), "D3D11");
}
TEST(EditorViewModePixelSmokeTest, VulkanWireframe) {
    RunWireframe(dse::test::RunVulkan(RenderWireSolid), dse::test::RunVulkan(RenderWireframe), "Vulkan");
}

// ============================================================
// overdraw：加性混合下重叠区（两块覆盖）亮度叠加，明显亮于单覆盖区。
// ============================================================
void RunOverdraw(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardShaded builtin program unavailable (" << backend << ")";
    const int cy = kRtSize / 2;
    const unsigned char* overlap = dse::test::PixelAt(r.readback, 128, cy);  // 中央 3 层
    const unsigned char* single = dse::test::PixelAt(r.readback, 60, cy);    // 左区 1 层
    const unsigned char* corner = dse::test::PixelAt(r.readback, 6, 6);      // 背景
    ASSERT_NE(overlap, nullptr) << backend;
    ASSERT_NE(single, nullptr) << backend;
    ASSERT_NE(corner, nullptr) << backend;
    EXPECT_GT(single[0], 4) << backend << " single-coverage lit (fixed overdraw color)";
    EXPECT_GT(static_cast<int>(overlap[0]) - static_cast<int>(single[0]), 10)
        << backend << " overlap brighter (additive accumulation of 3 layers)";
    EXPECT_LT(corner[0], 8) << backend << " background dark";
}
TEST(EditorViewModePixelSmokeTest, OpenGLOverdraw) { RunOverdraw(dse::test::RunOpenGL(RenderOverdraw), "OpenGL"); }
TEST(EditorViewModePixelSmokeTest, D3D11Overdraw) { RunOverdraw(dse::test::RunD3D11(RenderOverdraw), "D3D11"); }
TEST(EditorViewModePixelSmokeTest, VulkanOverdraw) { RunOverdraw(dse::test::RunVulkan(RenderOverdraw), "Vulkan"); }
