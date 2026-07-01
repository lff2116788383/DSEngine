/**
 * @file builtin_passes_postfx.cpp
 * @brief Post-processing render passes: Bloom, AutoExposure, SSAO, Contact Shadow,
 *        FXAA, TAA, DOF, MotionVector, MotionBlur, SSR, Outline, LightShaft.
 */

#include "engine/render/passes/builtin_passes.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/render/gi/ddgi_system.h"
#include "engine/base/debug.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/ui.h"
#include "engine/platform/screen.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/module.h"
#include "engine/render/light_buffer.h"
#include "engine/render/cluster_grid.h"
#include "engine/render/scene_renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include "engine/base/time.h"

namespace dse {
namespace render {
void BloomPass::Setup(RenderGraph& graph) {
    auto scene_color   = graph.DeclareResource("scene_color");
    auto bloom_extract = graph.DeclareResource("bloom_extract");
    auto bloom_mip0    = graph.DeclareResource("bloom_mip0");
    auto bloom_mip1    = graph.DeclareResource("bloom_mip1");
    auto bloom_mip2    = graph.DeclareResource("bloom_mip2");
    auto bloom_mip3    = graph.DeclareResource("bloom_mip3");
    auto bloom_mip4    = graph.DeclareResource("bloom_mip4");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassWrite(pass, bloom_extract);
    graph.PassWrite(pass, bloom_mip0);
    graph.PassWrite(pass, bloom_mip1);
    graph.PassWrite(pass, bloom_mip2);
    graph.PassWrite(pass, bloom_mip3);
    graph.PassWrite(pass, bloom_mip4);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void BloomPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& pp_config = snap.post_process;

    if (!ctx_.pipeline_features.bloom || !pp_config.valid || !pp_config.bloom_enabled) {
        return;
    }

    post_process_renderer_.BeginFrame();
    bloom_renderer_.BeginFrame();
    cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.bloom_extract, glm::vec4(0.0f), false});
    const unsigned int scene_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);
    const float bloom_threshold = ctx_.pipeline_overrides.bloom_threshold >= 0.0f
        ? ctx_.pipeline_overrides.bloom_threshold
        : pp_config.bloom_threshold;
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device,
        {"bloom_extract", scene_color_tex, {bloom_threshold, pp_config.bloom_knee}});
    cmd_buffer.EndRenderPass();

    unsigned int current_src = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.bloom_extract);
    int mip_w = Screen::width() / 2;
    int mip_h = Screen::height() / 2;
    for (size_t i = 0; i < ctx_.render_targets.bloom_mips.size(); ++i) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.bloom_mips[i], glm::vec4(0.0f), false});
        // compute åŽç«¯ï¼ˆDX11/Vulkanï¼‰èµ° DispatchComputePass å†™ UAVï¼›GL å›žé€€å…¨å± quadã€‚
        bloom_renderer_.Downsample(cmd_buffer, *ctx_.rhi_device, current_src,
                                   static_cast<float>(mip_w * 2), static_cast<float>(mip_h * 2));
        cmd_buffer.EndRenderPass();
        current_src = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.bloom_mips[i]);
        mip_w /= 2;
        mip_h /= 2;
        if (mip_w < 1) mip_w = 1;
        if (mip_h < 1) mip_h = 1;
    }

    for (int i = static_cast<int>(ctx_.render_targets.bloom_mips.size()) - 1; i > 0; --i) {
        unsigned int target_rt = ctx_.render_targets.bloom_mips[i - 1];
        current_src = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.bloom_mips[i]);
        cmd_buffer.BeginRenderPass({target_rt, glm::vec4(0.0f), false});
        const float mip_texel = 1.0f / static_cast<float>(std::max(mip_w, 1));
        // compute åŽç«¯ï¼ˆDX11/Vulkanï¼‰èµ° DispatchComputePass ç´¯åŠ è¿› UAVï¼›GL å›žé€€å…¨å± quadï¼ˆalpha æ··åˆï¼‰ã€‚
        bloom_renderer_.Upsample(cmd_buffer, *ctx_.rhi_device, current_src,
                                 mip_texel, pp_config.bloom_mip_weight);
        cmd_buffer.EndRenderPass();
    }
}

// ============================================================
// UIPass
// ============================================================

