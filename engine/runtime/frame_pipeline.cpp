/**
 * @file frame_pipeline.cpp
 * @brief å¼•æ“Žä¸»å¾ªçŽ¯ä¸Žå¸§æµæ°´çº¿ï¼Œåè°ƒæ›´æ–°ã€ç‰©ç†å’Œæ¸²æŸ“çš„æ‰§è¡Œé¡ºåº
 */

#include "engine/runtime/frame_pipeline.h"

// Full definitions for types forward-declared in frame_pipeline.h
#include "engine/runtime/i_builtin_modules.h"
#include "engine/core/module.h"
#include "engine/core/dynamic_library.h"
#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/passes/builtin_passes.h"
#include "engine/render/hiz_types.h"

// Pimpl: RenderState definition + heavy headers
#include "engine/runtime/frame_pipeline_impl.h"

FramePipeline::FramePipeline()
    : modules_impl_(CreateBuiltinModules()),
      rs_(std::make_unique<RenderState>()) {}
FramePipeline::~FramePipeline() = default;

dse::profiler::CPUProfiler& FramePipeline::GetCPUProfiler() { return rs_->cpu_profiler_; }
dse::profiler::RenderProfiler& FramePipeline::GetRenderProfiler() { return rs_->render_profiler_; }
dse::profiler::MemoryProfiler& FramePipeline::GetMemoryProfiler() { return rs_->memory_profiler_; }
dse::render::RenderThinSnapshot& FramePipeline::write_snapshot() { return rs_->snapshot_pool_[snapshot_write_idx_]; }
const dse::render::RenderThinSnapshot& FramePipeline::read_snapshot() const { return rs_->snapshot_pool_[1 - snapshot_write_idx_]; }

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
#include "engine/ecs/components_3d_weather.h"
#include "engine/core/event_bus.h"
#include "engine/core/service_locator.h"
#include "engine/core/job_system.h"
#include "engine/core/memory/memory.h"
#include "engine/core/memory/frame_allocator.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/render/rhi/rhi_factory.h"
#include <glm/gtc/matrix_transform.hpp>
#include "engine/render/rhi/opengl/gl_loader.h"
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

// å•ä¸€æ¥æºæ¶ˆè´¹ï¼šGL430 / VK GLSL450 / HLSL å‡å–è‡ª *_comp.gen.hï¼ˆsrc/*.comp ç¦»çº¿äº¤å‰ç¼–è¯‘ï¼‰ã€‚
// WGSL ä»ç”± builtin_passes.cpp æ‰‹å†™å•ä»½ä¿ç•™ï¼ˆä¸‹æ–¹ externï¼‰ã€‚
#include "engine/render/shaders/generated/embed/hi_z_copy_comp.gen.h"
#include "engine/render/shaders/generated/embed/hi_z_downsample_comp.gen.h"
#include "engine/render/shaders/generated/embed/hi_z_cull_comp.gen.h"
#include "engine/render/shaders/generated/embed/gpu_cull_comp.gen.h"

namespace dse::render {
    // WebGPU æ‰‹è¯‘ WGSLï¼ˆCreateComputeShaderEx ç¬¬ 8 å‚ï¼›å…¶ä½™åŽç«¯å¿½ç•¥ï¼‰ã€‚
    extern const char* kHiZCopyShaderSourceWGSL;
    extern const char* kHiZDownsampleShaderSourceWGSL;
    extern const char* kHiZCullShaderSourceWGSL;
    extern const char* kGPUCullShaderSourceWGSL;
}

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
    // è®¡ç®—é»‘è‰²åŒºåŸŸçš„è¾¹ç•ŒçŸ©å½¢
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
    // 9 ç‚¹ç½‘æ ¼é‡‡æ ·
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
    if (!glGetIntegerv) return;  // éž OpenGL åŽç«¯æ—  GL å‡½æ•°
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

FramePipeline::GpuDrivenPolicy ResolveGpuDrivenPolicy() {
    if (const char* legacy_disable = std::getenv("DSE_DISABLE_GPU_DRIVEN")) {
        if (legacy_disable[0] != '\0' && legacy_disable[0] != '0') {
            return FramePipeline::GpuDrivenPolicy::Off;
        }
    }
    if (const char* policy = std::getenv("DSE_GPU_DRIVEN_POLICY")) {
        if (std::strcmp(policy, "off") == 0) return FramePipeline::GpuDrivenPolicy::Off;
        if (std::strcmp(policy, "force") == 0) return FramePipeline::GpuDrivenPolicy::Force;
        if (std::strcmp(policy, "with_modules") == 0) return FramePipeline::GpuDrivenPolicy::WithModules;
    }
    return FramePipeline::GpuDrivenPolicy::Auto;
}

bool IsProfilePassEnabled(const dse::render::RenderPipelineProfile& profile, const std::string& name) {
    return dse::render::IsRenderPipelinePassEnabled(
        profile, dse::render::BuiltinRenderPipelineRegistry(), name);
}

