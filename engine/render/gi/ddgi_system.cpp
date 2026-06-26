#include "engine/render/gi/ddgi_system.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"
#include <cstring>
#include <cmath>

// 单一来源消费：GL430 / VK GLSL450 / HLSL 取自 *_comp.gen.h（src/ddgi_probe_update.comp 离线交叉编译）。
// DDGI 无手译 WGSL（生产路径 WebGPU 由进阶前向 WGSL 承载），故 7 参创建。
#include "engine/render/shaders/generated/embed/ddgi_probe_update_comp.gen.h"

namespace dse {
namespace render {
namespace gi {

// ============================================================================
// DDGISystem 实现
// ============================================================================

bool DDGISystem::Init(RhiDevice* rhi, const DDGIVolumeConfig& config) {
    if (!rhi) return false;
    if (!rhi->SupportsCompute()) {
        DEBUG_LOG_WARN("[DDGI] Compute shader not supported, DDGI disabled");
        return false;
    }

    config_ = config;

    if (!CreateAtlasTextures(rhi)) return false;
    if (!CreateComputeShader(rhi)) return false;
    InitProbeStates(rhi);

    // 每帧更新的探针数：总数/8（~8帧更新一轮），最小 1
    int total = config_.TotalProbeCount();
    probes_per_frame_ = std::max(1, total / 8);

    resources_.initialized = true;
    DEBUG_LOG_INFO("[DDGI] Initialized: {}x{}x{} probes ({} total), atlas {}x{}, {}/frame update",
                  config_.resolution.x, config_.resolution.y, config_.resolution.z,
                  total,
                  config_.IrradianceAtlasSize().x, config_.IrradianceAtlasSize().y,
                  probes_per_frame_);
    return true;
}

void DDGISystem::Shutdown(RhiDevice* rhi) {
    if (!rhi) return;

    if (resources_.irradiance_atlas != 0) {
        rhi->DeleteTexture(resources_.irradiance_atlas);
        resources_.irradiance_atlas = 0;
    }
    if (resources_.visibility_atlas != 0) {
        rhi->DeleteTexture(resources_.visibility_atlas);
        resources_.visibility_atlas = 0;
    }
    if (resources_.probe_state_ssbo) {
        rhi->DeleteGpuBuffer(resources_.probe_state_ssbo);
        resources_.probe_state_ssbo = {};
    }
    if (resources_.update_compute_shader != 0) {
        rhi->DeleteComputeShader(resources_.update_compute_shader);
        resources_.update_compute_shader = 0;
    }

    resources_.initialized = false;
    current_update_offset_ = 0;
    frame_counter_ = 0;
}

void DDGISystem::Reconfigure(RhiDevice* rhi, const DDGIVolumeConfig& config) {
    Shutdown(rhi);
    Init(rhi, config);
}

void DDGISystem::EnsureRSMResources(RhiDevice* rhi) {
    if (!rhi || !resources_.initialized) return;

    // RSM RTs 由 RenderPassContext 管理（在 FramePipeline 中创建）
    // 这里只做存在性检查
}

void DDGISystem::UpdateProbes(RhiDevice* rhi,
                               unsigned int rsm_position, unsigned int rsm_normal, unsigned int rsm_flux,
                               int rsm_width, int rsm_height,
                               const glm::vec3& light_dir, const glm::vec3& light_color) {
    if (!rhi || !resources_.initialized) return;
    if (resources_.update_compute_shader == 0) return;
    if (resources_.irradiance_atlas == 0 || resources_.visibility_atlas == 0) return;
    if (rsm_width <= 0 || rsm_height <= 0) return;
    if (rsm_position == 0 || rsm_normal == 0 || rsm_flux == 0) return;

    int total_probes = config_.TotalProbeCount();
    int probes_this_frame = std::min(probes_per_frame_, total_probes);

    // Bind resources
    rhi->SetComputeTextureImage(0, resources_.irradiance_atlas, false);
    rhi->SetComputeTextureImage(1, resources_.visibility_atlas, false);

    // RSM textures as samplers (externally managed by FramePipeline)
    rhi->SetComputeTextureSampler(0, rsm_position);
    rhi->SetComputeTextureSampler(1, rsm_normal);
    rhi->SetComputeTextureSampler(2, rsm_flux);

    // Probe state SSBO
    rhi->BindGpuBuffer(resources_.probe_state_ssbo, 0);

    // Set uniforms
    unsigned int shader = resources_.update_compute_shader;
    rhi->SetComputeUniformInt(shader, "u_probe_count", total_probes);
    rhi->SetComputeUniformInt(shader, "u_probe_start", current_update_offset_);
    rhi->SetComputeUniformInt(shader, "u_probes_to_update", probes_this_frame);
    rhi->SetComputeUniformInt(shader, "u_irradiance_texels", config_.irradiance_texels);
    rhi->SetComputeUniformInt(shader, "u_visibility_texels", config_.visibility_texels);
    rhi->SetComputeUniformInt(shader, "u_rsm_width", rsm_width);
    rhi->SetComputeUniformInt(shader, "u_rsm_height", rsm_height);
    rhi->SetComputeUniformInt(shader, "u_frame_index", frame_counter_);
    rhi->SetComputeUniformFloat(shader, "u_hysteresis", config_.hysteresis);

    // grid 参数
    glm::vec3 spacing = config_.ProbeSpacing();
    rhi->SetComputeUniformIVec3(shader, "u_grid_resolution",
                                 config_.resolution.x, config_.resolution.y, config_.resolution.z);
    rhi->SetComputeUniformVec3(shader, "u_grid_origin",
                                config_.origin.x, config_.origin.y, config_.origin.z);
    rhi->SetComputeUniformVec3(shader, "u_grid_spacing",
                                spacing.x, spacing.y, spacing.z);
    rhi->SetComputeUniformVec3(shader, "u_light_dir",
                                light_dir.x, light_dir.y, light_dir.z);
    rhi->SetComputeUniformVec3(shader, "u_light_color",
                                light_color.x, light_color.y, light_color.z);

    // Dispatch: X = irradiance_texels, Y = probes_this_frame
    unsigned int groups_x = static_cast<unsigned int>((config_.irradiance_texels + 7) / 8);
    unsigned int groups_y = static_cast<unsigned int>((probes_this_frame + 7) / 8);
    rhi->DispatchCompute(shader, groups_x, groups_y, 1);
    rhi->ComputeMemoryBarrier();

    // 推进级联轮转
    current_update_offset_ = (current_update_offset_ + probes_this_frame) % total_probes;
    frame_counter_++;
}

void DDGISystem::BindForSampling(RhiDevice* rhi, unsigned int irradiance_unit,
                                  unsigned int visibility_unit) const {
    if (!rhi || !resources_.initialized) return;
    if (resources_.irradiance_atlas != 0) {
        rhi->SetComputeTextureSampler(irradiance_unit, resources_.irradiance_atlas);
    }
    if (resources_.visibility_atlas != 0) {
        rhi->SetComputeTextureSampler(visibility_unit, resources_.visibility_atlas);
    }
}

// ============================================================================
// 私有方法
// ============================================================================

bool DDGISystem::CreateAtlasTextures(RhiDevice* rhi) {
    glm::ivec2 irr_size = config_.IrradianceAtlasSize();
    glm::ivec2 vis_size = config_.VisibilityAtlasSize();

    // 创建可供 compute shader 写入的 atlas（含 storage image / UAV 标志）
    resources_.irradiance_atlas = rhi->CreateComputeWriteTexture2D(irr_size.x, irr_size.y);
    if (resources_.irradiance_atlas == 0) {
        DEBUG_LOG_ERROR("[DDGI] Failed to create irradiance atlas ({}x{})", irr_size.x, irr_size.y);
        return false;
    }

    resources_.visibility_atlas = rhi->CreateComputeWriteTexture2D(vis_size.x, vis_size.y);
    if (resources_.visibility_atlas == 0) {
        DEBUG_LOG_ERROR("[DDGI] Failed to create visibility atlas ({}x{})", vis_size.x, vis_size.y);
        return false;
    }

    return true;
}

bool DDGISystem::CreateComputeShader(RhiDevice* rhi) {
    // ssbo=1(ProbeStates), img=2(irradiance+visibility), smp=3(RSM×3), pc=96B
    resources_.update_compute_shader = rhi->CreateComputeShaderEx(
        generated_shaders::kddgi_probe_update_comp_glsl430,
        generated_shaders::kddgi_probe_update_comp_glsl450,
        generated_shaders::kddgi_probe_update_comp_hlsl,
        /*ssbo_count=*/1,
        /*storage_image_count=*/2,
        /*sampler_count=*/3,
        /*push_constant_bytes=*/224);
    if (resources_.update_compute_shader == 0) {
        DEBUG_LOG_ERROR("[DDGI] Failed to compile probe update compute shader");
        return false;
    }
    return true;
}

void DDGISystem::InitProbeStates(RhiDevice* rhi) {
    int total = config_.TotalProbeCount();
    std::vector<ProbeState> states(total);

    for (int i = 0; i < total; ++i) {
        glm::vec3 pos = config_.ProbePosition(i);
        states[i].position_and_status = glm::vec4(pos, 1.0f);  // 默认全部激活
    }

    size_t byte_size = total * sizeof(ProbeState);
    GpuBufferDesc desc{byte_size, GpuBufferUsage::kStorage, true, "ddgi_probe_state"};
    resources_.probe_state_ssbo = rhi->CreateGpuBuffer(desc, states.data());
    if (!resources_.probe_state_ssbo) {
        DEBUG_LOG_ERROR("[DDGI] Failed to create probe state SSBO ({} probes)", total);
    }
}

} // namespace gi
} // namespace render
} // namespace dse