void UIPass::Setup(RenderGraph& graph) {
    auto ui_color = graph.DeclareResource("ui_color");
    auto pass = graph.AddPass(GetName());
    graph.PassWrite(pass, ui_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void UIPass::Execute(CommandBuffer& cmd_buffer) {
    cmd_buffer.BindPipeline(ctx_.pipeline_states.sprite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.ui, glm::vec4(0.0f), true});
    if (ctx_.render_2d_ui) {
        const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        ctx_.render_2d_ui(*ctx_.world, cmd_buffer, Screen::width(), Screen::height(), clip_correction);
    }
    cmd_buffer.EndRenderPass();
}

// ============================================================
// CompositePass
// ============================================================

void CompositePass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto ui_color    = graph.DeclareResource("ui_color");
    auto bloom_mip0  = graph.DeclareResource("bloom_mip0");
    auto ssao_color  = graph.DeclareResource("ssao_color");
    auto contact_shadow = graph.DeclareResource("contact_shadow");
    auto lum_data    = graph.DeclareResource("lum_data");
    auto main_color  = graph.DeclareResource("main_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, ui_color);
    graph.PassRead(pass, bloom_mip0);
    graph.PassRead(pass, ssao_color);
    graph.PassRead(pass, contact_shadow);
    graph.PassRead(pass, lum_data);
    graph.PassWrite(pass, main_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void CompositePass::Execute(CommandBuffer& cmd_buffer) {
    const unsigned int scene_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);
    const unsigned int ui_color_tex = ctx_.pipeline_features.ui
        ? ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.ui)
        : 0;

    const auto& snap = *ctx_.snapshot;
    const auto& pp_config = snap.post_process;
    bool pp_enabled = pp_config.valid;

    // èŽ·å– SSAO çº¹ç†ï¼ˆå¦‚æžœå¯ç”¨ï¼‰
    unsigned int ssao_tex = 0;
    if (ctx_.pipeline_features.ssao && pp_enabled && pp_config.ssao_enabled && ctx_.render_targets.ssao_blur != 0) {
        ssao_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.ssao_blur);
    }

    // èŽ·å– Contact Shadow çº¹ç†ï¼ˆå¦‚æžœå¯ç”¨ï¼‰
    unsigned int contact_shadow_tex = 0;
    if (ctx_.pipeline_features.contact_shadow && pp_enabled && pp_config.contact_shadow_enabled && ctx_.render_targets.contact_shadow != 0) {
        contact_shadow_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.contact_shadow);
    }

    // èŽ·å– auto exposure çº¹ç†ï¼ˆå¦‚æžœå¯ç”¨ï¼‰
    unsigned int ae_tex = 0;
    if (ctx_.pipeline_features.auto_exposure && ctx_.auto_exposure_active) {
        // ping-pong å·²ç¿»è½¬ï¼Œå½“å‰å¸§ç»“æžœåœ¨ 1 - current_index
        const int result_idx = 1 - ctx_.lum_ping_pong_index;
        ae_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.lum_adapted[result_idx]);
    }

    // Color Grading LUT
    float lut_tex = 0.0f;
    float lut_intensity = 0.0f;
    if (pp_enabled && pp_config.color_lut_handle != 0) {
        lut_tex = static_cast<float>(pp_config.color_lut_handle);
        lut_intensity = pp_config.color_lut_intensity;
    }

    // bloom_composite æ˜¯åŽ†å² effect nameï¼Œå½“å‰å®žé™…æ‰¿æ‹… final composite èŒè´£ã€‚
    // å·²è¿åˆ° PostProcessRendererï¼šçº¯ float UBOï¼ˆ16 æ ‡é‡ï¼Œè§ä¸‹æ–¹ PostProcessRequestï¼Œ
    // å­—æ®µé¡ºåºä¸Ž bloom_composite_ssao_ae.frag çš„ std140 UBO ä¸€è‡´ï¼‰ï¼›å„çº¹ç†ç» .Tex/.Tex3D
    // å†™å…¥å¯¹åº” bindingï¼ˆbloomBlur@2 / ssao@3 / autoExposure@4 / lut@5(3D) / contactShadow@6ï¼‰ã€‚
    float film_grain_time = 0.0f;
    if (pp_enabled && pp_config.film_grain_enabled && pp_config.film_grain_intensity > 0.0f) {
        film_grain_time = static_cast<float>(std::fmod(Time::TimeSinceStartup() * pp_config.film_grain_time_scale, 4096.0f));
    }

    cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.main, glm::vec4(0.0f), true});

    const bool bloom_enabled = ctx_.pipeline_features.bloom && pp_config.bloom_enabled;
    if (pp_enabled && (bloom_enabled || contact_shadow_tex != 0 || pp_config.vignette_enabled || pp_config.film_grain_enabled)) {
        const unsigned int bloom_tex = (bloom_enabled && !ctx_.render_targets.bloom_mips.empty())
            ? ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.bloom_mips[0])
            : 0;
        const float bloom_intensity = ctx_.pipeline_overrides.bloom_intensity >= 0.0f
            ? ctx_.pipeline_overrides.bloom_intensity
            : pp_config.bloom_intensity;
        const unsigned int lut_handle = static_cast<unsigned int>(lut_tex);
        // bloom_composite å·²è¿åˆ° PostProcessRendererï¼šparams çº¯ float UBOï¼ˆ16 æ ‡é‡ï¼Œ
        // ä¸Ž bloom_composite_ssao_ae.frag å­—æ®µåŒåºï¼‰ï¼›çº¹ç†ä¸€å¾‹ç» .Tex/.Tex3D å†™å…¥ï¼Œ
        // ä½¿èƒ½æ ‡å¿—æŒ‰çº¹ç†åœ¨å¦æ´¾ç”Ÿï¼ˆæ¸²æŸ“å™¨é¢å¤–çº¹ç†å¾ªçŽ¯é‡ handle==0 å³åœï¼Œæ•…ä»…æŒ‚éžé›¶çº¹ç†ï¼‰ã€‚
        PostProcessRequest req{"bloom_composite", scene_color_tex, {
            pp_config.exposure,
            bloom_intensity,
            bloom_tex != 0 ? 1.0f : 0.0f,
            ssao_tex != 0 ? 1.0f : 0.0f,
            ae_tex != 0 ? 1.0f : 0.0f,
            lut_handle != 0 ? 1.0f : 0.0f,
            lut_intensity,
            contact_shadow_tex != 0 ? 1.0f : 0.0f,
            pp_config.contact_shadow_strength,
            pp_config.vignette_enabled ? 1.0f : 0.0f,
            pp_config.vignette_intensity,
            pp_config.vignette_radius,
            pp_config.vignette_softness,
            pp_config.film_grain_enabled ? 1.0f : 0.0f,
            pp_config.film_grain_intensity,
            film_grain_time
        }};
        if (bloom_tex != 0)          req.Tex(2, bloom_tex);
        if (ssao_tex != 0)           req.Tex(3, ssao_tex);
        if (ae_tex != 0)             req.Tex(4, ae_tex);
        if (lut_handle != 0)         req.Tex3D(5, lut_handle);
        if (contact_shadow_tex != 0) req.Tex(6, contact_shadow_tex);
        post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, req);
    } else {
        // tonemapping / ssao_apply å·²è¿åˆ° PostProcessRendererï¼šUBO ä¸º 4 æ ‡é‡
        // {manual_exposure, auto_exposure_enabled, lut_enabled, lut_intensity}ï¼›
        // enable æ ‡å¿—æŒ‰çº¹ç†åœ¨å¦åœ¨è°ƒç”¨ç‚¹æ´¾ç”Ÿï¼ˆæ—§ binder åŽŸç”± FindTex æŽ¨å¯¼ï¼‰ã€‚
        // å¯é€‰çº¹ç†ä»…åœ¨éžé›¶æ—¶æŒ‚è½½â€”â€”æ¸²æŸ“å™¨çº¹ç†å¾ªçŽ¯é‡ handle==0 å³åœï¼Œé¿å…ä¸­æ–­åŽç»­ç»‘å®šã€‚
        const float ae_enabled  = ae_tex != 0 ? 1.0f : 0.0f;
        const float lut_enabled = static_cast<unsigned int>(lut_tex) != 0 ? 1.0f : 0.0f;
        if (ssao_tex != 0) {
            PostProcessRequest req{"ssao_apply", scene_color_tex,
                {pp_config.exposure, ae_enabled, lut_enabled, lut_intensity}};
            req.Tex(2, ssao_tex);
            if (ae_tex != 0) req.Tex(3, ae_tex);
            if (static_cast<unsigned int>(lut_tex) != 0) req.Tex3D(5, static_cast<unsigned int>(lut_tex));
            post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, req);
        } else {
            PostProcessRequest req{"tonemapping", scene_color_tex,
                {pp_config.exposure, ae_enabled, lut_enabled, lut_intensity}};
            if (ae_tex != 0) req.Tex(2, ae_tex);
            if (static_cast<unsigned int>(lut_tex) != 0) req.Tex3D(5, static_cast<unsigned int>(lut_tex));
            post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, req);
        }
    }

    if (ui_color_tex != 0) {
        post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"ui_overlay", ui_color_tex, {}, true});
    }
    cmd_buffer.EndRenderPass();
}

