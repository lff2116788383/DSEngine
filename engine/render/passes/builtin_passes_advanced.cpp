/**
 * @file builtin_passes_advanced.cpp
 * @brief Advanced render passes: VolumetricFog, VolumetricCloud, WBOIT, Water,
 *        Decal, HiZBuild, HiZCull, GPUCull, RSM, DDGI, SSSBlur, Weather.
 */

#include "engine/render/passes/builtin_passes.h"
#include "engine/render/passes/builtin_passes_internal.h"
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

using namespace dse::render::pass_internal;
void VolumetricFogPass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto prez_depth  = graph.DeclareResource("prez_depth");
    auto fog_color   = graph.DeclareResource("fog_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, prez_depth);
    graph.PassWrite(pass, fog_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void VolumetricFogPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& pp_snap = snap.post_process;
    if (!pp_snap.valid || !pp_snap.fog_enabled || ctx_.render_targets.fog == 0) return;

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    float near_p = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    float far_p  = snap.camera_3d.valid ? snap.camera_3d.far_clip  : 1000.0f;
    float fov_y  = snap.camera_3d.valid ? snap.camera_3d.fov       : 60.0f;
    float aspect = static_cast<float>(Screen::width()) / static_cast<float>(Screen::height());
    // Camera-Relative: cam_pos ä¸º vec3(0)ï¼ˆç›¸æœºåœ¨åŽŸç‚¹ï¼‰
    glm::vec3 cam_pos   = glm::vec3(0.0f);
    glm::vec3 cam_right = snap.camera_3d.right;
    glm::vec3 cam_up    = snap.camera_3d.up;
    glm::vec3 cam_fwd   = snap.camera_3d.forward;
    const float tan_fov_y = std::tan(glm::radians(fov_y) * 0.5f);

    glm::vec3 sun_dir{0.0f, -1.0f, 0.0f};
    if (snap.directional_light.valid) {
        sun_dir = glm::normalize(snap.directional_light.direction);
    }
    const auto* pp = &pp_snap;

    const unsigned int scene_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);

    // params å¸ƒå±€ï¼ˆ30 ä¸ª floatï¼Œä¸‰åŽç«¯é€šç”¨ï¼‰ï¼š
    // [0]      depth_tex handle
    // [1-3]    fog_color.rgb
    // [4]      fog_density
    // [5]      height_falloff
    // [6]      height_offset
    // [7]      fog_start
    // [8]      fog_end
    // [9]      fog_steps
    // [10]     sun_scatter
    // [11-13]  sun_dir.xyz
    // [14-16]  camera_pos.xyz
    // [17]     near
    // [18]     far
    // [19-21]  cam_right.xyz
    // [22-24]  cam_up.xyz
    // [25-27]  cam_fwd.xyz
    // [28]     tan_fov_y
    // [29]     aspect
    cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.fog, glm::vec4(0.0f), true});
    post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, PostProcessRequest{"volumetric_fog", scene_tex, {
        pp->fog_color.r, pp->fog_color.g, pp->fog_color.b,
        pp->fog_density, pp->fog_height_falloff, pp->fog_height_offset - ctx_.camera_offset.y,
        pp->fog_start, pp->fog_end,
        static_cast<float>(pp->fog_steps),
        pp->fog_sun_scatter,
        sun_dir.x, sun_dir.y, sun_dir.z,
        cam_pos.x, cam_pos.y, cam_pos.z,
        near_p, far_p,
        cam_right.x, cam_right.y, cam_right.z,
        cam_up.x, cam_up.y, cam_up.z,
        cam_fwd.x, cam_fwd.y, cam_fwd.z,
        tan_fov_y, aspect
    }}.Tex(2, depth_tex));
    cmd_buffer.EndRenderPass();

    // å°†é›¾æ•ˆç»“æžœï¼ˆå·²åŒ…å« scene é¢œè‰²ï¼‰è¦†å†™å›ž scene RT
    const unsigned int fog_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.fog);
    if (fog_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
        post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"copy", fog_tex});
        cmd_buffer.EndRenderPass();
    }
}

// ============================================================
// VolumetricCloudPass â€” Guerrilla-style raymarching volumetric clouds
// ============================================================

void VolumetricCloudPass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto prez_depth  = graph.DeclareResource("prez_depth");
    auto cloud_color = graph.DeclareResource("cloud_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, prez_depth);
    graph.PassWrite(pass, cloud_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void VolumetricCloudPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& vc = snap.volumetric_cloud;
    if (!vc.valid || ctx_.render_targets.cloud == 0) return;

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    float near_p = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    float far_p  = snap.camera_3d.valid ? snap.camera_3d.far_clip  : 1000.0f;
    float fov_y  = snap.camera_3d.valid ? snap.camera_3d.fov       : 60.0f;
    float aspect = static_cast<float>(Screen::width()) / static_cast<float>(Screen::height());
    glm::vec3 cam_pos   = glm::vec3(0.0f); // Camera-Relative: cam at origin
    glm::vec3 cam_right = snap.camera_3d.right;
    glm::vec3 cam_up    = snap.camera_3d.up;
    glm::vec3 cam_fwd   = snap.camera_3d.forward;
    const float tan_fov_y = std::tan(glm::radians(fov_y) * 0.5f);

    // Pre-scale right/up by tan_fov for ray reconstruction in shader
    glm::vec3 right_scaled = cam_right * tan_fov_y * aspect;
    glm::vec3 up_scaled    = cam_up * tan_fov_y;

    glm::vec3 sun_dir{0.0f, -1.0f, 0.0f};
    if (snap.directional_light.valid) {
        sun_dir = glm::normalize(snap.directional_light.direction);
    }

    // Cloud bottom/top in camera-relative space
    float cloud_bottom_rel = vc.cloud_bottom - ctx_.camera_offset.y;
    float cloud_top_rel    = vc.cloud_top    - ctx_.camera_offset.y;

    const unsigned int scene_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);

    // half_resolution: render to dedicated cloud RT then upscale-copy back
    // full resolution: render directly into scene RT (in-place, no copy needed)
    const bool use_half_res = vc.half_resolution && (ctx_.render_targets.cloud != 0);
    const unsigned int target_rt = use_half_res ? ctx_.render_targets.cloud : ctx_.render_targets.scene;

    // params å¸ƒå±€ï¼ˆ30 ä¸ª floatï¼Œä¸‰åŽç«¯é€šç”¨ï¼‰ï¼š
    cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({target_rt, glm::vec4(0.0f), use_half_res});
    post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, PostProcessRequest{"volumetric_cloud", scene_tex, {
        cloud_bottom_rel, cloud_top_rel,
        vc.coverage, vc.density,
        vc.shape_scale, vc.detail_scale, vc.detail_strength, vc.erosion,
        vc.wind_offset_x, vc.wind_offset_z,
        vc.silver_intensity, vc.powder_strength, vc.ambient_strength,
        sun_dir.x, sun_dir.y, sun_dir.z,
        cam_pos.x, cam_pos.y, cam_pos.z,
        near_p, far_p,
        right_scaled.x, right_scaled.y, right_scaled.z,
        up_scaled.x, up_scaled.y, up_scaled.z,
        cam_fwd.x, cam_fwd.y, cam_fwd.z
    }}.Tex(2, depth_tex));
    cmd_buffer.EndRenderPass();

    // Half-res: upscale-copy cloud result back to scene RT
    if (use_half_res) {
        const unsigned int cloud_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.cloud);
        if (cloud_tex != 0) {
            cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
            post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"copy", cloud_tex});
            cmd_buffer.EndRenderPass();
        }
    }
}

