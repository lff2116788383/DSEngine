/**
 * @file postprocess_pixel_smoke_test.cpp
 * @brief 阶段 2b 跨后端后处理像素闸门：在 GL/DX11/Vulkan 三后端跑同一份后处理请求，
 *        断言①解析真值（passthrough 逐像素等于输入；tonemapping 把匀色输入映成
 *        匀色灰阶输出）②三后端互相一致（RMSE 阈内）。
 *
 * 本闸门是 DrawPostProcess → PostProcessRenderer 迁移的回归基线（仿 A1 skybox /
 * B2a sprite 的做法）：断言后端无关的解析真值，而非对照旧实现，使迁移前后同一份
 * PostProcessRequest 必须产出同样像素。迁移时只需把 RenderFn 内的 driver 从
 * cmd.DrawPostProcess 换成 PostProcessRenderer，断言不变。
 *
 * 取样点用「左右竖分」与「匀色」两种对 DX11 离屏回读垂直翻转鲁棒的图案
 * （竖直翻转不改变左右半 / 匀色），故跨后端整帧比较与定点校验都不受翻转影响。
 */

#include "rhi_pixel_harness.h"

#include <gtest/gtest.h>

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/postprocess_common.h"
#include "engine/render/post_process_renderer.h"

#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;

// 左半红 / 右半蓝（关于水平中线对称，竖直翻转不变）。alpha 全 255。
std::vector<unsigned char> BuildSplitTexels() {
    std::vector<unsigned char> px(static_cast<size_t>(kRtSize) * kRtSize * 4);
    for (int y = 0; y < kRtSize; ++y) {
        for (int x = 0; x < kRtSize; ++x) {
            unsigned char* p = px.data() + (static_cast<size_t>(y) * kRtSize + x) * 4;
            const bool left = x < kRtSize / 2;
            p[0] = left ? 255 : 0;
            p[1] = 0;
            p[2] = left ? 0 : 255;
            p[3] = 255;
        }
    }
    return px;
}

// 匀色中灰（HDR 输入），用于 tonemapping：匀色进 → 匀色出。
std::vector<unsigned char> BuildUniformTexels(unsigned char v) {
    std::vector<unsigned char> px(static_cast<size_t>(kRtSize) * kRtSize * 4);
    for (size_t i = 0; i < px.size(); i += 4) {
        px[i] = v; px[i + 1] = v; px[i + 2] = v; px[i + 3] = 255;
    }
    return px;
}

// 公共：建源纹理 → 建输出 RT → 在一个 render pass 内跑一次 DrawPostProcess → 回读。
RenderTargetReadback RenderPP(RhiDevice& device, const PostProcessRequest& req_template,
                              const std::vector<unsigned char>& src_texels) {
    const unsigned int src_tex =
        device.CreateTexture2D(kRtSize, kRtSize, src_texels.data(), /*linear_filter=*/false);
    if (src_tex == 0) return {};

    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    const unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) { device.DeleteTexture(src_tex); return {}; }

    PostProcessRequest req = req_template;
    req.source_texture = src_tex;

    // 后处理全屏四边形需要关闭背面剔除（生产里各 Pass 在 DrawPostProcess 前都会
    // SetPipelineState 到一个 cull-off 的 PSO；DX11 默认 PP 路径不自带剔除状态，
    // 缺此会被默认 cull-back 丢弃整屏）。深度/混合关闭。
    PipelineStateDesc pp_pso_desc;
    pp_pso_desc.blend_enabled = false;
    pp_pso_desc.depth_test_enabled = false;
    pp_pso_desc.depth_write_enabled = false;
    pp_pso_desc.culling_enabled = false;
    const unsigned int pp_pso = device.CreatePipelineState(pp_pso_desc);

    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        if (pp_pso != 0) cmd->SetPipelineState(pp_pso);
        cmd->DrawPostProcess(req);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    device.DeleteRenderTarget(rt);
    device.DeleteTexture(src_tex);
    return rb;
}

