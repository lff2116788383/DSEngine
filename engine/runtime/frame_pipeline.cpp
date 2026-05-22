/**
 * @file frame_pipeline.cpp
 * @brief 引擎主循环与帧流水线，协调更新、物理和渲染的执行顺序
 */

#include "engine/runtime/frame_pipeline.h"

FramePipeline::FramePipeline() : modules_impl_(CreateBuiltinModules()) {}
FramePipeline::~FramePipeline() = default;

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
#include "engine/ecs/camera.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/components_3d_fluid.h"
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
#include <cstring>

#ifdef DSE_ENABLE_3D
#if defined(DSE_ENABLE_JOLT)
#include "engine/physics/physics3d/physics3d_system_jolt.h"
#elif defined(DSE_ENABLE_PHYSX)
#include "engine/physics/physics3d/physics3d_system.h"
#endif
namespace {
std::shared_ptr<dse::physics3d::IPhysics3DSystem> CreatePhysics3DSystem() {
#if defined(DSE_ENABLE_JOLT) || defined(DSE_ENABLE_PHYSX)
    return std::make_shared<dse::physics3d::Physics3DSystem>();
#else
    return nullptr;
#endif
}
} // anonymous namespace
#endif // DSE_ENABLE_3D

namespace dse::render {
    extern const char* kHiZCopyShaderSource;
    extern const char* kHiZDownsampleShaderSource;
    extern const char* kHiZCullShaderSource;
    extern const char* kHiZCopyShaderSourceVK;
    extern const char* kHiZDownsampleShaderSourceVK;
    extern const char* kHiZCullShaderSourceVK;
    extern const char* kHiZCopyShaderSourceHLSL;
    extern const char* kHiZDownsampleShaderSourceHLSL;
    extern const char* kHiZCullShaderSourceHLSL;
    extern const char* kGPUCullShaderSource;
    extern const char* kGPUCullShaderSourceVK;
    extern const char* kGPUCullShaderSourceHLSL;
}

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
    // 计算黑色区域的边界矩形
    int black_min_x = readback.width, black_max_x = -1;
    int black_min_y = readback.height, black_max_y = -1;
    int black_count = 0;
    for (int y = 0; y < readback.height; ++y) {
        for (int x = 0; x < readback.width; ++x) {
            const std::size_t idx = (static_cast<std::size_t>(y) * readback.width + x) * 4;
            const unsigned int rgb = static_cast<unsigned int>(readback.pixels[idx]) +
                                     static_cast<unsigned int>(readback.pixels[idx+1]) +
                                     static_cast<unsigned int>(readback.pixels[idx+2]);
            if (rgb <= 8u) {
                ++black_count;
                if (x < black_min_x) black_min_x = x;
                if (x > black_max_x) black_max_x = x;
                if (y < black_min_y) black_min_y = y;
                if (y > black_max_y) black_max_y = y;
            }
        }
    }
    // 9 点网格采样
    std::string grid;
    const int W = readback.width, H = readback.height;
    struct Pt { int x, y; const char* tag; };
    const Pt pts[] = {
        {10, 10, "BL"}, {W/2, 10, "BC"}, {W-11, 10, "BR"},
        {10, H/2, "ML"}, {W/2, H/2, "MC"}, {W-11, H/2, "MR"},
        {10, H-11, "TL"}, {W/2, H-11, "TC"}, {W-11, H-11, "TR"},
    };
    std::ostringstream oss;
    for (auto& p : pts) {
        if (p.x < 0 || p.x >= W || p.y < 0 || p.y >= H) continue;
        const std::size_t idx = (static_cast<std::size_t>(p.y) * W + p.x) * 4;
        oss << " " << p.tag << "=(" << static_cast<int>(readback.pixels[idx])
            << "," << static_cast<int>(readback.pixels[idx+1])
            << "," << static_cast<int>(readback.pixels[idx+2]) << ")";
    }
    grid = oss.str();
    DEBUG_LOG_INFO("Render readback {}: size={}x{} pixels={} non_black={} max_rgb={} avg_rgb={} black_count={} black_bbox=[({},{})..({},{})]{}",
                   label, stats.width, stats.height, stats.pixels,
                   stats.non_black, stats.max_rgb, stats.avg_rgb,
                   black_count,
                   black_min_x, black_min_y, black_max_x, black_max_y,
                   grid);
}