// ============================================================
// AutoExposurePass
// ============================================================

void AutoExposurePass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto lum_data    = graph.DeclareResource("lum_data");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassWrite(pass, lum_data);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void AutoExposurePass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& pp_config = snap.post_process;
    bool ae_enabled = ctx_.pipeline_features.auto_exposure && pp_config.valid && pp_config.auto_exposure_enabled;

    ctx_.auto_exposure_active = ae_enabled;
    if (!ae_enabled) return;
    if (ctx_.render_targets.lum_temp == 0 ||
        ctx_.render_targets.lum_adapted[0] == 0 ||
        ctx_.render_targets.lum_adapted[1] == 0) return;

    const unsigned int scene_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);
    if (scene_color_tex == 0) return;

    const int write_idx = ctx_.lum_ping_pong_index;
    const int read_idx  = 1 - write_idx;

    // Pass 1: åœºæ™¯ â†’ 64x64 log luminance (8x8 é‡‡æ ·ç½‘æ ¼)
    cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.lum_temp, glm::vec4(0.0f), true});
    post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"lum_compute", scene_color_tex});
    cmd_buffer.EndRenderPass();

    // Pass 2: 64x64 â†’ 1x1 adapted exposure (EMA blend with previous frame)
    const unsigned int lum_temp_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.lum_temp);
    const unsigned int prev_adapted_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.lum_adapted[read_idx]);

    cmd_buffer.BeginRenderPass({ctx_.render_targets.lum_adapted[write_idx], glm::vec4(1.0f), true});
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, PostProcessRequest{"lum_adapt", lum_temp_tex, {
        ctx_.delta_time,
        pp_config.adaptation_speed_up,
        pp_config.adaptation_speed_down,
        pp_config.exposure_min,
        pp_config.exposure_max,
        pp_config.exposure_compensation
    }}.Tex(2, prev_adapted_tex));
    cmd_buffer.EndRenderPass();

    // ç¿»è½¬ ping-pong
    ctx_.lum_ping_pong_index = 1 - ctx_.lum_ping_pong_index;
}