// 同 RenderPP，但 driver 换成 PostProcessRenderer（通用原语路径），断言不变。
// 验证迁移后的渲染器与 DrawPostProcess ABI 产出同样的解析真值像素。
RenderTargetReadback RenderPPViaRenderer(RhiDevice& device,
                                         const PostProcessRequest& req_template,
                                         const std::vector<unsigned char>& src_texels) {
    const unsigned int src_tex =
        device.CreateTexture2D(kRtSize, kRtSize, src_texels.data(), /*linear_filter=*/false);
    if (src_tex == 0) return {};

    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    const unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) { device.DeleteTexture(src_tex); return {}; }

    PostProcessRequest req = req_template;
    req.source_texture = src_tex;

    PostProcessRenderer renderer;
    bool drawn = false;
    device.BeginFrame();
    renderer.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        drawn = renderer.Draw(*cmd, device, req);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb;
    if (drawn) rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    device.DeleteTexture(src_tex);
    return rb;  // drawn==false → 空 readback（效果未迁移），断言会因尺寸不符而失败/暴露
}

// 同 RenderPPViaRenderer，但额外绑定一张纹理（slot=GLSL binding → t<slot-1>），
// 用于强校验双纹理效果的额外纹理通路（如 dof 的 u_color_texture@binding2 → t1）。
RenderTargetReadback RenderPPViaRenderer2Tex(RhiDevice& device,
                                             const PostProcessRequest& req_template,
                                             const std::vector<unsigned char>& src_texels,
                                             uint32_t extra_slot,
                                             const std::vector<unsigned char>& extra_texels) {
    const unsigned int src_tex =
        device.CreateTexture2D(kRtSize, kRtSize, src_texels.data(), /*linear_filter=*/false);
    if (src_tex == 0) return {};
    const unsigned int extra_tex =
        device.CreateTexture2D(kRtSize, kRtSize, extra_texels.data(), /*linear_filter=*/false);
    if (extra_tex == 0) { device.DeleteTexture(src_tex); return {}; }

    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    const unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) { device.DeleteTexture(src_tex); device.DeleteTexture(extra_tex); return {}; }

    PostProcessRequest req = req_template;
    req.source_texture = src_tex;
    req.Tex(extra_slot, extra_tex);

    PostProcessRenderer renderer;
    bool drawn = false;
    device.BeginFrame();
    renderer.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        drawn = renderer.Draw(*cmd, device, req);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb;
    if (drawn) rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    device.DeleteTexture(src_tex);
    device.DeleteTexture(extra_tex);
    return rb;
}

// 同 RenderPPViaRenderer，但可绑定多张额外纹理（slot → t<slot-1>），用于三纹理+
// 效果（如 taa_resolve：source + u_motion_vector + u_history）。
struct ExtraTex { uint32_t slot; std::vector<unsigned char> texels; };
RenderTargetReadback RenderPPViaRendererNTex(RhiDevice& device,
                                             const PostProcessRequest& req_template,
                                             const std::vector<unsigned char>& src_texels,
                                             const std::vector<ExtraTex>& extras) {
    const unsigned int src_tex =
        device.CreateTexture2D(kRtSize, kRtSize, src_texels.data(), /*linear_filter=*/false);
    if (src_tex == 0) return {};
    std::vector<unsigned int> extra_handles;
    for (const auto& e : extras) {
        const unsigned int h =
            device.CreateTexture2D(kRtSize, kRtSize, e.texels.data(), /*linear_filter=*/false);
        if (h == 0) {
            for (unsigned int x : extra_handles) device.DeleteTexture(x);
            device.DeleteTexture(src_tex);
            return {};
        }
        extra_handles.push_back(h);
    }

    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    const unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) {
        for (unsigned int x : extra_handles) device.DeleteTexture(x);
        device.DeleteTexture(src_tex);
        return {};
    }

    PostProcessRequest req = req_template;
    req.source_texture = src_tex;
    for (size_t i = 0; i < extras.size(); ++i) req.Tex(extras[i].slot, extra_handles[i]);

    PostProcessRenderer renderer;
    bool drawn = false;
    device.BeginFrame();
    renderer.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        drawn = renderer.Draw(*cmd, device, req);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb;
    if (drawn) rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    device.DeleteTexture(src_tex);
    for (unsigned int x : extra_handles) device.DeleteTexture(x);
    return rb;
}

RenderTargetReadback RenderPassthrough(RhiDevice& device) {
    return RenderPP(device, PostProcessRequest("postprocess_passthrough", 0), BuildSplitTexels());
}

RenderTargetReadback RenderPassthroughViaRenderer(RhiDevice& device) {
    return RenderPPViaRenderer(device, PostProcessRequest("postprocess_passthrough", 0),
                               BuildSplitTexels());
}

