/**
 * @file frame_pipeline.cpp
 * @brief 引擎主循环与帧流水线，协调更新、物理和渲染的执行顺序
 */

#include "engine/runtime/frame_pipeline.h"
#include "engine/runtime/runtime_frame_ops.h"
#include "engine/base/debug.h"
#include "engine/base/time.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"
#include "engine/assets/asset_manager.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/core/event_bus.h"
#include "engine/scene/scene.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <stdexcept>
#include <cassert>
#include <chrono>
#include <utility>
#include <vector>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <sstream>

namespace {
AssetManager& RequireAssetManager(AssetManager* asset_manager) {
    if (asset_manager) {
        return *asset_manager;
    }
    throw std::runtime_error("FramePipeline requires an injected AssetManager");
}

std::vector<std::string> ResolveRuntimeModules() {
    if (const char* env_modules = std::getenv("DSE_RUNTIME_MODULES")) {
        if (env_modules[0] != '\0') {
            std::vector<std::string> modules;
            std::stringstream stream(env_modules);
            std::string item;
            while (std::getline(stream, item, ',')) {
                if (!item.empty()) {
                    modules.push_back(item);
                }
            }
            return modules;
        }
    }
    return {};
}
}

FramePipeline& FramePipeline::Instance() {
    static FramePipeline instance;
    return instance;
}

bool FramePipeline::Init() {
    if (initialized_) {
        return true;
    }
    if (!runtime_context_.world) {
        DEBUG_LOG_ERROR("FramePipeline init failed: world is not injected");
        return false;
    }
    auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
    runtime_context_.rhi_device = std::make_unique<OpenGLRhiDevice>();
    asset_manager.SetRhiDevice(runtime_context_.rhi_device.get());
    std::string data_root = "data";
    if (const char* env_data_root = std::getenv("DSE_DATA_ROOT")) {
        if (env_data_root[0] != '\0') {
            data_root = env_data_root;
        }
    } else {
        const std::vector<std::string> data_candidates = {
            "data",
            "../data",
            "../../data"
        };
        for (const auto& candidate : data_candidates) {
            if (std::filesystem::exists(candidate)) {
                data_root = candidate;
                break;
            }
        }
    }
    asset_manager.ConfigureDataRoot(data_root);
    if (const char* env_budget = std::getenv("DSE_ASYNC_UPLOAD_BUDGET")) {
        int budget = std::atoi(env_budget);
        if (budget > 0) {
            callback_budget_per_frame_ = static_cast<std::size_t>(budget);
        }
    }
    DEBUG_LOG_INFO("Runtime data root: {}", asset_manager.GetDataRoot());
    if (Screen::width() <= 0 || Screen::height() <= 0) {
        Screen::set_width_height(1280, 720);
    }
    const int render_width = Screen::width();
    const int render_height = Screen::height();
    
    if (runtime_context_.editor_mode) {
        // 在编辑器模式下，将最终合成结果输出到一个纹理中，而不是直接输出到屏幕
        render_resources_.main_render_target = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, false, false});
    } else {
        render_resources_.main_render_target = 0;
    }
    
    // 使用支持 HDR 的浮点纹理作为 Scene Render Target，这是泛光和色调映射的基础
    render_resources_.scene_render_target = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, true}); // Enable depth for scene pass
    render_resources_.ui_render_target = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false});
    render_resources_.prez_render_target = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, false, true}); // Only depth
    
    for (int i = 0; i < CSM_CASCADES; ++i) {
        render_resources_.shadow_render_target[i] = runtime_context_.rhi_device->CreateRenderTarget({2048, 2048, false, true}); // Shadow map resolution
    }

    if (render_resources_.pp_bloom_extract_rt == 0) {
        render_resources_.pp_bloom_extract_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    }
    
    // Create mip chain for Dual Filter Bloom (5 levels)
    if (render_resources_.pp_bloom_mip_rts.empty()) {
        int mip_width = render_width / 2;
        int mip_height = render_height / 2;
        for (int i = 0; i < 5; ++i) {
            render_resources_.pp_bloom_mip_rts.push_back(runtime_context_.rhi_device->CreateRenderTarget({mip_width, mip_height, true, false, false}));
            mip_width /= 2;
            mip_height /= 2;
            if (mip_width < 1) mip_width = 1;
            if (mip_height < 1) mip_height = 1;
        }
    }

    render_resources_.sprite_pipeline_state = runtime_context_.rhi_device->CreatePipelineState({true, 0x0302, 0x0303});
    
    PipelineStateDesc mesh_desc;
    mesh_desc.blend_enabled = false;
    mesh_desc.depth_test_enabled = true;
    mesh_desc.depth_write_enabled = true;
    mesh_desc.culling_enabled = true;
    render_resources_.mesh_pipeline_state = runtime_context_.rhi_device->CreatePipelineState(mesh_desc);

    PipelineStateDesc prez_desc;
    prez_desc.blend_enabled = false;
    prez_desc.depth_test_enabled = true;
    prez_desc.depth_write_enabled = true;
    prez_desc.culling_enabled = true;
    // In a real engine we disable color write, but for now we'll just write to a depth-only FBO
    render_resources_.prez_pipeline_state = runtime_context_.rhi_device->CreatePipelineState(prez_desc);

    PipelineStateDesc shadow_desc;
    shadow_desc.blend_enabled = false;
    shadow_desc.depth_test_enabled = true;
    shadow_desc.depth_write_enabled = true;
    shadow_desc.culling_enabled = true;
    shadow_desc.cull_face = 0x0404; // GL_FRONT to avoid peter-panning
    render_resources_.shadow_pipeline_state = runtime_context_.rhi_device->CreatePipelineState(shadow_desc);
    
    render_resources_.composite_pipeline_state = runtime_context_.rhi_device->CreatePipelineState({false, 0x0302, 0x0303});
    
    physics2d_system_.Init(*runtime_context_.world);
    spine_system_.SetAssetManager(&asset_manager);
    audio_system_.Initialize(&asset_manager);
    mesh_render_system_.SetAssetManager(&asset_manager);

