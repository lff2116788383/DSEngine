#include "engine/runtime/frame_pipeline.h"
#include "engine/base/debug.h"
#include "engine/base/time.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"
#include "engine/assets/asset_manager.h"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/scripting/cpp/cpp_business_runtime.h"
#include "engine/ecs/components_2d.h"
#include "engine/core/event_bus.h"
#include "engine/scene/scene.h"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <cstdlib>
#include <stdexcept>
#include <chrono>
#include <utility>
#include <vector>

namespace {
AssetManager& ResolveAssetManager(AssetManager* asset_manager) {
    if (asset_manager) {
        return *asset_manager;
    }
    return AssetManager::Instance();
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
    auto& asset_manager = ResolveAssetManager(asset_manager_);
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
    main_render_target_ = 0;
    scene_render_target_ = rhi_device_->CreateRenderTarget({render_width, render_height, false});
    ui_render_target_ = rhi_device_->CreateRenderTarget({render_width, render_height, false});
    sprite_pipeline_state_ = rhi_device_->CreatePipelineState({true, 0x0302, 0x0303});
    composite_pipeline_state_ = rhi_device_->CreatePipelineState({false, 0x0302, 0x0303});
    
    physics2d_system_.Init(*world_);
    audio_system_.Initialize();

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
        const bool cpp_bootstrap_ok = dse::runtime::BootstrapCppBusiness(*world_);
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
    dse::core::EventBus::Instance().Publish<dse::core::SceneLifecycleEvent>(dse::core::SceneLifecyclePhase::Shutdown);
    auto& asset_manager = ResolveAssetManager(asset_manager_);
    render_graph_passes_.clear();
    if (business_mode_ == BusinessMode::Lua) {
        dse::runtime::ShutdownLuaRuntime();
    } else {
        dse::runtime::ShutdownCppBusiness();
    }
    audio_system_.Shutdown();
    asset_manager.SetRhiDevice(nullptr);
    if (rhi_device_) {
        rhi_device_->Shutdown();
        rhi_device_.reset();
    }
    main_render_target_ = 0;
    scene_render_target_ = 0;
    ui_render_target_ = 0;
    sprite_pipeline_state_ = 0;
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
    auto update_begin = std::chrono::high_resolution_clock::now();
    auto& asset_manager = ResolveAssetManager(asset_manager_);
    asset_manager.PumpMainThreadCallbacks(callback_budget_per_frame_);
    if (business_mode_ == BusinessMode::Lua) {
        dse::runtime::TickLuaRuntime(delta_time);
    } else {
        dse::runtime::TickCppBusiness(*world_, delta_time);
    }
    
    tilemap_system_.Update(world_->registry());
    animation_system_.Update(*world_, delta_time);
    transform_system_.Update(*world_);
    camera_system_.Update(*world_, Screen::aspect_ratio());
    ui_logic_system_.Update(world_->registry(), delta_time, glm::vec2(Screen::width(), Screen::height()), Input::mousePosition(), Input::GetMouseButton(0));
    audio_system_.Update(world_->registry(), delta_time);
    auto update_end = std::chrono::high_resolution_clock::now();
    update_time_accumulator_ms_ += std::chrono::duration<float, std::milli>(update_end - update_begin).count();
    update_samples_ += 1;
}

void FramePipeline::FixedUpdate(float fixed_delta_time) {
    if (!initialized_) {
        return;
    }
    auto fixed_begin = std::chrono::high_resolution_clock::now();
    physics2d_system_.FixedUpdate(*world_, fixed_delta_time);
    auto fixed_end = std::chrono::high_resolution_clock::now();
    fixed_time_accumulator_ms_ += std::chrono::duration<float, std::milli>(fixed_end - fixed_begin).count();
    fixed_samples_ += 1;
}

void FramePipeline::Render() {
    if (!initialized_) {
        return;
    }
    auto render_begin = std::chrono::high_resolution_clock::now();
    rhi_device_->BeginFrame();
    
    auto cmd_buffer = rhi_device_->CreateCommandBuffer();
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
        float avg_update_ms = update_samples_ > 0 ? update_time_accumulator_ms_ / static_cast<float>(update_samples_) : 0.0f;
        float avg_fixed_ms = fixed_samples_ > 0 ? fixed_time_accumulator_ms_ / static_cast<float>(fixed_samples_) : 0.0f;
        float avg_render_ms = render_samples_ > 0 ? render_time_accumulator_ms_ / static_cast<float>(render_samples_) : 0.0f;
        auto& asset_manager = ResolveAssetManager(asset_manager_);
        std::size_t pending_callbacks = asset_manager.PendingMainThreadCallbacks();
        std::size_t pending_callbacks_hwm = asset_manager.PendingMainThreadCallbacksHighWatermark();
        DEBUG_LOG_INFO("Runtime stats: entities={}, sprites={}, draw_calls={}, max_batch_sprites={}, render_passes={}, physics_bodies={}, avg_update_ms={:.3f}, avg_fixed_ms={:.3f}, avg_render_ms={:.3f}, pending_upload_callbacks={}, pending_upload_callbacks_hwm={}, upload_budget={}",
                       entity_count,
                       stats.sprite_count,
                       stats.draw_calls,
                       stats.max_batch_sprites,
                       stats.render_passes,
                       physics_bodies,
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
    render_graph_passes_.clear();
    render_graph_passes_.push_back({
        "scene_pass",
        [this](CommandBuffer& cmd_buffer) {
            cmd_buffer.SetPipelineState(sprite_pipeline_state_);
            cmd_buffer.BeginRenderPass({scene_render_target_, glm::vec4(0.02f, 0.02f, 0.02f, 1.0f), true});
            auto camera_view = world_->registry().view<CameraComponent>();
            if (!camera_view.empty()) {
                auto& camera = camera_view.get<CameraComponent>(camera_view.front());
                cmd_buffer.SetCamera(camera.view, camera.projection);
            }
            sprite_render_system_.Render(*world_, cmd_buffer);
            cmd_buffer.EndRenderPass();
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
            static bool logged_once = false;
            if (!logged_once) {
                DEBUG_LOG_INFO("Render diagnostics: scene_render_target={}, ui_render_target={}, scene_color={}, ui_color={}",
                               scene_render_target_, ui_render_target_, scene_color, ui_color);
                logged_once = true;
            }
            if (scene_color == 0 || ui_color == 0) {
                DEBUG_LOG_ERROR("Render composite skipped: scene_render_target={}, ui_render_target={}, scene_color={}, ui_color={}",
                                scene_render_target_, ui_render_target_, scene_color, ui_color);
                return;
            }

            cmd_buffer.SetPipelineState(composite_pipeline_state_);
            cmd_buffer.BeginRenderPass({main_render_target_, glm::vec4(0.0f), false});
            glm::mat4 ortho = glm::ortho(0.0f, static_cast<float>(Screen::width()), 0.0f, static_cast<float>(Screen::height()), -1.0f, 1.0f);
            cmd_buffer.SetCamera(glm::mat4(1.0f), ortho);

            SpriteDrawItem scene_quad;
            scene_quad.texture_handle = scene_color;
            scene_quad.model = glm::translate(glm::mat4(1.0f), glm::vec3(Screen::width() * 0.5f, Screen::height() * 0.5f, 0.0f));
            scene_quad.model = glm::scale(scene_quad.model, glm::vec3(Screen::width(), Screen::height(), 1.0f));
            scene_quad.color = glm::vec4(1.0f);
            cmd_buffer.DrawBatch({scene_quad});

            cmd_buffer.SetPipelineState(sprite_pipeline_state_);
            SpriteDrawItem ui_quad;
            ui_quad.texture_handle = ui_color;
            ui_quad.model = scene_quad.model;
            ui_quad.color = glm::vec4(1.0f);
            cmd_buffer.DrawBatch({ui_quad});
            cmd_buffer.EndRenderPass();
        }
    });
}

void FramePipeline::ExecuteRenderGraph(CommandBuffer& cmd_buffer) {
    for (auto& pass : render_graph_passes_) {
        pass.execute(cmd_buffer);
    }
}

void FramePipeline::SetWorld(World* world) {
    if (initialized_ || !world) {
        return;
    }
    world_ = world;
}

World& FramePipeline::world() {
    if (!world_) {
        throw std::runtime_error("FramePipeline world is not injected");
    }
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
