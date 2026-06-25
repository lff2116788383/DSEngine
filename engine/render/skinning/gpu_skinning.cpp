#include "engine/render/skinning/gpu_skinning.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/base/debug.h"
#include <cstring>
#include <algorithm>

// 尝试包含生成的 shader 数据（若已编译则可用）
#if __has_include("engine/render/shaders/generated/embed/skinning_comp.gen.h")
#include "engine/render/shaders/generated/embed/skinning_comp.gen.h"
#define DSE_HAS_SKINNING_SHADER_GEN 1
#else
#define DSE_HAS_SKINNING_SHADER_GEN 0
#endif

namespace dse {
namespace render {

// P3: 统一 GLSL 源 — 使用 InstanceInfo SSBO 实现单次 Dispatch（P2）
// Vulkan 使用 push_constant，GL 430 通过运行时字符串替换生成 uniform 版本
static const char* kSkinningComputeVK = R"(
#version 450
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct SrcVertex { vec4 pos_bw0; vec4 norm_bw1; vec4 tan_bw2; vec4 joints_bw3; };
struct DstVertex { vec4 pos; vec4 normal; vec4 tangent; };
struct InstanceInfo {
    uint vertex_start;
    uint vertex_count;
    uint bone_offset;
    uint morph_target_count;
    vec4 morph_weights;
    uint morph_delta_offset;
    uint _pad0; uint _pad1; uint _pad2;
};

layout(std430, binding = 0) readonly buffer SrcBuf { SrcVertex src_vertices[]; };
layout(std430, binding = 1) writeonly buffer DstBuf { DstVertex dst_vertices[]; };
layout(std430, binding = 2) readonly buffer BoneBuf { mat4 bone_matrices[]; };
layout(std430, binding = 3) readonly buffer MorphBuf { vec4 morph_deltas[]; };
layout(std430, binding = 4) readonly buffer InstBuf { InstanceInfo instances[]; };

layout(push_constant) uniform PC { uint total_vertices; uint instance_count; } params;

uint findInstance(uint gid) {
    uint lo = 0u, hi = params.instance_count;
    while (lo < hi) {
        uint mid = (lo + hi) / 2u;
        if (instances[mid].vertex_start + instances[mid].vertex_count <= gid) lo = mid + 1u;
        else hi = mid;
    }
    return lo;
}

void main() {
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= params.total_vertices) return;

    uint inst_id = findInstance(gid);
    if (inst_id >= params.instance_count) return;
    InstanceInfo inst = instances[inst_id];
    uint local_vid = gid - inst.vertex_start;

    SrcVertex src = src_vertices[gid];
    vec3 pos = src.pos_bw0.xyz;
    vec3 normal = src.norm_bw1.xyz;
    vec3 tangent = src.tan_bw2.xyz;

    if (inst.morph_target_count > 0u) {
        float w[4] = float[4](inst.morph_weights.x, inst.morph_weights.y,
                              inst.morph_weights.z, inst.morph_weights.w);
        uint mbase = inst.morph_delta_offset;
        for (uint m = 0u; m < inst.morph_target_count && m < 4u; ++m) {
            if (abs(w[m]) > 0.001) {
                pos += morph_deltas[mbase + m * inst.vertex_count + local_vid].xyz * w[m];
            }
        }
    }

    float bw0 = src.pos_bw0.w, bw1 = src.norm_bw1.w, bw2 = src.tan_bw2.w;
    float bw3 = 1.0 - bw0 - bw1 - bw2;
    int bi0 = int(src.joints_bw3.x), bi1 = int(src.joints_bw3.y);
    int bi2 = int(src.joints_bw3.z), bi3 = int(src.joints_bw3.w);
    uint bb = inst.bone_offset;

    mat4 sm = bone_matrices[bb + bi0] * bw0 + bone_matrices[bb + bi1] * bw1
            + bone_matrices[bb + bi2] * bw2 + bone_matrices[bb + bi3] * bw3;

