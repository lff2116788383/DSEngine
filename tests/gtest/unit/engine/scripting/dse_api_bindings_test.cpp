/**
 * @file dse_api_bindings_test.cpp
 * @brief dse_api C ABI 绑定回归 — DynamicObstacle / NavMeshAutoRebake / Tree 路径
 */

#include <gtest/gtest.h>
#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_tree.h"
#include "engine/ecs/components_3d_navmesh.h"
#include <cstring>

namespace {

uint32_t EntityId(Entity e) {
    return static_cast<uint32_t>(static_cast<entt::id_type>(e));
}

} // namespace

class DseApiBindingsTest : public ::testing::Test {
protected:
    void SetUp() override {
        dse_native_api_init(
            &world_, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr);
    }

    World world_;
};

TEST_F(DseApiBindingsTest, DynamicObstacle_ShapeRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DynamicObstacleComponent>(e);
    const uint32_t id = EntityId(e);

    dse_dyn_obstacle_set_shape(id, 1);
    EXPECT_EQ(dse_dyn_obstacle_get_shape(id), 1);
    EXPECT_EQ(world_.registry().get<dse::DynamicObstacleComponent>(e).shape,
              dse::DynamicObstacleComponent::Shape::Cylinder);
}

TEST_F(DseApiBindingsTest, DynamicObstacle_SettersMarkDirty) {
    Entity e = world_.CreateEntity();
    auto& obstacle = world_.registry().emplace<dse::DynamicObstacleComponent>(e);
    obstacle.dirty_ = false;
    const uint32_t id = EntityId(e);

    dse_dyn_obstacle_set_box_extents(id, 2.0f, 3.0f, 4.0f);
    EXPECT_TRUE(world_.registry().get<dse::DynamicObstacleComponent>(e).dirty_);

    obstacle.dirty_ = false;
    dse_dyn_obstacle_set_cylinder_radius(id, 1.5f);
    EXPECT_TRUE(world_.registry().get<dse::DynamicObstacleComponent>(e).dirty_);

    obstacle.dirty_ = false;
    dse_dyn_obstacle_set_enabled(id, 0);
    EXPECT_TRUE(world_.registry().get<dse::DynamicObstacleComponent>(e).dirty_);
}

TEST_F(DseApiBindingsTest, NavMeshAutoRebake_FieldRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::NavMeshAutoRebakeComponent>(e);
    const uint32_t id = EntityId(e);

    dse_navmesh_rebake_set_cell_size(id, 0.25f);
    dse_navmesh_rebake_set_agent_max_slope(id, 42.0f);
    dse_navmesh_rebake_set_collect_terrain(id, 0);

    EXPECT_FLOAT_EQ(dse_navmesh_rebake_get_cell_size(id), 0.25f);
    EXPECT_FLOAT_EQ(dse_navmesh_rebake_get_agent_max_slope(id), 42.0f);
    EXPECT_EQ(dse_navmesh_rebake_get_collect_terrain(id), 0);
}

TEST_F(DseApiBindingsTest, Tree_MeshPathRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::TreeComponent>(e);
    const uint32_t id = EntityId(e);

    dse_tree_set_mesh_path(id, "models/tree.dmesh");
    dse_tree_set_lod1_mesh_path(id, "models/tree_lod1.dmesh");

    char buf[256];
    EXPECT_GT(dse_tree_get_mesh_path(id, buf, sizeof(buf)), 0);
    EXPECT_STREQ(buf, "models/tree.dmesh");

    EXPECT_GT(dse_tree_get_lod1_mesh_path(id, buf, sizeof(buf)), 0);
    EXPECT_STREQ(buf, "models/tree_lod1.dmesh");
}

TEST_F(DseApiBindingsTest, InvalidEntity_ReturnsSafeDefaults) {
    const uint32_t invalid = 0xFFFFFFFEu;
    EXPECT_EQ(dse_dyn_obstacle_get_shape(invalid), 0);
    EXPECT_FLOAT_EQ(dse_navmesh_rebake_get_tile_size(invalid), 48.0f);

    char buf[8];
    buf[0] = 'x';
    EXPECT_EQ(dse_tree_get_mesh_path(invalid, buf, sizeof(buf)), 0);
    EXPECT_EQ(buf[0], '\0');
}