// ============================================================
// WBOITPass â€” Weighted Blended Order-Independent Transparency
// ============================================================

void WBOITPass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto prez_depth  = graph.DeclareResource("prez_depth");
    auto wboit_accum = graph.DeclareResource("wboit_accum");
    auto wboit_reveal = graph.DeclareResource("wboit_reveal");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, prez_depth);
    graph.PassWrite(pass, wboit_accum);
    graph.PassWrite(pass, wboit_reveal);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void WBOITPass::Execute(CommandBuffer& cmd_buffer) {
    if (ctx_.render_targets.wboit_accum == 0 || ctx_.render_targets.wboit_reveal == 0) return;

    const glm::mat4 scene_clip_correction = ctx_.rhi_device->GetProjectionCorrection();

    // --- Pass 1: Accumulation (blend ONE, ONE) ---
    cmd_buffer.BindPipeline(ctx_.pipeline_states.wboit_accum);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.wboit_accum, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), true});

    RenderScenePassContext accum_ctx;
    accum_ctx.world = ctx_.world;
    accum_ctx.clip_correction = &scene_clip_correction;
    if (ctx_.render_scene) {
        ctx_.render_scene->DrawTransparent(cmd_buffer, 1, *ctx_.rhi_device, *ctx_.mesh_renderer, ctx_.frame_camera);
    }
    cmd_buffer.EndRenderPass();

    // --- Pass 2: Revealage (blend ZERO, ONE_MINUS_SRC_ALPHA) ---
    cmd_buffer.BindPipeline(ctx_.pipeline_states.wboit_reveal);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.wboit_reveal, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), true});

    RenderScenePassContext reveal_ctx;
    reveal_ctx.world = ctx_.world;
    reveal_ctx.clip_correction = &scene_clip_correction;
    if (ctx_.render_scene) {
        ctx_.render_scene->DrawTransparent(cmd_buffer, 2, *ctx_.rhi_device, *ctx_.mesh_renderer, ctx_.frame_camera);
    }
    cmd_buffer.EndRenderPass();

    // --- Pass 3: Composite WBOIT onto scene RT ---
    const unsigned int accum_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.wboit_accum);
    const unsigned int reveal_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.wboit_reveal);
    if (accum_tex == 0 || reveal_tex == 0) return;

    cmd_buffer.BindPipeline(ctx_.pipeline_states.decal_blend);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
    post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device,
        PostProcessRequest{"wboit_composite", accum_tex, {}, true}.Tex(2, reveal_tex));
    cmd_buffer.EndRenderPass();
}

// ============================================================
// WaterPass â€” Screen-Space Water / Ocean (Gerstner wave + refraction)
// ============================================================

void WaterPass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto prez_depth  = graph.DeclareResource("prez_depth");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, prez_depth);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void WaterPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    if (snap.water_count == 0) return;

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;
    const unsigned int scene_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);

    // Camera-Relative: cam_pos åœ¨ç€è‰²å™¨ä¸­åº”ä¸º vec3(0)ï¼ˆç›¸æœºåœ¨åŽŸç‚¹ï¼‰
    glm::vec3 cam_pos = glm::vec3(0.0f);
    float cam_fov  = snap.camera_3d.valid ? snap.camera_3d.fov       : 60.0f;
    float cam_near = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    float cam_far  = snap.camera_3d.valid ? snap.camera_3d.far_clip  : 1000.0f;
    glm::vec3 cam_fwd = snap.camera_3d.forward;

    if (ctx_.editor_mode && ctx_.use_editor_camera) {
        cam_pos = glm::vec3(glm::inverse(ctx_.editor_view)[3]) - ctx_.camera_offset;
        glm::mat4 inv_view = glm::inverse(ctx_.editor_view);
        cam_fwd = -glm::normalize(glm::vec3(inv_view[2]));
    } else if (!snap.camera_3d.valid) {
        return;
    }

    float aspect = static_cast<float>(Screen::width()) / static_cast<float>(std::max(1, Screen::height()));
    float tan_fov_y = std::tan(glm::radians(cam_fov) * 0.5f);

    glm::vec3 sun_dir(0.0f, -1.0f, 0.0f);
    if (snap.directional_light.valid) {
        sun_dir = glm::normalize(snap.directional_light.direction);
    }

    const float current_time = Time::TimeSinceStartup();

    cmd_buffer.BindPipeline(ctx_.pipeline_states.decal_blend);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});

    // params å¸ƒå±€ï¼ˆ40 float = 160 bytesï¼‰
    std::vector<float> params(39);

    for (int wi = 0; wi < snap.water_count; ++wi) {
        const auto& wc = snap.waters[wi];

        glm::vec2 wave_dir = glm::length(wc.wave_direction) > 0.001f
            ? glm::normalize(wc.wave_direction) : glm::vec2(1.0f, 0.0f);

        params[0]  = wc.water_level - ctx_.camera_offset.y;
        params[1]  = wc.deep_color.r;    params[2]  = wc.deep_color.g;    params[3]  = wc.deep_color.b;
        params[4]  = wc.shallow_color.r;  params[5]  = wc.shallow_color.g;  params[6]  = wc.shallow_color.b;
        params[7]  = wc.max_depth;
        params[8]  = wc.transparency;
        params[9]  = wc.wave_amplitude;   params[10] = wc.wave_frequency;   params[11] = wc.wave_speed;
        params[12] = wave_dir.x;          params[13] = wave_dir.y;
        params[14] = wc.refraction_strength;
        params[15] = wc.specular_power;
        params[16] = wc.reflection_strength;
        params[17] = current_time;
        params[18] = sun_dir.x;           params[19] = sun_dir.y;           params[20] = sun_dir.z;
        params[21] = cam_pos.x;           params[22] = cam_pos.y;           params[23] = cam_pos.z;
        params[24] = cam_near;            params[25] = cam_far;
        params[26] = cam_fwd.x;           params[27] = cam_fwd.y;           params[28] = cam_fwd.z;
        params[29] = tan_fov_y;           params[30] = aspect;
        // è§†è§‰å¢žå¼ºå‚æ•°
        params[31] = wc.caustic_intensity;    params[32] = wc.caustic_scale;
        params[33] = wc.foam_intensity;       params[34] = wc.foam_depth_threshold;
        params[35] = wc.underwater_fog_density;
        params[36] = wc.underwater_fog_color.r; params[37] = wc.underwater_fog_color.g; params[38] = wc.underwater_fog_color.b;

        post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device,
            PostProcessRequest{"water", scene_tex, params, true}.Tex(2, depth_tex));
    }
    cmd_buffer.EndRenderPass();
}

