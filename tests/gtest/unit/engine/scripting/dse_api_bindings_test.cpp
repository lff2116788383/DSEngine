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
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/transform.h"
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

// S1.9：Animator3DComponent 纯字段进 defs 后的每字段访问器 round-trip。
// danim_path/dskel_path 为纯字符串 setter（无清缓存副作用，动画系统按路径值比较重载）。
TEST_F(DseApiBindingsTest, Animator3D_FieldRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::Animator3DComponent>(e);
    const uint32_t id = EntityId(e);

    dse_animator3d_set_enabled(id, 0);
    dse_animator3d_set_danim_path(id, "anims/run.danim");
    dse_animator3d_set_dskel_path(id, "skeletons/hero.dskel");
    dse_animator3d_set_speed(id, 2.5f);
    dse_animator3d_set_loop(id, 0);
    dse_animator3d_set_use_anim_tree(id, 1);
    dse_animator3d_set_blend_parameter(id, "velocity");
    dse_animator3d_set_blend_parameter_value(id, 0.75f);

    EXPECT_EQ(dse_animator3d_get_enabled(id), 0);
    EXPECT_FLOAT_EQ(dse_animator3d_get_speed(id), 2.5f);
    EXPECT_EQ(dse_animator3d_get_loop(id), 0);
    EXPECT_EQ(dse_animator3d_get_use_anim_tree(id), 1);
    EXPECT_FLOAT_EQ(dse_animator3d_get_blend_parameter_value(id), 0.75f);

    char buf[64] = {0};
    dse_animator3d_get_danim_path(id, buf, sizeof(buf));
    EXPECT_STREQ(buf, "anims/run.danim");
    dse_animator3d_get_dskel_path(id, buf, sizeof(buf));
    EXPECT_STREQ(buf, "skeletons/hero.dskel");
    dse_animator3d_get_blend_parameter(id, buf, sizeof(buf));
    EXPECT_STREQ(buf, "velocity");

    const auto& a = world_.registry().get<dse::Animator3DComponent>(e);
    EXPECT_FALSE(a.enabled);
    EXPECT_EQ(a.danim_path, "anims/run.danim");
    EXPECT_EQ(a.dskel_path, "skeletons/hero.dskel");
    EXPECT_TRUE(a.use_anim_tree);
}

