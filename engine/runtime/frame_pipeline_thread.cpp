/**
 * @file frame_pipeline_thread.cpp
 * @brief FramePipeline render frame execution — PrepareRenderFrame + ExecuteRenderFrame.
 *        Render thread lifecycle is now managed by RenderThreadManager.
 */

#include "engine/runtime/frame_pipeline.h"
#include "engine/runtime/frame_pipeline_impl.h"
#include "engine/runtime/render_thread_manager.h"
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
#include <cassert>
#include <algorithm>
#include <iostream>

void FramePipeline::PrepareRenderFrame() {
    glm::vec3 early_camera_offset(0.0f);
    // ── 主线程：纯 CPU 工作 + ECS 读取 ──

    // 确保所有 dirty 的 TransformComponent 在渲染前更新 local_to_world
    if (runtime_context_.world) {
        rs_->transform_system_.Update(*runtime_context_.world);
    }

    if (runtime_context_.world) {
        // Camera-Relative: 提前获取相机位置作为 camera_offset
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

        // Clustered Forward+: 收集光源（CPU）— 光源位置减去 camera_offset
        dse::render::ExtractRenderSceneView(*runtime_context_.world, rs_->scene_view_);
        render_pass_context_.scene_view = &rs_->scene_view_;

        rs_->light_buffer_.CollectLightsFromView(rs_->scene_view_, early_camera_offset);

        // 获取主相机参数构建 cluster
        // 编辑器相机激活时 cluster 必须用编辑器相机的 view/proj 构建，
        // 否则 fragment 的 tile/z 与 grid 不匹配，点光/聚光丢失或错位。
        if (render_pass_context_.editor_mode && render_pass_context_.use_editor_camera) {
            const int sw = Screen::width();
            const int sh = Screen::height();
            // Camera-Relative: 光源已减去 camera_offset，editor view 需配套调整
            const glm::mat4 view_mat = render_pass_context_.editor_view *
                glm::translate(glm::mat4(1.0f), early_camera_offset);
            const glm::mat4& proj = render_pass_context_.editor_projection;
            float near_p = 0.1f, far_p = 1000.0f;
            if (proj[2][3] != 0.0f) {  // glm::perspective 深度约定
                if (proj[2][2] - 1.0f != 0.0f) near_p = proj[3][2] / (proj[2][2] - 1.0f);
                if (proj[2][2] + 1.0f != 0.0f) far_p  = proj[3][2] / (proj[2][2] + 1.0f);
            }
            rs_->cluster_grid_.Build(view_mat, proj, near_p, far_p, sw, sh,
                                rs_->light_buffer_.point_lights(), rs_->light_buffer_.spot_lights());
        } else if (cam_entity != entt::null) {
            auto& cam = cam_view_3d.get<dse::Camera3DComponent>(cam_entity);
            const int sw = Screen::width();
            const int sh = Screen::height();
            glm::mat4 proj = glm::perspective(glm::radians(cam.fov),
                static_cast<float>(sw) / static_cast<float>(std::max(1, sh)),
                cam.near_clip, cam.far_clip);
            // Camera-Relative: 光源已减去 camera_offset，cluster view 也用 camera-at-origin
            glm::mat4 view_mat = glm::mat4(1.0f);
            if (runtime_context_.world->registry().all_of<TransformComponent>(cam_entity)) {
                auto& tf = runtime_context_.world->registry().get<TransformComponent>(cam_entity);
                glm::vec3 front = tf.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 up    = tf.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                view_mat = glm::lookAt(glm::vec3(0.0f), front, up);
            }
            rs_->cluster_grid_.Build(view_mat, proj, cam.near_clip, cam.far_clip, sw, sh,
                                rs_->light_buffer_.point_lights(), rs_->light_buffer_.spot_lights());
        } else {
            // 无有效相机：用空光源列表重建，避免 SSBO 残留上一帧数据
            static const std::vector<dse::render::GPUPointLight> kNoPointLights;
            static const std::vector<dse::render::GPUSpotLight> kNoSpotLights;
            rs_->cluster_grid_.Build(glm::mat4(1.0f),
                glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 1000.0f),
                0.1f, 1000.0f, Screen::width(), Screen::height(),
                kNoPointLights, kNoSpotLights);
        }
    }

    // TAA: 预检测 ECS 组件
    render_pass_context_.taa_active = false;
    if (taa_pass_ && render_pass_context_.pipeline_features.taa && runtime_context_.world) {
        auto pp_view = runtime_context_.world->registry().view<dse::PostProcessComponent>();
        for (auto entity : pp_view) {
            auto& pp = pp_view.get<dse::PostProcessComponent>(entity);
            if (pp.enabled && pp.taa_enabled) {
                render_pass_context_.taa_active =
                    static_cast<bool>(render_pass_context_.render_targets.taa);
                break;
            }
        }
        taa_pass_->UpdateJitter(taa_frame_index_++);
        render_pass_context_.taa_jitter = taa_pass_->GetCurrentJitter();
    }

    render_pass_context_.delta_time = Time::delta_time();

    // 全局湿度：从 ECS WeatherComponent 直接读取（雨 → wetness = intensity）
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

    // 植被风参数（渲染线程路径）
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
        // Phase 1：仅在主线程 Prepare 提取帧值；写入 RHI 全局态延后到渲染线程 Execute。
        render_pass_context_.foliage_wind =
            glm::vec4(Time::TimeSinceStartup(), wind_strength, wind_dir.x, wind_dir.y);
    }
    // 植被推力场
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
        render_pass_context_.foliage_push = glm::vec4(push_pos, 2.0f);
    }

    // 捕获快照 + 翻转双缓冲
    // Camera-Relative: GPU Driven / 队列构建需要 camera_offset
    render_pass_context_.camera_offset = early_camera_offset;

    // GPU 场景准备 + 渲染队列构建 + web 蒙皮烘焙（均读取 ECS，须在 Update 阶段完成）
    PrepareGPUSceneAndQueues();

    CaptureThinSnapshot();
    FlipSnapshotIndex();
    render_pass_context_.snapshot = &read_snapshot();
    render_pass_context_.camera_offset = render_pass_context_.snapshot->camera_offset;
}

