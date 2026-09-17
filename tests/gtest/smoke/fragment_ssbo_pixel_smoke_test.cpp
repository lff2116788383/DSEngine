/**
 * @file fragment_ssbo_pixel_smoke_test.cpp
 * @brief Stage-aware SSBO smoke: a fragment shader reads a storage buffer bound
 *        through CommandBuffer::BindStorageBuffer(ShaderStage::Fragment, ...).
 *
 * D3D11 historically bound generic primitive SSBOs only to VS(t).  This test
 * is the live-consumer gate for the stage-aware RHI contract: the same SSBO is
 * never referenced by the vertex shader, and the pixel readback only succeeds
 * when the fragment bind reaches PS(t0).
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

constexpr int kRtSize = 128;
constexpr float kR = 0.82f;
constexpr float kG = 0.27f;
constexpr float kB = 0.11f;

const char* kGlVert = R"(#version 430
void main() {
    vec2 corners[4] = vec2[4](vec2(-1.0,-1.0), vec2(1.0,-1.0), vec2(1.0,1.0), vec2(-1.0,1.0));
    gl_Position = vec4(corners[gl_VertexID], 0.0, 1.0);
}
)";

const char* kGlFrag = R"(#version 430
layout(std430, binding = 0) readonly buffer ColorSSBO { vec4 color; };
out vec4 FragColor;
void main() { FragColor = color; }
)";

const char* kVkVert = R"(#version 450
void main() {
    vec2 corners[4] = vec2[4](vec2(-1.0,-1.0), vec2(1.0,-1.0), vec2(1.0,1.0), vec2(-1.0,1.0));
    gl_Position = vec4(corners[gl_VertexIndex], 0.0, 1.0);
}
)";

const char* kVkFrag = R"(#version 450
layout(std430, set = 0, binding = 0) readonly buffer ColorSSBO { vec4 color; };
layout(location = 0) out vec4 FragColor;
void main() { FragColor = color; }
)";

const char* kDx = R"(
struct VSOut { float4 pos : SV_Position; };
VSOut VSMain(uint vid : SV_VertexID) {
    float2 corners[4] = { float2(-1.0,-1.0), float2(1.0,-1.0), float2(1.0,1.0), float2(-1.0,1.0) };
    VSOut o;
    o.pos = float4(corners[vid], 0.0, 1.0);
    return o;
}
ByteAddressBuffer colors : register(t0);
float4 PSMain(VSOut i) : SV_Target {
    return asfloat(colors.Load4(0));
}
)";

RenderTargetReadback RenderFragmentSSBO(RhiDevice& device,
                                        const std::string& vert_src,
                                        const std::string& frag_src) {
    RenderTargetDesc rt_desc{};
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    const auto rt = device.CreateRenderTarget(rt_desc);
    if (!rt) return {};

    const auto program = device.CreateShaderProgram(vert_src, frag_src);
    if (!program) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    const float color[4] = {kR, kG, kB, 1.0f};
    GpuBufferDesc ssbo_desc;
    ssbo_desc.size = sizeof(color);
    ssbo_desc.usage = GpuBufferUsage::kStorage;
    BufferHandle ssbo = device.CreateGpuBuffer(ssbo_desc, color);

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

    PipelineStateDesc pso_desc{};
    pso_desc.blend_enabled = false;
    pso_desc.depth_test_enabled = false;
    pso_desc.depth_write_enabled = false;
    pso_desc.culling_enabled = false;
    auto pso = device.CreatePipelineState(pso_desc);

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
        cmd->BindPipeline(device.GetGraphicsPipeline(pso, program));
        cmd->BindVertexBuffer(0u, vbo, sizeof(float) * 2, {});
        cmd->BindIndexBuffer(ibo, IndexType::UInt16);
        cmd->BindStorageBuffer(ShaderStage::Fragment, 0, ssbo, 0, 0);
        cmd->DrawIndexed(6, 0, 0);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    device.DeleteGpuBuffer(ssbo);
    device.DeleteGpuBuffer(vbo);
    device.DeleteGpuBuffer(ibo);
    device.DeleteShaderProgram(program);
    device.DeleteRenderTarget(rt);
    return rb;
}

void VerifyColor(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;
    const unsigned char* px = dse::test::PixelAt(rb, kRtSize / 2, kRtSize / 2);
    ASSERT_NE(px, nullptr) << backend;
    EXPECT_GT(px[0], 160) << backend << " R";
    EXPECT_GT(px[1], 35) << backend << " G";
    EXPECT_LT(px[1], 110) << backend << " G";
    EXPECT_GT(px[2], 5) << backend << " B";
    EXPECT_LT(px[2], 70) << backend << " B";
    EXPECT_GT(px[3], 240) << backend << " A";
}

}  // namespace

TEST(FragmentSSBOPixelSmokeTest, OpenGLFragmentReadsSSBO) {
    auto r = dse::test::RunOpenGL([](RhiDevice& d) {
        return RenderFragmentSSBO(d, kGlVert, kGlFrag);
    });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyColor(r.readback, "OpenGL");
}

TEST(FragmentSSBOPixelSmokeTest, D3D11FragmentReadsSSBO) {
    auto r = dse::test::RunD3D11([](RhiDevice& d) {
        return RenderFragmentSSBO(d, kDx, kDx);
    });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyColor(r.readback, "D3D11");
}

TEST(FragmentSSBOPixelSmokeTest, VulkanFragmentReadsSSBO) {
    auto r = dse::test::RunVulkan([](RhiDevice& d) {
        return RenderFragmentSSBO(d, kVkVert, kVkFrag);
    });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyColor(r.readback, "Vulkan");
}