// ============================================================
// SSAOPass
// ============================================================

void SSAOPass::Setup(RenderGraph& graph) {
    auto prez_depth = graph.DeclareResource("prez_depth");
    auto scene_color = graph.DeclareResource("scene_color");
    auto ssao_color = graph.DeclareResource("ssao_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, prez_depth);
    graph.PassRead(pass, scene_color);
    graph.PassWrite(pass, ssao_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void SSAOPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& pp_config = snap.post_process;
    bool ssao_enabled = ctx_.pipeline_features.ssao && pp_config.valid && pp_config.ssao_enabled;

    if (!ssao_enabled || ctx_.render_targets.ssao == 0 || ctx_.render_targets.ssao_blur == 0) {
        return;
    }

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    float near_plane = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    float far_plane  = snap.camera_3d.valid ? snap.camera_3d.far_clip  : 10000.0f;

    post_process_renderer_.BeginFrame();

    // Pass 1: SSAO è®¡ç®—ï¼ˆåŠåˆ†è¾¨çŽ‡ï¼‰
    cmd_buffer.BeginRenderPass({ctx_.render_targets.ssao, glm::vec4(1.0f), true});
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"ssao", depth_tex, {
        pp_config.ssao_radius,
        pp_config.ssao_bias,
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height()),
        static_cast<float>(pp_config.ssao_sample_count),
        pp_config.ssao_power,
        pp_config.ssao_intensity
    }});
    cmd_buffer.EndRenderPass();

    // Pass 2: åŒè¾¹æ¨¡ç³Š
    const unsigned int ssao_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.ssao);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.ssao_blur, glm::vec4(1.0f), true});
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"ssao_blur", ssao_tex});
    cmd_buffer.EndRenderPass();
}

// ============================================================
// ContactShadowPass
// ============================================================

void ContactShadowPass::Setup(RenderGraph& graph) {
    auto prez_depth = graph.DeclareResource("prez_depth");
    auto contact_shadow = graph.DeclareResource("contact_shadow");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, prez_depth);
    graph.PassWrite(pass, contact_shadow);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void ContactShadowPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& pp_config = snap.post_process;
    bool cs_enabled = ctx_.pipeline_features.contact_shadow && pp_config.valid && pp_config.contact_shadow_enabled;

    if (!cs_enabled || ctx_.render_targets.contact_shadow == 0) {
        return;
    }

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    glm::vec3 light_dir(-0.4f, -1.0f, -0.3f);
    if (snap.directional_light.valid) {
        light_dir = glm::normalize(snap.directional_light.direction);
    }

    float near_plane = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    float far_plane  = snap.camera_3d.valid ? snap.camera_3d.far_clip  : 10000.0f;

    cmd_buffer.BeginRenderPass({ctx_.render_targets.contact_shadow, glm::vec4(1.0f), true});
    post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"contact_shadow", depth_tex, {
        light_dir.x, light_dir.y, light_dir.z,
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height()),
        pp_config.contact_shadow_strength,
        static_cast<float>(pp_config.contact_shadow_steps),
        pp_config.contact_shadow_step_size
    }});
    cmd_buffer.EndRenderPass();
}

