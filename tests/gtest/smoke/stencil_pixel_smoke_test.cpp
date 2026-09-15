/**
 * @file stencil_pixel_smoke_test.cpp
 * @brief ADR-2 第 4 步：stencil（模板）真机像素用例  三后端逐一验证「模板掩码真的挡住几何」。
 *
 * 场景（三后端同一套语义）：
 *   1. RT = 256x256，has_color=true + has_depth=true + has_stencil=true，清屏黑
 *      （render pass 同时清 color / depth / stencil）；
 *   2. pass 1「写模板」：全屏 quad，PSO 的 stencil = Always / pass=Replace / ref=1 /
 *      writeMask=0xFF，片元着色器把 x >= 128 的像素 discard 掉
 *       只有左半屏被写成 stencil=1；
 *   3. pass 2「测模板」：全屏 quad，PSO 的 stencil = Equal(ref=1) / 所有 op=Keep /
 *      writeMask=0x00，片元输出绿色  只有 stencil==1 的左半屏通过。
 *
 * 期望：左半屏 = 绿（第 2 次绘制通过），右半屏 = 清屏黑（被模板挡住）。
 *
 * 判别力：
 *   - stencil 完全不生效（测试恒通过） 右半屏也会变绿  失败；
 *   - stencil 恒不通过  左半屏仍是黑  失败；
 *   - 模板附件没接线 / 没清 0  结果不稳定  失败。
 *
 * 顶点由 gl_VertexID / SV_VertexID 生成（不依赖顶点属性），与 bind_group / sprite_primitive
 * 两个 smoke 一致；仅绑一个占位 VBO 满足通用绘制路径的非空检查。
 * DX11 的 CreateShaderProgram 两个参数传同一份含 VSMain/PSMain 的 HLSL（与 bind_group 同构）。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/render/rhi/rhi_handle.h"
#include "engine/render/rhi/rhi_types.h"

#include <cstdint>
#include <string>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;
constexpr float kHalfPx = 128.0f;  // RT 宽的一半

// ---- OpenGL GLSL (#version 430) ----
const char* kGlVert = R"(#version 430
void main() {
    vec2 c = vec2[4](vec2(-1.0,-1.0), vec2(1.0,-1.0), vec2(1.0,1.0), vec2(-1.0,1.0))[gl_VertexID];
    gl_Position = vec4(c, 0.0, 1.0);
}
)";

const char* kGlFragWriteMask = R"(#version 430
out vec4 FragColor;
void main() {
    if (gl_FragCoord.x >= 128.0) discard;   // 只让左半屏写模板
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";

const char* kGlFragTest = R"(#version 430
out vec4 FragColor;
void main() { FragColor = vec4(0.0, 1.0, 0.0, 1.0); }
)";

// ---- Vulkan GLSL (#version 450) ----
const char* kVkVert = R"(#version 450
void main() {
    vec2 c = vec2[4](vec2(-1.0,-1.0), vec2(1.0,-1.0), vec2(1.0,1.0), vec2(-1.0,1.0))[gl_VertexIndex];
    gl_Position = vec4(c, 0.0, 1.0);
}
)";

const char* kVkFragWriteMask = R"(#version 450
layout(location = 0) out vec4 FragColor;
void main() {
    if (gl_FragCoord.x >= 128.0) discard;
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";

const char* kVkFragTest = R"(#version 450
layout(location = 0) out vec4 FragColor;
void main() { FragColor = vec4(0.0, 1.0, 0.0, 1.0); }
)";

// ---- DX11 HLSL：每份源里都含 VSMain + 一个 PSMain ----
const char* kDxSrcWriteMask = R"(
struct VSOut { float4 pos : SV_Position; };
VSOut VSMain(uint vid : SV_VertexID) {
    float2 corners[4] = { float2(-1.0,-1.0), float2(1.0,-1.0), float2(1.0,1.0), float2(-1.0,1.0) };
    VSOut o;
    o.pos = float4(corners[vid], 0.0, 1.0);
    return o;
}
float4 PSMain(VSOut i) : SV_Target {
    if (i.pos.x >= 128.0) discard;          // 只让左半屏写模板
    return float4(1.0, 0.0, 0.0, 1.0);
}
)";

const char* kDxSrcTest = R"(
struct VSOut { float4 pos : SV_Position; };
VSOut VSMain(uint vid : SV_VertexID) {
    float2 corners[4] = { float2(-1.0,-1.0), float2(1.0,-1.0), float2(1.0,1.0), float2(-1.0,1.0) };
    VSOut o;
    o.pos = float4(corners[vid], 0.0, 1.0);
    return o;
}
float4 PSMain(VSOut i) : SV_Target { return float4(0.0, 1.0, 0.0, 1.0); }
)";

// 写模板：Always 通过，pass 时 Replace 成 reference=1。
StencilState MakeWriteStencil() {
    StencilState st{};
    st.enabled = true;
    st.read_mask = 0xFFu;
    st.write_mask = 0xFFu;
    st.reference = 1u;
    st.front.compare = CompareFunc::Always;
    st.front.pass_op = StencilOp::Replace;
    st.back.compare = CompareFunc::Always;
    st.back.pass_op = StencilOp::Replace;
    return st;
}

// 测模板：仅 stencil == reference 通过，且不写模板。
StencilState MakeTestStencil() {
    StencilState st{};
    st.enabled = true;
    st.read_mask = 0xFFu;
    st.write_mask = 0x00u;
    st.reference = 1u;
    st.front.compare = CompareFunc::Equal;
    st.back.compare = CompareFunc::Equal;
    return st;
}

RenderTargetReadback RenderStencilMask(RhiDevice& device,
                                       const std::string& write_vert,
                                       const std::string& write_frag,
                                       const std::string& test_vert,
                                       const std::string& test_frag) {
    RenderTargetDesc rt_desc{};
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;
    rt_desc.has_stencil = true;   // ADR-2 第 2 步：RT 必须真的带模板面
    const auto rt = device.CreateRenderTarget(rt_desc);
    if (!rt) return {};

    const auto prog_write = device.CreateShaderProgram(write_vert, write_frag);
    const auto prog_test = device.CreateShaderProgram(test_vert, test_frag);
    if (!prog_write || !prog_test) {
        if (prog_write) device.DeleteShaderProgram(prog_write);
        if (prog_test) device.DeleteShaderProgram(prog_test);
        device.DeleteRenderTarget(rt);
        return {};
    }

    const float dummy[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    GpuBufferDesc vb_desc{};
    vb_desc.size = sizeof(dummy);
    vb_desc.usage = GpuBufferUsage::kVertex;
    BufferHandle vbo = device.CreateGpuBuffer(vb_desc, dummy);

    const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
    GpuBufferDesc ib_desc{};
    ib_desc.size = sizeof(indices);
    ib_desc.usage = GpuBufferUsage::kIndex;
    BufferHandle ibo = device.CreateGpuBuffer(ib_desc, indices);

    PipelineStateDesc pso_write_desc{};
    pso_write_desc.blend_enabled = false;
    pso_write_desc.depth_test_enabled = false;
    pso_write_desc.depth_write_enabled = false;
    pso_write_desc.culling_enabled = false;
    pso_write_desc.stencil = MakeWriteStencil();
    const auto pso_write = device.CreatePipelineState(pso_write_desc);

    PipelineStateDesc pso_test_desc = pso_write_desc;
    pso_test_desc.stencil = MakeTestStencil();
    const auto pso_test = device.CreatePipelineState(pso_test_desc);

    if (!vbo || !ibo || !pso_write || !pso_test) {
        if (vbo) device.DeleteGpuBuffer(vbo);
        if (ibo) device.DeleteGpuBuffer(ibo);
        device.DeleteShaderProgram(prog_write);
        device.DeleteShaderProgram(prog_test);
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
        cmd->BindVertexBuffer(0u, vbo, sizeof(float) * 2, {});
        cmd->BindIndexBuffer(ibo, IndexType::UInt16);
        cmd->BindPipeline(device.GetGraphicsPipeline(pso_write, prog_write));  // pass 1：写模板
        cmd->DrawIndexed(6, 0, 0);
        cmd->BindPipeline(device.GetGraphicsPipeline(pso_test, prog_test));    // pass 2：测模板
        cmd->DrawIndexed(6, 0, 0);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);

    device.DeleteGpuBuffer(vbo);
    device.DeleteGpuBuffer(ibo);
    device.DeleteShaderProgram(prog_write);
    device.DeleteShaderProgram(prog_test);
    device.DeleteRenderTarget(rt);
    return rb;
}

RenderTargetReadback RenderGL(RhiDevice& d) {
    return RenderStencilMask(d, kGlVert, kGlFragWriteMask, kGlVert, kGlFragTest);
}
RenderTargetReadback RenderVK(RhiDevice& d) {
    return RenderStencilMask(d, kVkVert, kVkFragWriteMask, kVkVert, kVkFragTest);
}
RenderTargetReadback RenderDX(RhiDevice& d) {
    return RenderStencilMask(d, kDxSrcWriteMask, kDxSrcWriteMask, kDxSrcTest, kDxSrcTest);
}

void VerifyStencilMask(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    // 左半屏：模板测试通过  第二次绘制（绿）可见
    const unsigned char* left = dse::test::PixelAt(rb, kRtSize / 4, kRtSize / 2);
    ASSERT_NE(left, nullptr) << backend;
    EXPECT_GT(left[1], 128) << backend << " left half should be GREEN (stencil test passed)";
    EXPECT_LT(left[0], 64) << backend << " left half R should be low";

    // 右半屏：模板 != 1  被挡住  保持清屏黑
    const unsigned char* right = dse::test::PixelAt(rb, kRtSize * 3 / 4, kRtSize / 2);
    ASSERT_NE(right, nullptr) << backend;
    EXPECT_LT(right[0], 24) << backend << " right half should be clear-black R (stencil masked)";
    EXPECT_LT(right[1], 24) << backend << " right half should be clear-black G (stencil masked)";
    EXPECT_LT(right[2], 24) << backend << " right half should be clear-black B (stencil masked)";
}

}  // namespace

TEST(StencilPixelSmokeTest, OpenGLStencilMaskHidesGeometry) {
    dse::test::RequestFreshDeviceForThisTest();
    auto r = dse::test::RunOpenGL(RenderGL);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyStencilMask(r.readback, "OpenGL");
}

TEST(StencilPixelSmokeTest, D3D11StencilMaskHidesGeometry) {
    dse::test::RequestFreshDeviceForThisTest();
    auto r = dse::test::RunD3D11(RenderDX);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyStencilMask(r.readback, "D3D11");
}

TEST(StencilPixelSmokeTest, VulkanStencilMaskHidesGeometry) {
    dse::test::RequestFreshDeviceForThisTest();
    auto r = dse::test::RunVulkan(RenderVK);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyStencilMask(r.readback, "Vulkan");
}