    dst_vertices[gid].pos     = vec4((sm * vec4(pos, 1.0)).xyz, 1.0);
    mat3 nm = mat3(sm);
    dst_vertices[gid].normal  = vec4(normalize(nm * normal), 0.0);
    dst_vertices[gid].tangent = vec4(normalize(nm * tangent), 0.0);
}
)";

// P3: 从 VK 源生成 GL 430 版本（替换 push_constant → uniform）
static std::string GenerateGL430Source() {
    std::string src = kSkinningComputeVK;
    // #version
    { auto p = src.find("#version 450"); if (p != std::string::npos) src.replace(p, 12, "#version 430"); }
    // push_constant block → uniform (用 int 而非 uint，因为 glUniform1i 不适用于 uint 类型)
    {
        auto p = src.find("layout(push_constant)");
        if (p != std::string::npos) {
            auto end = src.find("} params;", p);
            if (end != std::string::npos) {
                src.replace(p, end + 9 - p,
                    "uniform int u_total_vertices;\nuniform int u_instance_count;");
            }
        }
    }
    // params.xxx → u_xxx
    { size_t p = 0; while ((p = src.find("params.total_vertices", p)) != std::string::npos) src.replace(p, 21, "u_total_vertices"); }
    { size_t p = 0; while ((p = src.find("params.instance_count", p)) != std::string::npos) src.replace(p, 21, "u_instance_count"); }
    return src;
}

// HLSL CS 5.0 — P2: 与 GLSL 逻辑一致的单次 Dispatch 版本
static const char* kSkinningComputeHLSL = R"(
struct SrcVertex { float4 pos_bw0; float4 norm_bw1; float4 tan_bw2; float4 joints_bw3; };
struct DstVertex { float4 pos; float4 normal; float4 tangent; };
struct InstanceInfo { uint vertex_start; uint vertex_count; uint bone_offset; uint morph_target_count; float4 morph_weights; uint morph_delta_offset; uint _pad0; uint _pad1; uint _pad2; };

ByteAddressBuffer src_vertices : register(t16);
RWByteAddressBuffer dst_vertices : register(u1);
ByteAddressBuffer bone_matrices : register(t18);
ByteAddressBuffer morph_deltas : register(t19);
StructuredBuffer<InstanceInfo> instances : register(t20);

cbuffer SkinningParams : register(b0) { uint u_total_vertices; uint u_instance_count; };

float4x4 LoadBoneMatrix(uint idx) {
    uint b = idx * 64;
    return float4x4(asfloat(bone_matrices.Load4(b)), asfloat(bone_matrices.Load4(b+16)),
                    asfloat(bone_matrices.Load4(b+32)), asfloat(bone_matrices.Load4(b+48)));
}
SrcVertex LoadSrc(uint idx) {
    uint b = idx * 64;
    SrcVertex v; v.pos_bw0 = asfloat(src_vertices.Load4(b));
    v.norm_bw1 = asfloat(src_vertices.Load4(b+16));
    v.tan_bw2 = asfloat(src_vertices.Load4(b+32));
    v.joints_bw3 = asfloat(src_vertices.Load4(b+48)); return v;
}
void StoreDst(uint idx, DstVertex v) {
    uint b = idx * 48;
    dst_vertices.Store4(b, asuint(v.pos));
    dst_vertices.Store4(b+16, asuint(v.normal));
    dst_vertices.Store4(b+32, asuint(v.tangent));
}