float PipelineValueToFloat(const dse::render::PipelineValue* value, float fallback) {
    if (!value) return fallback;
    if (const auto* v = std::get_if<double>(value)) return static_cast<float>(*v);
    if (const auto* v = std::get_if<int>(value)) return static_cast<float>(*v);
    return fallback;
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
    auto rhi_backend = dse::render::ValidateRhiBackend(dse::render::ResolveRhiBackendFromEnv());
    runtime_context_.rhi_device = dse::render::CreateRhiDevice(rhi_backend);
    DEBUG_LOG_INFO("FramePipeline RHI åŽç«¯: {}", dse::render::RhiBackendToString(rhi_backend));

    // D3D11 / Vulkan åŽç«¯éœ€è¦ç”¨å¹³å°çª—å£å¥æŸ„å®Œæˆè®¾å¤‡åˆå§‹åŒ–
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
            // è‡ªåŠ¨å›žé€€åˆ° OpenGL
            if (rhi_backend != RhiBackend::OpenGL) {
                DEBUG_LOG_WARN("FramePipeline: {} åŽç«¯åˆå§‹åŒ–å¤±è´¥ï¼Œè‡ªåŠ¨å›žé€€åˆ° OpenGL",
                    dse::render::RhiBackendToString(rhi_backend));
                rhi_backend = RhiBackend::OpenGL;
                runtime_context_.rhi_device = dse::render::CreateRhiDevice(rhi_backend);
                runtime_context_.rhi_device->SetInitKeepAlive(init_keep_alive_);
                DEBUG_LOG_INFO("FramePipeline RHI åŽç«¯ (fallback): OpenGL");
            } else {
                return false;
            }
        } else {
            // ç«‹å³ present ä¸€å¸§é»‘å±ï¼Œæ¶ˆé™¤çª—å£åˆ›å»ºåŽåˆ°é¦–å¸§æ¸²æŸ“å‰çš„ç™½å±
            runtime_context_.rhi_device->BeginFrame();
            runtime_context_.rhi_device->EndFrame();
        }
    }

    lap("RHI device init");
    KeepAlive();
    // è¾“å‡ºå®žé™…æ¸²æŸ“è®¾å¤‡ + è½¯æ¸²æ ‡å¿—ï¼ˆæœºå™¨å¯è§£æžï¼‰ï¼Œä¾›æ€§èƒ½åŸºå‡†åŒºåˆ†ç¡¬ä»¶/è½¯æ¸²ï¼Œ
    // é¿å…æŠŠ WARP/Basic Render Driver/llvmpipe ç­‰è½¯æ¸²æ•°æ®è¯¯å½“ç¡¬ä»¶æ•°æ®ã€‚
    {
        const auto device_info = runtime_context_.rhi_device->GetDeviceInfo();
        DEBUG_LOG_INFO("DSE_RENDER_DEVICE backend={} adapter=\"{}\" software={}",
            dse::render::RhiBackendToString(rhi_backend),
            device_info.adapter_name,
            device_info.is_software ? 1 : 0);
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
    {
        auto profile_result = dse::render::ResolveRenderPipelineProfileFromEnvironment(
            dse::render::RhiBackendToString(rhi_backend), data_root);
        rs_->render_pipeline_profile_ = std::move(profile_result.profile);
        dse::render::RenderPipelineValidationContext validation_context{};
        validation_context.editor_mode = runtime_context_.editor_mode;
        if (const dse::render::RhiDevice* dev = runtime_context_.rhi_device.get()) {
            validation_context.compute_supported = dev->SupportsCompute();
            validation_context.ssbo_supported = dev->SupportsSSBO();
            validation_context.max_color_attachments = dev->GetMaxColorAttachments();
        }
        std::string validation_error;
        if (!dse::render::ValidateRenderPipelineProfile(
                rs_->render_pipeline_profile_, dse::render::BuiltinRenderPipelineRegistry(),
                validation_context, validation_error)) {
            DEBUG_LOG_ERROR("Render pipeline profile '{}' invalid: {}; using ForwardPlusDefault",
                            rs_->render_pipeline_profile_.name, validation_error);
            rs_->render_pipeline_profile_ = dse::render::MakeForwardPlusDefaultProfile();
            profile_result.used_fallback = true;
        }
        if (profile_result.used_fallback) {
            DEBUG_LOG_WARN("Render pipeline profile: {}", profile_result.message);
        } else {
            DEBUG_LOG_INFO("Render pipeline profile: {}", profile_result.message);
        }
        DEBUG_LOG_INFO("{}", dse::render::DumpRenderPipelineProfile(
            rs_->render_pipeline_profile_, dse::render::BuiltinRenderPipelineRegistry(), validation_context));
    }
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
    if (const char* env_scale = std::getenv("DSE_RENDER_SCALE")) {
        float s = static_cast<float>(std::atof(env_scale));
        if (s > 0.0f) Screen::set_render_scale(s);
    }
    InitResolutionDependentRTs();

    // å›ºå®šå°ºå¯¸ RTï¼ˆä¸éšçª—å£ç¼©æ”¾ï¼ŒInit æ—¶åˆ›å»ºä¸€æ¬¡ï¼‰
    if (render_resources_.pp_lum_temp_rt == 0)
        render_resources_.pp_lum_temp_rt = runtime_context_.rhi_device->CreateRenderTarget({64, 64, true, false, false});
    for (int i = 0; i < 2; ++i) {
        if (render_resources_.pp_lum_adapted_rt[i] == 0)
            render_resources_.pp_lum_adapted_rt[i] = runtime_context_.rhi_device->CreateRenderTarget({1, 1, true, false, false});
    }

    // Hi-Z Occlusion Culling shadersï¼ˆä¸ä¾èµ–åˆ†è¾¨çŽ‡ï¼ŒInit æ—¶åˆ›å»ºä¸€æ¬¡ï¼‰
    if (render_resources_.hiz_texture != 0 &&
        render_resources_.hiz_copy_shader == 0 &&
        runtime_context_.rhi_device->SupportsCompute()) {
        render_resources_.hiz_copy_shader = runtime_context_.rhi_device->CreateComputeShaderEx(
            dse::render::generated_shaders::khi_z_copy_comp_glsl430,
            dse::render::generated_shaders::khi_z_copy_comp_glsl450,
            dse::render::generated_shaders::khi_z_copy_comp_hlsl,
            0, 1, 1, 16, dse::render::kHiZCopyShaderSourceWGSL);
        render_resources_.hiz_downsample_shader = runtime_context_.rhi_device->CreateComputeShaderEx(
            dse::render::generated_shaders::khi_z_downsample_comp_glsl430,
            dse::render::generated_shaders::khi_z_downsample_comp_glsl450,
            dse::render::generated_shaders::khi_z_downsample_comp_hlsl,
            0, 2, 0, 32, dse::render::kHiZDownsampleShaderSourceWGSL);
        render_resources_.hiz_cull_shader = runtime_context_.rhi_device->CreateComputeShaderEx(
            dse::render::generated_shaders::khi_z_cull_comp_glsl430,
            dse::render::generated_shaders::khi_z_cull_comp_glsl450,
            dse::render::generated_shaders::khi_z_cull_comp_hlsl,
            2, 0, 1, 112, dse::render::kHiZCullShaderSourceWGSL);
        DEBUG_LOG_INFO("Hi-Z Occlusion Culling initialized: texture={} vis_ssbo={} aabb_ssbo={} capacity={} shaders=({},{},{})",
                       render_resources_.hiz_texture,
                       render_resources_.hiz_visibility_ssbo.raw(),
                       render_resources_.hiz_aabb_ssbo.raw(),
                       render_resources_.hiz_ssbo_capacity,
                       render_resources_.hiz_copy_shader,
                       render_resources_.hiz_downsample_shader,
                       render_resources_.hiz_cull_shader);
    }

    // CSM Shadow Atlas: single 4096Ã—2048 depth texture, cascades rendered via viewport
    // Layout: cascade 0 (2048Ã—2048) at (0,0); cascade 1 (1024Ã—1024) at (2048,0); cascade 2 (512Ã—512) at (3072,0)
    render_resources_.shadow_atlas_render_target = runtime_context_.rhi_device->CreateRenderTarget({4096, 2048, false, true});
    // Legacy per-cascade RTs kept for compatibility (spot/point shadow code paths)
    constexpr int kShadowResolutions[CSM_CASCADES] = {2048, 1024, 512};
    for (int i = 0; i < CSM_CASCADES; ++i) {
        render_resources_.shadow_render_target[i] = render_resources_.shadow_atlas_render_target;
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

    gpu_driven_policy_ = ResolveGpuDrivenPolicy();
    gpu_driven_requested_ = (gpu_driven_policy_ != GpuDrivenPolicy::Off) &&
        rs_->render_pipeline_profile_.settings.gpu_driven;
    gpu_driven_diag_ = [] {
        const char* diag = std::getenv("DSE_GPU_DRIVEN_DIAG");
        return diag && diag[0] != '\0' && diag[0] != '0';
    }();

    // GPU Driven Rendering èƒ½åŠ›æ£€æµ‹
    if (gpu_driven_requested_ &&
        runtime_context_.rhi_device->SupportsCompute() &&
        runtime_context_.rhi_device->SupportsIndirectDraw() &&
        runtime_context_.rhi_device->SupportsSSBO()) {
        render_resources_.gpu_cull_shader = runtime_context_.rhi_device->CreateComputeShaderEx(
            dse::render::generated_shaders::kgpu_cull_comp_glsl430,
            dse::render::generated_shaders::kgpu_cull_comp_glsl450,
            dse::render::generated_shaders::kgpu_cull_comp_hlsl,
            2, 0, 1, 208, dse::render::kGPUCullShaderSourceWGSL);
        render_resources_.gpu_driven_supported = (render_resources_.gpu_cull_shader != 0);
        // äºŒæ¬¡éªŒè¯ï¼šGPU-driven PBR shader ç¼–è¯‘å¯èƒ½å¤±è´¥ï¼ˆå¦‚ HLSL patch ä¸åŒ¹é…ï¼‰
        if (render_resources_.gpu_driven_supported &&
            !runtime_context_.rhi_device->HasGPUDrivenPBRShader()) {
            render_resources_.gpu_driven_supported = false;
            DEBUG_LOG_WARN("GPU Driven Rendering: disabled â€” GPU-driven PBR shader unavailable");
        }
        DEBUG_LOG_INFO("GPU Driven Rendering: supported={}, cull_shader={}",
                       render_resources_.gpu_driven_supported, render_resources_.gpu_cull_shader);
    } else if (!gpu_driven_requested_) {
        render_resources_.gpu_driven_supported = false;
        DEBUG_LOG_INFO("GPU Driven Rendering: requested=false (policy/profile)");
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
#if defined(DSE_ENABLE_3D) && !defined(__EMSCRIPTEN__)
    // Web has no pthreads: Jolt JobSystemThreadPool spawns std::thread and
    // would abort. Web target is 2D-only, so skip 3D physics entirely.
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

    // 2D/3D 双路径的选择封装在 IBuiltinModules 实现内（3D 完整模块或最小 fallback）
    if (enable_gameplay3d) {
        if (!modules_impl_->InitGameplay3D(*runtime_context_.world, runtime_context_.rhi_device.get(), &asset_manager)) {
            DEBUG_LOG_ERROR("FramePipeline init failed: Gameplay3D module OnInit returned false");
        } else {
            DEBUG_LOG_INFO("FramePipeline init: Gameplay3D module OnInit OK (static)");
            builtin_gameplay3d_enabled_ = true;
        }
    }

    // Floating Origin: è®¢é˜… rebase äº‹ä»¶ï¼Œè½¬å‘ç»™ NavMesh / StreamingManager
    {
        auto* event_bus = dse::core::ServiceLocator::Instance().Get<dse::core::EventBus>();
        if (event_bus) {
            rs_->origin_rebase_handle_ = event_bus->Subscribe<dse::core::OriginRebasedEvent>(
                [this](const dse::core::OriginRebasedEvent& evt) {
#ifdef DSE_ENABLE_NAVMESH
                    if (nav_mesh_system_initialized_) {
                        nav_mesh_system_.RebaseOrigin(evt.offset);
                    }
#endif
                    rs_->streaming_manager_.RebaseOrigin(evt.offset);
                });
        }
    }

    lap("systems init");
    KeepAlive();
    DEBUG_LOG_INFO("FramePipeline init: business bootstrap begin");

    runtime_context_.audio_system = &modules_impl_->GetAudioSystem();
    runtime_context_.floating_origin = &rs_->floating_origin_system_;

    const bool business_bootstrap_ok = dse::runtime::BootstrapBusinessRuntime(runtime_context_, {
        [this]() { return LastDrawCalls(); },
        [this]() { return LastMaxBatchSprites(); },
        [this]() { return LastSpriteCount(); },
        [this]() { return LastGpuDrivenActive(); },
        [this]() { return LastGpuIndirectDrawCount(); },
        [this]() { return LastGpuTotalInstances(); }
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

    // Clustered Forward+ å…‰æº SSBO + Cluster ç½‘æ ¼åˆå§‹åŒ–
    rs_->light_buffer_.Init(runtime_context_.rhi_device.get());
    rs_->cluster_grid_.Init(runtime_context_.rhi_device.get());
    lap("light buffer + cluster grid");

    // Light Probe SH Bake ç³»ç»Ÿåˆå§‹åŒ–
    rs_->light_probe_system_.Init(runtime_context_.rhi_device.get());
    lap("LightProbeSystem");

    // Reflection Probe + IBL ç³»ç»Ÿåˆå§‹åŒ–ï¼ˆç”Ÿæˆ BRDF LUTï¼‰
    rs_->reflection_probe_system_.Init(runtime_context_.rhi_device.get());
    lap("ReflectionProbeSystem");

    // DDGI ç³»ç»Ÿå»¶è¿Ÿåˆå§‹åŒ–ï¼ˆé¦–å¸§æ£€æµ‹ GIProbeVolumeComponent åŽæŒ‰éœ€åˆå§‹åŒ–ï¼‰

    // èµ„æºæµå¼åŠ è½½ç®¡ç†å™¨åˆå§‹åŒ–
    rs_->streaming_manager_.Init(&asset_manager);
    {
        auto streaming_shared = std::shared_ptr<dse::streaming::StreamingManager>(&rs_->streaming_manager_, [](auto*) {});
        dse::core::ServiceLocator::Instance().Register<dse::streaming::StreamingManager, dse::streaming::StreamingManager>(streaming_shared);
    }
    lap("StreamingManager");

    // GPU Compute Skinning åˆå§‹åŒ–ï¼ˆCompute Shader å¯ç”¨æ—¶å¯ç”¨ï¼‰
    if (rs_->gpu_skinning_system_.Init(runtime_context_.rhi_device.get())) {
        DEBUG_LOG_INFO("FramePipeline init: GPUSkinningSystem initialized (compute skinning available)");
    }
    lap("GPUSkinning");

    initialized_ = true;
    if (auto* event_bus = dse::core::ServiceLocator::Instance().Get<dse::core::EventBus>()) {
        event_bus->Publish<dse::core::SceneLifecycleEvent>(dse::core::SceneLifecyclePhase::Init);
    }

    // Phase 2: æ¸²æŸ“çº¿ç¨‹åˆ†ç¦»ï¼ˆDSE_RENDER_THREAD=1 å¯ç”¨ï¼Œç¼–è¾‘å™¨æ¨¡å¼ä¸‹ç¦ç”¨ï¼‰
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
    // èµ„æºæµå¼åŠ è½½ç®¡ç†å™¨å…³é—­
    rs_->streaming_manager_.Shutdown();
    dse::core::ServiceLocator::Instance().Reset<dse::streaming::StreamingManager>();

    auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
    render_graph_dag_.Reset();
    dse::runtime::ShutdownBusinessRuntime(runtime_context_);
    modules_impl_->ShutdownGameplay2D(*runtime_context_.world);
    if (builtin_gameplay3d_enabled_) {
        modules_impl_->ShutdownGameplay3D(*runtime_context_.world);
        builtin_gameplay3d_enabled_ = false;
    }

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

    if (runtime_context_.rhi_device) {
        runtime_context_.rhi_device->WaitIdle();
    }

    // Floating Origin: å–æ¶ˆè®¢é˜…
    if (rs_->origin_rebase_handle_.valid) {
        auto* event_bus = dse::core::ServiceLocator::Instance().Get<dse::core::EventBus>();
        if (event_bus) event_bus->Unsubscribe(rs_->origin_rebase_handle_);
        rs_->origin_rebase_handle_ = {};
    }

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
    rs_->gpu_skinning_system_.Shutdown();
    rs_->cluster_grid_.Shutdown();
    rs_->light_buffer_.Shutdown();
    rs_->light_probe_system_.Shutdown();
    rs_->reflection_probe_system_.Shutdown(runtime_context_.rhi_device.get());
    rs_->ddgi_system_.Shutdown(runtime_context_.rhi_device.get());
    taa_pass_ = nullptr;

    asset_manager.ReleaseGpuResources();

    // Hi-Z: é‡Šæ”¾ GPU èµ„æº
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

    // GPU Driven èµ„æºæ¸…ç†
    if (runtime_context_.rhi_device) {
        modules_impl_->CleanupGPUResources(runtime_context_.rhi_device.get());
        if (render_resources_.gpu_draw_cmd_ssbo) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.gpu_draw_cmd_ssbo);
            render_resources_.gpu_draw_cmd_ssbo = {};
        }
        if (render_resources_.gpu_aabb_ssbo) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.gpu_aabb_ssbo);
            render_resources_.gpu_aabb_ssbo = {};
            render_resources_.gpu_aabb_capacity = 0;
        }
        if (render_resources_.gpu_instance_ssbo) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.gpu_instance_ssbo);
            render_resources_.gpu_instance_ssbo = {};
        }
        if (render_resources_.gpu_material_ssbo) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.gpu_material_ssbo);
            render_resources_.gpu_material_ssbo = {};
        }
        if (render_resources_.gpu_indirect_buffer) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.gpu_indirect_buffer);
            render_resources_.gpu_indirect_buffer = {};
        }
        if (render_resources_.gpu_visible_indices_ssbo) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.gpu_visible_indices_ssbo);
            render_resources_.gpu_visible_indices_ssbo = {};
        }
        if (render_resources_.gpu_atomic_counter_ssbo) {
            runtime_context_.rhi_device->DeleteGpuBuffer(render_resources_.gpu_atomic_counter_ssbo);
            render_resources_.gpu_atomic_counter_ssbo = {};
        }
        if (render_resources_.gpu_cull_shader != 0) {
            runtime_context_.rhi_device->DeleteComputeShader(render_resources_.gpu_cull_shader);
            render_resources_.gpu_cull_shader = 0;
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

void FramePipeline::InitResolutionDependentRTs() {
    if (!runtime_context_.rhi_device) return;
    const int display_width  = Screen::width()  > 0 ? Screen::width()  : 1280;
    const int display_height = Screen::height() > 0 ? Screen::height() : 720;
    const int render_width  = Screen::render_width()  > 0 ? Screen::render_width()  : display_width;
    const int render_height = Screen::render_height() > 0 ? Screen::render_height() : display_height;

    if (render_resources_.main_render_target == 0) {
        RenderTargetDesc main_rt_desc{};
        main_rt_desc.width = render_width;
        main_rt_desc.height = render_height;
        main_rt_desc.has_color = true;
        main_rt_desc.has_depth = false;
        render_resources_.main_render_target = runtime_context_.rhi_device->CreateRenderTarget(main_rt_desc);
    }
    if (render_resources_.scene_render_target == 0) {
        RenderTargetDesc scene_desc{};
        scene_desc.width = render_width;
        scene_desc.height = render_height;
        scene_desc.has_color = true;
        scene_desc.has_depth = true;
        scene_desc.msaa_samples = 4;
        render_resources_.scene_render_target = runtime_context_.rhi_device->CreateRenderTarget(scene_desc);
    }
    if (render_resources_.ui_render_target == 0) {
        render_resources_.ui_render_target = runtime_context_.rhi_device->CreateRenderTarget({display_width, display_height, true, false});
    }
    if (render_resources_.prez_render_target == 0) {
        render_resources_.prez_render_target = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, false, true});
    }
    if (render_resources_.pp_bloom_extract_rt == 0) {
        render_resources_.pp_bloom_extract_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    }
    if (render_resources_.pp_bloom_mip_rts.empty()) {
        int mip_w = render_width / 2, mip_h = render_height / 2;
        for (int i = 0; i < 5; ++i) {
            RenderTargetDesc d{}; d.width = mip_w; d.height = mip_h;
            d.has_color = true; d.allow_uav = true;
            render_resources_.pp_bloom_mip_rts.push_back(runtime_context_.rhi_device->CreateRenderTarget(d));
            mip_w = (std::max)(1, mip_w / 2);
            mip_h = (std::max)(1, mip_h / 2);
        }
    }
    if (render_resources_.pp_ssao_rt == 0)
        render_resources_.pp_ssao_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width/2, render_height/2, true, false, false});
    if (render_resources_.pp_ssao_blur_rt == 0)
        render_resources_.pp_ssao_blur_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width/2, render_height/2, true, false, false});
    if (render_resources_.pp_contact_shadow_rt == 0)
        render_resources_.pp_contact_shadow_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width/2, render_height/2, true, false, false});
    if (render_resources_.pp_fxaa_rt == 0)
        render_resources_.pp_fxaa_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    if (render_resources_.pp_taa_rt == 0)
        render_resources_.pp_taa_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    if (render_resources_.pp_dof_rt == 0)
        render_resources_.pp_dof_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    if (render_resources_.pp_ssr_rt == 0)
        render_resources_.pp_ssr_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width/2, render_height/2, true, false, false});
    if (render_resources_.pp_motion_vector_rt == 0)
        render_resources_.pp_motion_vector_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    if (render_resources_.pp_outline_rt == 0)
        render_resources_.pp_outline_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    if (render_resources_.pp_fog_rt == 0)
        render_resources_.pp_fog_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width / 2, render_height / 2, true, false, false});
    if (render_resources_.pp_cloud_rt == 0)
        render_resources_.pp_cloud_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width / 2, render_height / 2, true, false, false});
    if (render_resources_.wboit_accum_rt == 0)
        render_resources_.wboit_accum_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    if (render_resources_.wboit_reveal_rt == 0)
        render_resources_.wboit_reveal_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    if (render_resources_.pp_sss_temp_rt == 0)
        render_resources_.pp_sss_temp_rt = runtime_context_.rhi_device->CreateRenderTarget({render_width, render_height, true, false, false});
    if (render_resources_.hiz_texture == 0 && runtime_context_.rhi_device->SupportsCompute()) {
        render_resources_.hiz_texture = runtime_context_.rhi_device->CreateHiZTexture(render_width, render_height);
        if (render_resources_.hiz_texture != 0) {
            const size_t cap = dse::runtime::RenderPipelineResources::kHiZMaxObjects;
            render_resources_.hiz_visibility_ssbo = runtime_context_.rhi_device->CreateGpuBuffer(
                {cap * sizeof(uint32_t), dse::render::GpuBufferUsage::kStorage, true, "hiz_visibility"}, nullptr);
            render_resources_.hiz_aabb_ssbo = runtime_context_.rhi_device->CreateGpuBuffer(
                {cap * 8 * sizeof(float), dse::render::GpuBufferUsage::kStorage, true, "hiz_aabb"}, nullptr);
            render_resources_.hiz_ssbo_capacity = cap;
        }
    }
}

