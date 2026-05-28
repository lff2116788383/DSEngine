#include "morph_target_system.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"

#include "engine/render/shaders/generated/embed/morph_target_comp.gen.h"

namespace dse {
namespace render {

// Vulkan 需要原始 GLSL 450 源（保留 layout(push_constant)），
// 而 gen.h 中的 GL430 交叉编译版本已将 push_constant 转为 uniform block，
// 导致 CPU push constant 写入无法到达 shader。
static const char* kMorphTargetCompVulkan = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(local_size_x = 256) in;
struct BaseVertex { vec4 position; vec4 normal; vec4 tangent; };
layout(std430, binding = 0) readonly buffer BaseVertices { BaseVertex base_vertices[]; };
struct MorphDelta { vec4 delta_pos; vec4 delta_normal; };
layout(std430, binding = 1) readonly buffer MorphDeltas { MorphDelta morph_deltas[]; };
layout(std430, binding = 2) readonly buffer MorphWeights { float morph_weights[]; };
struct DeformedVertex { vec4 position; vec4 normal; vec4 tangent; };
layout(std430, binding = 3) writeonly buffer DeformedVertices { DeformedVertex deformed_vertices[]; };
layout(push_constant) uniform MorphParams { int u_vertex_count; int u_target_count; };
void main() {
    uint vid = gl_GlobalInvocationID.x;
    if (vid >= uint(u_vertex_count)) return;
    vec3 pos = base_vertices[vid].position.xyz;
    vec3 nrm = base_vertices[vid].normal.xyz;
    vec4 tan = base_vertices[vid].tangent;
    for (int t = 0; t < u_target_count; ++t) {
        float w = morph_weights[t];
        if (abs(w) < 0.0001) continue;
        uint delta_idx = uint(t) * uint(u_vertex_count) + vid;
        pos += morph_deltas[delta_idx].delta_pos.xyz * w;
        nrm += morph_deltas[delta_idx].delta_normal.xyz * w;
    }
    nrm = normalize(nrm);
    deformed_vertices[vid].position = vec4(pos, 1.0);
    deformed_vertices[vid].normal = vec4(nrm, 0.0);
    deformed_vertices[vid].tangent = tan;
}
)";

bool MorphTargetSystem::Init(RhiDevice* device) {
    if (!device) return false;
    device_ = device;

    if (!device_->SupportsCompute()) {
        DEBUG_LOG_WARN("[MorphTargetSystem] Compute shaders not supported; morph targets disabled");
        return false;
    }

    using namespace generated_shaders;
    compute_program_ = device_->CreateComputeShaderEx(
        kmorph_target_comp_glsl430,   // GL 430 (push_constant → uniform block _20)
        kMorphTargetCompVulkan,       // Vulkan GLSL 450 (保留 push_constant)
        kmorph_target_comp_hlsl,      // DX11 HLSL
        4,   // ssbo_count: base, deltas, weights, output
        0,   // storage_image_count
        0,   // sampler_count
        8);  // push_constant_bytes: 2 × int = 8 bytes
    if (compute_program_ == 0) {
        DEBUG_LOG_WARN("[MorphTargetSystem] Compute shader compilation failed; morph targets disabled");
        return false;
    }

    available_ = true;
    DEBUG_LOG_INFO("[MorphTargetSystem] Initialized successfully");
    return true;
}

void MorphTargetSystem::Shutdown() {
    if (compute_program_ != 0 && device_) {
        device_->DeleteComputeShader(compute_program_);
        compute_program_ = 0;
    }
    available_ = false;
}

void MorphTargetSystem::UploadIfDirty(MorphTargetComponent& comp) {
    if (!available_ || !comp.gpu_dirty) return;
    if (comp.vertex_count <= 0 || comp.targets.empty()) return;

    const int target_count = static_cast<int>(comp.targets.size());
    const int vertex_count = comp.vertex_count;

    if (comp.weights.size() != comp.targets.size()) {
        comp.weights.resize(comp.targets.size(), 0.0f);
    }

    // Create/upload weight SSBO
    const size_t weight_bytes = target_count * sizeof(float);
    if (!comp.gpu_weight_buffer) {
        GpuBufferDesc desc;
        desc.size = weight_bytes;
        desc.usage = GpuBufferUsage::kStorage;
        comp.gpu_weight_buffer = device_->CreateGpuBuffer(desc, comp.weights.data());
    } else {
        device_->UpdateGpuBuffer(comp.gpu_weight_buffer, 0, weight_bytes, comp.weights.data());
    }

    // Create output SSBO if needed (DeformedVertex = 3 × vec4 = 48 bytes)
    if (!comp.gpu_output_buffer) {
        GpuBufferDesc desc;
        desc.size = static_cast<size_t>(vertex_count) * 48;
        desc.usage = GpuBufferUsage::kStorage;
        comp.gpu_output_buffer = device_->CreateGpuBuffer(desc, nullptr);
    }

    comp.gpu_dirty = false;
}

void MorphTargetSystem::Dispatch(MorphTargetComponent& comp) {
    if (!available_) return;
    if (comp.vertex_count <= 0 || comp.targets.empty()) return;
    if (!comp.gpu_output_buffer) return;

    bool any_active = false;
    for (float w : comp.weights) {
        if (std::abs(w) > 0.0001f) { any_active = true; break; }
    }
    if (!any_active) return;

    const int vertex_count = comp.vertex_count;
    const int target_count = static_cast<int>(comp.targets.size());
    const unsigned int workgroups = static_cast<unsigned int>((vertex_count + 255) / 256);

    device_->BindGpuBuffer(comp.gpu_base_buffer, 0);
    device_->BindGpuBuffer(comp.gpu_delta_buffer, 1);
    device_->BindGpuBuffer(comp.gpu_weight_buffer, 2);
    device_->BindGpuBuffer(comp.gpu_output_buffer, 3, true);

    device_->SetComputeUniformInt(compute_program_, "_20.u_vertex_count", vertex_count);
    device_->SetComputeUniformInt(compute_program_, "_20.u_target_count", target_count);

    device_->DispatchCompute(compute_program_, workgroups, 1, 1);
    device_->ComputeMemoryBarrier();
}

} // namespace render
} // namespace dse
