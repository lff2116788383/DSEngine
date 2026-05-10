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
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <cstdint>
#include <cmath>
#include <algorithm>

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
        glm::mat4 light_proj = clip_correction * glm::ortho(-size, size, -size, size, 1.0f, far_dist);
        glm::vec3 light_dir_n = glm::normalize(light.direction);
        glm::vec3 light_pos = shadow_center - light_dir_n * (far_dist * 0.5f);
        glm::mat4 light_view_mat = glm::lookAt(light_pos, shadow_center, glm::vec3(0.0f, 1.0f, 0.0f));

        light_space_matrices[i] = light_proj * light_view_mat;
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
        spot_light_space_matrices.push_back(light_proj * light_view_mat);
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

    // Editor camera override: use editor view/proj for Scene render target
    if (ctx_.editor_mode && ctx_.use_editor_camera) {
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
    auto main_color  = graph.DeclareResource("main_color");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, ui_color);
    graph.PassRead(pass, bloom_mip0);
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

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.main, glm::vec4(0.0f), true});

    if (pp_enabled && pp_config.bloom_enabled) {
        const unsigned int blur_v_color = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.bloom_mips.empty() ? 0 : ctx_.render_targets.bloom_mips[0]);
        cmd_buffer.DrawPostProcess(scene_color_tex, "bloom_composite", {static_cast<float>(blur_v_color), pp_config.exposure, pp_config.bloom_intensity});
    } else {
        cmd_buffer.DrawPostProcess(scene_color_tex, "copy", {});
    }

    cmd_buffer.DrawPostProcess(ui_color_tex, "ui_overlay", {});
    cmd_buffer.EndRenderPass();
}

// ============================================================
// PresentPass
// ============================================================

void PresentPass::Setup(RenderGraph& graph) {
    auto main_color = graph.DeclareResource("main_color");
    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, main_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void PresentPass::Execute(CommandBuffer& cmd_buffer) {
    const unsigned int main_color_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.main);
    if (main_color_tex == 0) {
        return;
    }
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({0, glm::vec4(0.0f), true});
    cmd_buffer.DrawPostProcess(main_color_tex, "copy", {});
    cmd_buffer.EndRenderPass();
}

} // namespace render
} // namespace dse