void FramePipeline::FreeResolutionDependentRTs() {
    if (!runtime_context_.rhi_device) return;
    auto& d = *runtime_context_.rhi_device;
    auto del = [&](unsigned int& h) { if (h) { d.DeleteRenderTarget(h); h = 0; } };
    del(render_resources_.main_render_target);
    del(render_resources_.scene_render_target);
    del(render_resources_.ui_render_target);
    del(render_resources_.prez_render_target);
    del(render_resources_.pp_bloom_extract_rt);
    for (auto& h : render_resources_.pp_bloom_mip_rts) d.DeleteRenderTarget(h);
    render_resources_.pp_bloom_mip_rts.clear();
    del(render_resources_.pp_ssao_rt);
    del(render_resources_.pp_ssao_blur_rt);
    del(render_resources_.pp_contact_shadow_rt);
    del(render_resources_.pp_fxaa_rt);
    del(render_resources_.pp_taa_rt);
    del(render_resources_.pp_dof_rt);
    del(render_resources_.pp_ssr_rt);
    del(render_resources_.pp_motion_vector_rt);
    del(render_resources_.pp_outline_rt);
    del(render_resources_.pp_fog_rt);
    del(render_resources_.pp_cloud_rt);
    del(render_resources_.wboit_accum_rt);
    del(render_resources_.wboit_reveal_rt);
    del(render_resources_.pp_sss_temp_rt);
    if (render_resources_.hiz_texture) {
        d.DeleteHiZTexture(render_resources_.hiz_texture);
        render_resources_.hiz_texture = 0;
        if (render_resources_.hiz_visibility_ssbo) {
            d.DeleteGpuBuffer(render_resources_.hiz_visibility_ssbo);
            render_resources_.hiz_visibility_ssbo = {};
        }
        if (render_resources_.hiz_aabb_ssbo) {
            d.DeleteGpuBuffer(render_resources_.hiz_aabb_ssbo);
            render_resources_.hiz_aabb_ssbo = {};
        }
        render_resources_.hiz_ssbo_capacity = 0;
    }
}

