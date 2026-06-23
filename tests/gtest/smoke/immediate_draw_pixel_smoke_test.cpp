/**
 * @file immediate_draw_pixel_smoke_test.cpp
 * @brief P1「活体」离屏像素冒烟 — 验证 RHI 新原语 ImmediateDraw + BlitRenderTarget
 *        （编辑器架构 §5.A / §5.B）。用「自定义 shader program + 一段顶点数据 + 少量 uniform」
 *        直接绘制到离屏 RT，再 ReadRenderTargetColorRgba8WithSize 回读校验，覆盖编辑器视口拾取
 *        / 多视口 blit 的核心数值路径，且不经高层 Mesh/Sprite 批。
 *
 * 四个场景（与 EDITOR_ARCHITECTURE.md §5.A/§5.B「需补测试」一一对应）：
 *  - ImmediateDrawFillsRenderTarget：纯色 shader 画全屏三角形覆盖整 RT，中心像素=期望色。
 *  - ImmediateDrawViewportSubregion：带 viewport 子区域绘制，区域内=绘制色 / 区域外=清屏色。
 *  - ImmediateDrawColorIdRoundTrip：模拟拾取——画多条不同「颜色 ID」竖条，回读后颜色→ID 反解正确。
 *  - BlitRenderTargetCopiesColor：§5.A 画纯色填 src → BlitRenderTarget → 读 dst 颜色一致。
 *
 * 本机 GPU 现实：仅 D3D11(WARP) 能跑真实像素；OpenGL/Vulkan 无驱动时由 harness 优雅 skip
 * （与既有 *_pixel_smoke_test 同构）。竖条沿 x 分布、子区域中心对称，DX11 垂直翻转不影响断言。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 全屏三角形（超出 NDC，裁剪后铺满整屏）。单属性 location0 = vec2 位置。
const float kFullscreenTri[6] = {
    -1.0f, -1.0f,
     3.0f, -1.0f,
    -1.0f,  3.0f,
};

// ---- 纯色 shader：location0=aPos(vec2)，uniform vec4 uColor ----
const char* kGlVert = R"(#version 430
layout(location = 0) in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
)";
const char* kGlFrag = R"(#version 430
out vec4 FragColor;
uniform vec4 uColor;
void main() { FragColor = uColor; }
)";

const char* kVkVert = R"(#version 450
layout(location = 0) in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
)";
const char* kVkFrag = R"(#version 450
layout(location = 0) out vec4 FragColor;
layout(push_constant) uniform PC { vec4 uColor; } pc;
void main() { FragColor = pc.uColor; }
)";

// DX11：VS 输入语义 TEXCOORD<location>（与 ImmediateDraw 的 InputLayout 约定一致），uniform → cbuffer 成员名。
const char* kDxSrc = R"(
cbuffer Params : register(b0) { float4 uColor; };
struct VSIn  { float2 aPos : TEXCOORD0; };
struct VSOut { float4 pos : SV_Position; };
VSOut VSMain(VSIn i) { VSOut o; o.pos = float4(i.aPos, 0.0, 1.0); return o; }
float4 PSMain(VSOut i) : SV_Target { return uColor; }
)";

enum class Api { GL, VK, DX };

unsigned int MakeColorProgram(RhiDevice& d, Api api) {
    switch (api) {
        case Api::GL: return d.CreateShaderProgram(kGlVert, kGlFrag);
        case Api::VK: return d.CreateShaderProgram(kVkVert, kVkFrag);
        case Api::DX: return d.CreateShaderProgram(kDxSrc, kDxSrc);
    }
    return 0u;
}

// 填好一个「画全屏三角形」的 ImmediateDrawDesc（位置属性 + uColor uniform）。
ImmediateDrawDesc MakeFullscreenDesc(unsigned int rt, unsigned int program, const glm::vec4& color) {
    ImmediateDrawDesc desc;
    desc.render_target = rt;
    desc.shader_program = program;
    desc.vertices = kFullscreenTri;
    desc.vertex_bytes = sizeof(kFullscreenTri);
    desc.vertex_count = 3;
    desc.stride_bytes = sizeof(float) * 2;
    desc.attribs.push_back({0, 2, 0});  // location0, 2 分量, 偏移 0
    desc.topology = ImmediateTopology::Triangles;
    desc.uniforms_vec4.push_back({"uColor", color});
    return desc;
}

unsigned int MakeColorRt(RhiDevice& d) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    return d.CreateRenderTarget(rt_desc);
}

// ===== 场景 1：全屏填充 =====
// 清屏红 → ImmediateDraw 全屏三角形画绿 → 整 RT 应为绿（绘制覆盖了清屏色）。
RenderTargetReadback RenderFill(RhiDevice& d, Api api) {
    unsigned int rt = MakeColorRt(d);
    unsigned int program = MakeColorProgram(d, api);
    if (rt == 0 || program == 0) { if (rt) d.DeleteRenderTarget(rt); return {}; }

    ImmediateDrawDesc desc = MakeFullscreenDesc(rt, program, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    desc.clear = true;
    desc.clear_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    d.ImmediateDraw(desc);

    RenderTargetReadback rb = d.ReadRenderTargetColorRgba8WithSize(rt);
    d.DeleteShaderProgram(program);
    d.DeleteRenderTarget(rt);
    return rb;
}

void VerifyFill(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    for (int p : {kRtSize / 2, 8, kRtSize - 8}) {
        const unsigned char* px = dse::test::PixelAt(rb, p, kRtSize / 2);
        ASSERT_NE(px, nullptr) << backend;
        EXPECT_LT(px[0], 32)  << backend << " R@" << p;
        EXPECT_GT(px[1], 220) << backend << " G@" << p << " (drawn green, not clear red)";
        EXPECT_LT(px[2], 32)  << backend << " B@" << p;
    }
}

// ===== 场景 2：viewport 子区域 =====
// 清屏红覆盖整 RT（clear 不受 viewport 限制）→ viewport 限定中心区域画绿 →
// 区域内中心=绿，区域外四角=红。
RenderTargetReadback RenderViewport(RhiDevice& d, Api api) {
    unsigned int rt = MakeColorRt(d);
    unsigned int program = MakeColorProgram(d, api);
    if (rt == 0 || program == 0) { if (rt) d.DeleteRenderTarget(rt); return {}; }

    ImmediateDrawDesc desc = MakeFullscreenDesc(rt, program, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    desc.clear = true;
    desc.clear_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    desc.viewport = glm::ivec4(64, 64, 128, 128);  // 居中 128² 子区域（上下左右对称）
    d.ImmediateDraw(desc);

    RenderTargetReadback rb = d.ReadRenderTargetColorRgba8WithSize(rt);
    d.DeleteShaderProgram(program);
    d.DeleteRenderTarget(rt);
    return rb;
}

void VerifyViewport(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;

    const unsigned char* center = dse::test::PixelAt(rb, kRtSize / 2, kRtSize / 2);
    ASSERT_NE(center, nullptr) << backend;
    EXPECT_LT(center[0], 32)  << backend << " center R";
    EXPECT_GT(center[1], 220) << backend << " center G (inside viewport = drawn green)";

    // 四角都在子区域外 → 保持清屏红（对称，DX11 翻转无影响）。
    for (auto [x, y] : {std::pair<int,int>{8, 8}, {kRtSize - 8, 8},
                        {8, kRtSize - 8}, {kRtSize - 8, kRtSize - 8}}) {
        const unsigned char* px = dse::test::PixelAt(rb, x, y);
        ASSERT_NE(px, nullptr) << backend;
        EXPECT_GT(px[0], 220) << backend << " corner R (outside viewport = clear red)";
        EXPECT_LT(px[1], 32)  << backend << " corner G";
    }
}

// ===== 场景 3：颜色 ID 往返（拾取核心） =====
// 把实体 ID 编码进 RGB（r=id&0xFF, g=(id>>8)&0xFF, b=(id>>16)&0xFF），画 3 条竖条 →
// 回读各竖条中心像素 → 反解 ID 与原值一致。竖条沿 x 分布，DX11 垂直翻转不改变所在竖条。
const uint32_t kIds[3] = {12u, 40000u, 66051u};  // 覆盖 r / g / b 三通道

glm::vec4 EncodeId(uint32_t id) {
    return glm::vec4(static_cast<float>(id & 0xFFu) / 255.0f,
                     static_cast<float>((id >> 8) & 0xFFu) / 255.0f,
                     static_cast<float>((id >> 16) & 0xFFu) / 255.0f,
                     1.0f);
}
uint32_t DecodeId(const unsigned char* px) {
    return static_cast<uint32_t>(px[0]) |
           (static_cast<uint32_t>(px[1]) << 8) |
           (static_cast<uint32_t>(px[2]) << 16);
}

RenderTargetReadback RenderColorId(RhiDevice& d, Api api) {
    unsigned int rt = MakeColorRt(d);
    unsigned int program = MakeColorProgram(d, api);
    if (rt == 0 || program == 0) { if (rt) d.DeleteRenderTarget(rt); return {}; }

    const int stripe_w = kRtSize / 3;
    for (int i = 0; i < 3; ++i) {
        ImmediateDrawDesc desc = MakeFullscreenDesc(rt, program, EncodeId(kIds[i]));
        desc.clear = (i == 0);  // 仅首条清屏（背景 = ID 0）
        desc.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        int x = i * stripe_w;
        int w = (i == 2) ? (kRtSize - x) : stripe_w;
        desc.viewport = glm::ivec4(x, 0, w, kRtSize);  // 整列竖条
        d.ImmediateDraw(desc);
    }

    RenderTargetReadback rb = d.ReadRenderTargetColorRgba8WithSize(rt);
    d.DeleteShaderProgram(program);
    d.DeleteRenderTarget(rt);
    return rb;
}

void VerifyColorId(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    const int stripe_w = kRtSize / 3;
    for (int i = 0; i < 3; ++i) {
        int cx = i * stripe_w + stripe_w / 2;
        const unsigned char* px = dse::test::PixelAt(rb, cx, kRtSize / 2);
        ASSERT_NE(px, nullptr) << backend << " stripe " << i;
        EXPECT_EQ(DecodeId(px), kIds[i])
            << backend << " stripe " << i << " color->ID round-trip"
            << " (rgb=" << int(px[0]) << "," << int(px[1]) << "," << int(px[2]) << ")";
    }
}

// ===== 场景 4：RT blit =====
// §5.A 纯色填 src → BlitRenderTarget(src,dst) → 读 dst，颜色与 src 一致。
RenderTargetReadback RenderBlit(RhiDevice& d, Api api) {
    unsigned int src = MakeColorRt(d);
    unsigned int dst = MakeColorRt(d);
    unsigned int program = MakeColorProgram(d, api);
    if (src == 0 || dst == 0 || program == 0) {
        if (src) d.DeleteRenderTarget(src);
        if (dst) d.DeleteRenderTarget(dst);
        if (program) d.DeleteShaderProgram(program);
        return {};
    }

    ImmediateDrawDesc desc = MakeFullscreenDesc(src, program, glm::vec4(0.0f, 0.5f, 1.0f, 1.0f));
    desc.clear = true;
    desc.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    d.ImmediateDraw(desc);

    d.BlitRenderTarget(src, dst);

    RenderTargetReadback rb = d.ReadRenderTargetColorRgba8WithSize(dst);
    d.DeleteShaderProgram(program);
    d.DeleteRenderTarget(src);
    d.DeleteRenderTarget(dst);
    return rb;
}

void VerifyBlit(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    const unsigned char* px = dse::test::PixelAt(rb, kRtSize / 2, kRtSize / 2);
    ASSERT_NE(px, nullptr) << backend;
    EXPECT_LT(px[0], 32)  << backend << " dst R (src=(0,0.5,1))";
    EXPECT_GT(px[1], 96)  << backend << " dst G";
    EXPECT_LT(px[1], 160) << backend << " dst G";
    EXPECT_GT(px[2], 220) << backend << " dst B";
}

}  // namespace

// ============================================================
// §5.A ImmediateDraw — 全屏填充
// ============================================================
TEST(ImmediateDrawPixelSmokeTest, OpenGLFillsRenderTarget) {
    auto r = dse::test::RunOpenGL([](RhiDevice& d) { return RenderFill(d, Api::GL); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyFill(r.readback, "OpenGL");
}
TEST(ImmediateDrawPixelSmokeTest, D3D11FillsRenderTarget) {
    auto r = dse::test::RunD3D11([](RhiDevice& d) { return RenderFill(d, Api::DX); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyFill(r.readback, "D3D11");
}
TEST(ImmediateDrawPixelSmokeTest, VulkanFillsRenderTarget) {
    auto r = dse::test::RunVulkan([](RhiDevice& d) { return RenderFill(d, Api::VK); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyFill(r.readback, "Vulkan");
}

// ============================================================
// §5.A ImmediateDraw — viewport 子区域
// ============================================================
TEST(ImmediateDrawPixelSmokeTest, OpenGLViewportSubregion) {
    auto r = dse::test::RunOpenGL([](RhiDevice& d) { return RenderViewport(d, Api::GL); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyViewport(r.readback, "OpenGL");
}
TEST(ImmediateDrawPixelSmokeTest, D3D11ViewportSubregion) {
    auto r = dse::test::RunD3D11([](RhiDevice& d) { return RenderViewport(d, Api::DX); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyViewport(r.readback, "D3D11");
}
TEST(ImmediateDrawPixelSmokeTest, VulkanViewportSubregion) {
    auto r = dse::test::RunVulkan([](RhiDevice& d) { return RenderViewport(d, Api::VK); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyViewport(r.readback, "Vulkan");
}

// ============================================================
// §5.A ImmediateDraw — 颜色 ID 往返（视口拾取核心数值）
// ============================================================
TEST(ImmediateDrawPixelSmokeTest, OpenGLColorIdRoundTrip) {
    auto r = dse::test::RunOpenGL([](RhiDevice& d) { return RenderColorId(d, Api::GL); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyColorId(r.readback, "OpenGL");
}
TEST(ImmediateDrawPixelSmokeTest, D3D11ColorIdRoundTrip) {
    auto r = dse::test::RunD3D11([](RhiDevice& d) { return RenderColorId(d, Api::DX); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyColorId(r.readback, "D3D11");
}
TEST(ImmediateDrawPixelSmokeTest, VulkanColorIdRoundTrip) {
    auto r = dse::test::RunVulkan([](RhiDevice& d) { return RenderColorId(d, Api::VK); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyColorId(r.readback, "Vulkan");
}

// ============================================================
// §5.B BlitRenderTarget — 等尺寸颜色拷贝
// ============================================================
TEST(BlitRenderTargetPixelSmokeTest, OpenGLCopiesColor) {
    auto r = dse::test::RunOpenGL([](RhiDevice& d) { return RenderBlit(d, Api::GL); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyBlit(r.readback, "OpenGL");
}
TEST(BlitRenderTargetPixelSmokeTest, D3D11CopiesColor) {
    auto r = dse::test::RunD3D11([](RhiDevice& d) { return RenderBlit(d, Api::DX); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyBlit(r.readback, "D3D11");
}
TEST(BlitRenderTargetPixelSmokeTest, VulkanCopiesColor) {
    auto r = dse::test::RunVulkan([](RhiDevice& d) { return RenderBlit(d, Api::VK); });
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyBlit(r.readback, "Vulkan");
}