uint findInstance(uint gid) {
    uint lo = 0, hi = u_instance_count;
    while (lo < hi) { uint mid = (lo+hi)/2;
        if (instances[mid].vertex_start + instances[mid].vertex_count <= gid) lo = mid+1; else hi = mid;
    } return lo;
}

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint gid = dtid.x;
    if (gid >= u_total_vertices) return;
    uint inst_id = findInstance(gid);
    if (inst_id >= u_instance_count) return;
    InstanceInfo inst = instances[inst_id];
    uint local_vid = gid - inst.vertex_start;

    SrcVertex src = LoadSrc(gid);
    float3 pos = src.pos_bw0.xyz, nrm = src.norm_bw1.xyz, tan_ = src.tan_bw2.xyz;

    if (inst.morph_target_count > 0) {
        float w[4] = {inst.morph_weights.x, inst.morph_weights.y, inst.morph_weights.z, inst.morph_weights.w};
        uint mbase = inst.morph_delta_offset;
        for (uint m = 0; m < inst.morph_target_count && m < 4; ++m) {
            if (abs(w[m]) > 0.001) { pos += asfloat(morph_deltas.Load4((mbase + m*inst.vertex_count+local_vid)*16)).xyz * w[m]; }
        }
    }

    float bw0 = src.pos_bw0.w, bw1 = src.norm_bw1.w, bw2 = src.tan_bw2.w, bw3 = 1.0-bw0-bw1-bw2;
    uint bb = inst.bone_offset;
    float4x4 sm = LoadBoneMatrix(bb+(int)src.joints_bw3.x)*bw0 + LoadBoneMatrix(bb+(int)src.joints_bw3.y)*bw1
                + LoadBoneMatrix(bb+(int)src.joints_bw3.z)*bw2 + LoadBoneMatrix(bb+(int)src.joints_bw3.w)*bw3;

    DstVertex d;
    d.pos = float4(mul(float4(pos,1.0), sm).xyz, 1.0);
    float3x3 nm_ = (float3x3)sm;
    d.normal = float4(normalize(mul(nrm, nm_)), 0.0);
    d.tangent = float4(normalize(mul(tan_, nm_)), 0.0);
    StoreDst(gid, d);
}
)";

// WebGPU 手译 WGSL（无离线 GLSL→WGSL 工具）：与上方 VK GLSL 450 / B3b-3 蒙皮离屏自检
//   （webgpu_rhi_device.cpp::kSkinningWGSL）算法逐句一致——单次 Dispatch、InstanceInfo 二分定位、
//   morph delta 叠加、4 骨权重混合。差异仅在参数传递：自检用真实 UBO(group1/b0)，真实消费方走命名
//   uniform（SetComputeUniformInt，见下方 Dispatch）→ 经 group1 保留 binding 8、成员按调用序 16B 对齐
//   （u_total_vertices@0、u_instance_count@16），与 morph_target_system.cpp::kMorphTargetCompWGSL 同方案。
//   i32 成员匹配 SetComputeUniformInt，算法内 cast u32。输入 SSBO 声明 read_write 以匹配 WebGPU RHI
//   compute 统一 BufferBindingType_Storage 布局（见 webgpu_rhi_device.cpp::BeginComputePass）；
//   着色器仅写 dst、只读其余缓冲。
static const char* kSkinningComputeWGSL = R"(// dse-wgsl
struct SrcVertex { pos_bw0 : vec4<f32>, norm_bw1 : vec4<f32>, tan_bw2 : vec4<f32>, joints_bw3 : vec4<f32>, };
struct DstVertex { pos : vec4<f32>, normal : vec4<f32>, tangent : vec4<f32>, };
struct InstanceInfo {
  vertex_start : u32, vertex_count : u32, bone_offset : u32, morph_target_count : u32,
  morph_weights : vec4<f32>,
  morph_delta_offset : u32, pad0 : u32, pad1 : u32, pad2 : u32,
};
struct Params { @align(16) total_vertices : i32, @align(16) instance_count : i32, };
@group(1) @binding(8) var<uniform> params : Params;
@group(3) @binding(0) var<storage, read_write> src_vertices : array<SrcVertex>;
@group(3) @binding(1) var<storage, read_write> dst_vertices : array<DstVertex>;
@group(3) @binding(2) var<storage, read_write> bone_matrices : array<mat4x4<f32>>;
@group(3) @binding(3) var<storage, read_write> morph_deltas : array<vec4<f32>>;
@group(3) @binding(4) var<storage, read_write> instances : array<InstanceInfo>;
fn find_instance(gid : u32, inst_count : u32) -> u32 {
  var lo : u32 = 0u; var hi : u32 = inst_count;
  while (lo < hi) {
    let mid = (lo + hi) / 2u;
    if (instances[mid].vertex_start + instances[mid].vertex_count <= gid) { lo = mid + 1u; }
    else { hi = mid; }
  }
  return lo;
}
@compute @workgroup_size(64)
fn cs_main(@builtin(global_invocation_id) gid3 : vec3<u32>) {
  let gid = gid3.x;
  let total = u32(params.total_vertices);
  let inst_count = u32(params.instance_count);
  if (gid >= total) { return; }
  let inst_id = find_instance(gid, inst_count);
  if (inst_id >= inst_count) { return; }
  let inst = instances[inst_id];
  let local_vid = gid - inst.vertex_start;
  let s = src_vertices[gid];
  var pos = s.pos_bw0.xyz;
  let normal = s.norm_bw1.xyz;
  let tangent = s.tan_bw2.xyz;
  if (inst.morph_target_count > 0u) {
    let w = array<f32, 4>(inst.morph_weights.x, inst.morph_weights.y,
                          inst.morph_weights.z, inst.morph_weights.w);
    let mbase = inst.morph_delta_offset;
    for (var m = 0u; m < inst.morph_target_count && m < 4u; m = m + 1u) {
      if (abs(w[m]) > 0.001) {
        pos = pos + morph_deltas[mbase + m * inst.vertex_count + local_vid].xyz * w[m];
      }
    }
  }
  let bw0 = s.pos_bw0.w; let bw1 = s.norm_bw1.w; let bw2 = s.tan_bw2.w;
  let bw3 = 1.0 - bw0 - bw1 - bw2;
  let bi0 = u32(s.joints_bw3.x); let bi1 = u32(s.joints_bw3.y);
  let bi2 = u32(s.joints_bw3.z); let bi3 = u32(s.joints_bw3.w);
  let bb = inst.bone_offset;
  let sm = bone_matrices[bb + bi0] * bw0 + bone_matrices[bb + bi1] * bw1
         + bone_matrices[bb + bi2] * bw2 + bone_matrices[bb + bi3] * bw3;
  let nm = mat3x3<f32>(sm[0].xyz, sm[1].xyz, sm[2].xyz);
  dst_vertices[gid].pos     = vec4<f32>((sm * vec4<f32>(pos, 1.0)).xyz, 1.0);
  dst_vertices[gid].normal  = vec4<f32>(normalize(nm * normal), 0.0);
  dst_vertices[gid].tangent = vec4<f32>(normalize(nm * tangent), 0.0);
}
)";

