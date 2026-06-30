/**
 * @file world_partition.h
 * @brief 世界分区流式加载系统
 *
 * 将世界按固定网格划分为 Cell，根据 StreamingOrigin 的位置异步加载/卸载 Cell
 * 对应的 SubScene。每个 Cell 是一个独立的 .dscene 文件。
 *
 * 设计：
 * - 每个 Cell 有 2D 坐标 (cx, cy) 和 AABB 边界
 * - 加载半径 (load_radius) 内的 Cell 被加载，卸载半径 (unload_radius) 外的 Cell 被卸载
 * - 通过 SceneManager 异步加载 SubScene，不阻塞主线程
 * - 支持配置优先级（距离越近优先级越高）
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

class World;

namespace scene {
class SceneManager;
}

namespace dse {

// ─── ECS 组件 ───────────────────────────────────────────────────────────────

/// 标记作为流式加载原点的实体（通常是 Camera 或 Player）
struct StreamingOriginComponent {
    bool enabled = true;
    float load_radius = 256.0f;     ///< 加载半径（Cell 中心在此距离内则加载）
    float unload_radius = 320.0f;   ///< 卸载半径（必须 > load_radius 以避免抖动）
};

/// 世界分区配置组件，附加到场景根实体
struct WorldPartitionConfigComponent {
    bool enabled = true;
    float cell_size = 128.0f;       ///< 每个 Cell 的边长（世界坐标单位）
    std::string cells_directory;    ///< Cell 场景文件所在目录（相对 data root）
    /// Cell 文件命名模板：{dir}/cell_{cx}_{cy}.dscene
    std::string cell_file_pattern = "cell_{cx}_{cy}.dscene";
    int grid_min_x = -16;          ///< 网格X最小坐标
    int grid_max_x = 16;           ///< 网格X最大坐标
    int grid_min_y = -16;          ///< 网格Y最小坐标
    int grid_max_y = 16;           ///< 网格Y最大坐标
    uint32_t max_loads_per_frame = 2;  ///< 每帧最多触发几个异步加载
};

// ─── 内部数据结构 ───────────────────────────────────────────────────────────

/// 2D Cell 坐标
struct CellCoord {
    int x = 0;
    int y = 0;
    bool operator==(const CellCoord& o) const { return x == o.x && y == o.y; }
    bool operator!=(const CellCoord& o) const { return !(*this == o); }
};

struct CellCoordHash {
    size_t operator()(const CellCoord& c) const {
        size_t h = std::hash<int>()(c.x);
        h ^= std::hash<int>()(c.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

enum class CellState : uint8_t {
    Unloaded = 0,
    Loading,
    Loaded,
    Unloading,
};

struct CellInfo {
    CellCoord coord;
    CellState state = CellState::Unloaded;
    std::string scene_path;
    float distance_sq = 0.0f;  ///< 到最近 StreamingOrigin 的距离平方（用于优先级排序）
};

// ─── WorldPartitionSystem ───────────────────────────────────────────────────

using WorldPartitionProgressCallback = std::function<void(int loaded, int total)>;

class DSE_EXPORT WorldPartitionSystem {
public:
    WorldPartitionSystem() = default;
    ~WorldPartitionSystem() = default;

    /// 初始化，注入 SceneManager 依赖
    void Init(::scene::SceneManager* scene_mgr);

    /// 每帧更新：根据 StreamingOrigin 位置决定加载/卸载
    void Update(::World& world);

    /// 强制加载指定 Cell
    void ForceLoadCell(const CellCoord& coord);

    /// 强制卸载指定 Cell
    void ForceUnloadCell(const CellCoord& coord);

    /// 获取当前已加载的 Cell 数量
    size_t LoadedCellCount() const;

    /// 获取所有 Cell 状态（调试/编辑器用）
    const std::unordered_map<CellCoord, CellInfo, CellCoordHash>& GetCells() const { return cells_; }

    /// 世界坐标 → Cell 坐标
    CellCoord WorldToCell(const glm::vec3& world_pos, float cell_size) const;

    /// Cell 坐标 → 世界空间中心
    glm::vec3 CellToWorld(const CellCoord& coord, float cell_size) const;

    void SetProgressCallback(WorldPartitionProgressCallback cb) { progress_cb_ = std::move(cb); }

    void Shutdown();

private:
    std::string BuildCellPath(const WorldPartitionConfigComponent& config, const CellCoord& coord) const;

    ::scene::SceneManager* scene_mgr_ = nullptr;
    std::unordered_map<CellCoord, CellInfo, CellCoordHash> cells_;
    WorldPartitionProgressCallback progress_cb_;
};

} // namespace dse