// ============================================================
// DecalPass â€” Screen-Space Decal (æ·±åº¦é‡å»º + ç›’ä½“æŠ•å½±)
// ============================================================

void DecalPass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto prez_depth  = graph.DeclareResource("prez_depth");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, prez_depth);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void DecalPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    if (snap.decal_count == 0) return;

    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
    glm::mat4 view_mat = snap.camera_3d.view;
    glm::mat4 proj_mat(1.0f);
    if (snap.camera_3d.valid) {
        float aspect = static_cast<float>(Screen::width()) / static_cast<float>(Screen::height());
        proj_mat = clip_correction * glm::perspective(glm::radians(snap.camera_3d.fov), aspect, snap.camera_3d.near_clip, snap.camera_3d.far_clip);
    }
    const glm::mat4 inv_vp = glm::inverse(proj_mat * view_mat);

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    const unsigned int scene_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);

    cmd_buffer.BindPipeline(ctx_.pipeline_states.decal_blend);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});

    std::vector<float> params(24);

    for (int di = 0; di < snap.decal_count; ++di) {
        const auto& dc = snap.decals[di];

        glm::mat4 model = glm::translate(glm::mat4(1.0f), dc.position - ctx_.camera_offset)
                        * glm::mat4_cast(dc.rotation)
                        * glm::scale(glm::mat4(1.0f), dc.scale);
        glm::mat4 inv_model_vp = glm::inverse(model) * inv_vp;
        glm::vec3 decal_up = glm::normalize(glm::vec3(model[1]));

        const float* m = &inv_model_vp[0][0];
        for (int i = 0; i < 16; ++i) params[i] = m[i];
        params[16] = dc.color.r;
        params[17] = dc.color.g;
        params[18] = dc.color.b;
        params[19] = dc.color.a;
        params[20] = dc.angle_fade;
        params[21] = decal_up.x;
        params[22] = decal_up.y;
        params[23] = decal_up.z;

        post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device,
            PostProcessRequest{"decal", scene_tex, params, true}
            .Tex(2, depth_tex).Tex(3, dc.albedo_texture));
    }
    cmd_buffer.EndRenderPass();
}

// ============================================================
// HiZBuildPass â€” ä»Ž PreZ æ·±åº¦æž„å»º Hi-Z Mip Chain (Compute Shader)
// ============================================================


void HiZBuildPass::EnsureShaders() {
    if (shaders_compiled_) return;
    shaders_compiled_ = true;

    // ä½¿ç”¨ FramePipeline ç¼“å­˜çš„ shader å¥æŸ„ï¼Œé¿å…æ¯å¸§é‡å»ºæ³„æ¼
    hiz_copy_shader_ = ctx_.hiz_copy_shader;
    hiz_downsample_shader_ = ctx_.hiz_downsample_shader;
}

void HiZBuildPass::Setup(RenderGraph& graph) {
    auto prez_depth = graph.DeclareResource("prez_depth");
    auto hiz_mip = graph.DeclareResource("hiz_mip_chain");
    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, prez_depth);
    graph.PassWrite(pass, hiz_mip);
    graph.MarkOutput(hiz_mip);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void HiZBuildPass::Execute(CommandBuffer& /*cmd_buffer*/) {
    EnsureShaders();
    if (hiz_copy_shader_ == 0 || hiz_downsample_shader_ == 0) return;
    if (ctx_.render_targets.hiz_texture == 0 || ctx_.render_targets.prez == 0) return;

    auto* rhi = ctx_.rhi_device;
    if (!rhi) return;

    const unsigned int hiz_gpu_tex = rhi->GetHiZGpuTexture(ctx_.render_targets.hiz_texture);
    if (hiz_gpu_tex == 0) return;

    const int mip_count = rhi->GetHiZMipCount(ctx_.render_targets.hiz_texture);
    if (mip_count <= 0) return;

    const int base_w = Screen::width();
    const int base_h = Screen::height();

    // Step 1: Copy PreZ depth â†’ Hi-Z mip 0
    {
        unsigned int depth_tex = rhi->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
        if (depth_tex == 0) return;

        rhi->SetComputeTextureSampler(0, depth_tex);
        rhi->SetComputeTextureImageMip(0, hiz_gpu_tex, 0, false, true);

        rhi->SetComputeUniformVec2i(hiz_copy_shader_, "u_dst_size", base_w, base_h);

        unsigned int groups_x = (base_w + 15) / 16;
        unsigned int groups_y = (base_h + 15) / 16;
        rhi->DispatchCompute(hiz_copy_shader_, groups_x, groups_y, 1);
        rhi->ComputeMemoryBarrier();
    }

    // Step 2: Iterative downsample mip N-1 â†’ mip N
    for (int mip = 1; mip < mip_count; ++mip) {
        int src_w = std::max(1, base_w >> (mip - 1));
        int src_h = std::max(1, base_h >> (mip - 1));
        int dst_w = std::max(1, base_w >> mip);
        int dst_h = std::max(1, base_h >> mip);

        rhi->SetComputeTextureImageMip(0, hiz_gpu_tex, mip - 1, true, true);
        rhi->SetComputeTextureImageMip(1, hiz_gpu_tex, mip, false, true);

        rhi->SetComputeUniformVec2i(hiz_downsample_shader_, "u_src_size", src_w, src_h);
        rhi->SetComputeUniformVec2i(hiz_downsample_shader_, "u_dst_size", dst_w, dst_h);

        unsigned int groups_x = (dst_w + 15) / 16;
        unsigned int groups_y = (dst_h + 15) / 16;
        rhi->DispatchCompute(hiz_downsample_shader_, groups_x, groups_y, 1);
        rhi->ComputeMemoryBarrier();
    }

    ctx_.hiz_culling_enabled = true;
}