// SrcVertex 内存布局：4 × vec4 = 64 bytes
static constexpr size_t kSrcVertexSize = 64;
// DstVertex 内存布局：3 × vec4 = 48 bytes
static constexpr size_t kDstVertexSize = 48;
// 初始 buffer 容量（顶点数）
static constexpr uint32_t kInitialVertexCapacity = 4096;
// 初始骨骼容量
static constexpr uint32_t kInitialBoneCapacity = 1024;

bool GPUSkinningSystem::Init(RhiDevice* rhi) {
    if (!rhi) return false;
    rhi_ = rhi;

    if (!rhi_->SupportsCompute() || !rhi_->SupportsSSBOCompute()) {
        DEBUG_LOG_INFO("[GPUSkinning] Compute not available, CPU fallback active");
        available_ = false;
        return false;
    }

    // P3: 从统一源生成 GL 430 版本
    static std::string gl_src = GenerateGL430Source();
    // push_constant_bytes: 2 uint = 8 bytes
    skinning_shader_ = rhi_->CreateComputeShaderEx(
        gl_src,                     // GL: uniform 版本（自动生成）
        kSkinningComputeVK,         // VK: push_constant 版本
        kSkinningComputeHLSL,       // DX11: cbuffer 版本
        5,   // ssbo_count: src, dst, bones, morph, instances
        0,   // storage_image_count
        0,   // sampler_count
        8,   // push_constant_bytes (2 × uint)
        kSkinningComputeWGSL        // WebGPU: 手译 WGSL 源（命名 uniform group1/b8）
    );

    if (skinning_shader_ == 0) {
        DEBUG_LOG_WARN("[GPUSkinning] Failed to create compute shader, CPU fallback active");
        available_ = false;
        return false;
    }

    // 分配初始 SSBO
    GpuBufferDesc src_desc{};
    src_desc.size = kInitialVertexCapacity * kSrcVertexSize;
    src_desc.usage = GpuBufferUsage::kStorage;
    src_desc.is_dynamic = true;
    src_buffer_ = rhi_->CreateGpuBuffer(src_desc, nullptr);
    src_buffer_capacity_ = src_desc.size;

    GpuBufferDesc dst_desc{};
    dst_desc.size = kInitialVertexCapacity * kDstVertexSize;
    dst_desc.usage = GpuBufferUsage::kStorage;
    dst_desc.is_dynamic = true;
    dst_buffer_[0] = rhi_->CreateGpuBuffer(dst_desc, nullptr);
    dst_buffer_[1] = rhi_->CreateGpuBuffer(dst_desc, nullptr);
    dst_buffer_capacity_ = dst_desc.size;
    dst_write_idx_ = 0;

    GpuBufferDesc bone_desc{};
    bone_desc.size = kInitialBoneCapacity * sizeof(glm::mat4);
    bone_desc.usage = GpuBufferUsage::kStorage;
    bone_desc.is_dynamic = true;
    bone_buffer_ = rhi_->CreateGpuBuffer(bone_desc, nullptr);
    bone_buffer_capacity_ = bone_desc.size;

    // Morph delta buffer: 初始 16 bytes（占位避免 Vulkan 验证层 binding 3 未绑定警告）
    // 有 morph delta 数据时按需扩容
    GpuBufferDesc morph_desc{};
    morph_desc.size = 16;
    morph_desc.usage = GpuBufferUsage::kStorage;
    morph_desc.is_dynamic = true;
    morph_buffer_ = rhi_->CreateGpuBuffer(morph_desc, nullptr);
    morph_buffer_capacity_ = morph_desc.size;

    // P2: Instance info buffer
    GpuBufferDesc inst_desc{};
    inst_desc.size = 64 * sizeof(InstanceInfoGPU);  // 64 instances initially
    inst_desc.usage = GpuBufferUsage::kStorage;
    inst_desc.is_dynamic = true;
    instance_buffer_ = rhi_->CreateGpuBuffer(inst_desc, nullptr);
    instance_buffer_capacity_ = inst_desc.size;

    available_ = true;
    DEBUG_LOG_INFO("[GPUSkinning] Initialized (shader={}, initial_capacity={}v/{}b)",
                   skinning_shader_, kInitialVertexCapacity, kInitialBoneCapacity);
    return true;
}