void FramePipeline::OnWindowResize(int w, int h) {
    if (!initialized_ || !runtime_context_.rhi_device) return;
    if (w <= 0 || h <= 0) return;
    Screen::set_width_height(w, h);
    runtime_context_.rhi_device->WaitIdle();
    FreeResolutionDependentRTs();
    InitResolutionDependentRTs();
    SyncRenderPassContextTargets();
    runtime_context_.rhi_device->OnWindowResized(w, h);
    DEBUG_LOG_INFO("FramePipeline::OnWindowResize: {}x{}", w, h);
}

void FramePipeline::SyncRenderPassContextTargets() {
    render_pass_context_.render_targets.main     = render_resources_.main_render_target;
    render_pass_context_.render_targets.scene    = render_resources_.scene_render_target;
    render_pass_context_.render_targets.ui       = render_resources_.ui_render_target;
    render_pass_context_.render_targets.prez     = render_resources_.prez_render_target;
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
    render_pass_context_.render_targets.cloud  = render_resources_.pp_cloud_rt;
    render_pass_context_.render_targets.wboit_accum = render_resources_.wboit_accum_rt;
    render_pass_context_.render_targets.wboit_reveal = render_resources_.wboit_reveal_rt;
    render_pass_context_.render_targets.sss_temp = render_resources_.pp_sss_temp_rt;
    render_pass_context_.render_targets.hiz_texture = render_resources_.hiz_texture;
    render_pass_context_.hiz_visibility_ssbo = render_resources_.hiz_visibility_ssbo;
    render_pass_context_.hiz_aabb_ssbo = render_resources_.hiz_aabb_ssbo;
    render_pass_context_.hiz_aabb_capacity = render_resources_.hiz_ssbo_capacity;
}

