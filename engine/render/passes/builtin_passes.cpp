/**
 * @file builtin_passes.cpp
 * @brief å¼•æ“Žå†…ç½®æ¸²æŸ“ Pass å®žçŽ°
 *
 * --- Section Index ---
 *   PreZPass                    ~91
 *   CSMShadowPass               ~139
 *   SpotShadowPass              ~200
 *   PointShadowPass             ~252
 *   ForwardScenePass            ~451
 *   BloomPass                   ~638
 *   UIPass                      ~707
 *   CompositePass               ~728
 *   AutoExposurePass            ~861
 *   SSAOPass                    ~925
 *   ContactShadowPass           ~993
 *   FXAAPass                    ~1064
 *   PresentPass                 ~1106
 *   TAAPass                     ~1138
 *   DOFPass                     ~1255
 *   MotionVectorPass            ~1325
 *   MotionBlurPass              ~1394
 *   SSRPass                     ~1450
 *   OutlinePass                 ~1520
 *   LightShaftPass              ~1589
 *   VolumetricFogPass           ~1678
 *   WBOITPass                   ~1785
 *   WaterPass                   ~1847
 */

#include "engine/render/passes/builtin_passes.h"
#include "engine/render/passes/builtin_passes_internal.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/render/gi/ddgi_system.h"
#include "engine/base/debug.h"
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

// è°ƒç”¨æ¨¡å—æ³¨å†Œçš„å¼ºç±»åž‹åœºæ™¯è´¡çŒ®å¯¹è±¡ï¼ˆISceneRendererï¼‰çš„æŒ‡å®šé˜¶æ®µã€‚
void ExecuteSceneRenderers(const RenderScene* scene,
                           SceneRenderStage stage,
                           CommandBuffer& cmd_buffer,
                           const RenderScenePassContext& pass_ctx) {
    if (!scene) return;
    for (auto* renderer : scene->scene_renderers) {
        if (!renderer) continue;
        switch (stage) {
            case SceneRenderStage::PreZ:        renderer->RenderPreZ(cmd_buffer, pass_ctx); break;
            case SceneRenderStage::Shadow:      renderer->RenderShadow(cmd_buffer, pass_ctx); break;
            case SceneRenderStage::Opaque:      renderer->RenderOpaque(cmd_buffer, pass_ctx); break;
            case SceneRenderStage::Transparent: renderer->RenderTransparent(cmd_buffer, pass_ctx); break;
        }
    }
}

// ============================================================
// PreZPass
// ============================================================