// ============================================================
// HiZCullPass â€” GPU-driven é®æŒ¡å‰”é™¤ (Compute Shader)
// ============================================================

void HiZCullPass::EnsureShader() {
    if (shader_compiled_) return;
    shader_compiled_ = true;

    // ä½¿ç”¨ FramePipeline ç¼“å­˜çš„ shader å¥æŸ„
    hiz_cull_shader_ = ctx_.hiz_cull_shader;
}

void HiZCullPass::Setup(RenderGraph& graph) {
    auto hiz_mip = graph.DeclareResource("hiz_mip_chain");
    auto hiz_visibility = graph.DeclareResource("hiz_visibility");
    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, hiz_mip);
    graph.PassWriteWithState(pass, hiz_visibility, ResourceState::UnorderedAccess);
    graph.MarkOutput(hiz_visibility);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void HiZCullPass::Execute(CommandBuffer& /*cmd_buffer*/) {
    EnsureShader();
    if (hiz_cull_shader_ == 0) return;
    if (ctx_.render_targets.hiz_texture == 0) return;
    if (!ctx_.hiz_aabb_ssbo || !ctx_.hiz_visibility_ssbo) return;
    if (ctx_.hiz_object_count <= 0) return;

    auto* rhi = ctx_.rhi_device;
    if (!rhi) return;

    const unsigned int hiz_gpu_tex = rhi->GetHiZGpuTexture(ctx_.render_targets.hiz_texture);
    if (hiz_gpu_tex == 0) return;

    const int mip_count = rhi->GetHiZMipCount(ctx_.render_targets.hiz_texture);

    // Bind SSBOs (DX11 åŒºåˆ† SRV/UAV; GL/VK å¿½ç•¥ writable)
    rhi->BindGpuBuffer(ctx_.hiz_aabb_ssbo, 0, false);
    rhi->BindGpuBuffer(ctx_.hiz_visibility_ssbo, 1, true);

    // Bind Hi-Z texture as sampler
    rhi->SetComputeTextureSampler(0, hiz_gpu_tex);

    // Get current camera VP matrix for AABB projection
    glm::mat4 view_projection(1.0f);
    {
        const auto& snap = *ctx_.snapshot;
        if (snap.camera_3d.valid) {
            const glm::mat4 clip_correction = rhi->GetProjectionCorrection();
            glm::mat4 projection = clip_correction * glm::perspective(
                glm::radians(snap.camera_3d.fov),
                static_cast<float>(Screen::width()) / static_cast<float>(std::max(1, Screen::height())),
                snap.camera_3d.near_clip, snap.camera_3d.far_clip);
            view_projection = projection * snap.camera_3d.view;

            // å•æº cull ç€è‰²å™¨ç»Ÿä¸€å‡è®¾ ndc.zâˆˆ[0,1]ã€‚GL/WebGL2 çš„ GetProjectionCorrection
            // åœ¨ Z è¡Œä¸ºæ’ç­‰ï¼ˆndc.zâˆˆ[-1,1]ï¼‰ï¼Œæ•…ç»™ä¸Šä¼ çŸ©é˜µè¡¥ä¸€æ¬¡ z'=0.5z+0.5 æŠ˜å ï¼Œä½¿å››åŽç«¯
            // ndc.z åŒä¸º [0,1]ï¼ˆæ•°å­¦ç­‰ä»·æ—§ GL inline ç€è‰²å™¨é‡Œçš„ *0.5+0.5ï¼›DX11/VK å·² remapï¼Œè·³è¿‡ï¼‰ã€‚
            if (clip_correction[2][2] == 1.0f && clip_correction[3][2] == 0.0f) {
                glm::mat4 z_remap(1.0f);
                z_remap[2][2] = 0.5f;
                z_remap[3][2] = 0.5f;
                view_projection = z_remap * view_projection;
            }
        }
    }

    // Set uniforms through RHI (push constants for VK, cbuffer for DX11, glUniform for GL)
    rhi->SetComputeUniformMat4(hiz_cull_shader_, "u_view_projection", &view_projection[0][0]);
    rhi->SetComputeUniformVec2f(hiz_cull_shader_, "u_screen_size",
                                static_cast<float>(Screen::width()),
                                static_cast<float>(Screen::height()));
    rhi->SetComputeUniformInt(hiz_cull_shader_, "u_mip_count", mip_count);
    rhi->SetComputeUniformInt(hiz_cull_shader_, "u_object_count", ctx_.hiz_object_count);

    unsigned int groups_x = (static_cast<unsigned int>(ctx_.hiz_object_count) + 63) / 64;
    rhi->DispatchCompute(hiz_cull_shader_, groups_x, 1, 1);
    rhi->ComputeMemoryBarrier();
}

// ============================================================
// GPUCullPass â€” GPU Driven è§†é”¥ + Hi-Z å‰”é™¤ï¼Œç›´æŽ¥å†™ indirect args
// ============================================================


