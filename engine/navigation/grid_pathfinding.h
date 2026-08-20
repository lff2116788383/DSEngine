/**
 * @file grid_pathfinding.h
 * @brief 2D 网格寻路系统 — A* 算法，支持 4/8 方向、动态障碍、Tilemap 集成
 *
 * 设计原则:
 * - 轻量级、无外部依赖（不依赖 Recast/Detour，专为 2D 网格游戏设计）
 * - 可从 TilemapComponent 直接构建，也可手动设置网格
 * - 支持运行时动态障碍（添加/移除）
 * - 二叉堆优先队列优化性能
 *
 * 用法:
 * @code
 *   GridPathfinding pathfinder;
 *   pathfinder.BuildFromGrid(20, 20, 1.0f, walkable_table);
 *   std::vector<glm::vec2> path;
 *   pathfinder.FindPath({0, 0}, {10, 5}, path);
 * @endcode
 */

#ifndef DSE_NAVIGATION_GRID_PATHFINDING_H
#define DSE_NAVIGATION_GRID_PATHFINDING_H

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace dse::navigation {

/// 移动方向模式
enum class GridMoveMode : uint8_t {
    Four    = 0,  ///< 4 方向（上下左右）
    Eight   = 1,  ///< 8 方向（含对角线）
};

/// 对角线移动策略
enum class GridDiagonalPolicy : uint8_t {
    Never    = 0,  ///< 不允许穿越角障
    Always   = 1,  ///< 总是允许对角线（即使角邻居是障碍）
    NoCorner = 2,  ///< 仅当两个角邻居都可行走时才允许对角线（防止切角穿墙）
};

/// 网格寻路配置
struct GridPathConfig {
    GridMoveMode move_mode = GridMoveMode::Eight;
    GridDiagonalPolicy diagonal_policy = GridDiagonalPolicy::NoCorner;
    float diagonal_cost = 1.41421356f;  ///< 对角线移动代价（√2）
    bool allow_diagonal = true;
};

/// 2D 网格 A* 寻路器
class GridPathfinding {
public:
    GridPathfinding() = default;
    ~GridPathfinding() = default;

    GridPathfinding(const GridPathfinding&) = delete;
    GridPathfinding& operator=(const GridPathfinding&) = delete;

    // ── 构建 ──

    /// 从手动网格数据构建
    /// @param width 网格列数
    /// @param height 网格行数
    /// @param cell_size 单元格世界尺寸
    /// @param walkable 可行走表（true=可走，false=障碍），长度需 >= width*height
    void BuildFromGrid(int width, int height, float cell_size,
                       const std::vector<bool>& walkable);

    /// 从 walkable 数组构建（C 风格指针版）
    void BuildFromGrid(int width, int height, float cell_size,
                       const bool* walkable, int count);

    /// 从 Tilemap 数据构建（tile_id == 0 或 blocked_tile_ids 中的 ID 视为障碍）
    /// @param width 地图列数
    /// @param height 地图行数
    /// @param cell_size 瓦片尺寸
    /// @param tiles 瓦片 ID 数组（0 = 空/障碍）
    /// @param blocked_tile_ids 额外视为障碍的 tile_id 集合（可为 nullptr）
    /// @param blocked_count blocked_tile_ids 元素数
    void BuildFromTilemap(int width, int height, float cell_size,
                          const int* tiles, int tiles_count,
                          const int* blocked_tile_ids = nullptr,
                          int blocked_count = 0);

    // ── 动态障碍 ──

    /// 设置某格是否可行走（运行时动态障碍）
    void SetBlocked(int x, int y, bool blocked);

    /// 查询某格是否可行走
    bool IsBlocked(int x, int y) const;

    /// 查询某格是否可行走（世界坐标版本）
    bool IsBlockedAt(const glm::vec2& world_pos) const;

    /// 清除所有动态障碍（恢复到构建时的状态需要重建）
    void ClearDynamicBlocks();

    // ── 寻路 ──

    /// A* 路径查找
    /// @param start 世界坐标起点
    /// @param end 世界坐标终点
    /// @param[out] path 输出路径点（世界坐标，含起点和终点）
    /// @param config 寻路配置
    /// @return 找到路径返回 true
    bool FindPath(const glm::vec2& start, const glm::vec2& end,
                  std::vector<glm::vec2>& path,
                  const GridPathConfig& config = {}) const;

    /// 网格坐标版本寻路
    bool FindPathGrid(int sx, int sy, int ex, int ey,
                      std::vector<glm::ivec2>& grid_path,
                      const GridPathConfig& config = {}) const;

    // ── 查询 ──

    bool IsReady() const { return width_ > 0 && height_ > 0; }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    float GetCellSize() const { return cell_size_; }

    /// 世界坐标 → 网格坐标
    glm::ivec2 WorldToGrid(const glm::vec2& world) const;

    /// 网格坐标 → 世界坐标（格中心）
    glm::vec2 GridToWorld(int x, int y) const;

    /// 获取可走表（只读）
    const std::vector<bool>& GetWalkable() const { return walkable_; }

private:
    int width_ = 0;
    int height_ = 0;
    float cell_size_ = 1.0f;
    glm::vec2 origin_ = {0.0f, 0.0f};  ///< 网格原点（左下角）

    std::vector<bool> walkable_;  ///< 可行走表（true=可走）

    /// 检查坐标是否在网格范围内
    bool InBounds(int x, int y) const {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }

    /// 线性索引
    int Index(int x, int y) const {
        return y * width_ + x;
    }

    /// 获取邻居方向
    struct NeighborOffset {
        int dx, dy;
        float cost;
    };
    void GetNeighbors(int x, int y, const GridPathConfig& config,
                      std::vector<NeighborOffset>& out) const;

    /// A* 节点
    struct AStarNode {
        int x, y;
        float g_cost;  ///< 从起点到当前的实际代价
        float f_cost;  ///< g + h
        int parent_x;
        int parent_y;
        bool in_open;
        bool closed;
    };

    /// 二叉堆优先队列元素
    struct HeapElement {
        int x, y;
        float f_cost;
    };

    /// 曼哈顿距离启发函数
    float Heuristic(int sx, int sy, int ex, int ey,
                    const GridPathConfig& config) const;

    /// 对角线穿越检查（NoCorner 策略）
    bool CanMoveDiagonally(int x, int y, int dx, int dy,
                           GridDiagonalPolicy policy) const;
};

} // namespace dse::navigation

#endif // DSE_NAVIGATION_GRID_PATHFINDING_H
