/**
 * @file sprite_primitive_smoke_test.cpp
 * @brief B0 通用绘制原语「活体」像素冒烟 — 用 SpriteRenderer 在真实 GPU 上画带纹理 quad，
 *        离屏回读像素校验。覆盖新原语全栈：BindShaderProgram / BindUniformBuffer /
 *        BindTexture(2D) / BindVertexBuffer / BindIndexBuffer / DrawIndexed。
 *
 * 三后端 GPU 上下文样板已抽到 rhi_pixel_harness（B1）；此处只描述「画什么 + 校验什么」。
 * 这是 RHI_PRIMITIVE_CONTRACT.md §7 要求的「每个原语随活体消费者 + 像素测试落地」闸门。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/sprite_renderer.h"

#include <glm/glm.hpp>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 在已初始化的 device 上：建 256² RT → 清黑 → 用 SpriteRenderer 画居中红色纹理 quad
// （裁剪空间半边长 0.5，覆盖屏幕中央一半）→ 回读像素。
RenderTargetReadback RenderCenteredSpriteQuad(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    // 2×2 纯红纹理（采样到 quad，验证纹理路径而非仅顶点色）
    const unsigned char red[] = {
        255, 0, 0, 255,  255, 0, 0, 255,
        255, 0, 0, 255,  255, 0, 0, 255,
    };
    unsigned int tex = device.CreateTexture2D(2, 2, red, false);

    // SpriteRenderer 必须存活到帧提交完成后才能 Shutdown：其 VBO/IBO/UBO 被命令缓冲引用，
    // 帧内删除会使 Vulkan 命令缓冲失效（GL/DX11 容忍，Vulkan 严格）。
    SpriteRenderer sprite;
    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);

        sprite.Draw(*cmd, device, tex, glm::mat4(1.0f), 0.5f, glm::vec4(1.0f));

        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();
    sprite.Shutdown(device);  // 帧已提交 + fence 等待完成，删除安全

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    device.DeleteTexture(tex);
    device.DeleteRenderTarget(rt);
    return rb;
}

// 断言：中央像素为红（纹理被采样到 quad），四角为黑（quad 外为清屏色）。
void VerifyCenteredRedQuad(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const unsigned char* center = dse::test::PixelAt(rb, kRtSize / 2, kRtSize / 2);
    ASSERT_NE(center, nullptr) << backend;
    EXPECT_GT(center[0], 128) << backend << " center R should be high (red quad)";
    EXPECT_LT(center[1], 64) << backend << " center G should be low";
    EXPECT_LT(center[2], 64) << backend << " center B should be low";

    // 四角应是清屏黑（quad 仅覆盖中央一半，角落在 quad 外）
    const int margin = 8;
    const unsigned char* corner = dse::test::PixelAt(rb, margin, margin);
    ASSERT_NE(corner, nullptr) << backend;
    EXPECT_LT(corner[0], 24) << backend << " corner should be clear-black R";
    EXPECT_LT(corner[1], 24) << backend << " corner should be clear-black G";
    EXPECT_LT(corner[2], 24) << backend << " corner should be clear-black B";
}

}  // namespace

// ============================================================
// 三后端：上下文样板走 harness，本文件只给「画什么 + 校验什么」。
// ============================================================

TEST(SpritePrimitiveSmokeTest, OpenGLDrawsCenteredTexturedQuad) {
    auto r = dse::test::RunOpenGL(RenderCenteredSpriteQuad);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyCenteredRedQuad(r.readback, "OpenGL");
}

TEST(SpritePrimitiveSmokeTest, D3D11DrawsCenteredTexturedQuad) {
    auto r = dse::test::RunD3D11(RenderCenteredSpriteQuad);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyCenteredRedQuad(r.readback, "D3D11");
}

TEST(SpritePrimitiveSmokeTest, VulkanDrawsCenteredTexturedQuad) {
    auto r = dse::test::RunVulkan(RenderCenteredSpriteQuad);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyCenteredRedQuad(r.readback, "Vulkan");
}
