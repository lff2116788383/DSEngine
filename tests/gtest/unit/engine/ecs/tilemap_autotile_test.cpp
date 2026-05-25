/**
 * @file tilemap_autotile_test.cpp
 * @brief Tilemap Auto-Tile 逻辑与 Bresenham Line 算法单元测试
 *
 * 覆盖场景：
 * - Bresenham 直线算法正确性（水平、垂直、对角线、反向）
 * - AutoTile 4-bit 邻居位掩码计算
 * - AutoTile 变体映射
 * - 边界条件处理
 */

#include <gtest/gtest.h>
#include "engine/ecs/tilemap.h"

#include <cmath>
#include <vector>
#include <utility>

namespace {

// ─── 与编辑器 static 函数等价的可测试副本 ─────────────────────────────────────

std::vector<std::pair<int,int>> BresenhamLine(int x0, int y0, int x1, int y1) {
    std::vector<std::pair<int,int>> pts;
    int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    while (true) {
        pts.push_back({x0, y0});
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
    return pts;
}

struct AutoTileRule {
    bool enabled = false;
    int base_tile_id = 1;
    int variant_tiles[16] = {};
};

void AutoTileResolve(TilemapComponent& tm, int cx, int cy, const AutoTileRule& rule) {
    if (!rule.enabled) return;
    if (cx < 0 || cx >= tm.width || cy < 0 || cy >= tm.height) return;
    int idx = cy * tm.width + cx;
    if (tm.tiles[idx] == 0) return;
    int base = rule.base_tile_id;
    bool belongs = false;
    for (int m = 0; m < 16; m++) {
        if (tm.tiles[idx] == rule.variant_tiles[m] || tm.tiles[idx] == base) { belongs = true; break; }
    }
    if (!belongs) return;

    auto is_same = [&](int x, int y) -> bool {
        if (x < 0 || x >= tm.width || y < 0 || y >= tm.height) return false;
        int tid = tm.tiles[y * tm.width + x];
        if (tid == base) return true;
        for (int m = 0; m < 16; m++) { if (tid == rule.variant_tiles[m]) return true; }
        return false;
    };
    int mask = 0;
    if (is_same(cx, cy - 1)) mask |= 1; // Up
    if (is_same(cx + 1, cy)) mask |= 2; // Right
    if (is_same(cx, cy + 1)) mask |= 4; // Down
    if (is_same(cx - 1, cy)) mask |= 8; // Left
    int resolved = rule.variant_tiles[mask];
    if (resolved > 0) tm.tiles[idx] = resolved;
}

} // namespace

// ============================================================
// Bresenham Line Tests
// ============================================================

TEST(BresenhamLineTest, 水平线_左到右) {
    auto pts = BresenhamLine(0, 0, 4, 0);
    ASSERT_EQ(pts.size(), 5u);
    for (int i = 0; i <= 4; i++) {
        EXPECT_EQ(pts[i].first, i);
        EXPECT_EQ(pts[i].second, 0);
    }
}

TEST(BresenhamLineTest, 水平线_右到左) {
    auto pts = BresenhamLine(3, 2, 0, 2);
    ASSERT_EQ(pts.size(), 4u);
    EXPECT_EQ(pts[0], std::make_pair(3, 2));
    EXPECT_EQ(pts[3], std::make_pair(0, 2));
}

TEST(BresenhamLineTest, 垂直线) {
    auto pts = BresenhamLine(1, 0, 1, 5);
    ASSERT_EQ(pts.size(), 6u);
    for (auto& p : pts) EXPECT_EQ(p.first, 1);
    EXPECT_EQ(pts[0].second, 0);
    EXPECT_EQ(pts[5].second, 5);
}

TEST(BresenhamLineTest, 对角线_45度) {
    auto pts = BresenhamLine(0, 0, 3, 3);
    ASSERT_EQ(pts.size(), 4u);
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(pts[i].first, i);
        EXPECT_EQ(pts[i].second, i);
    }
}

TEST(BresenhamLineTest, 单点) {
    auto pts = BresenhamLine(5, 5, 5, 5);
    ASSERT_EQ(pts.size(), 1u);
    EXPECT_EQ(pts[0], std::make_pair(5, 5));
}