void LogDefaultFramebufferStats() {
    if (!glGetIntegerv) return;  // 非 OpenGL 后端无 GL 函数
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

bool FramePipeline::Init() {
    if (initialized_) {
        return true;
    }
    if (!runtime_context_.world) {
        DEBUG_LOG_ERROR("FramePipeline init failed: world is not injected");
        return false;
    }
    auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
    auto t0 = std::chrono::steady_clock::now();
    auto lap = [&t0](const char* label) {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
        DEBUG_LOG_INFO("[InitTiming] {}: {}ms", label, ms);
        t0 = now;
    };
    const auto rhi_backend = dse::render::ValidateRhiBackend(dse::render::ResolveRhiBackendFromEnv());
    runtime_context_.rhi_device = dse::render::CreateRhiDevice(rhi_backend);
    DEBUG_LOG_INFO("FramePipeline RHI 后端: {}", dse::render::RhiBackendToString(rhi_backend));

    // D3D11 / Vulkan 后端需要用平台窗口句柄完成设备初始化
    runtime_context_.rhi_device->SetInitKeepAlive(init_keep_alive_);
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
        // 立即 present 一帧黑屏，消除窗口创建后到首帧渲染前的白屏
        runtime_context_.rhi_device->BeginFrame();
        runtime_context_.rhi_device->EndFrame();
    }

    lap("RHI device init");
    KeepAlive();
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
    asset_manager.StartFileWatcher();
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

    // RSM MRT: 3 color (position + normal + flux) + depth, 512x512
    if (render_resources_.rsm_render_target == 0) {
        RenderTargetDesc rsm_desc;
        rsm_desc.width = 512;
        rsm_desc.height = 512;
        rsm_desc.has_color = true;
        rsm_desc.has_depth = true;
        rsm_desc.color_attachment_count = 3;
        render_resources_.rsm_render_target = runtime_context_.rhi_device->CreateRenderTarget(rsm_desc);
    }

    if (render_resources_.pp_bloom_extract_rt == 0) {
        render_resources_.pp_bloom_extract_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    }
    
    // Create mip chain for Dual Filter Bloom (5 levels)
    // allow_uav=true：允许 CS 写入，D3D11 Bloom Compute Shader 路径使用
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
    // WBOIT revealage: 全分辨率，R 通道存储 prod(1-alpha_i)
    if (render_resources_.wboit_reveal_rt == 0) {
        render_resources_.wboit_reveal_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }

    // GBuffer MRT: 3 棰滆壊闄勪欢 (albedo, normal, position) + 娣卞害
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

    // Hi-Z Occlusion Culling: R32F 绾圭悊 + 瀹屾暣 mip chain
    if (render_resources_.hiz_texture == 0 && runtime_context_.rhi_device->SupportsCompute()) {
        render_resources_.hiz_texture = runtime_context_.rhi_device->CreateHiZTexture(render_width, render_height);
        if (render_resources_.hiz_texture != 0) {
            const size_t cap = dse::runtime::RenderPipelineResources::kHiZMaxObjects;
            {
                dse::render::GpuBufferDesc d{cap * sizeof(uint32_t), dse::render::GpuBufferUsage::kStorage, true, "hiz_visibility"};
                render_resources_.hiz_visibility_ssbo = runtime_context_.rhi_device->CreateGpuBuffer(d, nullptr);
            }
            {
                dse::render::GpuBufferDesc d{cap * 8 * sizeof(float), dse::render::GpuBufferUsage::kStorage, true, "hiz_aabb"};
                render_resources_.hiz_aabb_ssbo = runtime_context_.rhi_device->CreateGpuBuffer(d, nullptr);
            }
            render_resources_.hiz_ssbo_capacity = cap;

            // 创建 compute shader（三套源码 + binding 元数据，仅一次，缓存句柄）
            // HiZ Copy:       ssbo=0, img=1, smp=1, pc=8B  (ivec2 dst_size)
            // HiZ Downsample: ssbo=0, img=2, smp=0, pc=16B (ivec2 src_size + ivec2 dst_size)
            // HiZ Cull:       ssbo=2, img=0, smp=1, pc=80B (mat4 vp + vec2 screen + 2*int)
            render_resources_.hiz_copy_shader = runtime_context_.rhi_device->CreateComputeShaderEx(
                dse::render::kHiZCopyShaderSource,
                dse::render::kHiZCopyShaderSourceVK,
                dse::render::kHiZCopyShaderSourceHLSL,
                0, 1, 1, 8);
            render_resources_.hiz_downsample_shader = runtime_context_.rhi_device->CreateComputeShaderEx(
                dse::render::kHiZDownsampleShaderSource,
                dse::render::kHiZDownsampleShaderSourceVK,
                dse::render::kHiZDownsampleShaderSourceHLSL,
                0, 2, 0, 16);
            render_resources_.hiz_cull_shader = runtime_context_.rhi_device->CreateComputeShaderEx(
                dse::render::kHiZCullShaderSource,
                dse::render::kHiZCullShaderSourceVK,
                dse::render::kHiZCullShaderSourceHLSL,
                2, 0, 1, 80);

            DEBUG_LOG_INFO("Hi-Z Occlusion Culling initialized: texture={} vis_ssbo={} aabb_ssbo={} capacity={} shaders=({},{},{})",
                           render_resources_.hiz_texture,
                           render_resources_.hiz_visibility_ssbo.raw(),
                           render_resources_.hiz_aabb_ssbo.raw(),
                           cap,
                           render_resources_.hiz_copy_shader,
                           render_resources_.hiz_downsample_shader,
                           render_resources_.hiz_cull_shader);
        }
    }

    // GPU Driven Rendering 能力检测
    const char* disable_gpu_driven_env = std::getenv("DSE_DISABLE_GPU_DRIVEN");
    const bool gpu_driven_disabled = disable_gpu_driven_env && disable_gpu_driven_env[0] != '\0' && disable_gpu_driven_env[0] != '0';
    if (!gpu_driven_disabled &&
        runtime_context_.rhi_device->SupportsCompute() &&
        runtime_context_.rhi_device->SupportsIndirectDraw() &&
        runtime_context_.rhi_device->SupportsSSBO()) {
        render_resources_.gpu_cull_shader = runtime_context_.rhi_device->CreateComputeShaderEx(
            dse::render::kGPUCullShaderSource,
            dse::render::kGPUCullShaderSourceVK,
            dse::render::kGPUCullShaderSourceHLSL,
            2, 0, 1, 176);
        render_resources_.gpu_driven_supported = (render_resources_.gpu_cull_shader != 0);
        DEBUG_LOG_INFO("GPU Driven Rendering: supported, cull_shader={}", render_resources_.gpu_cull_shader);
    } else if (gpu_driven_disabled) {
        render_resources_.gpu_driven_supported = false;
        DEBUG_LOG_INFO("GPU Driven Rendering: disabled by DSE_DISABLE_GPU_DRIVEN");
    } else {
        render_resources_.gpu_driven_supported = false;
        DEBUG_LOG_INFO("GPU Driven Rendering: not supported, using CPU path");
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

    lap("render resources");
    KeepAlive();
    DEBUG_LOG_INFO("FramePipeline init: systems init begin");
    modules_impl_->InitGameplay2D(*runtime_context_.world, runtime_context_.rhi_device.get(), &asset_manager);
    modules_impl_->InitMeshSystem(&asset_manager);
    DEBUG_LOG_INFO("FramePipeline init: systems init complete");

    const auto runtime_modules = ResolveRuntimeModules();
    const bool enable_gameplay3d = runtime_modules.empty() ||
        std::find(runtime_modules.begin(), runtime_modules.end(), "Gameplay3D") != runtime_modules.end();
    DEBUG_LOG_INFO("FramePipeline init: Gameplay3D module enabled={}", enable_gameplay3d);
#ifdef DSE_ENABLE_3D
    {
        auto sys = CreatePhysics3DSystem();
        if (sys && sys->Init(*runtime_context_.world)) {
            physics3d_system_ = std::move(sys);
            dse::core::ServiceLocator::Instance().Register<dse::physics3d::IPhysics3DSystem,
                dse::physics3d::IPhysics3DSystem>(physics3d_system_);
            DEBUG_LOG_INFO("FramePipeline init: Physics3DSystem initialized and registered");
        } else {
            DEBUG_LOG_WARN("FramePipeline init: Physics3DSystem init failed, 3D physics will be unavailable");
        }
    }
#endif

#ifdef DSE_ENABLE_NAVMESH
    if (nav_mesh_system_.Init()) {
        dse::core::ServiceLocator::Instance().Register<dse::navigation::NavMeshSystem, dse::navigation::NavMeshSystem>(
            std::shared_ptr<dse::navigation::NavMeshSystem>(&nav_mesh_system_, [](auto*) {}));
        nav_mesh_system_initialized_ = true;
        DEBUG_LOG_INFO("FramePipeline init: NavMeshSystem initialized and registered");
    }
#endif

#ifdef DSE_ENABLE_3D
    if (enable_gameplay3d) {
        if (!modules_impl_->InitGameplay3D(*runtime_context_.world, runtime_context_.rhi_device.get(), &asset_manager)) {
            DEBUG_LOG_ERROR("FramePipeline init failed: Gameplay3D module OnInit returned false");
        } else {
            DEBUG_LOG_INFO("FramePipeline init: Gameplay3D module OnInit OK (static)");
            builtin_gameplay3d_enabled_ = true;
        }
    }
#else
    if (enable_gameplay3d) {
        DEBUG_LOG_INFO("FramePipeline init: Gameplay3D built-in fallback enabled (particle/steering/animator)");
        modules_impl_->InitFallback3D(*runtime_context_.world, runtime_context_.rhi_device.get(), &asset_manager);
        builtin_gameplay3d_enabled_ = true;
    }
#endif

    lap("systems init");
    KeepAlive();
    DEBUG_LOG_INFO("FramePipeline init: business bootstrap begin");

    runtime_context_.audio_system = &modules_impl_->GetAudioSystem();

    const bool business_bootstrap_ok = dse::runtime::BootstrapBusinessRuntime(runtime_context_, {
        [this]() { return LastDrawCalls(); },
        [this]() { return LastMaxBatchSprites(); },
        [this]() { return LastSpriteCount(); }
    });
    DEBUG_LOG_INFO("{} business mode bootstrap: {}",
                   runtime_context_.business_mode == BusinessMode::Lua ? "Lua" : "Cpp",
                   business_bootstrap_ok ? "OK" : "FAILED");
    if (!business_bootstrap_ok) {
        if (!runtime_context_.editor_mode) {
            Shutdown();
            return false;
        }
        DEBUG_LOG_INFO("Editor mode: business bootstrap failed, continuing with empty scene");
    }
    lap("business bootstrap");
    KeepAlive();
    BuildRenderGraph();
    lap("BuildRenderGraph");

    // Clustered Forward+ 光源 SSBO + Cluster 网格初始化
    light_buffer_.Init(runtime_context_.rhi_device.get());
    cluster_grid_.Init(runtime_context_.rhi_device.get());
    lap("light buffer + cluster grid");

    // Light Probe SH Bake 系统初始化
    light_probe_system_.Init(runtime_context_.rhi_device.get());
    lap("LightProbeSystem");

    // Reflection Probe + IBL 系统初始化（生成 BRDF LUT）
    reflection_probe_system_.Init(runtime_context_.rhi_device.get());
    lap("ReflectionProbeSystem");

    // DDGI 系统延迟初始化（首帧检测 GIProbeVolumeComponent 后按需初始化）

    // 资源流式加载管理器初始化
    streaming_manager_.Init(&asset_manager);
    {
        auto streaming_shared = std::shared_ptr<dse::streaming::StreamingManager>(&streaming_manager_, [](auto*) {});
        dse::core::ServiceLocator::Instance().Register<dse::streaming::StreamingManager, dse::streaming::StreamingManager>(streaming_shared);
    }
    lap("StreamingManager");
    initialized_ = true;
    if (auto* event_bus = dse::core::ServiceLocator::Instance().Get<dse::core::EventBus>()) {
        event_bus->Publish<dse::core::SceneLifecycleEvent>(dse::core::SceneLifecyclePhase::Init);
    }

    // Phase 2: 渲染线程分离（DSE_RENDER_THREAD=1 启用，编辑器模式下禁用）
    if (!runtime_context_.editor_mode) {
        if (const char* env = std::getenv("DSE_RENDER_THREAD")) {
            if (env[0] == '1') {
                StartRenderThread();
            }
        }
    }
    return true;
}

void FramePipeline::Shutdown() {
    if (!initialized_) {
        return;
    }
    StopRenderThread();
    if (auto* event_bus = dse::core::ServiceLocator::Instance().Get<dse::core::EventBus>()) {
        event_bus->Publish<dse::core::SceneLifecycleEvent>(dse::core::SceneLifecyclePhase::Shutdown);
    }
    // 资源流式加载管理器关闭
    streaming_manager_.Shutdown();
    dse::core::ServiceLocator::Instance().Reset<dse::streaming::StreamingManager>();

    auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
    render_graph_dag_.Reset();
    dse::runtime::ShutdownBusinessRuntime(runtime_context_);
    modules_impl_->ShutdownGameplay2D(*runtime_context_.world);
#ifdef DSE_ENABLE_3D
    if (builtin_gameplay3d_enabled_) {
        modules_impl_->ShutdownGameplay3D(*runtime_context_.world);
        builtin_gameplay3d_enabled_ = false;
    }
#else
    if (builtin_gameplay3d_enabled_) {
        modules_impl_->ShutdownFallback3D(*runtime_context_.world);
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

    modules_impl_->ShutdownMeshSystem();
#ifdef DSE_ENABLE_3D
    if (physics3d_system_) {
        dse::core::ServiceLocator::Instance().Reset<dse::physics3d::IPhysics3DSystem>();
        physics3d_system_->Shutdown();
        physics3d_system_.reset();
    }
#endif
#ifdef DSE_ENABLE_NAVMESH
    if (nav_mesh_system_initialized_) {
        dse::core::ServiceLocator::Instance().Reset<dse::navigation::NavMeshSystem>();
        nav_mesh_system_.Shutdown();
        nav_mesh_system_initialized_ = false;
    }
#endif
    cluster_grid_.Shutdown();
    light_buffer_.Shutdown();
    light_probe_system_.Shutdown();
    reflection_probe_system_.Shutdown(runtime_context_.rhi_device.get());
    ddgi_system_.Shutdown(runtime_context_.rhi_device.get());
    taa_pass_ = nullptr;

    asset_manager.ReleaseGpuResources();

    // Hi-Z: 释放 GPU 资源
    if (runtime_context_.rhi_device) {
        if (render_resources_.hiz_copy_shader != 0) {
            runtime_context_.rhi_device->DeleteComputeShader(render_resources_.hiz_copy_shader);
            render_resources_.hiz_copy_shader = 0;
        }
        if (render_resources_.hiz_downsample_shader != 0) {
            runtime_context_.rhi_device->DeleteComputeShader(render_resources_.hiz_downsample_shader);
            render_resources_.hiz_downsample_shader = 0;
        }
        if (render_resources_.hiz_cull_shader != 0) {
            runtime_context_.rhi_device->DeleteComputeShader(render_resources_.hiz_cull_shader);
            render_resources_.hiz_cull_shader = 0;
        }
        if (render_resources_.hiz_texture != 0) {
            runtime_context_.rhi_device->DeleteHiZTexture(render_resources_.hiz_texture);
            render_resources_.hiz_texture = 0;
        }
        if (render_resources_.hiz_visibility_ssbo) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.hiz_visibility_ssbo);
            render_resources_.hiz_visibility_ssbo = {};
        }
        if (render_resources_.hiz_aabb_ssbo) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.hiz_aabb_ssbo);
            render_resources_.hiz_aabb_ssbo = {};
        }
    }

    // GPU Driven 资源清理
    if (runtime_context_.rhi_device) {
        modules_impl_->CleanupGPUResources(runtime_context_.rhi_device.get());
        if (render_resources_.gpu_draw_cmd_ssbo) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.gpu_draw_cmd_ssbo);
            render_resources_.gpu_draw_cmd_ssbo = {};
        }
        if (render_resources_.gpu_instance_ssbo) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.gpu_instance_ssbo);
            render_resources_.gpu_instance_ssbo = {};
        }
        if (render_resources_.gpu_indirect_buffer) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.gpu_indirect_buffer);
            render_resources_.gpu_indirect_buffer = {};
        }
    }

    if (runtime_context_.rhi_device) {
        runtime_context_.rhi_device->Shutdown();
        runtime_context_.rhi_device.reset();
    }
    asset_manager.StopFileWatcher();
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