void PreZPass::Setup(RenderGraph& graph) {
    auto prez_depth = graph.DeclareResource("prez_depth");
    auto pass = graph.AddPass(GetName());
    graph.PassWrite(pass, prez_depth);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void PreZPass::Execute(CommandBuffer& cmd_buffer) {
    cmd_buffer.BeginRenderPass({ctx_.render_targets.prez, glm::vec4(0.0f), true});
    const auto& snap = *ctx_.snapshot;
    if (snap.camera_3d.valid) {
        const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        glm::mat4 projection = clip_correction * glm::perspective(glm::radians(snap.camera_3d.fov),
                                                static_cast<float>(Screen::width()) / static_cast<float>(Screen::height()),
                                                snap.camera_3d.near_clip, snap.camera_3d.far_clip);

        // TAA jitter å¿…é¡»ä¸Ž ForwardScenePass ä¸€è‡´ï¼Œå¦åˆ™ PreZ æ·±åº¦ä¸Žä¸» pass ä¸åŒ¹é…å¯¼è‡´é—ªçƒ
        if (ctx_.taa_active) {
            projection[2][0] += ctx_.taa_jitter.x * 2.0f;
            projection[2][1] += ctx_.taa_jitter.y * 2.0f;
        }

        FrameContext frame{snap.camera_3d.view, projection};
        cmd_buffer.BindPipeline(ctx_.pipeline_states.prez);

        // GPU-driven PreZ: eligible å®žä½“ depth-only indirect draw
        const bool use_gpu_indirect = ctx_.gpu_driven_active_this_frame
            && ctx_.gpu_mega_vao && ctx_.gpu_draw_cmd_ssbo
            && ctx_.gpu_indirect_draw_count > 0;
        if (use_gpu_indirect) {
            auto* rhi = ctx_.rhi_device;
            rhi->SetupGPUDrivenShadowShader(snap.camera_3d.view, projection);
            rhi->BindGpuBuffer(ctx_.gpu_instance_ssbo, dse::render::gpu_driven::kSSBOBindingInstances);
            rhi->BindMegaVAO(ctx_.gpu_mega_vao);
            rhi->MultiDrawIndexedIndirect(ctx_.gpu_draw_cmd_ssbo.raw(),
                                          ctx_.gpu_indirect_draw_count,
                                          sizeof(DrawElementsIndirectCommand));
            rhi->UnbindVAO();
        }

        if (ctx_.render_scene) {
            ctx_.render_scene->DrawOpaqueCpu(cmd_buffer, *ctx_.rhi_device, *ctx_.mesh_renderer, frame);
        }
        RenderScenePassContext pass_ctx;
        pass_ctx.world = ctx_.world;
        pass_ctx.view = &snap.camera_3d.view;
        pass_ctx.projection = &projection;
        pass_ctx.camera_offset = ctx_.camera_offset;
        ExecuteSceneRenderers(ctx_.render_scene, SceneRenderStage::PreZ, cmd_buffer, pass_ctx);
    }
    cmd_buffer.EndRenderPass();
}

// ============================================================
// CSMShadowPass
// ============================================================

void CSMShadowPass::Setup(RenderGraph& graph) {
    auto shadow_depth = graph.DeclareResource("shadow_depth");
    auto pass = graph.AddPass(GetName());
    graph.PassWrite(pass, shadow_depth);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void CSMShadowPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const auto& dl = snap.directional_light;
    if (!dl.valid || !dl.cast_shadow) return;

    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
    const glm::mat4 shadow_sample_correction = ctx_.rhi_device->GetShadowSampleCorrection();
    // Camera-Relative: shadow_center è½¬æ¢åˆ°ç›¸æœºç›¸å¯¹ç©ºé—´
    glm::vec3 shadow_center = FindShadowCenter(snap) - ctx_.camera_offset;

    const bool use_gpu_indirect = ctx_.gpu_driven_active_this_frame
        && ctx_.gpu_mega_vao
        && ctx_.gpu_draw_cmd_ssbo
        && ctx_.gpu_indirect_draw_count > 0;

    std::vector<float> cascade_splits(CSM_CASCADES);

    // Atlas layout: cascade 0 (2048Ã—2048) at (0,0); cascade 1 (1024Ã—1024) at (2048,0); cascade 2 (512Ã—512) at (3072,0)
    constexpr int kAtlasOffsetX[CSM_CASCADES] = {0, 2048, 3072};
    constexpr int kAtlasOffsetY[CSM_CASCADES] = {0, 0, 0};
    constexpr int kShadowRes[CSM_CASCADES] = {2048, 1024, 512};
    constexpr float kAtlasWidth = 4096.0f;
    constexpr float kAtlasHeight = 2048.0f;

    // çº§è”åˆ†è£‚è·ç¦»ï¼šPSSM ä¸Žç»„ä»¶æ‰‹åŠ¨ cascade_splits æŒ‰ lambda æ··åˆ
    const float cam_near = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    const float cam_far  = snap.camera_3d.valid ? snap.camera_3d.far_clip  : 1000.0f;
    const float lambda   = glm::clamp(dl.cascade_split_lambda, 0.0f, 1.0f);
    const float ratio    = cam_far / std::max(cam_near, 0.001f);
    const float aspect   = static_cast<float>(Screen::width()) / static_cast<float>(std::max(Screen::height(), 1));
    const float tan_half_fov = std::tan(glm::radians(snap.camera_3d.valid ? snap.camera_3d.fov * 0.5f : 30.0f));
    const glm::mat4 inv_view = snap.camera_3d.valid ? glm::inverse(snap.camera_3d.view) : glm::mat4(1.0f);

    for (int i = 0; i < CSM_CASCADES; ++i) {
        const float p = static_cast<float>(i + 1) / static_cast<float>(CSM_CASCADES);
        const float log_split     = cam_near * std::pow(ratio, p);
        const float uniform_split = cam_near + (cam_far - cam_near) * p;
        const float pssm_split    = lambda * log_split + (1.0f - lambda) * uniform_split;
        cascade_splits[i] = lambda * pssm_split + (1.0f - lambda) * dl.cascade_splits[i];
    }

    // å•æ¬¡ BeginRenderPass ç»‘å®š atlas RTï¼Œå…¨é‡æ¸…é™¤æ·±åº¦
    {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.shadow_atlas, glm::vec4(1.0f), true});
        cmd_buffer.BindPipeline(ctx_.pipeline_states.shadow);

        float prev_split = cam_near;
        for (int i = 0; i < CSM_CASCADES; ++i) {
            const float split_far_plane = cascade_splits[i];
            CascadeFit fit = ComputeCascadeFit(
                inv_view, dl.direction, prev_split, split_far_plane, aspect, tan_half_fov);
            prev_split = split_far_plane;
            const float size = fit.size;
            // é€çº§è”ä»¥å…¶è§†é”¥åˆ‡ç‰‡è´¨å¿ƒä¸ºå¯¹ç„¦ç‚¹ï¼ˆæ ‡å‡† CSMï¼‰ï¼Œè¿‘çº§è”ç›’ä¸å†è¢«è¿œç„¦ç‚¹æ¼æŽ‰ã€‚
            // é€€åŒ–åœºæ™¯ï¼ˆæ— æœ‰æ•ˆç›¸æœºåˆ‡ç‰‡ï¼‰å›žé€€åˆ°å…¨å±€ shadow_centerã€‚
            const glm::vec3 cascade_center =
                (snap.camera_3d.valid) ? fit.center : shadow_center;
            auto cam = ComputeDirectionalLightCamera(
                cascade_center, dl.direction, size, clip_correction, static_cast<float>(kShadowRes[i]));

            float far_dist = size * 4.0f;
            glm::mat4 sample_proj = shadow_sample_correction *
                glm::ortho(-size, size, -size, size, 1.0f, far_dist);

            cached_light_space_[i] = sample_proj * cam.view;

            // è®¾ç½® viewport åˆ° atlas å†…å¯¹åº”åŒºåŸŸ
            cmd_buffer.SetViewport(kAtlasOffsetX[i], kAtlasOffsetY[i], kShadowRes[i], kShadowRes[i]);
            FrameContext frame{cam.view, cam.projection};

            const bool skip_cpu_shadow = (size > 10000.0f);

            if (use_gpu_indirect) {
                auto* rhi = ctx_.rhi_device;
                rhi->SetupGPUDrivenShadowShader(cam.view, cam.projection);
                rhi->BindGpuBuffer(ctx_.gpu_instance_ssbo, dse::render::gpu_driven::kSSBOBindingInstances);
                rhi->BindMegaVAO(ctx_.gpu_mega_vao);
                rhi->MultiDrawIndexedIndirect(ctx_.gpu_draw_cmd_ssbo.raw(),
                                              ctx_.gpu_indirect_draw_count,
                                              sizeof(DrawElementsIndirectCommand));
                rhi->UnbindVAO();
            }

            if (!skip_cpu_shadow) {
                if (ctx_.render_scene) {
                    ctx_.render_scene->DrawOpaqueCpu(cmd_buffer, *ctx_.rhi_device, *ctx_.mesh_renderer, frame);
                }
                RenderScenePassContext pass_ctx;
                pass_ctx.world = ctx_.world;
                pass_ctx.view = &cam.view;
                pass_ctx.projection = &cam.projection;
                pass_ctx.camera_offset = ctx_.camera_offset;
                pass_ctx.cascade_index = i;
                ExecuteSceneRenderers(ctx_.render_scene, SceneRenderStage::Shadow, cmd_buffer, pass_ctx);
            }
        }

        cmd_buffer.EndRenderPass();
    }

    // ä½¿ç”¨ç¼“å­˜çš„ light space matrixï¼ˆä¸Ž shadow map å†…å®¹å¯¹åº”çš„çŸ©é˜µï¼‰
    for (int i = 0; i < CSM_CASCADES; ++i) {
        ctx_.rhi_device->SetGlobalLightSpaceMatrix(static_cast<unsigned int>(i), cached_light_space_[i]);
        ctx_.rhi_device->SetGlobalCascadeSplit(static_cast<unsigned int>(i), cascade_splits[i]);
    }

    // Atlas region UV: (scale_x, scale_y, offset_x, offset_y) ä¾› PBR shader é‡‡æ ·
    for (int i = 0; i < CSM_CASCADES; ++i) {
        float scale_x = static_cast<float>(kShadowRes[i]) / kAtlasWidth;
        float scale_y = static_cast<float>(kShadowRes[i]) / kAtlasHeight;
        float offset_x = static_cast<float>(kAtlasOffsetX[i]) / kAtlasWidth;
        float offset_y = static_cast<float>(kAtlasOffsetY[i]) / kAtlasHeight;
        ctx_.rhi_device->SetGlobalShadowAtlasRegion(static_cast<unsigned int>(i),
            glm::vec4(scale_x, scale_y, offset_x, offset_y));
    }

    // ç»‘å®šå•ä¸ª atlas æ·±åº¦çº¹ç†åˆ°æ‰€æœ‰ shadow map slot
    unsigned int atlas_depth = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.shadow_atlas);
    for (int i = 0; i < CSM_CASCADES; ++i) {
        cmd_buffer.BindGlobalShadowMap(i, atlas_depth);
    }

}