// fxaa：平坦区域（lumaRange < 阈值）走早退直通，故左右半内部逐像素 = 输入。
// 取样点（x=64/192）远离 x=128 分界，不受边缘混合影响 → 复用 passthrough 真值。
RenderTargetReadback RenderFxaaViaRenderer(RhiDevice& device) {
    return RenderPPViaRenderer(
        device,
        PostProcessRequest("fxaa", 0,
                           {static_cast<float>(kRtSize), static_cast<float>(kRtSize)}),
        BuildSplitTexels());
}

RenderTargetReadback RenderTonemapping(RhiDevice& device) {
    // exposure = 1.0；无 auto-exposure / LUT 额外纹理。
    return RenderPP(device, PostProcessRequest("tonemapping", 0, {1.0f}), BuildUniformTexels(128));
}

// bloom_extract（带参 UBO 契约，强校验 threshold 真实生效）：
// 匀色 0.5（亮度 0.5）输入。高阈值 0.9 → contribution=0 → 全黑；
// 低阈值 0.05 → contribution=1 → 原样灰 128。两条对比证明 threshold UBO 确实绑定
// （若 UBO 未绑定/读到大垃圾值，高阈值用例不会变黑 → 被捕获）。
RenderTargetReadback RenderBloomExtractHigh(RhiDevice& device) {
    return RenderPPViaRenderer(device, PostProcessRequest("bloom_extract", 0, {0.9f, 0.05f}),
                               BuildUniformTexels(128));
}
RenderTargetReadback RenderBloomExtractLow(RhiDevice& device) {
    return RenderPPViaRenderer(device, PostProcessRequest("bloom_extract", 0, {0.05f, 0.05f}),
                               BuildUniformTexels(128));
}

// dof（双纹理强校验）：bokeh_radius=0 → 16 个采样全落中心 → 输出 = u_color_texture（t1）。
// 源(depth@t0)=匀色 128，颜色(t1)=匀色 200。若额外纹理 t1 未绑定 → 输出近黑 → 被捕获，
// 证明 PostProcessRenderer 额外纹理通路（.Tex(2,..) → t1）真实生效。
RenderTargetReadback RenderDofColorPass(RhiDevice& device) {
    return RenderPPViaRenderer2Tex(
        device,
        PostProcessRequest("dof", 0, {10.0f, 5.0f, 0.0f, 0.1f, 1000.0f,
                                      static_cast<float>(kRtSize), static_cast<float>(kRtSize)}),
        BuildUniformTexels(128), 2, BuildUniformTexels(200));
}

// motion_blur（双纹理）：mv 源(t0)=匀色 0 → velocity=0 → 所有采样落中心 →
// 输出 = u_color_texture（t1）匀色 180。验证额外纹理 t1 绑定 + 着色器 UBO 上传。
RenderTargetReadback RenderMotionBlurColorPass(RhiDevice& device) {
    return RenderPPViaRenderer2Tex(
        device,
        PostProcessRequest("motion_blur", 0, {1.0f, 8.0f,
                           static_cast<float>(kRtSize), static_cast<float>(kRtSize)}),
        BuildUniformTexels(0), 2, BuildUniformTexels(180));
}

// ssr：depth>=1.0 像素走早退（FragColor=0）。源(depth@t0)=匀色 255(=1.0) → 全屏早退 → 全黑。
// 验证 ssr 着色器编译 / 源纹理绑定 / UBO 上传不崩（颜色 t1 提供但早退路径不读）。
RenderTargetReadback RenderSsrEarlyOut(RhiDevice& device) {
    return RenderPPViaRenderer2Tex(
        device,
        PostProcessRequest("ssr", 0, {100.0f, 0.5f, 1.0f, 32.0f, 0.1f, 1000.0f,
                           static_cast<float>(kRtSize), static_cast<float>(kRtSize), 0.1f, 0.8f}),
        BuildUniformTexels(255), 2, BuildUniformTexels(200));
}