void FramePipeline::FlushPhysicsEvents() {
#ifdef DSE_ENABLE_3D
    if (physics3d_system_) {
        physics3d_system_->FlushEvents();
    }
#endif
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
    if (render_thread_active_.load()) {
        WaitForRenderComplete();
        PrepareRenderFrame();
        SignalRenderThread();
    } else {
        dse::runtime::RunFrameRender(*this);
    }
}

void FramePipeline::RunUpdateInternal(float delta_time) {
    auto update_begin = std::chrono::high_resolution_clock::now();
    auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
    asset_manager.PumpMainThreadCallbacks(callback_budget_per_frame_);
    asset_manager.PumpHotReloads();

    // 资源流式加载：获取摄像机位置并 tick
    if (runtime_context_.world) {
        glm::vec3 streaming_cam_pos(0.0f);
        auto streaming_cam_view = runtime_context_.world->registry().view<TransformComponent, dse::Camera3DComponent>();
        for (auto e : streaming_cam_view) {
            auto& cam = streaming_cam_view.get<dse::Camera3DComponent>(e);
            if (cam.enabled) {
                streaming_cam_pos = streaming_cam_view.get<TransformComponent>(e).position;
                break;
            }
        }
        streaming_manager_.Tick(streaming_cam_pos);
    }

    dse::runtime::TickBusinessRuntime(runtime_context_, delta_time);

    dse::runtime::RunRuntimeUpdateGraph(*this, delta_time);
#ifndef DSE_ENABLE_3D
    if (builtin_gameplay3d_enabled_) {
        modules_impl_->UpdateFallback3D(*runtime_context_.world, delta_time);
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

    // TAA: 预检测 ECS 组件，提前设置 taa_active，ForwardScenePass 需要在场景渲染前知道是否应用 jitter
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

    // DDGI: 检测 GIProbeVolumeComponent，按需初始化/更新系统
    render_pass_context_.ddgi_active = false;
    render_pass_context_.ddgi_system = nullptr;
    if (runtime_context_.world && runtime_context_.rhi_device->SupportsCompute()) {
        auto gi_view = runtime_context_.world->registry().view<dse::GIProbeVolumeComponent>();
        for (auto entity : gi_view) {
            auto& gi = gi_view.get<dse::GIProbeVolumeComponent>(entity);
            if (!gi.enabled) continue;

            // 按需初始化或重配置
            if (gi.needs_reinit_ || !ddgi_system_.IsInitialized()) {
                dse::render::gi::DDGIVolumeConfig cfg;
                cfg.origin = gi.origin;
                cfg.extent = gi.extent;
                cfg.resolution = glm::ivec3(gi.resolution_x, gi.resolution_y, gi.resolution_z);
                cfg.irradiance_texels = gi.irradiance_texels;
                cfg.visibility_texels = gi.visibility_texels;
                cfg.rays_per_probe = gi.rays_per_probe;
                cfg.hysteresis = gi.hysteresis;
                if (ddgi_system_.IsInitialized()) {
                    ddgi_system_.Reconfigure(runtime_context_.rhi_device.get(), cfg);
                } else {
                    ddgi_system_.Init(runtime_context_.rhi_device.get(), cfg);
                }
                gi.needs_reinit_ = false;
            }

            if (ddgi_system_.IsInitialized()) {
                render_pass_context_.ddgi_system = &ddgi_system_;
                render_pass_context_.ddgi_active = true;
                render_pass_context_.ddgi_gi_intensity = gi.gi_intensity;
                render_pass_context_.ddgi_normal_bias = gi.normal_bias;
                const auto& res = ddgi_system_.GetResources();
                render_pass_context_.ddgi_irradiance_atlas = res.irradiance_atlas;
                render_pass_context_.ddgi_visibility_atlas = res.visibility_atlas;
            }
            break;  // 仅支持单个 GI Volume
        }
    }
    // 同步 DDGI 状态到 RHI 全局渲染状态
    if (render_pass_context_.ddgi_active) {
        const auto& cfg = ddgi_system_.GetConfig();
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

    // Hi-Z: 上传上一帧收集的 AABB 到 GPU SSBO（供 HiZCullPass 使用）
    if (render_resources_.hiz_aabb_ssbo && render_resources_.hiz_visibility_ssbo) {
        const auto& aabbs = modules_impl_->CachedAABBs();
        const int count = modules_impl_->CachedAABBCount();
        if (count > 0 && static_cast<size_t>(count) <= render_resources_.hiz_ssbo_capacity) {
            runtime_context_.rhi_device->UpdateGpuBuffer(
                render_resources_.hiz_aabb_ssbo, 0,
                count * sizeof(dse::gameplay3d::HiZAABB),
                aabbs.data());
            render_pass_context_.hiz_object_count = count;
        } else {
            render_pass_context_.hiz_object_count = 0;
        }
    }

    // GPU Driven: 准备 GPU 场景数据（AABB + DrawCommands + Instance 数据）
    if (render_resources_.gpu_driven_supported) {
        modules_impl_->PrepareGPUScene(*runtime_context_.world, render_pass_context_);
        // 同步动态创建的句柄到 render_resources_，确保下帧不被覆盖
        render_resources_.gpu_draw_cmd_ssbo = render_pass_context_.gpu_draw_cmd_ssbo;
        render_resources_.gpu_instance_ssbo = render_pass_context_.gpu_instance_ssbo;
    }

    CaptureThinSnapshot();
    FlipSnapshotIndex();
    render_pass_context_.snapshot = &read_snapshot();

    ExecuteRenderGraph(*cmd_buffer);
    
    dse::runtime::SubmitAndEndRuntimeRenderFrame(*this, std::move(cmd_buffer));

    // Hi-Z / GPU Driven: GPU 执行完毕后，读回可见性供下一帧 MeshRenderSystem 使用
    if (render_pass_context_.gpu_driven_enabled && render_pass_context_.gpu_indirect_draw_count > 0
        && render_pass_context_.gpu_draw_cmd_ssbo) {
        // GPU Driven 路径：从 draw commands SSBO 读回 instance_count 作为可见性
        const int count = render_pass_context_.gpu_indirect_draw_count;
        std::vector<DrawElementsIndirectCommand> cmds(count);
        runtime_context_.rhi_device->ReadGpuBuffer(
            render_pass_context_.gpu_draw_cmd_ssbo, 0,
            count * sizeof(DrawElementsIndirectCommand), cmds.data());
        std::vector<uint32_t> visibility(count);
        int culled = 0;
        for (int i = 0; i < count; ++i) {
            visibility[i] = cmds[i].instance_count > 0 ? 1u : 0u;
            if (visibility[i] == 0) ++culled;
        }
        modules_impl_->SetHiZVisibility(visibility);
        gpu_culled_last_frame_ = culled;
        runtime_context_.rhi_device->PatchLastFrameGPUCulledCount(culled);
    } else if (render_resources_.hiz_visibility_ssbo && render_pass_context_.hiz_object_count > 0
        && render_pass_context_.hiz_culling_enabled) {
        // 传统 Hi-Z 路径：从 visibility SSBO 读回
        const int count = render_pass_context_.hiz_object_count;
        std::vector<uint32_t> visibility(count, 1);
        runtime_context_.rhi_device->ReadGpuBuffer(
            render_resources_.hiz_visibility_ssbo, 0,
            count * sizeof(uint32_t), visibility.data());
        modules_impl_->SetHiZVisibility(visibility);
    }

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
#if defined(DSE_ENABLE_3D) && (defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT))
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
    if (render_resources_.rsm_render_target != 0) {
        render_pass_context_.rsm_render_target = render_resources_.rsm_render_target;
        render_pass_context_.rsm_targets.position = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.rsm_render_target, 0);
        render_pass_context_.rsm_targets.normal   = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.rsm_render_target, 1);
        render_pass_context_.rsm_targets.flux     = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.rsm_render_target, 2);
        render_pass_context_.rsm_targets.width = 512;
        render_pass_context_.rsm_targets.height = 512;
    }
    render_pass_context_.render_targets.lum_temp  = render_resources_.pp_lum_temp_rt;
    render_pass_context_.render_targets.lum_adapted[0] = render_resources_.pp_lum_adapted_rt[0];
    render_pass_context_.render_targets.lum_adapted[1] = render_resources_.pp_lum_adapted_rt[1];
    render_pass_context_.render_targets.hiz_texture = render_resources_.hiz_texture;
    render_pass_context_.hiz_visibility_ssbo = render_resources_.hiz_visibility_ssbo;
    render_pass_context_.hiz_aabb_ssbo = render_resources_.hiz_aabb_ssbo;
    render_pass_context_.hiz_culling_enabled = false;
    render_pass_context_.hiz_object_count = 0;
    render_pass_context_.hiz_copy_shader = render_resources_.hiz_copy_shader;
    render_pass_context_.hiz_downsample_shader = render_resources_.hiz_downsample_shader;
    render_pass_context_.hiz_cull_shader = render_resources_.hiz_cull_shader;

    // GPU Driven 鐘舵€?
    render_pass_context_.gpu_driven_enabled = render_resources_.gpu_driven_supported;
    render_pass_context_.gpu_indirect_buffer = render_resources_.gpu_indirect_buffer;
    render_pass_context_.gpu_instance_ssbo = render_resources_.gpu_instance_ssbo;
    render_pass_context_.gpu_material_ssbo = render_resources_.gpu_material_ssbo;
    render_pass_context_.gpu_draw_cmd_ssbo = render_resources_.gpu_draw_cmd_ssbo;
    render_pass_context_.gpu_visible_indices_ssbo = render_resources_.gpu_visible_indices_ssbo;
    render_pass_context_.gpu_atomic_counter_ssbo = render_resources_.gpu_atomic_counter_ssbo;
    render_pass_context_.gpu_mega_vao = render_resources_.gpu_mega_vao;
    render_pass_context_.gpu_cull_shader = render_resources_.gpu_cull_shader;
    render_pass_context_.gpu_indirect_draw_count = 0;
    render_pass_context_.gpu_total_instances = 0;

    render_pass_context_.modules.clear();
    for (auto& mod : modules_) {
        if (mod.instance) {
            render_pass_context_.modules.push_back({mod.instance});
        }
    }
