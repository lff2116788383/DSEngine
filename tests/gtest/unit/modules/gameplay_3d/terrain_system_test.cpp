/**
 * @file terrain_system_test.cpp
 * @brief TerrainSystem + TerrainComponent 无 GPU 单元测试
 *
 * 测试策略：
 * - TerrainComponent 默认值完整性
 * - TerrainSystem 空 World 不崩溃
 * - LOD 距离因子 / dirty 标记
 * - 高度数据容器
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/rendering/terrain_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/gl_command_buffer.h"

using namespace dse;
using namespace dse::gameplay3d;

// ============================================================
// TerrainComponent 默认值
// ============================================================

TEST(TerrainComponentTest, 默认值) {
    TerrainComponent tc;
    EXPECT_TRUE(tc.enabled);
    EXPECT_TRUE(tc.heightmap_path.empty());
    EXPECT_TRUE(tc.texture_path.empty());
    EXPECT_EQ(tc.texture_handle, 0u);
    EXPECT_FLOAT_EQ(tc.width, 100.0f);
    EXPECT_FLOAT_EQ(tc.depth, 100.0f);
    EXPECT_FLOAT_EQ(tc.max_height, 20.0f);
    EXPECT_EQ(tc.resolution_x, 64);
    EXPECT_EQ(tc.resolution_z, 64);
}

TEST(TerrainComponentTest, LOD参数默认值) {
    TerrainComponent tc;
    EXPECT_TRUE(tc.use_dynamic_lod);
    EXPECT_EQ(tc.max_lod_levels, 4);
    EXPECT_FLOAT_EQ(tc.lod_distance_factor, 50.0f);
    EXPECT_EQ(tc.current_lod, 0);
    EXPECT_TRUE(tc.visible);
}

TEST(TerrainComponentTest, SplatMap默认值) {
    TerrainComponent tc;
    EXPECT_TRUE(tc.splat_data.empty());
    EXPECT_TRUE(tc.splat_dirty);
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(tc.splat_texture_paths[i].empty());
        EXPECT_EQ(tc.splat_texture_handles[i], 0u);
    }
}

TEST(TerrainComponentTest, 内部状态默认值) {
    TerrainComponent tc;
    EXPECT_TRUE(tc.is_dirty);
    EXPECT_TRUE(tc.height_data.empty());
    EXPECT_EQ(tc.vao, 0u);
    EXPECT_EQ(tc.vbo, 0u);
    EXPECT_EQ(tc.ebo, 0u);
    EXPECT_TRUE(tc.lod_ebos.empty());
    EXPECT_TRUE(tc.lod_index_counts.empty());
    EXPECT_EQ(tc.index_count, 0u);
}

TEST(TerrainComponentTest, 高度数据写入) {
    TerrainComponent tc;
    tc.resolution_x = 4;
    tc.resolution_z = 4;
    tc.height_data.resize(16, 0.0f);
    tc.height_data[5] = 10.0f;
    EXPECT_FLOAT_EQ(tc.height_data[5], 10.0f);
    EXPECT_FLOAT_EQ(tc.height_data[0], 0.0f);
}

// ============================================================
// TerrainSystem
// ============================================================

TEST(TerrainSystemTest, 默认构造安全) {
    TerrainSystem sys;
    (void)sys;
}

TEST(TerrainSystemTest, 空World不崩溃) {
    TerrainSystem sys;
    World world;
    OpenGLCommandBuffer cmd;
    sys.Render(world, cmd);
}

TEST(TerrainSystemTest, 禁用地形不渲染) {
    TerrainSystem sys;
    World world;
    auto entity = world.registry().create();
    auto& tc = world.registry().emplace<TerrainComponent>(entity);
    tc.enabled = false;
    world.registry().emplace<TransformComponent>(entity);

    OpenGLCommandBuffer cmd;
    sys.Render(world, cmd);
}
