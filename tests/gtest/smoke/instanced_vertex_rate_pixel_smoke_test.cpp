/**
 * @file instanced_vertex_rate_pixel_smoke_test.cpp
 * @brief A「活体」像素冒烟 — 用 slot 化 BindVertexBuffer + VertexInputRate::PerInstance
 *        画 N 个实例：slot 0 顶点流（per-vertex，quad 角点）+ slot 1 实例流（per-instance，
 *        每实例 offset.xy + color.rgba），断言三后端各画出 N 个互不重叠、颜色正确的 quad。
 *
 * 这是 RHI_PRIMITIVE_CONTRACT.md §7「每个原语随活体消费者 + 像素测试落地」闸门，覆盖契约 §3
 * BindVertexBuffer 终态签名的两项新能力：
 *  - 多 slot 顶点流：BindVertexBuffer(slot=0, ...) + BindVertexBuffer(slot=1, ...)。
 *  - per-instance 步进：slot 1 走 VertexInputRate::PerInstance
 *    （GL glVertexAttribDivisor(loc,1) / DX11 D3D11_INPUT_PER_INSTANCE_DATA / Vulkan
 *    VK_VERTEX_INPUT_RATE_INSTANCE），实例顶点流不再靠 SSBO 绕过。
 *
 * 设计要点（与 instanced_ssbo_pixel_smoke_test 同构，便于对照）：
 *  - slot 0：4 个角点 vec2（已乘半边长），per-vertex；location 0。
 *  - slot 1：每实例 24 字节 = vec2 offset（location 1）+ vec4 color（location 2），per-instance。
 *  - 场景沿 y=128 对称（quad 居中成一横排），故 DX11 垂直翻转不影响整帧 RMSE 比较。
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
constexpr uint32_t kInstanceCount = 3;
constexpr float kHalf = 0.12f;  // quad 裁剪空间半边长

// slot 1 实例顶点流字节布局：每实例 24 字节 = vec2 offset + vec4 color。
struct InstanceVertex {
    float offset[2];  // location 1
    float color[4];   // location 2
};

// slot 0 顶点流：quad 4 角点（已乘半边长），per-vertex。
const float kQuadCorners[8] = {
    -kHalf, -kHalf,
     kHalf, -kHalf,
     kHalf,  kHalf,
    -kHalf,  kHalf,
};

// 三实例：X 偏移互不重叠（中心 NDC -0.5/0/+0.5 → 像素 64/128/192），Y 居中（横排，y 对称）。
std::vector<InstanceVertex> MakeInstances() {
    return {
        {{-0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // 红
        {{ 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},  // 绿
        {{ 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},  // 蓝
    };
}

// ---- GL GLSL (#version 430) ----
const char* kGlVert = R"(#version 430
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aOffset;
layout(location = 2) in vec4 aColor;
out vec4 vColor;
void main() {
    gl_Position = vec4(aPos + aOffset, 0.0, 1.0);
    vColor = aColor;
}
)";

const char* kGlFrag = R"(#version 430
in vec4 vColor;
out vec4 FragColor;
void main() { FragColor = vColor; }
)";

// ---- Vulkan GLSL (#version 450 → glslang → SPIR-V) ----
const char* kVkVert = R"(#version 450
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aOffset;
layout(location = 2) in vec4 aColor;
layout(location = 0) out vec4 vColor;
void main() {
    gl_Position = vec4(aPos + aOffset, 0.0, 1.0);
    vColor = aColor;
}
)";

const char* kVkFrag = R"(#version 450
layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vColor; }
)";

// ---- DX11 HLSL (VSMain/PSMain) ----
// 通用原语 attr-derived InputLayout 把 location N 映射为语义 TEXCOORD<N>（见 DX11DrawExecutor::ResolvePrimInputLayout）。
const char* kDxVert = R"(
struct VSIn  { float2 pos : TEXCOORD0; float2 off : TEXCOORD1; float4 col : TEXCOORD2; };
struct VSOut { float4 pos : SV_Position; float4 color : COLOR0; };
VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos + i.off, 0.0, 1.0);
    o.color = i.col;
    return o;
}
float4 PSMain(VSOut i) : SV_Target { return i.color; }
)";

// 在已初始化 device 上：建 256² RT → 清黑 → slot0 顶点流 + slot1 per-instance 实例流画 N 个 quad → 回读。
RenderTargetReadback RenderInstancedVertexRate(RhiDevice& device,
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

    // slot 0：per-vertex 顶点流（quad 角点）。
    GpuBufferDesc vb_desc;
    vb_desc.size = sizeof(kQuadCorners);
    vb_desc.usage = GpuBufferUsage::kVertex;
    BufferHandle vbo = device.CreateGpuBuffer(vb_desc, kQuadCorners);

    // slot 1：per-instance 实例顶点流（offset + color）。
    std::vector<InstanceVertex> instances = MakeInstances();
    GpuBufferDesc inst_desc;
    inst_desc.size = instances.size() * sizeof(InstanceVertex);
    inst_desc.usage = GpuBufferUsage::kVertex;
    BufferHandle inst_vbo = device.CreateGpuBuffer(inst_desc, instances.data());

    // 索引缓冲：单 quad（4 角点 → 2 三角）。
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

    if (!vbo || !inst_vbo || !ibo) {
        device.DeleteShaderProgram(program);
        device.DeleteRenderTarget(rt);
        return {};
    }

    // slot 0 顶点布局：location 0 = vec2 pos @ offset 0。
    const std::vector<VertexAttr> slot0_attrs = { VertexAttr{0u, 2u, 0u} };
    // slot 1 实例布局：location 1 = vec2 offset @ 0；location 2 = vec4 color @ 8。
    const std::vector<VertexAttr> slot1_attrs = {
        VertexAttr{1u, 2u, 0u},
        VertexAttr{2u, 4u, 8u},
    };

    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        cmd->BindPipeline(device.GetGraphicsPipeline(pso, program));
        // A：slot 0 per-vertex + slot 1 per-instance 实例顶点流。
        cmd->BindVertexBuffer(0u, vbo.raw(), static_cast<uint32_t>(sizeof(float) * 2), slot0_attrs);
        cmd->BindVertexBuffer(1u, inst_vbo.raw(), static_cast<uint32_t>(sizeof(InstanceVertex)),
                              slot1_attrs, VertexInputRate::PerInstance);
        cmd->BindIndexBuffer(ibo.raw(), IndexType::UInt16);
        cmd->DrawIndexedInstanced(6, kInstanceCount, 0, 0, 0);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();  // fence 等待完成后删除资源才安全（Vulkan 严格）

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);

    device.DeleteGpuBuffer(vbo);
    device.DeleteGpuBuffer(inst_vbo);
    device.DeleteGpuBuffer(ibo);
    device.DeleteShaderProgram(program);
    device.DeleteRenderTarget(rt);
    return rb;
}

RenderTargetReadback RenderGL(RhiDevice& d) { return RenderInstancedVertexRate(d, kGlVert, kGlFrag); }
RenderTargetReadback RenderVK(RhiDevice& d) { return RenderInstancedVertexRate(d, kVkVert, kVkFrag); }
RenderTargetReadback RenderDX(RhiDevice& d) { return RenderInstancedVertexRate(d, kDxVert, kDxVert); }

// 断言：三个 quad 中心分别为 红/绿/蓝；quad 间隙与四角为清屏黑。
void VerifyThreeQuads(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const int cy = kRtSize / 2;  // 128，翻转轴
    struct Q { int cx; int r; int g; int b; const char* name; };
    const Q quads[3] = {
        {64,  1, 0, 0, "red"},    // off.x = -0.5
        {128, 0, 1, 0, "green"},  // off.x = 0
        {192, 0, 0, 1, "blue"},   // off.x = +0.5
    };
    for (const auto& q : quads) {
        const unsigned char* px = dse::test::PixelAt(rb, q.cx, cy);
        ASSERT_NE(px, nullptr) << backend << " quad " << q.name;
        EXPECT_EQ(px[0] > 128, q.r != 0) << backend << " quad " << q.name << " R";
        EXPECT_EQ(px[1] > 128, q.g != 0) << backend << " quad " << q.name << " G";
        EXPECT_EQ(px[2] > 128, q.b != 0) << backend << " quad " << q.name << " B";
    }

    // quad 间隙（px 96 / 160）应为清屏黑。
    for (int gx : {96, 160}) {
        const unsigned char* gap = dse::test::PixelAt(rb, gx, cy);
        ASSERT_NE(gap, nullptr) << backend;
        EXPECT_LT(gap[0], 32) << backend << " gap R @" << gx;
        EXPECT_LT(gap[1], 32) << backend << " gap G @" << gx;
        EXPECT_LT(gap[2], 32) << backend << " gap B @" << gx;
    }

    // 四角为清屏黑。
    const unsigned char* corner = dse::test::PixelAt(rb, 8, 8);
    ASSERT_NE(corner, nullptr) << backend;
    EXPECT_LT(corner[0], 32) << backend << " corner R";
    EXPECT_LT(corner[1], 32) << backend << " corner G";
    EXPECT_LT(corner[2], 32) << backend << " corner B";
}

}  // namespace

// ============================================================
// 三后端单独像素验证（活体消费 A：slot 化 + PerInstance）。
// ============================================================

TEST(InstancedVertexRatePixelSmokeTest, OpenGLDrawsThreeInstancedQuads) {
    auto r = dse::test::RunOpenGL(RenderGL);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyThreeQuads(r.readback, "OpenGL");
}

TEST(InstancedVertexRatePixelSmokeTest, D3D11DrawsThreeInstancedQuads) {
    auto r = dse::test::RunD3D11(RenderDX);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyThreeQuads(r.readback, "D3D11");
}

TEST(InstancedVertexRatePixelSmokeTest, VulkanDrawsThreeInstancedQuads) {
    auto r = dse::test::RunVulkan(RenderVK);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyThreeQuads(r.readback, "Vulkan");
}

// ============================================================
// 跨后端一致性：场景沿 y=128 对称，DX11 垂直翻转不影响整帧 RMSE。
// ============================================================

TEST(InstancedVertexRatePixelSmokeTest, CrossBackendConsistent) {
    auto gl = dse::test::RunOpenGL(RenderGL);
    auto dx = dse::test::RunD3D11(RenderDX);
    auto vk = dse::test::RunVulkan(RenderVK);
    int available = (gl.available ? 1 : 0) + (dx.available ? 1 : 0) + (vk.available ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "need >=2 backends for cross-backend RMSE";

    const double kRmseGate = 8.0;  // 软件光栅器友好（边缘 AA / 翻转 off-by-one）
    if (gl.available && vk.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, vk.readback);
        fprintf(stderr, "[A] GL-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs Vulkan diverged";
    }
    if (gl.available && dx.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, dx.readback);
        fprintf(stderr, "[A] GL-vs-D3D11 RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs D3D11 diverged";
    }
    if (dx.available && vk.available) {
        double rmse = dse::test::ComputeRmse(dx.readback, vk.readback);
        fprintf(stderr, "[A] D3D11-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "D3D11 vs Vulkan diverged";
    }
}
