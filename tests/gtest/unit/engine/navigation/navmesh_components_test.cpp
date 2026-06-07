/**
 * @file navmesh_components_test.cpp
 * @brief NavMesh 组件默认值纯逻辑单元测试（无 GPU/窗口）
 *
 * 测试策略：
 * - DynamicObstacleComponent 各字段默认值
 * - NavMeshAutoRebakeComponent 各字段默认值
 */

#include "gtest/gtest.h"
#include "engine/ecs/components_3d_navmesh.h"

using namespace dse;

// ============================================================
// 3.1 DynamicObstacleComponent 默认值
// ============================================================

TEST(DynamicObstacleComponentTest, 默认值_基本字段) {
    DynamicObstacleComponent doc;
    EXPECT_TRUE(doc.enabled);
    EXPECT_EQ(doc.shape, DynamicObstacleComponent::Shape::Box);
}

TEST(DynamicObstacleComponentTest, 默认值_Box尺寸) {
    DynamicObstacleComponent doc;
    EXPECT_FLOAT_EQ(doc.box_extents.x, 1.0f);
    EXPECT_FLOAT_EQ(doc.box_extents.y, 2.0f);
    EXPECT_FLOAT_EQ(doc.box_extents.z, 1.0f);
}

TEST(DynamicObstacleComponentTest, 默认值_Cylinder参数) {
    DynamicObstacleComponent doc;
    EXPECT_FLOAT_EQ(doc.cylinder_radius, 1.0f);
    EXPECT_FLOAT_EQ(doc.cylinder_height, 2.0f);
}

TEST(DynamicObstacleComponentTest, 默认值_运行时状态) {
    DynamicObstacleComponent doc;
    EXPECT_EQ(doc.obstacle_ref_, 0u);
    EXPECT_TRUE(doc.dirty_);
}

// ============================================================
// 3.2 NavMeshAutoRebakeComponent 默认值
// ============================================================

TEST(NavMeshAutoRebakeComponentTest, 默认值_启用与瓦片) {
    NavMeshAutoRebakeComponent nrc;
    EXPECT_TRUE(nrc.enabled);
    EXPECT_FLOAT_EQ(nrc.tile_size, 48.0f);
}

TEST(NavMeshAutoRebakeComponentTest, 默认值_重建触发) {
    NavMeshAutoRebakeComponent nrc;
    EXPECT_FLOAT_EQ(nrc.rebake_cooldown, 1.0f);
    EXPECT_TRUE(nrc.collect_terrain);
    EXPECT_TRUE(nrc.collect_mesh_renderers);
}

TEST(NavMeshAutoRebakeComponentTest, 默认值_Agent参数) {
    NavMeshAutoRebakeComponent nrc;
    EXPECT_FLOAT_EQ(nrc.agent_height, 2.0f);
    EXPECT_FLOAT_EQ(nrc.agent_radius, 0.6f);
    EXPECT_FLOAT_EQ(nrc.agent_max_climb, 0.9f);
    EXPECT_FLOAT_EQ(nrc.agent_max_slope, 45.0f);
}

TEST(NavMeshAutoRebakeComponentTest, 默认值_网格参数) {
    NavMeshAutoRebakeComponent nrc;
    EXPECT_FLOAT_EQ(nrc.cell_size, 0.3f);
    EXPECT_FLOAT_EQ(nrc.cell_height, 0.2f);
}

TEST(NavMeshAutoRebakeComponentTest, 默认值_运行时状态) {
    NavMeshAutoRebakeComponent nrc;
    EXPECT_FLOAT_EQ(nrc.cooldown_timer_, 0.0f);
    EXPECT_TRUE(nrc.needs_full_rebake_);
    EXPECT_EQ(nrc.baked_tile_count_, 0);
}