// ============================================================
// FXAAPass
// ============================================================

void FXAAPass::Setup(RenderGraph& graph) {
    auto main_color  = graph.DeclareResource("main_color");
    auto fxaa_color  = graph.DeclareResource("fxaa_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, main_color);
    graph.PassWrite(pass, fxaa_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void FXAAPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    bool fxaa_enabled = ctx_.pipeline_features.fxaa && snap.post_process.valid && snap.post_process.fxaa_enabled;

    ctx_.fxaa_active = fxaa_enabled && ctx_.render_targets.fxaa != 0;
    if (!ctx_.fxaa_active) {
        return;
    }

    const unsigned int main_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.main);
    if (main_color_tex == 0) return;

    cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.fxaa, glm::vec4(0.0f), true});
    post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device,
        {"fxaa", main_color_tex, {
            static_cast<float>(Screen::width()),
            static_cast<float>(Screen::height())
        }});
    cmd_buffer.EndRenderPass();
}

// ============================================================
// PresentPass
// ============================================================

void PresentPass::Setup(RenderGraph& graph) {
    auto main_color = graph.DeclareResource("main_color");
    auto fxaa_color = graph.DeclareResource("fxaa_color");
    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, main_color);
    graph.PassRead(pass, fxaa_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void PresentPass::Execute(CommandBuffer& cmd_buffer) {
    unsigned int present_tex = 0;
    if (ctx_.taa_active) {
        present_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.taa);
    } else if (ctx_.fxaa_active) {
        present_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.fxaa);
    }
    if (present_tex == 0) {
        present_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.main);
    }
    if (present_tex == 0) {
        return;
    }
    cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({0, glm::vec4(0.0f), true});
    post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"copy", present_tex});
    cmd_buffer.EndRenderPass();
}

// ============================================================
// TAAPass
// ============================================================

float TAAPass::Halton(int index, int base) {
    float result = 0.0f;
    float f = 1.0f;
    int i = index;
    while (i > 0) {
        f /= static_cast<float>(base);
        result += f * static_cast<float>(i % base);
        i = i / base;
    }
    return result;
}

void TAAPass::UpdateJitter(int frame_index) {
    frame_index_ = frame_index;
    int seq = (frame_index % 16) + 1;
    float jx = Halton(seq, 2) - 0.5f;
    float jy = Halton(seq, 3) - 0.5f;
    const int sw = Screen::width();
    const int sh = Screen::height();
    current_jitter_.x = sw > 0 ? (jx / static_cast<float>(sw)) : 0.0f;
    current_jitter_.y = sh > 0 ? (jy / static_cast<float>(sh)) : 0.0f;
}

void TAAPass::Setup(RenderGraph& graph) {
    auto main_color = graph.DeclareResource("main_color");
    auto taa_color  = graph.DeclareResource("taa_color");
    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, main_color);
    graph.PassWrite(pass, taa_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void TAAPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    bool taa_enabled = ctx_.pipeline_features.taa && snap.post_process.valid && snap.post_process.taa_enabled;
    float blend_factor = snap.post_process.valid ? snap.post_process.taa_blend_factor : 0.1f;

    ctx_.taa_active = taa_enabled && ctx_.render_targets.taa != 0;
    if (!ctx_.taa_active) {
        return;
    }

    const int sw = Screen::width();
    const int sh = Screen::height();
    EnsureHistoryRT(sw, sh);

    const unsigned int main_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.main);
    if (main_color_tex == 0) return;

    // è¯»å– motion vector çº¹ç†ï¼ˆå¦‚æžœå¯ç”¨ï¼‰
    const unsigned int mv_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.motion_vector);

    // åŽ†å²å¸§è¯»å–æ¥è‡ªä¸Šä¸€å¸§å†™å…¥çš„ RT
    const int read_idx = 1 - history_index_;
    const unsigned int history_tex = has_valid_history_
        ? ctx_.rhi_device->GetRenderTargetColorTexture(history_rt_[read_idx])
        : 0;

    // TAA resolveï¼šå†™å…¥å½“å‰å¸§çš„ history RTï¼ˆç›´æŽ¥åšè¾“å‡ºï¼ŒçœæŽ‰ copyï¼‰
    const int write_idx = history_index_;
    post_process_renderer_.BeginFrame();
    cmd_buffer.BeginRenderPass({history_rt_[write_idx], glm::vec4(0.0f), true});
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, PostProcessRequest{"taa_resolve", main_color_tex, {
        blend_factor,
        current_jitter_.x,
        current_jitter_.y,
        static_cast<float>(frame_index_),
        static_cast<float>(sw),
        static_cast<float>(sh)
    }}.Tex(2, mv_tex).Tex(5, history_tex));
    cmd_buffer.EndRenderPass();

    // å°† TAA ç»“æžœ copy åˆ° taa RTï¼ˆä¾› Present/FXAA è¯»å–ï¼‰
    const unsigned int taa_out_tex = ctx_.rhi_device->GetRenderTargetColorTexture(history_rt_[write_idx]);
    if (taa_out_tex != 0 && ctx_.render_targets.taa != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.taa, glm::vec4(0.0f), true});
        post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"copy", taa_out_tex});
        cmd_buffer.EndRenderPass();
    }

    // ç¿»è½¬ ping-pong ç´¢å¼•
    history_index_ = 1 - history_index_;
    has_valid_history_ = true;
}