#ifdef DSE_ENABLE_3D
    const auto runtime_modules = ResolveRuntimeModules();
    const bool enable_gameplay3d = runtime_modules.empty() ||
        std::find(runtime_modules.begin(), runtime_modules.end(), "Gameplay3D") != runtime_modules.end();
    if (enable_gameplay3d) {
        auto lib_3d = std::make_unique<dse::core::DynamicLibrary>();
        const std::vector<std::string> candidates = {
            "DSE_Gameplay3D_debug",
            "DSE_Gameplay3D_release",
            "DSE_Gameplay3D_relwithdebinfo",
            "DSE_Gameplay3D_minsizerel",
            "DSE_Gameplay3D"
        };
        bool loaded = false;
        for (const auto& candidate : candidates) {
            if (lib_3d->Load(candidate)) {
                loaded = true;
                break;
            }
        }
        if (loaded) {
            using CreateModuleFunc = dse::core::IModule*(*)();
            using DestroyModuleFunc = void(*)(dse::core::IModule*);
            auto create_func = reinterpret_cast<CreateModuleFunc>(lib_3d->GetSymbol("CreateModule"));
            auto destroy_func = reinterpret_cast<DestroyModuleFunc>(lib_3d->GetSymbol("DestroyModule"));
            if (create_func && destroy_func) {
                dse::core::IModule* module_instance = create_func();
                if (module_instance) {
                    if (!module_instance->OnInit(*runtime_context_.world, runtime_context_.rhi_device.get(), &asset_manager)) {
                        destroy_func(module_instance);
                    } else {
                        modules_.push_back({std::move(lib_3d), module_instance, destroy_func});
                    }
                }
            }
        }
    }
#endif

    bool scene_round_trip_ok = scene::RunSceneRoundTripRegressionSample("bin/scene_roundtrip_regression.json");
    DEBUG_LOG_INFO("Scene round-trip regression: {}", scene_round_trip_ok ? "PASSED" : "FAILED");
    bool scene_backward_compat_ok = scene::RunSceneBackwardCompatibilityRegressionSample("bin/scene_backward_compat_regression.json");
    DEBUG_LOG_INFO("Scene backward-compat regression: {}", scene_backward_compat_ok ? "PASSED" : "FAILED");
    
    const bool business_bootstrap_ok = dse::runtime::BootstrapBusinessRuntime(runtime_context_, {
        [this]() { return LastDrawCalls(); },
        [this]() { return LastMaxBatchSprites(); },
        [this]() { return LastSpriteCount(); }
    });
    DEBUG_LOG_INFO("{} business mode bootstrap: {}",
                   runtime_context_.business_mode == BusinessMode::Lua ? "Lua" : "Cpp",
                   business_bootstrap_ok ? "OK" : "FAILED");
    if (!business_bootstrap_ok) {
        Shutdown();
        return false;
    }
    BuildRenderGraph();

    initialized_ = true;
    dse::core::EventBus::Instance().Publish<dse::core::SceneLifecycleEvent>(dse::core::SceneLifecyclePhase::Init);
    return true;
}

