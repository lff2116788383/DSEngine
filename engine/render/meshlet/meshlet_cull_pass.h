/**
 * @file meshlet_cull_pass.h
 * @brief 运行时 per-meshlet GPU 剔除 Pass
 *
 * 使用 Compute Shader 对每个 meshlet cluster 执行：
 * 1. 视锥剔除（6 平面 vs 包围球）
 * 2. 法线锥背面剔除（可选）
 * 3. Hi-Z 遮挡剔除（基于包围球屏幕投影）
 *
 * 输出：per-meshlet DrawElementsIndirectCommand（instance_count=0/1 表示剔除结果）
 */

#ifndef DSE_MESHLET_CULL_PASS_H
#define DSE_MESHLET_CULL_PASS_H

#include "engine/render/meshlet/meshlet_types.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace dse {
namespace render {

class RhiDevice;

/// 单个已注册的 meshlet mesh 实例（运行时）
struct MeshletInstance {
    uint32_t mesh_id;           ///< 引用 MeshletRegistry 中的 mesh
    glm::mat4 model;            ///< world transform
    uint32_t base_meshlet;      ///< 在全局 meshlet GPU buffer 中的起始偏移
    uint32_t meshlet_count;     ///< 该实例的 meshlet 数量
};

/// Meshlet 注册表中一个 mesh 条目
struct MeshletRegistryEntry {
    MeshletMesh mesh_data;
    uint32_t global_vertex_offset;   ///< 在 mega VBO 中的顶点偏移
    uint32_t global_index_offset;    ///< 在 mega IBO 中的索引偏移
};

/// Meshlet Cull Pass 配置
struct MeshletCullConfig {
    bool enable_frustum_cull = true;
    bool enable_occlusion_cull = true;
    bool enable_cone_cull = true;
    float cone_cull_threshold = 0.0f;   ///< 只有 cone_cutoff > threshold 时才做锥剔除
};

/**
 * @class MeshletCullPass
 * @brief 管理 meshlet cluster 的注册、GPU 数据上传和每帧剔除调度
 */
class MeshletCullPass {
public:
    MeshletCullPass() = default;
    ~MeshletCullPass() = default;

    /// 注册一个 meshlet mesh 到注册表，返回 mesh_id
    uint32_t RegisterMesh(const MeshletMesh& mesh);

    /// 移除已注册的 mesh
    void UnregisterMesh(uint32_t mesh_id);

    /// 添加一个实例（每帧收集 phase 调用）
    void AddInstance(uint32_t mesh_id, const glm::mat4& model);

    /// 清空当前帧实例列表（帧开始时调用）
    void BeginFrame();

    /// 准备 GPU 数据（构建 draw commands + meshlet gpu data）
    /// @return 本帧 meshlet draw command 总数
    uint32_t PrepareGPUData(const glm::mat4& view, const glm::mat4& proj,
                            const glm::vec3& camera_pos);

    /// 执行 CPU 侧剔除（fallback，不需要 compute shader）
    /// 修改 draw_commands_ 中的 instance_count
    void CullCPU(const glm::mat4& view_proj, const glm::vec3& camera_pos,
                 const MeshletCullConfig& config = {});

    /// 获取剔除后的 draw commands
    const std::vector<MeshletDrawCommand>& GetDrawCommands() const { return draw_commands_; }

    /// 获取 meshlet GPU 数据（供 compute shader SSBO）
    const std::vector<MeshletGPUData>& GetMeshletGPUData() const { return meshlet_gpu_data_; }

    /// 获取当前帧总 meshlet 数
    uint32_t GetTotalMeshletCount() const { return total_meshlet_count_; }

    /// 获取可见 meshlet 数（CullCPU 后有效）
    uint32_t GetVisibleMeshletCount() const;

    /// 统计信息
    uint32_t GetRegisteredMeshCount() const { return static_cast<uint32_t>(mesh_registry_.size()); }
    uint32_t GetInstanceCount() const { return static_cast<uint32_t>(instances_.size()); }

    /// 访问已注册的 mesh 注册表（供 MeshletRenderPass 查询 meshlet 数量等）
    const std::unordered_map<uint32_t, MeshletRegistryEntry>& GetMeshRegistry() const { return mesh_registry_; }

private:
    void ExtractFrustumPlanes(const glm::mat4& view_proj, glm::vec4 planes[6]);
    bool FrustumTestSphere(const glm::vec4 planes[6], const glm::vec3& center, float radius);
    bool ConeCullTest(const glm::vec3& cone_axis, float cone_cutoff, const glm::vec3& camera_pos,
                      const glm::vec3& center);

    uint32_t next_mesh_id_ = 1;
    std::unordered_map<uint32_t, MeshletRegistryEntry> mesh_registry_;
    std::vector<MeshletInstance> instances_;

    // Per-frame GPU data
    std::vector<MeshletDrawCommand> draw_commands_;
    std::vector<MeshletGPUData> meshlet_gpu_data_;
    uint32_t total_meshlet_count_ = 0;
};

} // namespace render
} // namespace dse

#endif // DSE_MESHLET_CULL_PASS_H