void GPUSkinningSystem::Shutdown() {
    if (!rhi_) return;

    if (skinning_shader_) {
        rhi_->DeleteComputeShader(skinning_shader_);
        skinning_shader_ = 0;
    }
    if (src_buffer_) { rhi_->DeleteGpuBuffer(src_buffer_); src_buffer_ = {}; }
    if (dst_buffer_[0]) { rhi_->DeleteGpuBuffer(dst_buffer_[0]); dst_buffer_[0] = {}; }
    if (dst_buffer_[1]) { rhi_->DeleteGpuBuffer(dst_buffer_[1]); dst_buffer_[1] = {}; }
    if (bone_buffer_) { rhi_->DeleteGpuBuffer(bone_buffer_); bone_buffer_ = {}; }
    if (morph_buffer_) { rhi_->DeleteGpuBuffer(morph_buffer_); morph_buffer_ = {}; }
    if (instance_buffer_) { rhi_->DeleteGpuBuffer(instance_buffer_); instance_buffer_ = {}; }

    available_ = false;
    rhi_ = nullptr;
}

void GPUSkinningSystem::BeginFrame() {
    // P4: 读回上一帧的蒙皮结果（此时 GPU 已完成上一帧的 compute）
    ReadBackPrevFrame();

    pending_requests_.clear();
    total_dst_vertices_ = 0;
    total_bone_count_ = 0;
    total_morph_vec4s_ = 0;
}

void GPUSkinningSystem::Submit(SkinningRequest request) {
    if (!available_) return;

    request.dst_vertex_offset = total_dst_vertices_;
    total_dst_vertices_ += request.vertex_count;
    total_bone_count_ += static_cast<uint32_t>(request.bone_matrices.size());
    if (request.morph_target_count > 0 && !request.morph_deltas.empty()) {
        total_morph_vec4s_ += request.morph_target_count * request.vertex_count;
    }
    pending_requests_.push_back(std::move(request));
}

