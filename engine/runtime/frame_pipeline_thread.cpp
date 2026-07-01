/**
 * @file frame_pipeline_thread.cpp
 * @brief FramePipeline render thread management â€” Start/Stop/RenderThreadFunc.
 */

#include "engine/runtime/frame_pipeline.h"
#include "engine/runtime/frame_pipeline_impl.h"
#include "engine/runtime/i_builtin_modules.h"
#include "engine/render/passes/builtin_passes.h"
#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/hiz_types.h"
#include "engine/render/rhi/rhi_factory.h"
#include "engine/base/debug.h"
#include "engine/base/time.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"
#include "engine/assets/asset_manager.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/components_3d.h"
#include "engine/core/event_bus.h"
#include "engine/core/service_locator.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include <chrono>
#include <algorithm>
#include <iostream>

void FramePipeline::StartRenderThread() {
    if (render_thread_active_.load()) return;

    {
        std::lock_guard<std::mutex> lock(render_mutex_);
        render_thread_exit_ = false;
        render_frame_pending_ = false;
        render_frame_done_ = true;
    }

    // ä¸»çº¿ç¨‹é‡Šæ”¾ GL contextï¼Œç”±æ¸²æŸ“çº¿ç¨‹æŽ¥ç®¡
    if (runtime_context_.release_render_context) {
        runtime_context_.release_render_context();
    }

    render_thread_ = std::thread(&FramePipeline::RenderThreadFunc, this);
    render_thread_active_.store(true);
    DEBUG_LOG_INFO("[RenderThread] Started");
}

void FramePipeline::StopRenderThread() {
    if (!render_thread_active_.load()) return;

    {
        std::lock_guard<std::mutex> lock(render_mutex_);
        render_thread_exit_ = true;
        render_frame_pending_ = true;  // å”¤é†’çº¿ç¨‹ä½¿å…¶æ£€æŸ¥ exit æ ‡å¿—
    }
    render_cv_.notify_one();

    if (render_thread_.joinable()) {
        render_thread_.join();
    }
    render_thread_active_.store(false);

    // ä¸»çº¿ç¨‹é‡æ–°èŽ·å– GL context
    if (runtime_context_.make_render_context_current) {
        runtime_context_.make_render_context_current();
    }
    DEBUG_LOG_INFO("[RenderThread] Stopped");
}

void FramePipeline::RenderThreadFunc() {
    // æ¸²æŸ“çº¿ç¨‹èŽ·å– GL/Vulkan/DX11 context
    if (runtime_context_.make_render_context_current) {
        runtime_context_.make_render_context_current();
    }

    while (true) {
        // ç­‰å¾…ä¸»çº¿ç¨‹å‘å‡ºæ¸²æŸ“ä¿¡å·
        {
            std::unique_lock<std::mutex> lock(render_mutex_);
            render_cv_.wait(lock, [this] { return render_frame_pending_ || render_thread_exit_; });
            if (render_thread_exit_) break;
            render_frame_pending_ = false;
        }

        ExecuteRenderFrame();

        // é€šçŸ¥ä¸»çº¿ç¨‹æ¸²æŸ“å®Œæˆ
        {
            std::lock_guard<std::mutex> lock(render_mutex_);
            render_frame_done_ = true;
        }
        main_cv_.notify_one();
    }

    // é‡Šæ”¾ context
    if (runtime_context_.release_render_context) {
        runtime_context_.release_render_context();
    }
}

void FramePipeline::WaitForRenderComplete() {
    std::unique_lock<std::mutex> lock(render_mutex_);
    main_cv_.wait(lock, [this] { return render_frame_done_; });
}

void FramePipeline::SignalRenderThread() {
    {
        std::lock_guard<std::mutex> lock(render_mutex_);
        render_frame_pending_ = true;
        render_frame_done_ = false;
    }
    render_cv_.notify_one();
}