// taa_resolve（三纹理）：frame_index=0 → alpha=1 → 输出 = current（screenTexture@t0）。
// source=左红右蓝分块、motion_vector(t1)=匀色 0、history(t4)=匀色任意。
// 输出复用 passthrough 真值 → 验证三纹理绑定 + 源@t0 + frame_index UBO 走首帧路径。
RenderTargetReadback RenderTaaFirstFrame(RhiDevice& device) {
    return RenderPPViaRendererNTex(
        device,
        PostProcessRequest("taa_resolve", 0, {0.9f, 0.0f, 0.0f, 0.0f,
                           static_cast<float>(kRtSize), static_cast<float>(kRtSize)}),
        BuildSplitTexels(),
        {{2, BuildUniformTexels(0)}, {5, BuildUniformTexels(64)}});
}

void ExpectColorNear(const RenderTargetReadback& rb, int x, int y,
                     int r, int g, int b, const char* tag) {
    const unsigned char* p = dse::test::PixelAt(rb, x, y);
    ASSERT_NE(p, nullptr) << tag << " (" << x << "," << y << ")";
    const int tol = 12;
    EXPECT_NEAR(static_cast<int>(p[0]), r, tol) << tag << " R @(" << x << "," << y << ")";
    EXPECT_NEAR(static_cast<int>(p[1]), g, tol) << tag << " G @(" << x << "," << y << ")";
    EXPECT_NEAR(static_cast<int>(p[2]), b, tol) << tag << " B @(" << x << "," << y << ")";
}

// passthrough 解析真值：输出逐像素等于输入（左半红、右半蓝），取样点避开 x=128 分界。
void VerifyPassthrough(const RenderTargetReadback& rb, const char* tag) {
    ASSERT_EQ(rb.width, kRtSize) << tag;
    ASSERT_EQ(rb.height, kRtSize) << tag;
    ExpectColorNear(rb, 64, 128, 255, 0, 0, tag);    // 左半红
    ExpectColorNear(rb, 192, 128, 0, 0, 255, tag);   // 右半蓝
    ExpectColorNear(rb, 64, 40, 255, 0, 0, tag);     // 左半红（另一行）
    ExpectColorNear(rb, 192, 220, 0, 0, 255, tag);   // 右半蓝（另一行）
}

// tonemapping 真值：匀色进 → 匀色灰阶出（R==G==B，且非纯黑/纯白），全屏一致。
void VerifyTonemapping(const RenderTargetReadback& rb, const char* tag) {
    ASSERT_EQ(rb.width, kRtSize) << tag;
    ASSERT_EQ(rb.height, kRtSize) << tag;
    const unsigned char* c = dse::test::PixelAt(rb, 128, 128);
    ASSERT_NE(c, nullptr) << tag;
    const int r = c[0], g = c[1], b = c[2];
    EXPECT_GT(r, 4) << tag << " 输出过暗（接近纯黑）";
    EXPECT_LT(r, 251) << tag << " 输出过亮（接近纯白）";
    EXPECT_NEAR(g, r, 6) << tag << " 灰阶不保（G≠R）";
    EXPECT_NEAR(b, r, 6) << tag << " 灰阶不保（B≠R）";
    // 全屏匀色：四角与中心一致。
    for (auto [x, y] : {std::pair<int,int>{20, 20}, {235, 20}, {20, 235}, {235, 235}}) {
        const unsigned char* p = dse::test::PixelAt(rb, x, y);
        ASSERT_NE(p, nullptr) << tag;
        EXPECT_NEAR(static_cast<int>(p[0]), r, 6) << tag << " 非匀色 @(" << x << "," << y << ")";
    }
}

// bloom_extract 高阈值真值：全屏近黑。
void VerifyBloomExtractHigh(const RenderTargetReadback& rb, const char* tag) {
    ASSERT_EQ(rb.width, kRtSize) << tag;
    ASSERT_EQ(rb.height, kRtSize) << tag;
    ExpectColorNear(rb, 128, 128, 0, 0, 0, tag);
    ExpectColorNear(rb, 20, 20, 0, 0, 0, tag);
    ExpectColorNear(rb, 235, 235, 0, 0, 0, tag);
}
// bloom_extract 低阈值真值：全屏原样灰 128。
void VerifyBloomExtractLow(const RenderTargetReadback& rb, const char* tag) {
    ASSERT_EQ(rb.width, kRtSize) << tag;
    ASSERT_EQ(rb.height, kRtSize) << tag;
    ExpectColorNear(rb, 128, 128, 128, 128, 128, tag);
    ExpectColorNear(rb, 20, 235, 128, 128, 128, tag);
}