void TAAPass::EnsureHistoryRT(int width, int height) {
    if (width == history_width_ && height == history_height_
        && history_rt_[0] != 0 && history_rt_[1] != 0) {
        return;
    }
    // åˆ†è¾¨çŽ‡å˜åŒ–æˆ–é¦–æ¬¡åˆ›å»ºï¼ˆæ—§ RT ç”± RhiDevice èµ„æºç®¡ç†å™¨ç»Ÿä¸€å›žæ”¶ï¼‰
    for (int i = 0; i < 2; ++i) {
        RenderTargetDesc desc;
        desc.width = width;
        desc.height = height;
        desc.has_color = true;
        desc.has_depth = false;
        history_rt_[i] = ctx_.rhi_device->CreateRenderTarget(desc);
    }
    history_width_ = width;
    history_height_ = height;
    has_valid_history_ = false;
    history_index_ = 0;
}

// ============================================================
// DOFPass
// ============================================================

void DOFPass::Setup(RenderGraph& graph) {
    auto main_color  = graph.DeclareResource("main_color");
    auto prez_depth  = graph.DeclareResource("prez_depth");
    auto dof_color   = graph.DeclareResource("dof_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, main_color);
    graph.PassRead(pass, prez_depth);
    graph.PassWrite(pass, dof_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void DOFPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& pp_config = snap.post_process;
    bool dof_enabled = pp_config.valid && pp_config.dof_enabled;

    if (!dof_enabled || ctx_.render_targets.dof == 0) return;

    const unsigned int main_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.main);
    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (main_color_tex == 0 || depth_tex == 0) return;

    float near_plane = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    float far_plane  = snap.camera_3d.valid ? snap.camera_3d.far_clip  : 10000.0f;

    // Pass 1: DOF â†’ dof RT
    post_process_renderer_.BeginFrame();
    cmd_buffer.BeginRenderPass({ctx_.render_targets.dof, glm::vec4(0.0f), true});
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, PostProcessRequest{"dof", depth_tex, {
        pp_config.dof_focus_distance,
        pp_config.dof_focus_range,
        pp_config.dof_bokeh_radius,
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height())
    }}.Tex(2, main_color_tex));
    cmd_buffer.EndRenderPass();

    // Pass 2: dof RT â†’ main RTï¼ˆå›žå†™ï¼‰
    const unsigned int dof_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.dof);
    if (dof_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.main, glm::vec4(0.0f), true});
        post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"copy", dof_tex});
        cmd_buffer.EndRenderPass();
    }
}

// ============================================================
// MotionVectorPass
// ============================================================

