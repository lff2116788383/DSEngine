/**
 * @file grid_pathfinding.cpp
 * @brief 2D 网格寻路系统 — A* 算法实现
 */

#include "engine/navigation/grid_pathfinding.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace dse::navigation {

// ── 构建 ──────────────────────────────────────────────────────────────────

void GridPathfinding::BuildFromGrid(int width, int height, float cell_size,
                                    const std::vector<bool>& walkable) {
    width_ = width;
    height_ = height;
    cell_size_ = cell_size;
    const int total = width * height;
    walkable_.resize(static_cast<size_t>(total));
    for (int i = 0; i < total && i < static_cast<int>(walkable.size()); ++i)
        walkable_[static_cast<size_t>(i)] = walkable[static_cast<size_t>(i)];
    // 如果输入不足，剩余默认可走
    for (int i = static_cast<int>(walkable.size()); i < total; ++i)
        walkable_[static_cast<size_t>(i)] = true;
}

void GridPathfinding::BuildFromGrid(int width, int height, float cell_size,
                                    const bool* walkable, int count) {
    width_ = width;
    height_ = height;
    cell_size_ = cell_size;
    const int total = width * height;
    walkable_.resize(static_cast<size_t>(total));
    if (walkable && count >= total) {
        for (int i = 0; i < total; ++i)
            walkable_[static_cast<size_t>(i)] = walkable[i];
    } else {
        // 默认全部可走
        std::fill(walkable_.begin(), walkable_.end(), true);
    }
}

void GridPathfinding::BuildFromTilemap(int width, int height, float cell_size,
                                      const int* tiles, int tiles_count,
                                      const int* blocked_tile_ids,
                                      int blocked_count) {
    width_ = width;
    height_ = height;
    cell_size_ = cell_size;
    const int total = width * height;
    walkable_.resize(static_cast<size_t>(total));

    // 构建 blocked 集合
    auto is_blocked_id = [&](int tile_id) -> bool {
        if (tile_id <= 0) return true;  // 0 = 空/障碍
        if (blocked_tile_ids) {
            for (int i = 0; i < blocked_count; ++i) {
                if (blocked_tile_ids[i] == tile_id) return true;
            }
        }
        return false;
    };

    for (int i = 0; i < total && i < tiles_count; ++i) {
        walkable_[i] = !is_blocked_id(tiles[i]);
    }
    // 如果 tiles 数组不够长，剩余默认可走
    for (int i = total > tiles_count ? tiles_count : total; i < total; ++i) {
        walkable_[i] = true;
    }
}

// ── 动态障碍 ──────────────────────────────────────────────────────────────

void GridPathfinding::SetBlocked(int x, int y, bool blocked) {
    if (!InBounds(x, y)) return;
    walkable_[Index(x, y)] = !blocked;
}

bool GridPathfinding::IsBlocked(int x, int y) const {
    if (!InBounds(x, y)) return true;
    return !walkable_[Index(x, y)];
}

bool GridPathfinding::IsBlockedAt(const glm::vec2& world_pos) const {
    glm::ivec2 grid = WorldToGrid(world_pos);
    return IsBlocked(grid.x, grid.y);
}

void GridPathfinding::ClearDynamicBlocks() {
    // 无原始快照，仅重置为全可走；需重建恢复初始状态
    std::fill(walkable_.begin(), walkable_.end(), true);
}

// ── 坐标转换 ──────────────────────────────────────────────────────────────

glm::ivec2 GridPathfinding::WorldToGrid(const glm::vec2& world) const {
    if (!IsReady()) return {0, 0};
    int x = static_cast<int>(std::floor((world.x - origin_.x) / cell_size_));
    int y = static_cast<int>(std::floor((world.y - origin_.y) / cell_size_));
    // 钳制到有效范围
    x = std::clamp(x, 0, width_ - 1);
    y = std::clamp(y, 0, height_ - 1);
    return {x, y};
}

glm::vec2 GridPathfinding::GridToWorld(int x, int y) const {
    return {
        origin_.x + (static_cast<float>(x) + 0.5f) * cell_size_,
        origin_.y + (static_cast<float>(y) + 0.5f) * cell_size_
    };
}

// ── 邻居 & 启发函数 ────────────────────────────────────────────────────────