void FramePipeline::PrepareRenderFrame() {
    // â”€â”€ ä¸»çº¿ç¨‹ï¼šçº¯ CPU å·¥ä½œ + ECS è¯»å– â”€â”€

    // ç¡®ä¿æ‰€æœ‰ dirty çš„ TransformComponent åœ¨æ¸²æŸ“å‰æ›´æ–° local_to_world
    if (runtime_context_.world) {
        rs_->transform_system_.Update(*runtime_context_.world);
    }

    if (runtime_context_.world) {
        // Camera-Relative: æå‰èŽ·å–ç›¸æœºä½ç½®ä½œä¸º camera_offset
        glm::vec3 early_camera_offset(0.0f);
        auto cam_view_3d = runtime_context_.world->registry().view<dse::Camera3DComponent>();
        entt::entity cam_entity = entt::null;
        int cam_priority = std::numeric_limits<int>::min();
        for (auto e : cam_view_3d) {
            auto& cam = cam_view_3d.get<dse::Camera3DComponent>(e);
            if (cam.enabled && cam.priority > cam_priority) {
                cam_entity = e;
                cam_priority = cam.priority;
            }
        }
        if (cam_entity != entt::null && runtime_context_.world->registry().all_of<TransformComponent>(cam_entity)) {
            early_camera_offset = runtime_context_.world->registry().get<TransformComponent>(cam_entity).position;
        }

        // Clustered Forward+: æ”¶é›†å…‰æºï¼ˆCPUï¼‰â€” å…‰æºä½ç½®å‡åŽ» camera_offset
        dse::render::ExtractRenderSceneView(*runtime_context_.world, rs_->scene_view_);
        render_pass_context_.scene_view = &rs_->scene_view_;

        rs_->light_buffer_.CollectLightsFromView(rs_->scene_view_, early_camera_offset);

        // èŽ·å–ä¸»ç›¸æœºå‚æ•°æž„å»º cluster
        if (cam_entity != entt::null) {
            auto& cam = cam_view_3d.get<dse::Camera3DComponent>(cam_entity);
            const int sw = Screen::width();
            const int sh = Screen::height();
            glm::mat4 proj = glm::perspective(glm::radians(cam.fov),
                static_cast<float>(sw) / static_cast<float>(std::max(1, sh)),
                cam.near_clip, cam.far_clip);
            // Camera-Relative: å…‰æºå·²å‡åŽ» camera_offsetï¼Œcluster view ä¹Ÿç”¨ camera-at-origin
            glm::mat4 view_mat = glm::mat4(1.0f);
            if (runtime_context_.world->registry().all_of<TransformComponent>(cam_entity)) {
                auto& tf = runtime_context_.world->registry().get<TransformComponent>(cam_entity);
                glm::vec3 front = tf.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 up    = tf.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                view_mat = glm::lookAt(glm::vec3(0.0f), front, up);
            }
            rs_->cluster_grid_.Build(view_mat, proj, cam.near_clip, cam.far_clip, sw, sh,
                                rs_->light_buffer_.point_lights(), rs_->light_buffer_.spot_lights());
        }
    }

    // TAA: é¢„æ£€æµ‹ ECS ç»„ä»¶
    render_pass_context_.taa_active = false;
    if (taa_pass_ && render_pass_context_.pipeline_features.taa && runtime_context_.world) {
        auto pp_view = runtime_context_.world->registry().view<dse::PostProcessComponent>();
        for (auto entity : pp_view) {
            auto& pp = pp_view.get<dse::PostProcessComponent>(entity);
            if (pp.enabled && pp.taa_enabled) {
                render_pass_context_.taa_active = (render_pass_context_.render_targets.taa != 0);
                break;
            }
        }
        taa_pass_->UpdateJitter(taa_frame_index_++);
        render_pass_context_.taa_jitter = taa_pass_->GetCurrentJitter();
    }

    render_pass_context_.delta_time = Time::delta_time();

    // å…¨å±€æ¹¿åº¦ï¼šä»Ž ECS WeatherComponent ç›´æŽ¥è¯»å–ï¼ˆé›¨ â†’ wetness = intensityï¼‰
    render_pass_context_.global_wetness = 0.0f;
    if (runtime_context_.world) {
        auto wv = runtime_context_.world->registry().view<dse::WeatherComponent>();
        for (auto e : wv) {
            auto& wc = wv.get<dse::WeatherComponent>(e);
            if (wc.enabled && wc.type == dse::WeatherType::Rain) {
                render_pass_context_.global_wetness = wc.intensity;
                break;
            }
        }
    }

    // æ¤è¢«é£Žå‚æ•°ï¼ˆæ¸²æŸ“çº¿ç¨‹è·¯å¾„ï¼‰
    {
        float wind_x = 0.0f, wind_z = 0.0f, wind_strength = 0.0f;
        if (runtime_context_.world) {
            auto wv = runtime_context_.world->registry().view<dse::WeatherComponent>();
            for (auto e : wv) {
                auto& wc = wv.get<dse::WeatherComponent>(e);
                if (wc.enabled) {
                    wind_x = wc.wind_x;
                    wind_z = wc.wind_z;
                    wind_strength = glm::length(glm::vec2(wc.wind_x, wc.wind_z));
                    break;
                }
            }
        }
        if (wind_strength < 0.001f) wind_strength = 0.3f;
        glm::vec2 wind_dir = wind_strength > 0.001f
            ? glm::normalize(glm::vec2(wind_x, wind_z))
            : glm::vec2(1.0f, 0.0f);
        runtime_context_.rhi_device->SetGlobalFoliageWind(
            glm::vec4(Time::TimeSinceStartup(), wind_strength, wind_dir.x, wind_dir.y));
    }
    // æ¤è¢«æŽ¨åŠ›åœº
    {
        glm::vec3 push_pos(0.0f);
        if (runtime_context_.world) {
            auto cv = runtime_context_.world->registry().view<TransformComponent, dse::Camera3DComponent>();
            for (auto e : cv) {
                if (cv.get<dse::Camera3DComponent>(e).enabled) {
                    push_pos = cv.get<TransformComponent>(e).position;
                    break;
                }
            }
        }
        runtime_context_.rhi_device->SetGlobalFoliagePush(glm::vec4(push_pos, 2.0f));
    }

    // æ•èŽ·å¿«ç…§ + ç¿»è½¬åŒç¼“å†²
    CaptureThinSnapshot();
    FlipSnapshotIndex();
    render_pass_context_.snapshot = &read_snapshot();
    render_pass_context_.camera_offset = render_pass_context_.snapshot->camera_offset;
}

