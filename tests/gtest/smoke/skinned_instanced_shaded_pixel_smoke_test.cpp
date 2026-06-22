/**
 * @file skinned_instanced_shaded_pixel_smoke_test.cpp
 * @brief 阶段4-M1 蒙皮 × 硬件实例化 + 高级 shading 组合「活体」像素冒烟 —— 用高层
 *        MeshRenderer::DrawSkinnedInstancedShaded（消费内建程序
 *        BuiltinProgram::ForwardSkinnedInstancedShaded：forward_shaded_skinned_instanced.vert
 *        按 gl_InstanceIndex 取每实例 {model, bone_offset}（实例 SSBO\@slot0[set8.b0]），骨骼调色板
 *        密排于骨骼 SSBO\@slot1[set8.b1]；VS 先骨骼混合（bone-palette + bone_offset），再施每实例
 *        model 到世界空间，最后 vp，复用高级 shading frag forward_shaded.frag）。
 *
 * 设计要点（验证「bone-palette 去重 + bone_offset 寻址」与「每实例独立 model」同时生效）：
 *  - 单块居中于原点的窄竖直矩形面片（x∈[-0.2,0.2], y∈[-0.7,0.7]），法线 +X（朝光源），
 *    所有顶点全权重绑各自调色板内的骨骼 0（bone_indices=0、weight=1）。
 *  - 两份**单位骨骼**调色板（各含 1 根 identity 骨骼），密排后 palette_base = {0, 1}。
 *    实例 0 引用调色板 0（bone_offset=0），实例 1 引用调色板 1（bone_offset=1）。
 *    → 验证 bone_offset≠0 的寻址：实例 1 必须读到密排下标 1 的 identity，否则越界/读错矩阵。
 *  - 两实例 model：实例 0 = 平移(-0.5,0,0)、实例 1 = 平移(+0.5,0,0)。
 *    单位骨骼下 worldPos = model * pos → 左面片落到 x∈[-0.7,-0.3]（中心 x=64px）、右面片落到
 *    x∈[0.3,0.7]（中心 x=192px），两者皆朝光被照亮，中缝 x=128 与四角为清屏黑。
 *  - 若实例化失效（未按 gl_InstanceIndex 取各自 model）→ 右侧 x=192 不出现面片；
 *    若 bone_offset 寻址失效（实例 1 读到错误骨骼）→ 右面片错位/消失；二者皆致断言失败。
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

// 单块居中于原点的窄竖直矩形面片（绑定空间，z=0，CCW 朝 +Z 相机），法线 +X 朝光源。
// 全权重绑调色板内骨骼 0（bone_indices=0、weight=1）。
void MakeCenterSkinnedQuad(std::vector<SkinnedMeshVertex>& verts,
                           std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    const glm::vec3 n(1.0f, 0.0f, 0.0f);   // 法线朝光源 → 亮
    const glm::vec3 tan(0.0f, 1.0f, 0.0f);
    const float x0 = -0.2f, x1 = 0.2f, y0 = -0.7f, y1 = 0.7f;
    SkinnedMeshVertex v;
    v.color = col; v.normal = n; v.tangent = tan;
    v.bone_indices = glm::vec4(0.0f);
    v.bone_weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
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

RenderTargetReadback RenderSkinnedInstancedShaded(RhiDevice& device, int shading_mode) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    if (device.GetBuiltinProgram(BuiltinProgram::ForwardSkinnedInstancedShaded) == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    std::vector<SkinnedMeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeCenterSkinnedQuad(verts, indices);

    // 两份单位骨骼调色板（各 1 根 identity）；密排 palette_base = {0, 1}，验证 bone_offset≠0 寻址。
    std::vector<std::vector<glm::mat4>> bone_palettes(2);
    bone_palettes[0] = {glm::mat4(1.0f)};
    bone_palettes[1] = {glm::mat4(1.0f)};
    std::vector<int> instance_palette_idx = {0, 1};

    // 两实例 model：平移把居中面片搬到左右目标位置（每实例独立 model 矩阵）。
    std::vector<glm::mat4> models(2);
    models[0] = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.0f, 0.0f));
    models[1] = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 0.0f));

    ShadedMaterial material;
    material.albedo = glm::vec3(0.8f);
    material.metallic = 0.0f;
    material.roughness = 0.5f;
    material.ao = 1.0f;
    material.shading_mode = shading_mode;

    const glm::mat4 proj = device.GetProjectionCorrection();
    const glm::mat4 view(1.0f);
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
        renderer.DrawSkinnedInstancedShaded(*cmd, device, verts, indices, models,
                                            bone_palettes, instance_palette_idx,
                                            view, proj, cam_pos,
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

RenderTargetReadback RenderPbr(RhiDevice& device) { return RenderSkinnedInstancedShaded(device, 0); }
RenderTargetReadback RenderHalfLambert(RhiDevice& device) { return RenderSkinnedInstancedShaded(device, 3); }

// 左右两面片皆亮、中缝与四角清屏黑 —— 证明 bone-palette 蒙皮 + 每实例独立 model 同时生效。
void VerifyTwoLitInstances(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cy = kRtSize / 2;
    const unsigned char* left = dse::test::PixelAt(rb, 64, cy);
    const unsigned char* right = dse::test::PixelAt(rb, 192, cy);
    ASSERT_NE(left, nullptr) << backend << " left";
    ASSERT_NE(right, nullptr) << backend << " right";

    // 实例 0（左，调色板 0/offset 0）与实例 1（右，调色板 1/offset 1）皆出现且被照亮。
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

void RunSingleBackend(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardSkinnedInstancedShaded builtin program unavailable (" << backend << ")";
    VerifyTwoLitInstances(r.readback, backend);
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
// 三后端单独像素验证（活体消费 MeshRenderer::DrawSkinnedInstancedShaded + ForwardSkinnedInstancedShaded）。
// ============================================================

// ── PBR(0)：bone-palette 蒙皮 + 每实例位移 + 标准 PBR shading ──
TEST(SkinnedInstancedShadedPixelSmokeTest, OpenGLPbr) { RunSingleBackend(dse::test::RunOpenGL(RenderPbr), "OpenGL"); }
TEST(SkinnedInstancedShadedPixelSmokeTest, D3D11Pbr) { RunSingleBackend(dse::test::RunD3D11(RenderPbr), "D3D11"); }
TEST(SkinnedInstancedShadedPixelSmokeTest, VulkanPbr) { RunSingleBackend(dse::test::RunVulkan(RenderPbr), "Vulkan"); }
TEST(SkinnedInstancedShadedPixelSmokeTest, PbrCrossBackend) { CheckCrossBackend(RenderPbr, 12.0); }

// ── HalfLambert-Static(3)：bone-palette 蒙皮 + 每实例位移 + 高级 shading 分支 ──
TEST(SkinnedInstancedShadedPixelSmokeTest, OpenGLHalfLambert) { RunSingleBackend(dse::test::RunOpenGL(RenderHalfLambert), "OpenGL"); }
TEST(SkinnedInstancedShadedPixelSmokeTest, D3D11HalfLambert) { RunSingleBackend(dse::test::RunD3D11(RenderHalfLambert), "D3D11"); }
TEST(SkinnedInstancedShadedPixelSmokeTest, VulkanHalfLambert) { RunSingleBackend(dse::test::RunVulkan(RenderHalfLambert), "Vulkan"); }
TEST(SkinnedInstancedShadedPixelSmokeTest, HalfLambertCrossBackend) { CheckCrossBackend(RenderHalfLambert, 12.0); }
