/**
 * @file geometry_clipmap_test.cpp
 * @brief Geometry Clipmap 单元测试
 */

#include <gtest/gtest.h>
#include "engine/terrain/geometry_clipmap.h"

using namespace dse::terrain;

namespace {

// 简单高度采样：正弦波地形
float SineHeightSampler(float x, float z, void* /*user_data*/) {
    return std::sin(x * 0.1f) * std::cos(z * 0.1f) * 0.5f + 0.5f;
}

float FlatHeightSampler(float /*x*/, float /*z*/, void* /*user_data*/) {
    return 0.5f;
}

} // namespace

TEST(GeometryClipmapTest, Init_CreatesLevels) {
    GeometryClipmapSystem system;
    GeometryClipmapConfig config;
    config.num_levels = 4;
    config.grid_size = 32;
    config.base_cell_size = 1.0f;

    system.Init(config);

    EXPECT_EQ(system.LevelCount(), 4);
    EXPECT_EQ(system.GetLevel(0).grid_size, 32);
    EXPECT_FLOAT_EQ(system.GetLevel(0).cell_size, 1.0f);
    EXPECT_FLOAT_EQ(system.GetLevel(1).cell_size, 2.0f);
    EXPECT_FLOAT_EQ(system.GetLevel(2).cell_size, 4.0f);
    EXPECT_FLOAT_EQ(system.GetLevel(3).cell_size, 8.0f);
}

TEST(GeometryClipmapTest, Init_LevelExtent) {
    GeometryClipmapSystem system;
    GeometryClipmapConfig config;
    config.num_levels = 3;
    config.grid_size = 64;
    config.base_cell_size = 2.0f;

    system.Init(config);

    // extent = cell_size * grid_size / 2
    EXPECT_FLOAT_EQ(system.GetLevel(0).extent, 2.0f * 32.0f);  // 64
    EXPECT_FLOAT_EQ(system.GetLevel(1).extent, 4.0f * 32.0f);  // 128
    EXPECT_FLOAT_EQ(system.GetLevel(2).extent, 8.0f * 32.0f);  // 256
}

TEST(GeometryClipmapTest, Update_PopulatesHeightData) {
    GeometryClipmapSystem system;
    GeometryClipmapConfig config;
    config.num_levels = 2;
    config.grid_size = 8;
    config.base_cell_size = 1.0f;
    config.height_scale = 10.0f;

    system.Init(config);
    system.SetHeightSampler(FlatHeightSampler);
    system.Update(glm::vec3(0.0f, 0.0f, 0.0f));

    // Flat sampler returns 0.5 → height = 0.5 * 10.0 = 5.0
    const auto& level = system.GetLevel(0);
    EXPECT_FLOAT_EQ(level.height_data[0], 5.0f);
}

TEST(GeometryClipmapTest, Update_DirtyAfterMove) {
    GeometryClipmapSystem system;
    GeometryClipmapConfig config;
    config.num_levels = 2;
    config.grid_size = 8;
    config.base_cell_size = 1.0f;

    system.Init(config);
    system.SetHeightSampler(FlatHeightSampler);
    system.Update(glm::vec3(0.0f, 0.0f, 0.0f));

    // Clear dirty
    for (int i = 0; i < system.LevelCount(); ++i) system.ClearDirty(i);
    EXPECT_TRUE(system.GetDirtyLevels().empty());

    // Move viewer enough to trigger update
    system.Update(glm::vec3(5.0f, 0.0f, 0.0f));
    EXPECT_FALSE(system.GetDirtyLevels().empty());
}

TEST(GeometryClipmapTest, SampleHeight_InterpolatesCorrectly) {
    GeometryClipmapSystem system;
    GeometryClipmapConfig config;
    config.num_levels = 1;
    config.grid_size = 16;
    config.base_cell_size = 1.0f;
    config.height_scale = 1.0f;

    system.Init(config);
    system.SetHeightSampler(FlatHeightSampler);
    system.Update(glm::vec3(8.0f, 0.0f, 8.0f)); // center the grid

    float h = system.SampleHeight(8.0f, 8.0f);
    EXPECT_NEAR(h, 0.5f, 0.1f); // flat terrain = 0.5
}

TEST(GeometryClipmapTest, ClearDirty_Works) {
    GeometryClipmapSystem system;
    GeometryClipmapConfig config;
    config.num_levels = 3;
    config.grid_size = 8;

    system.Init(config);
    EXPECT_EQ(system.GetDirtyLevels().size(), 3u); // all dirty after init

    system.ClearDirty(0);
    system.ClearDirty(1);
    system.ClearDirty(2);
    EXPECT_TRUE(system.GetDirtyLevels().empty());
}

TEST(GeometryClipmapTest, Shutdown_ClearsState) {
    GeometryClipmapSystem system;
    GeometryClipmapConfig config;
    config.num_levels = 4;
    config.grid_size = 16;

    system.Init(config);
    EXPECT_EQ(system.LevelCount(), 4);

    system.Shutdown();
    EXPECT_EQ(system.LevelCount(), 0);
}