void GridPathfinding::GetNeighbors(int x, int y, const GridPathConfig& config,
                                    std::vector<NeighborOffset>& out) const {
    out.clear();
    // 4 方向
    static const NeighborOffset four[] = {
        {0, -1, 1.0f}, {0, 1, 1.0f}, {-1, 0, 1.0f}, {1, 0, 1.0f}
    };
    // 8 方向（4 正交 + 4 对角线）
    static const NeighborOffset eight[] = {
        {0, -1, 1.0f}, {0, 1, 1.0f}, {-1, 0, 1.0f}, {1, 0, 1.0f},
        {-1, -1, 1.41421356f}, {1, -1, 1.41421356f},
        {-1, 1, 1.41421356f},  {1, 1, 1.41421356f}
    };

    const NeighborOffset* dirs = nullptr;
    int dir_count = 0;
    if (config.move_mode == GridMoveMode::Four || !config.allow_diagonal) {
        dirs = four;
        dir_count = 4;
    } else {
        dirs = eight;
        dir_count = 8;
    }

    for (int i = 0; i < dir_count; ++i) {
        const auto& d = dirs[i];
        int nx = x + d.dx;
        int ny = y + d.dy;
        if (!InBounds(nx, ny)) continue;
        if (!walkable_[Index(nx, ny)]) continue;

        // 对角线穿越检查
        if (d.dx != 0 && d.dy != 0) {
            if (!CanMoveDiagonally(x, y, d.dx, d.dy, config.diagonal_policy)) continue;
        }
        out.push_back(d);
    }
}

float GridPathfinding::Heuristic(int sx, int sy, int ex, int ey,
                                  const GridPathConfig& config) const {
    int dx = std::abs(sx - ex);
    int dy = std::abs(sy - ey);
    if (config.move_mode == GridMoveMode::Eight && config.allow_diagonal) {
        // 对角线距离
        int diag = std::min(dx, dy);
        int straight = std::max(dx, dy) - diag;
        return static_cast<float>(diag) * config.diagonal_cost
             + static_cast<float>(straight);
    }
    // 曼哈顿距离
    return static_cast<float>(dx + dy);
}

bool GridPathfinding::CanMoveDiagonally(int x, int y, int dx, int dy,
                                         GridDiagonalPolicy policy) const {
    if (policy == GridDiagonalPolicy::Always)
        return true;
    if (policy == GridDiagonalPolicy::Never)
        return false;
    // NoCorner: 两个正交邻居都必须可行走才允许对角线
    // 防止从墙角穿过
    bool ok1 = InBounds(x + dx, y) && walkable_[Index(x + dx, y)];
    bool ok2 = InBounds(x, y + dy) && walkable_[Index(x, y + dy)];
    return ok1 && ok2;
}

// ── A* 核心寻路 ────────────────────────────────────────────────────────────

bool GridPathfinding::FindPath(const glm::vec2& start, const glm::vec2& end,
                                std::vector<glm::vec2>& path,
                                const GridPathConfig& config) const {
    glm::ivec2 start_grid = WorldToGrid(start);
    glm::ivec2 end_grid = WorldToGrid(end);

    // 起点或终点被阻挡
    if (IsBlocked(start_grid.x, start_grid.y)) {
        // 尝试在周围找一个可走的格子
        bool found = false;
        for (int r = 1; r <= 3 && !found; ++r) {
            for (int dy = -r; dy <= r && !found; ++dy) {
                for (int dx = -r; dx <= r && !found; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = start_grid.x + dx;
                    int ny = start_grid.y + dy;
                    if (InBounds(nx, ny) && walkable_[Index(nx, ny)]) {
                        start_grid = {nx, ny};
                        found = true;
                    }
                }
            }
        }
        if (!found) return false;
    }

    if (IsBlocked(end_grid.x, end_grid.y)) {
        bool found = false;
        for (int r = 1; r <= 3 && !found; ++r) {
            for (int dy = -r; dy <= r && !found; ++dy) {
                for (int dx = -r; dx <= r && !found; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = end_grid.x + dx;
                    int ny = end_grid.y + dy;
                    if (InBounds(nx, ny) && walkable_[Index(nx, ny)]) {
                        end_grid = {nx, ny};
                        found = true;
                    }
                }
            }
        }
        if (!found) return false;
    }

    std::vector<glm::ivec2> grid_path;
    if (!FindPathGrid(start_grid.x, start_grid.y, end_grid.x, end_grid.y,
                      grid_path, config)) {
        return false;
    }

    // 转换为世界坐标
    path.clear();
    path.reserve(grid_path.size());
    for (const auto& gp : grid_path) {
        path.push_back(GridToWorld(gp.x, gp.y));
    }

    // 如果起终点不在格中心，用实际坐标替换首尾
    if (!path.empty()) {
        path[0] = start;
        path.back() = end;
    }
    return !path.empty();
}