void FramePipeline::ExecuteRenderFrame() {
    rs_->render_profiler_.BeginFrame();
    auto render_begin = std::chrono::high_resolution_clock::now();

    dse::runtime::BeginRuntimeRenderFrame(*this);
    auto cmd_buffer = dse::runtime::CreateRuntimeRenderCommandBuffer(*this);
    dse::runtime::BindRuntimeShadowMaps(*this);

    // GPU ä¸Šä¼ ï¼šå…‰æº SSBO
    rs_->light_buffer_.Upload();

    // GPU ä¸Šä¼ ï¼šcluster SSBO
    rs_->cluster_grid_.Upload();

    // Light Probe SHï¼šä»Žå¿«ç…§ä¸Šä¼ åˆ° RHI å…¨å±€çŠ¶æ€
    const auto& snap = *render_pass_context_.snapshot;
    if (snap.light_probe_sh.valid) {
        runtime_context_.rhi_device->SetGlobalLightProbeSH(
            snap.light_probe_sh.coefficients, true);
    } else {
        glm::vec4 zero_sh[9] = {};
        runtime_context_.rhi_device->SetGlobalLightProbeSH(zero_sh, false);
    }

    // å…¨å±€æ¹¿åº¦åŒæ­¥åˆ° RHIï¼ˆæ¸²æŸ“çº¿ç¨‹è·¯å¾„ï¼‰
    runtime_context_.rhi_device->SetGlobalWetness(render_pass_context_.global_wetness);

    // DDGI: ä»Žå¿«ç…§é…ç½®åˆå§‹åŒ–/é‡é…ç½® + åŒæ­¥åˆ° RHI å…¨å±€çŠ¶æ€
    render_pass_context_.ddgi_active = false;
    render_pass_context_.ddgi_system = nullptr;
    if (snap.ddgi_config.enabled && runtime_context_.rhi_device->SupportsCompute()) {
        const auto& dcfg = snap.ddgi_config;
        if (dcfg.needs_reinit || !rs_->ddgi_system_.IsInitialized()) {
            dse::render::gi::DDGIVolumeConfig cfg;
            cfg.origin = dcfg.origin;
            cfg.extent = dcfg.extent;
            cfg.resolution = glm::ivec3(dcfg.resolution_x, dcfg.resolution_y, dcfg.resolution_z);
            cfg.irradiance_texels = dcfg.irradiance_texels;
            cfg.visibility_texels = dcfg.visibility_texels;
            cfg.rays_per_probe = dcfg.rays_per_probe;
            cfg.hysteresis = dcfg.hysteresis;
            if (rs_->ddgi_system_.IsInitialized()) {
                rs_->ddgi_system_.Reconfigure(runtime_context_.rhi_device.get(), cfg);
            } else {
                rs_->ddgi_system_.Init(runtime_context_.rhi_device.get(), cfg);
            }
        }
        if (rs_->ddgi_system_.IsInitialized()) {
            render_pass_context_.ddgi_system = &rs_->ddgi_system_;
            render_pass_context_.ddgi_active = true;
            render_pass_context_.ddgi_gi_intensity = dcfg.gi_intensity;
            render_pass_context_.ddgi_normal_bias = dcfg.normal_bias;
            const auto& res = rs_->ddgi_system_.GetResources();
            render_pass_context_.ddgi_irradiance_atlas = res.irradiance_atlas;
            render_pass_context_.ddgi_visibility_atlas = res.visibility_atlas;
        }
    }
    if (render_pass_context_.ddgi_active) {
        const auto& cfg = rs_->ddgi_system_.GetConfig();
        runtime_context_.rhi_device->SetGlobalDDGI(
            true, render_pass_context_.ddgi_irradiance_atlas,
            cfg.origin, cfg.ProbeSpacing(), cfg.resolution,
            cfg.irradiance_texels,
            render_pass_context_.ddgi_gi_intensity,
            render_pass_context_.ddgi_normal_bias);
    } else {
        runtime_context_.rhi_device->SetGlobalDDGI(
            false, 0, glm::vec3(0), glm::vec3(1), glm::ivec3(0), 8, 0.0f, 0.0f);
    }

    // Hi-Z AABB ä¸Šä¼ 
    if (render_resources_.hiz_aabb_ssbo && render_resources_.hiz_visibility_ssbo) {
        const auto& aabbs = modules_impl_->CachedAABBs();
        const int count = modules_impl_->CachedAABBCount();
        if (count > 0) {
            if (static_cast<size_t>(count) > render_resources_.hiz_ssbo_capacity) {
                const size_t new_cap = static_cast<size_t>(count) * 2;
                runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.hiz_aabb_ssbo);
                runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.hiz_visibility_ssbo);
                {
                    dse::render::GpuBufferDesc d{new_cap * sizeof(uint32_t), dse::render::GpuBufferUsage::kStorage, true, "hiz_visibility"};
                    render_resources_.hiz_visibility_ssbo = runtime_context_.rhi_device->CreateGpuBuffer(d, nullptr);
                }
                {
                    dse::render::GpuBufferDesc d{new_cap * 8 * sizeof(float), dse::render::GpuBufferUsage::kStorage, true, "hiz_aabb"};
                    render_resources_.hiz_aabb_ssbo = runtime_context_.rhi_device->CreateGpuBuffer(d, nullptr);
                }
                render_resources_.hiz_ssbo_capacity = new_cap;
                render_pass_context_.hiz_aabb_ssbo = render_resources_.hiz_aabb_ssbo;
                render_pass_context_.hiz_visibility_ssbo = render_resources_.hiz_visibility_ssbo;
                render_pass_context_.hiz_aabb_capacity = new_cap;
                DEBUG_LOG_INFO("[Hi-Z] SSBO resized (render thread): new_capacity={}", new_cap);
            }
            runtime_context_.rhi_device->UpdateGpuBuffer(
                render_resources_.hiz_aabb_ssbo, 0,
                count * sizeof(dse::gameplay3d::HiZAABB),
                aabbs.data());
            render_pass_context_.hiz_object_count = count;
        } else {
            render_pass_context_.hiz_object_count = 0;
        }
    }

    BuildRenderSceneQueues();

    // Camera-Relative Rendering: CPU mesh model matrix å‡åŽ» camera_offset
    rs_->render_scene_.ApplyCameraOffset(render_pass_context_.camera_offset);

    // â”€â”€ æ‰§è¡Œæ¸²æŸ“å›¾ â”€â”€
    ExecuteRenderGraph(*cmd_buffer);

    dse::runtime::SubmitAndEndRuntimeRenderFrame(*this, std::move(cmd_buffer));

    // GPU Timer â†’ RenderProfiler æ¡¥æŽ¥ï¼ˆæ¸²æŸ“çº¿ç¨‹è·¯å¾„ï¼‰
    {
        auto gpu_results = runtime_context_.rhi_device->GetAllGpuTimerResults();
        if (!gpu_results.empty()) {
            std::vector<dse::profiler::GpuPassTiming> timings;
            timings.reserve(gpu_results.size());
            for (const auto& entry : gpu_results) {
                timings.push_back({entry.name, entry.ms});
            }
            rs_->render_profiler_.UpdateGpuTimers(timings);
        }
    }

    // Hi-Z / GPU Driven: å¼‚æ­¥è¯»å›žï¼ˆåŒç¼“å†² stagingï¼Œå»¶è¿Ÿ 1 å¸§ï¼‰
    if (render_resources_.hiz_visibility_ssbo && render_pass_context_.hiz_object_count > 0
        && render_pass_context_.hiz_culling_enabled) {
        const int count = render_pass_context_.hiz_object_count;
        const size_t read_size = count * sizeof(uint32_t);
        bool has_data = runtime_context_.rhi_device->BeginGpuReadback(
            render_resources_.hiz_visibility_ssbo, 0, read_size);
        if (has_data) {
            size_t result_size = 0;
            const auto* raw = runtime_context_.rhi_device->GetLastReadbackResult(&result_size);
            if (raw && result_size >= read_size) {
                const auto* vis = static_cast<const uint32_t*>(raw);
                std::vector<uint32_t> visibility(vis, vis + count);
                modules_impl_->SetHiZVisibility(visibility);
            }
        }
    } else if (!render_pass_context_.hiz_culling_enabled
        && render_pass_context_.gpu_driven_active_this_frame && render_pass_context_.gpu_indirect_draw_count > 0
        && render_pass_context_.gpu_draw_cmd_ssbo) {
        const int count = render_pass_context_.gpu_indirect_draw_count;
        const size_t read_size = count * sizeof(DrawElementsIndirectCommand);
        bool has_data = runtime_context_.rhi_device->BeginGpuReadback(
            render_pass_context_.gpu_draw_cmd_ssbo, 0, read_size);
        if (has_data) {
            size_t result_size = 0;
            const auto* raw = runtime_context_.rhi_device->GetLastReadbackResult(&result_size);
            if (raw && result_size >= read_size) {
                const auto* cmds = static_cast<const DrawElementsIndirectCommand*>(raw);
                std::vector<uint32_t> visibility(count);
                int culled = 0;
                for (int i = 0; i < count; ++i) {
                    visibility[i] = cmds[i].instance_count > 0 ? 1u : 0u;
                    if (visibility[i] == 0) ++culled;
                }
                gpu_culled_last_frame_ = culled;
                runtime_context_.rhi_device->PatchLastFrameGPUCulledCount(culled);
            }
        }
    }

    dse::runtime::FinalizeRuntimeRenderFrame(*this);
    if (runtime_context_.rhi_device) {
        const auto rhi_stats = runtime_context_.rhi_device->GetFrameStats();
        rs_->render_profiler_.UpdateFromRhi(
            rhi_stats.draw_calls,
            0,
            rhi_stats.triangle_count,
            rhi_stats.sprite_count,
            rhi_stats.texture_binds,
            rhi_stats.shader_switches);
    }
    rs_->render_profiler_.EndFrame();

    // Render diagnostics
    if (const char* readback_diag = std::getenv("DSE_RENDER_READBACK_DIAG")) {
        if (readback_diag[0] != '\0' && readback_diag[0] != '0') {
            static int readback_diag_frame = 0;
            if (readback_diag_frame < 5 || (readback_diag_frame % 60) == 0) {
                LogReadbackStats("scene", ReadSceneColorRgba8WithSize());
                LogReadbackStats("main", ReadMainColorRgba8WithSize());
                LogDefaultFramebufferStats();
            }
            ++readback_diag_frame;
        }
    }

    auto render_end = std::chrono::high_resolution_clock::now();
    render_time_accumulator_ms_ += std::chrono::duration<float, std::milli>(render_end - render_begin).count();
    render_samples_ += 1;

    // Present (SwapBuffers) â€” åœ¨ render è®¡æ—¶ä¹‹å¤–ï¼Œé¿å… Present å»¶è¿Ÿæ±¡æŸ“ avg_render_ms
    if (runtime_context_.present_frame) {
        runtime_context_.present_frame();
    }
}
