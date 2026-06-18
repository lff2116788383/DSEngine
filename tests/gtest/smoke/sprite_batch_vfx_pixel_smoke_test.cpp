/**
 * @file sprite_batch_vfx_pixel_smoke_test.cpp
 * @brief B2a step3 跨后端像素 smoke：SpriteBatchRenderer 的 UI 特效（VFX）路径
 *        （sprite_fx.vert + sprite_fx_vfx.frag，参数走 SpriteFx push-block UBO\@slot0）
 *        在 GL/DX11/Vulkan 上渲染渐变 + 圆角两个特效 quad，断言①渐变 quad 符合
 *        解析真值（base = texColor*vColor*mix(start,end,u)）②圆角 quad 中心填充
 *        ③三后端互相一致（RMSE 阈内，覆盖圆角 SDF 分支）。
 *
 * 纹理统一用白回退（texture_handle==0），故 texColor=1，结果仅由顶点色与渐变决定，
 * 给出与采样器/后端无关的确定真值。场景关于 y=128 垂直对称（采样点均在 y=128），
 * 使 DX11 离屏回读的垂直翻转约定不影响跨后端比较。
 */

#include "rhi_pixel_harness.h"

#include <gtest/gtest.h>

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/sprite_batch_renderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;
constexpr float kCenterY = 128.0f;

// 两个 VFX quad：①水平红→蓝渐变（x 16..112）②白色圆角（x 144..240，corner_radius=16）。
std::vector<SpriteDrawItem> BuildItems() {
    std::vector<SpriteDrawItem> items;

    // ① 渐变 quad（无圆角/无模糊）。
    {
        SpriteDrawItem it;
        it.texture_handle = 0;  // 白纹理回退
        it.blend_mode = 0u;
        it.model = glm::translate(glm::mat4(1.0f), glm::vec3(64.0f, kCenterY, 0.0f)) *
                   glm::scale(glm::mat4(1.0f), glm::vec3(96.0f, 64.0f, 1.0f));
        it.color = glm::vec4(1.0f);
        it.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        SpriteVisualEffect& ve = it.visual_effect;
        ve.enabled = true;
        ve.gradient_direction = 0.0f;  // 水平：grad_t = u
        ve.gradient_start = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);  // red
        ve.gradient_end = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);    // blue
        items.push_back(it);
    }

    // ② 圆角 quad（渐变设为纯白以隔离圆角分支）。
    {
        SpriteDrawItem it;
        it.texture_handle = 0;
        it.blend_mode = 0u;
        it.model = glm::translate(glm::mat4(1.0f), glm::vec3(192.0f, kCenterY, 0.0f)) *
                   glm::scale(glm::mat4(1.0f), glm::vec3(96.0f, 64.0f, 1.0f));
        it.color = glm::vec4(1.0f);
        it.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        SpriteVisualEffect& ve = it.visual_effect;
        ve.enabled = true;
        ve.gradient_start = glm::vec4(1.0f);
        ve.gradient_end = glm::vec4(1.0f);
        ve.corner_radius = 16.0f;
        ve.rect_size = glm::vec2(96.0f, 64.0f);
        items.push_back(it);
    }
    return items;
}

RenderTargetReadback RenderVfx(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    const std::vector<SpriteDrawItem> items = BuildItems();
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
    device.DeleteRenderTarget(rt);
    return rb;
}

void ExpectColorNear(const RenderTargetReadback& rb, int x, int y,
                     int r, int g, int b, const char* tag) {
    const unsigned char* p = dse::test::PixelAt(rb, x, y);
    ASSERT_NE(p, nullptr) << tag << " (" << x << "," << y << ")";
    const int tol = 16;
    EXPECT_NEAR(static_cast<int>(p[0]), r, tol) << tag << " R @(" << x << "," << y << ")";
    EXPECT_NEAR(static_cast<int>(p[1]), g, tol) << tag << " G @(" << x << "," << y << ")";
    EXPECT_NEAR(static_cast<int>(p[2]), b, tol) << tag << " B @(" << x << "," << y << ")";
}

// 解析真值（采样点都在 y=128 对称轴上，对 DX11 垂直翻转鲁棒）。
void VerifyCorrectness(const RenderTargetReadback& rb, const char* tag) {
    ASSERT_EQ(rb.width, kRtSize) << tag;
    ASSERT_EQ(rb.height, kRtSize) << tag;
    // 渐变 quad x 16..112，u=(x-16)/96，base = mix(red, blue, u)。
    ExpectColorNear(rb, 40, 128, 191, 0, 64, tag);    // u=0.25 → (0.75,0,0.25)
    ExpectColorNear(rb, 88, 128, 64, 0, 191, tag);    // u=0.75 → (0.25,0,0.75)
    // 圆角 quad x 144..240：y=128 是垂直中线，圆角不在此切，整行填充白。
    ExpectColorNear(rb, 192, 128, 255, 255, 255, tag);  // 中心填充
    // 背景。
    ExpectColorNear(rb, 128, 128, 0, 0, 0, tag);      // 两 quad 之间空隙
}

void RunBackend(const char* backend,
                dse::test::BackendResult (*run)(const dse::test::RenderFn&)) {
    auto r = run(RenderVfx);
    if (!r.available) GTEST_SKIP() << backend << " unavailable: " << r.skip_reason;
    VerifyCorrectness(r.readback, backend);
}

}  // namespace

TEST(SpriteBatchVfxPixelSmokeTest, OpenGLRendersVfx) {
    RunBackend("OpenGL", &dse::test::RunOpenGL);
}
TEST(SpriteBatchVfxPixelSmokeTest, D3D11RendersVfx) {
    RunBackend("D3D11", &dse::test::RunD3D11);
}
TEST(SpriteBatchVfxPixelSmokeTest, VulkanRendersVfx) {
    RunBackend("Vulkan", &dse::test::RunVulkan);
}

// 跨后端一致性：场景关于 y=128 对称，故 DX11 垂直翻转不影响整帧比较（含圆角 SDF 切角）。
TEST(SpriteBatchVfxPixelSmokeTest, CrossBackendConsistent) {
    auto gl = dse::test::RunOpenGL(RenderVfx);
    auto dx = dse::test::RunD3D11(RenderVfx);
    auto vk = dse::test::RunVulkan(RenderVfx);
    int available = (gl.available ? 1 : 0) + (dx.available ? 1 : 0) + (vk.available ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "need >=2 backends for cross-backend RMSE";

    const double kRmseGate = 8.0;
    if (gl.available && vk.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, vk.readback);
        fprintf(stderr, "[SPRITE-VFX-B2a] GL-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs Vulkan diverged";
    }
    if (gl.available && dx.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, dx.readback);
        fprintf(stderr, "[SPRITE-VFX-B2a] GL-vs-D3D11 RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs D3D11 diverged";
    }
    if (dx.available && vk.available) {
        double rmse = dse::test::ComputeRmse(dx.readback, vk.readback);
        fprintf(stderr, "[SPRITE-VFX-B2a] D3D11-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "D3D11 vs Vulkan diverged";
    }
}