#ifdef DSE_ENABLE_3D
    if (builtin_gameplay3d_enabled_) {
        render_pass_context_.modules.push_back({modules_impl_->GetGameplay3DModule()});
    }
#endif

    render_pass_context_.render_2d_scene = [this](World& world, CommandBuffer& cmd) {
        modules_impl_->RenderScene2D(world, cmd);
    };
    render_pass_context_.render_2d_ui = [this](World& world, CommandBuffer& cmd, int w, int h, const glm::mat4& clip) {
        modules_impl_->RenderUI2D(world, cmd, w, h, clip);
    };
    render_pass_context_.render_meshes = [this](World& world, CommandBuffer& cmd) {
        modules_impl_->RenderMeshes(world, cmd);
    };
    render_pass_context_.render_transparent_meshes = [this](World& world, CommandBuffer& cmd, int wboit_mode) {
        modules_impl_->RenderTransparentMeshes(world, cmd, wboit_mode);
    };

    // ---- 澹版槑澶栭儴杈撳嚭 ----
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

    // ---- 娉ㄥ唽鍐呯疆 Pass ----
    registered_passes_.push_back(std::make_unique<dse::render::PreZPass>(render_pass_context_));
    // Hi-Z passes 鈥?绱ц窡 PreZ 涔嬪悗锛屽湪 Forward 涔嬪墠
    if (render_resources_.hiz_texture != 0) {
        registered_passes_.push_back(std::make_unique<dse::render::HiZBuildPass>(render_pass_context_));
        registered_passes_.push_back(std::make_unique<dse::render::HiZCullPass>(render_pass_context_));
    }
    // Shadow passes 必须在 GPUCullPass 之前执行：
    // GPUCullPass 会修改 draw commands 的 instance_count（基于主摄像机视锥剔除），
    // 而阴影 pass 需要从光源视角渲染所有物体（使用未剔除的原始 draw commands）。
    registered_passes_.push_back(std::make_unique<dse::render::CSMShadowPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::SpotShadowPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::PointShadowPass>(render_pass_context_));
    // GPU Driven Cull Pass — 视锥 + Hi-Z 剔除，直接写 indirect draw commands
    if (render_resources_.gpu_driven_supported) {
        registered_passes_.push_back(std::make_unique<dse::render::GPUCullPass>(render_pass_context_));
    }
    // RSM 鈥?浠庢柟鍚戝厜瑙嗚娓叉煋 GBuffer 鍒?RSM MRT锛圖DGI 鐨?VPL 鏁版嵁婧愶級
    registered_passes_.push_back(std::make_unique<dse::render::RSMRenderPass>(render_pass_context_));
    // DDGI Probe Update 鈥?RSM 涔嬪悗銆佸厜鐓?Forward pass 涔嬪墠
    registered_passes_.push_back(std::make_unique<dse::render::DDGIUpdatePass>(render_pass_context_));
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

    // ---- 妯″潡鍔ㄦ€佹敞鍐岃嚜瀹氫箟 Pass ----
    for (auto& mod : modules_) {
        if (mod.instance) {
            mod.instance->RegisterRenderPasses(render_graph_dag_, render_pass_context_, registered_passes_);
        }
    }
