/**
 * @file sprite_batch_pixel_smoke_test.cpp
 * @brief B2a 跨后端像素 smoke：SpriteBatchRenderer 默认路径在 GL/DX11/Vulkan 上
 *        渲染同一批 sprite，断言①每后端混合结果符合解析真值（纹理*顶点色 +
 *        alpha/additive 混合）②三后端互相一致（RMSE 阈内）。
 *
 * 校验解析真值而非对照旧 ABI DrawSpriteBatch：sprite2d 着色器（FragColor =
 * texture(u_texture, uv) * vColor）是 2D 批的规范实现，其输出即为 sprite 应有像素；
 * 旧 PBR 批路径在隔离 harness 中缺整套材质/UBO 设置无法忠实运行（DX11 全黑、GL alpha
 * 偏差），故解析真值是更严格、与后端无关的闸门。已实测 Vulkan 上旧 DrawSpriteBatch
 * 与本渲染器逐字节一致（sprite2d 是 PBR 2D 路径的 drop-in），佐证迁移保真。
 *
 * 场景关于水平中线 y=128 对称，使 DX11 离屏回读的垂直翻转约定不影响跨后端比较
 * （与 skybox smoke 同法）。
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

// 关于 y=128 垂直对称：red 不透明 + green 半透明（重叠测 alpha 混合，同态合批）+
// blue additive（单独批）。
std::vector<SpriteDrawItem> BuildItems() {
    std::vector<SpriteDrawItem> items;
    auto quad = [&](float cx, float w, float h, glm::vec4 col, unsigned int blend) {
        SpriteDrawItem it;
        it.model = glm::translate(glm::mat4(1.0f), glm::vec3(cx, kCenterY, 0.0f)) *
                   glm::scale(glm::mat4(1.0f), glm::vec3(w, h, 1.0f));
        it.color = col;
        it.blend_mode = blend;
        it.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        items.push_back(it);
    };
    quad(64.0f, 64.0f, 64.0f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 0u);   // red, x:32..96
    quad(96.0f, 80.0f, 80.0f, glm::vec4(0.0f, 1.0f, 0.0f, 0.5f), 0u);   // green α0.5, x:56..136
    quad(192.0f, 50.0f, 50.0f, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), 1u);  // blue additive, x:167..217
    return items;
}

RenderTargetReadback RenderNew(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;  // 2D overlay：不测/不写深度
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
    const int tol = 12;  // 软件光栅器近似容差
    EXPECT_NEAR(static_cast<int>(p[0]), r, tol) << tag << " R @(" << x << "," << y << ")";
    EXPECT_NEAR(static_cast<int>(p[1]), g, tol) << tag << " G @(" << x << "," << y << ")";
    EXPECT_NEAR(static_cast<int>(p[2]), b, tol) << tag << " B @(" << x << "," << y << ")";
}

// 解析真值（采样点都在 y=128 对称轴上，对 DX11 垂直翻转鲁棒）。
void VerifyCorrectness(const RenderTargetReadback& rb, const char* tag) {
    ASSERT_EQ(rb.width, kRtSize) << tag;
    ASSERT_EQ(rb.height, kRtSize) << tag;
    ExpectColorNear(rb, 40, 128, 255, 0, 0, tag);    // red only
    ExpectColorNear(rb, 64, 128, 128, 128, 0, tag);  // red + green(α0.5) 混合
    ExpectColorNear(rb, 120, 128, 0, 128, 0, tag);   // green(α0.5) over black
    ExpectColorNear(rb, 192, 128, 0, 0, 255, tag);   // blue additive over black
    ExpectColorNear(rb, 10, 128, 0, 0, 0, tag);      // background
}

void RunBackend(const char* backend,
                dse::test::BackendResult (*run)(const dse::test::RenderFn&)) {
    auto r = run(RenderNew);
    if (!r.available) GTEST_SKIP() << backend << " unavailable: " << r.skip_reason;
    VerifyCorrectness(r.readback, backend);
}

}  // namespace

TEST(SpriteBatchPixelSmokeTest, OpenGLRendersBatch) {
    RunBackend("OpenGL", &dse::test::RunOpenGL);
}
TEST(SpriteBatchPixelSmokeTest, D3D11RendersBatch) {
    RunBackend("D3D11", &dse::test::RunD3D11);
}
TEST(SpriteBatchPixelSmokeTest, VulkanRendersBatch) {
    RunBackend("Vulkan", &dse::test::RunVulkan);
}

// 跨后端一致性：场景关于 y=128 对称，故 DX11 垂直翻转不影响整帧比较。
TEST(SpriteBatchPixelSmokeTest, CrossBackendConsistent) {
    auto gl = dse::test::RunOpenGL(RenderNew);
    auto dx = dse::test::RunD3D11(RenderNew);
    auto vk = dse::test::RunVulkan(RenderNew);
    int available = (gl.available ? 1 : 0) + (dx.available ? 1 : 0) + (vk.available ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "need >=2 backends for cross-backend RMSE";

    const double kRmseGate = 8.0;  // 软件光栅器友好（边缘 AA/翻转 off-by-one）
    if (gl.available && vk.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, vk.readback);
        fprintf(stderr, "[SPRITE-B2a] GL-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs Vulkan diverged";
    }
    if (gl.available && dx.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, dx.readback);
        fprintf(stderr, "[SPRITE-B2a] GL-vs-D3D11 RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "GL vs D3D11 diverged";
    }
    if (dx.available && vk.available) {
        double rmse = dse::test::ComputeRmse(dx.readback, vk.readback);
        fprintf(stderr, "[SPRITE-B2a] D3D11-vs-Vulkan RMSE = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseGate) << "D3D11 vs Vulkan diverged";
    }
}
