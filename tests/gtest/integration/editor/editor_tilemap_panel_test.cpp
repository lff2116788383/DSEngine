/**
 * @file editor_tilemap_panel_test.cpp
 * @brief 瓦片地图编辑纯核心（editor_tilemap_panel_core）的无头测试。
 *
 * 覆盖：WorldToTilemapCell（世界→格子下标 + 范围判定）、FloodFillTiles（连通区域洪水填充）、
 * AutoTileResolve / AutoTileResolveNeighbours（4 邻位 bitmask → 变体 id）、
 * BresenhamLine（整数格子直线序列）。ImGui 调色板 / 网格叠加 / 视口接线不在此覆盖。
 */

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "editor_tilemap_panel.h"
#include "editor_tilemap_panel_core.h"
#include "engine/ecs/tilemap.h"
#include "engine/ecs/transform.h"

using namespace dse;
using namespace dse::editor;

namespace {
// width x height、tile_size=1、全 0（空）瓦片地图。
TilemapComponent MakeTilemap(int w, int h, int fill = 0) {
    TilemapComponent tm;
    tm.width = w;
    tm.height = h;
    tm.tile_size = 1.0f;
    tm.tiles.assign(static_cast<size_t>(w) * h, fill);
    return tm;
}

TransformComponent MakeTransformAt(float x, float y, float z) {
    TransformComponent tf;
    tf.position = glm::vec3(x, y, z);
    return tf;
}

int CountTiles(const TilemapComponent& tm, int id) {
    int n = 0;
    for (int t : tm.tiles) if (t == id) n++;
    return n;
}
}  // namespace

// ── WorldToTilemapCell ───────────────────────────────────────────────────────

TEST(TilemapCore, WorldToCellCenterOrigin) {
    auto tm = MakeTilemap(4, 4);
    auto tf = MakeTransformAt(0, 0, 0);  // 地图中心在原点，4x4 → 占 [-2,2]
    int cx, cy;
    EXPECT_TRUE(WorldToTilemapCell(glm::vec3(-2.0f, -2.0f, 0), tm, tf, cx, cy));
    EXPECT_EQ(cx, 0);
    EXPECT_EQ(cy, 0);
    EXPECT_TRUE(WorldToTilemapCell(glm::vec3(1.5f, 1.5f, 0), tm, tf, cx, cy));
    EXPECT_EQ(cx, 3);
    EXPECT_EQ(cy, 3);
}

TEST(TilemapCore, WorldToCellOutsideReturnsFalse) {
    auto tm = MakeTilemap(4, 4);
    auto tf = MakeTransformAt(0, 0, 0);
    int cx, cy;
    EXPECT_FALSE(WorldToTilemapCell(glm::vec3(100.0f, 0, 0), tm, tf, cx, cy));
    EXPECT_FALSE(WorldToTilemapCell(glm::vec3(-100.0f, 0, 0), tm, tf, cx, cy));
}

TEST(TilemapCore, WorldToCellRespectsTransform) {
    auto tm = MakeTilemap(4, 4);
    auto tf = MakeTransformAt(10, 10, 0);
    int cx, cy;
    EXPECT_TRUE(WorldToTilemapCell(glm::vec3(10.0f, 10.0f, 0), tm, tf, cx, cy));
    EXPECT_EQ(cx, 2);  // 中心格
    EXPECT_EQ(cy, 2);
}

// ── FloodFillTiles ───────────────────────────────────────────────────────────

TEST(TilemapCore, FloodFillEmptyMapFillsAll) {
    auto tm = MakeTilemap(5, 5, 0);
    FloodFillTiles(tm, 2, 2, 7);
    EXPECT_EQ(CountTiles(tm, 7), 25);
}

TEST(TilemapCore, FloodFillStopsAtBoundary) {
    auto tm = MakeTilemap(5, 5, 0);
    // 竖墙：x=2 整列设为 9，把地图分成两半
    for (int y = 0; y < 5; y++) tm.tiles[y * 5 + 2] = 9;
    FloodFillTiles(tm, 0, 0, 3);  // 从左半填 3
    // 左半 (x=0,1) 共 10 格变 3；墙不变；右半仍 0
    EXPECT_EQ(CountTiles(tm, 3), 10);
    EXPECT_EQ(CountTiles(tm, 9), 5);
    EXPECT_EQ(CountTiles(tm, 0), 10);
}

TEST(TilemapCore, FloodFillSameIdIsNoOp) {
    auto tm = MakeTilemap(4, 4, 5);
    FloodFillTiles(tm, 0, 0, 5);  // 目标==fill → 直接返回
    EXPECT_EQ(CountTiles(tm, 5), 16);
}