// dof 双纹理真值：全屏 = 颜色纹理 t1 的匀色 200。
void VerifyDofColorPass(const RenderTargetReadback& rb, const char* tag) {
    ASSERT_EQ(rb.width, kRtSize) << tag;
    ASSERT_EQ(rb.height, kRtSize) << tag;
    ExpectColorNear(rb, 128, 128, 200, 200, 200, tag);
    ExpectColorNear(rb, 40, 40, 200, 200, 200, tag);
    ExpectColorNear(rb, 220, 220, 200, 200, 200, tag);
}

// motion_blur 双纹理真值：全屏 = 颜色纹理 t1 的匀色 180。
void VerifyMotionBlurColorPass(const RenderTargetReadback& rb, const char* tag) {
    ASSERT_EQ(rb.width, kRtSize) << tag;
    ASSERT_EQ(rb.height, kRtSize) << tag;
    ExpectColorNear(rb, 128, 128, 180, 180, 180, tag);
    ExpectColorNear(rb, 40, 220, 180, 180, 180, tag);
    ExpectColorNear(rb, 220, 40, 180, 180, 180, tag);
}

// ssr 早退真值：全屏近黑。
void VerifySsrEarlyOut(const RenderTargetReadback& rb, const char* tag) {
    ASSERT_EQ(rb.width, kRtSize) << tag;
    ASSERT_EQ(rb.height, kRtSize) << tag;
    ExpectColorNear(rb, 128, 128, 0, 0, 0, tag);
    ExpectColorNear(rb, 40, 40, 0, 0, 0, tag);
    ExpectColorNear(rb, 220, 220, 0, 0, 0, tag);
}

// taa 首帧真值：alpha=1 → 输出 = current = source（复用 passthrough 左红右蓝）。
void VerifyTaaFirstFrame(const RenderTargetReadback& rb, const char* tag) {
    VerifyPassthrough(rb, tag);
}

void RunBackend(const char* backend, dse::test::BackendResult (*run)(const dse::test::RenderFn&),
                const dse::test::RenderFn& fn, void (*verify)(const RenderTargetReadback&, const char*)) {
    auto r = run(fn);
    if (!r.available) GTEST_SKIP() << backend << " unavailable: " << r.skip_reason;
    verify(r.readback, backend);
}

void CrossBackendRmse(const dse::test::RenderFn& fn, const char* label, double gate) {
    auto gl = dse::test::RunOpenGL(fn);
    auto dx = dse::test::RunD3D11(fn);
    auto vk = dse::test::RunVulkan(fn);
    const int available = (gl.available ? 1 : 0) + (dx.available ? 1 : 0) + (vk.available ? 1 : 0);
    if (available < 2) GTEST_SKIP() << "need >=2 backends for cross-backend RMSE";
    if (gl.available && vk.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, vk.readback);
        fprintf(stderr, "[PP-2b %s] GL-vs-Vulkan RMSE = %.4f\n", label, rmse);
        EXPECT_LT(rmse, gate) << "GL vs Vulkan diverged";
    }
    if (gl.available && dx.available) {
        double rmse = dse::test::ComputeRmse(gl.readback, dx.readback);
        fprintf(stderr, "[PP-2b %s] GL-vs-D3D11 RMSE = %.4f\n", label, rmse);
        EXPECT_LT(rmse, gate) << "GL vs D3D11 diverged";
    }
    if (dx.available && vk.available) {
        double rmse = dse::test::ComputeRmse(dx.readback, vk.readback);
        fprintf(stderr, "[PP-2b %s] D3D11-vs-Vulkan RMSE = %.4f\n", label, rmse);
        EXPECT_LT(rmse, gate) << "D3D11 vs Vulkan diverged";
    }
}

}  // namespace

TEST(PostProcessPixelSmokeTest, OpenGLPassthrough) {
    RunBackend("OpenGL", &dse::test::RunOpenGL, RenderPassthrough, VerifyPassthrough);
}
TEST(PostProcessPixelSmokeTest, D3D11Passthrough) {
    RunBackend("D3D11", &dse::test::RunD3D11, RenderPassthrough, VerifyPassthrough);
}
TEST(PostProcessPixelSmokeTest, VulkanPassthrough) {
    RunBackend("Vulkan", &dse::test::RunVulkan, RenderPassthrough, VerifyPassthrough);
}
TEST(PostProcessPixelSmokeTest, CrossBackendPassthroughConsistent) {
    CrossBackendRmse(RenderPassthrough, "passthrough", 8.0);
}