void GPUSkinningSystem::EnsureBufferCapacity() {
    const size_t needed_src = static_cast<size_t>(total_dst_vertices_) * kSrcVertexSize;
    const size_t needed_dst = static_cast<size_t>(total_dst_vertices_) * kDstVertexSize;
    const size_t needed_bone = static_cast<size_t>(total_bone_count_) * sizeof(glm::mat4);

    if (needed_src > src_buffer_capacity_) {
        if (src_buffer_) rhi_->DeleteGpuBuffer(src_buffer_);
        size_t new_cap = needed_src * 2;
        GpuBufferDesc desc{};
        desc.size = new_cap;
        desc.usage = GpuBufferUsage::kStorage;
        desc.is_dynamic = true;
        src_buffer_ = rhi_->CreateGpuBuffer(desc, nullptr);
        src_buffer_capacity_ = new_cap;
    }

    if (needed_dst > dst_buffer_capacity_) {
        if (dst_buffer_[0]) rhi_->DeleteGpuBuffer(dst_buffer_[0]);
        if (dst_buffer_[1]) rhi_->DeleteGpuBuffer(dst_buffer_[1]);
        size_t new_cap = needed_dst * 2;
        GpuBufferDesc desc{};
        desc.size = new_cap;
        desc.usage = GpuBufferUsage::kStorage;
        desc.is_dynamic = true;
        dst_buffer_[0] = rhi_->CreateGpuBuffer(desc, nullptr);
        dst_buffer_[1] = rhi_->CreateGpuBuffer(desc, nullptr);
        dst_buffer_capacity_ = new_cap;
    }

    if (needed_bone > bone_buffer_capacity_) {
        if (bone_buffer_) rhi_->DeleteGpuBuffer(bone_buffer_);
        size_t new_cap = needed_bone * 2;
        GpuBufferDesc desc{};
        desc.size = new_cap;
        desc.usage = GpuBufferUsage::kStorage;
        desc.is_dynamic = true;
        bone_buffer_ = rhi_->CreateGpuBuffer(desc, nullptr);
        bone_buffer_capacity_ = new_cap;
    }

    // Morph delta buffer
    const size_t needed_morph = static_cast<size_t>(total_morph_vec4s_) * 16;  // 16 bytes per vec4
    if (needed_morph > morph_buffer_capacity_) {
        if (morph_buffer_) rhi_->DeleteGpuBuffer(morph_buffer_);
        size_t new_cap = (std::max)(needed_morph * 2, static_cast<size_t>(16));  // 最小 16 bytes 占位
        GpuBufferDesc desc{};
        desc.size = new_cap;
        desc.usage = GpuBufferUsage::kStorage;
        desc.is_dynamic = true;
        morph_buffer_ = rhi_->CreateGpuBuffer(desc, nullptr);
        morph_buffer_capacity_ = new_cap;
    }

    // P2: instance info buffer
    const size_t needed_inst = pending_requests_.size() * sizeof(InstanceInfoGPU);
    if (needed_inst > instance_buffer_capacity_) {
        if (instance_buffer_) rhi_->DeleteGpuBuffer(instance_buffer_);
        size_t new_cap = needed_inst * 2;
        GpuBufferDesc desc{};
        desc.size = new_cap;
        desc.usage = GpuBufferUsage::kStorage;
        desc.is_dynamic = true;
        instance_buffer_ = rhi_->CreateGpuBuffer(desc, nullptr);
        instance_buffer_capacity_ = new_cap;
    }
}

