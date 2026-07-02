/**
 * @file world_partition_deep_test.cpp
 * @brief P3: WorldPartition 系统深度边界测试
 *
 * 补充原 world_partition_test.cpp 中未覆盖的边界场景：
 * - 极端 cell_size（极大/极小）下的坐标转换
 * - 负坐标大范围遍历 round-trip
 * - CellCoord 哈希分布质量
 * - 配置组件边界值
 * - Init/Shutdown 生命周期
 */

#include <gtest/gtest.h>
#include "engine/scene/world_partition.h"
#include <cmath>
#include <unordered_set>
#include <vector>

using namespace dse;

class WorldPartitionDeepTest : public ::testing::Test {
protected:
    WorldPartitionSystem system;
};

// ─── 极端 cell_size 下的坐标转换 ────────────────────────────────────────────

TEST_F(WorldPartitionDeepTest, WorldToCell_VeryLargeCellSize) {
    const float huge_size = 10000.0f;
    auto coord = system.WorldToCell(glm::vec3(5000.0f, 0.0f, -5000.0f), huge_size);
    // 5000/10000 = 0.5 → 通常 floor = 0
    // -5000/10000 = -0.5 → floor = -1
    EXPECT_EQ(coord.x, 0);
    EXPECT_EQ(coord.y, -1);
}

TEST_F(WorldPartitionDeepTest, WorldToCell_VerySmallCellSize) {
    const float tiny_size = 0.01f;
    auto coord = system.WorldToCell(glm::vec3(1.0f, 0.0f, 1.0f), tiny_size);
    EXPECT_EQ(coord.x, 100);
    EXPECT_EQ(coord.y, 100);
}

TEST_F(WorldPartitionDeepTest, CellToWorld_LargeCellSize_Center) {
    const float size = 1000.0f;
    auto pos = system.CellToWorld(CellCoord{5, -3}, size);
    EXPECT_FLOAT_EQ(pos.x, (5 + 0.5f) * size);
    EXPECT_FLOAT_EQ(pos.z, (-3 + 0.5f) * size);
}

TEST_F(WorldPartitionDeepTest, WorldToCell_AtOrigin) {
    auto coord = system.WorldToCell(glm::vec3(0.0f, 0.0f, 0.0f), 128.0f);
    EXPECT_EQ(coord.x, 0);
    EXPECT_EQ(coord.y, 0);
}

TEST_F(WorldPartitionDeepTest, WorldToCell_ExactBoundary) {
    auto coord = system.WorldToCell(glm::vec3(128.0f, 0.0f, 128.0f), 128.0f);
    EXPECT_EQ(coord.x, 1);
    EXPECT_EQ(coord.y, 1);
}

TEST_F(WorldPartitionDeepTest, WorldToCell_NegativeExactBoundary) {
    auto coord = system.WorldToCell(glm::vec3(-128.0f, 0.0f, -128.0f), 128.0f);
    EXPECT_EQ(coord.x, -1);
    EXPECT_EQ(coord.y, -1);
}

// ─── 负坐标大范围遍历正确性 ─────────────────────────────────────────────────

TEST_F(WorldPartitionDeepTest, WorldToCell_NegativeRange_RoundTrip) {
    const float cell_size = 64.0f;
    for (int cx = -50; cx <= 50; ++cx) {
        for (int cy = -50; cy <= 50; ++cy) {
            CellCoord original{cx, cy};
            glm::vec3 center = system.CellToWorld(original, cell_size);
            auto back = system.WorldToCell(center, cell_size);
            EXPECT_EQ(back.x, original.x) << "cx=" << cx << " cy=" << cy;
            EXPECT_EQ(back.y, original.y) << "cx=" << cx << " cy=" << cy;
        }
    }
}

TEST_F(WorldPartitionDeepTest, WorldToCell_RoundTrip_VariousCellSizes) {
    const float sizes[] = {1.0f, 16.0f, 64.0f, 128.0f, 256.0f, 1024.0f};
    for (float sz : sizes) {
        for (int cx = -5; cx <= 5; ++cx) {
            for (int cy = -5; cy <= 5; ++cy) {
                CellCoord original{cx, cy};
                glm::vec3 center = system.CellToWorld(original, sz);
                auto back = system.WorldToCell(center, sz);
                EXPECT_EQ(back.x, original.x) << "sz=" << sz << " cx=" << cx;
                EXPECT_EQ(back.y, original.y) << "sz=" << sz << " cy=" << cy;
            }
        }
    }
}

// ─── CellCoord 哈希分布 ────────────────────────────────────────────────────

