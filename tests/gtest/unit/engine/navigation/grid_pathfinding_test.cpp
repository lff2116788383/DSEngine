/**
 * @file grid_pathfinding_test.cpp
 * @brief 2D 网格寻路系统单元测试
 *
 * 覆盖：
 * - 基础 A* 寻路（直线、对角线）
 * - 障碍绕行
 * - 4方向 vs 8方向
 * - 对角线策略 (NoCorner / Always / Never)
 * - 动态障碍添加/移除
 * - Tilemap 构建
 * - 无路径场景
 * - 坐标转换
 */

#include <gtest/gtest.h>
#include "engine/navigation/grid_pathfinding.h"

#include <glm/glm.hpp>
#include <vector>

using namespace dse::navigation;

class GridPathfindingTest : public ::testing::Test {
protected:
    GridPathfinding pf;

    // 辅助：创建一个全可走的网格
    void BuildOpenGrid(int w, int h, float cell = 1.0f) {
        std::vector<bool> walkable(static_cast<size_t>(w * h), true);
        pf.BuildFromGrid(w, h, cell, walkable);
    }
};

// ── 基础寻路 ─────────────────────────────────────────────────────────────

TEST_F(GridPathfindingTest, StraightLinePath8Dir) {
    BuildOpenGrid(10, 10);
    std::vector<glm::vec2> path;
    EXPECT_TRUE(pf.FindPath({0.5f, 0.5f}, {5.5f, 0.5f}, path));
    EXPECT_GE(path.size(), 2u);
    // 起点终点正确
    EXPECT_FLOAT_EQ(path.front().x, 0.5f);
    EXPECT_FLOAT_EQ(path.front().y, 0.5f);
    EXPECT_FLOAT_EQ(path.back().x, 5.5f);
    EXPECT_FLOAT_EQ(path.back().y, 0.5f);
}

TEST_F(GridPathfindingTest, DiagonalPath8Dir) {
    BuildOpenGrid(10, 10);
    std::vector<glm::vec2> path;
    EXPECT_TRUE(pf.FindPath({0.5f, 0.5f}, {5.5f, 5.5f}, path));
    EXPECT_GE(path.size(), 2u);
    // 对角线最优路径应该比直线+拐弯短
    // 8方向时对角线一步≈1.414, 走5步对角线≈7.07
}

TEST_F(GridPathfindingTest, SamePoint) {
    BuildOpenGrid(5, 5);
    std::vector<glm::vec2> path;
    EXPECT_TRUE(pf.FindPath({2.5f, 2.5f}, {2.5f, 2.5f}, path));
    EXPECT_EQ(path.size(), 1u);
}

TEST_F(GridPathfindingTest, FourDirNoDiagonal) {
    BuildOpenGrid(10, 10);
    GridPathConfig config;
    config.move_mode = GridMoveMode::Four;
    std::vector<glm::vec2> path;
    EXPECT_TRUE(pf.FindPath({0.5f, 0.5f}, {3.5f, 3.5f}, path, config));
    // 4方向时至少需要 6 步 (3+3)
    EXPECT_GE(path.size(), 7u);  // 含起点
}

// ── 障碍 ─────────────────────────────────────────────────────────────────

TEST_F(GridPathfindingTest, WallObstacle) {
    // 10x10 网格，中间有一堵墙（y=5, x=0..8）
    std::vector<bool> walkable(100, true);
    for (int x = 0; x < 9; ++x)
        walkable[5 * 10 + x] = false;  // y=5 行
    pf.BuildFromGrid(10, 10, 1.0f, walkable);

    std::vector<glm::vec2> path;
    EXPECT_TRUE(pf.FindPath({0.5f, 0.5f}, {0.5f, 8.5f}, path));
    EXPECT_GE(path.size(), 2u);
    // 路径必须绕过墙（通过 x=9 的缺口）
    // 检查路径中没有穿过墙
    for (const auto& p : path) {
        glm::ivec2 g = pf.WorldToGrid(p);
        if (g.y == 5) {
            EXPECT_EQ(g.x, 9);  // 只能从 x=9 穿过
        }
    }
}

TEST_F(GridPathfindingTest, NoPathWhenFullyBlocked) {
    // 3x3 网格，中间全是障碍
    std::vector<bool> walkable(9, false);
    pf.BuildFromGrid(3, 3, 1.0f, walkable);

    std::vector<glm::vec2> path;
    EXPECT_FALSE(pf.FindPath({0.5f, 0.5f}, {2.5f, 2.5f}, path));
}

