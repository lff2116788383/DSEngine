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
#include "engine/ecs/audio.h"
#include "engine/ecs/physics_2d.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/ui.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/core/event_bus.h"
#include "engine/core/service_locator.h"
#include "engine/core/job_system.h"
#include "engine/scene/scene.h"
#include "engine/render/rhi/rhi_factory.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glad/gl.h>
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
struct ReadbackStats {
    int width = 0;
    int height = 0;
    std::size_t pixels = 0;
    std::size_t non_black = 0;
    unsigned int max_rgb = 0;
    double avg_rgb = 0.0;
};

ReadbackStats AnalyzeReadback(const RenderTargetReadback& readback) {
    ReadbackStats stats;
    stats.width = readback.width;
    stats.height = readback.height;
    stats.pixels = readback.pixels.size() / 4u;
    if (stats.pixels == 0) {
        return stats;
    }

    unsigned long long rgb_sum = 0;
    for (std::size_t i = 0; i + 3 < readback.pixels.size(); i += 4) {
        const unsigned int rgb = static_cast<unsigned int>(readback.pixels[i]) +
                                 static_cast<unsigned int>(readback.pixels[i + 1]) +
                                 static_cast<unsigned int>(readback.pixels[i + 2]);
        rgb_sum += rgb;
        stats.max_rgb = std::max(stats.max_rgb, rgb);
        if (rgb > 8u) {
            ++stats.non_black;
        }
    }
    stats.avg_rgb = static_cast<double>(rgb_sum) / static_cast<double>(stats.pixels);
    return stats;
}

void LogReadbackStats(const char* label, const RenderTargetReadback& readback) {
    const auto stats = AnalyzeReadback(readback);
    DEBUG_LOG_INFO("Render readback {}: size={}x{} pixels={} non_black={} max_rgb={} avg_rgb={}",
                   label,
                   stats.width,
                   stats.height,
                   stats.pixels,
                   stats.non_black,
                   stats.max_rgb,
                   stats.avg_rgb);
}