// ============================================================
// SpotShadowPass
// ============================================================

void SpotShadowPass::Setup(RenderGraph& graph) {
    auto spot_shadow = graph.DeclareResource("spot_shadow");
    auto pass = graph.AddPass(GetName());
    graph.PassWrite(pass, spot_shadow);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void SpotShadowPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
    const glm::mat4 shadow_sample_correction = ctx_.rhi_device->GetShadowSampleCorrection();
    std::vector<glm::mat4> spot_light_space_matrices;
    spot_light_space_matrices.reserve(4);

    const bool use_gpu_indirect = ctx_.gpu_driven_active_this_frame
        && ctx_.gpu_mega_vao
        && ctx_.gpu_draw_cmd_ssbo
        && ctx_.gpu_indirect_draw_count > 0;

    for (int i = 0; i < snap.spot_shadow_count; ++i) {
        if (ctx_.render_targets.spot_shadow[i] == 0) continue;
        const auto& sl = snap.spot_lights[i];

        // Camera-Relative: spot light ä½ç½®è½¬æ¢åˆ°ç›¸æœºç›¸å¯¹ç©ºé—´
        const glm::vec3 sl_pos_relative = sl.position - ctx_.camera_offset;
        const glm::mat4 light_view_mat = glm::lookAt(sl_pos_relative, sl_pos_relative + sl.forward, sl.up);
        const glm::mat4 light_proj = clip_correction * glm::perspective(glm::radians(sl.outer_cone_angle * 2.0f), 1.0f, 0.1f, std::max(1.0f, sl.radius));
        cmd_buffer.BeginRenderPass({ctx_.render_targets.spot_shadow[i], glm::vec4(1.0f), true});
        FrameContext frame{light_view_mat, light_proj};
        cmd_buffer.BindPipeline(ctx_.pipeline_states.shadow);

        if (use_gpu_indirect) {
            auto* rhi = ctx_.rhi_device;
            rhi->SetupGPUDrivenShadowShader(light_view_mat, light_proj);
            rhi->BindGpuBuffer(ctx_.gpu_instance_ssbo, dse::render::gpu_driven::kSSBOBindingInstances);
            rhi->BindMegaVAO(ctx_.gpu_mega_vao);
            rhi->MultiDrawIndexedIndirect(ctx_.gpu_draw_cmd_ssbo.raw(),
                                          ctx_.gpu_indirect_draw_count,
                                          sizeof(DrawElementsIndirectCommand));
            rhi->UnbindVAO();
        }

        if (ctx_.render_scene) {
            ctx_.render_scene->DrawOpaqueCpu(cmd_buffer, *ctx_.rhi_device, *ctx_.mesh_renderer, frame);
        }
        RenderScenePassContext pass_ctx;
        pass_ctx.world = ctx_.world;
        pass_ctx.view = &light_view_mat;
        pass_ctx.projection = &light_proj;
        pass_ctx.camera_offset = ctx_.camera_offset;
        pass_ctx.cascade_index = CSM_CASCADES;
        ExecuteSceneRenderers(ctx_.render_scene, SceneRenderStage::Shadow, cmd_buffer, pass_ctx);
        cmd_buffer.EndRenderPass();
        const glm::mat4 sample_proj = shadow_sample_correction * glm::perspective(glm::radians(sl.outer_cone_angle * 2.0f), 1.0f, 0.1f, std::max(1.0f, sl.radius));
        spot_light_space_matrices.push_back(sample_proj * light_view_mat);
        cmd_buffer.BindGlobalSpotShadowMap(static_cast<unsigned int>(i), ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.spot_shadow[i]));
    }
    for (size_t i = 0; i < spot_light_space_matrices.size() && i < 4; ++i) {
        ctx_.rhi_device->SetGlobalSpotLightSpaceMatrix(static_cast<unsigned int>(i), spot_light_space_matrices[i]);
    }
}

