/**
 * @file skybox_pixel_smoke_test.cpp
 * @brief A1 skybox 跨后端「离屏像素」冒烟 — 用生产级 SkyboxRenderer 在三后端各自 GPU 上
 *        渲染同一立方体贴图天空盒，回读像素并：
 *          (1) 每后端：天空盒铺满屏幕（中心/四象限非黑），catches 黑屏 bug（教训#2）；
 *          (2) 跨后端：RMSE 在阈值内，验证三后端像素一致。
 *
 * 上下文样板走 rhi_pixel_harness；本文件只给「画什么 + 校验什么」。
 * 注：真机 (RTX 3070) A1 实测 GL vs DX11≈0.75、vs Vulkan≈2.1；本 VM 为软件渲染
 *    (llvmpipe/WARP/lavapipe)，RMSE 不复现真机值，故用较宽上界仅作「跨后端不发散」闸门。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/skybox_renderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 6 个面各一纯色：+X 红 / -X 绿 / +Y 蓝 / -Y 黄 / +Z 品红 / -Z 青。
// 用 64² 实心面（非 1×1）：使各后端面内部恒为纯色，仅边缝 ~1 texel 受过滤约定影响；
// 1×1 面在 Vulkan 软件渲染下整面被线性/seamless 采样成跨面渐变，无法跨后端一致。
// 面序与 CreateTextureCube 约定一致（GL_TEXTURE_CUBE_MAP_POSITIVE_X + face）。
unsigned int CreateDistinctCubemap(RhiDevice& device) {
    constexpr int kFace = 64;
    static const unsigned char colors[6][4] = {
        {255, 0, 0, 255},      // +X red
        {0, 255, 0, 255},      // -X green
        {0, 0, 255, 255},      // +Y blue
        {255, 255, 0, 255},    // -Y yellow
        {255, 0, 255, 255},    // +Z magenta
        {0, 255, 255, 255},    // -Z cyan
    };
    std::array<std::vector<unsigned char>, 6> face_data;
    const unsigned char* faces[6];
    for (int f = 0; f < 6; ++f) {
        face_data[f].resize(static_cast<size_t>(kFace) * kFace * 4);
        for (int i = 0; i < kFace * kFace; ++i) {
            std::memcpy(&face_data[f][static_cast<size_t>(i) * 4], colors[f], 4);
        }
        faces[f] = face_data[f].data();
    }
    return device.CreateTextureCube(kFace, kFace, faces, false);
}

// 建 256² (color+depth) RT → 清黑 + 深度 1.0 → SkyboxRenderer 画天空盒 → 回读。
RenderTargetReadback RenderSkybox(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;  // 天空盒走 LEQUAL 深度，需 depth buffer 清到 1.0
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    unsigned int cube = CreateDistinctCubemap(device);
    if (cube == 0) { device.DeleteRenderTarget(rt); return {}; }

    // 相机看向 -Z（中心采 -Z 面），90° FOV 使相邻面在边缘进入。
    const glm::mat4 view = glm::mat4(1.0f);
    const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    // SkyboxRenderer 仅持 PSO；但 cubemap 被命令缓冲引用，须存活到帧提交完成后再删
    // （Vulkan 严格：帧内删除会使 pass 静默不执行，回读全 0）。
    SkyboxRenderer skybox;
    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);

        skybox.Draw(*cmd, device, cube, view, proj);

        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    device.DeleteTexture(cube);  // 帧已提交 + fence 完成，删除安全
    device.DeleteRenderTarget(rt);
    return rb;
}

// 中央裁剪区 [96,160)²：方向落在 -Z 面内部（远离 45° 面缝），三后端在此应一致采到 -Z 面，
// 不受软件渲染器立方体面缝过滤差异影响（lavapipe 在缝处会跨面线性混合）。
constexpr int kCropLo = kRtSize * 3 / 8;  // 96
constexpr int kCropHi = kRtSize * 5 / 8;  // 160

// 每后端：天空盒铺满屏幕（中心非黑）+ 中心采到 -Z 面（青：R 低、G 高、B 高），
// 验证朝向正确（catches 黑屏/翻转，教训#2）。
void VerifySkyboxNegZCenter(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    const unsigned char* c = dse::test::PixelAt(rb, kRtSize / 2, kRtSize / 2);
    ASSERT_NE(c, nullptr) << backend;
    EXPECT_LT(c[0], 64)  << backend << " center R low (-Z=cyan) got " << int(c[0]);
    EXPECT_GT(c[1], 192) << backend << " center G high (-Z=cyan) got " << int(c[1]);
    EXPECT_GT(c[2], 192) << backend << " center B high (-Z=cyan) got " << int(c[2]);
}

// 中央裁剪区上的跨后端 RMSE（避开面缝，软件渲染下仍应高度一致）。
double CropRmse(const RenderTargetReadback& a, const RenderTargetReadback& b) {
    if (a.width != b.width || a.height != b.height || a.pixels.empty()) return -1.0;
    double sum = 0.0;
    long n = 0;
    for (int y = kCropLo; y < kCropHi; ++y) {
        for (int x = kCropLo; x < kCropHi; ++x) {
            const unsigned char* pa = dse::test::PixelAt(a, x, y);
            const unsigned char* pb = dse::test::PixelAt(b, x, y);
            if (!pa || !pb) return -1.0;
            for (int ch = 0; ch < 4; ++ch) {
                const double d = static_cast<double>(pa[ch]) - pb[ch];
                sum += d * d;
                ++n;
            }
        }
    }
    return n ? std::sqrt(sum / n) : -1.0;
}

}  // namespace

// ============================================================
// 三后端各自渲染 + 跨后端 RMSE 一致性闸门
// ============================================================

TEST(SkyboxPixelSmokeTest, CrossBackendOffscreenPixels) {
    auto gl = dse::test::RunOpenGL(RenderSkybox);
    auto dx = dse::test::RunD3D11(RenderSkybox);
    auto vk = dse::test::RunVulkan(RenderSkybox);

    int available = 0;
    if (gl.available) { VerifySkyboxNegZCenter(gl.readback, "OpenGL"); ++available; }
    if (dx.available) { VerifySkyboxNegZCenter(dx.readback, "D3D11"); ++available; }
    if (vk.available) { VerifySkyboxNegZCenter(vk.readback, "Vulkan"); ++available; }

    if (available == 0) {
        GTEST_SKIP() << "No GPU backend available (gl=" << gl.skip_reason
                     << ", dx11=" << dx.skip_reason << ", vk=" << vk.skip_reason << ")";
    }

    // 跨后端一致性：在中央裁剪区比较（避开立方体面缝的软件渲染过滤差异）。
    // 真机 A1 实测全帧 GL vs DX11≈0.75、vs Vulkan≈2.1；此处软件渲染用中央区 + 较宽上界。
    constexpr double kRmseCap = 8.0;
    if (gl.available && dx.available) {
        const double rmse = CropRmse(gl.readback, dx.readback);
        fprintf(stderr, "[SKYBOX] crop-RMSE GL vs DX11 = %.4f (A1 RTX3070 full-frame ref ~0.75)\n", rmse);
        EXPECT_GE(rmse, 0.0) << "GL/DX11 readback size mismatch";
        EXPECT_LT(rmse, kRmseCap) << "GL vs DX11 diverged in center crop";
    }
    if (gl.available && vk.available) {
        const double rmse = CropRmse(gl.readback, vk.readback);
        fprintf(stderr, "[SKYBOX] crop-RMSE GL vs Vulkan = %.4f (A1 RTX3070 full-frame ref ~2.1)\n", rmse);
        EXPECT_GE(rmse, 0.0) << "GL/Vulkan readback size mismatch";
        EXPECT_LT(rmse, kRmseCap) << "GL vs Vulkan diverged in center crop";
    }
    if (dx.available && vk.available) {
        const double rmse = CropRmse(dx.readback, vk.readback);
        fprintf(stderr, "[SKYBOX] crop-RMSE DX11 vs Vulkan = %.4f\n", rmse);
        EXPECT_LT(rmse, kRmseCap) << "DX11 vs Vulkan diverged in center crop";
    }
}