void FramePipeline::Shutdown() {
    if (!initialized_) {
        return;
    }
    dse::core::EventBus::Instance().Publish<dse::core::SceneLifecycleEvent>(dse::core::SceneLifecyclePhase::Shutdown);
    auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
    render_graph_passes_.clear();
    dse::runtime::ShutdownBusinessRuntime(runtime_context_);
    audio_system_.Shutdown();
    physics2d_system_.Shutdown();
    spine_system_.Shutdown(runtime_context_.world->registry());
    spine_system_.SetAssetManager(nullptr);
    mesh_render_system_.SetAssetManager(nullptr);
    asset_manager.ReleaseGpuResources();
    
    for (auto& mod : modules_) {
        if (mod.instance) {
            mod.instance->OnShutdown(*runtime_context_.world);
            if (mod.destroy) {
                mod.destroy(mod.instance);
            }
        }
    }
    modules_.clear();

    if (runtime_context_.rhi_device) {
        runtime_context_.rhi_device->Shutdown();
        runtime_context_.rhi_device.reset();
    }
    asset_manager.SetRhiDevice(nullptr);
    render_resources_.Reset();
    update_time_accumulator_ms_ = 0.0f;
    fixed_time_accumulator_ms_ = 0.0f;
    render_time_accumulator_ms_ = 0.0f;
    update_samples_ = 0;
    fixed_samples_ = 0;
    render_samples_ = 0;
    initialized_ = false;
}

void FramePipeline::Update(float delta_time) {
    if (!initialized_) {
        return;
    }
    dse::runtime::RunFrameUpdate(*this, delta_time);
}

void FramePipeline::FixedUpdate(float fixed_delta_time) {
    if (!initialized_) {
        return;
    }
    dse::runtime::RunFrameFixedUpdate(*this, fixed_delta_time);
}

void FramePipeline::Render() {
    if (!initialized_) {
        return;
    }
    dse::runtime::RunFrameRender(*this);
}

void FramePipeline::RunUpdateInternal(float delta_time) {
    auto update_begin = std::chrono::high_resolution_clock::now();
    auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
    asset_manager.PumpMainThreadCallbacks(callback_budget_per_frame_);
    dse::runtime::TickBusinessRuntime(runtime_context_, delta_time);
    
    dse::runtime::RunRuntimeUpdateGraph(*this, delta_time);
    
    auto update_end = std::chrono::high_resolution_clock::now();
    update_time_accumulator_ms_ += std::chrono::duration<float, std::milli>(update_end - update_begin).count();
    update_samples_ += 1;
}

void FramePipeline::RunFixedUpdateInternal(float fixed_delta_time) {
    auto fixed_begin = std::chrono::high_resolution_clock::now();
    dse::runtime::RunRuntimeFixedUpdateGraph(*this, fixed_delta_time);
    
    auto fixed_end = std::chrono::high_resolution_clock::now();
    fixed_time_accumulator_ms_ += std::chrono::duration<float, std::milli>(fixed_end - fixed_begin).count();
    fixed_samples_ += 1;
}

