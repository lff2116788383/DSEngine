/**
 * @file sprite_batch_sdf_pixel_smoke_test.cpp
 * @brief B2a step2 跨后端像素 smoke：SpriteBatchRenderer 的 SDF 文本路径
 *        （sprite_fx.vert + sprite_fx_sdf.frag，参数走 SpriteFx push-block UBO\@slot0）
 *        在 GL/DX11/Vulkan 上渲染同一批 SDF glyph，断言①每后端符合 SDF 解析真值
 *        （alpha>threshold 处填充 vColor、alpha<threshold 处 discard 露背景）
 *        ②三后端互相一致（RMSE 阈内）。
 *
 * SDF 纹理：左半 alpha=1、右半 alpha=0 的竖直边。threshold=0.5、smoothing 小时，
 * glyph 左侧填充、右侧丢弃——给出与采样器/后端无关的确定真值。场景关于 y=128
 * 垂直对称，使 DX11 离屏回读的垂直翻转约定不影响跨后端比较（与其它 smoke 同法）。
 */

#include "rhi_pixel_harness.h"

#include <gtest/gtest.h>

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/sprite_batch_renderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <functional>
#include <string>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;
constexpr float kCenterY = 128.0f;

const unsigned int kSdfVariantKey =
    static_cast<unsigned int>(std::hash<std::string>{}("TEXT_SDF"));

// 4x4 RGBA8：RGB 全白，左两列 alpha=255、右两列 alpha=0（竖直 SDF 边）。
unsigned int CreateSdfTexture(RhiDevice& device) {
    unsigned char px[4 * 4 * 4];
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            unsigned char* p = px + (y * 4 + x) * 4;
            p[0] = 255; p[1] = 255; p[2] = 255;
            p[3] = (x < 2) ? 255 : 0;
        }
    }
    return device.CreateTexture2D(4, 4, px, false);
}

// 单个居中 SDF glyph：x 64..192（u=(x-64)/128），y 96..160（关于 128 对称）。
// 顶点色橙 (1,0.5,0)；左半采到 alpha=1 → 填充橙，右半 alpha=0 → discard 露黑底。
std::vector<SpriteDrawItem> BuildItems(unsigned int sdf_tex) {
    std::vector<SpriteDrawItem> items;
    SpriteDrawItem it;
    it.texture_handle = sdf_tex;
    it.shader_variant_key = kSdfVariantKey;
    it.blend_mode = 0u;  // alpha
    it.model = glm::translate(glm::mat4(1.0f), glm::vec3(128.0f, kCenterY, 0.0f)) *
               glm::scale(glm::mat4(1.0f), glm::vec3(128.0f, 64.0f, 1.0f));
    it.color = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
    it.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    it.sdf_threshold = 0.5f;
    it.sdf_smoothing = 0.05f;
    it.sdf_outline_width = 0.0f;
    items.push_back(it);
    return items;
}

RenderTargetReadback RenderSdf(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    unsigned int sdf_tex = CreateSdfTexture(device);
    const std::vector<SpriteDrawItem> items = BuildItems(sdf_tex);
    const glm::mat4 view(1.0f);
    const glm::mat4 proj =
        glm::ortho(0.0f, static_cast<float>(kRtSize), 0.0f, static_cast<float>(kRtSize), -1.0f, 1.0f);

    SpriteBatchRenderer renderer;
    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        renderer.Draw(*cmd, device, items, view, proj);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteTexture(sdf_tex);
    device.DeleteRenderTarget(rt);
    return rb;
}

void ExpectColorNear(const RenderTargetReadback& rb, int x, int y,
                     int r, int g, int b, const char* tag) {
    const unsigned char* p = dse::test::PixelAt(rb, x, y);
    ASSERT_NE(p, nullptr) << tag << " (" << x << "," << y << ")";
    const int tol = 16;  // 软件光栅器近似 + SDF smoothstep 容差
    EXPECT_NEAR(static_cast<int>(p[0]), r, tol) << tag << " R @(" << x << "," << y << ")";
    EXPECT_NEAR(static_cast<int>(p[1]), g, tol) << tag << " G @(" << x << "," << y << ")";
    EXPECT_NEAR(static_cast<int>(p[2]), b, tol) << tag << " B @(" << x << "," << y << ")";
}

// 解析真值（采样点都在 y=128 对称轴上，对 DX11 垂直翻转鲁棒）。
void VerifyCorrectness(const RenderTargetReadback& rb, const char* tag) {
    ASSERT_EQ(rb.width, kRtSize) << tag;
    ASSERT_EQ(rb.height, kRtSize) << tag;
    ExpectColorNear(rb, 90, 128, 255, 128, 0, tag);   // u≈0.20: alpha=1 → 填充橙
    ExpectColorNear(rb, 166, 128, 0, 0, 0, tag);      // u≈0.80: alpha=0 → discard 露黑底
    ExpectColorNear(rb, 10, 128, 0, 0, 0, tag);       // glyph 外：黑底
}

void RunBackend(const char* backend,
                dse::test::BackendResult (*run)(const dse::test::RenderFn&)) {
    auto r = run(RenderSdf);
    if (!r.available) GTEST_SKIP() << backend << " unavailable: " << r.skip_reason;
    VerifyCorrectness(r.readback, backend);
}

}  // namespace

TEST(SpriteBatchSdfPixelSmokeTest, OpenGLRendersSdf) {
    RunBackend("OpenGL", &dse::test::RunOpenGL);
}
TEST(SpriteBatchSdfPixelSmokeTest, D3D11RendersSdf) {
    RunBackend("D3D11", &dse::test::RunD3D11);
}
TEST(SpriteBatchSdfPixelSmokeTest, VulkanRendersSdf) {
    RunBackend("Vulkan", &dse::test::RunVulkan);
}

// 跨后端一致性：场景关于 y=128 对称，故 DX11 垂直翻转不影响整帧比较。
TEST(SpriteBatchSdfPixelSmokeTest, CrossBackendConsistent) {
    auto gl = dse::test::RunOpenGL(RenderSdf);
    auto dx = dse::test::RunD3D11(RenderSdf);
    auto vk = dse::test::RunVulkan(RenderSdf);
    int available = (gl.available ? 1 : 0) + (dx.available ? 1 : 0) + (vk.available ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "need >=2 backends for cross-backend RMSE";

    const double kRmseGate = 8.0;
    if (gl.available && vk.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, vk.readback);
        fprintf(stderr, "[SPRITE-SDF-B2a] GL-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs Vulkan diverged";
    }
    if (gl.available && dx.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, dx.readback);
        fprintf(stderr, "[SPRITE-SDF-B2a] GL-vs-D3D11 RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs D3D11 diverged";
    }
    if (dx.available && vk.available) {
        double rmse = dse::test::ComputeRmse(dx.readback, vk.readback);
        fprintf(stderr, "[SPRITE-SDF-B2a] D3D11-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "D3D11 vs Vulkan diverged";
    }
}