// L5：dse_physics3d_raycast — 无物理服务时走 ECS 碰撞体回退（Box AABB）。
TEST_F(DseApiBindingsTest, Physics3DRaycast_EcsBoxHitAndMiss) {
    Entity e = world_.CreateEntity();
    auto& tf = world_.registry().emplace<TransformComponent>(e);
    tf.position = glm::vec3(5.0f, 0.0f, 0.0f);
    tf.scale = glm::vec3(1.0f);
    auto& box = world_.registry().emplace<dse::BoxCollider3DComponent>(e);
    box.size = glm::vec3(2.0f);   // half-size 1.0 → AABB [4..6] on x
    box.center = glm::vec3(0.0f);

    // 命中：从原点沿 +x 射线
    uint32_t hit_entity = 0;
    float point[3] = {0, 0, 0};
    float normal[3] = {0, 0, 0};
    float distance = -1.0f;
    int hit = dse_physics3d_raycast(0, 0, 0, 1, 0, 0, 100.0f,
                                    &hit_entity, point, normal, &distance);
    EXPECT_EQ(hit, 1);
    EXPECT_EQ(hit_entity, EntityId(e));
    EXPECT_NEAR(point[0], 4.0f, 1e-3f);   // 进入面 x=4
    EXPECT_NEAR(normal[0], -1.0f, 1e-3f); // 朝向 -x
    EXPECT_NEAR(distance, 4.0f, 1e-3f);

    // 未命中：射线背离盒子（-x 方向）
    int miss = dse_physics3d_raycast(0, 0, 0, -1, 0, 0, 100.0f,
                                     nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(miss, 0);

    // 退化输入：零长方向 / max_dist<=0 → 未命中
    EXPECT_EQ(dse_physics3d_raycast(0, 0, 0, 0, 0, 0, 100.0f, nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(dse_physics3d_raycast(0, 0, 0, 1, 0, 0, 0.0f, nullptr, nullptr, nullptr, nullptr), 0);
}

// L5：dse_rigidbody3d_* — 无物理服务时 set/get velocity 走组件缓存，set_gravity 同步组件，
// add_force/add_impulse/add_torque/set_angular_velocity 为 no-op（仅验证安全 + 角速度回退 0）。
TEST_F(DseApiBindingsTest, RigidBody3D_VelocityAndGravityEcsCache) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    auto& rb = world_.registry().emplace<dse::RigidBody3DComponent>(e);
    rb.use_gravity = true;
    const uint32_t id = EntityId(e);

    dse_rigidbody3d_set_velocity(id, 1.0f, 2.0f, 3.0f);
    const auto& cached = world_.registry().get<dse::RigidBody3DComponent>(e);
    EXPECT_FLOAT_EQ(cached.velocity.x, 1.0f);
    EXPECT_FLOAT_EQ(cached.velocity.y, 2.0f);
    EXPECT_FLOAT_EQ(cached.velocity.z, 3.0f);

    float v[3] = {0, 0, 0};
    dse_rigidbody3d_get_velocity(id, v);
    EXPECT_FLOAT_EQ(v[0], 1.0f);
    EXPECT_FLOAT_EQ(v[1], 2.0f);
    EXPECT_FLOAT_EQ(v[2], 3.0f);

    dse_rigidbody3d_set_gravity(id, 0);
    EXPECT_FALSE(world_.registry().get<dse::RigidBody3DComponent>(e).use_gravity);

    // 无服务路径：以下调用应安全（no-op），角速度回退为 0
    dse_rigidbody3d_add_force(id, 1.0f, 0.0f, 0.0f);
    dse_rigidbody3d_add_impulse(id, 0.0f, 1.0f, 0.0f);
    dse_rigidbody3d_add_torque(id, 0.0f, 0.0f, 1.0f);
    dse_rigidbody3d_set_angular_velocity(id, 1.0f, 1.0f, 1.0f);
    float av[3] = {9.0f, 9.0f, 9.0f};
    dse_rigidbody3d_get_angular_velocity(id, av);
    EXPECT_FLOAT_EQ(av[0], 0.0f);
    EXPECT_FLOAT_EQ(av[1], 0.0f);
    EXPECT_FLOAT_EQ(av[2], 0.0f);
}

// L5：dse_character_controller3d_move — 无物理服务时 ECS 回退（空场景：按位移更新，不着地）。
TEST_F(DseApiBindingsTest, CharacterController3DMove_EcsFallbackNoTerrain) {
    Entity e = world_.CreateEntity();
    auto& tf = world_.registry().emplace<TransformComponent>(e);
    tf.position = glm::vec3(0.0f, 5.0f, 0.0f);
    tf.scale = glm::vec3(1.0f);
    world_.registry().emplace<dse::CharacterController3DComponent>(e);
    const uint32_t id = EntityId(e);

    float vel[3] = {0, 0, 0};
    uint32_t flags = 0xFFFFFFFFu;
    int grounded = dse_character_controller3d_move(id, 1.0f, 0.0f, 0.0f, 0.0f, 0.5f, vel, &flags);
    EXPECT_EQ(grounded, 0);
    EXPECT_EQ(flags, 0u);
    EXPECT_NEAR(vel[0], 2.0f, 1e-3f);   // dx/dt = 1/0.5
    EXPECT_NEAR(world_.registry().get<TransformComponent>(e).position.x, 1.0f, 1e-3f);
    EXPECT_NEAR(world_.registry().get<TransformComponent>(e).position.y, 5.0f, 1e-3f);
}

// L5：CCT move — 平坦地形贴地（低于地形高度 → 着地 + Down flag）。
TEST_F(DseApiBindingsTest, CharacterController3DMove_TerrainSnapGrounds) {
    Entity e = world_.CreateEntity();
    auto& tf = world_.registry().emplace<TransformComponent>(e);
    tf.position = glm::vec3(0.0f, 2.0f, 0.0f);
    tf.scale = glm::vec3(1.0f);
    world_.registry().emplace<dse::CharacterController3DComponent>(e);

    Entity terrain = world_.CreateEntity();
    auto& hm = world_.registry().emplace<dse::TerrainHeightmapComponent>(terrain);
    hm.cols = 2; hm.rows = 2; hm.block_size = 1000.0f; hm.scale = 1.0f;
    hm.origin_x = -500.0f; hm.origin_z = 500.0f;
    hm.heights = {1.0f, 1.0f, 1.0f, 1.0f};   // 平坦地形 y=1

    const uint32_t id = EntityId(e);
    float vel[3] = {0, 0, 0};
    uint32_t flags = 0;
    int grounded = dse_character_controller3d_move(id, 0.0f, -5.0f, 0.0f, 0.0f, 1.0f, vel, &flags);
    EXPECT_EQ(grounded, 1);
    EXPECT_TRUE(flags & static_cast<uint32_t>(dse::CharacterCollisionFlag::Down));
    EXPECT_NEAR(world_.registry().get<TransformComponent>(e).position.y, 1.0f, 1e-3f);
}

// L5：dse_render_world_to_screen — 主相机可见性判定（屏幕尺寸无关）。
TEST_F(DseApiBindingsTest, RenderWorldToScreen_VisibilityFrontVsBehind) {
    Entity cam = world_.CreateEntity();
    world_.registry().emplace<dse::Camera3DComponent>(cam);   // enabled, priority 0
    auto& tf = world_.registry().emplace<TransformComponent>(cam);
    tf.position = glm::vec3(0.0f, 0.0f, 0.0f);
    tf.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);          // identity → front -z

    float sx = -1.0f, sy = -1.0f;
    EXPECT_EQ(dse_render_world_to_screen(0.0f, 0.0f, -5.0f, &sx, &sy), 1);   // 前方可见
    EXPECT_EQ(dse_render_world_to_screen(0.0f, 0.0f, 5.0f, nullptr, nullptr), 0);  // 背后不可见

    // 无相机世界 → 不可见且输出清零
    World empty;
    dse_native_api_init(&empty, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    float ox = 9.0f, oy = 9.0f;
    EXPECT_EQ(dse_render_world_to_screen(0.0f, 0.0f, -5.0f, &ox, &oy), 0);
    EXPECT_FLOAT_EQ(ox, 0.0f);
    EXPECT_FLOAT_EQ(oy, 0.0f);
    dse_native_api_init(&world_, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);  // 还原
}

// L5：MeshRenderer 材质/贴图加载在无 AssetManager 时安全返回 0。
TEST_F(DseApiBindingsTest, MeshRenderer_MaterialTextureNoAssetManager) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::MeshRendererComponent>(e);
    const uint32_t id = EntityId(e);
    EXPECT_EQ(dse_mesh_renderer_set_material_from_dmat(id, "x.dmat", 0), 0);
    uint32_t h = 7; int w = 7, ht = 7;
    EXPECT_EQ(dse_mesh_renderer_set_texture(id, "albedo", "x.png", &h, &w, &ht), 0);
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