void FramePipeline::RunRenderInternal() {
    auto render_begin = std::chrono::high_resolution_clock::now();
    runtime_context_.rhi_device->BeginFrame();
    
    auto cmd_buffer = runtime_context_.rhi_device->CreateCommandBuffer();
    
    dse::runtime::BindRuntimeShadowMaps(*this);

    ExecuteRenderGraph(*cmd_buffer);
    
    runtime_context_.rhi_device->Submit(cmd_buffer);
    
    runtime_context_.rhi_device->EndFrame();
    dse::runtime::FinalizeRuntimeRenderFrame(*this);
    const auto& frame_stats = runtime_context_.rhi_device->LastFrameStats();
    auto render_end = std::chrono::high_resolution_clock::now();
    render_time_accumulator_ms_ += std::chrono::duration<float, std::milli>(render_end - render_begin).count();
    render_samples_ += 1;

    stats_accumulator_ += Time::delta_time();
    if (stats_accumulator_ >= 1.0f) {
        stats_accumulator_ = 0.0f;
        const auto& stats = frame_stats;
        size_t entity_count = runtime_context_.world->EntityCount();
        size_t physics_bodies = 0;
        auto physics_view = runtime_context_.world->registry().view<RigidBody2DComponent>();
        for (auto entity : physics_view) {
            (void)entity;
            ++physics_bodies;
        }
        size_t particle_emitters = 0;
        size_t active_particles = 0;
        auto particle_view = runtime_context_.world->registry().view<ParticleEmitterComponent>();
        for (auto emitter_entity : particle_view) {
            auto& emitter = particle_view.get<ParticleEmitterComponent>(emitter_entity);
            (void)emitter_entity;
            ++particle_emitters;
            active_particles += emitter.particles.size();
        }
        float avg_update_ms = update_samples_ > 0 ? update_time_accumulator_ms_ / static_cast<float>(update_samples_) : 0.0f;
        float avg_fixed_ms = fixed_samples_ > 0 ? fixed_time_accumulator_ms_ / static_cast<float>(fixed_samples_) : 0.0f;
        float avg_render_ms = render_samples_ > 0 ? render_time_accumulator_ms_ / static_cast<float>(render_samples_) : 0.0f;
        auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
        std::size_t pending_callbacks = asset_manager.PendingMainThreadCallbacks();
        std::size_t pending_callbacks_hwm = asset_manager.PendingMainThreadCallbacksHighWatermark();
        DEBUG_LOG_INFO("Runtime stats: entities={}, sprites={}, draw_calls={}, max_batch_sprites={}, render_passes={}, physics_bodies={}, particle_emitters={}, active_particles={}, avg_update_ms={:.3f}, avg_fixed_ms={:.3f}, avg_render_ms={:.3f}, pending_upload_callbacks={}, pending_upload_callbacks_hwm={}, upload_budget={}",
                       entity_count,
                       stats.sprite_count,
                       stats.draw_calls,
                       stats.max_batch_sprites,
                       stats.render_passes,
                       physics_bodies,
                       particle_emitters,
                       active_particles,
                       avg_update_ms,
                       avg_fixed_ms,
                       avg_render_ms,
                       pending_callbacks,
                       pending_callbacks_hwm,
                       callback_budget_per_frame_);
        update_time_accumulator_ms_ = 0.0f;
        fixed_time_accumulator_ms_ = 0.0f;
        render_time_accumulator_ms_ = 0.0f;
        update_samples_ = 0;
        fixed_samples_ = 0;
        render_samples_ = 0;
    }
}

void FramePipeline::BuildRenderGraph() {
    dse::runtime::BuildFrameRenderGraph(*this);
}

