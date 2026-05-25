/**
 * @file streaming_manager.h
 * @brief 资源流式加载管理器 — 基于空间距离的分块加载/卸载 + 异步 IO 优先级队列
 *
 * 核心概念：
 * - StreamingZone：空间球形区域，包含一组待加载资源
 * - 距离触发：摄像机进入 load_radius 时开始加载，离开 unload_radius 时卸载（滞回）
 * - 优先级队列：按距离排序，近处 High、中距 Normal、远处（预取）Low
 * - 每帧预算：限制单帧最大新发起加载数，避免 IO 洪泛
 */

#ifndef DSE_STREAMING_MANAGER_H
#define DSE_STREAMING_MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>
#include <any>
#include <glm/glm.hpp>

class AssetManager;

namespace dse::streaming {

/// 资源类型标签
enum class AssetType : uint8_t {
    Texture = 0,
    Mesh,
    Animation,
    Skeleton,
    Audio,
    Material,
};

/// 单个待流式加载的资源条目
struct StreamingAssetEntry {
    std::string path;
    AssetType type = AssetType::Texture;
    bool loaded = false;
    /// 持有加载完成的资源 shared_ptr（type-erased），防止 AssetManager weak_ptr 缓存过期
    std::any retained_resource;
};

/// Zone 生命周期状态
enum class ZoneState : uint8_t {
    Unloaded = 0, ///< 完全未加载
    Loading,      ///< 正在异步加载
    Loaded,       ///< 全部资源就绪
    Unloading,    ///< 正在卸载（释放引用）
};

/// 流式加载区域
struct StreamingZone {
    uint32_t id = 0;
    std::string name;
    glm::vec3 center = glm::vec3(0.0f);
    float load_radius = 1000.0f;    ///< 触发加载的距离
    float unload_radius = 1500.0f;  ///< 触发卸载的距离（应 > load_radius）
    int priority_bias = 0;          ///< 手动优先级偏移（正值=更高优先）
    std::vector<StreamingAssetEntry> assets;
    ZoneState state = ZoneState::Unloaded;
    int assets_pending = 0;         ///< 仍在等待加载的资源数
    bool force_loaded = false;      ///< 手动强制加载（忽略距离）
};

/**
 * @class StreamingManager
 * @brief 资源流式加载管理器
 *
 * 生命周期由 FramePipeline 管理：
 * - Init() 在 FramePipeline::Init() 末尾调用
 * - Tick() 在每帧 RunUpdateInternal() 中调用
 * - Shutdown() 在 FramePipeline::Shutdown() 中调用
 */
class StreamingManager {
public:
    StreamingManager() = default;
    ~StreamingManager() = default;

    /// 初始化，注入 AssetManager
    void Init(AssetManager* asset_manager);

    /// 关闭并释放所有 zone
    void Shutdown();

    // ========== Zone 管理 ==========

    /// 创建流式加载区域，返回 zone_id
    uint32_t CreateZone(const std::string& name, const glm::vec3& center,
                        float load_radius, float unload_radius);

    /// 销毁指定 zone（立即释放资源引用）
    void DestroyZone(uint32_t zone_id);

    /// 向 zone 添加资源条目
    void AddAsset(uint32_t zone_id, const std::string& path, AssetType type);

    /// 批量添加同类型资源
    void AddAssets(uint32_t zone_id, const std::vector<std::string>& paths, AssetType type);

    /// 动态更新 zone 中心（用于移动 zone）
    void SetZoneCenter(uint32_t zone_id, const glm::vec3& center);

    /// 强制加载 zone（忽略距离）
    void ForceLoadZone(uint32_t zone_id);

    /// 强制卸载 zone
    void ForceUnloadZone(uint32_t zone_id);

    // ========== 每帧更新 ==========

    /// 每帧调用：根据摄像机位置评估所有 zone，发起加载/卸载
    void Tick(const glm::vec3& camera_position);

    // ========== 配置 ==========

    /// 设置每帧最大新发起加载请求数
    void SetLoadBudgetPerFrame(int budget) { load_budget_per_frame_ = budget; }

    /// 设置全局最大并发加载数（跨帧累积上限）
    void SetMaxConcurrentLoads(int max_loads) { max_concurrent_loads_ = max_loads; }

    // ========== 查询 ==========

    /// 获取 zone 当前状态
    ZoneState GetZoneState(uint32_t zone_id) const;

    /// 获取 zone 加载进度 [0.0, 1.0]
    float GetZoneProgress(uint32_t zone_id) const;

    /// 获取当前活跃（正在加载）的请求数
    int GetActiveLoadCount() const { return active_load_count_.load(std::memory_order_relaxed); }

    /// 获取已注册 zone 总数
    std::size_t GetZoneCount() const;

    /// 获取所有 zone 的只读快照（编辑器调试用）
    std::vector<StreamingZone> GetZoneSnapshot() const;

private:
    /// 对单个 zone 发起加载
    void BeginLoadZone(StreamingZone& zone);

    /// 对单个 zone 执行卸载
    void BeginUnloadZone(StreamingZone& zone);

    /// 资源加载完成回调
    void OnAssetLoaded(uint32_t zone_id, const std::string& path, bool success);

    /// 资源加载完成回调（带资源引用持有）
    void OnAssetLoadedWithResource(uint32_t zone_id, const std::string& path,
                                    bool success, std::any resource);

    AssetManager* asset_manager_ = nullptr;

    std::unordered_map<uint32_t, StreamingZone> zones_;
    mutable std::mutex zones_mutex_;
    uint32_t next_zone_id_ = 1;

    int load_budget_per_frame_ = 8;    ///< 每帧最多新发起 8 个加载
    int max_concurrent_loads_ = 32;    ///< 全局最大并发加载数
    std::atomic<int> active_load_count_{0};

    bool initialized_ = false;
};

} // namespace dse::streaming

#endif // DSE_STREAMING_MANAGER_H
