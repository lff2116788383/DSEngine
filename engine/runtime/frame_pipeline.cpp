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
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/scripting/cpp/cpp_business_runtime.h"
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
    if (!world_) {
        DEBUG_LOG_ERROR("FramePipeline init failed: world is not injected");
        return false;
    }
    auto& asset_manager = RequireAssetManager(asset_manager_);
    rhi_device_ = std::make_unique<OpenGLRhiDevice>();
    asset_manager.SetRhiDevice(rhi_device_.get());
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
    
    if (editor_mode_) {
        // 在编辑器模式下，将最终合成结果输出到一个纹理中，而不是直接输出到屏幕
        main_render_target_ = rhi_device_->CreateRenderTarget({render_width, render_height, false, false});
    } else {
        main_render_target_ = 0;
    }
    
    // 使用支持 HDR 的浮点纹理作为 Scene Render Target，这是泛光和色调映射的基础
    scene_render_target_ = rhi_device_->CreateRenderTarget({render_width, render_height, true, true}); // Enable depth for scene pass
    ui_render_target_ = rhi_device_->CreateRenderTarget({render_width, render_height, true, false});
    prez_render_target_ = rhi_device_->CreateRenderTarget({render_width, render_height, false, true}); // Only depth
    
    for (int i = 0; i < CSM_CASCADES; ++i) {
        shadow_render_target_[i] = rhi_device_->CreateRenderTarget({2048, 2048, false, true}); // Shadow map resolution
    }

    if (pp_bloom_extract_rt_ == 0) {
        pp_bloom_extract_rt_ = rhi_device_->CreateRenderTarget({render_width, render_height, true, false, false});
    }
    
    // Create mip chain for Dual Filter Bloom (5 levels)
    if (pp_bloom_mip_rts_.empty()) {
        int mip_width = render_width / 2;
        int mip_height = render_height / 2;
        for (int i = 0; i < 5; ++i) {
            pp_bloom_mip_rts_.push_back(rhi_device_->CreateRenderTarget({mip_width, mip_height, true, false, false}));
            mip_width /= 2;
            mip_height /= 2;
            if (mip_width < 1) mip_width = 1;
            if (mip_height < 1) mip_height = 1;
        }
    }

    sprite_pipeline_state_ = rhi_device_->CreatePipelineState({true, 0x0302, 0x0303});
    
    PipelineStateDesc mesh_desc;
    mesh_desc.blend_enabled = false;
    mesh_desc.depth_test_enabled = true;
    mesh_desc.depth_write_enabled = true;
    mesh_desc.culling_enabled = true;
    mesh_pipeline_state_ = rhi_device_->CreatePipelineState(mesh_desc);

    PipelineStateDesc prez_desc;
    prez_desc.blend_enabled = false;
    prez_desc.depth_test_enabled = true;
    prez_desc.depth_write_enabled = true;
    prez_desc.culling_enabled = true;
    // In a real engine we disable color write, but for now we'll just write to a depth-only FBO
    prez_pipeline_state_ = rhi_device_->CreatePipelineState(prez_desc);

    PipelineStateDesc shadow_desc;
    shadow_desc.blend_enabled = false;
    shadow_desc.depth_test_enabled = true;
    shadow_desc.depth_write_enabled = true;
    shadow_desc.culling_enabled = true;
    shadow_desc.cull_face = 0x0404; // GL_FRONT to avoid peter-panning
    shadow_pipeline_state_ = rhi_device_->CreatePipelineState(shadow_desc);
    
    composite_pipeline_state_ = rhi_device_->CreatePipelineState({false, 0x0302, 0x0303});
    
    physics2d_system_.Init(*world_);
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
                    if (!module_instance->OnInit(*world_, rhi_device_.get(), &asset_manager)) {
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
    
    if (business_mode_ == BusinessMode::Lua) {
        dse::runtime::ConfigureLuaApiContext({
            world_,
            [this](const std::string& title) { SetWindowTitle(title); },
            [this]() { return LastDrawCalls(); },
            [this]() { return LastMaxBatchSprites(); },
            [this]() { return LastSpriteCount(); },
            &asset_manager
        });
        const bool lua_bootstrap_ok = dse::runtime::BootstrapLuaRuntime();
        DEBUG_LOG_INFO("Lua bootstrap: {}", lua_bootstrap_ok ? "OK" : "FAILED");
        if (!lua_bootstrap_ok) {
            Shutdown();
            return false;
        }
    } else {
        const bool cpp_bootstrap_ok = dse::runtime::BootstrapCppBusiness(*world_, asset_manager);
        DEBUG_LOG_INFO("Cpp business mode bootstrap: {}", cpp_bootstrap_ok ? "OK" : "FAILED");
        if (!cpp_bootstrap_ok) {
            Shutdown();
            return false;
        }
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
    auto& asset_manager = RequireAssetManager(asset_manager_);
    render_graph_passes_.clear();
    if (business_mode_ == BusinessMode::Lua) {
        dse::runtime::ShutdownLuaRuntime();
    } else {
        dse::runtime::ShutdownCppBusiness();
    }
    audio_system_.Shutdown();
    physics2d_system_.Shutdown();
    spine_system_.Shutdown(world_->registry());
    spine_system_.SetAssetManager(nullptr);
    mesh_render_system_.SetAssetManager(nullptr);
    asset_manager.ReleaseGpuResources();
    
    for (auto& mod : modules_) {
        if (mod.instance) {
            mod.instance->OnShutdown(*world_);
            if (mod.destroy) {
                mod.destroy(mod.instance);
            }
        }
    }
    modules_.clear();

    if (rhi_device_) {
        rhi_device_->Shutdown();
        rhi_device_.reset();
    }
    asset_manager.SetRhiDevice(nullptr);
    main_render_target_ = 0;
    scene_render_target_ = 0;
    ui_render_target_ = 0;
    prez_render_target_ = 0;
    for (int i = 0; i < CSM_CASCADES; ++i) {
        shadow_render_target_[i] = 0;
    }
    pp_bloom_extract_rt_ = 0;
    pp_bloom_mip_rts_.clear();
    sprite_pipeline_state_ = 0;
    mesh_pipeline_state_ = 0;
    prez_pipeline_state_ = 0;
    shadow_pipeline_state_ = 0;
    composite_pipeline_state_ = 0;
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
    auto& asset_manager = RequireAssetManager(asset_manager_);
    asset_manager.PumpMainThreadCallbacks(callback_budget_per_frame_);
    if (business_mode_ == BusinessMode::Lua) {
        dse::runtime::TickLuaRuntime(delta_time);
    } else {
        dse::runtime::TickCppBusiness(*world_, delta_time);
    }
    
    tilemap_system_.Update(world_->registry());
    animation_system_.Update(*world_, delta_time);
    particle_system_.Update(*world_, delta_time, &physics2d_system_);
    spine_system_.Update(world_->registry(), delta_time);
    transform_system_.Update(*world_);
    ui_logic_system_.Update(world_->registry(), delta_time, glm::vec2(Screen::width(), Screen::height()), Input::mousePosition(), Input::GetMouseButton(0));
    camera_system_.Update(*world_, Screen::aspect_ratio());
    audio_system_.Update(world_->registry(), delta_time);
    
    // 分发模块更新事件
    for (auto& mod : modules_) {
        if (mod.instance) {
            mod.instance->OnUpdate(*world_, delta_time);
        }
    }
    
    auto update_end = std::chrono::high_resolution_clock::now();
    update_time_accumulator_ms_ += std::chrono::duration<float, std::milli>(update_end - update_begin).count();
    update_samples_ += 1;
}

void FramePipeline::RunFixedUpdateInternal(float fixed_delta_time) {
    auto fixed_begin = std::chrono::high_resolution_clock::now();
    physics2d_system_.FixedUpdate(*world_, fixed_delta_time);
    
    // 分发模块固定更新事件
    for (auto& mod : modules_) {
        if (mod.instance) {
            mod.instance->OnFixedUpdate(*world_, fixed_delta_time);
        }
    }
    
    auto fixed_end = std::chrono::high_resolution_clock::now();
    fixed_time_accumulator_ms_ += std::chrono::duration<float, std::milli>(fixed_end - fixed_begin).count();
    fixed_samples_ += 1;
}

void FramePipeline::RunRenderInternal() {
    auto render_begin = std::chrono::high_resolution_clock::now();
    rhi_device_->BeginFrame();
    
    auto cmd_buffer = rhi_device_->CreateCommandBuffer();
    
    // Set global shadow maps early (they might be accessed in the scene pass)
    for (int i = 0; i < CSM_CASCADES; ++i) {
        if (auto* device = dynamic_cast<OpenGLRhiDevice*>(rhi_device_.get())) {
            device->SetGlobalShadowMap(i, rhi_device_->GetRenderTargetDepthTexture(shadow_render_target_[i]));
        }
    }

    ExecuteRenderGraph(*cmd_buffer);
    
    rhi_device_->Submit(cmd_buffer);
    
    rhi_device_->EndFrame();
    const auto& frame_stats = rhi_device_->LastFrameStats();
    last_draw_calls_ = static_cast<int>(frame_stats.draw_calls);
    last_max_batch_sprites_ = static_cast<int>(frame_stats.max_batch_sprites);
    last_sprite_count_ = static_cast<int>(frame_stats.sprite_count);
    auto render_end = std::chrono::high_resolution_clock::now();
    render_time_accumulator_ms_ += std::chrono::duration<float, std::milli>(render_end - render_begin).count();
    render_samples_ += 1;

    stats_accumulator_ += Time::delta_time();
    if (stats_accumulator_ >= 1.0f) {
        stats_accumulator_ = 0.0f;
        const auto& stats = frame_stats;
        size_t entity_count = world_->EntityCount();
        size_t physics_bodies = 0;
        auto physics_view = world_->registry().view<RigidBody2DComponent>();
        for (auto entity : physics_view) {
            (void)entity;
            ++physics_bodies;
        }
        size_t particle_emitters = 0;
        size_t active_particles = 0;
        auto particle_view = world_->registry().view<ParticleEmitterComponent>();
        for (auto emitter_entity : particle_view) {
            auto& emitter = particle_view.get<ParticleEmitterComponent>(emitter_entity);
            (void)emitter_entity;
            ++particle_emitters;
            active_particles += emitter.particles.size();
        }
        float avg_update_ms = update_samples_ > 0 ? update_time_accumulator_ms_ / static_cast<float>(update_samples_) : 0.0f;
        float avg_fixed_ms = fixed_samples_ > 0 ? fixed_time_accumulator_ms_ / static_cast<float>(fixed_samples_) : 0.0f;
        float avg_render_ms = render_samples_ > 0 ? render_time_accumulator_ms_ / static_cast<float>(render_samples_) : 0.0f;
        auto& asset_manager = RequireAssetManager(asset_manager_);
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
            cmd_buffer.BeginRenderPass({prez_render_target_, glm::vec4(0.0f), true});
            auto camera3d_view = world_->registry().view<dse::Camera3DComponent>();
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
                if (world_->registry().all_of<TransformComponent>(selected_camera3d)) {
                    auto& transform = world_->registry().get<TransformComponent>(selected_camera3d);
                    glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                    glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                    view = glm::lookAt(transform.position, transform.position + front, up);
                }
                cmd_buffer.SetCamera(view, projection);
                cmd_buffer.SetPipelineState(prez_pipeline_state_);
                
                // 分发 PreZ 渲染事件
                for (auto& mod : modules_) {
                    if (mod.instance) {
                        mod.instance->OnRenderPreZ(*world_, cmd_buffer);
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
            auto light_view = world_->registry().view<dse::DirectionalLight3DComponent>();
            if (light_view.begin() == light_view.end()) return;
            auto& light = light_view.get<dse::DirectionalLight3DComponent>(*light_view.begin());
            if (!light.enabled || !light.cast_shadow) return;

            std::vector<glm::mat4> light_space_matrices(CSM_CASCADES);
            std::vector<float> cascade_splits(CSM_CASCADES);

            for (int i = 0; i < CSM_CASCADES; ++i) {
                cmd_buffer.BeginRenderPass({shadow_render_target_[i], glm::vec4(1.0f), true}); // Clear depth to 1.0
                
                // Simple ortho projection for cascades (in a full implementation, this should bound the view frustum splits)
                // Here we just scale up the orthographic size based on the cascade level
                float size = 20.0f * std::pow(2.0f, static_cast<float>(i));
                glm::mat4 light_proj = glm::ortho(-size, size, -size, size, 1.0f, 200.0f);
                glm::vec3 light_pos = -glm::normalize(light.direction) * 100.0f;
                glm::mat4 light_view_mat = glm::lookAt(light_pos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                
                light_space_matrices[i] = light_proj * light_view_mat;
                cascade_splits[i] = light.cascade_splits[i];

                cmd_buffer.SetCamera(light_view_mat, light_proj);
                cmd_buffer.SetPipelineState(shadow_pipeline_state_);
                
                // 分发 Shadow 渲染事件
                for (auto& mod : modules_) {
                    if (mod.instance) {
                        mod.instance->OnRenderShadow(*world_, cmd_buffer, i, light_view_mat, light_proj);
                    }
                }
                
                cmd_buffer.EndRenderPass();
            }

            cmd_buffer.SetGlobalMat4Array("u_light_space_matrices", light_space_matrices);
            cmd_buffer.SetGlobalFloatArray("u_cascade_splits", cascade_splits);

            for (int i = 0; i < CSM_CASCADES; ++i) {
                if (auto* device = dynamic_cast<OpenGLRhiDevice*>(rhi_device_.get())) {
                    device->SetGlobalShadowMap(i, rhi_device_->GetRenderTargetDepthTexture(shadow_render_target_[i]));
                }
            }
        }
    });

    // 2. Main Scene Pass
    render_graph_passes_.push_back({
        "scene_pass",
        [this](CommandBuffer& cmd_buffer) {
            cmd_buffer.BeginRenderPass({scene_render_target_, glm::vec4(0.02f, 0.02f, 0.02f, 1.0f), true});
            
            auto camera3d_view = world_->registry().view<dse::Camera3DComponent>();
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
                if (world_->registry().all_of<TransformComponent>(selected_camera3d)) {
                    auto& transform = world_->registry().get<TransformComponent>(selected_camera3d);
                    glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                    glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                    view = glm::lookAt(transform.position, transform.position + front, up);
                }
                cmd_buffer.SetCamera(view, projection);
                
                // Draw Skybox
                auto skybox_view = world_->registry().view<dse::SkyboxComponent>();
                for (auto sky_entity : skybox_view) {
                    auto& skybox = skybox_view.get<dse::SkyboxComponent>(sky_entity);
                    if (skybox.enabled) {
                        // Use the cubemap_handle directly
                        cmd_buffer.DrawSkybox(skybox.cubemap_handle);
                        break;
                    }
                }
            } else {
                auto camera_view = world_->registry().view<CameraComponent>();
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
            cmd_buffer.SetPipelineState(mesh_pipeline_state_);
            
            // 分发 Scene 渲染事件
            for (auto& mod : modules_) {
                if (mod.instance) {
                    mod.instance->OnRenderScene(*world_, cmd_buffer);
                }
            }
            
            // 2D Sprite/Particle/Spine rendering
            cmd_buffer.SetPipelineState(sprite_pipeline_state_);
            sprite_render_system_.Render(*world_, cmd_buffer);
            spine_system_.Render(*world_, cmd_buffer);
            particle_system_.Render(*world_, cmd_buffer);
            cmd_buffer.EndRenderPass();
        }
    });

    render_graph_passes_.push_back({
        "post_process_pass",
        [this](CommandBuffer& cmd_buffer) {
            auto pp_view = world_->registry().view<dse::PostProcessComponent>();
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
            cmd_buffer.SetPipelineState(composite_pipeline_state_);
            cmd_buffer.BeginRenderPass({pp_bloom_extract_rt_, glm::vec4(0.0f), false});
            const unsigned int scene_color = rhi_device_->GetRenderTargetColorTexture(scene_render_target_);
            cmd_buffer.DrawPostProcess(scene_color, "bloom_extract", {pp_config.bloom_threshold});
            cmd_buffer.EndRenderPass();

            // Downsample Pass (Dual Filter)
            unsigned int current_src = rhi_device_->GetRenderTargetColorTexture(pp_bloom_extract_rt_);
            int mip_w = Screen::width() / 2;
            int mip_h = Screen::height() / 2;
            for (size_t i = 0; i < pp_bloom_mip_rts_.size(); ++i) {
                cmd_buffer.BeginRenderPass({pp_bloom_mip_rts_[i], glm::vec4(0.0f), false});
                cmd_buffer.DrawPostProcess(current_src, "bloom_downsample", {static_cast<float>(mip_w * 2), static_cast<float>(mip_h * 2)});
                cmd_buffer.EndRenderPass();
                current_src = rhi_device_->GetRenderTargetColorTexture(pp_bloom_mip_rts_[i]);
                mip_w /= 2;
                mip_h /= 2;
                if (mip_w < 1) mip_w = 1;
                if (mip_h < 1) mip_h = 1;
            }

            // Upsample Pass (Dual Filter Tent)
            // We go backwards from the smallest mip
            for (int i = static_cast<int>(pp_bloom_mip_rts_.size()) - 1; i > 0; --i) {
                unsigned int target_rt = pp_bloom_mip_rts_[i - 1];
                current_src = rhi_device_->GetRenderTargetColorTexture(pp_bloom_mip_rts_[i]);
                cmd_buffer.BeginRenderPass({target_rt, glm::vec4(0.0f), false});
                // Note: To implement additive blending correctly for PBR bloom upsample, we might need a specific blend state.
                // For simplicity in Phase 2, we just use the tent filter directly and rely on composite later, or add blend.
                // In standard Dual Filtering, upsample actually blends with the previous downsample layer.
                // Since our pipeline doesn't have an easy way to specify additive blend just for post-process here without a new state,
                // we'll pass the base texture as a second param if we update the shader, but let's just do a basic upsample for now.
                cmd_buffer.DrawPostProcess(current_src, "bloom_upsample", {0.005f}); // Filter radius
                cmd_buffer.EndRenderPass();
            }

            // Final result is in pp_bloom_mip_rts_[0]
        }
    });

    render_graph_passes_.push_back({
        "ui_pass",
        [this](CommandBuffer& cmd_buffer) {
            cmd_buffer.SetPipelineState(sprite_pipeline_state_);
            cmd_buffer.BeginRenderPass({ui_render_target_, glm::vec4(0.0f), true});
            ui_render_system_.Render(*world_, cmd_buffer, Screen::width(), Screen::height());
            cmd_buffer.EndRenderPass();
        }
    });

    render_graph_passes_.push_back({
        "composite_pass",
        [this](CommandBuffer& cmd_buffer) {
            const unsigned int scene_color = rhi_device_->GetRenderTargetColorTexture(scene_render_target_);
            const unsigned int ui_color = rhi_device_->GetRenderTargetColorTexture(ui_render_target_);
            
            auto pp_view = world_->registry().view<dse::PostProcessComponent>();
            bool pp_enabled = false;
            dse::PostProcessComponent pp_config;
            for (auto entity : pp_view) {
                if (pp_view.get<dse::PostProcessComponent>(entity).enabled) {
                    pp_enabled = true;
                    pp_config = pp_view.get<dse::PostProcessComponent>(entity);
                    break;
                }
            }

            cmd_buffer.SetPipelineState(composite_pipeline_state_);
            cmd_buffer.BeginRenderPass({main_render_target_, glm::vec4(0.0f), false});
            glm::mat4 ortho = glm::ortho(0.0f, static_cast<float>(Screen::width()), 0.0f, static_cast<float>(Screen::height()), -1.0f, 1.0f);
            cmd_buffer.SetCamera(glm::mat4(1.0f), ortho);

            if (pp_enabled && pp_config.bloom_enabled) {
                const unsigned int blur_v_color = rhi_device_->GetRenderTargetColorTexture(pp_bloom_mip_rts_.empty() ? 0 : pp_bloom_mip_rts_[0]);
                cmd_buffer.DrawPostProcess(scene_color, "bloom_composite", {static_cast<float>(blur_v_color), pp_config.exposure, pp_config.bloom_intensity});
            } else {
                SpriteDrawItem scene_quad;
                scene_quad.texture_handle = scene_color;
                scene_quad.model = glm::translate(glm::mat4(1.0f), glm::vec3(Screen::width() * 0.5f, Screen::height() * 0.5f, 0.0f));
                scene_quad.model = glm::scale(scene_quad.model, glm::vec3(Screen::width(), Screen::height(), 1.0f));
                scene_quad.color = glm::vec4(1.0f);
                cmd_buffer.DrawBatch({scene_quad});
            }

            cmd_buffer.SetPipelineState(sprite_pipeline_state_);
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
        editor_mode_ = enable;
    }
}

void FramePipeline::SetWorld(World* world) {
    if (initialized_ || !world) {
        return;
    }
    world_ = world;
}

World& FramePipeline::world() {
    assert(world_ && "FramePipeline world is not injected");
    return *world_;
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
    if (!rhi_device_ || scene_render_target_ == 0) return 0;
    return rhi_device_->GetRenderTargetColorTexture(scene_render_target_);
}

unsigned int FramePipeline::GetMainTextureId() const {
    if (!rhi_device_ || main_render_target_ == 0) return 0;
    return rhi_device_->GetRenderTargetColorTexture(main_render_target_);
}

void FramePipeline::SetWindowTitleSetter(std::function<void(const std::string&)> setter) {
    window_title_setter_ = std::move(setter);
}

void FramePipeline::SetWindowTitle(const std::string& title) {
    if (!window_title_setter_) {
        return;
    }
    window_title_setter_(title);
}

void FramePipeline::SetBusinessMode(BusinessMode mode) {
    if (initialized_) {
        return;
    }
    business_mode_ = mode;
}

void FramePipeline::SetAssetManager(AssetManager* asset_manager) {
    if (initialized_) {
        return;
    }
    asset_manager_ = asset_manager;
}
