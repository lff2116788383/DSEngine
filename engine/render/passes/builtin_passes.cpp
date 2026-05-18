/**
 * @file builtin_passes.cpp
 * @brief 引擎内置渲染 Pass 实现
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
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "engine/base/time.h"

namespace dse {
namespace render {

// ============================================================
// 公共工具：方向光阴影相机计算（CSMShadowPass / RSMRenderPass 共用）
// ============================================================
namespace {

glm::vec3 FindShadowCenter(World& world) {
    glm::vec3 shadow_center(0.0f);
    auto camera3d_view = world.registry().view<dse::Camera3DComponent>();
    for (auto entity : camera3d_view) {
        auto& camera = camera3d_view.get<dse::Camera3DComponent>(entity);
        if (camera.enabled && world.registry().all_of<TransformComponent>(entity)) {
            auto& transform = world.registry().get<TransformComponent>(entity);
            glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            shadow_center = transform.position + front * 50.0f;
            break;
        }
    }
    return shadow_center;
}

struct DirectionalLightCamera {
    glm::mat4 view;
    glm::mat4 projection;
};

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

    glm::vec3 shadow_center = FindShadowCenter(*ctx_.world);

    std::vector<glm::mat4> light_space_matrices(CSM_CASCADES);
    std::vector<float> cascade_splits(CSM_CASCADES);

    for (int i = 0; i < CSM_CASCADES; ++i) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.shadow[i], glm::vec4(1.0f), true});

        float size = light.cascade_splits[i];
        constexpr float shadow_map_res = 2048.0f;
        auto cam = ComputeDirectionalLightCamera(
            shadow_center, light.direction, size, clip_correction, shadow_map_res);

        // Sampling matrix uses shadow_sample_correction (no Z remap)
        float far_dist = size * 4.0f;
        glm::mat4 sample_proj = shadow_sample_correction *
            glm::ortho(-size, size, -size, size, 1.0f, far_dist);
        light_space_matrices[i] = sample_proj * cam.view;
        cascade_splits[i] = light.cascade_splits[i];

        cmd_buffer.SetCamera(cam.view, cam.projection);
        cmd_buffer.SetPipelineState(ctx_.pipeline_states.shadow);

        for (auto& mod : ctx_.modules) {
            if (mod.instance) {
                mod.instance->OnRenderShadow(*ctx_.world, cmd_buffer, i, cam.view, cam.projection);
            }
        }

        cmd_buffer.EndRenderPass();
    }

    for (int i = 0; i < CSM_CASCADES; ++i) {
        ctx_.rhi_device->SetGlobalLightSpaceMatrix(static_cast<unsigned int>(i), light_space_matrices[i]);
        ctx_.rhi_device->SetGlobalCascadeSplit(static_cast<unsigned int>(i), cascade_splits[i]);
    }

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

    // params: [light_dir.xyz, light_color.xyz, intensity, ambient]
    std::vector<float> params;
    params.push_back(light_dir.x); params.push_back(light_dir.y); params.push_back(light_dir.z);
    params.push_back(light_color.x); params.push_back(light_color.y); params.push_back(light_color.z);
    params.push_back(light_intensity);
    params.push_back(ambient_intensity);

    cmd_buffer.DrawPostProcess(PostProcessRequest{"deferred_lighting", gbuf_albedo, std::move(params)}
        .Tex(2, gbuf_normal).Tex(3, gbuf_position));
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

        // GPU Driven Indirect Draw：mega VAO 就绪且有 draw commands 时使用
        const bool use_gpu_indirect = ctx_.gpu_driven_enabled
            && ctx_.gpu_mega_vao != 0
            && ctx_.gpu_draw_cmd_ssbo != 0
            && ctx_.gpu_indirect_draw_count > 0;

        if (use_gpu_indirect) {
            // 绑定 instance SSBO 供 vertex shader 读取 model matrix
            auto* rhi = ctx_.rhi_device;
            rhi->BindSSBO(ctx_.gpu_instance_ssbo, dse::render::gpu_driven::kSSBOBindingInstances);
            rhi->BindMegaVAO(ctx_.gpu_mega_vao);
            rhi->MultiDrawIndexedIndirect(ctx_.gpu_draw_cmd_ssbo,
                                          ctx_.gpu_indirect_draw_count,
                                          sizeof(DrawElementsIndirectCommand));
            rhi->UnbindVAO();
        }

        // CPU 路径：仅在无 module 时走 render_meshes fallback；
        // 有 module 时由 OnRenderScene 负责 mesh 渲染（避免双重绘制）
        if (!use_gpu_indirect) {
            if (ctx_.modules.empty() && ctx_.render_meshes) {
                ctx_.render_meshes(*ctx_.world, cmd_buffer);
            }
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
    cmd_buffer.DrawPostProcess({"bloom_extract", scene_color_tex, {pp_config.bloom_threshold}});
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
        cmd_buffer.DrawPostProcess({"bloom_upsample", current_src, {0.005f}});
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
        cmd_buffer.DrawPostProcess({"bloom_composite", scene_color_tex, {
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

    cmd_buffer.DrawPostProcess({"ui_overlay", ui_color_tex});
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
    cmd_buffer.DrawPostProcess({"lum_compute", scene_color_tex});
    cmd_buffer.EndRenderPass();

    // Pass 2: 64x64 → 1x1 adapted exposure (EMA blend with previous frame)
    const unsigned int lum_temp_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.lum_temp);
    const unsigned int prev_adapted_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.lum_adapted[read_idx]);

    cmd_buffer.BeginRenderPass({ctx_.render_targets.lum_adapted[write_idx], glm::vec4(1.0f), true});
    cmd_buffer.DrawPostProcess(PostProcessRequest{"lum_adapt", lum_temp_tex, {
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
    cmd_buffer.DrawPostProcess({"ssao", depth_tex, {
        pp_config.ssao_radius,
        pp_config.ssao_bias,
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height())
    }});
    cmd_buffer.EndRenderPass();

    // Pass 2: 双边模糊
    const unsigned int ssao_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.ssao);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.ssao_blur, glm::vec4(1.0f), true});
    cmd_buffer.DrawPostProcess({"ssao_blur", ssao_tex});
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
    cmd_buffer.DrawPostProcess({"contact_shadow", depth_tex, {
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
    cmd_buffer.DrawPostProcess({"fxaa", main_color_tex, {
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
    cmd_buffer.DrawPostProcess({"copy", present_tex});
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
        cmd_buffer.DrawPostProcess({"copy", taa_out_tex});
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
    cmd_buffer.DrawPostProcess(PostProcessRequest{"dof", depth_tex, {
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
        cmd_buffer.DrawPostProcess({"copy", dof_tex});
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
        cmd_buffer.DrawPostProcess({"copy", mb_tex});
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
    cmd_buffer.DrawPostProcess(PostProcessRequest{"ssr", depth_tex, {
        pp_config.ssr_max_distance,
        pp_config.ssr_thickness,
        pp_config.ssr_step_size,
        static_cast<float>(pp_config.ssr_max_steps),
        near_plane,
        far_plane,
        static_cast<float>(Screen::width()),
        static_cast<float>(Screen::height())
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
    cmd_buffer.DrawPostProcess({"edge_detect", depth_tex, {
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
    auto pp_view = ctx_.world->registry().view<dse::PostProcessComponent>();
    const dse::PostProcessComponent* pp = nullptr;
    for (auto entity : pp_view) {
        auto& p = pp_view.get<dse::PostProcessComponent>(entity);
        if (p.enabled && p.light_shaft_enabled) { pp = &p; break; }
    }
    if (!pp) return;

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    glm::vec3 cam_fwd{0,0,-1}, cam_right{1,0,0}, cam_up{0,1,0};
    float fov_y = 60.0f;
    float aspect = static_cast<float>(Screen::width()) / static_cast<float>(Screen::height());

    auto camera_view = ctx_.world->registry().view<dse::Camera3DComponent>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<dse::Camera3DComponent>(entity);
        if (!cam.enabled) continue;
        fov_y = cam.fov;
        if (ctx_.world->registry().all_of<TransformComponent>(entity)) {
            auto& t = ctx_.world->registry().get<TransformComponent>(entity);
            cam_fwd   = glm::normalize(t.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            cam_right = glm::normalize(t.rotation * glm::vec3(1.0f, 0.0f,  0.0f));
            cam_up    = glm::normalize(t.rotation * glm::vec3(0.0f, 1.0f,  0.0f));
        }
        break;
    }
    const float tan_fov_y = std::tan(glm::radians(fov_y) * 0.5f);

    glm::vec3 sun_dir{0.0f, -1.0f, 0.0f};
    auto light_view = ctx_.world->registry().view<dse::DirectionalLight3DComponent>();
    for (auto entity : light_view) {
        auto& l = light_view.get<dse::DirectionalLight3DComponent>(entity);
        if (l.enabled) { sun_dir = glm::normalize(l.direction); break; }
    }

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
    cmd_buffer.DrawPostProcess(PostProcessRequest{"volumetric_fog", scene_tex, {
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
    }}.Tex(2, depth_tex));
    cmd_buffer.EndRenderPass();

    // 将雾效结果（已包含 scene 颜色）覆写回 scene RT
    const unsigned int fog_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.fog);
    if (fog_tex != 0) {
        cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
        cmd_buffer.DrawPostProcess({"copy", fog_tex});
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
    auto water_view = ctx_.world->registry().view<dse::WaterComponent>();
    bool has_water = false;
    for (auto entity : water_view) {
        if (water_view.get<dse::WaterComponent>(entity).enabled) { has_water = true; break; }
    }
    if (!has_water) return;

    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;
    const unsigned int scene_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);

    // 获取相机信息
    glm::mat4 view_mat(1.0f);
    float cam_fov = 60.0f, cam_near = 0.1f, cam_far = 1000.0f;
    glm::vec3 cam_pos(0.0f);
    bool found_camera = false;

    if (ctx_.editor_mode && ctx_.use_editor_camera) {
        found_camera = true;
        cam_pos = glm::vec3(glm::inverse(ctx_.editor_view)[3]);
        view_mat = ctx_.editor_view;
        // 从 projection 中估算 fov 不太精确，使用默认值
    } else {
        auto camera_view = ctx_.world->registry().view<dse::Camera3DComponent>();
        for (auto entity : camera_view) {
            auto& cam = camera_view.get<dse::Camera3DComponent>(entity);
            if (!cam.enabled) continue;
            cam_fov = cam.fov;
            cam_near = cam.near_clip;
            cam_far = cam.far_clip;
            if (ctx_.world->registry().all_of<TransformComponent>(entity)) {
                auto& t = ctx_.world->registry().get<TransformComponent>(entity);
                glm::vec3 front = t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 up    = t.rotation * glm::vec3(0.0f, 1.0f,  0.0f);
                view_mat = glm::lookAt(t.position, t.position + front, up);
                cam_pos = t.position;
            }
            found_camera = true;
            break;
        }
    }
    if (!found_camera) return;

    glm::mat4 inv_view = glm::inverse(view_mat);
    glm::vec3 cam_fwd   = -glm::normalize(glm::vec3(inv_view[2]));
    float aspect = static_cast<float>(Screen::width()) / static_cast<float>(std::max(1, Screen::height()));
    float tan_fov_y = std::tan(glm::radians(cam_fov) * 0.5f);

    // 获取太阳光方向
    glm::vec3 sun_dir(0.0f, -1.0f, 0.0f);
    auto light_view = ctx_.world->registry().view<dse::DirectionalLight3DComponent>();
    for (auto entity : light_view) {
        auto& light = light_view.get<dse::DirectionalLight3DComponent>(entity);
        if (light.enabled) {
            sun_dir = glm::normalize(light.direction);
            break;
        }
    }

    const float current_time = Time::TimeSinceStartup();

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.decal_blend);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});

    // params 布局（40 float = 160 bytes）
    std::vector<float> params(39);

    for (auto entity : water_view) {
        auto& wc = water_view.get<dse::WaterComponent>(entity);
        if (!wc.enabled) continue;

        glm::vec2 wave_dir = glm::length(wc.wave_direction) > 0.001f
            ? glm::normalize(wc.wave_direction) : glm::vec2(1.0f, 0.0f);

        params[0]  = wc.water_level;
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
    std::vector<float> params(24);

    for (auto entity : decal_view) {
        auto& dc = decal_view.get<dse::DecalComponent>(entity);
        if (!dc.enabled || dc.albedo_texture == 0) continue;
        auto& transform = decal_view.get<TransformComponent>(entity);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position)
                        * glm::mat4_cast(transform.rotation)
                        * glm::scale(glm::mat4(1.0f), transform.scale);
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
layout(local_size_x = 16, local_size_y = 16) in;

uniform sampler2D u_depth_texture;
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

const char* kHiZCullShaderSource = R"(
#version 430 core
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

uniform sampler2D u_hiz_texture;
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
    float test_depth = nearest_z * 0.5 + 0.5;

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

        rhi->SetComputeUniformInt(hiz_copy_shader_, "u_depth_texture", 0);
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
    graph.PassWrite(pass, hiz_visibility);
    graph.MarkOutput(hiz_visibility);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void HiZCullPass::Execute(CommandBuffer& /*cmd_buffer*/) {
    EnsureShader();
    if (hiz_cull_shader_ == 0) return;
    if (ctx_.render_targets.hiz_texture == 0) return;
    if (ctx_.hiz_aabb_ssbo == 0 || ctx_.hiz_visibility_ssbo == 0) return;
    if (ctx_.hiz_object_count <= 0) return;

    auto* rhi = ctx_.rhi_device;
    if (!rhi) return;

    const unsigned int hiz_gpu_tex = rhi->GetHiZGpuTexture(ctx_.render_targets.hiz_texture);
    if (hiz_gpu_tex == 0) return;

    const int mip_count = rhi->GetHiZMipCount(ctx_.render_targets.hiz_texture);

    // Bind SSBOs
    rhi->BindSSBO(ctx_.hiz_aabb_ssbo, 0);
    rhi->BindSSBO(ctx_.hiz_visibility_ssbo, 1);

    // Bind Hi-Z texture as sampler
    rhi->SetComputeTextureSampler(0, hiz_gpu_tex);

    // Get current camera VP matrix for AABB projection
    glm::mat4 view_projection(1.0f);
    {
        auto camera3d_view = ctx_.world->registry().view<dse::Camera3DComponent>();
        for (auto entity : camera3d_view) {
            auto& camera = camera3d_view.get<dse::Camera3DComponent>(entity);
            if (!camera.enabled) continue;
            if (!ctx_.world->registry().all_of<TransformComponent>(entity)) continue;
            auto& transform = ctx_.world->registry().get<TransformComponent>(entity);
            glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            glm::mat4 view = glm::lookAt(transform.position, transform.position + front, up);
            const glm::mat4 clip_correction = rhi->GetProjectionCorrection();
            glm::mat4 projection = clip_correction * glm::perspective(
                glm::radians(camera.fov),
                static_cast<float>(Screen::width()) / static_cast<float>(std::max(1, Screen::height())),
                camera.near_clip, camera.far_clip);
            view_projection = projection * view;
            break;
        }
    }

    // Set uniforms through RHI
    rhi->SetComputeUniformInt(hiz_cull_shader_, "u_hiz_texture", 0);
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

layout(std430, binding = 6) buffer DrawCommandBuffer {
    DrawCommand draw_cmds[];
};

uniform sampler2D u_hiz_texture;
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

    float test_depth = nearest_z * 0.5 + 0.5;

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

void GPUCullPass::Setup(RenderGraph& graph) {
    auto hiz_mips = graph.DeclareResource("hiz_mips");
    auto gpu_draw_cmds = graph.DeclareResource("gpu_draw_commands");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, hiz_mips);
    graph.PassWrite(pass, gpu_draw_cmds);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void GPUCullPass::Execute(CommandBuffer& /*cmd_buffer*/) {
    if (!ctx_.gpu_driven_enabled) return;
    if (ctx_.gpu_cull_shader == 0) return;
    if (ctx_.render_targets.hiz_texture == 0) return;
    if (ctx_.gpu_draw_cmd_ssbo == 0) return;
    if (ctx_.gpu_indirect_draw_count <= 0) return;

    auto* rhi = ctx_.rhi_device;
    if (!rhi) return;

    const unsigned int hiz_gpu_tex = rhi->GetHiZGpuTexture(ctx_.render_targets.hiz_texture);
    if (hiz_gpu_tex == 0) return;

    const int mip_count = rhi->GetHiZMipCount(ctx_.render_targets.hiz_texture);

    // Bind SSBOs
    rhi->BindSSBO(ctx_.hiz_aabb_ssbo, 0);           // AABB input
    rhi->BindSSBO(ctx_.gpu_draw_cmd_ssbo, 6);        // DrawCommands (read/write)

    // Bind Hi-Z texture
    rhi->SetComputeTextureSampler(0, hiz_gpu_tex);

    // Get VP matrix
    glm::mat4 view_projection(1.0f);
    glm::vec4 frustum_planes[6] = {};
    {
        auto camera3d_view = ctx_.world->registry().view<dse::Camera3DComponent>();
        for (auto entity : camera3d_view) {
            auto& camera = camera3d_view.get<dse::Camera3DComponent>(entity);
            if (!camera.enabled) continue;
            if (!ctx_.world->registry().all_of<TransformComponent>(entity)) continue;
            auto& transform = ctx_.world->registry().get<TransformComponent>(entity);
            glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            glm::mat4 view = glm::lookAt(transform.position, transform.position + front, up);
            const glm::mat4 clip_correction = rhi->GetProjectionCorrection();
            glm::mat4 projection = clip_correction * glm::perspective(
                glm::radians(camera.fov),
                static_cast<float>(Screen::width()) / static_cast<float>(std::max(1, Screen::height())),
                camera.near_clip, camera.far_clip);
            view_projection = projection * view;

            // Extract frustum planes from VP matrix (Gribb/Hartmann method)
            const glm::mat4& m = view_projection;
            // Left
            frustum_planes[0] = glm::vec4(m[0][3]+m[0][0], m[1][3]+m[1][0], m[2][3]+m[2][0], m[3][3]+m[3][0]);
            // Right
            frustum_planes[1] = glm::vec4(m[0][3]-m[0][0], m[1][3]-m[1][0], m[2][3]-m[2][0], m[3][3]-m[3][0]);
            // Bottom
            frustum_planes[2] = glm::vec4(m[0][3]+m[0][1], m[1][3]+m[1][1], m[2][3]+m[2][1], m[3][3]+m[3][1]);
            // Top
            frustum_planes[3] = glm::vec4(m[0][3]-m[0][1], m[1][3]-m[1][1], m[2][3]-m[2][1], m[3][3]-m[3][1]);
            // Near
            frustum_planes[4] = glm::vec4(m[0][3]+m[0][2], m[1][3]+m[1][2], m[2][3]+m[2][2], m[3][3]+m[3][2]);
            // Far
            frustum_planes[5] = glm::vec4(m[0][3]-m[0][2], m[1][3]-m[1][2], m[2][3]-m[2][2], m[3][3]-m[3][2]);

            // Normalize planes
            for (int i = 0; i < 6; ++i) {
                float len = glm::length(glm::vec3(frustum_planes[i]));
                if (len > 0.0f) frustum_planes[i] /= len;
            }
            break;
        }
    }

    // Set uniforms
    unsigned int shader = ctx_.gpu_cull_shader;
    rhi->SetComputeUniformInt(shader, "u_hiz_texture", 0);
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

    auto light_view = ctx_.world->registry().view<dse::DirectionalLight3DComponent>();
    if (light_view.begin() == light_view.end()) return;
    auto& light = light_view.get<dse::DirectionalLight3DComponent>(*light_view.begin());
    if (!light.enabled) return;

    glm::vec3 shadow_center = FindShadowCenter(*ctx_.world);
    const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
    auto cam = ComputeDirectionalLightCamera(
        shadow_center, light.direction, light.cascade_splits[0], clip_correction);

    cmd_buffer.BeginRenderPass({ctx_.rsm_render_target, glm::vec4(0.0f), true});
    ctx_.rhi_device->SetGBufferRenderingMode(true);
    cmd_buffer.SetCamera(cam.view, cam.projection);
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.mesh);

    for (auto& mod : ctx_.modules) {
        if (mod.instance) {
            mod.instance->OnRenderScene(*ctx_.world, cmd_buffer);
        }
    }

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
    graph.PassWrite(pass, ddgi_atlas);
    graph.MarkOutput(ddgi_atlas);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void DDGIUpdatePass::Execute(CommandBuffer& /*cmd_buffer*/) {
    if (!ctx_.ddgi_active || !ctx_.ddgi_system) return;

    auto* rhi = ctx_.rhi_device;
    if (!rhi || !rhi->SupportsCompute()) return;

    // 获取主方向光参数
    glm::vec3 light_dir(0.0f, -1.0f, 0.0f);
    glm::vec3 light_color(1.0f);

    auto light_view = ctx_.world->registry().view<dse::DirectionalLight3DComponent>();
    for (auto entity : light_view) {
        auto& light = light_view.get<dse::DirectionalLight3DComponent>(entity);
        if (!light.enabled) continue;
        light_dir = glm::normalize(-light.direction);  // toward light
        light_color = glm::vec3(light.color) * light.intensity;
        break;
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

} // namespace render
} // namespace dse