// ---------------- WebGPU WGSL ç‰ˆæœ¬ï¼ˆæ‰‹è¯‘ï¼›å¼•æ“Žæ—  GLSL/SPIR-Vâ†’WGSL å·¥å…·ï¼‰ ----------------
// ç»‘å®šçº¦å®šï¼ˆä¸Ž WebGPU RHI compute ç»‘å®šé¢ä¸€è‡´ï¼‰ï¼š
//   group1 b8 = å‘½å uniform å—ï¼ˆSetComputeUniform* æŒ‰è°ƒç”¨åº 16B å¯¹é½ç´¯ç§¯ï¼›å„æˆå‘˜ @align(16)ï¼‰ã€‚
//   group2    = çº¹ç†/storage imageï¼›é‡‡æ ·çº¹ç† @slotï¼›storage image ä¸Žé‡‡æ ·çº¹ç†åŒæ§½æ—¶æŒªåˆ° slot+8ï¼ˆä»… Hi-Z copyï¼‰ã€‚
//   group3    = SSBOï¼ˆBindGpuBuffer slot â†’ bindingï¼›compute ç»Ÿä¸€ read_write storageï¼‰ã€‚
// Hi-Z æ·±åº¦çº¦å®šï¼šWebGPU NDC zâˆˆ[0,1]ï¼ˆæŠ•å½±å« GetProjectionCorrectionï¼‰ï¼Œæ•…å‰”é™¤ test_depth ä¸åš GL çš„
//   *0.5+0.5 é‡æ˜ å°„ï¼Œç›´æŽ¥ç”¨ nearest_zï¼ˆä¸Žæ·±åº¦ç¼“å†²ä¸€è‡´ï¼‰ã€‚R32Float Hi-Z ä¸º unfilterable-floatï¼Œé‡‡æ ·æ”¹
//   textureLoadï¼ˆuvâ†’texelï¼‰å–ä»£ textureLodã€‚
const char* kHiZCopyShaderSourceWGSL = R"WGSL(// dse-wgsl
struct PC { u_dst_size : vec2<i32>, };
@group(1) @binding(8) var<uniform> pc : PC;
@group(2) @binding(0) var u_depth_texture : texture_depth_2d;
@group(2) @binding(8) var u_hiz_mip0 : texture_storage_2d<r32float, write>;
@compute @workgroup_size(16, 16, 1)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let coord = vec2<i32>(gid.xy);
  if (coord.x >= pc.u_dst_size.x || coord.y >= pc.u_dst_size.y) { return; }
  let depth = textureLoad(u_depth_texture, coord, 0);
  textureStore(u_hiz_mip0, coord, vec4<f32>(depth, 0.0, 0.0, 0.0));
}
)WGSL";

const char* kHiZDownsampleShaderSourceWGSL = R"WGSL(// dse-wgsl
struct PC { u_src_size : vec2<i32>, @align(16) u_dst_size : vec2<i32>, };
@group(1) @binding(8) var<uniform> pc : PC;
@group(2) @binding(0) var u_src_mip : texture_2d<f32>;
@group(2) @binding(1) var u_dst_mip : texture_storage_2d<r32float, write>;
@compute @workgroup_size(16, 16, 1)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let dst = vec2<i32>(gid.xy);
  if (dst.x >= pc.u_dst_size.x || dst.y >= pc.u_dst_size.y) { return; }
  let sc = dst * 2;
  let smax = pc.u_src_size - vec2<i32>(1, 1);
  let d00 = textureLoad(u_src_mip, sc, 0).r;
  let d10 = textureLoad(u_src_mip, min(sc + vec2<i32>(1, 0), smax), 0).r;
  let d01 = textureLoad(u_src_mip, min(sc + vec2<i32>(0, 1), smax), 0).r;
  let d11 = textureLoad(u_src_mip, min(sc + vec2<i32>(1, 1), smax), 0).r;
  let m = max(max(d00, d10), max(d01, d11));
  textureStore(u_dst_mip, dst, vec4<f32>(m, 0.0, 0.0, 0.0));
}
)WGSL";

const char* kHiZCullShaderSourceWGSL = R"WGSL(// dse-wgsl
struct AABB { min_point : vec4<f32>, max_point : vec4<f32>, };
struct PC {
  u_view_projection : mat4x4<f32>,
  @align(16) u_screen_size : vec2<f32>,
  @align(16) u_mip_count : i32,
  @align(16) u_object_count : i32,
};
@group(1) @binding(8) var<uniform> pc : PC;
@group(2) @binding(0) var u_hiz_texture : texture_2d<f32>;
@group(3) @binding(0) var<storage, read_write> aabbs : array<AABB>;
@group(3) @binding(1) var<storage, read_write> visibility : array<u32>;
fn sample_hiz(uv : vec2<f32>, mip : i32) -> f32 {
  let dim = vec2<i32>(textureDimensions(u_hiz_texture, mip));
  let c = clamp(vec2<i32>(uv * vec2<f32>(dim)), vec2<i32>(0, 0), dim - vec2<i32>(1, 1));
  return textureLoad(u_hiz_texture, c, mip).r;
}
@compute @workgroup_size(64)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let idx = gid.x;
  if (i32(idx) >= pc.u_object_count) { return; }
  let amin = aabbs[idx].min_point.xyz;
  let amax = aabbs[idx].max_point.xyz;
  var ndc_min = vec2<f32>(1.0, 1.0);
  var ndc_max = vec2<f32>(-1.0, -1.0);
  var nearest_z = 1.0;
  for (var i = 0; i < 8; i = i + 1) {
    let corner = vec3<f32>(
      select(amin.x, amax.x, (i & 1) != 0),
      select(amin.y, amax.y, (i & 2) != 0),
      select(amin.z, amax.z, (i & 4) != 0));
    let clip = pc.u_view_projection * vec4<f32>(corner, 1.0);
    if (clip.w <= 0.0) { visibility[idx] = 1u; return; }
    let ndc = clip.xyz / clip.w;
    ndc_min = min(ndc_min, ndc.xy);
    ndc_max = max(ndc_max, ndc.xy);
    nearest_z = min(nearest_z, ndc.z);
  }
  let uv_min = clamp(ndc_min * 0.5 + 0.5, vec2<f32>(0.0), vec2<f32>(1.0));
  let uv_max = clamp(ndc_max * 0.5 + 0.5, vec2<f32>(0.0), vec2<f32>(1.0));
  if (uv_max.x <= 0.0 || uv_min.x >= 1.0 || uv_max.y <= 0.0 || uv_min.y >= 1.0) {
    visibility[idx] = 0u; return;
  }
  let size_pixels = (uv_max - uv_min) * pc.u_screen_size;
  let max_dim = max(size_pixels.x, size_pixels.y);
  var mipf = select(0.0, ceil(log2(max_dim)), max_dim > 0.0);
  mipf = clamp(mipf, 0.0, f32(pc.u_mip_count - 1));
  let mip = i32(mipf);
  let test_depth = nearest_z - 0.005;
  let uv_center = (uv_min + uv_max) * 0.5;
  let h0 = sample_hiz(uv_center, mip);
  let h1 = sample_hiz(uv_min, mip);
  let h2 = sample_hiz(uv_max, mip);
  let h3 = sample_hiz(vec2<f32>(uv_max.x, uv_min.y), mip);
  let h4 = sample_hiz(vec2<f32>(uv_min.x, uv_max.y), mip);
  let max_hiz = max(max(h0, h1), max(max(h2, h3), h4));
  if (test_depth > max_hiz) { visibility[idx] = 0u; } else { visibility[idx] = 1u; }
}
)WGSL";