void LogDefaultFramebufferStats() {
    const int width = Screen::width();
    const int height = Screen::height();
    if (width <= 0 || height <= 0) {
        DEBUG_LOG_WARN("Render readback default framebuffer skipped: invalid size {}x{}", width, height);
        return;
    }

    RenderTargetReadback readback;
    readback.width = width;
    readback.height = height;
    readback.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0u);

    GLint previous_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, readback.pixels.data());
    const GLenum gl_error = glGetError();
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_fbo));

    LogReadbackStats("default_backbuffer", readback);
    if (gl_error != GL_NO_ERROR) {
        DEBUG_LOG_ERROR("Render readback default framebuffer gl_error=0x{}", static_cast<unsigned int>(gl_error));
    }
}

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
    if (auto* registered = dse::core::ServiceLocator::Instance().Get<FramePipeline>()) {
        return *registered;
    }
    throw std::runtime_error("FramePipeline::Instance() requires an EngineInstance-managed FramePipeline registration");
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
    const auto rhi_backend = dse::render::ResolveRhiBackendFromEnv();
    runtime_context_.rhi_device = dse::render::CreateRhiDevice(rhi_backend);
    DEBUG_LOG_INFO("FramePipeline RHI 后端: {}", dse::render::RhiBackendToString(rhi_backend));

    // D3D11 / Vulkan 后端需要用平台窗口句柄完成设备初始化
    if (runtime_context_.native_window_handle != nullptr || rhi_backend == RhiBackend::D3D11) {
        const int init_w = Screen::width() > 0 ? Screen::width() : 1280;
        const int init_h = Screen::height() > 0 ? Screen::height() : 720;
        bool init_ok = false;
        try {
            init_ok = runtime_context_.rhi_device->InitDevice(
                runtime_context_.native_window_handle, init_w, init_h);
        } catch (const std::exception& e) {
            DEBUG_LOG_ERROR("FramePipeline [{}] InitDevice exception: {}",
                dse::render::RhiBackendToString(rhi_backend), e.what());
        } catch (...) {
            DEBUG_LOG_ERROR("FramePipeline [{}] InitDevice unknown exception",
                dse::render::RhiBackendToString(rhi_backend));
        }
        if (!init_ok) {
            DEBUG_LOG_ERROR("FramePipeline init failed: [{}] InitDevice returned false",
                dse::render::RhiBackendToString(rhi_backend));
            return false;
        }
    }

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
    
    // 始终创建最终合成 RenderTarget：editor 直接展示该纹理，runtime 再 present 到默认 framebuffer。
    RenderTargetDesc main_rt_desc{};
    main_rt_desc.width = render_width;
    main_rt_desc.height = render_height;
    main_rt_desc.has_color = true;
    main_rt_desc.has_depth = false;
    render_resources_.main_render_target = runtime_context_.rhi_device->CreateRenderTarget(main_rt_desc);





    
    // 使用支持 HDR 的浮点纹理作为 Scene Render Target，这是泛光和色调映射的基础
    // msaa_samples=4：开启 4x MSAA（不支持时 resource_mgr 自动降级为 1x）
    {
        RenderTargetDesc scene_desc{};
        scene_desc.width = render_width;
        scene_desc.height = render_height;
        scene_desc.has_color = true;
        scene_desc.has_depth = true;
        scene_desc.msaa_samples = 4;
        render_resources_.scene_render_target = runtime_context_.rhi_device->CreateRenderTarget(scene_desc);
    }
    render_resources_.ui_render_target = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false});
    render_resources_.prez_render_target = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, false, true}); // Only depth
    
    for (int i = 0; i < CSM_CASCADES; ++i) {
        render_resources_.shadow_render_target[i] = runtime_context_.rhi_device->CreateRenderTarget({2048, 2048, false, true}); // Shadow map resolution
    }
    for (int i = 0; i < 4; ++i) {
        render_resources_.spot_shadow_render_target[i] = runtime_context_.rhi_device->CreateRenderTarget({1024, 1024, false, true});
        render_resources_.point_shadow_render_target[i] = runtime_context_.rhi_device->CreateRenderTarget({1024, 1024, false, true, false, true});
    }

    if (render_resources_.pp_bloom_extract_rt == 0) {
        render_resources_.pp_bloom_extract_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    }
    
    // Create mip chain for Dual Filter Bloom (5 levels)
    // allow_uav=true：允许 CS 写入（D3D11 Bloom Compute Shader 路径使用）
    if (render_resources_.pp_bloom_mip_rts.empty()) {
        int mip_width = render_width / 2;
        int mip_height = render_height / 2;
        for (int i = 0; i < 5; ++i) {
            RenderTargetDesc bloom_mip_desc{};
            bloom_mip_desc.width = mip_width;
            bloom_mip_desc.height = mip_height;
            bloom_mip_desc.has_color = true;
            bloom_mip_desc.has_depth = false;
            bloom_mip_desc.allow_uav = true;
            render_resources_.pp_bloom_mip_rts.push_back(
                runtime_context_.rhi_device->CreateRenderTarget(bloom_mip_desc));
            mip_width /= 2;
            mip_height /= 2;
            if (mip_width < 1) mip_width = 1;
            if (mip_height < 1) mip_height = 1;
        }
    }

    // SSAO: 半分辨率单通道 RT
    if (render_resources_.pp_ssao_rt == 0) {
        render_resources_.pp_ssao_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width / 2, render_height / 2, true, false, false});
    }
    if (render_resources_.pp_ssao_blur_rt == 0) {
        render_resources_.pp_ssao_blur_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width / 2, render_height / 2, true, false, false});
    }
    // Contact Shadow: 半分辨率单通道 RT
    if (render_resources_.pp_contact_shadow_rt == 0) {
        render_resources_.pp_contact_shadow_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width / 2, render_height / 2, true, false, false});
    }
    // FXAA: 全分辨率 RT
    if (render_resources_.pp_fxaa_rt == 0) {
        render_resources_.pp_fxaa_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // TAA: 全分辨率 RT
    if (render_resources_.pp_taa_rt == 0) {
        render_resources_.pp_taa_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // DOF: 全分辨率 RT
    if (render_resources_.pp_dof_rt == 0) {
        render_resources_.pp_dof_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // SSR: 半分辨率 RT
    if (render_resources_.pp_ssr_rt == 0) {
        render_resources_.pp_ssr_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width / 2, render_height / 2, true, false, false});
    }
    // Motion Vector: 全分辨率 RT (RG16F 速度场)
    if (render_resources_.pp_motion_vector_rt == 0) {
        render_resources_.pp_motion_vector_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // Outline / Edge Detection: 全分辨率 RT
    if (render_resources_.pp_outline_rt == 0) {
        render_resources_.pp_outline_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // Volumetric Fog: 全分辨率 RT（存储 scene+fog 合成结果）
    if (render_resources_.pp_fog_rt == 0) {
        render_resources_.pp_fog_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }

    // WBOIT accumulation: 全分辨率 RGBA16F（需要 HDR 精度累积加权颜色）
    if (render_resources_.wboit_accum_rt == 0) {
        render_resources_.wboit_accum_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // WBOIT revealage: 全分辨率（R 通道存储 prod(1-alpha_i)）
    if (render_resources_.wboit_reveal_rt == 0) {
        render_resources_.wboit_reveal_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }

    // GBuffer MRT: 3 颜色附件 (albedo, normal, position) + 深度
    if (render_resources_.gbuffer_rt == 0) {
        RenderTargetDesc gbuf_desc;
        gbuf_desc.width = render_width;
        gbuf_desc.height = render_height;
        gbuf_desc.has_color = true;
        gbuf_desc.has_depth = true;
        gbuf_desc.generate_mipmaps = false;
        gbuf_desc.color_attachment_count = 3;
        render_resources_.gbuffer_rt = runtime_context_.rhi_device->CreateRenderTarget(gbuf_desc);
    }
    // Deferred Lighting output: 全分辨率单颜色附件
    if (render_resources_.deferred_lighting_rt == 0) {
        render_resources_.deferred_lighting_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }

    // Auto Exposure: 64x64 临时亮度 + 2 个 1x1 ping-pong RT
    if (render_resources_.pp_lum_temp_rt == 0) {
        render_resources_.pp_lum_temp_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {64, 64, true, false, false});
    }
    for (int i = 0; i < 2; ++i) {
        if (render_resources_.pp_lum_adapted_rt[i] == 0) {
            render_resources_.pp_lum_adapted_rt[i] = runtime_context_.rhi_device->CreateRenderTarget(
                {1, 1, true, false, false});
        }
    }

    PipelineStateDesc sprite_desc;
    sprite_desc.blend_enabled = true;
    sprite_desc.blend_src = BlendFactor::SrcAlpha;
    sprite_desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
    sprite_desc.depth_test_enabled = false;
    sprite_desc.depth_write_enabled = false;
    sprite_desc.culling_enabled = false;
    render_resources_.sprite_pipeline_state = runtime_context_.rhi_device->CreatePipelineState(sprite_desc);
    if (render_resources_.sprite_pipeline_state == 0) {
        DEBUG_LOG_ERROR("FramePipeline init failed: sprite pipeline state creation returned 0");
        Shutdown();
        return false;
    }

    PipelineStateDesc mesh_desc;
    mesh_desc.blend_enabled = false;
    mesh_desc.depth_test_enabled = true;
    mesh_desc.depth_write_enabled = true;
    mesh_desc.culling_enabled = true;
    mesh_desc.depth_func = CompareFunc::LessEqual; // scene color pass must accept depth equality after PreZ.
    render_resources_.mesh_pipeline_state = runtime_context_.rhi_device->CreatePipelineState(mesh_desc);
    if (render_resources_.mesh_pipeline_state == 0) {
        DEBUG_LOG_ERROR("FramePipeline init failed: mesh pipeline state creation returned 0");
        Shutdown();
        return false;
    }

    PipelineStateDesc prez_desc;
    prez_desc.blend_enabled = false;
    prez_desc.depth_test_enabled = true;
    prez_desc.depth_write_enabled = true;
    prez_desc.culling_enabled = true;
    // In a real engine we disable color write, but for now we'll just write to a depth-only FBO
    render_resources_.prez_pipeline_state = runtime_context_.rhi_device->CreatePipelineState(prez_desc);
    if (render_resources_.prez_pipeline_state == 0) {
        DEBUG_LOG_ERROR("FramePipeline init failed: prez pipeline state creation returned 0");
        Shutdown();
        return false;
    }

    PipelineStateDesc shadow_desc;
    shadow_desc.blend_enabled = false;
    shadow_desc.depth_test_enabled = true;
    shadow_desc.depth_write_enabled = true;
    shadow_desc.culling_enabled = true;
    shadow_desc.cull_face = CullFace::Front; // avoid peter-panning
    render_resources_.shadow_pipeline_state = runtime_context_.rhi_device->CreatePipelineState(shadow_desc);
    if (render_resources_.shadow_pipeline_state == 0) {
        DEBUG_LOG_ERROR("FramePipeline init failed: shadow pipeline state creation returned 0");
        Shutdown();
        return false;
    }

    PipelineStateDesc composite_desc;
    composite_desc.blend_enabled = false;
    composite_desc.blend_src = BlendFactor::SrcAlpha;
    composite_desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
    composite_desc.depth_test_enabled = false;
    composite_desc.depth_write_enabled = false;
    composite_desc.culling_enabled = false;
    render_resources_.composite_pipeline_state = runtime_context_.rhi_device->CreatePipelineState(composite_desc);
    if (render_resources_.composite_pipeline_state == 0) {
        DEBUG_LOG_ERROR("FramePipeline init failed: composite pipeline state creation returned 0");
        Shutdown();
        return false;
    }

    PipelineStateDesc decal_blend_desc;
    decal_blend_desc.blend_enabled = true;
    decal_blend_desc.blend_src = BlendFactor::SrcAlpha;
    decal_blend_desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
    decal_blend_desc.depth_test_enabled = false;
    decal_blend_desc.depth_write_enabled = false;
    decal_blend_desc.culling_enabled = false;
    render_resources_.decal_blend_pipeline_state = runtime_context_.rhi_device->CreatePipelineState(decal_blend_desc);
    if (render_resources_.decal_blend_pipeline_state == 0) {
        DEBUG_LOG_ERROR("FramePipeline init failed: decal_blend pipeline state creation returned 0");
        Shutdown();
        return false;
    }

    // WBOIT accumulation: additive blend (ONE, ONE), depth test OFF, depth write OFF
    PipelineStateDesc wboit_accum_desc;
    wboit_accum_desc.blend_enabled = true;
    wboit_accum_desc.blend_src = BlendFactor::One;
    wboit_accum_desc.blend_dst = BlendFactor::One;
    wboit_accum_desc.alpha_blend_src = BlendFactor::One;
    wboit_accum_desc.alpha_blend_dst = BlendFactor::One;
    wboit_accum_desc.depth_test_enabled = false;
    wboit_accum_desc.depth_write_enabled = false;
    wboit_accum_desc.culling_enabled = false;
    render_resources_.wboit_accum_pipeline_state = runtime_context_.rhi_device->CreatePipelineState(wboit_accum_desc);

    // WBOIT revealage: blend (ZERO, ONE_MINUS_SRC_ALPHA), depth test OFF, depth write OFF
    PipelineStateDesc wboit_reveal_desc;
    wboit_reveal_desc.blend_enabled = true;
    wboit_reveal_desc.blend_src = BlendFactor::Zero;
    wboit_reveal_desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
    wboit_reveal_desc.alpha_blend_src = BlendFactor::Zero;
    wboit_reveal_desc.alpha_blend_dst = BlendFactor::OneMinusSrcAlpha;
    wboit_reveal_desc.depth_test_enabled = false;
    wboit_reveal_desc.depth_write_enabled = false;
    wboit_reveal_desc.culling_enabled = false;
    render_resources_.wboit_reveal_pipeline_state = runtime_context_.rhi_device->CreatePipelineState(wboit_reveal_desc);

    DEBUG_LOG_INFO("FramePipeline init: systems init begin");
    gameplay2d_module_.OnInit(*runtime_context_.world, runtime_context_.rhi_device.get(), &asset_manager);
    mesh_render_system_.SetAssetManager(&asset_manager);
    DEBUG_LOG_INFO("FramePipeline init: systems init complete");

    const auto runtime_modules = ResolveRuntimeModules();
    const bool enable_gameplay3d = runtime_modules.empty() ||
        std::find(runtime_modules.begin(), runtime_modules.end(), "Gameplay3D") != runtime_modules.end();
    DEBUG_LOG_INFO("FramePipeline init: Gameplay3D module enabled={}", enable_gameplay3d);
#if defined(DSE_ENABLE_3D) && defined(DSE_ENABLE_PHYSX)
    // PhysX 3D 物理必须在 Gameplay3D 模块之前初始化并注册到 ServiceLocator，
    // 否则 FractureSystem::SetPhysics3D() 会拿到 nullptr，导致碎片同时启用
    // CPU fallback 物理和 PhysX 物理，两套物理交替覆写 transform → 闪烁。
    if (physics3d_system_.Init(*runtime_context_.world)) {
        dse::core::ServiceLocator::Instance().Register<dse::physics3d::Physics3DSystem, dse::physics3d::Physics3DSystem>(
            std::shared_ptr<dse::physics3d::Physics3DSystem>(&physics3d_system_, [](auto*) {}));
        physics3d_system_initialized_ = true;
        DEBUG_LOG_INFO("FramePipeline init: Physics3DSystem (PhysX) initialized and registered");
    } else {
        DEBUG_LOG_WARN("FramePipeline init: Physics3DSystem init failed, 3D physics will be unavailable");
    }
#endif

#ifdef DSE_ENABLE_3D
    if (enable_gameplay3d) {
        if (!gameplay3d_module_.OnInit(*runtime_context_.world, runtime_context_.rhi_device.get(), &asset_manager)) {
            DEBUG_LOG_ERROR("FramePipeline init failed: Gameplay3D module OnInit returned false");
        } else {
            DEBUG_LOG_INFO("FramePipeline init: Gameplay3D module OnInit OK (static)");
            builtin_gameplay3d_enabled_ = true;
        }
    }
#else
    if (enable_gameplay3d) {
        DEBUG_LOG_INFO("FramePipeline init: Gameplay3D built-in fallback enabled (particle/steering/animator)");
        particle3d_system_.SetAssetManager(&asset_manager);
        particle3d_system_.Init(*runtime_context_.world, runtime_context_.rhi_device.get());
        dse::gameplay3d::AnimatorSystem::SetAssetManager(&asset_manager);
        builtin_gameplay3d_enabled_ = true;
    }
#endif

    DEBUG_LOG_INFO("FramePipeline init: business bootstrap begin");

    runtime_context_.audio_system = &gameplay2d_module_.audio_system();

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

    // Clustered Forward+ 光源 SSBO + Cluster 网格初始化
    light_buffer_.Init(runtime_context_.rhi_device.get());
    cluster_grid_.Init(runtime_context_.rhi_device.get());

    // Light Probe SH Bake 系统初始化
    light_probe_system_.Init(runtime_context_.rhi_device.get());

    // Reflection Probe + IBL 系统初始化（生成 BRDF LUT）
    reflection_probe_system_.Init(runtime_context_.rhi_device.get());

    initialized_ = true;
    if (auto* event_bus = dse::core::ServiceLocator::Instance().Get<dse::core::EventBus>()) {
        event_bus->Publish<dse::core::SceneLifecycleEvent>(dse::core::SceneLifecyclePhase::Init);
    }
    return true;
}

void FramePipeline::Shutdown() {
    if (!initialized_) {
        return;
    }
    if (auto* event_bus = dse::core::ServiceLocator::Instance().Get<dse::core::EventBus>()) {
        event_bus->Publish<dse::core::SceneLifecycleEvent>(dse::core::SceneLifecyclePhase::Shutdown);
    }
    auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
    render_graph_dag_.Reset();
    dse::runtime::ShutdownBusinessRuntime(runtime_context_);
    gameplay2d_module_.OnShutdown(*runtime_context_.world);
#ifdef DSE_ENABLE_3D
    if (builtin_gameplay3d_enabled_) {
        gameplay3d_module_.OnShutdown(*runtime_context_.world);
        builtin_gameplay3d_enabled_ = false;
    }
#else
    if (builtin_gameplay3d_enabled_) {
        particle3d_system_.Shutdown(*runtime_context_.world);
        particle3d_system_.SetAssetManager(nullptr);
        dse::gameplay3d::AnimatorSystem::SetAssetManager(nullptr);
        builtin_gameplay3d_enabled_ = false;
    }
#endif

    // Dynamic modules own 3D render-side systems and may still reference asset/RHI resources.
    // Shut them down before global GPU resource release and before unloading their DLLs.
    for (auto& mod : modules_) {
        if (mod.instance) {
            mod.instance->OnShutdown(*runtime_context_.world);
            if (mod.destroy) {
                mod.destroy(mod.instance);
            }
            mod.instance = nullptr;
        }
    }

    // ECS components may hold runtime/shared_ptr state whose destructors live in dynamically
    // loaded gameplay modules or touch asset/RHI objects. Clear the world while module DLLs,
    // AssetManager and RHI are still alive, instead of deferring registry destruction to
    // EngineInstance teardown after modules_.clear() unloads DLLs.
    if (runtime_context_.world) {
        runtime_context_.world->Clear();
    }

    modules_.clear();

    mesh_render_system_.SetAssetManager(nullptr);
#if defined(DSE_ENABLE_3D) && defined(DSE_ENABLE_PHYSX)
    if (physics3d_system_initialized_) {
        dse::core::ServiceLocator::Instance().Reset<dse::physics3d::Physics3DSystem>();
        physics3d_system_.Shutdown();
        physics3d_system_initialized_ = false;
    }
#endif
    cluster_grid_.Shutdown();
    light_buffer_.Shutdown();
    light_probe_system_.Shutdown();
    reflection_probe_system_.Shutdown(runtime_context_.rhi_device.get());
    taa_pass_ = nullptr;

    asset_manager.ReleaseGpuResources();

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
    asset_manager.PumpHotReloads();
    dse::runtime::TickBusinessRuntime(runtime_context_, delta_time);

    dse::runtime::RunRuntimeUpdateGraph(*this, delta_time);
#ifndef DSE_ENABLE_3D
    if (builtin_gameplay3d_enabled_) {
        particle3d_system_.Update(*runtime_context_.world, delta_time);
        steering_system_.Update(*runtime_context_.world, delta_time);
        dse::gameplay3d::AnimatorSystem::Update(*runtime_context_.world, delta_time);
    }
#endif

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
    dse::runtime::BeginRuntimeRenderFrame(*this);
    
    auto cmd_buffer = dse::runtime::CreateRuntimeRenderCommandBuffer(*this);
    
    dse::runtime::BindRuntimeShadowMaps(*this);

    // Clustered Forward+: 每帧收集光源 → 构建 Cluster → 上传 SSBO
    if (runtime_context_.world) {
        light_buffer_.CollectLights(*runtime_context_.world);
        light_buffer_.Upload();

        // 获取主相机参数用于 cluster 构建
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
        if (cam_entity != entt::null) {
            auto& cam = cam_view_3d.get<dse::Camera3DComponent>(cam_entity);
            const int sw = Screen::width();
            const int sh = Screen::height();
            glm::mat4 proj = glm::perspective(glm::radians(cam.fov),
                static_cast<float>(sw) / static_cast<float>(std::max(1, sh)),
                cam.near_clip, cam.far_clip);
            glm::mat4 view_mat = glm::mat4(1.0f);
            if (runtime_context_.world->registry().all_of<TransformComponent>(cam_entity)) {
                auto& tf = runtime_context_.world->registry().get<TransformComponent>(cam_entity);
                glm::vec3 front = tf.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 up    = tf.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                view_mat = glm::lookAt(tf.position, tf.position + front, up);
            }
            cluster_grid_.Build(view_mat, proj, cam.near_clip, cam.far_clip, sw, sh,
                                light_buffer_.point_lights(), light_buffer_.spot_lights());
            cluster_grid_.Upload();
        }
    }

    // Light Probe SH: 查询最近 probe，传给 GPU UBO（无 probe 时 fallback 到 ambient_intensity）
    if (runtime_context_.world) {
        glm::vec3 cam_pos(0.0f);
        auto cam_view = runtime_context_.world->registry().view<TransformComponent, dse::Camera3DComponent>();
        for (auto e : cam_view) {
            auto& cam = cam_view.get<dse::Camera3DComponent>(e);
            if (cam.enabled) {
                cam_pos = cam_view.get<TransformComponent>(e).position;
                break;
            }
        }
        light_probe_system_.UpdateGlobalSH(*runtime_context_.world,
                                            runtime_context_.rhi_device.get(), cam_pos);
    } else {
        glm::vec4 sh_coeffs[9] = {};
        runtime_context_.rhi_device->SetGlobalLightProbeSH(sh_coeffs, false);
    }

    // TAA: 预检测 ECS 组件，提前设置 taa_active（ForwardScenePass 需要在场景渲染前知道是否应用 jitter）
    render_pass_context_.taa_active = false;
    if (taa_pass_ && runtime_context_.world) {
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

    // 每帧更新 Auto Exposure 所需的 delta_time
    render_pass_context_.delta_time = Time::delta_time();

    ExecuteRenderGraph(*cmd_buffer);
    
    dse::runtime::SubmitAndEndRuntimeRenderFrame(*this, std::move(cmd_buffer));
    dse::runtime::FinalizeRuntimeRenderFrame(*this);
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
#if defined(DSE_ENABLE_3D) && defined(DSE_ENABLE_PHYSX)
        auto physics3d_view = runtime_context_.world->registry().view<dse::RigidBody3DComponent>();
        for (auto entity : physics3d_view) {
            (void)entity;
            ++physics_bodies;
        }
#endif
        size_t particle_emitters = 0;
        size_t active_particles = 0;
        auto particle2d_view = runtime_context_.world->registry().view<ParticleEmitterComponent>();
        for (auto emitter_entity : particle2d_view) {
            auto& emitter = particle2d_view.get<ParticleEmitterComponent>(emitter_entity);
            (void)emitter_entity;
            ++particle_emitters;
            active_particles += emitter.particles.size();
        }
        auto particle3d_view = runtime_context_.world->registry().view<dse::ParticleSystem3DComponent>();
        for (auto particle_entity : particle3d_view) {
            const auto& particle_system = particle3d_view.get<dse::ParticleSystem3DComponent>(particle_entity);
            (void)particle_entity;
            ++particle_emitters;
            const int active_particle_count = particle_system.active_particle_count;
            active_particles += static_cast<size_t>(active_particle_count > 0 ? active_particle_count : 0);
        }
        float avg_update_ms = update_samples_ > 0 ? update_time_accumulator_ms_ / static_cast<float>(update_samples_) : 0.0f;
        float avg_fixed_ms = fixed_samples_ > 0 ? fixed_time_accumulator_ms_ / static_cast<float>(fixed_samples_) : 0.0f;
        float avg_render_ms = render_samples_ > 0 ? render_time_accumulator_ms_ / static_cast<float>(render_samples_) : 0.0f;
        auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
        std::size_t pending_callbacks = asset_manager.PendingMainThreadCallbacks();
        std::size_t pending_callbacks_hwm = asset_manager.PendingMainThreadCallbacksHighWatermark();
        DEBUG_LOG_INFO("Runtime stats: entities={}, sprites={}, meshes={}, draw_calls={}, material_switches={}, shadow_passes={}, max_batch_sprites={}, render_passes={}, physics_bodies={}, particle_emitters={}, active_particles={}, avg_update_ms={}, avg_fixed_ms={}, avg_render_ms={}, pending_upload_callbacks={}, pending_upload_callbacks_hwm={}, upload_budget={}",
                       entity_count,
                       stats.sprite_count,
                       stats.mesh_count,
                       stats.draw_calls,
                       stats.material_switches,
                       stats.shadow_passes,
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
    render_graph_dag_.Reset();
    registered_passes_.clear();

    // ---- 填充 RenderPassContext ----
    render_pass_context_.world = runtime_context_.world;
    render_pass_context_.asset_manager = runtime_context_.asset_manager;
    render_pass_context_.rhi_device = runtime_context_.rhi_device.get();
    render_pass_context_.light_buffer = &light_buffer_;
    render_pass_context_.cluster_grid = &cluster_grid_;
    render_pass_context_.editor_mode = runtime_context_.editor_mode;

    render_pass_context_.pipeline_states.sprite    = render_resources_.sprite_pipeline_state;
    render_pass_context_.pipeline_states.mesh      = render_resources_.mesh_pipeline_state;
    render_pass_context_.pipeline_states.prez      = render_resources_.prez_pipeline_state;
    render_pass_context_.pipeline_states.shadow    = render_resources_.shadow_pipeline_state;
    render_pass_context_.pipeline_states.composite = render_resources_.composite_pipeline_state;
    render_pass_context_.pipeline_states.decal_blend = render_resources_.decal_blend_pipeline_state;
    render_pass_context_.pipeline_states.wboit_accum = render_resources_.wboit_accum_pipeline_state;
    render_pass_context_.pipeline_states.wboit_reveal = render_resources_.wboit_reveal_pipeline_state;

    render_pass_context_.render_targets.main     = render_resources_.main_render_target;
    render_pass_context_.render_targets.scene    = render_resources_.scene_render_target;
    render_pass_context_.render_targets.ui       = render_resources_.ui_render_target;
    render_pass_context_.render_targets.prez     = render_resources_.prez_render_target;
    for (int i = 0; i < CSM_CASCADES; ++i) {
        render_pass_context_.render_targets.shadow[i] = render_resources_.shadow_render_target[i];
    }
    for (int i = 0; i < 4; ++i) {
        render_pass_context_.render_targets.spot_shadow[i]  = render_resources_.spot_shadow_render_target[i];
        render_pass_context_.render_targets.point_shadow[i] = render_resources_.point_shadow_render_target[i];
    }
    render_pass_context_.render_targets.bloom_extract = render_resources_.pp_bloom_extract_rt;
    render_pass_context_.render_targets.bloom_mips    = render_resources_.pp_bloom_mip_rts;
    render_pass_context_.render_targets.ssao      = render_resources_.pp_ssao_rt;
    render_pass_context_.render_targets.ssao_blur = render_resources_.pp_ssao_blur_rt;
    render_pass_context_.render_targets.contact_shadow = render_resources_.pp_contact_shadow_rt;
    render_pass_context_.render_targets.fxaa      = render_resources_.pp_fxaa_rt;
    render_pass_context_.render_targets.taa       = render_resources_.pp_taa_rt;
    render_pass_context_.render_targets.dof       = render_resources_.pp_dof_rt;
    render_pass_context_.render_targets.ssr       = render_resources_.pp_ssr_rt;
    render_pass_context_.render_targets.motion_vector = render_resources_.pp_motion_vector_rt;
    render_pass_context_.render_targets.outline = render_resources_.pp_outline_rt;
    render_pass_context_.render_targets.fog    = render_resources_.pp_fog_rt;
    render_pass_context_.render_targets.wboit_accum = render_resources_.wboit_accum_rt;
    render_pass_context_.render_targets.wboit_reveal = render_resources_.wboit_reveal_rt;
    render_pass_context_.render_targets.gbuffer = render_resources_.gbuffer_rt;
    render_pass_context_.render_targets.deferred_lighting = render_resources_.deferred_lighting_rt;
    render_pass_context_.render_targets.lum_temp  = render_resources_.pp_lum_temp_rt;
    render_pass_context_.render_targets.lum_adapted[0] = render_resources_.pp_lum_adapted_rt[0];
    render_pass_context_.render_targets.lum_adapted[1] = render_resources_.pp_lum_adapted_rt[1];

    render_pass_context_.modules.clear();
    for (auto& mod : modules_) {
        if (mod.instance) {
            render_pass_context_.modules.push_back({mod.instance});
        }
    }
#ifdef DSE_ENABLE_3D
    if (builtin_gameplay3d_enabled_) {
        render_pass_context_.modules.push_back({&gameplay3d_module_});
    }
#endif

    render_pass_context_.render_2d_scene = [this](World& world, CommandBuffer& cmd) {
        gameplay2d_module_.OnRenderScene(world, cmd);
    };
    render_pass_context_.render_2d_ui = [this](World& world, CommandBuffer& cmd, int w, int h, const glm::mat4& clip) {
        gameplay2d_module_.OnRenderUI(world, cmd, w, h, clip);
    };
    render_pass_context_.render_meshes = [this](World& world, CommandBuffer& cmd) {
        mesh_render_system_.Render(world, cmd);
    };
    render_pass_context_.render_transparent_meshes = [this](World& world, CommandBuffer& cmd, int wboit_mode) {
        mesh_render_system_.RenderTransparent(world, cmd, wboit_mode);
    };

    // ---- 声明外部输出 ----
    auto main_color  = render_graph_dag_.DeclareResource("main_color");
    auto scene_color = render_graph_dag_.DeclareResource("scene_color");
    auto taa_color   = render_graph_dag_.DeclareResource("taa_color");
    auto dof_color   = render_graph_dag_.DeclareResource("dof_color");
    auto ssr_color   = render_graph_dag_.DeclareResource("ssr_color");
    auto mb_color    = render_graph_dag_.DeclareResource("motion_blur_color");
    auto gbuffer_color = render_graph_dag_.DeclareResource("gbuffer_color");
    auto gbuffer_depth = render_graph_dag_.DeclareResource("gbuffer_depth");
    auto deferred_lit  = render_graph_dag_.DeclareResource("deferred_lit_color");
    auto outline_color = render_graph_dag_.DeclareResource("outline_color");
    render_graph_dag_.MarkOutput(main_color);
    render_graph_dag_.MarkOutput(scene_color);
    render_graph_dag_.MarkOutput(taa_color);
    render_graph_dag_.MarkOutput(dof_color);
    render_graph_dag_.MarkOutput(ssr_color);
    render_graph_dag_.MarkOutput(mb_color);
    render_graph_dag_.MarkOutput(gbuffer_color);
    render_graph_dag_.MarkOutput(deferred_lit);
    render_graph_dag_.MarkOutput(outline_color);

    // ---- 注册内置 Pass ----
    registered_passes_.push_back(std::make_unique<dse::render::PreZPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::CSMShadowPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::SpotShadowPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::PointShadowPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::GBufferPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::DeferredLightingPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::ForwardScenePass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::WBOITPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::WaterPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::BloomPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::SSAOPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::ContactShadowPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::AutoExposurePass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::MotionVectorPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::SSRPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::OutlinePass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::LightShaftPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::VolumetricFogPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::DecalPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::UIPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::CompositePass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::DOFPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::MotionBlurPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::FXAAPass>(render_pass_context_));
    {
        auto taa = std::make_unique<dse::render::TAAPass>(render_pass_context_);
        taa_pass_ = taa.get();
        registered_passes_.push_back(std::move(taa));
    }
    if (!runtime_context_.editor_mode) {
        registered_passes_.push_back(std::make_unique<dse::render::PresentPass>(render_pass_context_));
    }

    // ---- 模块动态注册自定义 Pass ----
    for (auto& mod : modules_) {
        if (mod.instance) {
            mod.instance->RegisterRenderPasses(render_graph_dag_, render_pass_context_, registered_passes_);
        }
    }
#ifdef DSE_ENABLE_3D
    if (builtin_gameplay3d_enabled_) {
        gameplay3d_module_.RegisterRenderPasses(render_graph_dag_, render_pass_context_, registered_passes_);
    }
#endif

    // ---- 所有 Pass 在 RenderGraph 上声明依赖 ----
    for (auto& pass : registered_passes_) {
        pass->Setup(render_graph_dag_);
    }

    // 编译 DAG（拓扑排序 + 无用 Pass 剔除）
    if (!render_graph_dag_.Compile()) {
        DEBUG_LOG_ERROR("RenderGraph 编译失败：检测到循环依赖");
    }
}


void FramePipeline::ExecuteRenderGraph(CommandBuffer& cmd_buffer) {
    dse::runtime::ExecuteFrameRenderGraph(*this, cmd_buffer);
}

void FramePipeline::ExecuteRenderGraphInternal(CommandBuffer& cmd_buffer) {
    // 渲染 Pass 的 Execute() 内部通过 registry.view<>() 访问 ECS，
    // 而 EnTT registry 非线程安全（assure() 可能触发 pools_ 重新分配）。
    // 在实现 Pass 数据隔离（预缓存 view 结果）之前，使用串行执行保证正确性。
    render_graph_dag_.Execute(cmd_buffer);
}

void FramePipeline::EnableEditorMode(bool enable) {
    if (!initialized_) {
        runtime_context_.editor_mode = enable;
    }
}

void FramePipeline::SetNativeWindowHandle(void* handle) {
    runtime_context_.native_window_handle = handle;
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

int FramePipeline::LastMaterialSwitches() const {
    return last_material_switches_;
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

std::vector<unsigned char> FramePipeline::ReadSceneColorRgba8() const {
    if (!runtime_context_.rhi_device || render_resources_.scene_render_target == 0) {
        return {};
    }
    return runtime_context_.rhi_device->ReadRenderTargetColorRgba8(render_resources_.scene_render_target);
}

RenderTargetReadback FramePipeline::ReadSceneColorRgba8WithSize() const {
    if (!runtime_context_.rhi_device || render_resources_.scene_render_target == 0) {
        return {};
    }
    return runtime_context_.rhi_device->ReadRenderTargetColorRgba8WithSize(render_resources_.scene_render_target);
}

std::vector<unsigned char> FramePipeline::ReadMainColorRgba8() const {
    if (!runtime_context_.rhi_device || render_resources_.main_render_target == 0) {
        return {};
    }
    return runtime_context_.rhi_device->ReadRenderTargetColorRgba8(render_resources_.main_render_target);
}

RenderTargetReadback FramePipeline::ReadMainColorRgba8WithSize() const {
    if (!runtime_context_.rhi_device || render_resources_.main_render_target == 0) {
        return {};
    }
    return runtime_context_.rhi_device->ReadRenderTargetColorRgba8WithSize(render_resources_.main_render_target);
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

void FramePipeline::SetEditorCamera(const glm::mat4& view, const glm::mat4& projection) {
    render_pass_context_.use_editor_camera = true;
    render_pass_context_.editor_view = view;
    render_pass_context_.editor_projection = projection;
}

void FramePipeline::DisableEditorCamera() {
    render_pass_context_.use_editor_camera = false;
}

void FramePipeline::SetAssetManager(AssetManager* asset_manager) {
    if (initialized_) {
        return;
    }
    runtime_context_.asset_manager = asset_manager;
}