bool GridPathfinding::FindPathGrid(int sx, int sy, int ex, int ey,
                                    std::vector<glm::ivec2>& grid_path,
                                    const GridPathConfig& config) const {
    if (!IsReady()) return false;
    if (!InBounds(sx, sy) || !InBounds(ex, ey)) return false;
    if (IsBlocked(sx, sy) || IsBlocked(ex, ey)) return false;
    if (sx == ex && sy == ey) {
        grid_path.clear();
        grid_path.push_back({sx, sy});
        return true;
    }

    const int total = width_ * height_;

    // 节点池
    std::vector<AStarNode> nodes(static_cast<size_t>(total));
    for (int i = 0; i < total; ++i) {
        nodes[i].x = i % width_;
        nodes[i].y = i / width_;
        nodes[i].g_cost = std::numeric_limits<float>::max();
        nodes[i].f_cost = std::numeric_limits<float>::max();
        nodes[i].parent_x = -1;
        nodes[i].parent_y = -1;
        nodes[i].in_open = false;
        nodes[i].closed = false;
    }

    // 二叉堆（最小堆，按 f_cost 排序）
    // 使用 vector + push_heap/pop_heap 实现
    std::vector<HeapElement> open_heap;

    auto node_idx = [&](int x, int y) { return Index(x, y); };

    // 起点入堆
    int start_idx = node_idx(sx, sy);
    nodes[start_idx].g_cost = 0.0f;
    nodes[start_idx].f_cost = Heuristic(sx, sy, ex, ey, config);
    nodes[start_idx].in_open = true;
    open_heap.push_back({sx, sy, nodes[start_idx].f_cost});
    std::push_heap(open_heap.begin(), open_heap.end(),
                   [](const HeapElement& a, const HeapElement& b) {
                       return a.f_cost > b.f_cost;  // min-heap
                   });

    std::vector<NeighborOffset> neighbors;

    while (!open_heap.empty()) {
        // 取出 f 最小的节点
        std::pop_heap(open_heap.begin(), open_heap.end(),
                      [](const HeapElement& a, const HeapElement& b) {
                          return a.f_cost > b.f_cost;
                      });
        HeapElement current = open_heap.back();
        open_heap.pop_back();

        int cur_idx = node_idx(current.x, current.y);
        if (nodes[cur_idx].closed) continue;  // 已处理
        nodes[cur_idx].closed = true;
        nodes[cur_idx].in_open = false;

        // 到达终点
        if (current.x == ex && current.y == ey) {
            // 回溯路径
            grid_path.clear();
            int cx = ex, cy = ey;
            while (cx != -1 && cy != -1) {
                grid_path.push_back({cx, cy});
                int idx = node_idx(cx, cy);
                int px = nodes[idx].parent_x;
                int py = nodes[idx].parent_y;
                cx = px;
                cy = py;
            }
            std::reverse(grid_path.begin(), grid_path.end());
            return true;
        }

        // 展开邻居
        GetNeighbors(current.x, current.y, config, neighbors);
        for (const auto& n : neighbors) {
            int nx = current.x + n.dx;
            int ny = current.y + n.dy;
            int n_idx = node_idx(nx, ny);
            if (nodes[n_idx].closed) continue;

            float tentative_g = nodes[cur_idx].g_cost + n.cost;
            if (tentative_g < nodes[n_idx].g_cost) {
                nodes[n_idx].parent_x = current.x;
                nodes[n_idx].parent_y = current.y;
                nodes[n_idx].g_cost = tentative_g;
                nodes[n_idx].f_cost = tentative_g + Heuristic(nx, ny, ex, ey, config);

                if (!nodes[n_idx].in_open) {
                    nodes[n_idx].in_open = true;
                    open_heap.push_back({nx, ny, nodes[n_idx].f_cost});
                    std::push_heap(open_heap.begin(), open_heap.end(),
                                   [](const HeapElement& a, const HeapElement& b) {
                                       return a.f_cost > b.f_cost;
                                   });
                } else {
                    // 节点已在堆中，更新 f_cost（通过移除旧条目+重新插入实现）
                    // 简化实现：直接插入新条目（旧的会在弹出时被 closed 跳过）
                    open_heap.push_back({nx, ny, nodes[n_idx].f_cost});
                    std::push_heap(open_heap.begin(), open_heap.end(),
                                   [](const HeapElement& a, const HeapElement& b) {
                                       return a.f_cost > b.f_cost;
                                   });
                }
            }
        }
    }

    // 未找到路径
    return false;
}

} // namespace dse::navigation