const char* kGPUCullShaderSourceWGSL = R"WGSL(// dse-wgsl
struct AABB { min_point : vec4<f32>, max_point : vec4<f32>, };
struct DrawCmd { count : u32, instance_count : u32, first_index : u32, base_vertex : i32, base_instance : u32, };
struct PC {
  u_view_projection : mat4x4<f32>,
  @align(16) u_screen_size : vec2<f32>,
  @align(16) u_mip_count : i32,
  @align(16) u_object_count : i32,
  @align(16) u_frustum_planes : array<vec4<f32>, 6>,
};
@group(1) @binding(8) var<uniform> pc : PC;
@group(2) @binding(0) var u_hiz_texture : texture_2d<f32>;
@group(3) @binding(0) var<storage, read_write> aabbs : array<AABB>;
@group(3) @binding(1) var<storage, read_write> draw_cmds : array<DrawCmd>;
fn frustum_test(amin : vec3<f32>, amax : vec3<f32>) -> bool {
  for (var i = 0; i < 6; i = i + 1) {
    let p = pc.u_frustum_planes[i];
    let pv = vec3<f32>(
      select(amin.x, amax.x, p.x >= 0.0),
      select(amin.y, amax.y, p.y >= 0.0),
      select(amin.z, amax.z, p.z >= 0.0));
    if (dot(p.xyz, pv) + p.w < 0.0) { return false; }
  }
  return true;
}
fn sample_hiz(uv : vec2<f32>, mip : i32) -> f32 {
  let dim = vec2<i32>(textureDimensions(u_hiz_texture, mip));
  let c = clamp(vec2<i32>(uv * vec2<f32>(dim)), vec2<i32>(0, 0), dim - vec2<i32>(1, 1));
  return textureLoad(u_hiz_texture, c, mip).r;
}
@compute @workgroup_size(64)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let idx = gid.x;
  if (i32(idx) >= pc.u_object_count) { return; }
  let amin = aabbs[idx].min_point.xyz;
  let amax = aabbs[idx].max_point.xyz;
  if (!frustum_test(amin, amax)) { draw_cmds[idx].instance_count = 0u; return; }
  var ndc_min = vec2<f32>(1.0, 1.0);
  var ndc_max = vec2<f32>(-1.0, -1.0);
  var nearest_z = 1.0;
  for (var i = 0; i < 8; i = i + 1) {
    let corner = vec3<f32>(
      select(amin.x, amax.x, (i & 1) != 0),
      select(amin.y, amax.y, (i & 2) != 0),
      select(amin.z, amax.z, (i & 4) != 0));
    let clip = pc.u_view_projection * vec4<f32>(corner, 1.0);
    if (clip.w <= 0.0) { draw_cmds[idx].instance_count = 1u; return; }
    let ndc = clip.xyz / clip.w;
    ndc_min = min(ndc_min, ndc.xy);
    ndc_max = max(ndc_max, ndc.xy);
    nearest_z = min(nearest_z, ndc.z);
  }
  let uv_min = clamp(ndc_min * 0.5 + 0.5, vec2<f32>(0.0), vec2<f32>(1.0));
  let uv_max = clamp(ndc_max * 0.5 + 0.5, vec2<f32>(0.0), vec2<f32>(1.0));
  if (uv_max.x <= 0.0 || uv_min.x >= 1.0 || uv_max.y <= 0.0 || uv_min.y >= 1.0) {
    draw_cmds[idx].instance_count = 0u; return;
  }
  let size_pixels = (uv_max - uv_min) * pc.u_screen_size;
  let max_dim = max(size_pixels.x, size_pixels.y);
  var mipf = select(0.0, ceil(log2(max_dim)), max_dim > 0.0);
  mipf = clamp(mipf, 0.0, f32(pc.u_mip_count - 1));
  let mip = i32(mipf);
  let test_depth = nearest_z - 0.005;
  let uv_center = (uv_min + uv_max) * 0.5;
  let h0 = sample_hiz(uv_center, mip);
  let h1 = sample_hiz(uv_min, mip);
  let h2 = sample_hiz(uv_max, mip);
  let h3 = sample_hiz(vec2<f32>(uv_max.x, uv_min.y), mip);
  let h4 = sample_hiz(vec2<f32>(uv_min.x, uv_max.y), mip);
  let max_hiz = max(max(h0, h1), max(max(h2, h3), h4));
  if (test_depth > max_hiz) { draw_cmds[idx].instance_count = 0u; } else { draw_cmds[idx].instance_count = 1u; }
}
)WGSL";

