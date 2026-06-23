/**
 * @file bind_group_pixel_smoke_test.cpp
 * @brief B「活体」像素冒烟 — 用 CommandBuffer::BindGroup 把多个 UBO 打包为一次原子绑定
 *        （契约 §2.3 绑定组 / argument buffer），画一个居中 quad，片元色 = tintA * tintB，
 *        断言三后端中央像素为两组 UBO 乘积色、四角为清屏黑。
 *
 * 这是 RHI_PRIMITIVE_CONTRACT.md §7「每个原语随活体消费者 + 像素测试落地」闸门，验证 BindGroup
 * 把一组 UBO 绑定汇成一次提交后，各 slot（b0/b1）仍被正确分发到着色器：
 *  - 顶点由 gl_VertexID/SV_VertexID 生成（不依赖顶点属性，避免运行期自定义着色器无反射输入布局的限制）。
 *  - 仅绑一个占位 VBO 满足各后端通用绘制路径的 null 检查。
 *  - 另有生产消费者 SpriteRenderer 经 BindGroup 绑定 {PerFrame UBO + u_texture}，
 *    其像素闸门见 sprite_primitive_smoke_test / sprite_batch_*；本测试专门覆盖「多 UBO 成组」。
 *
 * 场景沿 RT 中心对称，DX11 垂直翻转不影响整帧 RMSE。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/render/rhi/rhi_handle.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// std140 / cbuffer 共用：每个 UBO 一个 vec4（16 字节）。
struct TintUBO {
    float rgba[4];
};

// tintA * tintB = (0.5*2, 0, 0, 1) = 红。
TintUBO MakeTintA() { return {{0.5f, 0.0f, 0.0f, 1.0f}}; }
TintUBO MakeTintB() { return {{2.0f, 1.0f, 1.0f, 1.0f}}; }

// ---- GL GLSL (#version 430) ----
const char* kGlVert = R"(#version 430
void main() {
    vec2 corners[4] = vec2[4](vec2(-1.0,-1.0), vec2(1.0,-1.0), vec2(1.0,1.0), vec2(-1.0,1.0));
    gl_Position = vec4(corners[gl_VertexID] * 0.5, 0.0, 1.0);
}
)";

const char* kGlFrag = R"(#version 430
out vec4 FragColor;
layout(std140, binding = 0) uniform TintA { vec4 tintA; };
layout(std140, binding = 1) uniform TintB { vec4 tintB; };
void main() { FragColor = tintA * tintB; }
)";

// ---- Vulkan GLSL (#version 450 → glslang → SPIR-V) ----
const char* kVkVert = R"(#version 450
void main() {
    vec2 corners[4] = vec2[4](vec2(-1.0,-1.0), vec2(1.0,-1.0), vec2(1.0,1.0), vec2(-1.0,1.0));
    gl_Position = vec4(corners[gl_VertexIndex] * 0.5, 0.0, 1.0);
}
)";

const char* kVkFrag = R"(#version 450
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 0) uniform TintA { vec4 tintA; };
layout(set = 0, binding = 1) uniform TintB { vec4 tintB; };
void main() { FragColor = tintA * tintB; }
)";

// ---- DX11 HLSL (VSMain/PSMain；UBO → b0/b1) ----
const char* kDxVert = R"(
cbuffer TintA : register(b0) { float4 tintA; };
cbuffer TintB : register(b1) { float4 tintB; };
struct VSOut { float4 pos : SV_Position; };
VSOut VSMain(uint vid : SV_VertexID) {
    float2 corners[4] = { float2(-1.0,-1.0), float2(1.0,-1.0), float2(1.0,1.0), float2(-1.0,1.0) };
    VSOut o;
    o.pos = float4(corners[vid] * 0.5, 0.0, 1.0);
    return o;
}
float4 PSMain(VSOut i) : SV_Target { return tintA * tintB; }
)";

// 在已初始化 device 上：建 256² RT → 清黑 → BindGroup({b0,b1}) 画居中 quad → 回读。
RenderTargetReadback RenderBindGroupQuad(RhiDevice& device,
                                         const std::string& vert_src,
                                         const std::string& frag_src) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    unsigned int program = device.CreateShaderProgram(vert_src, frag_src);
    if (program == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    TintUBO tint_a = MakeTintA();
    TintUBO tint_b = MakeTintB();
    GpuBufferDesc ub_desc;
    ub_desc.size = sizeof(TintUBO);
    ub_desc.usage = GpuBufferUsage::kUniform;
    BufferHandle ubo_a = device.CreateGpuBuffer(ub_desc, &tint_a);
    BufferHandle ubo_b = device.CreateGpuBuffer(ub_desc, &tint_b);

    // 占位 VBO（顶点拉取不读属性，但各后端通用绘制路径要求 VBO 非空）。
    const float dummy_vtx[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GpuBufferDesc vb_desc;
    vb_desc.size = sizeof(dummy_vtx);
    vb_desc.usage = GpuBufferUsage::kVertex;
    BufferHandle vbo = device.CreateGpuBuffer(vb_desc, dummy_vtx);

    const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
    GpuBufferDesc ib_desc;
    ib_desc.size = sizeof(indices);
    ib_desc.usage = GpuBufferUsage::kIndex;
    BufferHandle ibo = device.CreateGpuBuffer(ib_desc, indices);

    PipelineStateDesc pso_desc;
    pso_desc.blend_enabled = false;
    pso_desc.depth_test_enabled = false;
    pso_desc.depth_write_enabled = false;
    pso_desc.culling_enabled = false;
    unsigned int pso = device.CreatePipelineState(pso_desc);

    if (!ubo_a || !ubo_b || !vbo || !ibo) {
        device.DeleteShaderProgram(program);
        device.DeleteRenderTarget(rt);
        return {};
    }

    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        cmd->BindPipeline(device.GetGraphicsPipeline(pso, program));
        cmd->BindVertexBuffer(0u, vbo.raw(), sizeof(float) * 2, {});  // 占位
        cmd->BindIndexBuffer(ibo.raw(), IndexType::UInt16);
        // B：两个 UBO 成组，一次 BindGroup 绑定 b0/b1。
        BindGroupDesc group;
        group.uniform_buffers.push_back({0u, ubo_a.raw(), 0u, 0u});
        group.uniform_buffers.push_back({1u, ubo_b.raw(), 0u, 0u});
        cmd->BindGroup(group);
        cmd->DrawIndexed(6, 0, 0);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);

    device.DeleteGpuBuffer(ubo_a);
    device.DeleteGpuBuffer(ubo_b);
    device.DeleteGpuBuffer(vbo);
    device.DeleteGpuBuffer(ibo);
    device.DeleteShaderProgram(program);
    device.DeleteRenderTarget(rt);
    return rb;
}

RenderTargetReadback RenderGL(RhiDevice& d) { return RenderBindGroupQuad(d, kGlVert, kGlFrag); }
RenderTargetReadback RenderVK(RhiDevice& d) { return RenderBindGroupQuad(d, kVkVert, kVkFrag); }
RenderTargetReadback RenderDX(RhiDevice& d) { return RenderBindGroupQuad(d, kDxVert, kDxVert); }

// 中央像素为红（tintA*tintB），四角为清屏黑。
void VerifyTintedQuad(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const unsigned char* center = dse::test::PixelAt(rb, kRtSize / 2, kRtSize / 2);
    ASSERT_NE(center, nullptr) << backend;
    EXPECT_GT(center[0], 128) << backend << " center R (tintA.r*tintB.r)";
    EXPECT_LT(center[1], 64) << backend << " center G";
    EXPECT_LT(center[2], 64) << backend << " center B";

    const int margin = 8;
    const unsigned char* corner = dse::test::PixelAt(rb, margin, margin);
    ASSERT_NE(corner, nullptr) << backend;
    EXPECT_LT(corner[0], 24) << backend << " corner clear-black R";
    EXPECT_LT(corner[1], 24) << backend << " corner clear-black G";
    EXPECT_LT(corner[2], 24) << backend << " corner clear-black B";
}

}  // namespace

// ============================================================
// 三后端单独像素验证（活体消费 BindGroup：多 UBO 成组）。
// ============================================================

TEST(BindGroupPixelSmokeTest, OpenGLDrawsTintedQuad) {
    auto r = dse::test::RunOpenGL(RenderGL);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyTintedQuad(r.readback, "OpenGL");
}

TEST(BindGroupPixelSmokeTest, D3D11DrawsTintedQuad) {
    auto r = dse::test::RunD3D11(RenderDX);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyTintedQuad(r.readback, "D3D11");
}

TEST(BindGroupPixelSmokeTest, VulkanDrawsTintedQuad) {
    auto r = dse::test::RunVulkan(RenderVK);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyTintedQuad(r.readback, "Vulkan");
}

// ============================================================
// 跨后端一致性：场景中心对称，DX11 垂直翻转不影响整帧 RMSE。
// ============================================================

TEST(BindGroupPixelSmokeTest, CrossBackendConsistent) {
    auto gl = dse::test::RunOpenGL(RenderGL);
    auto dx = dse::test::RunD3D11(RenderDX);
    auto vk = dse::test::RunVulkan(RenderVK);
    int available = (gl.available ? 1 : 0) + (dx.available ? 1 : 0) + (vk.available ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "need >=2 backends for cross-backend RMSE";

    const double kRmseGate = 8.0;
    if (gl.available && vk.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, vk.readback);
        fprintf(stderr, "[B] GL-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs Vulkan diverged";
    }
    if (gl.available && dx.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, dx.readback);
        fprintf(stderr, "[B] GL-vs-D3D11 RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs D3D11 diverged";
    }
    if (dx.available && vk.available) {
        double rmse = dse::test::ComputeRmse(dx.readback, vk.readback);
        fprintf(stderr, "[B] D3D11-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "D3D11 vs Vulkan diverged";
    }
}