void FramePipeline::Update(const dse::FrameUpdateContext& frame) {
    if (!initialized_) {
        return;
    }
    dse::runtime::RunFrameUpdate(*this, frame);
}

void FramePipeline::Update(float delta_time) {
    Update(dse::FrameUpdateContext{dse::TimeContext{delta_time, delta_time, 1.0f}, 0});
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
        // æ¸²æŸ“çº¿ç¨‹å·²æ¶ˆè´¹å®Œä¸Šä¸€å¸§å¿«ç…§ï¼ˆå«å…¶å¸§åˆ†é…å™¨ç¼“å†²ï¼‰ï¼Œæ­¤å¤„æŽ¨è¿›+å¤ä½æ‰å®‰å…¨ï¼ˆè§è®¾è®¡æ–‡æ¡£ Â§3.5ï¼‰ã€‚
        dse::core::Memory::Frame().BeginFrame();
        PrepareRenderFrame();
        SignalRenderThread();
    } else {
        // å•çº¿ç¨‹ï¼šæœ¬å¸§åŒæ­¥æ¶ˆè´¹ï¼Œå¸§é¦–å¤ä½å³å¯ã€‚
        dse::core::Memory::Frame().BeginFrame();
        dse::runtime::RunFrameRender(*this);
    }
}

