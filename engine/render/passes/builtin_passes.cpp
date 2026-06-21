/**
 * @file builtin_passes.cpp
 * @brief 引擎内置渲染 Pass 实现
 *
 * --- Section Index ---
 *   PreZPass                    ~91
 *   CSMShadowPass               ~139
 *   SpotShadowPass              ~200
 *   PointShadowPass             ~252
 *   GBufferPass                 ~310
 *   DeferredLightingPass        ~397
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

// ============================================================
// 公共工具：方向光阴影相机计算（CSMShadowPass / RSMRenderPass 共用）
// ============================================================
namespace {

glm::vec3 FindShadowCenter(const dse::render::RenderThinSnapshot& snapshot) {
    return snapshot.camera_3d.shadow_center;
}

struct DirectionalLightCamera {
    glm::mat4 view;
    glm::mat4 projection;
};

/// 计算级联视锥切片在光源空间的正交半宽（用于 PSSM 阴影盒拟合）
float ComputeCascadeOrthoSize(
        const glm::mat4& inv_view,
        const glm::vec3& light_direction,
        float split_near,
        float split_far,
        float aspect,
        float tan_half_fov) {
    const glm::mat4 light_view = glm::lookAt(
        glm::vec3(0.0f), -glm::normalize(light_direction), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec3 min_ls(1e9f);
    glm::vec3 max_ls(-1e9f);

    const float near_planes[2] = {split_near, split_far};
    for (float plane_dist : near_planes) {
        const float half_h = plane_dist * tan_half_fov;
        const float half_w = half_h * aspect;
        const glm::vec3 view_corners[4] = {
            {-half_w, -half_h, -plane_dist},
            { half_w, -half_h, -plane_dist},
            { half_w,  half_h, -plane_dist},
            {-half_w,  half_h, -plane_dist},
        };
        for (const auto& vc : view_corners) {
            const glm::vec3 world = glm::vec3(inv_view * glm::vec4(vc, 1.0f));
            const glm::vec3 ls = glm::vec3(light_view * glm::vec4(world, 1.0f));
            min_ls = glm::min(min_ls, ls);
            max_ls = glm::max(max_ls, ls);
        }
    }

    const float extent_x = (max_ls.x - min_ls.x) * 0.5f;
    const float extent_y = (max_ls.y - min_ls.y) * 0.5f;
    return std::max(extent_x, extent_y) * 1.05f;
}

DirectionalLightCamera ComputeDirectionalLightCamera(
        const glm::vec3& shadow_center,
        const glm::vec3& light_direction,
        float ortho_size,
        const glm::mat4& clip_correction,
        float shadow_map_res = 0.0f) {
    float far_dist = ortho_size * 4.0f;
    glm::vec3 light_dir_n = glm::normalize(light_direction);

    glm::vec3 center = shadow_center;
    if (shadow_map_res > 0.0f) {
        float texel_world_size = (2.0f * ortho_size) / shadow_map_res;
        glm::mat4 lv = glm::lookAt(
            shadow_center - light_dir_n * (far_dist * 0.5f),
            shadow_center, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::vec4 sc_ls = lv * glm::vec4(shadow_center, 1.0f);
        sc_ls.x = std::floor(sc_ls.x / texel_world_size) * texel_world_size;
        sc_ls.y = std::floor(sc_ls.y / texel_world_size) * texel_world_size;
        center = glm::vec3(glm::inverse(lv) * sc_ls);
    }

    glm::vec3 light_pos = center - light_dir_n * (far_dist * 0.5f);
    return {
        glm::lookAt(light_pos, center, glm::vec3(0.0f, 1.0f, 0.0f)),
        clip_correction * glm::ortho(-ortho_size, ortho_size, -ortho_size, ortho_size, 1.0f, far_dist)
    };
}

} // anonymous namespace

// 调用模块注册的强类型场景贡献对象（ISceneRenderer）的指定阶段。
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

        // TAA jitter 必须与 ForwardScenePass 一致，否则 PreZ 深度与主 pass 不匹配导致闪烁
        if (ctx_.taa_active) {
            projection[2][0] += ctx_.taa_jitter.x * 2.0f;
            projection[2][1] += ctx_.taa_jitter.y * 2.0f;
        }

        cmd_buffer.SetCamera(snap.camera_3d.view, projection);
        cmd_buffer.SetPipelineState(ctx_.pipeline_states.prez);

        // GPU-driven PreZ: eligible 实体 depth-only indirect draw
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
            ctx_.render_scene->DrawOpaqueCpu(cmd_buffer);
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
    // Camera-Relative: shadow_center 转换到相机相对空间
    glm::vec3 shadow_center = FindShadowCenter(snap) - ctx_.camera_offset;

    const bool use_gpu_indirect = ctx_.gpu_driven_active_this_frame
        && ctx_.gpu_mega_vao
        && ctx_.gpu_draw_cmd_ssbo
        && ctx_.gpu_indirect_draw_count > 0;

    std::vector<float> cascade_splits(CSM_CASCADES);

    // Atlas layout: cascade 0 (2048×2048) at (0,0); cascade 1 (1024×1024) at (2048,0); cascade 2 (512×512) at (3072,0)
    constexpr int kAtlasOffsetX[CSM_CASCADES] = {0, 2048, 3072};
    constexpr int kAtlasOffsetY[CSM_CASCADES] = {0, 0, 0};
    constexpr int kShadowRes[CSM_CASCADES] = {2048, 1024, 512};
    constexpr float kAtlasWidth = 4096.0f;
    constexpr float kAtlasHeight = 2048.0f;

    // 级联分裂距离：PSSM 与组件手动 cascade_splits 按 lambda 混合
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

    // 单次 BeginRenderPass 绑定 atlas RT，全量清除深度
    {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.shadow_atlas, glm::vec4(1.0f), true});
        cmd_buffer.SetPipelineState(ctx_.pipeline_states.shadow);

        float prev_split = cam_near;
        for (int i = 0; i < CSM_CASCADES; ++i) {
            const float split_far_plane = cascade_splits[i];
            float size = ComputeCascadeOrthoSize(
                inv_view, dl.direction, prev_split, split_far_plane, aspect, tan_half_fov);
            prev_split = split_far_plane;
            auto cam = ComputeDirectionalLightCamera(
                shadow_center, dl.direction, size, clip_correction, static_cast<float>(kShadowRes[i]));

            float far_dist = size * 4.0f;
            glm::mat4 sample_proj = shadow_sample_correction *
                glm::ortho(-size, size, -size, size, 1.0f, far_dist);

            cached_light_space_[i] = sample_proj * cam.view;

            // 设置 viewport 到 atlas 内对应区域
            cmd_buffer.SetViewport(kAtlasOffsetX[i], kAtlasOffsetY[i], kShadowRes[i], kShadowRes[i]);
            cmd_buffer.SetCamera(cam.view, cam.projection);

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
                    ctx_.render_scene->DrawOpaqueCpu(cmd_buffer);
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

    // 使用缓存的 light space matrix（与 shadow map 内容对应的矩阵）
    for (int i = 0; i < CSM_CASCADES; ++i) {
        ctx_.rhi_device->SetGlobalLightSpaceMatrix(static_cast<unsigned int>(i), cached_light_space_[i]);
        ctx_.rhi_device->SetGlobalCascadeSplit(static_cast<unsigned int>(i), cascade_splits[i]);
    }

    // Atlas region UV: (scale_x, scale_y, offset_x, offset_y) 供 PBR shader 采样
    for (int i = 0; i < CSM_CASCADES; ++i) {
        float scale_x = static_cast<float>(kShadowRes[i]) / kAtlasWidth;
        float scale_y = static_cast<float>(kShadowRes[i]) / kAtlasHeight;
        float offset_x = static_cast<float>(kAtlasOffsetX[i]) / kAtlasWidth;
        float offset_y = static_cast<float>(kAtlasOffsetY[i]) / kAtlasHeight;
        ctx_.rhi_device->SetGlobalShadowAtlasRegion(static_cast<unsigned int>(i),
            glm::vec4(scale_x, scale_y, offset_x, offset_y));
    }

    // 绑定单个 atlas 深度纹理到所有 shadow map slot
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

        // Camera-Relative: spot light 位置转换到相机相对空间
        const glm::vec3 sl_pos_relative = sl.position - ctx_.camera_offset;
        const glm::mat4 light_view_mat = glm::lookAt(sl_pos_relative, sl_pos_relative + sl.forward, sl.up);
        const glm::mat4 light_proj = clip_correction * glm::perspective(glm::radians(sl.outer_cone_angle * 2.0f), 1.0f, 0.1f, std::max(1.0f, sl.radius));
        cmd_buffer.BeginRenderPass({ctx_.render_targets.spot_shadow[i], glm::vec4(1.0f), true});
        cmd_buffer.SetCamera(light_view_mat, light_proj);
        cmd_buffer.SetPipelineState(ctx_.pipeline_states.shadow);

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
            ctx_.render_scene->DrawOpaqueCpu(cmd_buffer);
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
        // Camera-Relative: point light 位置转换到相机相对空间
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
            cmd_buffer.BeginRenderPass({ctx_.render_targets.point_shadow[shadow_slot], glm::vec4(1.0f), true});
            cmd_buffer.SetCamera(light_view_mat, light_proj);
            cmd_buffer.SetPipelineState(ctx_.pipeline_states.shadow);

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
                ctx_.render_scene->DrawOpaqueCpu(cmd_buffer);
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
// GBufferPass (deferred geometry)
// ============================================================

void GBufferPass::Setup(RenderGraph& graph) {
    auto prez_depth    = graph.DeclareResource("prez_depth");
    auto gbuffer_color = graph.DeclareResource("gbuffer_color");
    auto gbuffer_depth = graph.DeclareResource("gbuffer_depth");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, prez_depth);
    graph.PassWrite(pass, gbuffer_color);
    graph.PassWrite(pass, gbuffer_depth);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void GBufferPass::Execute(CommandBuffer& cmd_buffer) {
    if (ctx_.render_targets.gbuffer == 0) return;

    cmd_buffer.BeginRenderPass({ctx_.render_targets.gbuffer, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), true});

    ctx_.rhi_device->SetGBufferRenderingMode(true);

    // 选择相机（与 ForwardScenePass 相同逻辑）
    const auto& snap = *ctx_.snapshot;
    bool render_3d = false;
    if (ctx_.editor_mode && ctx_.use_editor_camera) {
        render_3d = true;
        const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        cmd_buffer.SetCamera(ctx_.editor_view, clip_correction * ctx_.editor_projection);
    } else if (snap.camera_3d.valid) {
        render_3d = true;
        const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        glm::mat4 projection = clip_correction * glm::perspective(glm::radians(snap.camera_3d.fov),
                                                static_cast<float>(Screen::width()) / static_cast<float>(Screen::height()),
                                                snap.camera_3d.near_clip, snap.camera_3d.far_clip);
        cmd_buffer.SetCamera(snap.camera_3d.view, projection);
    }

    // NOTE: GBuffer module rendering 跳过 — DeferredLightingPass 输出未被任何后续 pass 消费。
    // 保留 RT clear + global texture 注册以维持 API 安全；如未来新增 deferred consumer 需恢复此路径。
    (void)render_3d;

    ctx_.rhi_device->SetGBufferRenderingMode(false);
    cmd_buffer.EndRenderPass();

    // 将 GBuffer MRT 纹理注册为全局 GBuffer 纹理
    for (int i = 0; i < 3; ++i) {
        unsigned int tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.gbuffer, i);
        ctx_.rhi_device->SetGlobalGBufferTexture(static_cast<unsigned int>(i), tex);
    }
}

// ============================================================
// DeferredLightingPass
// ============================================================

void DeferredLightingPass::Setup(RenderGraph& graph) {
    auto gbuffer_color      = graph.DeclareResource("gbuffer_color");
    auto shadow_depth       = graph.DeclareResource("shadow_depth");
    auto deferred_lit_color = graph.DeclareResource("deferred_lit_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, gbuffer_color);
    graph.PassRead(pass, shadow_depth);
    graph.PassWrite(pass, deferred_lit_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void DeferredLightingPass::Execute(CommandBuffer& cmd_buffer) {
    // NOTE: GBuffer module rendering 已跳过（GBufferPass 仅 clear），且 deferred_lighting 输出
    // 未被任何后续 pass 消费。跳过此 pass 避免对空 GBuffer 做无用 fullscreen draw。
    // 如未来新增 deferred consumer，需同步恢复 GBufferPass 渲染与此 pass。
    if (ctx_.render_targets.deferred_lighting == 0 || ctx_.render_targets.gbuffer == 0) return;

    // 仍然 clear RT 以保持 API 安全（全局纹理注册不会读到上帧残留）
    cmd_buffer.BeginRenderPass({ctx_.render_targets.deferred_lighting, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), true});
    cmd_buffer.EndRenderPass();
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

    // Editor camera override: use editor view/proj for Scene render target
    if (ctx_.editor_mode && ctx_.use_editor_camera) {
        render_3d = true;
        const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        // Camera-Relative: 场景已减去 camera_offset，editor view 需配套调整
        gpu_view       = ctx_.editor_view * glm::translate(glm::mat4(1.0f), ctx_.camera_offset);
        gpu_proj       = clip_correction * ctx_.editor_projection;
        gpu_camera_pos = glm::vec3(glm::inverse(ctx_.editor_view)[3]) - ctx_.camera_offset;
        cmd_buffer.SetCamera(gpu_view, gpu_proj);

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
        // Camera-Relative: 相机在原点
        gpu_camera_pos = glm::vec3(0.0f);
        gpu_view = snap.camera_3d.view;
        cmd_buffer.SetCamera(gpu_view, projection);

        if (snap.skybox.valid) {
            const glm::mat4 skybox_view = snap.skybox.has_transform
                ? gpu_view * glm::mat4_cast(glm::conjugate(snap.skybox.rotation))
                : gpu_view;
            skybox_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, snap.skybox.cubemap_handle,
                                  skybox_view, projection);
        }
    } else if (snap.camera_2d.valid) {
        const glm::mat4 clip_correction_2d = ctx_.rhi_device->GetProjectionCorrection();
        cmd_buffer.SetCamera(snap.camera_2d.view, clip_correction_2d * snap.camera_2d.projection);
    }

    if (render_3d) {
        cmd_buffer.SetPipelineState(ctx_.pipeline_states.mesh);

        // 编辑器场景视图模式 (仅在 editor_mode 下生效)
        // 0=Shaded, 1=Wireframe, 2=ShadedWireframe, 3=Unlit, 4=Overdraw
        const int view_mode = ctx_.editor_mode ? ctx_.scene_view_mode : 0;
        if (view_mode == 1) {
            ctx_.rhi_device->SetWireframeMode(true);
        }
        // ShadedWireframe: 第一遍正常 fill 渲染，线框叠加在 mesh 渲染结束后
        if (view_mode == 3 || view_mode == 4) {
            ctx_.rhi_device->SetForceUnlit(true);
        }
        if (view_mode == 4) {
            ctx_.rhi_device->SetOverdrawMode(true);
        }

        // Clustered Forward+: 绑定光源 SSBO 和 Cluster 网格 SSBO
        if (ctx_.light_buffer) ctx_.light_buffer->Bind();
        if (ctx_.cluster_grid) ctx_.cluster_grid->Bind();

        // GPU Driven Indirect Draw：mega VAO 就绪且有 draw commands 时使用
        // eligible 实体走 GPU indirect；non-eligible 实体由 OnRenderScene per-item 渲染
        // Render() 中 IsGPUDrivenEligible 跳过 eligible 实体，保证无双重绘制
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

            // 绑定 instance SSBO 供 vertex shader 读取 model matrix
            rhi->BindGpuBuffer(ctx_.gpu_instance_ssbo, dse::render::gpu_driven::kSSBOBindingInstances);
            // 绑定 material SSBO 供 fragment shader 读取 per-instance 材质
            if (ctx_.gpu_material_ssbo) {
                rhi->BindGpuBuffer(ctx_.gpu_material_ssbo, dse::render::gpu_driven::kSSBOBindingMaterials);
            }
            rhi->BindMegaVAO(ctx_.gpu_mega_vao);

            // Phase 5: 按纹理桶绘制 — 每桶绑定纹理后 indirect draw
            if (ctx_.gpu_texture_buckets && ctx_.gpu_texture_bucket_count > 0) {
                const size_t stride = sizeof(DrawElementsIndirectCommand);
                for (int bi = 0; bi < ctx_.gpu_texture_bucket_count; ++bi) {
                    const auto& bucket = ctx_.gpu_texture_buckets[bi];
                    // per-bucket PerMaterial 更新（DX11/VK: 更新 cbuffer/UBO; GL: no-op, 用 MaterialSSBO）
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
            ctx_.render_scene->DrawOpaqueCpu(cmd_buffer);
        }
        ExecuteSceneRenderers(ctx_.render_scene, SceneRenderStage::Opaque, cmd_buffer, scene_pass_ctx);

        // ShadedWireframe: 正常渲染已完成，叠加一遍线框
        if (view_mode == 2) {
            ctx_.rhi_device->SetWireframeMode(true);
            // GPU Driven 路径：重新 indirect draw（wireframe overlay）
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
                ctx_.render_scene->DrawOpaqueCpu(cmd_buffer);
            }
            ExecuteSceneRenderers(ctx_.render_scene, SceneRenderStage::Opaque, cmd_buffer, scene_pass_ctx);
            ctx_.rhi_device->SetWireframeMode(false);
        }

        // 恢复场景视图模式修改的 RHI 状态
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


    cmd_buffer.SetPipelineState(ctx_.pipeline_states.sprite);
    if (ctx_.render_2d_scene) {
        ctx_.render_2d_scene(*ctx_.world, cmd_buffer);
    }
    cmd_buffer.EndRenderPass();
}

// ============================================================
// BloomPass
// ============================================================

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
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
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
        cmd_buffer.DrawPostProcess({"bloom_downsample", current_src, {static_cast<float>(mip_w * 2), static_cast<float>(mip_h * 2)}});
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
        cmd_buffer.DrawPostProcess({"bloom_upsample", current_src, {mip_texel, pp_config.bloom_mip_weight}});
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
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.sprite);
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

    // 获取 SSAO 纹理（如果启用）
    unsigned int ssao_tex = 0;
    if (ctx_.pipeline_features.ssao && pp_enabled && pp_config.ssao_enabled && ctx_.render_targets.ssao_blur != 0) {
        ssao_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.ssao_blur);
    }

    // 获取 Contact Shadow 纹理（如果启用）
    unsigned int contact_shadow_tex = 0;
    if (ctx_.pipeline_features.contact_shadow && pp_enabled && pp_config.contact_shadow_enabled && ctx_.render_targets.contact_shadow != 0) {
        contact_shadow_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.contact_shadow);
    }

    // 获取 auto exposure 纹理（如果启用）
    unsigned int ae_tex = 0;
    if (ctx_.pipeline_features.auto_exposure && ctx_.auto_exposure_active) {
        // ping-pong 已翻转，当前帧结果在 1 - current_index
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

    // bloom_composite 是历史 effect name，当前实际承担 final composite 职责。
    // 以下为 final composite 参数布局（按固定顺序传递到三后端）:
    // [0] bloom_tex_handle
    // [1] manual_exposure
    // [2] bloom_intensity
    // [3] bloom_enabled
    // [4] ssao_tex_handle
    // [5] auto_exposure_tex_handle
    // [6] lut_tex_handle
    // [7] lut_intensity
    // [8] contact_shadow_tex_handle
    // [9] contact_shadow_strength
    // [10] vignette_enabled
    // [11] vignette_intensity
    // [12] vignette_radius
    // [13] vignette_softness
    // [14] film_grain_enabled
    // [15] film_grain_intensity
    // [16] film_grain_time
    float film_grain_time = 0.0f;
    if (pp_enabled && pp_config.film_grain_enabled && pp_config.film_grain_intensity > 0.0f) {
        film_grain_time = static_cast<float>(std::fmod(Time::TimeSinceStartup() * pp_config.film_grain_time_scale, 4096.0f));
    }

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.main, glm::vec4(0.0f), true});

    const bool bloom_enabled = ctx_.pipeline_features.bloom && pp_config.bloom_enabled;
    if (pp_enabled && (bloom_enabled || contact_shadow_tex != 0 || pp_config.vignette_enabled || pp_config.film_grain_enabled)) {
        const unsigned int bloom_tex = (bloom_enabled && !ctx_.render_targets.bloom_mips.empty())
            ? ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.bloom_mips[0])
            : 0;
        cmd_buffer.DrawPostProcess({"bloom_composite", scene_color_tex, {
            static_cast<float>(bloom_tex),
            pp_config.exposure,
            ctx_.pipeline_overrides.bloom_intensity >= 0.0f
                ? ctx_.pipeline_overrides.bloom_intensity
                : pp_config.bloom_intensity,
            bloom_enabled ? 1.0f : 0.0f,
            static_cast<float>(ssao_tex),
            static_cast<float>(ae_tex),
            lut_tex,
            lut_intensity,
            static_cast<float>(contact_shadow_tex),
            pp_config.contact_shadow_strength,
            pp_config.vignette_enabled ? 1.0f : 0.0f,
            pp_config.vignette_intensity,
            pp_config.vignette_radius,
            pp_config.vignette_softness,
            pp_config.film_grain_enabled ? 1.0f : 0.0f,
            pp_config.film_grain_intensity,
            film_grain_time
        }});
    } else {
        if (ssao_tex != 0) {
            cmd_buffer.DrawPostProcess(PostProcessRequest{"ssao_apply", scene_color_tex,
                {pp_config.exposure, lut_intensity}}
                .Tex(2, ssao_tex).Tex(3, ae_tex).Tex3D(5, static_cast<unsigned int>(lut_tex)));
        } else {
            cmd_buffer.DrawPostProcess(PostProcessRequest{"tonemapping", scene_color_tex,
                {pp_config.exposure, lut_intensity}}
                .Tex(2, ae_tex).Tex3D(5, static_cast<unsigned int>(lut_tex)));
        }
    }

    if (ui_color_tex != 0) {
        cmd_buffer.DrawPostProcess({"ui_overlay", ui_color_tex});
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

    // Pass 1: 场景 → 64x64 log luminance (8x8 采样网格)
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.lum_temp, glm::vec4(0.0f), true});
    post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"lum_compute", scene_color_tex});
    cmd_buffer.EndRenderPass();

    // Pass 2: 64x64 → 1x1 adapted exposure (EMA blend with previous frame)
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

    // 翻转 ping-pong
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

    // Pass 1: SSAO 计算（半分辨率）
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

    // Pass 2: 双边模糊
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

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
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
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
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

    // 读取 motion vector 纹理（如果可用）
    const unsigned int mv_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.motion_vector);

    // 历史帧读取来自上一帧写入的 RT
    const int read_idx = 1 - history_index_;
    const unsigned int history_tex = has_valid_history_
        ? ctx_.rhi_device->GetRenderTargetColorTexture(history_rt_[read_idx])
        : 0;

    // TAA resolve：写入当前帧的 history RT（直接做输出，省掉 copy）
    const int write_idx = history_index_;
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({history_rt_[write_idx], glm::vec4(0.0f), true});
    cmd_buffer.DrawPostProcess(PostProcessRequest{"taa_resolve", main_color_tex, {
        blend_factor,
        current_jitter_.x,
        current_jitter_.y,
        static_cast<float>(frame_index_),
        static_cast<float>(sw),
        static_cast<float>(sh)
    }}.Tex(2, mv_tex).Tex(5, history_tex));
    cmd_buffer.EndRenderPass();

    // 将 TAA 结果 copy 到 taa RT（供 Present/FXAA 读取）
    const unsigned int taa_out_tex = ctx_.rhi_device->GetRenderTargetColorTexture(history_rt_[write_idx]);
    if (taa_out_tex != 0 && ctx_.render_targets.taa != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.taa, glm::vec4(0.0f), true});
        post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"copy", taa_out_tex});
        cmd_buffer.EndRenderPass();
    }

    // 翻转 ping-pong 索引
    history_index_ = 1 - history_index_;
    has_valid_history_ = true;
}

void TAAPass::EnsureHistoryRT(int width, int height) {
    if (width == history_width_ && height == history_height_
        && history_rt_[0] != 0 && history_rt_[1] != 0) {
        return;
    }
    // 分辨率变化或首次创建（旧 RT 由 RhiDevice 资源管理器统一回收）
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

    // Pass 1: DOF → dof RT
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

    // Pass 2: dof RT → main RT（回写）
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
        // 首帧输出零速度
        cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
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

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.motion_vector, glm::vec4(0.0f), true});
    cmd_buffer.DrawPostProcess({"motion_vector", depth_tex, params});
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

    // motion_blur 现在读 motion_vector RT 而非深度 + reproj
    // params: [0] intensity, [1] samples, [2] screen_w, [3] screen_h, [4] color_tex
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.dof, glm::vec4(0.0f), true});
    cmd_buffer.DrawPostProcess(PostProcessRequest{"motion_blur", mv_tex, {
        pp_config.motion_blur_intensity,
        static_cast<float>(pp_config.motion_blur_samples),
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height())
    }}.Tex(2, main_color_tex));
    cmd_buffer.EndRenderPass();

    // dof RT → main RT
    const unsigned int mb_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.dof);
    if (mb_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.main, glm::vec4(0.0f), true});
        post_process_renderer_.BeginFrame();
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

    // Pass 1: 渲染 SSR 到半分辨率 ssr RT
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.ssr, glm::vec4(0.0f), true});
    cmd_buffer.DrawPostProcess(PostProcessRequest{"ssr", depth_tex, {
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

    // Pass 2: 将 SSR 结果叠加到 scene RT（利用 SSR alpha 作为混合权重）
    const unsigned int ssr_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.ssr);
    if (ssr_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
        cmd_buffer.DrawPostProcess({"ui_overlay", ssr_tex});
        cmd_buffer.EndRenderPass();
    }
}

// ============================================================
// OutlinePass — 屏幕空间边缘检测描边
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

    // Pass 1: 边缘检测 → outline RT
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

    // Pass 2: 将边缘结果叠加到 scene RT
    const unsigned int outline_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.outline);
    if (outline_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
        cmd_buffer.DrawPostProcess({"ui_overlay", outline_tex});
        cmd_buffer.EndRenderPass();
    }
}

// ============================================================
// LightShaftPass — screen-space radial blur (God Ray)
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

    // params 布局（15 float）:
    // [0-1]  sun_screen_pos.xy (UV space)
    // [2-4]  light_color.rgb
    // [5]    density
    // [6]    weight
    // [7]    decay
    // [8]    exposure
    // [9]    num_samples
    // [10]   intensity
    // [11-14] reserved (pad)
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
    cmd_buffer.DrawPostProcess(PostProcessRequest{"light_shaft", scene_tex, {
        sun_uv_x, sun_uv_y,
        pp->light_shaft_color.r, pp->light_shaft_color.g, pp->light_shaft_color.b,
        pp->light_shaft_density,
        pp->light_shaft_weight,
        pp->light_shaft_decay,
        pp->light_shaft_exposure,
        static_cast<float>(pp->light_shaft_samples),
        pp->light_shaft_intensity,
        0.0f, 0.0f, 0.0f, 0.0f
    }, false, 0}.Tex(2, depth_tex));
    cmd_buffer.EndRenderPass();
}

// ============================================================
// VolumetricFogPass — 高度指数雾 + Mie 散射近似 raymarching
// ============================================================

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
    // Camera-Relative: cam_pos 为 vec3(0)（相机在原点）
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

    // params 布局（30 个 float，三后端通用）：
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
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.fog, glm::vec4(0.0f), true});
    cmd_buffer.DrawPostProcess(PostProcessRequest{"volumetric_fog", scene_tex, {
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

    // 将雾效结果（已包含 scene 颜色）覆写回 scene RT
    const unsigned int fog_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.fog);
    if (fog_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
        post_process_renderer_.BeginFrame();
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device, {"copy", fog_tex});
        cmd_buffer.EndRenderPass();
    }
}

// ============================================================
// VolumetricCloudPass — Guerrilla-style raymarching volumetric clouds
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

    // params 布局（30 个 float，三后端通用）：
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({target_rt, glm::vec4(0.0f), use_half_res});
    cmd_buffer.DrawPostProcess(PostProcessRequest{"volumetric_cloud", scene_tex, {
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
// WBOITPass — Weighted Blended Order-Independent Transparency
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
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.wboit_accum);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.wboit_accum, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), true});

    RenderScenePassContext accum_ctx;
    accum_ctx.world = ctx_.world;
    accum_ctx.clip_correction = &scene_clip_correction;
    if (ctx_.render_scene) {
        ctx_.render_scene->DrawTransparent(cmd_buffer, 1);
    }
    cmd_buffer.EndRenderPass();

    // --- Pass 2: Revealage (blend ZERO, ONE_MINUS_SRC_ALPHA) ---
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.wboit_reveal);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.wboit_reveal, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), true});

    RenderScenePassContext reveal_ctx;
    reveal_ctx.world = ctx_.world;
    reveal_ctx.clip_correction = &scene_clip_correction;
    if (ctx_.render_scene) {
        ctx_.render_scene->DrawTransparent(cmd_buffer, 2);
    }
    cmd_buffer.EndRenderPass();

    // --- Pass 3: Composite WBOIT onto scene RT ---
    const unsigned int accum_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.wboit_accum);
    const unsigned int reveal_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.wboit_reveal);
    if (accum_tex == 0 || reveal_tex == 0) return;

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.decal_blend);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
    cmd_buffer.DrawPostProcess(PostProcessRequest{"wboit_composite", accum_tex}.Tex(2, reveal_tex));
    cmd_buffer.EndRenderPass();
}

// ============================================================
// WaterPass — Screen-Space Water / Ocean (Gerstner wave + refraction)
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

    // Camera-Relative: cam_pos 在着色器中应为 vec3(0)（相机在原点）
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

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.decal_blend);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});

    // params 布局（40 float = 160 bytes）
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
        // 视觉增强参数
        params[31] = wc.caustic_intensity;    params[32] = wc.caustic_scale;
        params[33] = wc.foam_intensity;       params[34] = wc.foam_depth_threshold;
        params[35] = wc.underwater_fog_density;
        params[36] = wc.underwater_fog_color.r; params[37] = wc.underwater_fog_color.g; params[38] = wc.underwater_fog_color.b;

        cmd_buffer.DrawPostProcess(PostProcessRequest{"water", scene_tex, params}.Tex(2, depth_tex));
    }
    cmd_buffer.EndRenderPass();
}

// ============================================================
// DecalPass — Screen-Space Decal (深度重建 + 盒体投影)
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

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.decal_blend);
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

        cmd_buffer.DrawPostProcess(PostProcessRequest{"decal", scene_tex, params}
            .Tex(2, depth_tex).Tex(3, dc.albedo_texture));
    }
    cmd_buffer.EndRenderPass();
}

// ============================================================
// HiZBuildPass — 从 PreZ 深度构建 Hi-Z Mip Chain (Compute Shader)
// ============================================================

const char* kHiZCopyShaderSource = R"(
#version 430 core
#extension GL_ARB_shading_language_420pack : enable
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D u_depth_texture;
layout(r32f, binding = 0) writeonly uniform image2D u_hiz_mip0;

uniform ivec2 u_dst_size;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= u_dst_size.x || coord.y >= u_dst_size.y) return;

    vec2 uv = (vec2(coord) + 0.5) / vec2(u_dst_size);
    float depth = texture(u_depth_texture, uv).r;
    imageStore(u_hiz_mip0, coord, vec4(depth, 0.0, 0.0, 0.0));
}
)";

const char* kHiZDownsampleShaderSource = R"(
#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(r32f, binding = 0) readonly uniform image2D u_src_mip;
layout(r32f, binding = 1) writeonly uniform image2D u_dst_mip;

uniform ivec2 u_src_size;
uniform ivec2 u_dst_size;

void main() {
    ivec2 dst_coord = ivec2(gl_GlobalInvocationID.xy);
    if (dst_coord.x >= u_dst_size.x || dst_coord.y >= u_dst_size.y) return;

    ivec2 src_coord = dst_coord * 2;

    float d00 = imageLoad(u_src_mip, src_coord).r;
    float d10 = imageLoad(u_src_mip, min(src_coord + ivec2(1, 0), u_src_size - 1)).r;
    float d01 = imageLoad(u_src_mip, min(src_coord + ivec2(0, 1), u_src_size - 1)).r;
    float d11 = imageLoad(u_src_mip, min(src_coord + ivec2(1, 1), u_src_size - 1)).r;

    // Conservative: take MAX depth (farthest), so occluder test is pessimistic
    float max_depth = max(max(d00, d10), max(d01, d11));
    imageStore(u_dst_mip, dst_coord, vec4(max_depth, 0.0, 0.0, 0.0));
}
)";

// ---------------- Vulkan GLSL 450 版本（push constants + 显式 set/binding） ----------------
// binding 布局对应 CreateComputeShaderEx 参数：
//   HiZ Copy: ssbo=0, img=1, smp=1, pc=8B
//     binding 0 = storage image hiz_mip0
//     binding 1 = sampler2D depth_texture
//   HiZ Downsample: ssbo=0, img=2, smp=0, pc=16B
//     binding 0 = storage image src_mip (readonly)
//     binding 1 = storage image dst_mip (writeonly)
//   HiZ Cull: ssbo=2, img=0, smp=1, pc=96B
//     binding 0 = SSBO AABB (readonly)
//     binding 1 = SSBO Visibility (writeonly)
//     binding 2 = sampler2D hiz_texture

const char* kHiZCopyShaderSourceVK = R"(
#version 450
layout(local_size_x = 16, local_size_y = 16) in;

layout(set=0, binding=0, r32f) writeonly uniform image2D u_hiz_mip0;
layout(set=0, binding=1) uniform sampler2D u_depth_texture;

layout(push_constant) uniform PC {
    int u_dst_size_x;
    int u_dst_size_y;
} pc;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= pc.u_dst_size_x || coord.y >= pc.u_dst_size_y) return;

    vec2 uv = (vec2(coord) + 0.5) / vec2(pc.u_dst_size_x, pc.u_dst_size_y);
    float depth = texture(u_depth_texture, uv).r;
    imageStore(u_hiz_mip0, coord, vec4(depth, 0.0, 0.0, 0.0));
}
)";

const char* kHiZDownsampleShaderSourceVK = R"(
#version 450
layout(local_size_x = 16, local_size_y = 16) in;

layout(set=0, binding=0, r32f) readonly uniform image2D u_src_mip;
layout(set=0, binding=1, r32f) writeonly uniform image2D u_dst_mip;

layout(push_constant) uniform PC {
    int u_src_size_x;
    int u_src_size_y;
    int u_dst_size_x;
    int u_dst_size_y;
} pc;

void main() {
    ivec2 dst_coord = ivec2(gl_GlobalInvocationID.xy);
    if (dst_coord.x >= pc.u_dst_size_x || dst_coord.y >= pc.u_dst_size_y) return;

    ivec2 src_coord = dst_coord * 2;
    ivec2 src_max = ivec2(pc.u_src_size_x - 1, pc.u_src_size_y - 1);
    float d00 = imageLoad(u_src_mip, src_coord).r;
    float d10 = imageLoad(u_src_mip, min(src_coord + ivec2(1, 0), src_max)).r;
    float d01 = imageLoad(u_src_mip, min(src_coord + ivec2(0, 1), src_max)).r;
    float d11 = imageLoad(u_src_mip, min(src_coord + ivec2(1, 1), src_max)).r;

    float max_depth = max(max(d00, d10), max(d01, d11));
    imageStore(u_dst_mip, dst_coord, vec4(max_depth, 0.0, 0.0, 0.0));
}
)";

const char* kHiZCullShaderSourceVK = R"(
#version 450
layout(local_size_x = 64) in;

struct AABB {
    vec4 min_point;
    vec4 max_point;
};

layout(set=0, binding=0, std430) readonly buffer AABBBuffer {
    AABB aabbs[];
};

layout(set=0, binding=1, std430) writeonly buffer VisibilityBuffer {
    uint visibility[];
};

layout(set=0, binding=2) uniform sampler2D u_hiz_texture;

layout(push_constant) uniform PC {
    mat4 u_view_projection; // 64 B
    float u_screen_size_x;  //  4 B
    float u_screen_size_y;  //  4 B
    int u_mip_count;        //  4 B
    int u_object_count;     //  4 B
} pc; // 80 B total

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (int(idx) >= pc.u_object_count) return;

    vec3 aabb_min = aabbs[idx].min_point.xyz;
    vec3 aabb_max = aabbs[idx].max_point.xyz;

    vec2 ndc_min = vec2(1.0);
    vec2 ndc_max = vec2(-1.0);
    float nearest_z = 1.0;

    for (int i = 0; i < 8; ++i) {
        vec3 corner = vec3(
            ((i & 1) != 0) ? aabb_max.x : aabb_min.x,
            ((i & 2) != 0) ? aabb_max.y : aabb_min.y,
            ((i & 4) != 0) ? aabb_max.z : aabb_min.z
        );
        vec4 clip = pc.u_view_projection * vec4(corner, 1.0);
        if (clip.w <= 0.0) {
            visibility[idx] = 1u;
            return;
        }
        vec3 ndc = clip.xyz / clip.w;
        ndc_min = min(ndc_min, ndc.xy);
        ndc_max = max(ndc_max, ndc.xy);
        nearest_z = min(nearest_z, ndc.z);
    }

    vec2 uv_min = ndc_min * 0.5 + 0.5;
    vec2 uv_max = ndc_max * 0.5 + 0.5;
    uv_min = clamp(uv_min, vec2(0.0), vec2(1.0));
    uv_max = clamp(uv_max, vec2(0.0), vec2(1.0));

    if (uv_max.x <= 0.0 || uv_min.x >= 1.0 || uv_max.y <= 0.0 || uv_min.y >= 1.0) {
        visibility[idx] = 0u;
        return;
    }

    vec2 size_pixels = (uv_max - uv_min) * vec2(pc.u_screen_size_x, pc.u_screen_size_y);
    float max_dim = max(size_pixels.x, size_pixels.y);
    float mip_level = max_dim > 0.0 ? ceil(log2(max_dim)) : 0.0;
    mip_level = clamp(mip_level, 0.0, float(pc.u_mip_count - 1));

    float test_depth = nearest_z * 0.5 + 0.5 - 0.005;
    vec2 uv_center = (uv_min + uv_max) * 0.5;
    float hiz_depth = textureLod(u_hiz_texture, uv_center, mip_level).r;
    float hiz_tl = textureLod(u_hiz_texture, uv_min, mip_level).r;
    float hiz_br = textureLod(u_hiz_texture, uv_max, mip_level).r;
    float hiz_tr = textureLod(u_hiz_texture, vec2(uv_max.x, uv_min.y), mip_level).r;
    float hiz_bl = textureLod(u_hiz_texture, vec2(uv_min.x, uv_max.y), mip_level).r;
    float max_hiz = max(max(hiz_depth, hiz_tl), max(max(hiz_br, hiz_tr), hiz_bl));

    visibility[idx] = (test_depth > max_hiz) ? 0u : 1u;
}
)";

// ---------------- HLSL CS 5.0 版本（D3D11） ----------------
// HLSL register 映射:
//   HiZ Copy: cbuffer b0(PC), t0(depth sampler), s0, u0(hiz_mip0)
//   HiZ Downsample: cbuffer b0(PC), u0(src readonly via RWTexture2D), u1(dst)
//   HiZ Cull: cbuffer b0(PC), t0(hiz sampler), s0, t16(AABB SSBO SRV), u0(Visibility UAV)
// SSBO 在 DX11 中可读 = StructuredBuffer (t-register), 可写 = RWStructuredBuffer (u-register)

const char* kHiZCopyShaderSourceHLSL = R"(
cbuffer Params : register(b0) {
    int u_dst_size_x;
    int u_dst_size_y;
};
Texture2D<float> u_depth_texture : register(t0);
SamplerState LinearSampler        : register(s0);
RWTexture2D<float> u_hiz_mip0    : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    int2 coord = int2(id.xy);
    if (coord.x >= u_dst_size_x || coord.y >= u_dst_size_y) return;
    float2 uv = (float2(coord) + 0.5) / float2(u_dst_size_x, u_dst_size_y);
    float depth = u_depth_texture.SampleLevel(LinearSampler, uv, 0.0);
    u_hiz_mip0[coord] = depth;
}
)";

const char* kHiZDownsampleShaderSourceHLSL = R"(
cbuffer Params : register(b0) {
    int u_src_size_x;
    int u_src_size_y;
    int u_dst_size_x;
    int u_dst_size_y;
};
RWTexture2D<float> u_src_mip : register(u0);
RWTexture2D<float> u_dst_mip : register(u1);

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    int2 dst_coord = int2(id.xy);
    if (dst_coord.x >= u_dst_size_x || dst_coord.y >= u_dst_size_y) return;
    int2 src_coord = dst_coord * 2;
    int2 src_max = int2(u_src_size_x - 1, u_src_size_y - 1);

    float d00 = u_src_mip[src_coord];
    float d10 = u_src_mip[min(src_coord + int2(1, 0), src_max)];
    float d01 = u_src_mip[min(src_coord + int2(0, 1), src_max)];
    float d11 = u_src_mip[min(src_coord + int2(1, 1), src_max)];
    float max_depth = max(max(d00, d10), max(d01, d11));
    u_dst_mip[dst_coord] = max_depth;
}
)";

const char* kHiZCullShaderSourceHLSL = R"(
cbuffer Params : register(b0) {
    float4x4 u_view_projection;
    float u_screen_size_x;
    float u_screen_size_y;
    int u_mip_count;
    int u_object_count;
};

struct AABB {
    float4 min_point;
    float4 max_point;
};
StructuredBuffer<AABB> aabbs : register(t16);
RWStructuredBuffer<uint> visibility : register(u1);

Texture2D<float> u_hiz_texture : register(t0);
SamplerState LinearSampler      : register(s0);

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint idx = id.x;
    if (int(idx) >= u_object_count) return;

    float3 aabb_min = aabbs[idx].min_point.xyz;
    float3 aabb_max = aabbs[idx].max_point.xyz;

    float2 ndc_min = float2(1.0, 1.0);
    float2 ndc_max = float2(-1.0, -1.0);
    float nearest_z = 1.0;

    [unroll]
    for (int i = 0; i < 8; ++i) {
        float3 corner = float3(
            ((i & 1) != 0) ? aabb_max.x : aabb_min.x,
            ((i & 2) != 0) ? aabb_max.y : aabb_min.y,
            ((i & 4) != 0) ? aabb_max.z : aabb_min.z
        );
        float4 clip = mul(float4(corner, 1.0), u_view_projection);
        if (clip.w <= 0.0) {
            visibility[idx] = 1u;
            return;
        }
        float3 ndc = clip.xyz / clip.w;
        ndc_min = min(ndc_min, ndc.xy);
        ndc_max = max(ndc_max, ndc.xy);
        nearest_z = min(nearest_z, ndc.z);
    }

    float2 uv_min = ndc_min * 0.5 + 0.5;
    float2 uv_max = ndc_max * 0.5 + 0.5;
    uv_min = saturate(uv_min);
    uv_max = saturate(uv_max);

    if (uv_max.x <= 0.0 || uv_min.x >= 1.0 || uv_max.y <= 0.0 || uv_min.y >= 1.0) {
        visibility[idx] = 0u;
        return;
    }

    float2 size_pixels = (uv_max - uv_min) * float2(u_screen_size_x, u_screen_size_y);
    float max_dim = max(size_pixels.x, size_pixels.y);
    float mip_level = max_dim > 0.0 ? ceil(log2(max_dim)) : 0.0;
    mip_level = clamp(mip_level, 0.0, float(u_mip_count - 1));

    float test_depth = nearest_z - 0.005;
    float2 uv_center = (uv_min + uv_max) * 0.5;
    float hiz_depth = u_hiz_texture.SampleLevel(LinearSampler, uv_center, mip_level);
    float hiz_tl = u_hiz_texture.SampleLevel(LinearSampler, uv_min, mip_level);
    float hiz_br = u_hiz_texture.SampleLevel(LinearSampler, uv_max, mip_level);
    float hiz_tr = u_hiz_texture.SampleLevel(LinearSampler, float2(uv_max.x, uv_min.y), mip_level);
    float hiz_bl = u_hiz_texture.SampleLevel(LinearSampler, float2(uv_min.x, uv_max.y), mip_level);
    float max_hiz = max(max(hiz_depth, hiz_tl), max(max(hiz_br, hiz_tr), hiz_bl));

    visibility[idx] = (test_depth > max_hiz) ? 0u : 1u;
}
)";

const char* kHiZCullShaderSource = R"(
#version 430 core
#extension GL_ARB_shading_language_420pack : enable
layout(local_size_x = 64) in;

struct AABB {
    vec4 min_point; // xyz = world min, w = padding
    vec4 max_point; // xyz = world max, w = padding
};

layout(std430, binding = 0) readonly buffer AABBBuffer {
    AABB aabbs[];
};

layout(std430, binding = 1) writeonly buffer VisibilityBuffer {
    uint visibility[];
};

layout(binding = 0) uniform sampler2D u_hiz_texture;
uniform mat4 u_view_projection;
uniform vec2 u_screen_size;
uniform int u_mip_count;
uniform int u_object_count;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (int(idx) >= u_object_count) return;

    vec3 aabb_min = aabbs[idx].min_point.xyz;
    vec3 aabb_max = aabbs[idx].max_point.xyz;

    // Project all 8 corners to NDC
    vec2 ndc_min = vec2(1.0);
    vec2 ndc_max = vec2(-1.0);
    float nearest_z = 1.0;

    for (int i = 0; i < 8; ++i) {
        vec3 corner = vec3(
            ((i & 1) != 0) ? aabb_max.x : aabb_min.x,
            ((i & 2) != 0) ? aabb_max.y : aabb_min.y,
            ((i & 4) != 0) ? aabb_max.z : aabb_min.z
        );
        vec4 clip = u_view_projection * vec4(corner, 1.0);
        if (clip.w <= 0.0) {
            // Behind camera — conservatively mark as visible
            visibility[idx] = 1u;
            return;
        }
        vec3 ndc = clip.xyz / clip.w;
        ndc_min = min(ndc_min, ndc.xy);
        ndc_max = max(ndc_max, ndc.xy);
        nearest_z = min(nearest_z, ndc.z);
    }

    // Convert NDC [-1,1] to UV [0,1]
    vec2 uv_min = ndc_min * 0.5 + 0.5;
    vec2 uv_max = ndc_max * 0.5 + 0.5;

    // Clamp to screen
    uv_min = clamp(uv_min, vec2(0.0), vec2(1.0));
    uv_max = clamp(uv_max, vec2(0.0), vec2(1.0));

    // If fully outside frustum, mark occluded
    if (uv_max.x <= 0.0 || uv_min.x >= 1.0 || uv_max.y <= 0.0 || uv_min.y >= 1.0) {
        visibility[idx] = 0u;
        return;
    }

    // Determine appropriate mip level based on screen-space size
    vec2 size_pixels = (uv_max - uv_min) * u_screen_size;
    float max_dim = max(size_pixels.x, size_pixels.y);
    float mip_level = max_dim > 0.0 ? ceil(log2(max_dim)) : 0.0;
    mip_level = clamp(mip_level, 0.0, float(u_mip_count - 1));

    // Convert depth from NDC [-1,1] to [0,1] for comparison
    float test_depth = nearest_z * 0.5 + 0.5 - 0.005;

    // Sample Hi-Z at the center of the projected AABB
    vec2 uv_center = (uv_min + uv_max) * 0.5;
    float hiz_depth = textureLod(u_hiz_texture, uv_center, mip_level).r;

    // Also sample at corners for conservative test
    float hiz_tl = textureLod(u_hiz_texture, uv_min, mip_level).r;
    float hiz_br = textureLod(u_hiz_texture, uv_max, mip_level).r;
    float hiz_tr = textureLod(u_hiz_texture, vec2(uv_max.x, uv_min.y), mip_level).r;
    float hiz_bl = textureLod(u_hiz_texture, vec2(uv_min.x, uv_max.y), mip_level).r;
    float max_hiz = max(max(hiz_depth, hiz_tl), max(max(hiz_br, hiz_tr), hiz_bl));

    // Object is occluded if its nearest depth is farther than Hi-Z (reversed for max buffer)
    // For standard depth buffer (0=near, 1=far): occluded if test_depth > max_hiz
    if (test_depth > max_hiz) {
        visibility[idx] = 0u;
    } else {
        visibility[idx] = 1u;
    }
}
)";

void HiZBuildPass::EnsureShaders() {
    if (shaders_compiled_) return;
    shaders_compiled_ = true;

    // 使用 FramePipeline 缓存的 shader 句柄，避免每帧重建泄漏
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

    // Step 1: Copy PreZ depth → Hi-Z mip 0
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

    // Step 2: Iterative downsample mip N-1 → mip N
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
// HiZCullPass — GPU-driven 遮挡剔除 (Compute Shader)
// ============================================================

void HiZCullPass::EnsureShader() {
    if (shader_compiled_) return;
    shader_compiled_ = true;

    // 使用 FramePipeline 缓存的 shader 句柄
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

    // Bind SSBOs (DX11 区分 SRV/UAV; GL/VK 忽略 writable)
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
// GPUCullPass — GPU Driven 视锥 + Hi-Z 剔除，直接写 indirect args
// ============================================================

const char* kGPUCullShaderSource = R"(
#version 430 core
#extension GL_ARB_shading_language_420pack : enable
layout(local_size_x = 64) in;

struct AABB {
    vec4 min_point; // xyz = world min, w = padding
    vec4 max_point; // xyz = world max, w = padding
};

struct DrawCommand {
    uint count;
    uint instance_count;
    uint first_index;
    int  base_vertex;
    uint base_instance;
};

layout(std430, binding = 0) readonly buffer AABBBuffer {
    AABB aabbs[];
};

layout(std430, binding = 1) buffer DrawCommandBuffer {
    DrawCommand draw_cmds[];
};

layout(binding = 0) uniform sampler2D u_hiz_texture;
uniform mat4 u_view_projection;
uniform vec4 u_frustum_planes[6];
uniform vec2 u_screen_size;
uniform int u_mip_count;
uniform int u_object_count;

bool FrustumTestAABB(vec3 aabb_min, vec3 aabb_max) {
    for (int i = 0; i < 6; ++i) {
        vec3 positive_vertex = vec3(
            (u_frustum_planes[i].x >= 0.0) ? aabb_max.x : aabb_min.x,
            (u_frustum_planes[i].y >= 0.0) ? aabb_max.y : aabb_min.y,
            (u_frustum_planes[i].z >= 0.0) ? aabb_max.z : aabb_min.z
        );
        float d = dot(u_frustum_planes[i].xyz, positive_vertex) + u_frustum_planes[i].w;
        if (d < 0.0) return false;
    }
    return true;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (int(idx) >= u_object_count) return;

    vec3 aabb_min = aabbs[idx].min_point.xyz;
    vec3 aabb_max = aabbs[idx].max_point.xyz;

    // 1. Frustum culling
    if (!FrustumTestAABB(aabb_min, aabb_max)) {
        draw_cmds[idx].instance_count = 0u;
        return;
    }

    // 2. Hi-Z occlusion culling
    vec2 ndc_min = vec2(1.0);
    vec2 ndc_max = vec2(-1.0);
    float nearest_z = 1.0;

    for (int i = 0; i < 8; ++i) {
        vec3 corner = vec3(
            ((i & 1) != 0) ? aabb_max.x : aabb_min.x,
            ((i & 2) != 0) ? aabb_max.y : aabb_min.y,
            ((i & 4) != 0) ? aabb_max.z : aabb_min.z
        );
        vec4 clip = u_view_projection * vec4(corner, 1.0);
        if (clip.w <= 0.0) {
            // Behind camera — conservatively mark visible
            draw_cmds[idx].instance_count = 1u;
            return;
        }
        vec3 ndc = clip.xyz / clip.w;
        ndc_min = min(ndc_min, ndc.xy);
        ndc_max = max(ndc_max, ndc.xy);
        nearest_z = min(nearest_z, ndc.z);
    }

    vec2 uv_min = clamp(ndc_min * 0.5 + 0.5, vec2(0.0), vec2(1.0));
    vec2 uv_max = clamp(ndc_max * 0.5 + 0.5, vec2(0.0), vec2(1.0));

    // Fully outside screen
    if (uv_max.x <= 0.0 || uv_min.x >= 1.0 || uv_max.y <= 0.0 || uv_min.y >= 1.0) {
        draw_cmds[idx].instance_count = 0u;
        return;
    }

    // Determine mip level
    vec2 size_pixels = (uv_max - uv_min) * u_screen_size;
    float max_dim = max(size_pixels.x, size_pixels.y);
    float mip_level = max_dim > 0.0 ? ceil(log2(max_dim)) : 0.0;
    mip_level = clamp(mip_level, 0.0, float(u_mip_count - 1));

    float test_depth = nearest_z * 0.5 + 0.5 - 0.005;

    // Sample Hi-Z (5-tap conservative)
    vec2 uv_center = (uv_min + uv_max) * 0.5;
    float hiz_c  = textureLod(u_hiz_texture, uv_center, mip_level).r;
    float hiz_tl = textureLod(u_hiz_texture, uv_min, mip_level).r;
    float hiz_br = textureLod(u_hiz_texture, uv_max, mip_level).r;
    float hiz_tr = textureLod(u_hiz_texture, vec2(uv_max.x, uv_min.y), mip_level).r;
    float hiz_bl = textureLod(u_hiz_texture, vec2(uv_min.x, uv_max.y), mip_level).r;
    float max_hiz = max(max(hiz_c, hiz_tl), max(max(hiz_br, hiz_tr), hiz_bl));

    if (test_depth > max_hiz) {
        draw_cmds[idx].instance_count = 0u;
    } else {
        draw_cmds[idx].instance_count = 1u;
    }
}
)";

const char* kGPUCullShaderSourceVK = R"(
#version 450
layout(local_size_x = 64) in;

struct AABB {
    vec4 min_point;
    vec4 max_point;
};

struct DrawCommand {
    uint count;
    uint instance_count;
    uint first_index;
    int  base_vertex;
    uint base_instance;
};

layout(set=0, binding=0, std430) readonly buffer AABBBuffer {
    AABB aabbs[];
};

layout(set=0, binding=1, std430) buffer DrawCommandBuffer {
    DrawCommand draw_cmds[];
};

layout(set=0, binding=2) uniform sampler2D u_hiz_texture;

layout(push_constant) uniform PC {
    mat4 u_view_projection;
    vec2 u_screen_size;
    int u_mip_count;
    int u_object_count;
    vec4 u_frustum_planes[6];
} pc;

bool FrustumTestAABB(vec3 aabb_min, vec3 aabb_max) {
    for (int i = 0; i < 6; ++i) {
        vec3 positive_vertex = vec3(
            (pc.u_frustum_planes[i].x >= 0.0) ? aabb_max.x : aabb_min.x,
            (pc.u_frustum_planes[i].y >= 0.0) ? aabb_max.y : aabb_min.y,
            (pc.u_frustum_planes[i].z >= 0.0) ? aabb_max.z : aabb_min.z
        );
        float d = dot(pc.u_frustum_planes[i].xyz, positive_vertex) + pc.u_frustum_planes[i].w;
        if (d < 0.0) return false;
    }
    return true;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (int(idx) >= pc.u_object_count) return;

    vec3 aabb_min = aabbs[idx].min_point.xyz;
    vec3 aabb_max = aabbs[idx].max_point.xyz;

    if (!FrustumTestAABB(aabb_min, aabb_max)) {
        draw_cmds[idx].instance_count = 0u;
        return;
    }

    vec2 ndc_min = vec2(1.0);
    vec2 ndc_max = vec2(-1.0);
    float nearest_z = 1.0;

    for (int i = 0; i < 8; ++i) {
        vec3 corner = vec3(
            ((i & 1) != 0) ? aabb_max.x : aabb_min.x,
            ((i & 2) != 0) ? aabb_max.y : aabb_min.y,
            ((i & 4) != 0) ? aabb_max.z : aabb_min.z
        );
        vec4 clip = pc.u_view_projection * vec4(corner, 1.0);
        if (clip.w <= 0.0) {
            draw_cmds[idx].instance_count = 1u;
            return;
        }
        vec3 ndc = clip.xyz / clip.w;
        ndc_min = min(ndc_min, ndc.xy);
        ndc_max = max(ndc_max, ndc.xy);
        nearest_z = min(nearest_z, ndc.z);
    }

    vec2 uv_min = clamp(ndc_min * 0.5 + 0.5, vec2(0.0), vec2(1.0));
    vec2 uv_max = clamp(ndc_max * 0.5 + 0.5, vec2(0.0), vec2(1.0));

    if (uv_max.x <= 0.0 || uv_min.x >= 1.0 || uv_max.y <= 0.0 || uv_min.y >= 1.0) {
        draw_cmds[idx].instance_count = 0u;
        return;
    }

    vec2 size_pixels = (uv_max - uv_min) * pc.u_screen_size;
    float max_dim = max(size_pixels.x, size_pixels.y);
    float mip_level = max_dim > 0.0 ? ceil(log2(max_dim)) : 0.0;
    mip_level = clamp(mip_level, 0.0, float(pc.u_mip_count - 1));

    float test_depth = nearest_z * 0.5 + 0.5 - 0.005;
    vec2 uv_center = (uv_min + uv_max) * 0.5;
    float hiz_c  = textureLod(u_hiz_texture, uv_center, mip_level).r;
    float hiz_tl = textureLod(u_hiz_texture, uv_min, mip_level).r;
    float hiz_br = textureLod(u_hiz_texture, uv_max, mip_level).r;
    float hiz_tr = textureLod(u_hiz_texture, vec2(uv_max.x, uv_min.y), mip_level).r;
    float hiz_bl = textureLod(u_hiz_texture, vec2(uv_min.x, uv_max.y), mip_level).r;
    float max_hiz = max(max(hiz_c, hiz_tl), max(max(hiz_br, hiz_tr), hiz_bl));

    draw_cmds[idx].instance_count = (test_depth > max_hiz) ? 0u : 1u;
}
)";

const char* kGPUCullShaderSourceHLSL = R"(
struct AABB {
    float4 min_point;
    float4 max_point;
};

struct DrawCommand {
    uint count;
    uint instance_count;
    uint first_index;
    int  base_vertex;
    uint base_instance;
};

cbuffer Params : register(b0) {
    float4x4 u_view_projection;
    float2 u_screen_size;
    int u_mip_count;
    int u_object_count;
    float4 u_frustum_planes[6];
};

StructuredBuffer<AABB> aabbs : register(t16);
RWStructuredBuffer<DrawCommand> draw_cmds : register(u1);
Texture2D<float> u_hiz_texture : register(t0);
SamplerState LinearSampler : register(s0);

bool FrustumTestAABB(float3 aabb_min, float3 aabb_max) {
    for (int i = 0; i < 6; ++i) {
        float3 positive_vertex = float3(
            (u_frustum_planes[i].x >= 0.0f) ? aabb_max.x : aabb_min.x,
            (u_frustum_planes[i].y >= 0.0f) ? aabb_max.y : aabb_min.y,
            (u_frustum_planes[i].z >= 0.0f) ? aabb_max.z : aabb_min.z
        );
        float d = dot(u_frustum_planes[i].xyz, positive_vertex) + u_frustum_planes[i].w;
        if (d < 0.0f) return false;
    }
    return true;
}

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint idx = id.x;
    if ((int)idx >= u_object_count) return;

    float3 aabb_min = aabbs[idx].min_point.xyz;
    float3 aabb_max = aabbs[idx].max_point.xyz;

    if (!FrustumTestAABB(aabb_min, aabb_max)) {
        draw_cmds[idx].instance_count = 0u;
        return;
    }

    float2 ndc_min = float2(1.0f, 1.0f);
    float2 ndc_max = float2(-1.0f, -1.0f);
    float nearest_z = 1.0f;

    for (int i = 0; i < 8; ++i) {
        float3 corner = float3(
            ((i & 1) != 0) ? aabb_max.x : aabb_min.x,
            ((i & 2) != 0) ? aabb_max.y : aabb_min.y,
            ((i & 4) != 0) ? aabb_max.z : aabb_min.z
        );
        float4 clip = mul(u_view_projection, float4(corner, 1.0f));
        if (clip.w <= 0.0f) {
            draw_cmds[idx].instance_count = 1u;
            return;
        }
        float3 ndc = clip.xyz / clip.w;
        ndc_min = min(ndc_min, ndc.xy);
        ndc_max = max(ndc_max, ndc.xy);
        nearest_z = min(nearest_z, ndc.z);
    }

    float2 uv_min = clamp(ndc_min * 0.5f + 0.5f, float2(0.0f, 0.0f), float2(1.0f, 1.0f));
    float2 uv_max = clamp(ndc_max * 0.5f + 0.5f, float2(0.0f, 0.0f), float2(1.0f, 1.0f));

    if (uv_max.x <= 0.0f || uv_min.x >= 1.0f || uv_max.y <= 0.0f || uv_min.y >= 1.0f) {
        draw_cmds[idx].instance_count = 0u;
        return;
    }

    float2 size_pixels = (uv_max - uv_min) * u_screen_size;
    float max_dim = max(size_pixels.x, size_pixels.y);
    float mip_level = max_dim > 0.0f ? ceil(log2(max_dim)) : 0.0f;
    mip_level = clamp(mip_level, 0.0f, (float)(u_mip_count - 1));

    float test_depth = nearest_z * 0.5f + 0.5f - 0.005f;
    float2 uv_center = (uv_min + uv_max) * 0.5f;
    float hiz_c  = u_hiz_texture.SampleLevel(LinearSampler, uv_center, mip_level);
    float hiz_tl = u_hiz_texture.SampleLevel(LinearSampler, uv_min, mip_level);
    float hiz_br = u_hiz_texture.SampleLevel(LinearSampler, uv_max, mip_level);
    float hiz_tr = u_hiz_texture.SampleLevel(LinearSampler, float2(uv_max.x, uv_min.y), mip_level);
    float hiz_bl = u_hiz_texture.SampleLevel(LinearSampler, float2(uv_min.x, uv_max.y), mip_level);
    float max_hiz = max(max(hiz_c, hiz_tl), max(max(hiz_br, hiz_tr), hiz_bl));

    draw_cmds[idx].instance_count = (test_depth > max_hiz) ? 0u : 1u;
}
)";

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
// RSMRenderPass — 从方向光视角渲染场景到 RSM MRT (position/normal/flux)
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

    // Camera-Relative: shadow_center 转换到相机相对空间
    glm::vec3 shadow_center = FindShadowCenter(snap) - ctx_.camera_offset;
    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
    const float cam_near = snap.camera_3d.valid ? snap.camera_3d.near_clip : 0.1f;
    const float aspect = static_cast<float>(Screen::width()) / static_cast<float>(std::max(Screen::height(), 1));
    const float tan_half_fov = std::tan(glm::radians(snap.camera_3d.valid ? snap.camera_3d.fov * 0.5f : 30.0f));
    const glm::mat4 inv_view = snap.camera_3d.valid ? glm::inverse(snap.camera_3d.view) : glm::mat4(1.0f);
    const float ortho_size = ComputeCascadeOrthoSize(
        inv_view, snap.directional_light.direction, cam_near,
        snap.directional_light.cascade_splits[0], aspect, tan_half_fov);
    auto cam = ComputeDirectionalLightCamera(
        shadow_center, snap.directional_light.direction, ortho_size, clip_correction);

    cmd_buffer.BeginRenderPass({ctx_.rsm_render_target, glm::vec4(0.0f), true});
    ctx_.rhi_device->SetGBufferRenderingMode(true);
    cmd_buffer.SetCamera(cam.view, cam.projection);
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.mesh);

    RenderScenePassContext pass_ctx;
    pass_ctx.world = ctx_.world;
    pass_ctx.view = &cam.view;
    pass_ctx.projection = &cam.projection;
    pass_ctx.camera_offset = ctx_.camera_offset;
    if (ctx_.render_scene) {
        ctx_.render_scene->DrawOpaqueCpu(cmd_buffer);
    }
    ExecuteSceneRenderers(ctx_.render_scene, SceneRenderStage::Opaque, cmd_buffer, pass_ctx);

    ctx_.rhi_device->SetGBufferRenderingMode(false);
    cmd_buffer.EndRenderPass();
}

// ============================================================
// DDGIUpdatePass — 从 RSM VPL 更新 Irradiance Probe Atlas (Compute Shader)
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

    // 驱动 DDGI 系统更新探针（传入外部管理的 RSM 纹理句柄）
    ctx_.ddgi_system->UpdateProbes(rhi,
                                    ctx_.rsm_targets.position,
                                    ctx_.rsm_targets.normal,
                                    ctx_.rsm_targets.flux,
                                    ctx_.rsm_targets.width,
                                    ctx_.rsm_targets.height,
                                    light_dir, light_color);

    // 更新 context 中的 atlas 句柄供后续 Pass 采样
    const auto& res = ctx_.ddgi_system->GetResources();
    ctx_.ddgi_irradiance_atlas = res.irradiance_atlas;
    ctx_.ddgi_visibility_atlas = res.visibility_atlas;
}

// ============================================================
// SSSBlurPass — Separable Subsurface Scattering
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

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);

    // Pass 1: Horizontal blur → sss_temp
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
        cmd_buffer.DrawPostProcess(std::move(req));
    }
    cmd_buffer.EndRenderPass();

    // Pass 2: Vertical blur → back to scene RT
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
        cmd_buffer.DrawPostProcess(std::move(req));
    }
    cmd_buffer.EndRenderPass();
}

// ============================================================
// WeatherPass — Screen-Space Weather Particles (rain / snow)
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

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
    cmd_buffer.DrawPostProcess(PostProcessRequest{"weather_particle", scene_tex, params}.Tex(2, depth_tex));
    cmd_buffer.EndRenderPass();
}

// ============================================================
// FoliagePass — Vegetation Wind Bending + Character Interaction
// ============================================================

void FoliagePass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void FoliagePass::Execute(CommandBuffer& cmd_buffer) {
    // 植被（grass/tree）已通过 Gameplay3D 的 ISceneRenderer 在 PreZ/Shadow/Opaque
    // 阶段贡献绘制，本 Pass 不再承载任何渲染（历史 foliage_callbacks 已移除）。
    (void)cmd_buffer;
}

} // namespace render
} // namespace dse