void GPUSkinningSystem::UploadData() {
    // 打包源顶点
    const size_t total_src_bytes = static_cast<size_t>(total_dst_vertices_) * kSrcVertexSize;
    packed_src_.resize(total_src_bytes);

    // 打包骨骼矩阵
    packed_bones_.resize(total_bone_count_);

    // P2: 打包 InstanceInfo
    packed_instances_.resize(pending_requests_.size());

    // P4: 记录本帧 entity slot（用于下一帧 readback 解析）
    prev_entity_slots_.clear();

    // 打包 morph deltas (total_morph_vec4s_ 已在 Submit() 中累加)
    packed_morph_deltas_.resize(static_cast<size_t>(total_morph_vec4s_) * 4);

    uint32_t vertex_offset = 0;
    uint32_t bone_offset = 0;
    uint32_t morph_vec4_offset = 0;

    for (size_t i = 0; i < pending_requests_.size(); ++i) {
        const auto& req = pending_requests_[i];

        // 源顶点数据
        if (!req.src_vertex_data.empty()) {
            const size_t copy_size = (std::min)(req.src_vertex_data.size() * sizeof(float),
                static_cast<size_t>(req.vertex_count) * kSrcVertexSize);
            std::memcpy(packed_src_.data() + vertex_offset * kSrcVertexSize,
                       req.src_vertex_data.data(), copy_size);
        }

        // 骨骼矩阵
        if (!req.bone_matrices.empty()) {
            std::memcpy(packed_bones_.data() + bone_offset,
                       req.bone_matrices.data(),
                       req.bone_matrices.size() * sizeof(glm::mat4));
        }

        // P2: 填充 InstanceInfo
        auto& info = packed_instances_[i];
        info.vertex_start = vertex_offset;
        info.vertex_count = req.vertex_count;
        info.bone_offset = bone_offset;
        info._pad[0] = info._pad[1] = info._pad[2] = 0;

        // Morph target: 仅当有实际 delta 数据时启用
        const bool has_morph_data = req.morph_target_count > 0 && !req.morph_deltas.empty();
        if (has_morph_data) {
            info.morph_target_count = req.morph_target_count;
            info.morph_delta_offset = morph_vec4_offset;
            for (int k = 0; k < 4; ++k) {
                info.morph_weights[k] = (k < static_cast<int>(req.morph_weights.size()))
                    ? req.morph_weights[k] : 0.0f;
            }
            // 拷贝 morph delta 数据
            const uint32_t morph_floats = req.morph_target_count * req.vertex_count * 4;
            const size_t copy_size = (std::min)(req.morph_deltas.size(),
                static_cast<size_t>(morph_floats)) * sizeof(float);
            std::memcpy(packed_morph_deltas_.data() + static_cast<size_t>(morph_vec4_offset) * 4,
                       req.morph_deltas.data(), copy_size);
            morph_vec4_offset += req.morph_target_count * req.vertex_count;
        } else {
            info.morph_target_count = 0;
            info.morph_delta_offset = 0;
            for (int k = 0; k < 4; ++k) info.morph_weights[k] = 0.0f;
        }

        // P4: 记录 entity → dst offset/count
        prev_entity_slots_[req.entity_id] = { vertex_offset, req.vertex_count };

        vertex_offset += req.vertex_count;
        bone_offset += static_cast<uint32_t>(req.bone_matrices.size());
    }

    // 上传到 GPU
    rhi_->UpdateGpuBuffer(src_buffer_, 0, total_src_bytes, packed_src_.data());
    rhi_->UpdateGpuBuffer(bone_buffer_, 0,
                          total_bone_count_ * sizeof(glm::mat4), packed_bones_.data());
    if (total_morph_vec4s_ > 0) {
        rhi_->UpdateGpuBuffer(morph_buffer_, 0,
                              static_cast<size_t>(total_morph_vec4s_) * 16,
                              packed_morph_deltas_.data());
    }
    rhi_->UpdateGpuBuffer(instance_buffer_, 0,
                          packed_instances_.size() * sizeof(InstanceInfoGPU),
                          packed_instances_.data());
}