TEST_F(GridPathfindingTest, NoPathWhenSurrounded) {
    // 5x5 网格，起点周围被墙围住
    std::vector<bool> walkable(25, true);
    // 围住 (2,2)
    walkable[1 * 5 + 2] = false;  // 上
    walkable[3 * 5 + 2] = false;  // 下
    walkable[2 * 5 + 1] = false;  // 左
    walkable[2 * 5 + 3] = false;  // 右
    // 对角线邻居也堵（NoCorner策略下墙角穿越会被阻止）
    walkable[1 * 5 + 1] = false;
    walkable[1 * 5 + 3] = false;
    walkable[3 * 5 + 1] = false;
    walkable[3 * 5 + 3] = false;
    pf.BuildFromGrid(5, 5, 1.0f, walkable);

    std::vector<glm::vec2> path;
    EXPECT_FALSE(pf.FindPath({2.5f, 2.5f}, {4.5f, 4.5f}, path));
}

// ── 动态障碍 ──────────────────────────────────────────────────────────────

TEST_F(GridPathfindingTest, DynamicBlockUnblock) {
    BuildOpenGrid(10, 10);
    std::vector<glm::vec2> path;
    EXPECT_TRUE(pf.FindPath({0.5f, 0.5f}, {5.5f, 5.5f}, path));

    // 在路径中间放障碍
    pf.SetBlocked(3, 3, true);
    EXPECT_TRUE(pf.IsBlocked(3, 3));

    // 移除障碍
    pf.SetBlocked(3, 3, false);
    EXPECT_FALSE(pf.IsBlocked(3, 3));

    // 再找路径
    path.clear();
    EXPECT_TRUE(pf.FindPath({0.5f, 0.5f}, {5.5f, 5.5f}, path));
}

TEST_F(GridPathfindingTest, IsBlockedAtWorldCoord) {
    BuildOpenGrid(5, 5, 2.0f);
    pf.SetBlocked(2, 2, true);
    EXPECT_TRUE(pf.IsBlockedAt({5.0f, 5.0f}));  // 格(2,2)中心 = (5,5)
    EXPECT_FALSE(pf.IsBlockedAt({0.5f, 0.5f}));
}

// ── 对角线策略 ────────────────────────────────────────────────────────────

TEST_F(GridPathfindingTest, NoCornerPreventsCornerCutting) {
    // L 形墙：在(2,1)和(1,2)放障碍，验证 NoCorner 不允许从(1,1)对角到(3,3)
    // 用 5x5 网格让 A* 有绕行空间
    std::vector<bool> walkable(25, true);
    walkable[1 * 5 + 2] = false;  // (2,1)
    walkable[2 * 5 + 1] = false;  // (1,2)
    pf.BuildFromGrid(5, 5, 1.0f, walkable);

    GridPathConfig config;
    config.diagonal_policy = GridDiagonalPolicy::NoCorner;
    std::vector<glm::vec2> path;
    // 从(1,1)到(3,3)，NoCorner 下不能直接对角穿越 (2,1)-(1,2) 的角
    // 但可以绕行，比如 (1,1)->(0,1)->(0,2)->(0,3)->(1,3)->(2,3)->(3,3)
    EXPECT_TRUE(pf.FindPath({1.5f, 1.5f}, {3.5f, 3.5f}, path, config));
    // 路径不应直接对角穿越（至少 3 个点）
    EXPECT_GE(path.size(), 3u);
}

TEST_F(GridPathfindingTest, AlwaysPolicyAllowsCornerCutting) {
    // 同样的 L 形墙，5x5 网格
    std::vector<bool> walkable(25, true);
    walkable[0 * 5 + 1] = false;  // (1,0)
    walkable[1 * 5 + 0] = false;  // (0,1)
    pf.BuildFromGrid(5, 5, 1.0f, walkable);

    GridPathConfig config;
    config.diagonal_policy = GridDiagonalPolicy::Always;
    std::vector<glm::vec2> path;
    EXPECT_TRUE(pf.FindPath({0.5f, 0.5f}, {1.5f, 1.5f}, path, config));
    // Always 策略下可以直接对角穿越（路径更短）
    EXPECT_LE(path.size(), 3u);
}

// ── Tilemap 构建 ──────────────────────────────────────────────────────────

TEST_F(GridPathfindingTest, BuildFromTilemap) {
    // 5x5 地图，tile_id 0 = 空地（障碍），1 = 地面（可走）
    std::vector<int> tiles = {
        1, 1, 1, 1, 1,
        1, 0, 0, 0, 1,
        1, 0, 1, 0, 1,
        1, 0, 0, 0, 1,
        1, 1, 1, 1, 1
    };
    pf.BuildFromTilemap(5, 5, 1.0f, tiles.data(), static_cast<int>(tiles.size()));

    // (1,1) 是 0 = 障碍
    EXPECT_TRUE(pf.IsBlocked(1, 1));
    // (0,0) 是 1 = 可走
    EXPECT_FALSE(pf.IsBlocked(0, 0));
    // (2,2) 是 1 = 可走
    EXPECT_FALSE(pf.IsBlocked(2, 2));

    // 寻路绕行
    std::vector<glm::vec2> path;
    EXPECT_TRUE(pf.FindPath({0.5f, 0.5f}, {4.5f, 4.5f}, path));
}

