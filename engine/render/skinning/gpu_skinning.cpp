#include "engine/render/skinning/gpu_skinning.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/base/debug.h"
#include <cstring>
#include <algorithm>

// 着色器单一来源（option 2-strengthened）：GL430 / HLSL / GLSL450(Vulkan) 三种形态
// 全部由 src/skinning.comp 经离线编译链（glslang + spirv-cross）自动生成、嵌入此
// gen.h，消费方不再内联任何 GLSL/HLSL 字符串。WGSL 无离线 GLSL→WGSL 工具，集中手写
// 单份保留在本文件（见 kSkinningComputeWGSL）。
#include "engine/render/shaders/generated/embed/skinning_comp.gen.h"

namespace dse {
namespace render {

using namespace generated_shaders;

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

    // 单一来源消费：GL430 / VK GLSL450 / HLSL 均取自 skinning_comp.gen.h（src/skinning.comp
    // 自动交叉编译）。push 块在 src 中已显式 16B 对齐（total@0、instance@16），DsePushCS/cbuffer
    // 偏移与原生 SetComputeUniformInt 的 16B-per-call staging 一致；push_constant_bytes 取 32
    // （= WGSL std140 结构尺寸），VK 据此推送足量覆盖 instance@16（range 恒 ≥160B，放大安全）。
    skinning_shader_ = rhi_->CreateComputeShaderEx(
        kskinning_comp_glsl430,     // GL: 交叉编译 GL430（DsePushCS UBO）
        kskinning_comp_glsl450,     // VK: 逐字 GLSL450 源（保留 push_constant）
        kskinning_comp_hlsl,        // DX11: 交叉编译 HLSL（cbuffer PC）
        5,   // ssbo_count: src, dst, bones, morph, instances
        0,   // storage_image_count
        0,   // sampler_count
        32,  // push_constant_bytes (16B-aligned: total@0, instance@16)
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
    // dispatch 须包裹在 compute pass 内（WebGPU DispatchCompute 在无 pass 时 no-op；
    // 桌面 GL/VK/DX 的 Begin/EndComputePass 为安全空操作或状态作用域）。与 grass/hair 一致。
    const uint32_t groups_x = (total_dst_vertices_ + 63) / 64;
    rhi_->BeginComputePass();
    rhi_->DispatchCompute(skinning_shader_, groups_x, 1, 1);
    // Memory barrier: 确保 compute 写入对后续读操作可见
    rhi_->ComputeMemoryBarrier();
    rhi_->EndComputePass();

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

    // 读回上一帧 compute 输出（双缓冲保证此 buffer 不被当前帧写入，消除 GPU sync stall）。
    // 经异步双缓冲延迟回读 API：桌面 GL/VK/DX 用基类默认同步实现（当帧即就绪、语义不变）；
    // WebGPU 覆写为延迟 1 帧（frame_encoder_ 上 CopyBufferToBuffer → staging → mapAsync）。
    // 未就绪（WebGPU 暖机首帧 / map 飞行中）则 readback_results_ 留空 → 消费方落 CPU 蒙皮回退。
    const size_t readback_bytes = static_cast<size_t>(prev_total_vertices_) * kDstVertexSize;
    readback_raw_.resize(readback_bytes);
    const bool ready = rhi_->BeginGpuReadback(dst_buffer_[read_idx], 0, readback_bytes);
    if (!ready) return;
    size_t mapped_size = 0;
    const void* mapped = rhi_->GetLastReadbackResult(&mapped_size);
    if (!mapped || mapped_size < readback_bytes) return;
    std::memcpy(readback_raw_.data(), mapped, readback_bytes);

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