void MotionVectorPass::Setup(RenderGraph& graph) {
    auto prez_depth    = graph.DeclareResource("prez_depth");
    auto mv_color      = graph.DeclareResource("motion_vector_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, prez_depth);
    graph.PassWrite(pass, mv_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void MotionVectorPass::Execute(CommandBuffer& cmd_buffer) {
    if (ctx_.render_targets.motion_vector == 0) return;

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    const auto& snap = *ctx_.snapshot;
    glm::mat4 current_vp = glm::mat4(1.0f);
    if (snap.camera_3d.valid) {
        const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        glm::mat4 projection = clip_correction * glm::perspective(glm::radians(snap.camera_3d.fov),
            static_cast<float>(Screen::width()) / static_cast<float>(Screen::height()),
            snap.camera_3d.near_clip, snap.camera_3d.far_clip);
        current_vp = projection * snap.camera_3d.view;
    }

    if (!has_prev_vp_) {
        prev_vp_ = current_vp;
        has_prev_vp_ = true;
        // é¦–å¸§è¾“å‡ºé›¶é€Ÿåº¦
        cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);
        cmd_buffer.BeginRenderPass({ctx_.render_targets.motion_vector, glm::vec4(0.0f), true});
        cmd_buffer.EndRenderPass();
        return;
    }

    glm::mat4 reproj = prev_vp_ * glm::inverse(current_vp);
    const float* reproj_ptr = &reproj[0][0];

    std::vector<float> params;
    params.reserve(20);
    params.push_back(static_cast<float>(Screen::width()));
    params.push_back(static_cast<float>(Screen::height()));
    // [2..17]: reproj matrix
    for (int i = 0; i < 16; ++i) params.push_back(reproj_ptr[i]);

    cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.motion_vector, glm::vec4(0.0f), true});
    post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"motion_vector", depth_tex, params});
    cmd_buffer.EndRenderPass();

    prev_vp_ = current_vp;
}

// ============================================================
// MotionBlurPass
// ============================================================

void MotionBlurPass::Setup(RenderGraph& graph) {
    auto main_color  = graph.DeclareResource("main_color");
    auto mv_color    = graph.DeclareResource("motion_vector_color");
    auto mb_color    = graph.DeclareResource("motion_blur_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, main_color);
    graph.PassRead(pass, mv_color);
    graph.PassWrite(pass, mb_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void MotionBlurPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& pp_config = snap.post_process;
    bool mb_enabled = pp_config.valid && pp_config.motion_blur_enabled;

    if (!mb_enabled || ctx_.render_targets.dof == 0) return;

    const unsigned int main_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.main);
    const unsigned int mv_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.motion_vector);
    if (main_color_tex == 0 || mv_tex == 0) return;

    // motion_blur çŽ°åœ¨è¯» motion_vector RT è€Œéžæ·±åº¦ + reproj
    // params: [0] intensity, [1] samples, [2] screen_w, [3] screen_h, [4] color_tex
    post_process_renderer_.BeginFrame();
    cmd_buffer.BeginRenderPass({ctx_.render_targets.dof, glm::vec4(0.0f), true});
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, PostProcessRequest{"motion_blur", mv_tex, {
        pp_config.motion_blur_intensity,
        static_cast<float>(pp_config.motion_blur_samples),
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height())
    }}.Tex(2, main_color_tex));
    cmd_buffer.EndRenderPass();

    // dof RT â†’ main RT
    const unsigned int mb_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.dof);
    if (mb_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.main, glm::vec4(0.0f), true});
        post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"copy", mb_tex});
        cmd_buffer.EndRenderPass();
    }
}

// ============================================================
// SSRPass
// ============================================================

void SSRPass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto prez_depth  = graph.DeclareResource("prez_depth");
    auto ssr_color   = graph.DeclareResource("ssr_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, prez_depth);
    graph.PassWrite(pass, ssr_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void SSRPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& pp_config = snap.post_process;
    bool ssr_enabled = ctx_.pipeline_features.ssr && pp_config.valid && pp_config.ssr_enabled;

    if (!ssr_enabled || ctx_.render_targets.ssr == 0) return;

    const unsigned int scene_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);
    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (scene_color_tex == 0 || depth_tex == 0) return;

    float near_plane = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    float far_plane  = snap.camera_3d.valid ? snap.camera_3d.far_clip  : 10000.0f;

    // Pass 1: æ¸²æŸ“ SSR åˆ°åŠåˆ†è¾¨çŽ‡ ssr RT
    post_process_renderer_.BeginFrame();
    cmd_buffer.BeginRenderPass({ctx_.render_targets.ssr, glm::vec4(0.0f), true});
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, PostProcessRequest{"ssr", depth_tex, {
        pp_config.ssr_max_distance,
        pp_config.ssr_thickness,
        pp_config.ssr_step_size,
        static_cast<float>(pp_config.ssr_max_steps),
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height()),
        pp_config.ssr_fade_distance,
        pp_config.ssr_max_roughness
    }}.Tex(2, scene_color_tex));
    cmd_buffer.EndRenderPass();

    // Pass 2: å°† SSR ç»“æžœå åŠ åˆ° scene RTï¼ˆåˆ©ç”¨ SSR alpha ä½œä¸ºæ··åˆæƒé‡ï¼‰
    const unsigned int ssr_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.ssr);
    if (ssr_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
        post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"ui_overlay", ssr_tex, {}, true});
        cmd_buffer.EndRenderPass();
    }
}