// ============================================================
// PointShadowPass
// ============================================================

void PointShadowPass::Setup(RenderGraph& graph) {
    auto point_shadow = graph.DeclareResource("point_shadow");
    auto pass = graph.AddPass(GetName());
    graph.PassWrite(pass, point_shadow);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void PointShadowPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();

    const bool use_gpu_indirect = ctx_.gpu_driven_active_this_frame
        && ctx_.gpu_mega_vao
        && ctx_.gpu_draw_cmd_ssbo
        && ctx_.gpu_indirect_draw_count > 0;

    for (int shadow_slot = 0; shadow_slot < snap.point_shadow_count; ++shadow_slot) {
        if (ctx_.render_targets.point_shadow[shadow_slot] == 0) continue;
        const auto& pl = snap.point_lights[shadow_slot];
        // Camera-Relative: point light ä½ç½®è½¬æ¢åˆ°ç›¸æœºç›¸å¯¹ç©ºé—´
        const glm::vec3 pl_pos_relative = pl.position - ctx_.camera_offset;
        const glm::mat4 light_proj = clip_correction * glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, std::max(1.0f, pl.radius));
        static const glm::vec3 face_directions[6] = {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, -1.0f)
        };
        static const glm::vec3 face_ups[6] = {
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f)
        };

        for (int face = 0; face < 6; ++face) {
            const glm::mat4 light_view_mat = glm::lookAt(pl_pos_relative, pl_pos_relative + face_directions[face], face_ups[face]);
            cmd_buffer.BeginRenderPass({ctx_.render_targets.point_shadow[shadow_slot], glm::vec4(1.0f), true, face});
            FrameContext frame{light_view_mat, light_proj};
            cmd_buffer.BindPipeline(ctx_.pipeline_states.shadow);

            if (use_gpu_indirect) {
                auto* rhi = ctx_.rhi_device;
                rhi->SetupGPUDrivenShadowShader(light_view_mat, light_proj);
                rhi->BindGpuBuffer(ctx_.gpu_instance_ssbo, dse::render::gpu_driven::kSSBOBindingInstances);
                rhi->BindMegaVAO(ctx_.gpu_mega_vao);
                rhi->MultiDrawIndexedIndirect(ctx_.gpu_draw_cmd_ssbo.raw(),
                                              ctx_.gpu_indirect_draw_count,
                                              sizeof(DrawElementsIndirectCommand));
                rhi->UnbindVAO();
            }

            if (ctx_.render_scene) {
                ctx_.render_scene->DrawOpaqueCpu(cmd_buffer, *ctx_.rhi_device, *ctx_.mesh_renderer, frame);
            }
            RenderScenePassContext pass_ctx;
            pass_ctx.world = ctx_.world;
            pass_ctx.view = &light_view_mat;
            pass_ctx.projection = &light_proj;
            pass_ctx.camera_offset = ctx_.camera_offset;
            pass_ctx.cascade_index = CSM_CASCADES + 1 + face;
            ExecuteSceneRenderers(ctx_.render_scene, SceneRenderStage::Shadow, cmd_buffer, pass_ctx);
            cmd_buffer.EndRenderPass();
        }

        cmd_buffer.BindGlobalPointShadowMap(static_cast<unsigned int>(shadow_slot), ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.point_shadow[shadow_slot]));
    }
}