void FramePipeline::BuildRenderGraphInternal() {
    render_graph_passes_.clear();
    
    // 0. PreZ Pass
    render_graph_passes_.push_back({
        "prez_pass",
        [this](CommandBuffer& cmd_buffer) {
            cmd_buffer.BeginRenderPass({render_resources_.prez_render_target, glm::vec4(0.0f), true});
            auto camera3d_view = runtime_context_.world->registry().view<dse::Camera3DComponent>();
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
                glm::mat4 projection = glm::perspective(glm::radians(camera.fov),
                                                        static_cast<float>(Screen::width()) / static_cast<float>(Screen::height()),
                                                        camera.near_clip, camera.far_clip);
                glm::mat4 view = glm::mat4(1.0f);
                if (runtime_context_.world->registry().all_of<TransformComponent>(selected_camera3d)) {
                    auto& transform = runtime_context_.world->registry().get<TransformComponent>(selected_camera3d);
                    glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                    glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                    view = glm::lookAt(transform.position, transform.position + front, up);
                }
                cmd_buffer.SetCamera(view, projection);
                cmd_buffer.SetPipelineState(render_resources_.prez_pipeline_state);
                
                // 分发 PreZ 渲染事件
                for (auto& mod : modules_) {
                    if (mod.instance) {
                        mod.instance->OnRenderPreZ(*runtime_context_.world, cmd_buffer);
                    }
                }
            }
            cmd_buffer.EndRenderPass();
        }
    });

    // 1. Shadow Map Pass
    render_graph_passes_.push_back({
        "shadow_pass",
        [this](CommandBuffer& cmd_buffer) {
            auto light_view = runtime_context_.world->registry().view<dse::DirectionalLight3DComponent>();
            if (light_view.begin() == light_view.end()) return;
            auto& light = light_view.get<dse::DirectionalLight3DComponent>(*light_view.begin());
            if (!light.enabled || !light.cast_shadow) return;

            std::vector<glm::mat4> light_space_matrices(CSM_CASCADES);
            std::vector<float> cascade_splits(CSM_CASCADES);

            for (int i = 0; i < CSM_CASCADES; ++i) {
                cmd_buffer.BeginRenderPass({render_resources_.shadow_render_target[i], glm::vec4(1.0f), true}); // Clear depth to 1.0
                
                // Simple ortho projection for cascades (in a full implementation, this should bound the view frustum splits)
                // Here we just scale up the orthographic size based on the cascade level
                float size = 20.0f * std::pow(2.0f, static_cast<float>(i));
                glm::mat4 light_proj = glm::ortho(-size, size, -size, size, 1.0f, 200.0f);
                glm::vec3 light_pos = -glm::normalize(light.direction) * 100.0f;
                glm::mat4 light_view_mat = glm::lookAt(light_pos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                
                light_space_matrices[i] = light_proj * light_view_mat;
                cascade_splits[i] = light.cascade_splits[i];

                cmd_buffer.SetCamera(light_view_mat, light_proj);
                cmd_buffer.SetPipelineState(render_resources_.shadow_pipeline_state);
                
                // 分发 Shadow 渲染事件
                for (auto& mod : modules_) {
                    if (mod.instance) {
                        mod.instance->OnRenderShadow(*runtime_context_.world, cmd_buffer, i, light_view_mat, light_proj);
                    }
                }
                
                cmd_buffer.EndRenderPass();
            }

            cmd_buffer.SetGlobalMat4Array("u_light_space_matrices", light_space_matrices);
            cmd_buffer.SetGlobalFloatArray("u_cascade_splits", cascade_splits);

            for (int i = 0; i < CSM_CASCADES; ++i) {
                if (auto* device = dynamic_cast<OpenGLRhiDevice*>(runtime_context_.rhi_device.get())) {
                    device->SetGlobalShadowMap(i, runtime_context_.rhi_device->GetRenderTargetDepthTexture(render_resources_.shadow_render_target[i]));
                }
            }
        }
    });

    // 2. Main Scene Pass
    render_graph_passes_.push_back({
        "scene_pass",
        [this](CommandBuffer& cmd_buffer) {
            cmd_buffer.BeginRenderPass({render_resources_.scene_render_target, glm::vec4(0.02f, 0.02f, 0.02f, 1.0f), true});
            
            auto camera3d_view = runtime_context_.world->registry().view<dse::Camera3DComponent>();
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
                glm::mat4 projection = glm::perspective(glm::radians(camera.fov),
                                                        static_cast<float>(Screen::width()) / static_cast<float>(Screen::height()),
                                                        camera.near_clip, camera.far_clip);
                glm::mat4 view = glm::mat4(1.0f);
                if (runtime_context_.world->registry().all_of<TransformComponent>(selected_camera3d)) {
                    auto& transform = runtime_context_.world->registry().get<TransformComponent>(selected_camera3d);
                    glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                    glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                    view = glm::lookAt(transform.position, transform.position + front, up);
                }
                cmd_buffer.SetCamera(view, projection);
                
                // Draw Skybox
                auto skybox_view = runtime_context_.world->registry().view<dse::SkyboxComponent>();
                for (auto sky_entity : skybox_view) {
                    auto& skybox = skybox_view.get<dse::SkyboxComponent>(sky_entity);
                    if (skybox.enabled) {
                        // Use the cubemap_handle directly
                        cmd_buffer.DrawSkybox(skybox.cubemap_handle);
                        break;
                    }
                }
            } else {
                auto camera_view = runtime_context_.world->registry().view<CameraComponent>();
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
                    cmd_buffer.SetCamera(camera.view, camera.projection);
                }
            }
            
            // 3D Mesh & Particle rendering
            cmd_buffer.SetPipelineState(render_resources_.mesh_pipeline_state);
            
            // 分发 Scene 渲染事件
            for (auto& mod : modules_) {
                if (mod.instance) {
                    mod.instance->OnRenderScene(*runtime_context_.world, cmd_buffer);
                }
            }
            
            // 2D Sprite/Particle/Spine rendering
            cmd_buffer.SetPipelineState(render_resources_.sprite_pipeline_state);
            sprite_render_system_.Render(*runtime_context_.world, cmd_buffer);
            spine_system_.Render(*runtime_context_.world, cmd_buffer);
            particle_system_.Render(*runtime_context_.world, cmd_buffer);
            cmd_buffer.EndRenderPass();
        }
    });

    render_graph_passes_.push_back({
        "post_process_pass",
        [this](CommandBuffer& cmd_buffer) {
            auto pp_view = runtime_context_.world->registry().view<dse::PostProcessComponent>();
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
                return; // 只有在启用后处理时才执行
            }

            // Bloom Extract
            cmd_buffer.SetPipelineState(render_resources_.composite_pipeline_state);
            cmd_buffer.BeginRenderPass({render_resources_.pp_bloom_extract_rt, glm::vec4(0.0f), false});
            const unsigned int scene_color = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.scene_render_target);
            cmd_buffer.DrawPostProcess(scene_color, "bloom_extract", {pp_config.bloom_threshold});
            cmd_buffer.EndRenderPass();

            // Downsample Pass (Dual Filter)
            unsigned int current_src = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.pp_bloom_extract_rt);
            int mip_w = Screen::width() / 2;
            int mip_h = Screen::height() / 2;
            for (size_t i = 0; i < render_resources_.pp_bloom_mip_rts.size(); ++i) {
                cmd_buffer.BeginRenderPass({render_resources_.pp_bloom_mip_rts[i], glm::vec4(0.0f), false});
                cmd_buffer.DrawPostProcess(current_src, "bloom_downsample", {static_cast<float>(mip_w * 2), static_cast<float>(mip_h * 2)});
                cmd_buffer.EndRenderPass();
                current_src = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.pp_bloom_mip_rts[i]);
                mip_w /= 2;
                mip_h /= 2;
                if (mip_w < 1) mip_w = 1;
                if (mip_h < 1) mip_h = 1;
            }

            // Upsample Pass (Dual Filter Tent)
            // We go backwards from the smallest mip
            for (int i = static_cast<int>(render_resources_.pp_bloom_mip_rts.size()) - 1; i > 0; --i) {
                unsigned int target_rt = render_resources_.pp_bloom_mip_rts[i - 1];
                current_src = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.pp_bloom_mip_rts[i]);
                cmd_buffer.BeginRenderPass({target_rt, glm::vec4(0.0f), false});
                // Note: To implement additive blending correctly for PBR bloom upsample, we might need a specific blend state.
                // For simplicity in Phase 2, we just use the tent filter directly and rely on composite later, or add blend.
                // In standard Dual Filtering, upsample actually blends with the previous downsample layer.
                // Since our pipeline doesn't have an easy way to specify additive blend just for post-process here without a new state,
                // we'll pass the base texture as a second param if we update the shader, but let's just do a basic upsample for now.
                cmd_buffer.DrawPostProcess(current_src, "bloom_upsample", {0.005f}); // Filter radius
                cmd_buffer.EndRenderPass();
            }

            // Final result is in render_resources_.pp_bloom_mip_rts[0]
        }
    });

    render_graph_passes_.push_back({
        "ui_pass",
        [this](CommandBuffer& cmd_buffer) {
            cmd_buffer.SetPipelineState(render_resources_.sprite_pipeline_state);
            cmd_buffer.BeginRenderPass({render_resources_.ui_render_target, glm::vec4(0.0f), true});
            ui_render_system_.Render(*runtime_context_.world, cmd_buffer, Screen::width(), Screen::height());
            cmd_buffer.EndRenderPass();
        }
    });

    render_graph_passes_.push_back({
        "composite_pass",
        [this](CommandBuffer& cmd_buffer) {
            const unsigned int scene_color = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.scene_render_target);
            const unsigned int ui_color = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.ui_render_target);
            
            auto pp_view = runtime_context_.world->registry().view<dse::PostProcessComponent>();
            bool pp_enabled = false;
            dse::PostProcessComponent pp_config;
            for (auto entity : pp_view) {
                if (pp_view.get<dse::PostProcessComponent>(entity).enabled) {
                    pp_enabled = true;
                    pp_config = pp_view.get<dse::PostProcessComponent>(entity);
                    break;
                }
            }

            cmd_buffer.SetPipelineState(render_resources_.composite_pipeline_state);
            cmd_buffer.BeginRenderPass({render_resources_.main_render_target, glm::vec4(0.0f), false});
            glm::mat4 ortho = glm::ortho(0.0f, static_cast<float>(Screen::width()), 0.0f, static_cast<float>(Screen::height()), -1.0f, 1.0f);
            cmd_buffer.SetCamera(glm::mat4(1.0f), ortho);

            if (pp_enabled && pp_config.bloom_enabled) {
                const unsigned int blur_v_color = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.pp_bloom_mip_rts.empty() ? 0 : render_resources_.pp_bloom_mip_rts[0]);
                cmd_buffer.DrawPostProcess(scene_color, "bloom_composite", {static_cast<float>(blur_v_color), pp_config.exposure, pp_config.bloom_intensity});
            } else {
                SpriteDrawItem scene_quad;
                scene_quad.texture_handle = scene_color;
                scene_quad.model = glm::translate(glm::mat4(1.0f), glm::vec3(Screen::width() * 0.5f, Screen::height() * 0.5f, 0.0f));
                scene_quad.model = glm::scale(scene_quad.model, glm::vec3(Screen::width(), Screen::height(), 1.0f));
                scene_quad.color = glm::vec4(1.0f);
                cmd_buffer.DrawBatch({scene_quad});
            }

            cmd_buffer.SetPipelineState(render_resources_.sprite_pipeline_state);
            SpriteDrawItem ui_quad;
            ui_quad.texture_handle = ui_color;
            ui_quad.model = glm::translate(glm::mat4(1.0f), glm::vec3(Screen::width() * 0.5f, Screen::height() * 0.5f, 0.0f));
            ui_quad.model = glm::scale(ui_quad.model, glm::vec3(Screen::width(), Screen::height(), 1.0f));
            ui_quad.color = glm::vec4(1.0f);
            cmd_buffer.DrawBatch({ui_quad});
            cmd_buffer.EndRenderPass();
        }
    });
}