TEST(BresenhamLineTest, 斜线_首尾正确) {
    auto pts = BresenhamLine(1, 1, 5, 3);
    EXPECT_EQ(pts.front(), std::make_pair(1, 1));
    EXPECT_EQ(pts.back(), std::make_pair(5, 3));
    // 所有点应该连续（相邻点曼哈顿距离 <= 2）
    for (size_t i = 1; i < pts.size(); i++) {
        int d = std::abs(pts[i].first - pts[i-1].first) + std::abs(pts[i].second - pts[i-1].second);
        EXPECT_LE(d, 2);
    }
}

// ============================================================
// Auto-Tile Tests
// ============================================================

TEST(AutoTileTest, 中心无邻居_mask为0) {
    TilemapComponent tm;
    tm.width = 3; tm.height = 3;
    tm.tiles = {0, 0, 0,  0, 1, 0,  0, 0, 0};

    AutoTileRule rule;
    rule.enabled = true;
    rule.base_tile_id = 1;
    for (int i = 0; i < 16; i++) rule.variant_tiles[i] = 10 + i; // 10..25

    AutoTileResolve(tm, 1, 1, rule);
    // mask = 0 -> variant_tiles[0] = 10
    EXPECT_EQ(tm.tiles[4], 10);
}

TEST(AutoTileTest, 四面包围_mask为15) {
    TilemapComponent tm;
    tm.width = 3; tm.height = 3;
    tm.tiles = {0, 1, 0,  1, 1, 1,  0, 1, 0};

    AutoTileRule rule;
    rule.enabled = true;
    rule.base_tile_id = 1;
    for (int i = 0; i < 16; i++) rule.variant_tiles[i] = 100 + i;

    AutoTileResolve(tm, 1, 1, rule);
    // Up(1)+Right(2)+Down(4)+Left(8) = 15
    EXPECT_EQ(tm.tiles[4], 100 + 15);
}

TEST(AutoTileTest, 上方和右方有邻居_mask为3) {
    TilemapComponent tm;
    tm.width = 3; tm.height = 3;
    tm.tiles = {0, 1, 0,  0, 1, 1,  0, 0, 0};

    AutoTileRule rule;
    rule.enabled = true;
    rule.base_tile_id = 1;
    for (int i = 0; i < 16; i++) rule.variant_tiles[i] = 50 + i;

    AutoTileResolve(tm, 1, 1, rule);
    // Up=1, Right=2 -> mask=3
    EXPECT_EQ(tm.tiles[4], 53);
}

TEST(AutoTileTest, 空格子不解析) {
    TilemapComponent tm;
    tm.width = 3; tm.height = 3;
    tm.tiles = {0, 0, 0,  0, 0, 0,  0, 0, 0};

    AutoTileRule rule;
    rule.enabled = true;
    rule.base_tile_id = 1;
    for (int i = 0; i < 16; i++) rule.variant_tiles[i] = 10 + i;

    AutoTileResolve(tm, 1, 1, rule);
    EXPECT_EQ(tm.tiles[4], 0); // 未改动
}

TEST(AutoTileTest, 边缘格子不越界) {
    TilemapComponent tm;
    tm.width = 2; tm.height = 2;
    tm.tiles = {1, 1,  1, 1};

    AutoTileRule rule;
    rule.enabled = true;
    rule.base_tile_id = 1;
    for (int i = 0; i < 16; i++) rule.variant_tiles[i] = 20 + i;

    // (0,0): Right(1,0)=true, Down(0,1)=true -> mask = 2+4 = 6
    AutoTileResolve(tm, 0, 0, rule);
    EXPECT_EQ(tm.tiles[0], 20 + 6);
}

TEST(AutoTileTest, 规则禁用不修改) {
    TilemapComponent tm;
    tm.width = 3; tm.height = 3;
    tm.tiles = {0, 1, 0,  1, 1, 1,  0, 1, 0};

    AutoTileRule rule;
    rule.enabled = false;
    rule.base_tile_id = 1;
    for (int i = 0; i < 16; i++) rule.variant_tiles[i] = 100 + i;

    AutoTileResolve(tm, 1, 1, rule);
    EXPECT_EQ(tm.tiles[4], 1); // 不变
}