TEST_F(GridPathfindingTest, BuildFromTilemapWithBlockedIds) {
    // tile_id 2 也视为障碍
    std::vector<int> tiles = {
        1, 1, 2, 1, 1,
        1, 1, 1, 1, 1,
        1, 1, 1, 1, 1
    };
    int blocked_ids[] = {2};
    pf.BuildFromTilemap(5, 3, 1.0f, tiles.data(), static_cast<int>(tiles.size()),
                        blocked_ids, 1);

    EXPECT_TRUE(pf.IsBlocked(2, 0));  // tile_id=2 被标记为障碍
    EXPECT_FALSE(pf.IsBlocked(0, 0));
}

// ── 坐标转换 ──────────────────────────────────────────────────────────────

TEST_F(GridPathfindingTest, WorldToGridConversion) {
    BuildOpenGrid(10, 10, 1.0f);
    auto g = pf.WorldToGrid({3.5f, 7.5f});
    EXPECT_EQ(g.x, 3);
    EXPECT_EQ(g.y, 7);
}

TEST_F(GridPathfindingTest, GridToWorldConversion) {
    BuildOpenGrid(10, 10, 2.0f);
    auto w = pf.GridToWorld(3, 4);
    // 格中心 = (3 + 0.5) * 2 = 7, (4 + 0.5) * 2 = 9
    EXPECT_FLOAT_EQ(w.x, 7.0f);
    EXPECT_FLOAT_EQ(w.y, 9.0f);
}

TEST_F(GridPathfindingTest, WorldToGridClamping) {
    BuildOpenGrid(5, 5, 1.0f);
    // 超出范围的坐标被钳制
    auto g = pf.WorldToGrid({-10.0f, 100.0f});
    EXPECT_EQ(g.x, 0);
    EXPECT_EQ(g.y, 4);
}

// ── 边界情况 ──────────────────────────────────────────────────────────────

TEST_F(GridPathfindingTest, NotReadyWhenUnbuilt) {
    GridPathfinding empty_pf;
    EXPECT_FALSE(empty_pf.IsReady());
    std::vector<glm::vec2> path;
    EXPECT_FALSE(empty_pf.FindPath({0, 0}, {1, 1}, path));
}

TEST_F(GridPathfindingTest, StartOnBlockedFindsNearby) {
    // 起点被堵时自动搜索附近可走格子
    std::vector<bool> walkable(25, true);
    walkable[2 * 5 + 2] = false;  // (2,2) 被堵
    pf.BuildFromGrid(5, 5, 1.0f, walkable);

    std::vector<glm::vec2> path;
    // 起点恰好在障碍格
    EXPECT_TRUE(pf.FindPath({2.5f, 2.5f}, {4.5f, 4.5f}, path));
}

TEST_F(GridPathfindingTest, GridPathVersion) {
    BuildOpenGrid(10, 10);
    std::vector<glm::ivec2> grid_path;
    EXPECT_TRUE(pf.FindPathGrid(0, 0, 5, 5, grid_path));
    EXPECT_GE(grid_path.size(), 2u);
    EXPECT_EQ(grid_path.front().x, 0);
    EXPECT_EQ(grid_path.front().y, 0);
    EXPECT_EQ(grid_path.back().x, 5);
    EXPECT_EQ(grid_path.back().y, 5);
}

// ── 性能：大网格寻路 ────────────────────────────────────────────────────────

TEST_F(GridPathfindingTest, LargeGridPerformance) {
    // 50x50 网格，中间随机放一些障碍
    const int N = 50;
    std::vector<bool> walkable(N * N, true);
    // 放一些障碍条
    for (int y = 10; y < 40; ++y) {
        walkable[y * N + 25] = false;
    }
    for (int x = 10; x < 40; ++x) {
        walkable[30 * N + x] = false;
    }
    // 留个口
    walkable[30 * N + 25] = true;
    pf.BuildFromGrid(N, N, 1.0f, walkable);

    std::vector<glm::vec2> path;
    EXPECT_TRUE(pf.FindPath({1.5f, 1.5f}, {48.5f, 48.5f}, path));
    EXPECT_GE(path.size(), 2u);
}
