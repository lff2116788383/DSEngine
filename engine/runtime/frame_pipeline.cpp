/**
 * @file frame_pipeline.cpp
 * @brief 寮曟搸涓诲惊鐜笌甯ф祦姘寸嚎锛屽崗璋冩洿鏂般€佺墿鐞嗗拰娓叉煋鐨勬墽琛岄『搴?
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

namespace dse::render {
    extern const char* kHiZCopyShaderSource;
    extern const char* kHiZDownsampleShaderSource;
    extern const char* kHiZCullShaderSource;
    extern const char* kGPUCullShaderSource;
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
    DEBUG_LOG_INFO("FramePipeline RHI 鍚庣: {}", dse::render::RhiBackendToString(rhi_backend));

    // D3D11 / Vulkan 鍚庣闇€瑕佺敤骞冲彴绐楀彛鍙ユ焺瀹屾垚璁惧鍒濆鍖?
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
        // 绔嬪嵆 present 涓€甯ч粦灞忥紝娑堥櫎绐楀彛鍒涘缓鍚庡埌棣栧抚娓叉煋鍓嶇殑鐧藉睆
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
    if (Screen::width() <= 0 || Screen::height() <= 0) {
        Screen::set_width_height(1280, 720);
    }
    const int render_width = Screen::width();
    const int render_height = Screen::height();
    
    // 濮嬬粓鍒涘缓鏈€缁堝悎鎴?RenderTarget锛歟ditor 鐩存帴灞曠ず璇ョ汗鐞嗭紝runtime 鍐?present 鍒伴粯璁?framebuffer銆?
    RenderTargetDesc main_rt_desc{};
    main_rt_desc.width = render_width;
    main_rt_desc.height = render_height;
    main_rt_desc.has_color = true;
    main_rt_desc.has_depth = false;
    render_resources_.main_render_target = runtime_context_.rhi_device->CreateRenderTarget(main_rt_desc);





    
    // 浣跨敤鏀寔 HDR 鐨勬诞鐐圭汗鐞嗕綔涓?Scene Render Target锛岃繖鏄硾鍏夊拰鑹茶皟鏄犲皠鐨勫熀纭€
    // msaa_samples=4锛氬紑鍚?4x MSAA锛堜笉鏀寔鏃?resource_mgr 鑷姩闄嶇骇涓?1x锛?
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
    // allow_uav=true锛氬厑璁?CS 鍐欏叆锛圖3D11 Bloom Compute Shader 璺緞浣跨敤锛?
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

    // SSAO: 鍗婂垎杈ㄧ巼鍗曢€氶亾 RT
    if (render_resources_.pp_ssao_rt == 0) {
        render_resources_.pp_ssao_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width / 2, render_height / 2, true, false, false});
    }
    if (render_resources_.pp_ssao_blur_rt == 0) {
        render_resources_.pp_ssao_blur_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width / 2, render_height / 2, true, false, false});
    }
    // Contact Shadow: 鍗婂垎杈ㄧ巼鍗曢€氶亾 RT
    if (render_resources_.pp_contact_shadow_rt == 0) {
        render_resources_.pp_contact_shadow_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width / 2, render_height / 2, true, false, false});
    }
    // FXAA: 鍏ㄥ垎杈ㄧ巼 RT
    if (render_resources_.pp_fxaa_rt == 0) {
        render_resources_.pp_fxaa_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // TAA: 鍏ㄥ垎杈ㄧ巼 RT
    if (render_resources_.pp_taa_rt == 0) {
        render_resources_.pp_taa_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // DOF: 鍏ㄥ垎杈ㄧ巼 RT
    if (render_resources_.pp_dof_rt == 0) {
        render_resources_.pp_dof_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // SSR: 鍗婂垎杈ㄧ巼 RT
    if (render_resources_.pp_ssr_rt == 0) {
        render_resources_.pp_ssr_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width / 2, render_height / 2, true, false, false});
    }
    // Motion Vector: 鍏ㄥ垎杈ㄧ巼 RT (RG16F 閫熷害鍦?
    if (render_resources_.pp_motion_vector_rt == 0) {
        render_resources_.pp_motion_vector_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // Outline / Edge Detection: 鍏ㄥ垎杈ㄧ巼 RT
    if (render_resources_.pp_outline_rt == 0) {
        render_resources_.pp_outline_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // Volumetric Fog: 鍏ㄥ垎杈ㄧ巼 RT锛堝瓨鍌?scene+fog 鍚堟垚缁撴灉锛?
    if (render_resources_.pp_fog_rt == 0) {
        render_resources_.pp_fog_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }

    // WBOIT accumulation: 鍏ㄥ垎杈ㄧ巼 RGBA16F锛堥渶瑕?HDR 绮惧害绱Н鍔犳潈棰滆壊锛?
    if (render_resources_.wboit_accum_rt == 0) {
        render_resources_.wboit_accum_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }
    // WBOIT revealage: 鍏ㄥ垎杈ㄧ巼锛圧 閫氶亾瀛樺偍 prod(1-alpha_i)锛?
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
    // Deferred Lighting output: 鍏ㄥ垎杈ㄧ巼鍗曢鑹查檮浠?
    if (render_resources_.deferred_lighting_rt == 0) {
        render_resources_.deferred_lighting_rt = runtime_context_.rhi_device->CreateRenderTarget(
            {render_width, render_height, true, false, false});
    }

    // Auto Exposure: 64x64 涓存椂浜害 + 2 涓?1x1 ping-pong RT
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

            // 鍒涘缓 compute shader锛堜粎涓€娆★紝缂撳瓨鍙ユ焺锛夆€?婧愮爜瀹氫箟鍦?builtin_passes.cpp
            render_resources_.hiz_copy_shader = runtime_context_.rhi_device->CreateComputeShader(dse::render::kHiZCopyShaderSource);
            render_resources_.hiz_downsample_shader = runtime_context_.rhi_device->CreateComputeShader(dse::render::kHiZDownsampleShaderSource);
            render_resources_.hiz_cull_shader = runtime_context_.rhi_device->CreateComputeShader(dse::render::kHiZCullShaderSource);

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

    // GPU Driven Rendering 鑳藉姏妫€娴?
    const char* disable_gpu_driven_env = std::getenv("DSE_DISABLE_GPU_DRIVEN");
    const bool gpu_driven_disabled = disable_gpu_driven_env && disable_gpu_driven_env[0] != '\0' && disable_gpu_driven_env[0] != '0';
    if (!gpu_driven_disabled &&
        runtime_context_.rhi_device->SupportsCompute() &&
        runtime_context_.rhi_device->SupportsIndirectDraw() &&
        runtime_context_.rhi_device->SupportsSSBO()) {
        render_resources_.gpu_driven_supported = true;
        render_resources_.gpu_cull_shader = runtime_context_.rhi_device->CreateComputeShader(dse::render::kGPUCullShaderSource);
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
#if defined(DSE_ENABLE_3D) && (defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT))
    // 3D 物理必须在鐗╃悊蹇呴』鍦?Gameplay3D 妯″潡涔嬪墠鍒濆鍖栧苟娉ㄥ唽鍒?ServiceLocator锛?
    // 鍚﹀垯 FractureSystem::SetPhysics3D() 浼氭嬁鍒?nullptr锛屽鑷寸鐗囧悓鏃跺惎鐢?
    // CPU fallback 鐗╃悊鍜?PhysX 鐗╃悊锛屼袱濂楃墿鐞嗕氦鏇胯鍐?transform 鈫?闂儊銆?
    if (physics3d_system_.Init(*runtime_context_.world)) {
        dse::core::ServiceLocator::Instance().Register<dse::physics3d::Physics3DSystem, dse::physics3d::Physics3DSystem>(
            std::shared_ptr<dse::physics3d::Physics3DSystem>(&physics3d_system_, [](auto*) {}));
        physics3d_system_initialized_ = true;
        DEBUG_LOG_INFO("FramePipeline init: Physics3DSystem initialized and registered");
    } else {
        DEBUG_LOG_WARN("FramePipeline init: Physics3DSystem init failed, 3D physics will be unavailable");
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

    // Clustered Forward+ 鍏夋簮 SSBO + Cluster 缃戞牸鍒濆鍖?
    light_buffer_.Init(runtime_context_.rhi_device.get());
    cluster_grid_.Init(runtime_context_.rhi_device.get());
    lap("light buffer + cluster grid");

    // Light Probe SH Bake 绯荤粺鍒濆鍖?
    light_probe_system_.Init(runtime_context_.rhi_device.get());
    lap("LightProbeSystem");

    // Reflection Probe + IBL 绯荤粺鍒濆鍖栵紙鐢熸垚 BRDF LUT锛?
    reflection_probe_system_.Init(runtime_context_.rhi_device.get());
    lap("ReflectionProbeSystem");

    // DDGI 绯荤粺寤惰繜鍒濆鍖栵紙棣栧抚妫€娴?GIProbeVolumeComponent 鍚庢寜闇€鍒濆鍖栵級

    // 璧勬簮娴佸紡鍔犺浇绠＄悊鍣ㄥ垵濮嬪寲
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
    return true;
}

void FramePipeline::Shutdown() {
    if (!initialized_) {
        return;
    }
    if (auto* event_bus = dse::core::ServiceLocator::Instance().Get<dse::core::EventBus>()) {
        event_bus->Publish<dse::core::SceneLifecycleEvent>(dse::core::SceneLifecyclePhase::Shutdown);
    }
    // 璧勬簮娴佸紡鍔犺浇绠＄悊鍣ㄥ叧闂?
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
#if defined(DSE_ENABLE_3D) && (defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT))
    if (physics3d_system_initialized_) {
        dse::core::ServiceLocator::Instance().Reset<dse::physics3d::Physics3DSystem>();
        physics3d_system_.Shutdown();
        physics3d_system_initialized_ = false;
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

    // Hi-Z: 閲婃斁 GPU 璧勬簮
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

    // GPU Driven 璧勬簮娓呯悊
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
#if defined(DSE_ENABLE_3D) && (defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT))
    if (physics3d_system_initialized_) {
        physics3d_system_.FlushEvents();
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
    dse::runtime::RunFrameRender(*this);
}

void FramePipeline::RunUpdateInternal(float delta_time) {
    auto update_begin = std::chrono::high_resolution_clock::now();
    auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
    asset_manager.PumpMainThreadCallbacks(callback_budget_per_frame_);
    asset_manager.PumpHotReloads();

    // 璧勬簮娴佸紡鍔犺浇锛氳幏鍙栨憚鍍忔満浣嶇疆骞?tick
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

    // Clustered Forward+: 姣忓抚鏀堕泦鍏夋簮 鈫?鏋勫缓 Cluster 鈫?涓婁紶 SSBO
    if (runtime_context_.world) {
        light_buffer_.CollectLights(*runtime_context_.world);
        light_buffer_.Upload();

        // 鑾峰彇涓荤浉鏈哄弬鏁扮敤浜?cluster 鏋勫缓
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

    // Light Probe SH: 鏌ヨ鏈€杩?probe锛屼紶缁?GPU UBO锛堟棤 probe 鏃?fallback 鍒?ambient_intensity锛?
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

    // TAA: 棰勬娴?ECS 缁勪欢锛屾彁鍓嶈缃?taa_active锛團orwardScenePass 闇€瑕佸湪鍦烘櫙娓叉煋鍓嶇煡閬撴槸鍚﹀簲鐢?jitter锛?
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

    // 姣忓抚鏇存柊 Auto Exposure 鎵€闇€鐨?delta_time
    render_pass_context_.delta_time = Time::delta_time();

    // DDGI: 妫€娴?GIProbeVolumeComponent锛屾寜闇€鍒濆鍖?鏇存柊绯荤粺
    render_pass_context_.ddgi_active = false;
    render_pass_context_.ddgi_system = nullptr;
    if (runtime_context_.world && runtime_context_.rhi_device->SupportsCompute()) {
        auto gi_view = runtime_context_.world->registry().view<dse::GIProbeVolumeComponent>();
        for (auto entity : gi_view) {
            auto& gi = gi_view.get<dse::GIProbeVolumeComponent>(entity);
            if (!gi.enabled) continue;

            // 鎸夐渶鍒濆鍖栨垨閲嶉厤缃?
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
            break;  // 浠呮敮鎸佸崟涓?GI Volume
        }
    }
    // 鍚屾 DDGI 鐘舵€佸埌 RHI 鍏ㄥ眬娓叉煋鐘舵€?
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

    // Hi-Z: 涓婁紶涓婁竴甯ф敹闆嗙殑 AABB 鍒?GPU SSBO锛堜緵 HiZCullPass 浣跨敤锛?
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

    // GPU Driven: 鍑嗗 GPU 鍦烘櫙鏁版嵁锛圓ABB + DrawCommands + Instance 鏁版嵁锛?
    if (render_resources_.gpu_driven_supported) {
        modules_impl_->PrepareGPUScene(*runtime_context_.world, render_pass_context_);
        // 鍚屾鍔ㄦ€佸垱寤虹殑鍙ユ焺鍥?render_resources_锛岀‘淇濅笅甯т笉琚鐩?
        render_resources_.gpu_draw_cmd_ssbo = render_pass_context_.gpu_draw_cmd_ssbo;
        render_resources_.gpu_instance_ssbo = render_pass_context_.gpu_instance_ssbo;
    }

    ExecuteRenderGraph(*cmd_buffer);
    
    dse::runtime::SubmitAndEndRuntimeRenderFrame(*this, std::move(cmd_buffer));

    // Hi-Z / GPU Driven: GPU 鎵ц瀹屾瘯鍚庯紝璇诲洖鍙鎬т緵涓嬩竴甯?MeshRenderSystem 浣跨敤
    if (render_pass_context_.gpu_driven_enabled && render_pass_context_.gpu_indirect_draw_count > 0
        && render_pass_context_.gpu_draw_cmd_ssbo) {
        // GPU Driven 璺緞锛氫粠 draw commands SSBO 璇诲洖 instance_count 浣滀负鍙鎬?
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
        // 浼犵粺 Hi-Z 璺緞锛氫粠 visibility SSBO 璇诲洖
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

    // ---- 濉厖 RenderPassContext ----
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
    // GPU Driven Cull Pass 鈥?瑙嗛敟 + Hi-Z 鍓旈櫎锛岀洿鎺ュ啓 indirect draw commands
    if (render_resources_.gpu_driven_supported) {
        registered_passes_.push_back(std::make_unique<dse::render::GPUCullPass>(render_pass_context_));
    }
    registered_passes_.push_back(std::make_unique<dse::render::CSMShadowPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::SpotShadowPass>(render_pass_context_));
    registered_passes_.push_back(std::make_unique<dse::render::PointShadowPass>(render_pass_context_));
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
    const unsigned int scene_rt = render_resources_.scene_render_target;
    if (diag_pass_frame < 4 && scene_rt != 0 && runtime_context_.rhi_device) {
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

void FramePipeline::EnableEditorMode(bool enable) {
    if (!initialized_) {
        runtime_context_.editor_mode = enable;
    }
}

void FramePipeline::ResetPhysics3D() {
#if defined(DSE_ENABLE_3D) && (defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT))
    if (physics3d_system_initialized_) {
        dse::core::ServiceLocator::Instance().Reset<dse::physics3d::Physics3DSystem>();
        physics3d_system_.Shutdown();
        physics3d_system_initialized_ = false;
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

void FramePipeline::SetAssetManager(AssetManager* asset_manager) {
    if (initialized_) {
        return;
    }
    runtime_context_.asset_manager = asset_manager;
}