void FramePipeline::ExecuteRenderFrame() {
    assert(!snapshot_writing_.load(std::memory_order_acquire) &&
           "F2: 薄快照契约违反——主线程写快照期间渲染线程不得读");
    SnapshotPhaseScope f2_read_scope(snapshot_reading_);
    rs_->render_profiler_.BeginFrame();
    auto render_begin = std::chrono::high_resolution_clock::now();

    dse::runtime::BeginRuntimeRenderFrame(*this);
    auto cmd_buffer = dse::runtime::CreateRuntimeRenderCommandBuffer(*this);
    dse::runtime::BindRuntimeShadowMaps(*this);

    // GPU Compute Skinning: dispatch Update 阶段收集的请求，绑定输出 SSBO
    if (rs_->gpu_skinning_system_.IsAvailable() && rs_->gpu_skinning_system_.GetTotalSkinnedVertices() > 0) {
        rs_->gpu_skinning_system_.Dispatch();
        runtime_context_.rhi_device->BindGpuBuffer(
            rs_->gpu_skinning_system_.GetOutputBuffer(), 20);  // binding 20 = ComputeSkinBuf
    }

    // GPU 上传：光源 SSBO
    rs_->light_buffer_.Upload();

    // GPU 上传：cluster SSBO
    rs_->cluster_grid_.Upload();

    // Light Probe SH：从快照上传到 RHI 全局状态
    const auto& snap = *render_pass_context_.snapshot;
    if (snap.light_probe_sh.valid) {
        runtime_context_.rhi_device->SetGlobalLightProbeSH(
            snap.light_probe_sh.coefficients, true);
    } else {
        glm::vec4 zero_sh[9] = {};
        runtime_context_.rhi_device->SetGlobalLightProbeSH(zero_sh, false);
    }

    // 全局湿度同步到 RHI（渲染线程路径）
    runtime_context_.rhi_device->SetGlobalWetness(render_pass_context_.global_wetness);

    // Phase 1：植被风/推力场从帧值写入 RHI 全局态（渲染线程 Execute，而非主线程 Prepare）。
    runtime_context_.rhi_device->SetGlobalFoliageWind(render_pass_context_.foliage_wind);
    runtime_context_.rhi_device->SetGlobalFoliagePush(render_pass_context_.foliage_push);

    // DDGI: 从快照配置初始化/重配置 + 同步到 RHI 全局状态
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
            false, {}, glm::vec3(0), glm::vec3(1), glm::ivec3(0), 8, 0.0f, 0.0f);
    }

    // Hi-Z AABB 上传
    if (render_resources_.hiz_visibility_ssbo) {
        const auto& aabbs = modules_impl_->CachedAABBs();
        const int count = modules_impl_->CachedAABBCount();
        if (count > 0) {
            // hiz_visibility 仍为单缓冲（GPU compute 写、跨帧读回语义），容量不足时增长。
            if (static_cast<size_t>(count) > render_resources_.hiz_ssbo_capacity) {
                const size_t new_cap = static_cast<size_t>(count) * 2;
                // 旧 hiz_visibility 可能仍被在飞帧 GPU meshlet-cull 引用，直接 Delete 属 GPU
                // use-after-free。渲染线程不能照搬主线程 rhi->WaitIdle()（vkDeviceWaitIdle 与主线程
                // EndSingleTimeCommands 队列提交有外部同步顾虑）；用「删缓冲前等在飞帧 fence」原语。
                // capacity 起始 65536、按 2× 摊还增长，极少触发。
                runtime_context_.rhi_device->WaitForInFlightGpuUse();
                runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.hiz_visibility_ssbo);
                {
                    dse::render::GpuBufferDesc d{new_cap * sizeof(uint32_t), dse::render::GpuBufferUsage::kStorage, true, "hiz_visibility"};
                    render_resources_.hiz_visibility_ssbo = runtime_context_.rhi_device->CreateGpuBuffer(d, nullptr);
                }
                render_resources_.hiz_ssbo_capacity = new_cap;
                render_pass_context_.hiz_visibility_ssbo = render_resources_.hiz_visibility_ssbo;
                DEBUG_LOG_INFO("[Hi-Z] visibility SSBO resized (render thread): new_capacity={}", new_cap);
            }
            // hiz_aabb：per-in-flight ring。上传在 ExecuteRenderFrame 内、BeginRuntimeRenderFrame
            //（AcquireNextImage 已等待当前槽位 fence）之后，故 Acquire 当前槽位即可安全覆写、无需额外等待；
            // 每帧只写当前槽位、不再触发跨帧总闸（skip_host_sync=true）。
            render_resources_.hiz_aabb_ssbo = render_resources_.hiz_aabb_ring.Acquire(
                *runtime_context_.rhi_device,
                static_cast<size_t>(count) * sizeof(dse::gameplay3d::HiZAABB),
                dse::render::GpuBufferUsage::kStorage);
            runtime_context_.rhi_device->UpdateGpuBuffer(
                render_resources_.hiz_aabb_ssbo, 0,
                count * sizeof(dse::gameplay3d::HiZAABB),
                aabbs.data());
            render_pass_context_.hiz_aabb_ssbo = render_resources_.hiz_aabb_ssbo;
            render_pass_context_.hiz_aabb_capacity = static_cast<size_t>(count);
            render_pass_context_.hiz_object_count = count;
        } else {
            render_pass_context_.hiz_object_count = 0;
        }
    }

    // Camera-Relative Rendering: CPU mesh model matrix 减去 camera_offset
    rs_->render_scene_.ApplyCameraOffset(render_pass_context_.camera_offset);

    // ── 执行渲染图 ──
    ExecuteRenderGraph(*cmd_buffer);

    dse::runtime::SubmitAndEndRuntimeRenderFrame(*this, std::move(cmd_buffer));

    // GPU Timer → RenderProfiler 桥接（渲染线程路径）
    {
        auto gpu_results = runtime_context_.rhi_device->GetAllGpuTimerResults();
        if (!gpu_results.empty()) {
            std::vector<dse::profiler::GpuPassTiming> timings;
            timings.reserve(gpu_results.size());
            float gpu_frame_ms = 0.0f;
            for (const auto& entry : gpu_results) {
                timings.push_back({entry.name, entry.ms});
                if (entry.ms > 0.0f) gpu_frame_ms += entry.ms;
            }
            rs_->render_profiler_.UpdateGpuTimers(timings);
            if (gpu_frame_ms > 0.0f) stats_.RecordGpuSegment(gpu_frame_ms);
        }
    }

    // Hi-Z / GPU Driven: 异步读回（双缓冲 staging，延迟 1 帧）
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
    const float render_ms =
        std::chrono::duration<float, std::milli>(render_end - render_begin).count();
    stats_.RecordRender(render_ms);
    stats_.RecordExecuteSegment(render_ms);

    // Present (SwapBuffers) — 在 render 计时之外，避免 Present 延迟污染 avg_render_ms
    if (render_thread_mgr_->IsActive() && runtime_context_.present_frame) {
        runtime_context_.present_frame();
    }
}