TEST(TilemapCore, FloodFillOutOfBoundsIsNoOp) {
    auto tm = MakeTilemap(4, 4, 0);
    FloodFillTiles(tm, -1, 0, 9);
    FloodFillTiles(tm, 0, 99, 9);
    EXPECT_EQ(CountTiles(tm, 9), 0);
}

// ── BresenhamLine ────────────────────────────────────────────────────────────

TEST(TilemapCore, BresenhamSinglePoint) {
    auto pts = BresenhamLine(3, 3, 3, 3);
    ASSERT_EQ(pts.size(), 1u);
    EXPECT_EQ(pts[0], std::make_pair(3, 3));
}

TEST(TilemapCore, BresenhamHorizontalLine) {
    auto pts = BresenhamLine(0, 2, 4, 2);
    ASSERT_EQ(pts.size(), 5u);
    EXPECT_EQ(pts.front(), std::make_pair(0, 2));
    EXPECT_EQ(pts.back(), std::make_pair(4, 2));
    for (auto& p : pts) EXPECT_EQ(p.second, 2);
}

TEST(TilemapCore, BresenhamDiagonalLine) {
    auto pts = BresenhamLine(0, 0, 3, 3);
    ASSERT_EQ(pts.size(), 4u);
    EXPECT_EQ(pts[0], std::make_pair(0, 0));
    EXPECT_EQ(pts[1], std::make_pair(1, 1));
    EXPECT_EQ(pts[2], std::make_pair(2, 2));
    EXPECT_EQ(pts[3], std::make_pair(3, 3));
}

TEST(TilemapCore, BresenhamReversedEndpoints) {
    auto pts = BresenhamLine(4, 2, 0, 2);
    ASSERT_EQ(pts.size(), 5u);
    EXPECT_EQ(pts.front(), std::make_pair(4, 2));
    EXPECT_EQ(pts.back(), std::make_pair(0, 2));
}

// ── AutoTileResolve ──────────────────────────────────────────────────────────

namespace {
// base=1，variant_tiles[mask] = 100+mask，方便断言 mask。
AutoTileRule MakeAutoRule() {
    AutoTileRule r;
    r.enabled = true;
    r.base_tile_id = 1;
    for (int m = 0; m < 16; m++) r.variant_tiles[m] = 100 + m;
    return r;
}
}  // namespace

TEST(TilemapCore, AutoTileIsolatedCellMaskZero) {
    auto tm = MakeTilemap(3, 3, 0);
    auto rule = MakeAutoRule();
    tm.tiles[1 * 3 + 1] = 1;  // 中心是 base，四周空
    AutoTileResolve(tm, 1, 1, rule);
    // 无同类邻居 → mask 0 → variant_tiles[0] = 100
    EXPECT_EQ(tm.tiles[1 * 3 + 1], 100);
}

TEST(TilemapCore, AutoTileFullNeighboursMask15) {
    auto tm = MakeTilemap(3, 3, 1);  // 全 base
    auto rule = MakeAutoRule();
    AutoTileResolve(tm, 1, 1, rule);
    // U/R/D/L 都同类 → mask 15 → variant_tiles[15] = 115
    EXPECT_EQ(tm.tiles[1 * 3 + 1], 115);
}

TEST(TilemapCore, AutoTileSkipsEmptyAndNonMemberCells) {
    auto tm = MakeTilemap(3, 3, 0);
    auto rule = MakeAutoRule();
    AutoTileResolve(tm, 1, 1, rule);          // 空格不动
    EXPECT_EQ(tm.tiles[1 * 3 + 1], 0);
    tm.tiles[1 * 3 + 1] = 999;                // 非 auto-tile 集合
    AutoTileResolve(tm, 1, 1, rule);
    EXPECT_EQ(tm.tiles[1 * 3 + 1], 999);
}

TEST(TilemapCore, AutoTileDisabledRuleIsNoOp) {
    auto tm = MakeTilemap(3, 3, 1);
    auto rule = MakeAutoRule();
    rule.enabled = false;
    AutoTileResolve(tm, 1, 1, rule);
    EXPECT_EQ(tm.tiles[1 * 3 + 1], 1);  // 未变
}

TEST(TilemapCore, AutoTileResolveNeighboursUpdates3x3) {
    auto tm = MakeTilemap(3, 3, 1);  // 全 base
    auto rule = MakeAutoRule();
    AutoTileResolveNeighbours(tm, 1, 1, rule);
    // 中心四邻全同类 → 115；角格仅两邻 → 不会是 base 了
    EXPECT_EQ(tm.tiles[1 * 3 + 1], 115);
    for (int t : tm.tiles) EXPECT_GE(t, 100);  // 全部被解析为 variant
}