#ifdef DSE_ENABLE_3D
    if (builtin_gameplay3d_enabled_) {
        modules_impl_->RegisterGameplay3DPasses(render_graph_dag_, render_pass_context_, registered_passes_);
    }
#endif

    // ---- 鎵€鏈?Pass 鍦?RenderGraph 涓婂０鏄庝緷璧?----
    for (auto& pass : registered_passes_) {
        pass->Setup(render_graph_dag_);
    }

    // 缂栬瘧 DAG锛堟嫇鎵戞帓搴?+ 鏃犵敤 Pass 鍓旈櫎锛?
    if (!render_graph_dag_.Compile()) {
        DEBUG_LOG_ERROR("RenderGraph 缂栬瘧澶辫触锛氭娴嬪埌寰幆渚濊禆");
    }
}


void FramePipeline::ExecuteRenderGraph(CommandBuffer& cmd_buffer) {
    dse::runtime::ExecuteFrameRenderGraph(*this, cmd_buffer);
}

/// 棰勭儹 builtin Pass 鍦?Execute() 涓敤鍒扮殑鎵€鏈?ECS 缁勪欢姹犮€?
/// 鏂板 Pass 鑻ヤ娇鐢ㄦ柊缁勪欢绫诲瀷锛屽繀椤诲湪姝ゅ琛ュ厖瀵瑰簲 view 璋冪敤銆?
/// Debug 妯″紡涓?ExecuteRenderGraphInternal 浼氬湪骞惰鎵ц鍚庢柇瑷€姹犳暟閲忔湭澧為暱锛?
/// 浠ユ娴嬮仐婕忕殑缁勪欢绫诲瀷銆?
static void WarmUpRenderECSPools(entt::registry& reg) {
    // --- builtin Pass 鐩存帴浣跨敤 ---
    (void)reg.view<TransformComponent>();
    (void)reg.view<CameraComponent>();
    (void)reg.view<dse::Camera3DComponent>();
    (void)reg.view<dse::DirectionalLight3DComponent>();
    (void)reg.view<TransformComponent, dse::SpotLightComponent>();
    (void)reg.view<TransformComponent, dse::PointLightComponent>();
    (void)reg.view<dse::PostProcessComponent>();
    (void)reg.view<dse::SkyboxComponent>();
    (void)reg.view<dse::DecalComponent, TransformComponent>();
    (void)reg.view<dse::WaterComponent>();
    // --- 妯″潡娓叉煋鍥炶皟 (OnRenderScene / OnRenderTransparent) 闂存帴浣跨敤 ---
    (void)reg.view<TransformComponent, dse::MeshRendererComponent>();
    (void)reg.view<dse::SkyLightComponent>();
    (void)reg.view<dse::TerrainComponent, TransformComponent>();
    (void)reg.view<dse::GrassComponent, TransformComponent>();
    (void)reg.view<dse::HairComponent, TransformComponent>();
    (void)reg.view<dse::ParticleSystem3DComponent>();
    (void)reg.view<dse::FluidEmitterComponent>();
    (void)reg.view<dse::GIProbeVolumeComponent>();
}

void FramePipeline::ExecuteRenderGraphInternal(CommandBuffer& cmd_buffer) {
    static int diag_pass_frame = 0;
    const char* pass_diag = std::getenv("DSE_PASS_DIAG");
    const bool pass_diag_enabled = pass_diag && pass_diag[0] != '\0' && pass_diag[0] != '0';
    const unsigned int scene_rt = render_resources_.scene_render_target;
    if (pass_diag_enabled && diag_pass_frame < 4 && scene_rt != 0 && runtime_context_.rhi_device) {
        dse::render::RhiDevice* rhi = runtime_context_.rhi_device.get();
        render_graph_dag_.ExecuteWithCallback(cmd_buffer, [&](const std::string& pass_name) {
            auto rb = rhi->ReadRenderTargetColorRgba8WithSize(scene_rt);
            if (rb.width > 0 && rb.height > 0) {
                int cx = rb.width / 2, cy = rb.height / 2;
                int bx = rb.width / 4, by = rb.height / 4;
                std::size_t ci = (static_cast<std::size_t>(cy) * rb.width + cx) * 4;
                std::size_t bi = (static_cast<std::size_t>(by) * rb.width + bx) * 4;
                int black_count = 0;
                for (std::size_t i = 0; i + 3 < rb.pixels.size(); i += 4) {
                    if (static_cast<int>(rb.pixels[i]) + static_cast<int>(rb.pixels[i+1]) + static_cast<int>(rb.pixels[i+2]) <= 8)
                        ++black_count;
                }
                DEBUG_LOG_INFO("[PassDiag] frame={} after={} black={} center=({},{},{}) bbox_tl=({},{},{})",
                    diag_pass_frame, pass_name, black_count,
                    static_cast<int>(rb.pixels[ci]), static_cast<int>(rb.pixels[ci+1]), static_cast<int>(rb.pixels[ci+2]),
                    static_cast<int>(rb.pixels[bi]), static_cast<int>(rb.pixels[bi+1]), static_cast<int>(rb.pixels[bi+2]));
            }
        });
        ++diag_pass_frame;
    } else {
        render_graph_dag_.Execute(cmd_buffer);
    }
}

void FramePipeline::SetRenderContextCallbacks(
        std::function<void()> make_current,
        std::function<void()> release,
        std::function<void()> present) {
    runtime_context_.make_render_context_current = std::move(make_current);
    runtime_context_.release_render_context = std::move(release);
    runtime_context_.present_frame = std::move(present);
}

void FramePipeline::EnableEditorMode(bool enable) {
    if (!initialized_) {
        runtime_context_.editor_mode = enable;
    }
}