// PostProcessRenderer（通用原语）路径：passthrough 必须产出与 ABI 同样的逐像素真值。
TEST(PostProcessPixelSmokeTest, OpenGLPassthroughRenderer) {
    RunBackend("OpenGL", &dse::test::RunOpenGL, RenderPassthroughViaRenderer, VerifyPassthrough);
}
TEST(PostProcessPixelSmokeTest, D3D11PassthroughRenderer) {
    RunBackend("D3D11", &dse::test::RunD3D11, RenderPassthroughViaRenderer, VerifyPassthrough);
}
TEST(PostProcessPixelSmokeTest, VulkanPassthroughRenderer) {
    RunBackend("Vulkan", &dse::test::RunVulkan, RenderPassthroughViaRenderer, VerifyPassthrough);
}
TEST(PostProcessPixelSmokeTest, CrossBackendPassthroughRendererConsistent) {
    CrossBackendRmse(RenderPassthroughViaRenderer, "passthrough_renderer", 8.0);
}

// fxaa（首个带参 UBO 契约效果）经 PostProcessRenderer：平坦区直通真值。
TEST(PostProcessPixelSmokeTest, OpenGLFxaaRenderer) {
    RunBackend("OpenGL", &dse::test::RunOpenGL, RenderFxaaViaRenderer, VerifyPassthrough);
}
TEST(PostProcessPixelSmokeTest, D3D11FxaaRenderer) {
    RunBackend("D3D11", &dse::test::RunD3D11, RenderFxaaViaRenderer, VerifyPassthrough);
}
TEST(PostProcessPixelSmokeTest, VulkanFxaaRenderer) {
    RunBackend("Vulkan", &dse::test::RunVulkan, RenderFxaaViaRenderer, VerifyPassthrough);
}
TEST(PostProcessPixelSmokeTest, CrossBackendFxaaRendererConsistent) {
    CrossBackendRmse(RenderFxaaViaRenderer, "fxaa_renderer", 8.0);
}

// bloom_extract 经 PostProcessRenderer：threshold UBO 参数生效校验（高阈值黑 / 低阈值灰）。
TEST(PostProcessPixelSmokeTest, OpenGLBloomExtractHigh) {
    RunBackend("OpenGL", &dse::test::RunOpenGL, RenderBloomExtractHigh, VerifyBloomExtractHigh);
}
TEST(PostProcessPixelSmokeTest, D3D11BloomExtractHigh) {
    RunBackend("D3D11", &dse::test::RunD3D11, RenderBloomExtractHigh, VerifyBloomExtractHigh);
}
TEST(PostProcessPixelSmokeTest, VulkanBloomExtractHigh) {
    RunBackend("Vulkan", &dse::test::RunVulkan, RenderBloomExtractHigh, VerifyBloomExtractHigh);
}
TEST(PostProcessPixelSmokeTest, OpenGLBloomExtractLow) {
    RunBackend("OpenGL", &dse::test::RunOpenGL, RenderBloomExtractLow, VerifyBloomExtractLow);
}
TEST(PostProcessPixelSmokeTest, D3D11BloomExtractLow) {
    RunBackend("D3D11", &dse::test::RunD3D11, RenderBloomExtractLow, VerifyBloomExtractLow);
}
TEST(PostProcessPixelSmokeTest, VulkanBloomExtractLow) {
    RunBackend("Vulkan", &dse::test::RunVulkan, RenderBloomExtractLow, VerifyBloomExtractLow);
}
TEST(PostProcessPixelSmokeTest, CrossBackendBloomExtractConsistent) {
    CrossBackendRmse(RenderBloomExtractLow, "bloom_extract", 8.0);
}

