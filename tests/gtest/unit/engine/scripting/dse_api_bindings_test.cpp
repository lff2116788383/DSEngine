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

// S1.8-3b：PostProcess 字段进 defs 后，每字段 dse_post_process_* 访问器覆盖 float/int/bool/vec3
TEST_F(DseApiBindingsTest, PostProcess_FieldRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::PostProcessComponent>(e);
    const uint32_t id = EntityId(e);

    dse_post_process_set_bloom_threshold(id, 2.5f);       // float
    dse_post_process_set_ssao_sample_count(id, 48);       // int
    dse_post_process_set_fxaa_enabled(id, 0);             // bool
    dse_post_process_set_fog_color(id, 0.2f, 0.4f, 0.6f); // vec3

    EXPECT_FLOAT_EQ(dse_post_process_get_bloom_threshold(id), 2.5f);
    EXPECT_EQ(dse_post_process_get_ssao_sample_count(id), 48);
    EXPECT_EQ(dse_post_process_get_fxaa_enabled(id), 0);

    float x = 0.0f, y = 0.0f, z = 0.0f;
    dse_post_process_get_fog_color(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 0.2f);
    EXPECT_FLOAT_EQ(y, 0.4f);
    EXPECT_FLOAT_EQ(z, 0.6f);

    const auto& pp = world_.registry().get<dse::PostProcessComponent>(e);
    EXPECT_FLOAT_EQ(pp.bloom_threshold, 2.5f);
    EXPECT_EQ(pp.ssao_sample_count, 48);
    EXPECT_FALSE(pp.fxaa_enabled);
    EXPECT_FLOAT_EQ(pp.fog_color.x, 0.2f);
    EXPECT_FLOAT_EQ(pp.fog_color.z, 0.6f);
}

// 组件缺失时 getter 应返回 defs 中声明的默认值（与 header 一致）
TEST_F(DseApiBindingsTest, PostProcess_MissingComponentReturnsDefault) {
    Entity e = world_.CreateEntity();  // 不添加 PostProcessComponent
    const uint32_t id = EntityId(e);
    EXPECT_FLOAT_EQ(dse_post_process_get_bloom_threshold(id), 1.0f);
    EXPECT_EQ(dse_post_process_get_ssao_sample_count(id), 32);
}

// S1.8 Tier B：新增 dir_light.enabled / point_light.cast_shadow 每字段访问器 round-trip
TEST_F(DseApiBindingsTest, Light_TierBFieldRoundTrip) {
    Entity d = world_.CreateEntity();
    world_.registry().emplace<dse::DirectionalLight3DComponent>(d);
    const uint32_t did = EntityId(d);
    dse_dir_light_set_enabled(did, 0);
    EXPECT_EQ(dse_dir_light_get_enabled(did), 0);
    EXPECT_FALSE(world_.registry().get<dse::DirectionalLight3DComponent>(d).enabled);

    Entity p = world_.CreateEntity();
    world_.registry().emplace<dse::PointLightComponent>(p);
    const uint32_t pid = EntityId(p);
    dse_point_light_set_cast_shadow(pid, 1);
    EXPECT_EQ(dse_point_light_get_cast_shadow(pid), 1);
    EXPECT_TRUE(world_.registry().get<dse::PointLightComponent>(p).cast_shadow);
}

// S1.8 Tier C：手写复合 dse_dir_light_set_shadow_params 的 cascade 级联约束 + clamp
TEST_F(DseApiBindingsTest, DirLight_ShadowParamsClampAndCascade) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DirectionalLight3DComponent>(e);
    const uint32_t id = EntityId(e);

    // strength 1.5→1.0；c0 0.05→0.1；c1 0.05→max(0.1+0.1)=0.2；c2 0.05→0.3；lambda -1→0
    dse_dir_light_set_shadow_params(id, 1, 1.5f, 0.05f, 0.05f, 0.05f, -1.0f);

    const auto& dl = world_.registry().get<dse::DirectionalLight3DComponent>(e);
    EXPECT_TRUE(dl.cast_shadow);
    EXPECT_FLOAT_EQ(dl.shadow_strength, 1.0f);
    EXPECT_FLOAT_EQ(dl.cascade_splits[0], 0.1f);
    EXPECT_FLOAT_EQ(dl.cascade_splits[1], 0.2f);
    EXPECT_FLOAT_EQ(dl.cascade_splits[2], 0.3f);
    EXPECT_FLOAT_EQ(dl.cascade_split_lambda, 0.0f);
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