void GPUCullPass::Setup(RenderGraph& graph) {
    auto hiz_mips = graph.DeclareResource("hiz_mips");
    auto gpu_draw_cmds = graph.DeclareResource("gpu_draw_commands");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, hiz_mips);
    graph.PassWriteWithState(pass, gpu_draw_cmds, ResourceState::UnorderedAccess);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void GPUCullPass::Execute(CommandBuffer& /*cmd_buffer*/) {
    if (!ctx_.gpu_driven_active_this_frame) return;
    if (ctx_.gpu_cull_shader == 0) return;
    if (ctx_.render_targets.hiz_texture == 0) return;
    if (!ctx_.gpu_draw_cmd_ssbo) return;
    if (!ctx_.gpu_aabb_ssbo) return;
    if (ctx_.gpu_indirect_draw_count <= 0) return;

    auto* rhi = ctx_.rhi_device;
    if (!rhi) return;

    const unsigned int hiz_gpu_tex = rhi->GetHiZGpuTexture(ctx_.render_targets.hiz_texture);
    if (hiz_gpu_tex == 0) return;

    const int mip_count = rhi->GetHiZMipCount(ctx_.render_targets.hiz_texture);

    // Bind SSBOs
    rhi->BindGpuBuffer(ctx_.gpu_aabb_ssbo, 0, false);
    rhi->BindGpuBuffer(ctx_.gpu_draw_cmd_ssbo, 1, true);

    // Bind Hi-Z texture
    rhi->SetComputeTextureSampler(0, hiz_gpu_tex);

    // Get VP matrix
    glm::mat4 view_projection(1.0f);
    glm::vec4 frustum_planes[6] = {};
    {
        const auto& snap = *ctx_.snapshot;
        if (snap.camera_3d.valid) {
            const glm::mat4 clip_correction = rhi->GetProjectionCorrection();
            glm::mat4 projection = clip_correction * glm::perspective(
                glm::radians(snap.camera_3d.fov),
                static_cast<float>(Screen::width()) / static_cast<float>(std::max(1, Screen::height())),
                snap.camera_3d.near_clip, snap.camera_3d.far_clip);
            view_projection = projection * snap.camera_3d.view;

            // Extract frustum planes from VP matrix (Gribb/Hartmann method)
            const glm::mat4& m = view_projection;
            frustum_planes[0] = glm::vec4(m[0][3]+m[0][0], m[1][3]+m[1][0], m[2][3]+m[2][0], m[3][3]+m[3][0]);
            frustum_planes[1] = glm::vec4(m[0][3]-m[0][0], m[1][3]-m[1][0], m[2][3]-m[2][0], m[3][3]-m[3][0]);
            frustum_planes[2] = glm::vec4(m[0][3]+m[0][1], m[1][3]+m[1][1], m[2][3]+m[2][1], m[3][3]+m[3][1]);
            frustum_planes[3] = glm::vec4(m[0][3]-m[0][1], m[1][3]-m[1][1], m[2][3]-m[2][1], m[3][3]-m[3][1]);
            frustum_planes[4] = glm::vec4(m[0][3]+m[0][2], m[1][3]+m[1][2], m[2][3]+m[2][2], m[3][3]+m[3][2]);
            frustum_planes[5] = glm::vec4(m[0][3]-m[0][2], m[1][3]-m[1][2], m[2][3]-m[2][2], m[3][3]-m[3][2]);

            for (int i = 0; i < 6; ++i) {
                float len = glm::length(glm::vec3(frustum_planes[i]));
                if (len > 0.0f) frustum_planes[i] /= len;
            }

            // frustum planes å·²ä»ŽæœªæŠ˜å  VP æå–ï¼ˆå‰”é™¤è¯­ä¹‰é›¶æ”¹åŠ¨ï¼‰ã€‚å†å¯¹ä¸Šä¼ çŸ©é˜µè¡¥ GL Z æŠ˜å ï¼š
            // å•æº cull å‡è®¾ ndc.zâˆˆ[0,1]ï¼ŒGL/WebGL2 çš„ GetProjectionCorrection Z è¡Œæ’ç­‰ â†’ z'=0.5z+0.5ã€‚
            if (clip_correction[2][2] == 1.0f && clip_correction[3][2] == 0.0f) {
                glm::mat4 z_remap(1.0f);
                z_remap[2][2] = 0.5f;
                z_remap[3][2] = 0.5f;
                view_projection = z_remap * view_projection;
            }
        }
    }

    // Set uniforms
    unsigned int shader = ctx_.gpu_cull_shader;
    rhi->SetComputeUniformMat4(shader, "u_view_projection", &view_projection[0][0]);
    rhi->SetComputeUniformVec2f(shader, "u_screen_size",
                                static_cast<float>(Screen::width()),
                                static_cast<float>(Screen::height()));
    rhi->SetComputeUniformInt(shader, "u_mip_count", mip_count);
    rhi->SetComputeUniformInt(shader, "u_object_count", ctx_.gpu_indirect_draw_count);

    // Upload frustum planes as 6 vec4 uniforms
    for (int i = 0; i < 6; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "u_frustum_planes[%d]", i);
        rhi->SetComputeUniformVec4(shader, name,
            frustum_planes[i].x, frustum_planes[i].y,
            frustum_planes[i].z, frustum_planes[i].w);
    }

    unsigned int groups_x = (static_cast<unsigned int>(ctx_.gpu_indirect_draw_count) + 63) / 64;
    rhi->DispatchCompute(shader, groups_x, 1, 1);
    rhi->ComputeMemoryBarrier();
}

// ============================================================
// RSMRenderPass â€” ä»Žæ–¹å‘å…‰è§†è§’æ¸²æŸ“åœºæ™¯åˆ° RSM MRT (position/normal/flux)
// ============================================================

void RSMRenderPass::Setup(RenderGraph& graph) {
    auto shadow_depth = graph.DeclareResource("shadow_depth_rsm_dep");
    auto rsm_data = graph.DeclareResource("rsm_data");
    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, shadow_depth);
    graph.PassWrite(pass, rsm_data);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void RSMRenderPass::Execute(CommandBuffer& cmd_buffer) {
    if (!ctx_.ddgi_active || ctx_.rsm_targets.position == 0) return;
    if (ctx_.rsm_render_target == 0) return;

    const auto& snap = *ctx_.snapshot;
    if (!snap.directional_light.valid) return;

    // Camera-Relative: shadow_center è½¬æ¢åˆ°ç›¸æœºç›¸å¯¹ç©ºé—´
    glm::vec3 shadow_center = FindShadowCenter(snap) - ctx_.camera_offset;
    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
    const float cam_near = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    const float aspect = static_cast<float>(Screen::width()) / static_cast<float>(std::max(Screen::height(), 1));
    const float tan_half_fov = std::tan(glm::radians(snap.camera_3d.valid ? snap.camera_3d.fov * 0.5f : 30.0f));
    const glm::mat4 inv_view = snap.camera_3d.valid ? glm::inverse(snap.camera_3d.view) : glm::mat4(1.0f);
    const float ortho_size = ComputeCascadeFit(
        inv_view, snap.directional_light.direction, cam_near,
        snap.directional_light.cascade_splits[0], aspect, tan_half_fov).size;
    auto cam = ComputeDirectionalLightCamera(
        shadow_center, snap.directional_light.direction, ortho_size, clip_correction);

    cmd_buffer.BeginRenderPass({ctx_.rsm_render_target, glm::vec4(0.0f), true});
    ctx_.rhi_device->SetGBufferRenderingMode(true);
    FrameContext frame{cam.view, cam.projection};
    cmd_buffer.BindPipeline(ctx_.pipeline_states.mesh);

    RenderScenePassContext pass_ctx;
    pass_ctx.world = ctx_.world;
    pass_ctx.view = &cam.view;
    pass_ctx.projection = &cam.projection;
    pass_ctx.camera_offset = ctx_.camera_offset;
    if (ctx_.render_scene) {
        ctx_.render_scene->DrawOpaqueCpu(cmd_buffer, *ctx_.rhi_device, *ctx_.mesh_renderer, frame);
    }
    ExecuteSceneRenderers(ctx_.render_scene, SceneRenderStage::Opaque, cmd_buffer, pass_ctx);

    ctx_.rhi_device->SetGBufferRenderingMode(false);
    cmd_buffer.EndRenderPass();
}