void FramePipeline::ResetPhysics3D() {
#ifdef DSE_ENABLE_3D
    if (physics3d_system_) {
        dse::core::ServiceLocator::Instance().Reset<dse::physics3d::IPhysics3DSystem>();
        physics3d_system_->Shutdown();
        physics3d_system_.reset();
    }
#endif
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

dse::render::RhiDevice::RhiFrameStats FramePipeline::GetRhiFrameStats() const {
    if (!runtime_context_.rhi_device) return {};
    return runtime_context_.rhi_device->GetFrameStats();
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

RenderTargetReadback FramePipeline::ReadBloomMip0Rgba8WithSize() const {
    if (!runtime_context_.rhi_device || render_resources_.pp_bloom_mip_rts.empty()) return {};
    return runtime_context_.rhi_device->ReadRenderTargetColorRgba8WithSize(render_resources_.pp_bloom_mip_rts[0]);
}

RenderTargetReadback FramePipeline::ReadBloomExtractRgba8WithSize() const {
    if (!runtime_context_.rhi_device || render_resources_.pp_bloom_extract_rt == 0) return {};
    return runtime_context_.rhi_device->ReadRenderTargetColorRgba8WithSize(render_resources_.pp_bloom_extract_rt);
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

void FramePipeline::SetQuitCallback(std::function<void()> cb) {
    runtime_context_.quit_app = std::move(cb);
}

void FramePipeline::SetTargetFpsCallbacks(std::function<void(float)> setter, std::function<float()> getter) {
    runtime_context_.set_target_fps = std::move(setter);
    runtime_context_.get_target_fps = std::move(getter);
}

void FramePipeline::SetInitKeepAlive(std::function<void()> cb) {
    init_keep_alive_ = std::move(cb);
}

void FramePipeline::SetEditorCamera(const glm::mat4& view, const glm::mat4& projection) {
    render_pass_context_.use_editor_camera = true;
    render_pass_context_.editor_view = view;
    render_pass_context_.editor_projection = projection;
}

void FramePipeline::DisableEditorCamera() {
    render_pass_context_.use_editor_camera = false;
}

void FramePipeline::SetSceneViewMode(int mode) {
    render_pass_context_.scene_view_mode = mode;
}

unsigned int FramePipeline::RenderSceneWithCamera(const glm::mat4& view, const glm::mat4& projection) {
    if (!initialized_ || !runtime_context_.rhi_device) return 0;

    // 保存当前编辑器相机状态
    const bool saved_use = render_pass_context_.use_editor_camera;
    const glm::mat4 saved_view = render_pass_context_.editor_view;
    const glm::mat4 saved_proj = render_pass_context_.editor_projection;

    // 设置临时相机
    render_pass_context_.use_editor_camera = true;
    render_pass_context_.editor_view = view;
    render_pass_context_.editor_projection = projection;

    // 创建命令缓冲，执行 shadow pass + scene pass
    auto cmd = runtime_context_.rhi_device->CreateCommandBuffer();
    for (auto& pass : registered_passes_) {
        const char* name = pass->GetName();
        if (std::strcmp(name, "shadow_pass") == 0 ||
            std::strcmp(name, "spot_shadow_pass") == 0 ||
            std::strcmp(name, "point_shadow_pass") == 0 ||
            std::strcmp(name, "scene_pass") == 0) {
            pass->Execute(*cmd);
        }
    }
    runtime_context_.rhi_device->Submit(cmd);

    // 恢复相机
    render_pass_context_.use_editor_camera = saved_use;
    render_pass_context_.editor_view = saved_view;
    render_pass_context_.editor_projection = saved_proj;

    return GetSceneTextureId();
}

void FramePipeline::SetAssetManager(AssetManager* asset_manager) {
    if (initialized_) {
        return;
    }
    runtime_context_.asset_manager = asset_manager;
}

// ============================================================
// Phase 1 薄快照：一次性提取渲染线程所需的全部 ECS 数据
// ============================================================

void FramePipeline::CaptureThinSnapshot() {
    auto& snap = write_snapshot();
    snap.Reset();

    auto* world = runtime_context_.world;
    if (!world) return;
    auto& reg = world->registry();

    // ── 1. 3D Camera（priority + entity id 确定唯一主相机）──
    {
        auto view = reg.view<dse::Camera3DComponent>();
        entt::entity best = entt::null;
        int best_priority = std::numeric_limits<int>::min();
        std::uint32_t best_id = std::numeric_limits<std::uint32_t>::max();
        for (auto e : view) {
            auto& cam = view.get<dse::Camera3DComponent>(e);
            if (!cam.enabled) continue;
            auto eid = static_cast<std::uint32_t>(e);
            if (best == entt::null ||
                cam.priority > best_priority ||
                (cam.priority == best_priority && eid < best_id)) {
                best = e;
                best_priority = cam.priority;
                best_id = eid;
            }
        }
        if (best != entt::null) {
            auto& cam = view.get<dse::Camera3DComponent>(best);
            auto& c = snap.camera_3d;
            c.valid = true;
            c.fov = cam.fov;
            c.near_clip = cam.near_clip;
            c.far_clip = cam.far_clip;
            if (reg.all_of<TransformComponent>(best)) {
                auto& tf = reg.get<TransformComponent>(best);
                c.position = tf.position;
                c.forward = tf.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                c.up = tf.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                c.right = tf.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
                c.view = glm::lookAt(c.position, c.position + c.forward, c.up);
                c.shadow_center = c.position + c.forward * 50.0f;
            }
        }
    }

    // ── 2. 2D Camera fallback ──
    {
        auto view = reg.view<CameraComponent>();
        entt::entity best = entt::null;
        int best_priority = std::numeric_limits<int>::min();
        std::uint32_t best_id = std::numeric_limits<std::uint32_t>::max();
        for (auto e : view) {
            auto& cam = view.get<CameraComponent>(e);
            if (!cam.enabled) continue;
            auto eid = static_cast<std::uint32_t>(e);
            if (best == entt::null ||
                cam.priority > best_priority ||
                (cam.priority == best_priority && eid < best_id)) {
                best = e;
                best_priority = cam.priority;
                best_id = eid;
            }
        }
        if (best != entt::null) {
            auto& cam = view.get<CameraComponent>(best);
            snap.camera_2d.valid = true;
            snap.camera_2d.view = cam.view;
            snap.camera_2d.projection = cam.projection;
        }
    }

    // ── 3. Skybox（含 lazy load 写回，主线程安全）──
    {
        auto view = reg.view<dse::SkyboxComponent>();
        for (auto e : view) {
            auto& sb = view.get<dse::SkyboxComponent>(e);
            if (!sb.enabled) continue;
            if (sb.cubemap_handle == 0 && !sb.cubemap_path.empty()) {
                if (auto cubemap = runtime_context_.asset_manager->LoadCubemap(sb.cubemap_path)) {
                    sb.cubemap_handle = cubemap->GetHandle();
                }
            }
            if (sb.cubemap_handle != 0) {
                snap.skybox.valid = true;
                snap.skybox.cubemap_handle = sb.cubemap_handle;
                if (reg.all_of<TransformComponent>(e)) {
                    snap.skybox.has_transform = true;
                    snap.skybox.rotation = reg.get<TransformComponent>(e).rotation;
                }
            }
            break;
        }
    }

    // ── 4. Directional Light ──
    {
        auto view = reg.view<dse::DirectionalLight3DComponent>();
        for (auto e : view) {
            auto& dl = view.get<dse::DirectionalLight3DComponent>(e);
            if (!dl.enabled) continue;
            auto& s = snap.directional_light;
            s.valid = true;
            s.cast_shadow = dl.cast_shadow;
            s.direction = dl.direction;
            s.color = dl.color;
            s.intensity = dl.intensity;
            s.ambient_intensity = dl.ambient_intensity;
            s.shadow_strength = dl.shadow_strength;
            for (int i = 0; i < CSM_CASCADES; ++i)
                s.cascade_splits[i] = dl.cascade_splits[i];
            break;
        }
    }

    // ── 5. Spot Lights（shadow-casting, max 4）──
    {
        auto view = reg.view<TransformComponent, dse::SpotLightComponent>();
        snap.spot_shadow_count = 0;
        for (auto e : view) {
            if (snap.spot_shadow_count >= dse::render::RenderThinSnapshot::kMaxSpotShadowLights) break;
            auto& light = view.get<dse::SpotLightComponent>(e);
            if (!light.enabled || !light.cast_shadow) continue;
            auto& tf = view.get<TransformComponent>(e);
            auto& sl = snap.spot_lights[snap.spot_shadow_count];
            sl.position = tf.position;
            sl.forward = glm::normalize(tf.rotation * light.direction);
            sl.up = tf.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            if (std::abs(glm::dot(sl.forward, sl.up)) > 0.98f) {
                sl.up = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            sl.outer_cone_angle = light.outer_cone_angle;
            sl.radius = light.radius;
            ++snap.spot_shadow_count;
        }
    }

    // ── 6. Point Lights（shadow-casting, max 4）──
    {
        auto view = reg.view<TransformComponent, dse::PointLightComponent>();
        snap.point_shadow_count = 0;
        for (auto e : view) {
            if (snap.point_shadow_count >= dse::render::RenderThinSnapshot::kMaxPointShadowLights) break;
            auto& light = view.get<dse::PointLightComponent>(e);
            if (!light.enabled || !light.cast_shadow) continue;
            auto& tf = view.get<TransformComponent>(e);
            auto& pl = snap.point_lights[snap.point_shadow_count];
            pl.position = tf.position;
            pl.radius = light.radius;
            ++snap.point_shadow_count;
        }
    }

    // ── 7. PostProcess（合并 13 个 Pass 的重复查询）──
    {
        auto view = reg.view<dse::PostProcessComponent>();
        for (auto e : view) {
            auto& pp = view.get<dse::PostProcessComponent>(e);
            if (!pp.enabled) continue;
            auto& s = snap.post_process;
            s.valid = true;
            s.enabled = pp.enabled;
            s.bloom_enabled = pp.bloom_enabled;
            s.bloom_threshold = pp.bloom_threshold;
            s.bloom_intensity = pp.bloom_intensity;
            s.color_grading_enabled = pp.color_grading_enabled;
            s.exposure = pp.exposure;
            s.gamma = pp.gamma;
            s.ssao_enabled = pp.ssao_enabled;
            s.ssao_radius = pp.ssao_radius;
            s.ssao_bias = pp.ssao_bias;
            s.auto_exposure_enabled = pp.auto_exposure_enabled;
            s.exposure_min = pp.exposure_min;
            s.exposure_max = pp.exposure_max;
            s.adaptation_speed_up = pp.adaptation_speed_up;
            s.adaptation_speed_down = pp.adaptation_speed_down;
            s.exposure_compensation = pp.exposure_compensation;
            s.color_lut_handle = pp.color_lut_handle;
            s.color_lut_intensity = pp.color_lut_intensity;
            s.vignette_enabled = pp.vignette_enabled;
            s.vignette_intensity = pp.vignette_intensity;
            s.vignette_radius = pp.vignette_radius;
            s.vignette_softness = pp.vignette_softness;
            s.film_grain_enabled = pp.film_grain_enabled;
            s.film_grain_intensity = pp.film_grain_intensity;
            s.film_grain_time_scale = pp.film_grain_time_scale;
            s.fxaa_enabled = pp.fxaa_enabled;
            s.taa_enabled = pp.taa_enabled;
            s.taa_blend_factor = pp.taa_blend_factor;
            s.contact_shadow_enabled = pp.contact_shadow_enabled;
            s.contact_shadow_strength = pp.contact_shadow_strength;
            s.contact_shadow_steps = pp.contact_shadow_steps;
            s.contact_shadow_step_size = pp.contact_shadow_step_size;
            s.dof_enabled = pp.dof_enabled;
            s.dof_focus_distance = pp.dof_focus_distance;
            s.dof_focus_range = pp.dof_focus_range;
            s.dof_bokeh_radius = pp.dof_bokeh_radius;
            s.motion_blur_enabled = pp.motion_blur_enabled;
            s.motion_blur_intensity = pp.motion_blur_intensity;
            s.motion_blur_samples = pp.motion_blur_samples;
            s.ssr_enabled = pp.ssr_enabled;
            s.ssr_max_distance = pp.ssr_max_distance;
            s.ssr_thickness = pp.ssr_thickness;
            s.ssr_step_size = pp.ssr_step_size;
            s.ssr_max_steps = pp.ssr_max_steps;
            s.outline_enabled = pp.outline_enabled;
            s.outline_color = pp.outline_color;
            s.outline_thickness = pp.outline_thickness;
            s.outline_depth_threshold = pp.outline_depth_threshold;
            s.outline_normal_threshold = pp.outline_normal_threshold;
            s.light_shaft_enabled = pp.light_shaft_enabled;
            s.light_shaft_color = pp.light_shaft_color;
            s.light_shaft_density = pp.light_shaft_density;
            s.light_shaft_weight = pp.light_shaft_weight;
            s.light_shaft_decay = pp.light_shaft_decay;
            s.light_shaft_exposure = pp.light_shaft_exposure;
            s.light_shaft_intensity = pp.light_shaft_intensity;
            s.light_shaft_samples = pp.light_shaft_samples;
            s.fog_enabled = pp.fog_enabled;
            s.fog_color = pp.fog_color;
            s.fog_density = pp.fog_density;
            s.fog_height_falloff = pp.fog_height_falloff;
            s.fog_height_offset = pp.fog_height_offset;
            s.fog_start = pp.fog_start;
            s.fog_end = pp.fog_end;
            s.fog_steps = pp.fog_steps;
            s.fog_sun_scatter = pp.fog_sun_scatter;
            break;
        }
    }

    // ── 8. Water surfaces ──
    {
        auto view = reg.view<dse::WaterComponent>();
        snap.water_count = 0;
        for (auto e : view) {
            if (snap.water_count >= dse::render::RenderThinSnapshot::kMaxWaterSurfaces) break;
            auto& w = view.get<dse::WaterComponent>(e);
            if (!w.enabled) continue;
            auto& ws = snap.waters[snap.water_count];
            ws.water_level = w.water_level;
            ws.deep_color = w.deep_color;
            ws.shallow_color = w.shallow_color;
            ws.max_depth = w.max_depth;
            ws.transparency = w.transparency;
            ws.wave_amplitude = w.wave_amplitude;
            ws.wave_frequency = w.wave_frequency;
            ws.wave_speed = w.wave_speed;
            ws.wave_direction = w.wave_direction;
            ws.refraction_strength = w.refraction_strength;
            ws.reflection_strength = w.reflection_strength;
            ws.specular_power = w.specular_power;
            ws.caustic_intensity = w.caustic_intensity;
            ws.caustic_scale = w.caustic_scale;
            ws.foam_intensity = w.foam_intensity;
            ws.foam_depth_threshold = w.foam_depth_threshold;
            ws.underwater_fog_density = w.underwater_fog_density;
            ws.underwater_fog_color = w.underwater_fog_color;
            ++snap.water_count;
        }
    }

    // ── 9. Decals ──
    {
        auto view = reg.view<TransformComponent, dse::DecalComponent>();
        snap.decal_count = 0;
        for (auto e : view) {
            if (snap.decal_count >= dse::render::RenderThinSnapshot::kMaxDecals) break;
            auto& dc = view.get<dse::DecalComponent>(e);
            if (!dc.enabled || dc.albedo_texture == 0) continue;
            auto& tf = view.get<TransformComponent>(e);
            auto& d = snap.decals[snap.decal_count];
            d.albedo_texture = dc.albedo_texture;
            d.color = dc.color;
            d.angle_fade = dc.angle_fade;
            d.position = tf.position;
            d.rotation = tf.rotation;
            d.scale = tf.scale;
            ++snap.decal_count;
        }
    }

    // ── 10. Light Probe SH（距离加权混合最近两个 probe）──
    {
        glm::vec3 cam_pos = snap.camera_3d.position;
        auto probe_view = reg.view<TransformComponent, dse::LightProbeComponent>();
        float best_dist = std::numeric_limits<float>::max();
        float second_dist = std::numeric_limits<float>::max();
        const dse::LightProbeComponent* best_probe = nullptr;
        const dse::LightProbeComponent* second_probe = nullptr;

        for (auto e : probe_view) {
            auto& probe = probe_view.get<dse::LightProbeComponent>(e);
            if (!probe.enabled) continue;
            auto& tf = probe_view.get<TransformComponent>(e);
            float dist = glm::distance(tf.position, cam_pos);
            if (dist < best_dist) {
                second_dist = best_dist;
                second_probe = best_probe;
                best_dist = dist;
                best_probe = &probe;
            } else if (dist < second_dist) {
                second_dist = dist;
                second_probe = &probe;
            }
        }

        if (best_probe) {
            snap.light_probe_sh.valid = true;
            if (second_probe && best_dist < best_probe->influence_radius &&
                second_dist < second_probe->influence_radius) {
                float total = best_dist + second_dist;
                if (total < 0.001f) total = 0.001f;
                float w1 = 1.0f - best_dist / total;
                float w2 = 1.0f - second_dist / total;
                float wsum = w1 + w2;
                w1 /= wsum; w2 /= wsum;
                for (int i = 0; i < 9; ++i) {
                    glm::vec3 blended = best_probe->sh_coefficients[i] * w1 +
                                        second_probe->sh_coefficients[i] * w2;
                    snap.light_probe_sh.coefficients[i] = glm::vec4(blended, 0.0f);
                }
            } else {
                for (int i = 0; i < 9; ++i) {
                    snap.light_probe_sh.coefficients[i] = glm::vec4(best_probe->sh_coefficients[i], 0.0f);
                }
            }
        }
    }

    // ── 11. DDGI Config ──
    {
        auto gi_view = reg.view<dse::GIProbeVolumeComponent>();
        for (auto e : gi_view) {
            auto& gi = gi_view.get<dse::GIProbeVolumeComponent>(e);
            if (!gi.enabled) continue;
            auto& cfg = snap.ddgi_config;
            cfg.enabled = true;
            cfg.needs_reinit = gi.needs_reinit_;
            cfg.origin = gi.origin;
            cfg.extent = gi.extent;
            cfg.resolution_x = gi.resolution_x;
            cfg.resolution_y = gi.resolution_y;
            cfg.resolution_z = gi.resolution_z;
            cfg.irradiance_texels = gi.irradiance_texels;
            cfg.visibility_texels = gi.visibility_texels;
            cfg.rays_per_probe = gi.rays_per_probe;
            cfg.hysteresis = gi.hysteresis;
            cfg.gi_intensity = gi.gi_intensity;
            cfg.normal_bias = gi.normal_bias;
            gi.needs_reinit_ = false;
            break;
        }
    }
}

// ============================================================
// Phase 2: 渲染线程分离
// ============================================================

void FramePipeline::StartRenderThread() {
    if (render_thread_active_.load()) return;

    {
        std::lock_guard<std::mutex> lock(render_mutex_);
        render_thread_exit_ = false;
        render_frame_pending_ = false;
        render_frame_done_ = true;
    }

    // 主线程释放 GL context，由渲染线程接管
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
        render_frame_pending_ = true;  // 唤醒线程使其检查 exit 标志
    }
    render_cv_.notify_one();

    if (render_thread_.joinable()) {
        render_thread_.join();
    }
    render_thread_active_.store(false);

    // 主线程重新获取 GL context
    if (runtime_context_.make_render_context_current) {
        runtime_context_.make_render_context_current();
    }
    DEBUG_LOG_INFO("[RenderThread] Stopped");
}

void FramePipeline::RenderThreadFunc() {
    // 渲染线程获取 GL/Vulkan/DX11 context
    if (runtime_context_.make_render_context_current) {
        runtime_context_.make_render_context_current();
    }

    while (true) {
        // 等待主线程发出渲染信号
        {
            std::unique_lock<std::mutex> lock(render_mutex_);
            render_cv_.wait(lock, [this] { return render_frame_pending_ || render_thread_exit_; });
            if (render_thread_exit_) break;
            render_frame_pending_ = false;
        }

        ExecuteRenderFrame();

        // 通知主线程渲染完成
        {
            std::lock_guard<std::mutex> lock(render_mutex_);
            render_frame_done_ = true;
        }
        main_cv_.notify_one();
    }

    // 释放 context
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
    // ── 主线程：纯 CPU 工作 + ECS 读取 ──

    if (runtime_context_.world) {
        // Clustered Forward+: 收集光源（CPU）
        light_buffer_.CollectLights(*runtime_context_.world);

        // 从 ECS 获取主相机参数构建 cluster（与 RunRenderInternal 一致）
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
        }
    }

    // TAA: 预检测 ECS 组件
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

    render_pass_context_.delta_time = Time::delta_time();

    // 捕获快照 + 翻转双缓冲
    CaptureThinSnapshot();
    FlipSnapshotIndex();
    render_pass_context_.snapshot = &read_snapshot();
}

void FramePipeline::ExecuteRenderFrame() {
    auto render_begin = std::chrono::high_resolution_clock::now();

    dse::runtime::BeginRuntimeRenderFrame(*this);
    auto cmd_buffer = dse::runtime::CreateRuntimeRenderCommandBuffer(*this);
    dse::runtime::BindRuntimeShadowMaps(*this);

    // GPU 上传：光源 SSBO
    light_buffer_.Upload();

    // GPU 上传：cluster SSBO
    cluster_grid_.Upload();

    // Light Probe SH：从快照上传到 RHI 全局状态
    const auto& snap = *render_pass_context_.snapshot;
    if (snap.light_probe_sh.valid) {
        runtime_context_.rhi_device->SetGlobalLightProbeSH(
            snap.light_probe_sh.coefficients, true);
    } else {
        glm::vec4 zero_sh[9] = {};
        runtime_context_.rhi_device->SetGlobalLightProbeSH(zero_sh, false);
    }

    // DDGI: 从快照配置初始化/重配置 + 同步到 RHI 全局状态
    render_pass_context_.ddgi_active = false;
    render_pass_context_.ddgi_system = nullptr;
    if (snap.ddgi_config.enabled && runtime_context_.rhi_device->SupportsCompute()) {
        const auto& dcfg = snap.ddgi_config;
        if (dcfg.needs_reinit || !ddgi_system_.IsInitialized()) {
            dse::render::gi::DDGIVolumeConfig cfg;
            cfg.origin = dcfg.origin;
            cfg.extent = dcfg.extent;
            cfg.resolution = glm::ivec3(dcfg.resolution_x, dcfg.resolution_y, dcfg.resolution_z);
            cfg.irradiance_texels = dcfg.irradiance_texels;
            cfg.visibility_texels = dcfg.visibility_texels;
            cfg.rays_per_probe = dcfg.rays_per_probe;
            cfg.hysteresis = dcfg.hysteresis;
            if (ddgi_system_.IsInitialized()) {
                ddgi_system_.Reconfigure(runtime_context_.rhi_device.get(), cfg);
            } else {
                ddgi_system_.Init(runtime_context_.rhi_device.get(), cfg);
            }
        }
        if (ddgi_system_.IsInitialized()) {
            render_pass_context_.ddgi_system = &ddgi_system_;
            render_pass_context_.ddgi_active = true;
            render_pass_context_.ddgi_gi_intensity = dcfg.gi_intensity;
            render_pass_context_.ddgi_normal_bias = dcfg.normal_bias;
            const auto& res = ddgi_system_.GetResources();
            render_pass_context_.ddgi_irradiance_atlas = res.irradiance_atlas;
            render_pass_context_.ddgi_visibility_atlas = res.visibility_atlas;
        }
    }
    if (render_pass_context_.ddgi_active) {
        const auto& cfg = ddgi_system_.GetConfig();
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

    // Hi-Z AABB 上传
    if (render_resources_.hiz_aabb_ssbo && render_resources_.hiz_visibility_ssbo) {
        const auto& aabbs = modules_impl_->CachedAABBs();
        const int count = modules_impl_->CachedAABBCount();
        if (count > 0 && static_cast<size_t>(count) <= render_resources_.hiz_ssbo_capacity) {
            runtime_context_.rhi_device->UpdateGpuBuffer(
                render_resources_.hiz_aabb_ssbo, 0,
                count * sizeof(dse::gameplay3d::HiZAABB),
                aabbs.data());
            render_pass_context_.hiz_object_count = count;
        } else {
            render_pass_context_.hiz_object_count = 0;
        }
    }

    // GPU Driven: 线程模式下跳过（需要 ECS + GPU 混合访问，Phase 3 再支持）
    // 非线程模式在 RunRenderInternal 中处理

    // ── 执行渲染图 ──
    ExecuteRenderGraph(*cmd_buffer);

    dse::runtime::SubmitAndEndRuntimeRenderFrame(*this, std::move(cmd_buffer));

    // Hi-Z / GPU Driven readback
    if (render_pass_context_.gpu_driven_enabled && render_pass_context_.gpu_indirect_draw_count > 0
        && render_pass_context_.gpu_draw_cmd_ssbo) {
        const int count = render_pass_context_.gpu_indirect_draw_count;
        std::vector<DrawElementsIndirectCommand> cmds(count);
        runtime_context_.rhi_device->ReadGpuBuffer(
            render_pass_context_.gpu_draw_cmd_ssbo, 0,
            count * sizeof(DrawElementsIndirectCommand), cmds.data());
        std::vector<uint32_t> visibility(count);
        int culled = 0;
        for (int i = 0; i < count; ++i) {
            visibility[i] = cmds[i].instance_count > 0 ? 1u : 0u;
            if (visibility[i] == 0) ++culled;
        }
        modules_impl_->SetHiZVisibility(visibility);
        gpu_culled_last_frame_ = culled;
        runtime_context_.rhi_device->PatchLastFrameGPUCulledCount(culled);
    } else if (render_resources_.hiz_visibility_ssbo && render_pass_context_.hiz_object_count > 0
        && render_pass_context_.hiz_culling_enabled) {
        const int count = render_pass_context_.hiz_object_count;
        std::vector<uint32_t> visibility(count, 1);
        runtime_context_.rhi_device->ReadGpuBuffer(
            render_resources_.hiz_visibility_ssbo, 0,
            count * sizeof(uint32_t), visibility.data());
        modules_impl_->SetHiZVisibility(visibility);
    }

    dse::runtime::FinalizeRuntimeRenderFrame(*this);

    // Present (SwapBuffers)
    if (runtime_context_.present_frame) {
        runtime_context_.present_frame();
    }

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
}
