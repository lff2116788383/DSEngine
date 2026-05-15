/**
 * @file builtin_passes.cpp
 * @brief 引擎内置渲染 Pass 实现
 */

#include "engine/render/passes/builtin_passes.h"
#include "engine/render/rhi/rhi_device.h"
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
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "engine/base/time.h"

namespace dse {
namespace render {

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
    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
    auto camera3d_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    entt::entity selected_camera3d = entt::null;
    int selected_priority3d = std::numeric_limits<int>::min();
    for (auto entity : camera3d_view) {
        auto& camera = camera3d_view.get<dse::Camera3DComponent>(entity);
        if (camera.enabled && camera.priority > selected_priority3d) {
            selected_camera3d = entity;
            selected_priority3d = camera.priority;
        }
    }
    if (selected_camera3d != entt::null) {
        auto& camera = camera3d_view.get<dse::Camera3DComponent>(selected_camera3d);
        glm::mat4 projection = clip_correction * glm::perspective(glm::radians(camera.fov),
                                                static_cast<float>(Screen::width()) / static_cast<float>(Screen::height()),
                                                camera.near_clip, camera.far_clip);
        glm::mat4 view = glm::mat4(1.0f);
        if (ctx_.world->registry().all_of<TransformComponent>(selected_camera3d)) {
            auto& transform = ctx_.world->registry().get<TransformComponent>(selected_camera3d);
            glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            view = glm::lookAt(transform.position, transform.position + front, up);
        }
        cmd_buffer.SetCamera(view, projection);
        cmd_buffer.SetPipelineState(ctx_.pipeline_states.prez);

        for (auto& mod : ctx_.modules) {
            if (mod.instance) {
                mod.instance->OnRenderPreZ(*ctx_.world, cmd_buffer);
            }
        }
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
    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
    const glm::mat4 shadow_sample_correction = ctx_.rhi_device->GetShadowSampleCorrection();
    auto light_view = ctx_.world->registry().view<dse::DirectionalLight3DComponent>();
    if (light_view.begin() == light_view.end()) return;
    auto& light = light_view.get<dse::DirectionalLight3DComponent>(*light_view.begin());
    if (!light.enabled || !light.cast_shadow) return;

    // Find main camera position to center shadow maps on the scene
    glm::vec3 shadow_center(0.0f);
    auto camera3d_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    for (auto entity : camera3d_view) {
        auto& camera = camera3d_view.get<dse::Camera3DComponent>(entity);
        if (camera.enabled && ctx_.world->registry().all_of<TransformComponent>(entity)) {
            auto& transform = ctx_.world->registry().get<TransformComponent>(entity);
            glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            shadow_center = transform.position + front * 50.0f;
            break;
        }
    }

    std::vector<glm::mat4> light_space_matrices(CSM_CASCADES);
    std::vector<float> cascade_splits(CSM_CASCADES);

    for (int i = 0; i < CSM_CASCADES; ++i) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.shadow[i], glm::vec4(1.0f), true});

        // Use cascade_splits to scale ortho projection size
        float size = light.cascade_splits[i];
        float far_dist = size * 4.0f;
        glm::vec3 light_dir_n = glm::normalize(light.direction);

