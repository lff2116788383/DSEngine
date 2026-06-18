/**
 * @file instanced_ssbo_pixel_smoke_test.cpp
 * @brief P0a + P0b 组合「活体」像素冒烟 — 用自定义三语言着色器，经通用原语
 *        DrawIndexedInstanced（P0a）+ CommandBuffer::BindStorageBuffer（P0b）
 *        画 N 个实例：每实例从 SSBO（按 gl_InstanceID/SV_InstanceID 索引）取 X 偏移与颜色，
 *        断言三后端各画出 N 个互不重叠、颜色正确的 quad，且跨后端 RMSE 在阈值内。
 *
 * 这是 RHI_PRIMITIVE_CONTRACT.md §7「每个原语随活体消费者 + 像素测试落地」闸门：
 *  - P0a：DrawIndexedInstanced(index_count, instance_count, ...) 实例化绘制。
 *  - P0b：BindStorageBuffer(slot, handle, offset, size) 图形阶段 SSBO 绑定。
 *
 * 设计要点：
 *  - 顶点拉取（gl_VertexID/SV_VertexID 生成 quad 角点），不依赖顶点属性 → 避免 DX11
 *    自定义着色器无 InputLayout 的限制；仍绑定一个占位 VBO 以满足各后端 null 检查。
 *  - 实例数据为 std430 数组：每实例 32 字节 = vec4 offset + vec4 color（DX11 ByteAddressBuffer
 *    Load2/Load4 按相同字节布局取数）。
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

// std430 / ByteAddressBuffer 共用字节布局：每实例 32 字节。
struct InstanceGPU {
    float offset[4];  // xy = 偏移；zw 未用
    float color[4];   // rgba
};

// 三实例：X 偏移互不重叠（中心 NDC -0.5/0/+0.5 → 像素 64/128/192），Y 居中（横排，y 对称）。
std::vector<InstanceGPU> MakeInstances() {
    return {
        {{-0.5f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // 红
        {{ 0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},  // 绿
        {{ 0.5f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},  // 蓝
    };
}

// ---- GL GLSL (#version 430) ----
const char* kGlVert = R"(#version 430
struct Inst { vec4 offset; vec4 color; };
layout(std430, binding = 0) readonly buffer InstBuf { Inst items[]; };
out vec4 vColor;
void main() {
    vec2 corners[4] = vec2[4](vec2(-1.0,-1.0), vec2(1.0,-1.0), vec2(1.0,1.0), vec2(-1.0,1.0));
    vec2 p = corners[gl_VertexID] * float(0.12);
    vec2 off = items[gl_InstanceID].offset.xy;
    gl_Position = vec4(p + off, 0.0, 1.0);
    vColor = items[gl_InstanceID].color;
}
)";

const char* kGlFrag = R"(#version 430
in vec4 vColor;
out vec4 FragColor;
void main() { FragColor = vColor; }
)";

// ---- Vulkan GLSL (#version 450 → glslang → SPIR-V) ----
const char* kVkVert = R"(#version 450
struct Inst { vec4 offset; vec4 color; };
layout(std430, set = 0, binding = 0) readonly buffer InstBuf { Inst items[]; };
layout(location = 0) out vec4 vColor;
void main() {
    vec2 corners[4] = vec2[4](vec2(-1.0,-1.0), vec2(1.0,-1.0), vec2(1.0,1.0), vec2(-1.0,1.0));
    vec2 p = corners[gl_VertexIndex] * float(0.12);
    vec2 off = items[gl_InstanceIndex].offset.xy;
    gl_Position = vec4(p + off, 0.0, 1.0);
    vColor = items[gl_InstanceIndex].color;
}
)";

const char* kVkFrag = R"(#version 450
layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vColor; }
)";

// ---- DX11 HLSL (VSMain/PSMain, ByteAddressBuffer t0) ----
const char* kDxVert = R"(
ByteAddressBuffer items : register(t0);
struct VSOut { float4 pos : SV_Position; float4 color : COLOR0; };
VSOut VSMain(uint vid : SV_VertexID, uint iid : SV_InstanceID) {
    float2 corners[4] = { float2(-1.0,-1.0), float2(1.0,-1.0), float2(1.0,1.0), float2(-1.0,1.0) };
    float2 p = corners[vid] * 0.12;
    float2 off = asfloat(items.Load2(iid * 32));
    float4 col = asfloat(items.Load4(iid * 32 + 16));
    VSOut o;
    o.pos = float4(p + off, 0.0, 1.0);
    o.color = col;
    return o;
}
float4 PSMain(VSOut i) : SV_Target { return i.color; }
)";

// 在已初始化 device 上：建 256² RT → 清黑 → 自定义着色器 + SSBO 实例化画 N 个 quad → 回读。
RenderTargetReadback RenderInstancedSSBO(RhiDevice& device,
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

    // 实例数据 SSBO（kStorage）。
    std::vector<InstanceGPU> instances = MakeInstances();
    GpuBufferDesc ssbo_desc;
    ssbo_desc.size = instances.size() * sizeof(InstanceGPU);
    ssbo_desc.usage = GpuBufferUsage::kStorage;
    BufferHandle ssbo = device.CreateGpuBuffer(ssbo_desc, instances.data());

    // 占位 VBO（顶点拉取不读属性，但各后端通用绘制路径要求 VBO 非空）。
    const float dummy_vtx[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GpuBufferDesc vb_desc;
    vb_desc.size = sizeof(dummy_vtx);
    vb_desc.usage = GpuBufferUsage::kVertex;
    BufferHandle vbo = device.CreateGpuBuffer(vb_desc, dummy_vtx);

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

    if (!ssbo || !vbo || !ibo) {
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
        cmd->SetPipelineState(pso);
        cmd->BindShaderProgram(program);
        cmd->BindVertexBuffer(vbo.raw(), sizeof(float) * 2, {});  // 无属性，仅占位
        cmd->BindIndexBuffer(ibo.raw(), IndexType::UInt16);
        cmd->BindStorageBuffer(0, ssbo.raw(), 0, 0);              // P0b：图形阶段 SSBO → slot 0
        cmd->DrawIndexedInstanced(6, kInstanceCount, 0, 0, 0);   // P0a：实例化绘制
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();  // fence 等待完成后删除资源才安全（Vulkan 严格）

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);

    device.DeleteGpuBuffer(ssbo);
    device.DeleteGpuBuffer(vbo);
    device.DeleteGpuBuffer(ibo);
    device.DeleteShaderProgram(program);
    device.DeleteRenderTarget(rt);
    return rb;
}

RenderTargetReadback RenderGL(RhiDevice& d) { return RenderInstancedSSBO(d, kGlVert, kGlFrag); }
RenderTargetReadback RenderVK(RhiDevice& d) { return RenderInstancedSSBO(d, kVkVert, kVkFrag); }
RenderTargetReadback RenderDX(RhiDevice& d) { return RenderInstancedSSBO(d, kDxVert, kDxVert); }

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
// 三后端单独像素验证（活体消费 P0a + P0b）。
// ============================================================

TEST(InstancedSSBOPixelSmokeTest, OpenGLDrawsThreeInstancedQuads) {
    auto r = dse::test::RunOpenGL(RenderGL);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyThreeQuads(r.readback, "OpenGL");
}

TEST(InstancedSSBOPixelSmokeTest, D3D11DrawsThreeInstancedQuads) {
    auto r = dse::test::RunD3D11(RenderDX);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyThreeQuads(r.readback, "D3D11");
}

TEST(InstancedSSBOPixelSmokeTest, VulkanDrawsThreeInstancedQuads) {
    auto r = dse::test::RunVulkan(RenderVK);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyThreeQuads(r.readback, "Vulkan");
}

// ============================================================
// 跨后端一致性：场景沿 y=128 对称，DX11 垂直翻转不影响整帧 RMSE。
// ============================================================

TEST(InstancedSSBOPixelSmokeTest, CrossBackendConsistent) {
    auto gl = dse::test::RunOpenGL(RenderGL);
    auto dx = dse::test::RunD3D11(RenderDX);
    auto vk = dse::test::RunVulkan(RenderVK);
    int available = (gl.available ? 1 : 0) + (dx.available ? 1 : 0) + (vk.available ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "need >=2 backends for cross-backend RMSE";

    const double kRmseGate = 8.0;  // 软件光栅器友好（边缘 AA / 翻转 off-by-one）
    if (gl.available && vk.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, vk.readback);
        fprintf(stderr, "[P0a+P0b] GL-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs Vulkan diverged";
    }
    if (gl.available && dx.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, dx.readback);
        fprintf(stderr, "[P0a+P0b] GL-vs-D3D11 RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs D3D11 diverged";
    }
    if (dx.available && vk.available) {
        double rmse = dse::test::ComputeRmse(dx.readback, vk.readback);
        fprintf(stderr, "[P0a+P0b] D3D11-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "D3D11 vs Vulkan diverged";
    }
}
