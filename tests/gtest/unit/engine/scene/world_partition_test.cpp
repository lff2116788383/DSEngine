/**
 * @file world_partition_test.cpp
 * @brief 世界分区流式加载系统单元测试
 */

#include <gtest/gtest.h>
#include "engine/scene/world_partition.h"
#include <cmath>

using namespace dse;

class WorldPartitionTest : public ::testing::Test {
protected:
    WorldPartitionSystem system;
};

// ─── 坐标转换测试 ──────────────────────────────────────────────────────────

TEST_F(WorldPartitionTest, WorldToCell_Origin) {
    auto coord = system.WorldToCell(glm::vec3(0.0f, 0.0f, 0.0f), 128.0f);
    EXPECT_EQ(coord.x, 0);
    EXPECT_EQ(coord.y, 0);
}

TEST_F(WorldPartitionTest, WorldToCell_Positive) {
    auto coord = system.WorldToCell(glm::vec3(200.0f, 0.0f, 300.0f), 128.0f);
    EXPECT_EQ(coord.x, 1);  // 200/128 = 1.56 → floor = 1
    EXPECT_EQ(coord.y, 2);  // 300/128 = 2.34 → floor = 2
}

TEST_F(WorldPartitionTest, WorldToCell_Negative) {
    auto coord = system.WorldToCell(glm::vec3(-100.0f, 0.0f, -200.0f), 128.0f);
    EXPECT_EQ(coord.x, -1);  // -100/128 = -0.78 → floor = -1
    EXPECT_EQ(coord.y, -2);  // -200/128 = -1.56 → floor = -2
}

TEST_F(WorldPartitionTest, CellToWorld_Center) {
    auto pos = system.CellToWorld(CellCoord{0, 0}, 128.0f);
    EXPECT_FLOAT_EQ(pos.x, 64.0f);   // (0 + 0.5) * 128 = 64
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
    EXPECT_FLOAT_EQ(pos.z, 64.0f);
}

TEST_F(WorldPartitionTest, CellToWorld_Negative) {
    auto pos = system.CellToWorld(CellCoord{-1, -2}, 128.0f);
    EXPECT_FLOAT_EQ(pos.x, -64.0f);   // (-1 + 0.5) * 128 = -64
    EXPECT_FLOAT_EQ(pos.z, -192.0f);  // (-2 + 0.5) * 128 = -192
}

TEST_F(WorldPartitionTest, WorldToCell_RoundTrip) {
    // 从 cell 中心转回应该得到相同 cell
    CellCoord original{3, -5};
    glm::vec3 world_center = system.CellToWorld(original, 64.0f);
    auto back = system.WorldToCell(world_center, 64.0f);
    EXPECT_EQ(back.x, original.x);
    EXPECT_EQ(back.y, original.y);
}

// ─── CellCoord 哈希和比较测试 ──────────────────────────────────────────────

TEST_F(WorldPartitionTest, CellCoord_Equality) {
    CellCoord a{1, 2};
    CellCoord b{1, 2};
    CellCoord c{1, 3};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST_F(WorldPartitionTest, CellCoordHash_Distinct) {
    CellCoordHash hasher;
    EXPECT_NE(hasher(CellCoord{0, 0}), hasher(CellCoord{1, 0}));
    EXPECT_NE(hasher(CellCoord{0, 0}), hasher(CellCoord{0, 1}));
}

// ─── 系统状态测试 ──────────────────────────────────────────────────────────

TEST_F(WorldPartitionTest, InitialState_NoCells) {
    EXPECT_EQ(system.LoadedCellCount(), 0u);
    EXPECT_TRUE(system.GetCells().empty());
}

TEST_F(WorldPartitionTest, Shutdown_ClearsCells) {
    system.Init(nullptr);
    system.Shutdown();
    EXPECT_EQ(system.LoadedCellCount(), 0u);
}

// ─── 配置组件默认值 ──────────────────────────────────────────────────────

TEST(WorldPartitionConfigTest, DefaultValues) {
    WorldPartitionConfigComponent config;
    EXPECT_TRUE(config.enabled);
    EXPECT_FLOAT_EQ(config.cell_size, 128.0f);
    EXPECT_EQ(config.grid_min_x, -16);
    EXPECT_EQ(config.grid_max_x, 16);
    EXPECT_EQ(config.max_loads_per_frame, 2u);
}

TEST(StreamingOriginTest, DefaultValues) {
    StreamingOriginComponent so;
    EXPECT_TRUE(so.enabled);
    EXPECT_FLOAT_EQ(so.load_radius, 256.0f);
    EXPECT_FLOAT_EQ(so.unload_radius, 320.0f);
    EXPECT_GT(so.unload_radius, so.load_radius);
}

// ─── 边界条件 ──────────────────────────────────────────────────────────────

TEST_F(WorldPartitionTest, WorldToCell_ExactBoundary) {
    // 恰好在 cell 边界上：128.0 / 128.0 = 1.0 → floor = 1
    auto coord = system.WorldToCell(glm::vec3(128.0f, 0.0f, 128.0f), 128.0f);
    EXPECT_EQ(coord.x, 1);
    EXPECT_EQ(coord.y, 1);
}

TEST_F(WorldPartitionTest, WorldToCell_SmallCellSize) {
    auto coord = system.WorldToCell(glm::vec3(10.0f, 0.0f, 10.0f), 1.0f);
    EXPECT_EQ(coord.x, 10);
    EXPECT_EQ(coord.y, 10);
}