        // Texel snapping: 将 shadow_center 在光空间中对齐到阴影贴图纹素边界，
        // 避免相机移动时阴影边缘子像素抖动（shadow swimming）
        constexpr float shadow_map_res = 2048.0f;
        float texel_world_size = (2.0f * size) / shadow_map_res;
        glm::mat4 light_view_unsnapped = glm::lookAt(
            shadow_center - light_dir_n * (far_dist * 0.5f),
            shadow_center, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::vec4 sc_ls = light_view_unsnapped * glm::vec4(shadow_center, 1.0f);
        sc_ls.x = std::floor(sc_ls.x / texel_world_size) * texel_world_size;
        sc_ls.y = std::floor(sc_ls.y / texel_world_size) * texel_world_size;
        glm::vec3 snapped_center = glm::vec3(glm::inverse(light_view_unsnapped) * sc_ls);

        glm::vec3 light_pos = snapped_center - light_dir_n * (far_dist * 0.5f);
        glm::mat4 light_view_mat = glm::lookAt(light_pos, snapped_center, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 light_proj = clip_correction * glm::ortho(-size, size, -size, size, 1.0f, far_dist);

        // Sampling matrix uses shadow_sample_correction (no Z remap)
        // so shader can uniformly remap Z from [-1,1] to [0,1]
        glm::mat4 sample_proj = shadow_sample_correction * glm::ortho(-size, size, -size, size, 1.0f, far_dist);
        light_space_matrices[i] = sample_proj * light_view_mat;
        cascade_splits[i] = light.cascade_splits[i];

        cmd_buffer.SetCamera(light_view_mat, light_proj);
        cmd_buffer.SetPipelineState(ctx_.pipeline_states.shadow);

        for (auto& mod : ctx_.modules) {
            if (mod.instance) {
                mod.instance->OnRenderShadow(*ctx_.world, cmd_buffer, i, light_view_mat, light_proj);
            }
        }

        cmd_buffer.EndRenderPass();
    }

    cmd_buffer.SetGlobalMat4Array("u_light_space_matrices", light_space_matrices);
    cmd_buffer.SetGlobalFloatArray("u_cascade_splits", cascade_splits);

    for (int i = 0; i < CSM_CASCADES; ++i) {
        cmd_buffer.DeferSetGlobalShadowMap(i, ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.shadow[i]));
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
    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
    const glm::mat4 shadow_sample_correction = ctx_.rhi_device->GetShadowSampleCorrection();
    auto spot_light_view = ctx_.world->registry().view<TransformComponent, dse::SpotLightComponent>();
    std::vector<glm::mat4> spot_light_space_matrices;
    spot_light_space_matrices.reserve(4);
    int shadow_slot = 0;
    for (auto entity : spot_light_view) {
        auto& light = spot_light_view.get<dse::SpotLightComponent>(entity);
        if (!light.enabled || !light.cast_shadow || shadow_slot >= 4 || ctx_.render_targets.spot_shadow[shadow_slot] == 0) {
            continue;
        }

        auto& transform = spot_light_view.get<TransformComponent>(entity);
        const glm::vec3 forward = glm::normalize(transform.rotation * light.direction);
        glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(forward, up)) > 0.98f) {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        const glm::mat4 light_view_mat = glm::lookAt(transform.position, transform.position + forward, up);
        const glm::mat4 light_proj = clip_correction * glm::perspective(glm::radians(light.outer_cone_angle * 2.0f), 1.0f, 0.1f, std::max(1.0f, light.radius));
        cmd_buffer.BeginRenderPass({ctx_.render_targets.spot_shadow[shadow_slot], glm::vec4(1.0f), true});
        cmd_buffer.SetCamera(light_view_mat, light_proj);
        cmd_buffer.SetPipelineState(ctx_.pipeline_states.shadow);
        for (auto& mod : ctx_.modules) {
            if (mod.instance) {
                mod.instance->OnRenderShadow(*ctx_.world, cmd_buffer, CSM_CASCADES, light_view_mat, light_proj);
            }
        }
        cmd_buffer.EndRenderPass();
        // Sampling matrix: no Z remap, shader remaps Z uniformly
        const glm::mat4 sample_proj = shadow_sample_correction * glm::perspective(glm::radians(light.outer_cone_angle * 2.0f), 1.0f, 0.1f, std::max(1.0f, light.radius));
        spot_light_space_matrices.push_back(sample_proj * light_view_mat);
        cmd_buffer.DeferSetGlobalSpotShadowMap(static_cast<unsigned int>(shadow_slot), ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.spot_shadow[shadow_slot]));
        ++shadow_slot;
    }
    cmd_buffer.SetGlobalMat4Array("u_spot_light_space_matrices", spot_light_space_matrices);
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
    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
    auto point_light_view = ctx_.world->registry().view<TransformComponent, dse::PointLightComponent>();
    int shadow_slot = 0;
    for (auto entity : point_light_view) {
        auto& light = point_light_view.get<dse::PointLightComponent>(entity);
        if (!light.enabled || !light.cast_shadow || shadow_slot >= 4 || ctx_.render_targets.point_shadow[shadow_slot] == 0) {
            continue;
        }

        auto& transform = point_light_view.get<TransformComponent>(entity);
        const glm::mat4 light_proj = clip_correction * glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, std::max(1.0f, light.radius));
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
            const glm::mat4 light_view_mat = glm::lookAt(transform.position, transform.position + face_directions[face], face_ups[face]);
            cmd_buffer.BeginRenderPass({ctx_.render_targets.point_shadow[shadow_slot], glm::vec4(1.0f), true});
            cmd_buffer.SetCamera(light_view_mat, light_proj);
            cmd_buffer.SetPipelineState(ctx_.pipeline_states.shadow);
            for (auto& mod : ctx_.modules) {
                if (mod.instance) {
                    mod.instance->OnRenderShadow(*ctx_.world, cmd_buffer, CSM_CASCADES + 1 + face, light_view_mat, light_proj);
                }
            }
            cmd_buffer.EndRenderPass();
        }

        cmd_buffer.DeferSetGlobalPointShadowMap(static_cast<unsigned int>(shadow_slot), ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.point_shadow[shadow_slot]));
        ++shadow_slot;
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
    bool render_3d = false;
    if (ctx_.editor_mode && ctx_.use_editor_camera) {
        render_3d = true;
        const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        cmd_buffer.SetCamera(ctx_.editor_view, clip_correction * ctx_.editor_projection);
    } else {
        auto camera3d_view = ctx_.world->registry().view<dse::Camera3DComponent>();
        entt::entity selected_camera3d = entt::null;
        int selected_priority3d = std::numeric_limits<int>::min();
        std::uint32_t selected_id3d = std::numeric_limits<std::uint32_t>::max();
        for (auto entity : camera3d_view) {
            auto& camera = camera3d_view.get<dse::Camera3DComponent>(entity);
            if (!camera.enabled) continue;
            const std::uint32_t entity_id = static_cast<std::uint32_t>(entity);
            if (selected_camera3d == entt::null ||
                camera.priority > selected_priority3d ||
                (camera.priority == selected_priority3d && entity_id < selected_id3d)) {
                selected_camera3d = entity;
                selected_priority3d = camera.priority;
                selected_id3d = entity_id;
            }
        }
        if (selected_camera3d != entt::null) {
            render_3d = true;
            auto& camera = camera3d_view.get<dse::Camera3DComponent>(selected_camera3d);
            const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
            glm::mat4 projection = clip_correction * glm::perspective(glm::radians(camera.fov),
                                                    static_cast<float>(Screen::width()) / static_cast<float>(Screen::height()),
                                                    camera.near_clip, camera.far_clip);
            glm::mat4 view = glm::mat4(1.0f);
            if (ctx_.world->registry().all_of<TransformComponent>(selected_camera3d)) {
                auto& transform = ctx_.world->registry().get<TransformComponent>(selected_camera3d);
                glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                view = glm::lookAt(transform.position, transform.position + front, up);
            }
            cmd_buffer.SetCamera(view, projection);
        }
    }

    if (render_3d) {
        cmd_buffer.SetPipelineState(ctx_.pipeline_states.mesh);
        if (ctx_.modules.empty() && ctx_.render_meshes) {
            ctx_.render_meshes(*ctx_.world, cmd_buffer);
        }
        const glm::mat4 scene_clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        for (auto& mod : ctx_.modules) {
            if (mod.instance) {
                mod.instance->OnRenderScene(*ctx_.world, cmd_buffer, scene_clip_correction);
            }
        }
    }

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
    if (ctx_.render_targets.deferred_lighting == 0 || ctx_.render_targets.gbuffer == 0) return;

    cmd_buffer.BeginRenderPass({ctx_.render_targets.deferred_lighting, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), true});

    unsigned int gbuf_albedo   = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.gbuffer, 0);
    unsigned int gbuf_normal   = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.gbuffer, 1);
    unsigned int gbuf_position = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.gbuffer, 2);

    // 查找主方向光参数
    glm::vec3 light_dir(0.0f, -1.0f, 0.0f);
    glm::vec3 light_color(1.0f);
    float light_intensity = 1.0f;
    float ambient_intensity = 0.1f;

    auto dl_view = ctx_.world->registry().view<dse::DirectionalLight3DComponent>();
    for (auto entity : dl_view) {
        auto& dl = dl_view.get<dse::DirectionalLight3DComponent>(entity);
        if (!dl.enabled) continue;
        light_dir = glm::normalize(dl.direction);
        light_color = dl.color;
        light_intensity = dl.intensity;
        ambient_intensity = dl.ambient_intensity;
        break;
    }

    // params: [normal_tex, position_tex, light_dir.xyz, light_color.xyz, intensity, ambient]
    std::vector<float> params;
    params.push_back(static_cast<float>(gbuf_normal));
    params.push_back(static_cast<float>(gbuf_position));
    params.push_back(light_dir.x); params.push_back(light_dir.y); params.push_back(light_dir.z);
    params.push_back(light_color.x); params.push_back(light_color.y); params.push_back(light_color.z);
    params.push_back(light_intensity);
    params.push_back(ambient_intensity);

    cmd_buffer.DrawPostProcess(gbuf_albedo, "deferred_lighting", params);
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
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.02f, 0.02f, 0.02f, 1.0f), true});

    bool render_3d = false; // Only render 3D meshes when a valid camera is active

    // Editor camera override: use editor view/proj for Scene render target
    if (ctx_.editor_mode && ctx_.use_editor_camera) {
        render_3d = true;
        const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        cmd_buffer.SetCamera(ctx_.editor_view, clip_correction * ctx_.editor_projection);

        // Still render skybox if present
        auto skybox_view = ctx_.world->registry().view<dse::SkyboxComponent>();
        for (auto sky_entity : skybox_view) {
            auto& skybox = skybox_view.get<dse::SkyboxComponent>(sky_entity);
            if (!skybox.enabled) continue;
            if (skybox.cubemap_handle == 0 && !skybox.cubemap_path.empty()) {
                if (auto cubemap = ctx_.asset_manager->LoadCubemap(skybox.cubemap_path)) {
                    skybox.cubemap_handle = cubemap->GetHandle();
                }
            }
            if (skybox.cubemap_handle != 0) {
                cmd_buffer.DrawSkybox(skybox.cubemap_handle);
            }
            break;
        }
    } else {
    auto camera3d_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    entt::entity selected_camera3d = entt::null;
    int selected_priority3d = std::numeric_limits<int>::min();
    std::uint32_t selected_id3d = std::numeric_limits<std::uint32_t>::max();
    for (auto entity : camera3d_view) {
        auto& camera = camera3d_view.get<dse::Camera3DComponent>(entity);
        if (!camera.enabled) {
            continue;
        }
        const std::uint32_t entity_id = static_cast<std::uint32_t>(entity);
        if (selected_camera3d == entt::null ||
            camera.priority > selected_priority3d ||
            (camera.priority == selected_priority3d && entity_id < selected_id3d)) {
            selected_camera3d = entity;
            selected_priority3d = camera.priority;
            selected_id3d = entity_id;
        }
    }

    if (selected_camera3d != entt::null) {
        render_3d = true;
        auto& camera = camera3d_view.get<dse::Camera3DComponent>(selected_camera3d);
        const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        glm::mat4 projection = clip_correction * glm::perspective(glm::radians(camera.fov),
                                                static_cast<float>(Screen::width()) / static_cast<float>(Screen::height()),
                                                camera.near_clip, camera.far_clip);

        // TAA jitter：对投影矩阵做子像素偏移，使每帧采样位置不同
        if (ctx_.taa_active) {
            projection[2][0] += ctx_.taa_jitter.x * 2.0f;
            projection[2][1] += ctx_.taa_jitter.y * 2.0f;
        }

        glm::mat4 view = glm::mat4(1.0f);
        if (ctx_.world->registry().all_of<TransformComponent>(selected_camera3d)) {
            auto& transform = ctx_.world->registry().get<TransformComponent>(selected_camera3d);
            glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            view = glm::lookAt(transform.position, transform.position + front, up);
        }
        cmd_buffer.SetCamera(view, projection);

        auto skybox_view = ctx_.world->registry().view<dse::SkyboxComponent>();
        for (auto sky_entity : skybox_view) {
            auto& skybox = skybox_view.get<dse::SkyboxComponent>(sky_entity);
            if (!skybox.enabled) {
                continue;
            }
            if (skybox.cubemap_handle == 0 && !skybox.cubemap_path.empty()) {
                if (auto cubemap = ctx_.asset_manager->LoadCubemap(skybox.cubemap_path)) {
                    skybox.cubemap_handle = cubemap->GetHandle();
                }
            }
            if (skybox.cubemap_handle != 0) {
                // Apply skybox entity rotation to view (allows Lua to rotate skybox)
                if (ctx_.world->registry().all_of<TransformComponent>(sky_entity)) {
                    auto& sky_tf = ctx_.world->registry().get<TransformComponent>(sky_entity);
                    glm::mat4 sky_inv_rot = glm::mat4_cast(glm::conjugate(sky_tf.rotation));
                    cmd_buffer.SetCamera(view * sky_inv_rot, projection);
                }
                cmd_buffer.DrawSkybox(skybox.cubemap_handle);
                cmd_buffer.SetCamera(view, projection);
            }
            break;
        }
    } else {
        auto camera_view = ctx_.world->registry().view<CameraComponent>();
        entt::entity selected_camera = entt::null;
        int selected_priority = std::numeric_limits<int>::min();
        std::uint32_t selected_id = std::numeric_limits<std::uint32_t>::max();
        for (auto entity : camera_view) {
            auto& camera = camera_view.get<CameraComponent>(entity);
            if (!camera.enabled) {
                continue;
            }
            const std::uint32_t entity_id = static_cast<std::uint32_t>(entity);
            if (selected_camera == entt::null ||
                camera.priority > selected_priority ||
                (camera.priority == selected_priority && entity_id < selected_id)) {
                selected_camera = entity;
                selected_priority = camera.priority;
                selected_id = entity_id;
            }
        }
        if (selected_camera != entt::null) {
            auto& camera = camera_view.get<CameraComponent>(selected_camera);
            const glm::mat4 clip_correction_2d = ctx_.rhi_device->GetProjectionCorrection();
            cmd_buffer.SetCamera(camera.view, clip_correction_2d * camera.projection);
        }
    }
    } // end else (non-editor camera)

    if (render_3d) {
        cmd_buffer.SetPipelineState(ctx_.pipeline_states.mesh);

        // Clustered Forward+: 绑定光源 SSBO 和 Cluster 网格 SSBO
        if (ctx_.light_buffer) ctx_.light_buffer->Bind();
        if (ctx_.cluster_grid) ctx_.cluster_grid->Bind();

        if (ctx_.modules.empty() && ctx_.render_meshes) {
            ctx_.render_meshes(*ctx_.world, cmd_buffer);
        }
        const glm::mat4 scene_clip_correction = ctx_.rhi_device->GetProjectionCorrection();
        for (auto& mod : ctx_.modules) {
            if (mod.instance) {
                mod.instance->OnRenderScene(*ctx_.world, cmd_buffer, scene_clip_correction);
            }
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    bool pp_enabled = false;
    dse::PostProcessComponent pp_config;
    for (auto entity : pp_view) {
        if (pp_view.get<dse::PostProcessComponent>(entity).enabled) {
            pp_enabled = true;
            pp_config = pp_view.get<dse::PostProcessComponent>(entity);
            break;
        }
    }

    if (!pp_enabled || !pp_config.bloom_enabled) {
        return;
    }

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.bloom_extract, glm::vec4(0.0f), false});
    const unsigned int scene_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);
    cmd_buffer.DrawPostProcess(scene_color_tex, "bloom_extract", {pp_config.bloom_threshold});
    cmd_buffer.EndRenderPass();

    unsigned int current_src = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.bloom_extract);
    int mip_w = Screen::width() / 2;
    int mip_h = Screen::height() / 2;
    for (size_t i = 0; i < ctx_.render_targets.bloom_mips.size(); ++i) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.bloom_mips[i], glm::vec4(0.0f), false});
        cmd_buffer.DrawPostProcess(current_src, "bloom_downsample", {static_cast<float>(mip_w * 2), static_cast<float>(mip_h * 2)});
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
        cmd_buffer.DrawPostProcess(current_src, "bloom_upsample", {0.005f});
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
    const unsigned int ui_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.ui);

    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    bool pp_enabled = false;
    dse::PostProcessComponent pp_config;
    for (auto entity : pp_view) {
        if (pp_view.get<dse::PostProcessComponent>(entity).enabled) {
            pp_enabled = true;
            pp_config = pp_view.get<dse::PostProcessComponent>(entity);
            break;
        }
    }

    // 获取 SSAO 纹理（如果启用）
    unsigned int ssao_tex = 0;
    if (pp_enabled && pp_config.ssao_enabled && ctx_.render_targets.ssao_blur != 0) {
        ssao_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.ssao_blur);
    }

    // 获取 Contact Shadow 纹理（如果启用）
    unsigned int contact_shadow_tex = 0;
    if (pp_enabled && pp_config.contact_shadow_enabled && ctx_.render_targets.contact_shadow != 0) {
        contact_shadow_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.contact_shadow);
    }

    // 获取 auto exposure 纹理（如果启用）
    unsigned int ae_tex = 0;
    if (ctx_.auto_exposure_active) {
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

    if (pp_enabled && (pp_config.bloom_enabled || contact_shadow_tex != 0 || pp_config.vignette_enabled || pp_config.film_grain_enabled)) {
        const unsigned int bloom_tex = (pp_config.bloom_enabled && !ctx_.render_targets.bloom_mips.empty())
            ? ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.bloom_mips[0])
            : 0;
        cmd_buffer.DrawPostProcess(scene_color_tex, "bloom_composite", {
            static_cast<float>(bloom_tex),
            pp_config.exposure,
            pp_config.bloom_intensity,
            pp_config.bloom_enabled ? 1.0f : 0.0f,
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
        });
    } else {
        if (ssao_tex != 0) {
            cmd_buffer.DrawPostProcess(scene_color_tex, "ssao_apply", {static_cast<float>(ssao_tex), pp_config.exposure, static_cast<float>(ae_tex), lut_tex, lut_intensity});
        } else {
            if (ae_tex != 0) {
                cmd_buffer.DrawPostProcess(scene_color_tex, "tonemapping", {pp_config.exposure, static_cast<float>(ae_tex), lut_tex, lut_intensity});
            } else {
                if (lut_tex != 0.0f) {
                    cmd_buffer.DrawPostProcess(scene_color_tex, "color_grading", {lut_tex, lut_intensity});
                } else {
                    cmd_buffer.DrawPostProcess(scene_color_tex, "copy", {});
                }
            }
        }
    }

    cmd_buffer.DrawPostProcess(ui_color_tex, "ui_overlay", {});
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    bool ae_enabled = false;
    dse::PostProcessComponent pp_config;
    for (auto entity : pp_view) {
        auto& pp = pp_view.get<dse::PostProcessComponent>(entity);
        if (pp.enabled && pp.auto_exposure_enabled) {
            ae_enabled = true;
            pp_config = pp;
            break;
        }
    }

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
    cmd_buffer.DrawPostProcess(scene_color_tex, "lum_compute", {});
    cmd_buffer.EndRenderPass();

    // Pass 2: 64x64 → 1x1 adapted exposure (EMA blend with previous frame)
    const unsigned int lum_temp_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.lum_temp);
    const unsigned int prev_adapted_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.lum_adapted[read_idx]);

    cmd_buffer.BeginRenderPass({ctx_.render_targets.lum_adapted[write_idx], glm::vec4(1.0f), true});
    cmd_buffer.DrawPostProcess(lum_temp_tex, "lum_adapt", {
        static_cast<float>(prev_adapted_tex),
        ctx_.delta_time,
        pp_config.adaptation_speed_up,
        pp_config.adaptation_speed_down,
        pp_config.exposure_min,
        pp_config.exposure_max,
        pp_config.exposure_compensation
    });
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    bool ssao_enabled = false;
    dse::PostProcessComponent pp_config;
    for (auto entity : pp_view) {
        auto& pp = pp_view.get<dse::PostProcessComponent>(entity);
        if (pp.enabled && pp.ssao_enabled) {
            ssao_enabled = true;
            pp_config = pp;
            break;
        }
    }

    if (!ssao_enabled || ctx_.render_targets.ssao == 0 || ctx_.render_targets.ssao_blur == 0) {
        return;
    }

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    // 获取相机投影矩阵参数
    float near_plane = 0.1f, far_plane = 10000.0f;
    auto camera_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<dse::Camera3DComponent>(entity);
        if (cam.enabled) {
            near_plane = cam.near_clip;
            far_plane = cam.far_clip;
            break;
        }
    }

    // Pass 1: SSAO 计算（半分辨率）
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.ssao, glm::vec4(1.0f), true});
    cmd_buffer.DrawPostProcess(depth_tex, "ssao", {
        pp_config.ssao_radius,
        pp_config.ssao_bias,
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height())
    });
    cmd_buffer.EndRenderPass();

    // Pass 2: 双边模糊
    const unsigned int ssao_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.ssao);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.ssao_blur, glm::vec4(1.0f), true});
    cmd_buffer.DrawPostProcess(ssao_tex, "ssao_blur", {});
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    bool cs_enabled = false;
    dse::PostProcessComponent pp_config;
    for (auto entity : pp_view) {
        auto& pp = pp_view.get<dse::PostProcessComponent>(entity);
        if (pp.enabled && pp.contact_shadow_enabled) {
            cs_enabled = true;
            pp_config = pp;
            break;
        }
    }

    if (!cs_enabled || ctx_.render_targets.contact_shadow == 0) {
        return;
    }

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    // 获取主光源方向
    glm::vec3 light_dir(-0.4f, -1.0f, -0.3f);
    auto light_view = ctx_.world->registry().view<dse::DirectionalLight3DComponent>();
    if (light_view.begin() != light_view.end()) {
        auto& light = light_view.get<dse::DirectionalLight3DComponent>(*light_view.begin());
        if (light.enabled) {
            light_dir = glm::normalize(light.direction);
        }
    }

    // 获取相机参数
    float near_plane = 0.1f, far_plane = 10000.0f;
    auto camera_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<dse::Camera3DComponent>(entity);
        if (cam.enabled) {
            near_plane = cam.near_clip;
            far_plane = cam.far_clip;
            break;
        }
    }

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.contact_shadow, glm::vec4(1.0f), true});
    cmd_buffer.DrawPostProcess(depth_tex, "contact_shadow", {
        light_dir.x, light_dir.y, light_dir.z,
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height()),
        pp_config.contact_shadow_strength,
        static_cast<float>(pp_config.contact_shadow_steps),
        pp_config.contact_shadow_step_size
    });
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    bool fxaa_enabled = false;
    for (auto entity : pp_view) {
        auto& pp = pp_view.get<dse::PostProcessComponent>(entity);
        if (pp.enabled && pp.fxaa_enabled) {
            fxaa_enabled = true;
            break;
        }
    }

    ctx_.fxaa_active = fxaa_enabled && ctx_.render_targets.fxaa != 0;
    if (!ctx_.fxaa_active) {
        return;
    }

    const unsigned int main_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.main);
    if (main_color_tex == 0) return;

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.fxaa, glm::vec4(0.0f), true});
    cmd_buffer.DrawPostProcess(main_color_tex, "fxaa", {
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height())
    });
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
    cmd_buffer.DrawPostProcess(present_tex, "copy", {});
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    bool taa_enabled = false;
    float blend_factor = 0.1f;
    for (auto entity : pp_view) {
        auto& pp = pp_view.get<dse::PostProcessComponent>(entity);
        if (pp.enabled && pp.taa_enabled) {
            taa_enabled = true;
            blend_factor = pp.taa_blend_factor;
            break;
        }
    }

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
    cmd_buffer.DrawPostProcess(main_color_tex, "taa_resolve", {
        static_cast<float>(history_tex),
        blend_factor,
        current_jitter_.x,
        current_jitter_.y,
        static_cast<float>(frame_index_),
        static_cast<float>(mv_tex),
        static_cast<float>(sw),
        static_cast<float>(sh)
    });
    cmd_buffer.EndRenderPass();

    // 将 TAA 结果 copy 到 taa RT（供 Present/FXAA 读取）
    const unsigned int taa_out_tex = ctx_.rhi_device->GetRenderTargetColorTexture(history_rt_[write_idx]);
    if (taa_out_tex != 0 && ctx_.render_targets.taa != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.taa, glm::vec4(0.0f), true});
        cmd_buffer.DrawPostProcess(taa_out_tex, "copy", {});
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    bool dof_enabled = false;
    dse::PostProcessComponent pp_config;
    for (auto entity : pp_view) {
        auto& pp = pp_view.get<dse::PostProcessComponent>(entity);
        if (pp.enabled && pp.dof_enabled) {
            dof_enabled = true;
            pp_config = pp;
            break;
        }
    }

    if (!dof_enabled || ctx_.render_targets.dof == 0) return;

    // DOF 运行于 Composite 之后，读 main RT（已完成 tonemapping）
    const unsigned int main_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.main);
    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (main_color_tex == 0 || depth_tex == 0) return;

    float near_plane = 0.1f, far_plane = 10000.0f;
    auto camera_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<dse::Camera3DComponent>(entity);
        if (cam.enabled) {
            near_plane = cam.near_clip;
            far_plane = cam.far_clip;
            break;
        }
    }

    // Pass 1: DOF → dof RT
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.dof, glm::vec4(0.0f), true});
    cmd_buffer.DrawPostProcess(depth_tex, "dof", {
        pp_config.dof_focus_distance,
        pp_config.dof_focus_range,
        pp_config.dof_bokeh_radius,
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height()),
        static_cast<float>(main_color_tex)
    });
    cmd_buffer.EndRenderPass();

    // Pass 2: dof RT → main RT（回写）
    const unsigned int dof_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.dof);
    if (dof_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.main, glm::vec4(0.0f), true});
        cmd_buffer.DrawPostProcess(dof_tex, "copy", {});
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

    glm::mat4 current_vp = glm::mat4(1.0f);
    auto camera_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<dse::Camera3DComponent>(entity);
        if (cam.enabled) {
            const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
            glm::mat4 projection = clip_correction * glm::perspective(glm::radians(cam.fov),
                static_cast<float>(Screen::width()) / static_cast<float>(Screen::height()),
                cam.near_clip, cam.far_clip);
            glm::mat4 view = glm::mat4(1.0f);
            if (ctx_.world->registry().all_of<TransformComponent>(entity)) {
                auto& transform = ctx_.world->registry().get<TransformComponent>(entity);
                glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                view = glm::lookAt(transform.position, transform.position + front, up);
            }
            current_vp = projection * view;
            break;
        }
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
    cmd_buffer.DrawPostProcess(depth_tex, "motion_vector", params);
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    bool mb_enabled = false;
    dse::PostProcessComponent pp_config;
    for (auto entity : pp_view) {
        auto& pp = pp_view.get<dse::PostProcessComponent>(entity);
        if (pp.enabled && pp.motion_blur_enabled) {
            mb_enabled = true;
            pp_config = pp;
            break;
        }
    }

    if (!mb_enabled || ctx_.render_targets.dof == 0) return;

    const unsigned int main_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.main);
    const unsigned int mv_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.motion_vector);
    if (main_color_tex == 0 || mv_tex == 0) return;

    // motion_blur 现在读 motion_vector RT 而非深度 + reproj
    // params: [0] intensity, [1] samples, [2] screen_w, [3] screen_h, [4] color_tex
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.dof, glm::vec4(0.0f), true});
    cmd_buffer.DrawPostProcess(mv_tex, "motion_blur", {
        pp_config.motion_blur_intensity,
        static_cast<float>(pp_config.motion_blur_samples),
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height()),
        static_cast<float>(main_color_tex)
    });
    cmd_buffer.EndRenderPass();

    // dof RT → main RT
    const unsigned int mb_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.dof);
    if (mb_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.main, glm::vec4(0.0f), true});
        cmd_buffer.DrawPostProcess(mb_tex, "copy", {});
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    bool ssr_enabled = false;
    dse::PostProcessComponent pp_config;
    for (auto entity : pp_view) {
        auto& pp = pp_view.get<dse::PostProcessComponent>(entity);
        if (pp.enabled && pp.ssr_enabled) {
            ssr_enabled = true;
            pp_config = pp;
            break;
        }
    }

    if (!ssr_enabled || ctx_.render_targets.ssr == 0) return;

    const unsigned int scene_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);
    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (scene_color_tex == 0 || depth_tex == 0) return;

    float near_plane = 0.1f, far_plane = 10000.0f;
    auto camera_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<dse::Camera3DComponent>(entity);
        if (cam.enabled) {
            near_plane = cam.near_clip;
            far_plane = cam.far_clip;
            break;
        }
    }

    // Pass 1: 渲染 SSR 到半分辨率 ssr RT
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.ssr, glm::vec4(0.0f), true});
    cmd_buffer.DrawPostProcess(depth_tex, "ssr", {
        pp_config.ssr_max_distance,
        pp_config.ssr_thickness,
        pp_config.ssr_step_size,
        static_cast<float>(pp_config.ssr_max_steps),
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height()),
        static_cast<float>(scene_color_tex)
    });
    cmd_buffer.EndRenderPass();

    // Pass 2: 将 SSR 结果叠加到 scene RT（利用 SSR alpha 作为混合权重）
    const unsigned int ssr_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.ssr);
    if (ssr_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
        cmd_buffer.DrawPostProcess(ssr_tex, "ui_overlay", {});
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    const dse::PostProcessComponent* pp_ptr = nullptr;
    for (auto entity : pp_view) {
        auto& pp = pp_view.get<dse::PostProcessComponent>(entity);
        if (pp.enabled && pp.outline_enabled) {
            pp_ptr = &pp;
            break;
        }
    }

    if (!pp_ptr || ctx_.render_targets.outline == 0) return;

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    float near_plane = 0.1f, far_plane = 1000.0f;
    auto camera_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<dse::Camera3DComponent>(entity);
        if (cam.enabled) {
            near_plane = cam.near_clip;
            far_plane = cam.far_clip;
            break;
        }
    }

    // Pass 1: 边缘检测 → outline RT
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.outline, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), true});
    cmd_buffer.DrawPostProcess(depth_tex, "edge_detect", {
        pp_ptr->outline_thickness,
        pp_ptr->outline_depth_threshold,
        pp_ptr->outline_normal_threshold,
        pp_ptr->outline_color.r,
        pp_ptr->outline_color.g,
        pp_ptr->outline_color.b,
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height())
    });
    cmd_buffer.EndRenderPass();

    // Pass 2: 将边缘结果叠加到 scene RT
    const unsigned int outline_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.outline);
    if (outline_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
        cmd_buffer.DrawPostProcess(outline_tex, "ui_overlay", {});
        cmd_buffer.EndRenderPass();
    }
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    const dse::PostProcessComponent* pp = nullptr;
    for (auto entity : pp_view) {
        auto& p = pp_view.get<dse::PostProcessComponent>(entity);
        if (p.enabled && p.fog_enabled) { pp = &p; break; }
    }
    if (!pp || ctx_.render_targets.fog == 0) return;

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    // 获取相机参数
    float near_p = 0.1f, far_p = 1000.0f, fov_y = 60.0f;
    float aspect = static_cast<float>(Screen::width()) / static_cast<float>(Screen::height());
    glm::vec3 cam_pos{0.0f}, cam_right{1,0,0}, cam_up{0,1,0}, cam_fwd{0,0,-1};

    auto camera_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<dse::Camera3DComponent>(entity);
        if (!cam.enabled) continue;
        near_p = cam.near_clip;
        far_p  = cam.far_clip;
        fov_y  = cam.fov;
        if (ctx_.world->registry().all_of<TransformComponent>(entity)) {
            auto& t = ctx_.world->registry().get<TransformComponent>(entity);
            cam_pos   = t.position;
            cam_fwd   = glm::normalize(t.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            cam_right = glm::normalize(t.rotation * glm::vec3(1.0f, 0.0f,  0.0f));
            cam_up    = glm::normalize(t.rotation * glm::vec3(0.0f, 1.0f,  0.0f));
        }
        break;
    }
    const float tan_fov_y = std::tan(glm::radians(fov_y) * 0.5f);

    // 获取主平行光方向（用于 Mie 散射）
    glm::vec3 sun_dir{0.0f, -1.0f, 0.0f};
    auto light_view = ctx_.world->registry().view<dse::DirectionalLight3DComponent>();
    for (auto entity : light_view) {
        auto& l = light_view.get<dse::DirectionalLight3DComponent>(entity);
        if (l.enabled) { sun_dir = glm::normalize(l.direction); break; }
    }

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
    cmd_buffer.DrawPostProcess(scene_tex, "volumetric_fog", {
        static_cast<float>(depth_tex),
        pp->fog_color.r, pp->fog_color.g, pp->fog_color.b,
        pp->fog_density, pp->fog_height_falloff, pp->fog_height_offset,
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
    });
    cmd_buffer.EndRenderPass();

    // 将雾效结果（已包含 scene 颜色）覆写回 scene RT
    const unsigned int fog_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.fog);
    if (fog_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
        cmd_buffer.DrawPostProcess(fog_tex, "copy", {});
        cmd_buffer.EndRenderPass();
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
    graph.PassWrite(pass, scene_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void WBOITPass::Execute(CommandBuffer& cmd_buffer) {
    if (ctx_.render_targets.wboit_accum == 0 || ctx_.render_targets.wboit_reveal == 0) return;

    const glm::mat4 scene_clip_correction = ctx_.rhi_device->GetProjectionCorrection();

    // --- Pass 1: Accumulation (blend ONE, ONE) ---
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.wboit_accum);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.wboit_accum, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), true});

    if (ctx_.render_transparent_meshes) {
        ctx_.render_transparent_meshes(*ctx_.world, cmd_buffer, 1);
    }
    for (auto& mod : ctx_.modules) {
        if (mod.instance) {
            mod.instance->OnRenderTransparent(*ctx_.world, cmd_buffer, scene_clip_correction, 1);
        }
    }
    cmd_buffer.EndRenderPass();

    // --- Pass 2: Revealage (blend ZERO, ONE_MINUS_SRC_ALPHA) ---
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.wboit_reveal);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.wboit_reveal, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), true});

    if (ctx_.render_transparent_meshes) {
        ctx_.render_transparent_meshes(*ctx_.world, cmd_buffer, 2);
    }
    for (auto& mod : ctx_.modules) {
        if (mod.instance) {
            mod.instance->OnRenderTransparent(*ctx_.world, cmd_buffer, scene_clip_correction, 2);
        }
    }
    cmd_buffer.EndRenderPass();

    // --- Pass 3: Composite WBOIT onto scene RT ---
    const unsigned int accum_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.wboit_accum);
    const unsigned int reveal_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.wboit_reveal);
    if (accum_tex == 0 || reveal_tex == 0) return;

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.decal_blend);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
    cmd_buffer.DrawPostProcess(accum_tex, "wboit_composite", {
        static_cast<float>(reveal_tex)
    });
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
    graph.PassWrite(pass, scene_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void DecalPass::Execute(CommandBuffer& cmd_buffer) {
    auto decal_view = ctx_.world->registry().view<dse::DecalComponent, TransformComponent>();
    bool has_any = false;
    for (auto entity : decal_view) {
        auto& dc = decal_view.get<dse::DecalComponent>(entity);
        if (dc.enabled && dc.albedo_texture != 0) { has_any = true; break; }
    }
    if (!has_any) return;

    // 获取相机 view/projection 构建 inv_vp
    glm::mat4 view_mat(1.0f), proj_mat(1.0f);
    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
    auto camera_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<dse::Camera3DComponent>(entity);
        if (!cam.enabled) continue;
        float aspect = static_cast<float>(Screen::width()) / static_cast<float>(Screen::height());
        proj_mat = clip_correction * glm::perspective(glm::radians(cam.fov), aspect, cam.near_clip, cam.far_clip);
        if (ctx_.world->registry().all_of<TransformComponent>(entity)) {
            auto& t = ctx_.world->registry().get<TransformComponent>(entity);
            glm::vec3 front = t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 up    = t.rotation * glm::vec3(0.0f, 1.0f,  0.0f);
            view_mat = glm::lookAt(t.position, t.position + front, up);
        }
        break;
    }
    const glm::mat4 inv_vp = glm::inverse(proj_mat * view_mat);

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    const unsigned int scene_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.decal_blend);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});

    // params 布局（26 float）：
    // [0]      depth_tex handle
    // [1]      decal_tex handle
    // [2-17]   inv_model_vp (column-major, 16 floats)
    // [18-21]  color.rgba
    // [22]     angle_fade
    // [23-25]  decal Y-axis in world (用于角度衰减)
    std::vector<float> params(26);
    params[0] = static_cast<float>(depth_tex);

    for (auto entity : decal_view) {
        auto& dc = decal_view.get<dse::DecalComponent>(entity);
        if (!dc.enabled || dc.albedo_texture == 0) continue;
        auto& transform = decal_view.get<TransformComponent>(entity);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position)
                        * glm::mat4_cast(transform.rotation)
                        * glm::scale(glm::mat4(1.0f), transform.scale);
        glm::mat4 inv_model_vp = glm::inverse(model) * inv_vp;
        glm::vec3 decal_up = glm::normalize(glm::vec3(model[1]));

        params[1] = static_cast<float>(dc.albedo_texture);
        const float* m = &inv_model_vp[0][0];
        for (int i = 0; i < 16; ++i) params[2 + i] = m[i];
        params[18] = dc.color.r;
        params[19] = dc.color.g;
        params[20] = dc.color.b;
        params[21] = dc.color.a;
        params[22] = dc.angle_fade;
        params[23] = decal_up.x;
        params[24] = decal_up.y;
        params[25] = decal_up.z;

        cmd_buffer.DrawPostProcess(scene_tex, "decal", params);
    }
    cmd_buffer.EndRenderPass();
}

} // namespace render
} // namespace dse
