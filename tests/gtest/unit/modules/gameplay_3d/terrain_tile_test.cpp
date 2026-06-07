/**
 * @file terrain_tile_test.cpp
 * @brief 地形分块系统纯逻辑单元测试（无 GPU/窗口）
 *
 * 测试策略：
 * - TerrainTileManagerComponent 默认值
 * - TerrainTileData 默认值
 * - TerrainTileKey 唯一性 / 负坐标 / 幂等性
 * - tiles map 基本增删操作
 * - load_radius < unload_radius 防抖约束
 */

#include "gtest/gtest.h"
#include "engine/ecs/components_3d_terrain_tile.h"
#include <set>

using namespace dse;

// ============================================================
// 2.1 TerrainTileManagerComponent 默认值
// ============================================================

TEST(TerrainTileManagerComponentTest, 默认值_瓦片配置) {
    TerrainTileManagerComponent ttm;
    EXPECT_TRUE(ttm.enabled);
    EXPECT_FLOAT_EQ(ttm.tile_world_size, 64.0f);
    EXPECT_EQ(ttm.tile_resolution, 64);
    EXPECT_FLOAT_EQ(ttm.max_height, 20.0f);
    EXPECT_EQ(ttm.max_lod_levels, 4);
    EXPECT_FLOAT_EQ(ttm.lod_distance_factor, 50.0f);
}

TEST(TerrainTileManagerComponentTest, 默认值_加载半径) {
    TerrainTileManagerComponent ttm;
    EXPECT_FLOAT_EQ(ttm.load_radius, 200.0f);
    EXPECT_FLOAT_EQ(ttm.unload_radius, 250.0f);
}

TEST(TerrainTileManagerComponentTest, 默认值_程序化生成) {
    TerrainTileManagerComponent ttm;
    EXPECT_TRUE(ttm.use_procedural);
    EXPECT_FLOAT_EQ(ttm.procedural_base_height, 0.0f);
}

TEST(TerrainTileManagerComponentTest, 默认值_统计计数) {
    TerrainTileManagerComponent ttm;
    EXPECT_EQ(ttm.loaded_tile_count, 0);
    EXPECT_EQ(ttm.visible_tile_count, 0);
}

TEST(TerrainTileManagerComponentTest, 默认值_tiles为空) {
    TerrainTileManagerComponent ttm;
    EXPECT_TRUE(ttm.tiles.empty());
}

// ============================================================
// 2.2 TerrainTileData 默认值
// ============================================================

TEST(TerrainTileDataTest, 默认值) {
    TerrainTileData td;
    EXPECT_TRUE(td.gpu_dirty);
    EXPECT_FALSE(td.loaded);
    EXPECT_EQ(td.current_lod, 0);
    EXPECT_EQ(td.index_count, 0u);
}

TEST(TerrainTileDataTest, 默认值_坐标) {
    TerrainTileData td;
    EXPECT_EQ(td.tile_x, 0);
    EXPECT_EQ(td.tile_z, 0);
}

TEST(TerrainTileDataTest, 默认值_高度数据为空) {
    TerrainTileData td;
    EXPECT_TRUE(td.height_data.empty());
    EXPECT_TRUE(td.splat_data.empty());
}

// ============================================================
// 2.3 TerrainTileKey 正确性
// ============================================================

TEST(TerrainTileTest, TileKey不同坐标不同值) {
    EXPECT_NE(dse::TerrainTileKey(0, 0), dse::TerrainTileKey(1, 0));
    EXPECT_NE(dse::TerrainTileKey(0, 0), dse::TerrainTileKey(0, 1));
    EXPECT_NE(dse::TerrainTileKey(1, 2), dse::TerrainTileKey(2, 1));
}

TEST(TerrainTileTest, TileKey负坐标) {
    int64_t k1 = dse::TerrainTileKey(-1, -1);
    int64_t k2 = dse::TerrainTileKey(1, 1);
    EXPECT_NE(k1, k2);

    EXPECT_NE(dse::TerrainTileKey(-1, 0), dse::TerrainTileKey(1, 0));
    EXPECT_NE(dse::TerrainTileKey(0, -1), dse::TerrainTileKey(0, 1));
}

TEST(TerrainTileTest, TileKey相同坐标相同值) {
    EXPECT_EQ(dse::TerrainTileKey(5, 7), dse::TerrainTileKey(5, 7));
    EXPECT_EQ(dse::TerrainTileKey(0, 0), dse::TerrainTileKey(0, 0));
    EXPECT_EQ(dse::TerrainTileKey(-3, -4), dse::TerrainTileKey(-3, -4));
}

TEST(TerrainTileTest, TileKey大范围无碰撞) {
    std::set<int64_t> keys;
    for (int tx = -10; tx <= 10; ++tx) {
        for (int tz = -10; tz <= 10; ++tz) {
            int64_t k = dse::TerrainTileKey(tx, tz);
            EXPECT_TRUE(keys.insert(k).second)
                << "collision at tx=" << tx << " tz=" << tz;
        }
    }
    EXPECT_EQ(keys.size(), 21u * 21u);
}

// ============================================================
// 2.4 tiles map 增删
// ============================================================

TEST(TerrainTileMapTest, 插入与查找) {
    std::unordered_map<int64_t, TerrainTileData> tiles;

    int64_t key = dse::TerrainTileKey(3, 4);
    tiles[key].tile_x = 3;
    tiles[key].tile_z = 4;
    tiles[key].loaded = true;

    ASSERT_EQ(tiles.count(key), 1u);
    EXPECT_EQ(tiles[key].tile_x, 3);
    EXPECT_EQ(tiles[key].tile_z, 4);
    EXPECT_TRUE(tiles[key].loaded);
}

TEST(TerrainTileMapTest, 删除) {
    std::unordered_map<int64_t, TerrainTileData> tiles;

    int64_t k1 = dse::TerrainTileKey(0, 0);
    int64_t k2 = dse::TerrainTileKey(1, 1);
    tiles[k1].tile_x = 0;
    tiles[k2].tile_x = 1;

    EXPECT_EQ(tiles.size(), 2u);

    tiles.erase(k1);
    EXPECT_EQ(tiles.size(), 1u);
    EXPECT_EQ(tiles.count(k1), 0u);
    EXPECT_EQ(tiles.count(k2), 1u);
}

TEST(TerrainTileMapTest, 覆盖写入) {
    std::unordered_map<int64_t, TerrainTileData> tiles;

    int64_t key = dse::TerrainTileKey(2, 2);
    tiles[key].current_lod = 0;
    tiles[key].current_lod = 3;

    EXPECT_EQ(tiles[key].current_lod, 3);
    EXPECT_EQ(tiles.size(), 1u);
}

// ============================================================
// 2.5 load_radius < unload_radius 防抖
// ============================================================

TEST(TerrainTileHysteresisTest, 默认值满足防抖约束) {
    TerrainTileManagerComponent ttm;
    EXPECT_LT(ttm.load_radius, ttm.unload_radius);
}

TEST(TerrainTileHysteresisTest, 防抖间距合理) {
    TerrainTileManagerComponent ttm;
    float gap = ttm.unload_radius - ttm.load_radius;
    EXPECT_GT(gap, 0.0f) << "unload_radius must be greater than load_radius";
    EXPECT_GE(gap, 10.0f) << "gap should be meaningful to avoid thrashing";
}