// dof 经 PostProcessRenderer：双纹理额外通路（u_color_texture@t1）强校验。
TEST(PostProcessPixelSmokeTest, OpenGLDofColorPass) {
    RunBackend("OpenGL", &dse::test::RunOpenGL, RenderDofColorPass, VerifyDofColorPass);
}
TEST(PostProcessPixelSmokeTest, D3D11DofColorPass) {
    RunBackend("D3D11", &dse::test::RunD3D11, RenderDofColorPass, VerifyDofColorPass);
}
TEST(PostProcessPixelSmokeTest, VulkanDofColorPass) {
    RunBackend("Vulkan", &dse::test::RunVulkan, RenderDofColorPass, VerifyDofColorPass);
}
TEST(PostProcessPixelSmokeTest, CrossBackendDofColorPassConsistent) {
    CrossBackendRmse(RenderDofColorPass, "dof_color", 8.0);
}

// motion_blur 经 PostProcessRenderer：双纹理额外通路（u_color_texture@t1）。
TEST(PostProcessPixelSmokeTest, OpenGLMotionBlurColorPass) {
    RunBackend("OpenGL", &dse::test::RunOpenGL, RenderMotionBlurColorPass, VerifyMotionBlurColorPass);
}
TEST(PostProcessPixelSmokeTest, D3D11MotionBlurColorPass) {
    RunBackend("D3D11", &dse::test::RunD3D11, RenderMotionBlurColorPass, VerifyMotionBlurColorPass);
}
TEST(PostProcessPixelSmokeTest, VulkanMotionBlurColorPass) {
    RunBackend("Vulkan", &dse::test::RunVulkan, RenderMotionBlurColorPass, VerifyMotionBlurColorPass);
}
TEST(PostProcessPixelSmokeTest, CrossBackendMotionBlurColorPassConsistent) {
    CrossBackendRmse(RenderMotionBlurColorPass, "motion_blur_color", 8.0);
}

// ssr 经 PostProcessRenderer：depth>=1.0 早退黑屏（源绑定 + UBO 上传健全性）。
TEST(PostProcessPixelSmokeTest, OpenGLSsrEarlyOut) {
    RunBackend("OpenGL", &dse::test::RunOpenGL, RenderSsrEarlyOut, VerifySsrEarlyOut);
}
TEST(PostProcessPixelSmokeTest, D3D11SsrEarlyOut) {
    RunBackend("D3D11", &dse::test::RunD3D11, RenderSsrEarlyOut, VerifySsrEarlyOut);
}
TEST(PostProcessPixelSmokeTest, VulkanSsrEarlyOut) {
    RunBackend("Vulkan", &dse::test::RunVulkan, RenderSsrEarlyOut, VerifySsrEarlyOut);
}
TEST(PostProcessPixelSmokeTest, CrossBackendSsrEarlyOutConsistent) {
    CrossBackendRmse(RenderSsrEarlyOut, "ssr_early_out", 8.0);
}

// taa_resolve 经 PostProcessRenderer：三纹理绑定 + frame_index UBO 首帧路径。
TEST(PostProcessPixelSmokeTest, OpenGLTaaFirstFrame) {
    RunBackend("OpenGL", &dse::test::RunOpenGL, RenderTaaFirstFrame, VerifyTaaFirstFrame);
}
TEST(PostProcessPixelSmokeTest, D3D11TaaFirstFrame) {
    RunBackend("D3D11", &dse::test::RunD3D11, RenderTaaFirstFrame, VerifyTaaFirstFrame);
}
TEST(PostProcessPixelSmokeTest, VulkanTaaFirstFrame) {
    RunBackend("Vulkan", &dse::test::RunVulkan, RenderTaaFirstFrame, VerifyTaaFirstFrame);
}
TEST(PostProcessPixelSmokeTest, CrossBackendTaaFirstFrameConsistent) {
    CrossBackendRmse(RenderTaaFirstFrame, "taa_first_frame", 8.0);
}

TEST(PostProcessPixelSmokeTest, OpenGLTonemapping) {
    RunBackend("OpenGL", &dse::test::RunOpenGL, RenderTonemapping, VerifyTonemapping);
}
TEST(PostProcessPixelSmokeTest, D3D11Tonemapping) {
    RunBackend("D3D11", &dse::test::RunD3D11, RenderTonemapping, VerifyTonemapping);
}
TEST(PostProcessPixelSmokeTest, VulkanTonemapping) {
    RunBackend("Vulkan", &dse::test::RunVulkan, RenderTonemapping, VerifyTonemapping);
}
TEST(PostProcessPixelSmokeTest, CrossBackendTonemappingConsistent) {
    CrossBackendRmse(RenderTonemapping, "tonemapping", 10.0);
}