void GPUSkinningSystem::Dispatch() {
    if (!available_ || pending_requests_.empty()) return;

    EnsureBufferCapacity();
    UploadData();

    // P2: 绑定所有 SSBO + 单次 Dispatch（双缓冲写入当前帧 buffer）
    rhi_->BindGpuBuffer(src_buffer_, 0, false);                    // binding 0: src vertices
    rhi_->BindGpuBuffer(dst_buffer_[dst_write_idx_], 1, true);    // binding 1: dst vertices (write)
    rhi_->BindGpuBuffer(bone_buffer_, 2, false);                   // binding 2: bone matrices
    rhi_->BindGpuBuffer(morph_buffer_, 3, false);                  // binding 3: morph deltas (占位)
    rhi_->BindGpuBuffer(instance_buffer_, 4, false);               // binding 4: instance info

    // 设置 uniform / push_constant: total_vertices, instance_count
    rhi_->SetComputeUniformInt(skinning_shader_, "u_total_vertices",
                                static_cast<int>(total_dst_vertices_));
    rhi_->SetComputeUniformInt(skinning_shader_, "u_instance_count",
                                static_cast<int>(pending_requests_.size()));

    // 单次 Dispatch: ceil(total_vertices / 64) workgroups
    const uint32_t groups_x = (total_dst_vertices_ + 63) / 64;
    rhi_->DispatchCompute(skinning_shader_, groups_x, 1, 1);

    // Memory barrier: 确保 compute 写入对后续读操作可见
    rhi_->ComputeMemoryBarrier();

    // P4: 记录本帧总顶点数（用于下一帧 readback）
    prev_total_vertices_ = total_dst_vertices_;

    // 双缓冲翻转：下一帧写另一个 buffer，读当前帧的 buffer
    dst_write_idx_ = 1 - dst_write_idx_;
}

// --- P4: Readback 与查询 API ---

void GPUSkinningSystem::ReadBackPrevFrame() {
    readback_results_.clear();

    if (prev_total_vertices_ == 0 || prev_entity_slots_.empty()) return;

    // 双缓冲: Dispatch 末尾已 flip，当前 dst_write_idx_ 指向本帧要写入的 buffer，
    // 上一帧写入的是 dst_buffer_[dst_write_idx_] — 即翻转后的 "当前写"索引，
    // 实际上上一帧 Dispatch 写入的是翻转前的 buffer = dst_buffer_[1 - dst_write_idx_]
    // 但 flip 发生在 Dispatch 末尾，所以在 BeginFrame 时 dst_write_idx_ 已经是翻转后的值。
    // 因此上一帧写的是 dst_buffer_[1 - dst_write_idx_]（当前帧的读取 buffer）。
    const uint32_t read_idx = 1 - dst_write_idx_;
    if (!dst_buffer_[read_idx]) return;

    // 读回上一帧 compute 输出（双缓冲保证此 buffer 不被当前帧写入，消除 GPU sync stall）
    const size_t readback_bytes = static_cast<size_t>(prev_total_vertices_) * kDstVertexSize;
    readback_raw_.resize(readback_bytes);
    rhi_->ReadGpuBuffer(dst_buffer_[read_idx], 0, readback_bytes, readback_raw_.data());

    // 解析为 per-entity SkinnedOutput
    for (const auto& [eid, slot] : prev_entity_slots_) {
        SkinnedOutput out;
        out.vertex_count = slot.count;
        out.positions.resize(slot.count);
        out.normals.resize(slot.count);
        out.tangents.resize(slot.count);

        const uint8_t* base = readback_raw_.data() + static_cast<size_t>(slot.offset) * kDstVertexSize;
        for (uint32_t v = 0; v < slot.count; ++v) {
            const float* dst = reinterpret_cast<const float*>(base + v * kDstVertexSize);
            out.positions[v] = glm::vec3(dst[0], dst[1], dst[2]);
            out.normals[v]   = glm::vec3(dst[4], dst[5], dst[6]);
            out.tangents[v]  = glm::vec3(dst[8], dst[9], dst[10]);
        }

        readback_results_[eid] = std::move(out);
    }
}

bool GPUSkinningSystem::HasSkinnedOutput(uint32_t entity_id) const {
    return readback_results_.count(entity_id) > 0;
}

const SkinnedOutput* GPUSkinningSystem::GetSkinnedOutput(uint32_t entity_id) const {
    auto it = readback_results_.find(entity_id);
    return (it != readback_results_.end()) ? &it->second : nullptr;
}

} // namespace render
} // namespace dse