void FramePipeline::ExecuteRenderGraph(CommandBuffer& cmd_buffer) {
    dse::runtime::ExecuteFrameRenderGraph(*this, cmd_buffer);
}

void FramePipeline::ExecuteRenderGraphInternal(CommandBuffer& cmd_buffer) {
    for (auto& pass : render_graph_passes_) {
        pass.execute(cmd_buffer);
    }
}

void FramePipeline::EnableEditorMode(bool enable) {
    if (!initialized_) {
        runtime_context_.editor_mode = enable;
    }
}

void FramePipeline::SetWorld(World* world) {
    if (initialized_ || !world) {
        return;
    }
    runtime_context_.world = world;
}

World& FramePipeline::world() {
    assert(runtime_context_.world && "FramePipeline world is not injected");
    return *runtime_context_.world;
}

int FramePipeline::LastDrawCalls() const {
    return last_draw_calls_;
}

int FramePipeline::LastMaxBatchSprites() const {
    return last_max_batch_sprites_;
}

int FramePipeline::LastSpriteCount() const {
    return last_sprite_count_;
}

unsigned int FramePipeline::GetSceneTextureId() const {
    if (!runtime_context_.rhi_device || render_resources_.scene_render_target == 0) return 0;
    return runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.scene_render_target);
}

unsigned int FramePipeline::GetMainTextureId() const {
    if (!runtime_context_.rhi_device || render_resources_.main_render_target == 0) return 0;
    return runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.main_render_target);
}

void FramePipeline::SetWindowTitleSetter(std::function<void(const std::string&)> setter) {
    runtime_context_.window_title_setter = std::move(setter);
}

void FramePipeline::SetWindowTitle(const std::string& title) {
    if (!runtime_context_.window_title_setter) {
        return;
    }
    runtime_context_.window_title_setter(title);
}

void FramePipeline::SetBusinessMode(BusinessMode mode) {
    if (initialized_) {
        return;
    }
    runtime_context_.business_mode = mode;
}

void FramePipeline::SetAssetManager(AssetManager* asset_manager) {
    if (initialized_) {
        return;
    }
    runtime_context_.asset_manager = asset_manager;
}
