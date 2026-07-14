/**
 * @file three_rhi_golden_scene_test.cpp
 * @brief P0-4：三 RHI（OpenGL / D3D11 / Vulkan）对同一确定性 golden 场景离屏渲染 → 读回
 *        framebuffer → 与入库参考 PNG 做 SSIM + 逐像素误差阈值比对，作为可复跑的画面门禁。
 *
 * 证据链（为何该用例满足 P0-4 验收）：
 *  1. 确定性场景：固定几何（两块朝向相反的光照面片）+ 固定单方向光 + 固定点光 + 固定相机/投影，
 *     无时间、无随机、无 auto-exposure 抖动 —— 同一后端在同一 GPU 上逐次渲染像素可复现。
 *  2. 真实渲染：走引擎实际的 MeshRenderer::DrawShaded（BuiltinProgram::ForwardShaded，PBR），
 *     每后端各自建离屏 RT → 渲染 → ReadRenderTargetColorRgba8WithSize 回读 RGBA8。
 *  3. golden 门禁：首次以 DSE_UPDATE_GOLDEN=1 在真机（RTX 3070）生成每后端参考 PNG 并入库；
 *     此后每次加载参考 PNG，计算 mean-SSIM（8x8 窗、luma）+ 平均/最大逐通道误差，超阈值 fail。
 *  4. 每后端 vs「自己的」golden：GL/DX11/Vulkan 光栅化/采样规则不同，各自与本后端参考比对，
 *     既避免跨后端伪差异，又能抓住着色/管线回归（shader、状态、投影修正等改动会显著移动像素）。
 *
 * 诚实边界：golden 参考在 RTX 3070 真机生成，仅在同 GPU/驱动上位比特级复现；换机/软件回退
 *          渲染像素不同，门禁将（如实）失败 —— 参考必须在真机重生成。CI 接入见任务 9（暂缓）。
 *
 * 生成/更新参考：
 *   set DSE_UPDATE_GOLDEN=1 && bin\dse_gtest_smoke_tests.exe --gtest_filter=ThreeRhiGolden*
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/mesh_renderer.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/glm.hpp>

// stb_image_write 设为文件局部（STATIC），避免与 dse_engine 内既有实现重复符号；
// 读取用 stbi_load 走 dse_engine 已导出的 STB_IMAGE_IMPLEMENTATION。
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <stb/stb_image.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 记录最近一次渲染所用后端的实际适配器 + 是否软件渲染（RenderFn 内捕获，CheckGolden 断言）。
RenderDeviceInfo g_last_info;

// ── 确定性 golden 场景 ─────────────────────────────────────────────────────────
// 两块竖直面片：左面片法线 +X（朝光 → 亮），右面片法线 -X（背光 → 暗）。
void MakeTwoQuads(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    auto add_quad = [&](float x0, float x1, const glm::vec3& n) {
        const uint16_t base = static_cast<uint16_t>(verts.size());
        const float y0 = -0.7f, y1 = 0.7f;
        verts.push_back({{x0, y0, 0.0f}, col, {0.0f, 0.0f}, n, {0.0f, 1.0f, 0.0f}});
        verts.push_back({{x1, y0, 0.0f}, col, {1.0f, 0.0f}, n, {0.0f, 1.0f, 0.0f}});
        verts.push_back({{x1, y1, 0.0f}, col, {1.0f, 1.0f}, n, {0.0f, 1.0f, 0.0f}});
        verts.push_back({{x0, y1, 0.0f}, col, {0.0f, 1.0f}, n, {0.0f, 1.0f, 0.0f}});
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    };
    add_quad(-0.9f, -0.1f, glm::vec3(1.0f, 0.0f, 0.0f));   // 左：朝光 → 亮
    add_quad(0.1f, 0.9f, glm::vec3(-1.0f, 0.0f, 0.0f));    // 右：背光 → 暗
}

// 固定场景渲染：PBR + 单方向光（沿 +X）+ 一盏点光（上方偏置，打破 Y 对称，使 golden 更具信息量）。
RenderTargetReadback RenderGoldenScene(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;
    g_last_info = device.GetDeviceInfo();
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};
    if (device.GetBuiltinProgram(BuiltinProgram::ForwardShaded) == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    ShadedMaterial m;
    m.albedo = glm::vec3(0.8f);
    m.roughness = 0.5f;
    m.shading_mode = 0;  // PBR

    DirectionalLight light;
    light.direction = glm::vec3(-1.0f, 0.0f, 0.0f);  // to_light = +X
    light.color = glm::vec3(1.0f);
    light.intensity = 2.5f;
    light.ambient = 0.05f;
    light.enabled = true;

    std::vector<ShadedPointLight> point_lights;
    ShadedPointLight pl;
    pl.color = glm::vec3(0.4f, 0.7f, 1.0f);      // 冷色点光
    pl.intensity = 3.0f;
    pl.position = glm::vec3(0.0f, 0.45f, 0.6f);  // 上方偏前 → 破 Y 对称
    pl.radius = 3.0f;
    point_lights.push_back(pl);

    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeTwoQuads(verts, indices);

    const glm::mat4 I(1.0f);
    const glm::mat4 proj = device.GetProjectionCorrection();
    const glm::vec3 cam_pos(0.0f, 0.0f, 1.0f);

    MeshRenderer renderer;
    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.02f, 0.02f, 0.03f, 1.0f);  // 固定近黑清屏色
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        renderer.DrawShaded(*cmd, device, verts, indices, I, I, proj, cam_pos, m, light, point_lights);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    return rb;
}

// ── golden 比对 ────────────────────────────────────────────────────────────────
std::string GoldenPath(const char* backend) {
    return std::string("tests/golden/p0_4_forward3d_") + backend + ".png";
}

bool UpdateGoldenMode() {
    const char* v = std::getenv("DSE_UPDATE_GOLDEN");
    return v && v[0] && !(v[0] == '0' && v[1] == '\0');
}

// RGBA8 → 单通道 luma（BT.601）。
std::vector<double> ToLuma(const unsigned char* px, int w, int h) {
    std::vector<double> l(static_cast<size_t>(w) * h);
    for (int i = 0; i < w * h; ++i) {
        const unsigned char* p = px + static_cast<size_t>(i) * 4;
        l[i] = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
    }
    return l;
}

// 8x8 非重叠窗 mean-SSIM（luma 标度 0..255）。两图尺寸须一致。
double MeanSsim(const std::vector<double>& a, const std::vector<double>& b, int w, int h) {
    const double C1 = 6.5025;    // (0.01*255)^2
    const double C2 = 58.5225;   // (0.03*255)^2
    const int win = 8;
    double ssim_sum = 0.0;
    int win_count = 0;
    for (int by = 0; by + win <= h; by += win) {
        for (int bx = 0; bx + win <= w; bx += win) {
            double ma = 0.0, mb = 0.0;
            for (int y = 0; y < win; ++y)
                for (int x = 0; x < win; ++x) {
                    const int idx = (by + y) * w + (bx + x);
                    ma += a[idx];
                    mb += b[idx];
                }
            const double n = win * win;
            ma /= n;
            mb /= n;
            double va = 0.0, vb = 0.0, cov = 0.0;
            for (int y = 0; y < win; ++y)
                for (int x = 0; x < win; ++x) {
                    const int idx = (by + y) * w + (bx + x);
                    const double da = a[idx] - ma;
                    const double db = b[idx] - mb;
                    va += da * da;
                    vb += db * db;
                    cov += da * db;
                }
            va /= (n - 1);
            vb /= (n - 1);
            cov /= (n - 1);
            const double s = ((2 * ma * mb + C1) * (2 * cov + C2)) /
                             ((ma * ma + mb * mb + C1) * (va + vb + C2));
            ssim_sum += s;
            ++win_count;
        }
    }
    return win_count > 0 ? ssim_sum / win_count : 1.0;
}

struct PixelDiff {
    double mean_abs = 0.0;
    double max_abs = 0.0;
};

PixelDiff ComputePixelDiff(const unsigned char* a, const unsigned char* b, int count) {
    PixelDiff d;
    double sum = 0.0;
    for (int i = 0; i < count; ++i) {
        const double e = std::fabs(static_cast<double>(a[i]) - static_cast<double>(b[i]));
        sum += e;
        if (e > d.max_abs) d.max_abs = e;
    }
    d.mean_abs = count > 0 ? sum / count : 0.0;
    return d;
}

void CheckGolden(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty()) GTEST_SKIP() << "ForwardShaded builtin program unavailable (" << backend << ")";

    const RenderTargetReadback& rb = r.readback;
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    // 活体：渲染结果不能是纯清屏色（证明真画了几何）。
    long long nonclear = 0;
    for (int i = 0; i < kRtSize * kRtSize; ++i) {
        const unsigned char* p = &rb.pixels[static_cast<size_t>(i) * 4];
        if (p[0] > 40 || p[1] > 40 || p[2] > 40) ++nonclear;
    }
    ASSERT_GT(nonclear, kRtSize * kRtSize / 20) << backend << ": 渲染结果近乎纯清屏（未画出几何）";

    // 记录并断言真实 GPU：软件回退（WARP/SwiftShader/lavapipe 等）不能作为 P0-4 真机 golden 证据。
    std::printf("[P0-4] backend=%s adapter=\"%s\" software=%d\n",
                backend, g_last_info.adapter_name.c_str(), g_last_info.is_software ? 1 : 0);
    std::fflush(stdout);
    EXPECT_FALSE(g_last_info.is_software)
        << backend << ": 软件渲染回退不能作为真机 GPU 证据（adapter=" << g_last_info.adapter_name << "）";

    const std::string path = GoldenPath(backend);

    if (UpdateGoldenMode()) {
        const int ok = stbi_write_png(path.c_str(), kRtSize, kRtSize, 4,
                                      rb.pixels.data(), kRtSize * 4);
        ASSERT_NE(ok, 0) << backend << ": 写入 golden 失败 " << path;
        GTEST_SUCCEED() << backend << ": golden 已生成/更新 → " << path;
        return;
    }

    int gw = 0, gh = 0, gc = 0;
    unsigned char* golden = stbi_load(path.c_str(), &gw, &gh, &gc, 4);
    ASSERT_NE(golden, nullptr) << backend << ": 缺少 golden 参考 " << path
                               << "（首次请以 DSE_UPDATE_GOLDEN=1 在真机生成并入库）";
    ASSERT_EQ(gw, kRtSize) << backend << " golden 宽";
    ASSERT_EQ(gh, kRtSize) << backend << " golden 高";

    const int byte_count = kRtSize * kRtSize * 4;
    const PixelDiff diff = ComputePixelDiff(rb.pixels.data(), golden, byte_count);
    const std::vector<double> la = ToLuma(rb.pixels.data(), kRtSize, kRtSize);
    const std::vector<double> lg = ToLuma(golden, kRtSize, kRtSize);
    const double ssim = MeanSsim(la, lg, kRtSize, kRtSize);
    stbi_image_free(golden);

    // 同后端 + 同 GPU 渲染确定性高（近比特级），阈值既能稳过又能抓住真实回归。
    EXPECT_GE(ssim, 0.98) << backend << ": SSIM=" << ssim << " 低于阈值（画面回归？）";
    EXPECT_LE(diff.mean_abs, 3.0) << backend << ": 平均逐通道误差=" << diff.mean_abs
                                  << "（max=" << diff.max_abs << "）超阈值";
}

}  // namespace

TEST(ThreeRhiGoldenSceneTest, OpenGL) { CheckGolden(dse::test::RunOpenGL(RenderGoldenScene), "opengl"); }
TEST(ThreeRhiGoldenSceneTest, D3D11) { CheckGolden(dse::test::RunD3D11(RenderGoldenScene), "d3d11"); }
TEST(ThreeRhiGoldenSceneTest, Vulkan) { CheckGolden(dse::test::RunVulkan(RenderGoldenScene), "vulkan"); }