TEST_F(WorldPartitionDeepTest, CellCoordHash_LargeRange_LowCollision) {
    CellCoordHash hasher;
    std::unordered_set<size_t> hashes;
    const int range = 20;
    for (int x = -range; x <= range; ++x) {
        for (int y = -range; y <= range; ++y) {
            hashes.insert(hasher(CellCoord{x, y}));
        }
    }
    const size_t total = static_cast<size_t>((2 * range + 1) * (2 * range + 1));
    EXPECT_GT(hashes.size(), total * 95 / 100);
}

TEST_F(WorldPartitionDeepTest, CellCoordHash_SymmetricPairs) {
    CellCoordHash hasher;
    EXPECT_NE(hasher(CellCoord{1, 2}), hasher(CellCoord{2, 1}));
    EXPECT_NE(hasher(CellCoord{-1, 1}), hasher(CellCoord{1, -1}));
}

// ─── CellCoord 比较运算符 ──────────────────────────────────────────────────

TEST_F(WorldPartitionDeepTest, CellCoordEquality) {
    CellCoord a{3, 5};
    CellCoord b{3, 5};
    CellCoord c{3, 6};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// ─── Init / Shutdown 生命周期 ──────────────────────────────────────────────

TEST_F(WorldPartitionDeepTest, InitShutdown_CleanState) {
    system.Init(nullptr);
    EXPECT_EQ(system.LoadedCellCount(), 0u);

    system.Shutdown();
    EXPECT_EQ(system.LoadedCellCount(), 0u);
}

TEST_F(WorldPartitionDeepTest, MultipleShutdown_NoCrash) {
    system.Init(nullptr);
    system.Shutdown();
    system.Shutdown();
    EXPECT_EQ(system.LoadedCellCount(), 0u);
}

// ─── 配置组件边界值 ────────────────────────────────────────────────────────

TEST(WorldPartitionConfigDeepTest, DefaultValues) {
    WorldPartitionConfigComponent config;
    EXPECT_TRUE(config.enabled);
    EXPECT_FLOAT_EQ(config.cell_size, 128.0f);
    EXPECT_EQ(config.grid_min_x, -16);
    EXPECT_EQ(config.grid_max_x, 16);
    EXPECT_EQ(config.grid_min_y, -16);
    EXPECT_EQ(config.grid_max_y, 16);
    EXPECT_EQ(config.max_loads_per_frame, 2u);
}

TEST(WorldPartitionConfigDeepTest, CustomValues) {
    WorldPartitionConfigComponent config;
    config.cell_size = 256.0f;
    config.grid_min_x = -100;
    config.grid_max_x = 100;
    config.grid_min_y = -50;
    config.grid_max_y = 50;
    config.max_loads_per_frame = 10;

    EXPECT_FLOAT_EQ(config.cell_size, 256.0f);
    EXPECT_EQ(config.grid_min_x, -100);
    EXPECT_EQ(config.grid_max_x, 100);
    EXPECT_EQ(config.max_loads_per_frame, 10u);
}

TEST(WorldPartitionConfigDeepTest, CellFilePattern) {
    WorldPartitionConfigComponent config;
    EXPECT_EQ(config.cell_file_pattern, "cell_{cx}_{cy}.dscene");

    config.cell_file_pattern = "custom_{cx}_{cy}.dat";
    EXPECT_EQ(config.cell_file_pattern, "custom_{cx}_{cy}.dat");
}

TEST(StreamingOriginDeepTest, DefaultValues) {
    StreamingOriginComponent so;
    EXPECT_TRUE(so.enabled);
    EXPECT_FLOAT_EQ(so.load_radius, 256.0f);
    EXPECT_FLOAT_EQ(so.unload_radius, 320.0f);
}

TEST(StreamingOriginDeepTest, UnloadRadiusMustExceedLoadRadius) {
    StreamingOriginComponent so;
    EXPECT_GT(so.unload_radius, so.load_radius);
}

TEST(StreamingOriginDeepTest, CustomRadii) {
    StreamingOriginComponent so;
    so.load_radius = 500.0f;
    so.unload_radius = 600.0f;

    EXPECT_FLOAT_EQ(so.load_radius, 500.0f);
    EXPECT_FLOAT_EQ(so.unload_radius, 600.0f);
    EXPECT_GT(so.unload_radius, so.load_radius);
}

// ─── CellInfo 默认值 ──────────────────────────────────────────────────────

TEST(CellInfoDeepTest, DefaultState) {
    CellInfo info;
    EXPECT_EQ(info.state, CellState::Unloaded);
    EXPECT_FLOAT_EQ(info.distance_sq, 0.0f);
    EXPECT_TRUE(info.scene_path.empty());
}

TEST(CellInfoDeepTest, CellStateEnum) {
    EXPECT_NE(CellState::Unloaded, CellState::Loading);
    EXPECT_NE(CellState::Loading, CellState::Loaded);
    EXPECT_NE(CellState::Loaded, CellState::Unloading);
}