// ============================================================
// DDGIUpdatePass â€” ä»Ž RSM VPL æ›´æ–° Irradiance Probe Atlas (Compute Shader)
// ============================================================

void DDGIUpdatePass::Setup(RenderGraph& graph) {
    auto rsm_data = graph.DeclareResource("rsm_data");
    auto ddgi_atlas = graph.DeclareResource("ddgi_irradiance_atlas");
    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, rsm_data);
    graph.PassWriteWithState(pass, ddgi_atlas, ResourceState::UnorderedAccess);
    graph.MarkOutput(ddgi_atlas);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void DDGIUpdatePass::Execute(CommandBuffer& /*cmd_buffer*/) {
    if (!ctx_.ddgi_active || !ctx_.ddgi_system) return;

    auto* rhi = ctx_.rhi_device;
    if (!rhi || !rhi->SupportsCompute()) return;

    const auto& snap = *ctx_.snapshot;
    glm::vec3 light_dir(0.0f, -1.0f, 0.0f);
    glm::vec3 light_color(1.0f);

    if (snap.directional_light.valid) {
        light_dir = glm::normalize(-snap.directional_light.direction);
        light_color = glm::vec3(snap.directional_light.color) * snap.directional_light.intensity;
    }

    // é©±åŠ¨ DDGI ç³»ç»Ÿæ›´æ–°æŽ¢é’ˆï¼ˆä¼ å…¥å¤–éƒ¨ç®¡ç†çš„ RSM çº¹ç†å¥æŸ„ï¼‰
    ctx_.ddgi_system->UpdateProbes(rhi,
                                    ctx_.rsm_targets.position,
                                    ctx_.rsm_targets.normal,
                                    ctx_.rsm_targets.flux,
                                    ctx_.rsm_targets.width,
                                    ctx_.rsm_targets.height,
                                    light_dir, light_color);

    // æ›´æ–° context ä¸­çš„ atlas å¥æŸ„ä¾›åŽç»­ Pass é‡‡æ ·
    const auto& res = ctx_.ddgi_system->GetResources();
    ctx_.ddgi_irradiance_atlas = res.irradiance_atlas;
    ctx_.ddgi_visibility_atlas = res.visibility_atlas;
}

// ============================================================
// SSSBlurPass â€” Separable Subsurface Scattering
// ============================================================

void SSSBlurPass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto sss_output = graph.DeclareResource("sss_output");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassWrite(pass, sss_output);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void SSSBlurPass::Execute(CommandBuffer& cmd_buffer) {
    // SSS blur: two-pass separable Gaussian on scene color, masked by alpha (SSS strength)
    if (ctx_.render_targets.sss_temp == 0 || ctx_.render_targets.scene == 0) return;

    const unsigned int scene_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);
    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.scene);
    if (scene_color_tex == 0 || depth_tex == 0) return;

    const float sss_width = 11.0f;
    const float depth_falloff = 500.0f;

    cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);

    // Pass 1: Horizontal blur â†’ sss_temp
    cmd_buffer.BeginRenderPass({ctx_.render_targets.sss_temp, glm::vec4(0.0f), true});
    {
        auto req = PostProcessRequest("sss_blur", scene_color_tex, {
            1.0f, 0.0f,
            static_cast<float>(Screen::width()),
            static_cast<float>(Screen::height()),
            sss_width,
            depth_falloff
        });
        req.Tex(2, depth_tex);
        post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, std::move(req));
    }
    cmd_buffer.EndRenderPass();

    // Pass 2: Vertical blur â†’ back to scene RT
    const unsigned int sss_temp_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.sss_temp);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
    {
        auto req = PostProcessRequest("sss_blur", sss_temp_tex, {
            0.0f, 1.0f,
            static_cast<float>(Screen::width()),
            static_cast<float>(Screen::height()),
            sss_width,
            depth_falloff
        });
        req.Tex(2, depth_tex);
        post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, std::move(req));
    }
    cmd_buffer.EndRenderPass();
}

// ============================================================
// WeatherPass â€” Screen-Space Weather Particles (rain / snow)
// ============================================================

void WeatherPass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto prez_depth  = graph.DeclareResource("prez_depth");
    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, prez_depth);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void WeatherPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    if (!snap.weather.valid || snap.weather.type == 0 || snap.weather.intensity < 0.001f) return;

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;
    const unsigned int scene_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);

    glm::vec3 cam_pos(0.0f);
    float cam_fov  = snap.camera_3d.valid ? snap.camera_3d.fov       : 60.0f;
    float cam_near = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    float cam_far  = snap.camera_3d.valid ? snap.camera_3d.far_clip  : 1000.0f;
    glm::vec3 cam_fwd = snap.camera_3d.forward;

    if (ctx_.editor_mode && ctx_.use_editor_camera) {
        cam_pos = glm::vec3(glm::inverse(ctx_.editor_view)[3]) - ctx_.camera_offset;
        cam_fwd = -glm::normalize(glm::vec3(glm::inverse(ctx_.editor_view)[2]));
    } else if (!snap.camera_3d.valid) {
        return;
    }

    float aspect = static_cast<float>(Screen::width()) / static_cast<float>(std::max(1, Screen::height()));
    float tan_fov_y = std::tan(glm::radians(cam_fov) * 0.5f);
    float current_time = Time::TimeSinceStartup();

    const auto& w = snap.weather;
    std::vector<float> params(24);
    params[0]  = current_time;
    params[1]  = w.intensity;
    params[2]  = w.wind_x;
    params[3]  = w.wind_z;
    params[4]  = static_cast<float>(w.type);
    params[5]  = w.color.r;
    params[6]  = w.color.g;
    params[7]  = w.color.b;
    params[8]  = w.color.a;
    params[9]  = cam_pos.x;
    params[10] = cam_pos.y;
    params[11] = cam_pos.z;
    params[12] = cam_near;
    params[13] = cam_far;
    params[14] = w.spawn_radius;
    params[15] = w.spawn_height;
    params[16] = static_cast<float>(Screen::width());
    params[17] = static_cast<float>(Screen::height());
    params[18] = cam_fwd.x;
    params[19] = cam_fwd.y;
    params[20] = cam_fwd.z;
    params[21] = tan_fov_y;
    params[22] = aspect;
    params[23] = w.intensity;  // wetness = rain intensity

    cmd_buffer.BindPipeline(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
    post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device,
        PostProcessRequest{"weather_particle", scene_tex, params}.Tex(2, depth_tex));
    cmd_buffer.EndRenderPass();
}

} // namespace render
} // namespace dse