void FramePipeline::RunUpdateInternal(const dse::FrameUpdateContext& frame) {
    const dse::TimeContext& time = frame.time;
    // ç¼©æ”¾é€šé“ä¾› gameplay/ä¸šåŠ¡é€»è¾‘ä½¿ç”¨ï¼›çœŸå®žé€šé“ä¾›èµ„æºæµå¼åŠ è½½/åœºæ™¯æµåŠ è½½ä½¿ç”¨ï¼ˆä¸éšæš‚åœå†»ç»“ï¼‰ã€‚
    const float delta_time = time.scaled_dt;
    dse::profiler::ScopedCPUProfile _profile_update(rs_->cpu_profiler_, "FramePipeline::Update");
    auto update_begin = std::chrono::high_resolution_clock::now();
    auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
    {
        dse::profiler::ScopedCPUProfile _p(rs_->cpu_profiler_, "AssetManager::PumpCallbacks");
        asset_manager.PumpMainThreadCallbacks(callback_budget_per_frame_);
    }
    asset_manager.PumpHotReloads();

    // èµ„æºæµå¼åŠ è½½ï¼šèŽ·å–æ‘„åƒæœºä½ç½®å¹¶ tick
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
        {
            dse::profiler::ScopedCPUProfile _p(rs_->cpu_profiler_, "StreamingManager::Tick");
            rs_->streaming_manager_.Tick(streaming_cam_pos);
        }
    }

    // SceneManager: pump å¼‚æ­¥åŠ è½½å®Œæˆçš„å­åœºæ™¯
    if (auto* sm = dse::core::ServiceLocator::Instance().Get<scene::SceneManager>()) {
        sm->Update(time.unscaled_dt);

        if (runtime_context_.world) {
            auto sub_view = runtime_context_.world->registry().view<dse::SubSceneComponent>();
            for (auto entity : sub_view) {
                auto& sub = sub_view.get<dse::SubSceneComponent>(entity);
                if (!sub.enabled || sub.scene_path.empty()) {
                    continue;
                }
                if (!sm->IsSubSceneLoaded(sub.scene_path)) {
                    sm->LoadSubSceneAsync(sub.scene_path);
                }
            }
        }
    }

    // AssetManager å†…å­˜ â†’ MemoryProfilerï¼ˆæ¯å¸§è¿½è¸ª deltaï¼‰
    if (runtime_context_.asset_manager) {
        std::size_t current = runtime_context_.asset_manager->EstimatedMemoryUsage();
        if (current > last_reported_asset_memory_) {
            rs_->memory_profiler_.RecordAlloc("AssetManager", current - last_reported_asset_memory_);
        } else if (current < last_reported_asset_memory_) {
            rs_->memory_profiler_.RecordFree("AssetManager", last_reported_asset_memory_ - current);
        }
        last_reported_asset_memory_ = current;
    }

    dse::runtime::TickBusinessRuntime(runtime_context_, delta_time);

    dse::runtime::RunRuntimeUpdateGraph(*this, frame);

    auto update_end = std::chrono::high_resolution_clock::now();
    update_time_accumulator_ms_ += std::chrono::duration<float, std::milli>(update_end - update_begin).count();
    update_samples_ += 1;
}

