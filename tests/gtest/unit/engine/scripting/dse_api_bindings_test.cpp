/**
 * @file dse_api_bindings_test.cpp
 * @brief dse_api C ABI 绑定回归 — DynamicObstacle / NavMeshAutoRebake / Tree 路径
 */

#include <gtest/gtest.h>
#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_render.h"
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

TEST_F(DseApiBindingsTest, MeshRenderer_ShaderVariantRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::MeshRendererComponent>(e);
    const uint32_t id = EntityId(e);

    char buf[128];
    // 默认 "MESH_UNLIT"
    EXPECT_GT(dse_mesh_renderer_get_shader_variant(id, buf, sizeof(buf)), 0);
    EXPECT_STREQ(buf, "MESH_UNLIT");

    dse_mesh_renderer_set_shader_variant(id, "MESH_PBR");
    EXPECT_GT(dse_mesh_renderer_get_shader_variant(id, buf, sizeof(buf)), 0);
    EXPECT_STREQ(buf, "MESH_PBR");
    EXPECT_EQ(world_.registry().get<dse::MeshRendererComponent>(e).shader_variant, "MESH_PBR");
}

TEST_F(DseApiBindingsTest, MeshRenderer_MeshPathRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::MeshRendererComponent>(e);
    const uint32_t id = EntityId(e);

    dse_mesh_renderer_set_mesh_path(id, "models/rock.dmesh");

    char buf[256];
    EXPECT_GT(dse_mesh_renderer_get_mesh_path(id, buf, sizeof(buf)), 0);
    EXPECT_STREQ(buf, "models/rock.dmesh");
    EXPECT_EQ(world_.registry().get<dse::MeshRendererComponent>(e).mesh_path, "models/rock.dmesh");
}

// set_mesh_path 必须清空过程网格缓存，否则 MeshRenderSystem 见 temp_* 非空会跳过加载新 mesh_path
TEST_F(DseApiBindingsTest, MeshRenderer_SetMeshPathClearsProceduralBuffers) {
    Entity e = world_.CreateEntity();
    auto& mr = world_.registry().emplace<dse::MeshRendererComponent>(e);
    mr.temp_vertices = {1.0f, 2.0f, 3.0f};
    mr.temp_indices = {0u, 1u, 2u};
    mr.temp_uvs = {0.0f, 1.0f};
    mr.temp_normals = {0.0f, 0.0f, 1.0f};
    mr.temp_tangents = {1.0f, 0.0f, 0.0f};
    const uint32_t id = EntityId(e);

    dse_mesh_renderer_set_mesh_path(id, "models/rock.dmesh");

    const auto& after = world_.registry().get<dse::MeshRendererComponent>(e);
    EXPECT_TRUE(after.temp_vertices.empty());
    EXPECT_TRUE(after.temp_indices.empty());
    EXPECT_TRUE(after.temp_uvs.empty());
    EXPECT_TRUE(after.temp_normals.empty());
    EXPECT_TRUE(after.temp_tangents.empty());
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
