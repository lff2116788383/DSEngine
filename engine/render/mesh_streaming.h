/**
 * @file mesh_streaming.h
 * @brief 网格 LOD 流式加载系统
 *
 * 独立于 Virtual Texture 的 Mesh 按需加载：
 * - 远处只加载低 LOD mesh 数据，靠近时异步加载高精度
 * - 基于距离的 LOD 选择 + 异步加载队列
 * - 滞回（hysteresis）防止 LOD 频繁切换
 * - 每帧预算限制，避免 IO 洪泛
 * - 与 StreamingManager 协作但独立管理 mesh 资源生命周期
 */

#ifndef DSE_MESH_STREAMING_H
#define DSE_MESH_STREAMING_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <glm/glm.hpp>

namespace dse {
namespace render {

/// 单个 LOD 级别描述
struct MeshLODLevel {
    uint32_t level = 0;             ///< LOD 编号（0=最高精度）
    std::string asset_path;         ///< 资源路径
    float screen_coverage = 0.0f;   ///< 触发该 LOD 的最小屏幕覆盖率 [0,1]
    float distance_threshold = 0.0f;///< 触发该 LOD 的最大距离
    uint32_t triangle_count = 0;    ///< 该 LOD 的三角形数量（统计用）
    bool loaded = false;            ///< 是否已加载到 GPU
    bool loading = false;           ///< 是否正在加载中
};

/// Mesh 流式加载实体
struct StreamingMeshEntry {
    uint32_t id = 0;
    std::string name;
    glm::vec3 world_position = glm::vec3(0.0f);
    float bounding_radius = 1.0f;   ///< 包围球半径
    std::vector<MeshLODLevel> lods;
    uint32_t current_lod = 0;       ///< 当前激活的 LOD
    uint32_t desired_lod = 0;       ///< 目标 LOD（可能还在加载中）
    float last_distance = 0.0f;     ///< 上一帧距离
    bool resident = false;          ///< 是否至少有一个 LOD 常驻
};

/// LOD 切换请求（内部队列）
struct MeshLODRequest {
    uint32_t mesh_id = 0;
    uint32_t target_lod = 0;
    float priority = 0.0f;          ///< 越小越优先（基于距离）
};

/// Mesh Streaming 配置
struct MeshStreamingConfig {
    float hysteresis_factor = 1.2f;     ///< 切回高 LOD 的距离乘数（防抖动）
    int load_budget_per_frame = 4;      ///< 每帧最大新发起加载数
    int max_concurrent_loads = 16;      ///< 最大并发加载数
    float min_switch_interval = 0.5f;   ///< 同一 mesh 两次 LOD 切换的最小间隔（秒）
    bool force_lowest_lod_resident = true; ///< 最低 LOD 始终常驻内存
};

/// 加载完成回调
using MeshLoadCallback = std::function<void(uint32_t mesh_id, uint32_t lod_level, bool success)>;

/**
 * @class MeshStreamingSystem
 * @brief 管理 mesh LOD 的流式加载/卸载
 */
class MeshStreamingSystem {
public:
    MeshStreamingSystem() = default;
    ~MeshStreamingSystem() = default;

    void Init(const MeshStreamingConfig& config = {});
    void Shutdown();

    /// 注册一个流式 mesh（返回 mesh_id）
    uint32_t RegisterMesh(const std::string& name, const glm::vec3& position, float radius);

    /// 为已注册 mesh 添加 LOD 级别
    void AddLODLevel(uint32_t mesh_id, uint32_t level, const std::string& asset_path,
                     float distance_threshold, uint32_t tri_count = 0);

    /// 移除已注册 mesh
    void UnregisterMesh(uint32_t mesh_id);

    /// 更新 mesh 世界位置（每帧或变化时调用）
    void UpdatePosition(uint32_t mesh_id, const glm::vec3& position);

    /// 每帧 Tick：评估所有 mesh 的 LOD 需求，发起加载/卸载
    void Tick(const glm::vec3& camera_position, float delta_time);

    /// 设置加载完成回调
    void SetLoadCallback(MeshLoadCallback callback) { load_callback_ = std::move(callback); }

    /// 强制加载某 mesh 的指定 LOD
    void ForceLoadLOD(uint32_t mesh_id, uint32_t lod_level);

    /// 强制卸载某 mesh 的指定 LOD（保留最低 LOD）
    void ForceUnloadLOD(uint32_t mesh_id, uint32_t lod_level);

    // ========== 查询 ==========

    /// 获取当前激活的 LOD
    uint32_t GetCurrentLOD(uint32_t mesh_id) const;

    /// 获取目标 LOD（可能还在加载中）
    uint32_t GetDesiredLOD(uint32_t mesh_id) const;

    /// 获取注册 mesh 总数
    uint32_t GetMeshCount() const;

    /// 获取当前并发加载数
    int GetActiveLoadCount() const { return active_loads_.load(std::memory_order_relaxed); }

    /// 获取某 mesh 的 LOD 数量
    uint32_t GetLODCount(uint32_t mesh_id) const;

    /// 获取内存使用统计（已加载的总三角形数）
    uint64_t GetLoadedTriangleCount() const;

    /// Floating Origin 偏移
    void RebaseOrigin(const glm::vec3& offset);

    /// 获取配置
    const MeshStreamingConfig& GetConfig() const { return config_; }

private:
    uint32_t EvaluateDesiredLOD(const StreamingMeshEntry& entry, float distance) const;
    void ProcessLoadQueue();
    void SimulateLoadComplete(uint32_t mesh_id, uint32_t lod_level);

    MeshStreamingConfig config_;
    std::unordered_map<uint32_t, StreamingMeshEntry> meshes_;
    mutable std::mutex meshes_mutex_;
    uint32_t next_mesh_id_ = 1;

    std::vector<MeshLODRequest> load_queue_;
    std::atomic<int> active_loads_{0};
    MeshLoadCallback load_callback_;

    float accumulated_time_ = 0.0f;
    std::unordered_map<uint32_t, float> last_switch_time_; ///< mesh_id → 上次切换时间
    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_MESH_STREAMING_H