// ============================================================
// ForwardScenePass
// ============================================================

void ForwardScenePass::Setup(RenderGraph& graph) {
    auto prez_depth   = graph.DeclareResource("prez_depth");
    auto shadow_depth = graph.DeclareResource("shadow_depth");
    auto spot_shadow  = graph.DeclareResource("spot_shadow");
    auto point_shadow = graph.DeclareResource("point_shadow");
    auto scene_color  = graph.DeclareResource("scene_color");
    auto scene_depth  = graph.DeclareResource("scene_depth");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, prez_depth);
    graph.PassRead(pass, shadow_depth);
    graph.PassRead(pass, spot_shadow);
    graph.PassRead(pass, point_shadow);
    graph.PassWrite(pass, scene_color);
    graph.PassWrite(pass, scene_depth);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void ForwardScenePass::Execute(CommandBuffer& cmd_buffer) {
    // Editor camera: use editor_bg_color (theme-aware, set by editor each frame).
    // Game camera: intentional black so the skybox/camera fill is visually dominant.
    const glm::vec4 bg_color = (ctx_.editor_mode && ctx_.use_editor_camera)
        ? ctx_.editor_bg_color
        : glm::vec4(0.02f, 0.02f, 0.02f, 1.0f);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, bg_color, true});

    const auto& snap = *ctx_.snapshot;
    bool render_3d = false;
    glm::mat4 gpu_view = glm::mat4(1.0f);
    glm::mat4 gpu_proj = glm::mat4(1.0f);
    glm::vec3 gpu_camera_pos = glm::vec3(0.0f);
    FrameContext frame;

    // Editor camera override: use editor view/proj for Scene render target
    if (ctx_.editor_mode && ctx_.use_editor_camera) {
        render_3d = true;
        const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        // Camera-Relative: åœºæ™¯å·²å‡åŽ» camera_offsetï¼Œeditor view éœ€é…å¥—è°ƒæ•´
        gpu_view       = ctx_.editor_view * glm::translate(glm::mat4(1.0f), ctx_.camera_offset);
        gpu_proj       = clip_correction * ctx_.editor_projection;
        gpu_camera_pos = glm::vec3(glm::inverse(ctx_.editor_view)[3]) - ctx_.camera_offset;
        frame.view = gpu_view; frame.projection = gpu_proj;

        if (snap.skybox.valid) {
            skybox_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, snap.skybox.cubemap_handle,
                                  gpu_view, gpu_proj);
        }
    } else if (snap.camera_3d.valid) {
        render_3d = true;
        const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        glm::mat4 projection = clip_correction * glm::perspective(glm::radians(snap.camera_3d.fov),
                                                static_cast<float>(Screen::width()) / static_cast<float>(Screen::height()),
                                                snap.camera_3d.near_clip, snap.camera_3d.far_clip);

        if (ctx_.taa_active) {
            projection[2][0] += ctx_.taa_jitter.x * 2.0f;
            projection[2][1] += ctx_.taa_jitter.y * 2.0f;
        }

        gpu_proj = projection;
        // Camera-Relative: ç›¸æœºåœ¨åŽŸç‚¹
        gpu_camera_pos = glm::vec3(0.0f);
        gpu_view = snap.camera_3d.view;
        frame.view = gpu_view; frame.projection = projection;

        if (snap.skybox.valid) {
            const glm::mat4 skybox_view = snap.skybox.has_transform
                ? gpu_view * glm::mat4_cast(glm::conjugate(snap.skybox.rotation))
                : gpu_view;
            skybox_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, snap.skybox.cubemap_handle,
                                  skybox_view, projection);
        }
    } else if (snap.camera_2d.valid) {
        const glm::mat4 clip_correction_2d = ctx_.rhi_device->GetProjectionCorrection();
        frame.view = snap.camera_2d.view; frame.projection = clip_correction_2d * snap.camera_2d.projection;
    }
    ctx_.frame_camera = frame;

    if (render_3d) {
        cmd_buffer.BindPipeline(ctx_.pipeline_states.mesh);

        // ç¼–è¾‘å™¨åœºæ™¯è§†å›¾æ¨¡å¼ (ä»…åœ¨ editor_mode ä¸‹ç”Ÿæ•ˆ)
        // 0=Shaded, 1=Wireframe, 2=ShadedWireframe, 3=Unlit, 4=Overdraw
        const int view_mode = ctx_.editor_mode ? ctx_.scene_view_mode : 0;
        if (view_mode == 1) {
            ctx_.rhi_device->SetWireframeMode(true);
        }
        // ShadedWireframe: ç¬¬ä¸€éæ­£å¸¸ fill æ¸²æŸ“ï¼Œçº¿æ¡†å åŠ åœ¨ mesh æ¸²æŸ“ç»“æŸåŽ
        if (view_mode == 3 || view_mode == 4) {
            ctx_.rhi_device->SetForceUnlit(true);
        }
        if (view_mode == 4) {
            ctx_.rhi_device->SetOverdrawMode(true);
        }

        // Clustered Forward+: ç»‘å®šå…‰æº SSBO å’Œ Cluster ç½‘æ ¼ SSBO
        if (ctx_.light_buffer) ctx_.light_buffer->Bind();
        if (ctx_.cluster_grid) ctx_.cluster_grid->Bind();

        // GPU Driven Indirect Drawï¼šmega VAO å°±ç»ªä¸”æœ‰ draw commands æ—¶ä½¿ç”¨
        // eligible å®žä½“èµ° GPU indirectï¼›non-eligible å®žä½“ç”± OnRenderScene per-item æ¸²æŸ“
        // Render() ä¸­ IsGPUDrivenEligible è·³è¿‡ eligible å®žä½“ï¼Œä¿è¯æ— åŒé‡ç»˜åˆ¶
        const bool use_gpu_indirect = ctx_.gpu_driven_active_this_frame
            && ctx_.gpu_mega_vao
            && ctx_.gpu_draw_cmd_ssbo
            && ctx_.gpu_indirect_draw_count > 0;


        if (use_gpu_indirect) {
            glm::vec3 gpu_light_dir   = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
            glm::vec3 gpu_light_color = glm::vec3(1.0f, 1.0f, 1.0f);
            float gpu_light_intensity = 1.0f;
            float gpu_ambient         = 0.1f;
            float gpu_shadow_strength = 0.0f;
            if (snap.directional_light.valid) {
                gpu_light_dir       = glm::normalize(snap.directional_light.direction);
                gpu_light_color     = snap.directional_light.color;
                gpu_light_intensity = snap.directional_light.intensity;
                gpu_ambient         = snap.directional_light.ambient_intensity;
                if (snap.directional_light.cast_shadow) gpu_shadow_strength = snap.directional_light.shadow_strength;
            }

            auto* rhi = ctx_.rhi_device;
            rhi->SetupGPUDrivenPBRShader(gpu_view, gpu_proj, gpu_camera_pos,
                                          gpu_light_dir, gpu_light_color,
                                          gpu_light_intensity, gpu_ambient,
                                          gpu_shadow_strength);

            // ç»‘å®š instance SSBO ä¾› vertex shader è¯»å– model matrix
            rhi->BindGpuBuffer(ctx_.gpu_instance_ssbo, dse::render::gpu_driven::kSSBOBindingInstances);
            // ç»‘å®š material SSBO ä¾› fragment shader è¯»å– per-instance æè´¨
            if (ctx_.gpu_material_ssbo) {
                rhi->BindGpuBuffer(ctx_.gpu_material_ssbo, dse::render::gpu_driven::kSSBOBindingMaterials);
            }
            rhi->BindMegaVAO(ctx_.gpu_mega_vao);

            // Phase 5: æŒ‰çº¹ç†æ¡¶ç»˜åˆ¶ â€” æ¯æ¡¶ç»‘å®šçº¹ç†åŽ indirect draw
            if (ctx_.gpu_texture_buckets && ctx_.gpu_texture_bucket_count > 0) {
                const size_t stride = sizeof(DrawElementsIndirectCommand);
                for (int bi = 0; bi < ctx_.gpu_texture_bucket_count; ++bi) {
                    const auto& bucket = ctx_.gpu_texture_buckets[bi];
                    // per-bucket PerMaterial æ›´æ–°ï¼ˆDX11/VK: æ›´æ–° cbuffer/UBO; GL: no-op, ç”¨ MaterialSSBOï¼‰
                    if (ctx_.gpu_materials && bucket.material_id < static_cast<uint32_t>(ctx_.gpu_material_count)) {
                        rhi->UpdateGPUDrivenMaterial(&ctx_.gpu_materials[bucket.material_id]);
                    }
                    rhi->BindGPUDrivenTextures(bucket.textures.albedo,
                                                bucket.textures.normal,
                                                bucket.textures.metallic_roughness,
                                                bucket.textures.emissive,
                                                bucket.textures.occlusion);
                    rhi->MultiDrawIndexedIndirect(
                        ctx_.gpu_draw_cmd_ssbo.raw(),
                        static_cast<int>(bucket.cmd_count),
                        stride,
                        bucket.cmd_offset * stride);
                }
            } else {
                rhi->BindGPUDrivenTextures(0, 0, 0, 0, 0);
                rhi->MultiDrawIndexedIndirect(ctx_.gpu_draw_cmd_ssbo.raw(),
                                              ctx_.gpu_indirect_draw_count,
                                              sizeof(DrawElementsIndirectCommand));
            }
            rhi->UnbindVAO();
        }

        const glm::mat4 scene_clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        RenderScenePassContext scene_pass_ctx;
        scene_pass_ctx.world = ctx_.world;
        scene_pass_ctx.view = &gpu_view;
        scene_pass_ctx.projection = &gpu_proj;
        scene_pass_ctx.camera_offset = ctx_.camera_offset;
        scene_pass_ctx.clip_correction = &scene_clip_correction;
        if (ctx_.render_scene) {
            ctx_.render_scene->DrawOpaqueCpu(cmd_buffer, *ctx_.rhi_device, *ctx_.mesh_renderer, frame);
        }
        ExecuteSceneRenderers(ctx_.render_scene, SceneRenderStage::Opaque, cmd_buffer, scene_pass_ctx);

        // ShadedWireframe: æ­£å¸¸æ¸²æŸ“å·²å®Œæˆï¼Œå åŠ ä¸€éçº¿æ¡†
        if (view_mode == 2) {
            ctx_.rhi_device->SetWireframeMode(true);
            // GPU Driven è·¯å¾„ï¼šé‡æ–° indirect drawï¼ˆwireframe overlayï¼‰
            if (use_gpu_indirect) {
                auto* rhi = ctx_.rhi_device;
                rhi->BindGpuBuffer(ctx_.gpu_instance_ssbo, dse::render::gpu_driven::kSSBOBindingInstances);
                rhi->BindMegaVAO(ctx_.gpu_mega_vao);
                rhi->MultiDrawIndexedIndirect(ctx_.gpu_draw_cmd_ssbo.raw(),
                                              ctx_.gpu_indirect_draw_count,
                                              sizeof(DrawElementsIndirectCommand));
                rhi->UnbindVAO();
            }
            if (ctx_.render_scene) {
                ctx_.render_scene->DrawOpaqueCpu(cmd_buffer, *ctx_.rhi_device, *ctx_.mesh_renderer, frame);
            }
            ExecuteSceneRenderers(ctx_.render_scene, SceneRenderStage::Opaque, cmd_buffer, scene_pass_ctx);
            ctx_.rhi_device->SetWireframeMode(false);
        }

        // æ¢å¤åœºæ™¯è§†å›¾æ¨¡å¼ä¿®æ”¹çš„ RHI çŠ¶æ€
        if (view_mode == 1) {
            ctx_.rhi_device->SetWireframeMode(false);
        }
        if (view_mode == 3 || view_mode == 4) {
            ctx_.rhi_device->SetForceUnlit(false);
        }
        if (view_mode == 4) {
            ctx_.rhi_device->SetOverdrawMode(false);
        }
    }


    cmd_buffer.BindPipeline(ctx_.pipeline_states.sprite);
    if (ctx_.render_2d_scene) {
        ctx_.render_2d_scene(*ctx_.world, cmd_buffer, ctx_.frame_camera);
    }
    cmd_buffer.EndRenderPass();
}

// ============================================================
// BloomPass
// ============================================================


} // namespace render
} // namespace dse


