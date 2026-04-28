/**
 * @file tilemap_component_test.cpp
 * @brief TilemapComponent 瓦片地图组件的单元测试
 *
 * 覆盖场景：
 * - 默认值合理性
 * - tiles 数据赋值与访问
 * - tileset 参数修改
 */

#include <gtest/gtest.h>
#include "engine/ecs/tilemap.h"

TEST(TilemapComponentTest, 默认值) {
    TilemapComponent tm;
    EXPECT_TRUE(tm.tiles.empty());
    EXPECT_EQ(tm.width, 0);
    EXPECT_EQ(tm.height, 0);
    EXPECT_FLOAT_EQ(tm.tile_size, 1.0f);
    EXPECT_EQ(tm.tileset_handle, 0u);
    EXPECT_EQ(tm.tileset_cols, 1);
    EXPECT_EQ(tm.tileset_rows, 1);
    EXPECT_EQ(tm.sorting_layer, 0);
    EXPECT_FALSE(tm.generate_colliders);
    EXPECT_TRUE(tm.dirty);
}

TEST(TilemapComponentTest, tiles数据赋值与访问) {
    TilemapComponent tm;
    tm.width = 3;
    tm.height = 2;
    tm.tiles = {0, 1, 2, 3, 0, 1};
    EXPECT_EQ(tm.tiles.size(), 6u);
    EXPECT_EQ(tm.tiles[1], 1);
    EXPECT_EQ(tm.tiles[4], 0);
}

TEST(TilemapComponentTest, tileset参数修改) {
    TilemapComponent tm;
    tm.tileset_cols = 4;
    tm.tileset_rows = 3;
    tm.tile_size = 0.5f;
    tm.generate_colliders = true;
    tm.collider_tile_min = 2;
    EXPECT_EQ(tm.tileset_cols, 4);
    EXPECT_EQ(tm.tileset_rows, 3);
    EXPECT_FLOAT_EQ(tm.tile_size, 0.5f);
    EXPECT_TRUE(tm.generate_colliders);
    EXPECT_EQ(tm.collider_tile_min, 2);
}