// ============================================================
// OutlinePass â€” å±å¹•ç©ºé—´è¾¹ç¼˜æ£€æµ‹æè¾¹
// ============================================================

void OutlinePass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto prez_depth  = graph.DeclareResource("prez_depth");
    auto outline_color = graph.DeclareResource("outline_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, prez_depth);
    graph.PassWrite(pass, outline_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void OutlinePass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& pp = snap.post_process;

    if (!pp.valid || !pp.outline_enabled || ctx_.render_targets.outline == 0) return;

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    float near_plane = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    float far_plane  = snap.camera_3d.valid ? snap.camera_3d.far_clip  : 1000.0f;

    // Pass 1: è¾¹ç¼˜æ£€æµ‹ â†’ outline RT
    cmd_buffer.BeginRenderPass({ctx_.render_targets.outline, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), true});
    post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"edge_detect", depth_tex, {
        pp.outline_thickness,
        pp.outline_depth_threshold,
        pp.outline_normal_threshold,
        pp.outline_color.r,
        pp.outline_color.g,
        pp.outline_color.b,
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height())
    }});
    cmd_buffer.EndRenderPass();

    // Pass 2: å°†è¾¹ç¼˜ç»“æžœå åŠ åˆ° scene RT
    const unsigned int outline_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.outline);
    if (outline_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
        post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"ui_overlay", outline_tex, {}, true});
        cmd_buffer.EndRenderPass();
    }
}

// ============================================================
// LightShaftPass â€” screen-space radial blur (God Ray)
// ============================================================

void LightShaftPass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto prez_depth  = graph.DeclareResource("prez_depth");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, prez_depth);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void LightShaftPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& pp_snap = snap.post_process;
    if (!pp_snap.valid || !pp_snap.light_shaft_enabled) return;

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    glm::vec3 cam_fwd = snap.camera_3d.forward;
    glm::vec3 cam_right = snap.camera_3d.right;
    glm::vec3 cam_up = snap.camera_3d.up;
    float fov_y = snap.camera_3d.valid ? snap.camera_3d.fov : 60.0f;
    float aspect = static_cast<float>(Screen::width()) / static_cast<float>(Screen::height());
    const float tan_fov_y = std::tan(glm::radians(fov_y) * 0.5f);

    glm::vec3 sun_dir{0.0f, -1.0f, 0.0f};
    if (snap.directional_light.valid) {
        sun_dir = glm::normalize(snap.directional_light.direction);
    }
    const auto* pp = &pp_snap;

    glm::vec3 to_sun = -sun_dir;
    float d_fwd = glm::dot(to_sun, cam_fwd);
    if (d_fwd <= 0.01f) return;

    float d_right = glm::dot(to_sun, cam_right);
    float d_up    = glm::dot(to_sun, cam_up);
    float sun_uv_x = (d_right / (d_fwd * tan_fov_y * aspect)) * 0.5f + 0.5f;
    float sun_uv_y = (d_up / (d_fwd * tan_fov_y)) * 0.5f + 0.5f;

    const unsigned int scene_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);

    // params å¸ƒå±€ï¼ˆ15 floatï¼‰:
    // [0-1]  sun_screen_pos.xy (UV space)
    // [2-4]  light_color.rgb
    // [5]    density
    // [6]    weight
    // [7]    decay
    // [8]    exposure
    // [9]    num_samples
    // [10]   intensity
    // [11-14] reserved (pad)
    cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
    // å·²è¿åˆ° PostProcessRendererï¼šscreenTexture set=2,b1 / u_depth_tex set=2,b2 /
    // å‚æ•° std140 set=2,b0ï¼ˆ15 æ ‡é‡ï¼ŒåŽ»é™¤æ—§ vestigial u_depth_handle å­—æ®µï¼‰ã€‚
    PostProcessRequest req{"light_shaft", scene_tex, {
        sun_uv_x, sun_uv_y,
        pp->light_shaft_color.r, pp->light_shaft_color.g, pp->light_shaft_color.b,
        pp->light_shaft_density,
        pp->light_shaft_weight,
        pp->light_shaft_decay,
        pp->light_shaft_exposure,
        static_cast<float>(pp->light_shaft_samples),
        pp->light_shaft_intensity,
        0.0f, 0.0f, 0.0f, 0.0f
    }, false, 0};
    req.Tex(2, depth_tex);
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, req);
    cmd_buffer.EndRenderPass();
}

// ============================================================
// VolumetricFogPass â€” é«˜åº¦æŒ‡æ•°é›¾ + Mie æ•£å°„è¿‘ä¼¼ raymarching
// ============================================================

} // namespace render
} // namespace dse