void FramePipeline::RunFixedUpdateInternal(float fixed_delta_time) {
    dse::profiler::ScopedCPUProfile _profile_fixed(rs_->cpu_profiler_, "FramePipeline::FixedUpdate");
    auto fixed_begin = std::chrono::high_resolution_clock::now();
    dse::runtime::RunRuntimeFixedUpdateGraph(*this, fixed_delta_time);
    
    auto fixed_end = std::chrono::high_resolution_clock::now();
    fixed_time_accumulator_ms_ += std::chrono::duration<float, std::milli>(fixed_end - fixed_begin).count();
    fixed_samples_ += 1;
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

int FramePipeline::LastGpuDrivenActive() const {
    return last_gpu_driven_active_;
}

int FramePipeline::LastGpuIndirectDrawCount() const {
    return last_gpu_indirect_draw_count_;
}

int FramePipeline::LastGpuTotalInstances() const {
    return last_gpu_total_instances_;
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

void FramePipeline::SetEditorBgColor(const glm::vec4& color) {
    render_pass_context_.editor_bg_color = color;
}

void FramePipeline::DisableEditorCamera() {
    render_pass_context_.use_editor_camera = false;
}

void FramePipeline::SetSceneViewMode(int mode) {
    render_pass_context_.scene_view_mode = mode;
}

unsigned int FramePipeline::RenderSceneWithCamera(const glm::mat4& view, const glm::mat4& projection) {
    if (!initialized_ || !runtime_context_.rhi_device) return 0;

    // ä¿å­˜å½“å‰ç¼–è¾‘å™¨ç›¸æœºçŠ¶æ€
    const bool saved_use = render_pass_context_.use_editor_camera;
    const glm::mat4 saved_view = render_pass_context_.editor_view;
    const glm::mat4 saved_proj = render_pass_context_.editor_projection;

    // è®¾ç½®ä¸´æ—¶ç›¸æœº
    render_pass_context_.use_editor_camera = true;
    render_pass_context_.editor_view = view;
    render_pass_context_.editor_projection = projection;

    // åˆ›å»ºå‘½ä»¤ç¼“å†²ï¼Œæ‰§è¡Œ shadow pass + scene pass
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

    // æ¢å¤ç›¸æœº
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
// Phase 1 è–„å¿«ç…§ï¼šä¸€æ¬¡æ€§æå–æ¸²æŸ“çº¿ç¨‹æ‰€éœ€çš„å…¨éƒ¨ ECS æ•°æ®
// ============================================================


