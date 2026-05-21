/**
 * @file gpu_skinning.h
 * @brief GPU Compute Skinning 系统
 *
 * 将骨骼蒙皮和 morph target 计算从 CPU 转移到 GPU Compute Shader。
 * 三后端均支持，不支持 Compute 时自动回退 CPU 路径。
 */

#ifndef DSE_RENDER_GPU_SKINNING_H
#define DSE_RENDER_GPU_SKINNING_H

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "engine/render/rhi/rhi_handle.h"

namespace dse {
namespace render {

class RhiDevice;

/// GPU InstanceInfo — 与 shader 中的 InstanceInfo struct 内存布局一致 (std430, 48 bytes)
struct alignas(16) InstanceInfoGPU {
    uint32_t vertex_start;
    uint32_t vertex_count;
    uint32_t bone_offset;
    uint32_t morph_target_count;
    float morph_weights[4];
    uint32_t morph_delta_offset;  ///< 本实例在 morph_deltas SSBO 中的起始 vec4 索引
    uint32_t _pad[3];             ///< std430: struct alignment = 16 (from vec4), size = 48
};
static_assert(sizeof(InstanceInfoGPU) == 48, "InstanceInfoGPU must match shader layout (48 bytes)");

/// 单个蒙皮实例的提交数据
struct SkinningRequest {
    uint32_t entity_id = 0;             ///< ECS 实体 ID（用于缓存查找）
    uint32_t vertex_count = 0;          ///< 顶点数量
    std::vector<float> src_vertex_data;  ///< 源顶点数据（SrcVertex 布局，owned）

    std::vector<glm::mat4> bone_matrices; ///< 本实例的骨骼矩阵（已预乘 model）
    std::vector<float> morph_weights;     ///< morph target 权重（最多 4 个）
    uint32_t morph_target_count = 0;
    std::vector<float> morph_deltas;      ///< per-vertex morph delta (vec4 per vertex per target)

    /// 蒙皮完成后的输出 SSBO 偏移（由系统填充）
    uint32_t dst_vertex_offset = 0;
};

/// 蒙皮输出数据（上一帧 readback，供 Render() 使用）
struct SkinnedOutput {
    uint32_t vertex_count = 0;
    std::vector<glm::vec3> positions;   ///< world-space skinned positions
    std::vector<glm::vec3> normals;     ///< world-space skinned normals
    std::vector<glm::vec3> tangents;    ///< world-space skinned tangents
};

/// GPU Skinning 系统 — 管理 SSBO 分配、Compute Shader 创建和 Dispatch
class GPUSkinningSystem {
public:
    GPUSkinningSystem() = default;
    ~GPUSkinningSystem() = default;

    /// 初始化（创建 compute shader、分配初始 SSBO）
    /// @return true 如果 compute skinning 可用
    bool Init(RhiDevice* rhi);

    /// 关闭并释放 GPU 资源
    void Shutdown();

    /// 检查当前设备是否支持 GPU Compute Skinning
    bool IsAvailable() const { return available_; }

    /// 每帧开始时清空上一帧的请求，并读回上一帧的蒙皮结果
    void BeginFrame();

    /// 提交一个蒙皮请求（move 语义，所有权转入系统）
    void Submit(SkinningRequest request);

    /// P2: 单次 Dispatch 所有实例（通过 InstanceInfo SSBO）
    void Dispatch();

    /// 获取蒙皮输出 SSBO handle（当前帧写入的 buffer）
    BufferHandle GetOutputBuffer() const { return dst_buffer_[dst_write_idx_]; }

    /// 获取本帧处理的总顶点数
    uint32_t GetTotalSkinnedVertices() const { return total_dst_vertices_; }

    /// P4: 查询上一帧的蒙皮输出（readback）
    bool HasSkinnedOutput(uint32_t entity_id) const;
    const SkinnedOutput* GetSkinnedOutput(uint32_t entity_id) const;

private:
    RhiDevice* rhi_ = nullptr;
    bool available_ = false;

    unsigned int skinning_shader_ = 0;  ///< compute shader handle

    // SSBO 资源
    BufferHandle src_buffer_;           ///< 源顶点 SSBO
    BufferHandle dst_buffer_[2];        ///< 输出顶点 SSBO（双缓冲，消除 readback 同步阻塞）
    BufferHandle bone_buffer_;          ///< 骨骼矩阵 SSBO
    BufferHandle morph_buffer_;         ///< morph delta SSBO (binding 3)
    BufferHandle instance_buffer_;      ///< P2: per-instance info SSBO

    size_t src_buffer_capacity_ = 0;
    size_t dst_buffer_capacity_ = 0;    ///< 双缓冲共享容量（两个 buffer 大小相同）
    size_t bone_buffer_capacity_ = 0;
    size_t morph_buffer_capacity_ = 0;
    size_t instance_buffer_capacity_ = 0;
    uint32_t dst_write_idx_ = 0;        ///< 当前帧写入的 dst buffer 索引 (0 or 1)

    // 本帧请求（owned）
    std::vector<SkinningRequest> pending_requests_;
    uint32_t total_dst_vertices_ = 0;
    uint32_t total_bone_count_ = 0;
    uint32_t total_morph_vec4s_ = 0;  ///< morph_deltas SSBO 总 vec4 数

    // 打包缓存
    std::vector<glm::mat4> packed_bones_;
    std::vector<uint8_t> packed_src_;
    std::vector<InstanceInfoGPU> packed_instances_;
    std::vector<float> packed_morph_deltas_;  ///< morph delta data (vec4 per entry, 4 floats)

    // P4: readback — 上一帧的蒙皮结果
    std::unordered_map<uint32_t, SkinnedOutput> readback_results_;
    struct EntitySlot { uint32_t offset; uint32_t count; };
    std::unordered_map<uint32_t, EntitySlot> prev_entity_slots_;
    uint32_t prev_total_vertices_ = 0;
    std::vector<uint8_t> readback_raw_;  ///< 缓存 readback 临时缓冲（避免每帧分配）

    void EnsureBufferCapacity();
    void UploadData();
    void ReadBackPrevFrame();
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_GPU_SKINNING_H
