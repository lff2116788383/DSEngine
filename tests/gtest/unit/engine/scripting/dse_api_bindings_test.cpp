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
#include "engine/ecs/components_3d_fracture.h"
#include "engine/ecs/components_3d_cloth.h"
#include "engine/ecs/components_3d_fluid.h"
#include "engine/ecs/components_3d_weather.h"
#include "engine/ecs/components_3d_snow.h"
#include "engine/ecs/components_3d_sky.h"
#include "engine/ecs/components_3d_animation.h"
#include "engine/ecs/animation.h"
#include "engine/ecs/animation_state_machine.h"
#include "engine/ecs/transform.h"
#include <cmath>
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

// Gameplay3D：Fracture C ABI — add/set_params(NaN 保持)/apply_damage/trigger/is_fractured。
TEST_F(DseApiBindingsTest, Fracture_AddDamageTriggerEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_fracture_add(id, /*source=*/1, /*fragments=*/12, /*break_force=*/500.0f, /*health=*/30.0f);
    const auto& fc = world_.registry().get<dse::FractureComponent>(e);
    EXPECT_EQ(fc.source, dse::FractureSource::RuntimeVoronoi);
    EXPECT_EQ(fc.runtime_fragment_count, 12u);
    EXPECT_FLOAT_EQ(fc.break_force, 500.0f);
    EXPECT_FLOAT_EQ(fc.health, 30.0f);
    EXPECT_FLOAT_EQ(fc.max_health, 30.0f);

    // set_params：仅覆盖非 NaN 字段，其余保持
    const float keep = fc.fragment_lifetime;
    dse_fracture_set_params(id, /*explosion=*/77.0f, /*lifetime=*/NAN, /*fade=*/NAN, /*mass=*/2.5f);
    EXPECT_FLOAT_EQ(fc.explosion_force, 77.0f);
    EXPECT_FLOAT_EQ(fc.fragment_lifetime, keep);
    EXPECT_FLOAT_EQ(fc.fragment_mass_scale, 2.5f);

    // apply_damage 未致死 → 不请求碎裂
    dse_fracture_apply_damage(id, 10.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(fc.health, 20.0f);
    EXPECT_FALSE(fc.fracture_requested);
    EXPECT_EQ(dse_fracture_is_fractured(id), 0);

    // 致死 → 请求碎裂 + 记录冲击点
    dse_fracture_apply_damage(id, 50.0f, 1.0f, 2.0f, 3.0f);
    EXPECT_TRUE(fc.fracture_requested);
    EXPECT_FLOAT_EQ(fc.impact_point.x, 1.0f);
    EXPECT_FLOAT_EQ(fc.impact_point.z, 3.0f);
}

// Gameplay3D：Cloth C ABI — add/set_wind(turbulence NaN 保持)/pin_vertices/add_sphere_collider。
TEST_F(DseApiBindingsTest, Cloth_AddWindPinColliderEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_cloth_add(id, /*iters=*/16, /*stiffness=*/0.7f, /*damping=*/0.02f, /*bend=*/0.4f);
    const auto& cloth = world_.registry().get<dse::ClothComponent>(e);
    EXPECT_TRUE(cloth.enabled);
    EXPECT_EQ(cloth.solver_iterations, 16u);
    EXPECT_FLOAT_EQ(cloth.stiffness, 0.7f);

    const float keep_turb = cloth.wind_turbulence;
    dse_cloth_set_wind(id, 1.0f, 0.0f, -2.0f, NAN);   // turbulence 保持
    EXPECT_FLOAT_EQ(cloth.wind.x, 1.0f);
    EXPECT_FLOAT_EQ(cloth.wind.z, -2.0f);
    EXPECT_FLOAT_EQ(cloth.wind_turbulence, keep_turb);
    dse_cloth_set_wind(id, 0.0f, 0.0f, 0.0f, 0.5f);
    EXPECT_FLOAT_EQ(cloth.wind_turbulence, 0.5f);

    const uint32_t verts[3] = {2u, 5u, 9u};
    dse_cloth_pin_vertices(id, verts, 3);
    ASSERT_EQ(cloth.pinned_vertices.size(), 3u);
    EXPECT_EQ(cloth.pinned_vertices[1], 5u);
    dse_cloth_pin_vertices(id, nullptr, 0);           // 清空
    EXPECT_TRUE(cloth.pinned_vertices.empty());

    dse_cloth_add_sphere_collider(id, 42u, 1.25f);
    ASSERT_EQ(cloth.sphere_colliders.size(), 1u);
    EXPECT_EQ(cloth.sphere_colliders[0].entity_id, 42u);
    EXPECT_FLOAT_EQ(cloth.sphere_colliders[0].radius, 1.25f);
}

// Gameplay3D：Fluid C ABI — add/set_physics(NaN 保持)/set_rendering/emit_direction(归一化)/floor/count。
TEST_F(DseApiBindingsTest, Fluid_AddPhysicsRenderEmitEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_fluid_add_emitter(id, /*shape=*/2, /*rate=*/300.0f, /*lifetime=*/4.0f, /*speed=*/5.0f);
    const auto& fluid = world_.registry().get<dse::FluidEmitterComponent>(e);
    EXPECT_EQ(fluid.shape, dse::FluidEmitterShape::Box);
    EXPECT_FLOAT_EQ(fluid.emission_rate, 300.0f);
    EXPECT_FLOAT_EQ(fluid.emit_speed, 5.0f);

    const float keep_st = fluid.surface_tension;
    dse_fluid_set_physics(id, /*visc=*/0.05f, /*st=*/NAN, /*rest=*/900.0f, /*gas=*/NAN);
    EXPECT_FLOAT_EQ(fluid.viscosity, 0.05f);
    EXPECT_FLOAT_EQ(fluid.surface_tension, keep_st);
    EXPECT_FLOAT_EQ(fluid.rest_density, 900.0f);

    dse_fluid_set_rendering(id, 0.1f, 0.2f, 0.3f, 0.6f, NAN, 3.0f, NAN);
    EXPECT_FLOAT_EQ(fluid.color.a, 0.6f);
    EXPECT_FLOAT_EQ(fluid.fresnel_power, 3.0f);

    dse_fluid_set_emit_direction(id, 0.0f, 0.0f, -4.0f, 0.9f);   // 应归一化
    EXPECT_NEAR(fluid.emit_direction.z, -1.0f, 1e-4f);
    EXPECT_FLOAT_EQ(fluid.emit_spread, 0.9f);

    dse_fluid_set_floor(id, -3.0f, NAN);
    EXPECT_FLOAT_EQ(fluid.floor_y, -3.0f);

    EXPECT_EQ(dse_fluid_get_particle_count(id), 0u);   // active_count 初始 0
}

// Gameplay3D：缺组件/无效实体时安全 no-op（不崩溃，getter 返回默认）。
TEST_F(DseApiBindingsTest, Gameplay3D_MissingComponentSafe) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);
    // 无组件：set/get 不应崩溃
    dse_fracture_set_params(id, 1.0f, 1.0f, 1.0f, 1.0f);
    dse_fracture_apply_damage(id, 5.0f, 0.0f, 0.0f, 0.0f);
    dse_cloth_set_wind(id, 1.0f, 1.0f, 1.0f, 1.0f);
    dse_fluid_set_physics(id, 1.0f, 1.0f, 1.0f, 1.0f);
    EXPECT_EQ(dse_fracture_is_fractured(id), 0);
    EXPECT_EQ(dse_fluid_get_particle_count(0xFFFFFFFEu), 0u);
}

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
// Gameplay3D：Ragdoll C ABI — add/activate/deactivate/is_active/collision_layer。
TEST_F(DseApiBindingsTest, Ragdoll_AddActivateLayerEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_ragdoll_add(id, /*total_mass=*/20.0f, /*auto_setup=*/0, /*stiffness=*/5.0f, /*damping=*/30.0f);
    auto& rd = world_.registry().get<dse::RagdollComponent>(e);
    EXPECT_FLOAT_EQ(rd.total_mass, 20.0f);
    EXPECT_FALSE(rd.auto_setup);
    EXPECT_FLOAT_EQ(rd.joint_stiffness, 5.0f);
    EXPECT_EQ(dse_ragdoll_is_active(id), 0);

    dse_ragdoll_activate(id);
    EXPECT_TRUE(rd.active);
    EXPECT_EQ(dse_ragdoll_is_active(id), 1);
    dse_ragdoll_deactivate(id);
    EXPECT_FALSE(rd.active);

    dse_ragdoll_set_collision_layer(id, 0x0004u, 0x00FFu);
    EXPECT_EQ(rd.collision_layer, 0x0004u);
    EXPECT_EQ(rd.collision_mask, 0x00FFu);
}
#endif // DSE_ENABLE_PHYSX || DSE_ENABLE_JOLT

// Gameplay3D：SoftBody C ABI — add/set_gravity(NaN 保持)/pin_vertex/get_particle_count。
TEST_F(DseApiBindingsTest, SoftBody_AddGravityPinCountEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_softbody_add(id, /*stiffness=*/0.6f, /*iters=*/6, /*damping=*/0.95f, /*vol=*/0.7f);
    auto& sb = world_.registry().get<dse::SoftBodyComponent>(e);
    EXPECT_FLOAT_EQ(sb.stiffness, 0.6f);
    EXPECT_EQ(sb.solver_iterations, 6);
    EXPECT_FLOAT_EQ(sb.volume_stiffness, 0.7f);

    const float keep = sb.gravity_scale;
    dse_softbody_set_gravity(id, /*use=*/0, /*scale=*/NAN);   // scale 保持
    EXPECT_FALSE(sb.use_gravity);
    EXPECT_FLOAT_EQ(sb.gravity_scale, keep);
    dse_softbody_set_gravity(id, /*use=*/1, /*scale=*/2.0f);
    EXPECT_TRUE(sb.use_gravity);
    EXPECT_FLOAT_EQ(sb.gravity_scale, 2.0f);

    // 粒子数据由 solver 初始化，这里手动填充以验证 pin / count
    sb.inv_masses = {1.0f, 1.0f, 1.0f};
    sb.positions.resize(3);
    dse_softbody_pin_vertex(id, 1);
    EXPECT_FLOAT_EQ(sb.inv_masses[1], 0.0f);
    dse_softbody_pin_vertex(id, 99);   // 越界 → no-op
    EXPECT_EQ(dse_softbody_get_particle_count(id), 3u);
}

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
// Gameplay3D：Vehicle C ABI — add/add_wheel/set_input(clamp)/get_speed/get_wheel_count。
TEST_F(DseApiBindingsTest, Vehicle_AddWheelInputSpeedEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_vehicle_add(id, /*engine=*/6000.0f, /*brake=*/4000.0f, /*steer=*/40.0f);
    auto& v = world_.registry().get<dse::VehicleComponent>(e);
    EXPECT_FLOAT_EQ(v.max_engine_force, 6000.0f);
    EXPECT_FLOAT_EQ(v.max_steer_angle, 40.0f);

    dse_vehicle_add_wheel(id, 1.0f, 0.0f, 1.0f, /*radius=*/0.4f,
                          /*drive=*/1, /*steer=*/0, /*stiff=*/25000.0f, /*damp=*/4200.0f);
    ASSERT_EQ(dse_vehicle_get_wheel_count(id), 1u);
    EXPECT_FLOAT_EQ(v.wheels[0].radius, 0.4f);
    EXPECT_TRUE(v.wheels[0].is_drive_wheel);
    EXPECT_FALSE(v.wheels[0].is_steer_wheel);
    EXPECT_FALSE(v.initialized);

    dse_vehicle_set_input(id, /*throttle=*/2.0f, /*brake=*/-1.0f, /*steering=*/5.0f);  // 应 clamp
    EXPECT_FLOAT_EQ(v.throttle, 1.0f);
    EXPECT_FLOAT_EQ(v.brake, 0.0f);
    EXPECT_FLOAT_EQ(v.steering, 1.0f);

    v.current_speed = 12.5f;
    EXPECT_FLOAT_EQ(dse_vehicle_get_speed(id), 12.5f);
}
#endif // DSE_ENABLE_PHYSX || DSE_ENABLE_JOLT

// Gameplay3D：Rope C ABI — add/set_anchors/set_gravity(NaN 保持)/get_positions(两段式)。
TEST_F(DseApiBindingsTest, Rope_AddAnchorsGravityPositionsEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_rope_add(id, /*segments=*/16, /*seg_len=*/0.3f, /*damping=*/0.98f, /*iters=*/12);
    auto& rope = world_.registry().get<dse::RopeComponent>(e);
    EXPECT_EQ(rope.segment_count, 16);
    EXPECT_FLOAT_EQ(rope.segment_length, 0.3f);
    EXPECT_EQ(rope.solver_iterations, 12);

    dse_rope_set_anchors(id, /*a=*/7u, /*b=*/8u, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f);
    EXPECT_EQ(rope.anchor_entity_a, 7u);
    EXPECT_EQ(rope.anchor_entity_b, 8u);
    EXPECT_FLOAT_EQ(rope.anchor_offset_a.y, 0.2f);
    EXPECT_FLOAT_EQ(rope.anchor_offset_b.z, 0.6f);
    EXPECT_FALSE(rope.initialized);

    const float keep = rope.gravity_scale;
    dse_rope_set_gravity(id, /*use=*/0, /*scale=*/NAN);
    EXPECT_FALSE(rope.use_gravity);
    EXPECT_FLOAT_EQ(rope.gravity_scale, keep);
    dse_rope_set_gravity(id, /*use=*/1, /*scale=*/1.5f);
    EXPECT_FLOAT_EQ(rope.gravity_scale, 1.5f);

    // 位置数据由 solver 初始化，这里手动填充以验证两段式读取
    rope.positions = {glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(4.0f, 5.0f, 6.0f)};
    EXPECT_EQ(dse_rope_get_positions(id, nullptr, 0), 2);   // 仅查询数量
    float buf[6] = {0};
    EXPECT_EQ(dse_rope_get_positions(id, buf, 2), 2);
    EXPECT_FLOAT_EQ(buf[0], 1.0f);
    EXPECT_FLOAT_EQ(buf[5], 6.0f);
}

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
// Gameplay3D：Buoyancy C ABI — add/add_sample_point/set_water_level/get_submerge_ratio/set_use_fluid。
TEST_F(DseApiBindingsTest, Buoyancy_AddSampleWaterRatioEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_buoyancy_add(id, /*level=*/2.0f, /*force=*/15.0f, /*drag=*/4.0f, /*adrag=*/2.0f, /*depth=*/1.5f);
    auto& b = world_.registry().get<dse::BuoyancyComponent>(e);
    EXPECT_FLOAT_EQ(b.water_level, 2.0f);
    EXPECT_FLOAT_EQ(b.buoyancy_force, 15.0f);
    EXPECT_FLOAT_EQ(b.water_angular_drag, 2.0f);

    dse_buoyancy_add_sample_point(id, 0.5f, -0.5f, 0.25f, 0.8f);
    ASSERT_EQ(b.sample_points.size(), 1u);
    EXPECT_FLOAT_EQ(b.sample_points[0].offset.x, 0.5f);
    EXPECT_FLOAT_EQ(b.sample_points[0].force_scale, 0.8f);

    dse_buoyancy_set_water_level(id, 9.0f);
    EXPECT_FLOAT_EQ(b.water_level, 9.0f);

    b.submerge_ratio = 0.42f;
    EXPECT_FLOAT_EQ(dse_buoyancy_get_submerge_ratio(id), 0.42f);

    dse_buoyancy_set_use_fluid(id, 0);
    EXPECT_FALSE(b.use_fluid_system);
}
#endif // DSE_ENABLE_PHYSX || DSE_ENABLE_JOLT

// Batch 3 — Weather C ABI：add(type int)/set(type<0 + NaN 保持)/set_spawn(max<0 保持)。
TEST_F(DseApiBindingsTest, Weather_AddSetSpawnEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_weather_add(id, /*type=*/1 /*Rain*/, /*intensity=*/0.7f);
    auto& wc = world_.registry().get<dse::WeatherComponent>(e);
    EXPECT_EQ(wc.type, dse::WeatherType::Rain);
    EXPECT_FLOAT_EQ(wc.intensity, 0.7f);

    // type<0 + 部分 NaN → 仅写非保持字段
    dse_weather_set(id, /*type=*/-1, /*intensity=*/NAN, /*wind_x=*/3.0f, /*wind_z=*/NAN);
    EXPECT_EQ(wc.type, dse::WeatherType::Rain);      // 保持
    EXPECT_FLOAT_EQ(wc.intensity, 0.7f);             // 保持
    EXPECT_FLOAT_EQ(wc.wind_x, 3.0f);
    dse_weather_set(id, /*type=*/2 /*Snow*/, /*intensity=*/0.2f, NAN, NAN);
    EXPECT_EQ(wc.type, dse::WeatherType::Snow);
    EXPECT_FLOAT_EQ(wc.intensity, 0.2f);

    const float keep_h = wc.spawn_height;
    const int   keep_n = wc.max_particles;
    dse_weather_set_spawn(id, /*radius=*/40.0f, /*height=*/NAN, /*max=*/-1);
    EXPECT_FLOAT_EQ(wc.spawn_radius, 40.0f);
    EXPECT_FLOAT_EQ(wc.spawn_height, keep_h);        // NaN 保持
    EXPECT_EQ(wc.max_particles, keep_n);             // <0 保持
    dse_weather_set_spawn(id, NAN, NAN, /*max=*/500);
    EXPECT_EQ(wc.max_particles, 500);
}

// Batch 3 — SnowCover C ABI：add/set/appearance(NaN 保持)/get/enabled/texture/displacement/remove。
TEST_F(DseApiBindingsTest, SnowCover_FullLifecycleEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_snow_cover_add(id);
    auto& sc = world_.registry().get<dse::SnowCoverComponent>(e);

    dse_snow_cover_set(id, /*target=*/0.8f, /*accum=*/NAN, /*melt=*/0.05f);
    EXPECT_FLOAT_EQ(sc.target_coverage, 0.8f);
    EXPECT_FLOAT_EQ(sc.melt_rate, 0.05f);

    const float keep_metal = sc.snow_metallic;
    dse_snow_set_appearance(id, 0.5f, NAN, NAN, NAN, NAN, NAN, 4.0f);
    EXPECT_FLOAT_EQ(sc.snow_albedo.r, 0.5f);
    EXPECT_FLOAT_EQ(sc.snow_metallic, keep_metal);   // NaN 保持
    EXPECT_FLOAT_EQ(sc.edge_sharpness, 4.0f);

    sc.coverage = 0.3f;
    float cov = -1.0f, tgt = -1.0f;
    int en = -1;
    EXPECT_EQ(dse_snow_cover_get(id, &cov, &tgt, &en), 1);
    EXPECT_FLOAT_EQ(cov, 0.3f);
    EXPECT_FLOAT_EQ(tgt, 0.8f);
    EXPECT_EQ(en, 1);

    dse_snow_cover_set_enabled(id, 0);
    EXPECT_FALSE(sc.enabled);

    dse_snow_set_texture(id, "tex/snow.png", /*tiling=*/16.0f);
    EXPECT_EQ(sc.snow_texture_path, "tex/snow.png");
    EXPECT_EQ(sc.snow_texture_handle, 0u);
    EXPECT_FLOAT_EQ(sc.snow_tiling, 16.0f);
    dse_snow_set_texture(id, nullptr, NAN);          // path=null + NaN → 全保持
    EXPECT_EQ(sc.snow_texture_path, "tex/snow.png");
    EXPECT_FLOAT_EQ(sc.snow_tiling, 16.0f);

    dse_snow_set_displacement(id, /*disp=*/0.1f, /*deform=*/NAN);
    EXPECT_FLOAT_EQ(sc.displacement_height, 0.1f);

    // get 缺失 → 返回 0
    dse_snow_cover_remove(id);
    EXPECT_FALSE(world_.registry().all_of<dse::SnowCoverComponent>(e));
    EXPECT_EQ(dse_snow_cover_get(id, &cov, &tgt, &en), 0);
    EXPECT_FLOAT_EQ(cov, 0.0f);
    EXPECT_EQ(en, 0);
}

// Batch 3 — Atmosphere C ABI：add/params/rayleigh/mie/sun_intensity（NaN 保持）。
TEST_F(DseApiBindingsTest, Atmosphere_ParamsEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_atmosphere_add(id);
    auto& atm = world_.registry().get<dse::AtmosphereComponent>(e);

    const float keep_h = atm.atmosphere_height;
    dse_atmosphere_set_params(id, /*planet=*/1000.0f, /*height=*/NAN, /*sun_disk=*/0.6f);
    EXPECT_FLOAT_EQ(atm.planet_radius, 1000.0f);
    EXPECT_FLOAT_EQ(atm.atmosphere_height, keep_h);
    EXPECT_FLOAT_EQ(atm.sun_disk_angle, 0.6f);

    const float keep_gy = atm.rayleigh_coeff.y;
    dse_atmosphere_set_rayleigh(id, 1.0e-5f, NAN, 3.0e-5f, 9000.0f);
    EXPECT_FLOAT_EQ(atm.rayleigh_coeff.x, 1.0e-5f);
    EXPECT_FLOAT_EQ(atm.rayleigh_coeff.y, keep_gy);
    EXPECT_FLOAT_EQ(atm.rayleigh_scale_height, 9000.0f);

    dse_atmosphere_set_mie(id, NAN, NAN, /*g=*/0.8f);
    EXPECT_FLOAT_EQ(atm.mie_g, 0.8f);

    dse_atmosphere_set_sun_intensity(id, 10.0f, NAN, 30.0f);
    EXPECT_FLOAT_EQ(atm.sun_intensity.x, 10.0f);
    EXPECT_FLOAT_EQ(atm.sun_intensity.z, 30.0f);
}

// Batch 3 — DayNightCycle C ABI：add/set_time/get_time/speed/auto/location/sun getters。
TEST_F(DseApiBindingsTest, DayNightCycle_FieldsEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_day_night_add(id, /*time=*/8.0f, /*auto=*/1, /*speed=*/60.0f);
    auto& dnc = world_.registry().get<dse::DayNightCycleComponent>(e);
    EXPECT_FLOAT_EQ(dnc.time_of_day, 8.0f);
    EXPECT_TRUE(dnc.auto_advance);
    EXPECT_FLOAT_EQ(dnc.time_speed, 60.0f);

    dse_day_night_set_time(id, 15.5f);
    EXPECT_FLOAT_EQ(dse_day_night_get_time(id), 15.5f);
    dse_day_night_set_speed(id, 120.0f);
    EXPECT_FLOAT_EQ(dnc.time_speed, 120.0f);
    dse_day_night_set_auto_advance(id, 0);
    EXPECT_FALSE(dnc.auto_advance);

    const int keep_doy = dnc.day_of_year;
    dse_day_night_set_location(id, /*lat=*/45.0f, /*lon=*/NAN, /*doy=*/-1);
    EXPECT_FLOAT_EQ(dnc.latitude, 45.0f);
    EXPECT_EQ(dnc.day_of_year, keep_doy);            // <=0 保持
    dse_day_night_set_location(id, NAN, NAN, /*doy=*/200);
    EXPECT_EQ(dnc.day_of_year, 200);

    dnc.sun_elevation_ = 33.0f;
    dnc.sun_direction_ = glm::vec3(0.0f, 1.0f, 0.0f);
    EXPECT_FLOAT_EQ(dse_day_night_get_sun_elevation(id), 33.0f);
    float dir[3] = {0};
    dse_day_night_get_sun_direction(id, dir);
    EXPECT_FLOAT_EQ(dir[1], 1.0f);

    // 缺失实体 → 默认 (0,-1,0)
    float dir2[3] = {9, 9, 9};
    dse_day_night_get_sun_direction(0xFFFFFFFEu, dir2);
    EXPECT_FLOAT_EQ(dir2[1], -1.0f);
}

// Batch 3 — VolumetricCloud C ABI：add/set_layer/set_wind（NaN 保持）。
TEST_F(DseApiBindingsTest, VolumetricCloud_LayerWindEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_volumetric_cloud_add(id);
    auto& vc = world_.registry().get<dse::VolumetricCloudComponent>(e);

    const float keep_top = vc.cloud_top;
    dse_cloud_set_layer(id, /*bottom=*/2000.0f, /*top=*/NAN, /*coverage=*/0.7f, /*density=*/NAN);
    EXPECT_FLOAT_EQ(vc.cloud_bottom, 2000.0f);
    EXPECT_FLOAT_EQ(vc.cloud_top, keep_top);
    EXPECT_FLOAT_EQ(vc.coverage, 0.7f);

    const float keep_dy = vc.wind_direction.y;
    dse_cloud_set_wind(id, /*dir_x=*/0.5f, /*dir_y=*/NAN, /*speed=*/35.0f);
    EXPECT_FLOAT_EQ(vc.wind_direction.x, 0.5f);
    EXPECT_FLOAT_EQ(vc.wind_direction.y, keep_dy);
    EXPECT_FLOAT_EQ(vc.wind_speed, 35.0f);
}

// ---- 动画子系统 L4/L5 C ABI ----

TEST_F(DseApiBindingsTest, Anim2D_StatePlayEventEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_anim2d_add(id);
    const uint32_t frames[3] = {10u, 11u, 12u};
    dse_anim2d_add_state(id, "walk", 12.0f, /*loop=*/1, frames, 3);
    dse_anim2d_add_event(id, "walk", 0.5f, "footstep");
    dse_anim2d_play(id, "walk");

    auto& anim = world_.registry().get<AnimatorComponent>(e);
    ASSERT_TRUE(anim.states.count("walk"));
    const auto& st = anim.states.at("walk");
    EXPECT_FLOAT_EQ(st.frame_rate, 12.0f);
    EXPECT_TRUE(st.loop);
    ASSERT_EQ(st.frame_handles.size(), 3u);
    EXPECT_EQ(st.frame_handles[2], 12u);
    ASSERT_EQ(st.events.size(), 1u);
    EXPECT_EQ(st.events[0].second, "footstep");
    EXPECT_EQ(anim.current_state, "walk");
    EXPECT_TRUE(anim.playing);

    // pop_event: 无触发事件时返回空串。
    char buf[64] = {'x', 0};
    EXPECT_EQ(dse_anim2d_pop_event(id, buf, sizeof(buf)), 0);
    EXPECT_STREQ(buf, "");
    anim.fired_events.push_back("footstep");
    EXPECT_EQ(dse_anim2d_pop_event(id, buf, sizeof(buf)), 1);
    EXPECT_STREQ(buf, "footstep");
}

TEST_F(DseApiBindingsTest, Anim3D_FsmTransitionParamsEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_anim3d_add(id, "anims/idle.danim", "skel/hero.dskel");
    dse_anim3d_init_fsm(id);
    dse_anim3d_add_fsm_state(id, "idle", "anims/idle.danim", /*loop=*/1, 1.0f);
    dse_anim3d_add_fsm_state(id, "run", "anims/run.danim", /*loop=*/1, 1.5f);

    const char* names[1] = {"speed"};
    const int modes[1] = {static_cast<int>(dse::gameplay3d::AnimConditionMode::Greater)};
    const float thr[1] = {0.5f};
    const int ivals[1] = {0};
    dse_anim3d_add_transition(id, "idle", "run", /*dur=*/0.2f,
                              /*has_exit=*/0, /*exit=*/1.0f,
                              1, names, modes, thr, ivals);

    dse_anim3d_set_param_float(id, "speed", 0.8f);
    dse_anim3d_set_param_trigger(id, "jump");
    dse_anim3d_set_state(id, "run", /*speed=*/2.0f, /*loop=*/0);
    dse_anim3d_set_lock_root_motion(id, 1);

    auto& a = world_.registry().get<dse::Animator3DComponent>(e);
    ASSERT_TRUE(static_cast<bool>(a.state_machine));
    auto& sm = *a.state_machine;
    EXPECT_EQ(sm.GetStatesMutable().size(), 2u);
    auto& states = sm.GetStatesMutable();
    ASSERT_TRUE(states.count("idle"));
    ASSERT_EQ(states.at("idle").transitions.size(), 1u);
    EXPECT_EQ(states.at("idle").transitions[0].target_state, "run");
    EXPECT_FLOAT_EQ(sm.GetFloat("speed"), 0.8f);
    EXPECT_TRUE(sm.GetParameters().count("jump"));
    EXPECT_EQ(a.current_state_name, "run");
    EXPECT_FLOAT_EQ(a.speed, 2.0f);
    EXPECT_FALSE(a.loop);
    EXPECT_TRUE(a.lock_root_motion);

    // get_state 回读。
    char state[64] = {0};
    float norm = 0, time = 0, speed = 0;
    int loop = 1, trans = 1, bones = -1, has_skel = 0;
    EXPECT_EQ(dse_anim3d_get_state(id, state, sizeof(state), &norm, &time, &speed,
                                   &loop, &trans, &bones, &has_skel), 1);
    EXPECT_STREQ(state, "run");
    EXPECT_FLOAT_EQ(speed, 2.0f);
    EXPECT_EQ(loop, 0);
    EXPECT_EQ(has_skel, 1);
}

TEST_F(DseApiBindingsTest, Anim3D_EventRootMotionEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_anim3d_add(id, "a.danim", "");
    dse_anim3d_add_event(id, "hit", 0.3f);
    dse_anim3d_set_extract_root_motion(id, 1);

    auto& a = world_.registry().get<dse::Animator3DComponent>(e);
    ASSERT_EQ(a.events.size(), 1u);
    EXPECT_EQ(a.events[0].name, "hit");
    EXPECT_TRUE(a.extract_root_motion);

    a.root_motion_delta = glm::vec3(1.0f, 2.0f, 3.0f);
    float xyz[3] = {0, 0, 0};
    EXPECT_EQ(dse_anim3d_get_root_motion_delta(id, xyz), 1);
    EXPECT_FLOAT_EQ(xyz[1], 2.0f);

    a.fired_events.push_back("hit");
    char buf[32] = {0};
    EXPECT_EQ(dse_anim3d_pop_event(id, buf, sizeof(buf)), 1);
    EXPECT_STREQ(buf, "hit");
}

TEST_F(DseApiBindingsTest, AnimLayer_ClipBlendTreeMaskEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_animlayer_add_component(id);
    int idx = dse_animlayer_add(id, "upper", 0.5f, 0);
    ASSERT_EQ(idx, 0);
    dse_animlayer_set_clip(id, idx, "anims/aim.danim", 1.2f, /*loop=*/1);
    dse_animlayer_set_weight(id, idx, 0.8f);

    const char* bones[2] = {"spine", "head"};
    dse_animlayer_set_bone_mask(id, idx, bones, 2);

    const char* paths[2] = {"a.danim", "b.danim"};
    const float thr[2] = {0.0f, 1.0f};
    const float spd[2] = {1.0f, 1.0f};
    dse_animlayer_set_blend_tree_1d(id, idx, paths, thr, spd, 2);
    dse_animlayer_set_blend_param(id, idx, 0.4f);
    dse_animlayer_set_enabled(id, 0);

    auto& comp = world_.registry().get<dse::AnimLayerComponent>(e);
    ASSERT_EQ(comp.layers.size(), 1u);
    const auto& layer = comp.layers[0];
    EXPECT_EQ(layer.name, "upper");
    EXPECT_FLOAT_EQ(layer.weight, 0.8f);
    EXPECT_EQ(layer.source_type, dse::AnimSourceType::BlendTree1D);
    ASSERT_EQ(layer.bone_mask_include.size(), 2u);
    EXPECT_EQ(layer.bone_mask_include[1], "head");
    ASSERT_EQ(layer.blend_nodes.size(), 2u);
    EXPECT_EQ(layer.blend_nodes[1].danim_path, "b.danim");
    EXPECT_FLOAT_EQ(layer.blend_parameter_value, 0.4f);
    EXPECT_FALSE(comp.enabled);
}

TEST_F(DseApiBindingsTest, IK_ChainTargetEcs) {
    Entity e = world_.CreateEntity();
    Entity tgt = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_ik_add_component(id);
    int idx = dse_ik_add_chain(id, "leg_l", 1, "hip", "foot", 0.9f);
    ASSERT_EQ(idx, 0);
    dse_ik_set_target(id, idx, 1.0f, 2.0f, 3.0f);
    dse_ik_set_target_entity(id, idx, EntityId(tgt));
    dse_ik_set_weight(id, idx, 0.7f);
    dse_ik_set_pole_vector(id, idx, 0.0f, 0.0f, 1.0f);
    dse_ik_set_iterations(id, idx, 12);
    dse_ik_set_enabled(id, 0);

    auto& comp = world_.registry().get<dse::IKChain3DComponent>(e);
    ASSERT_EQ(comp.chains.size(), 1u);
    const auto& c = comp.chains[0];
    EXPECT_EQ(c.name, "leg_l");
    EXPECT_EQ(c.root_bone, "hip");
    EXPECT_EQ(c.tip_bone, "foot");
    EXPECT_FLOAT_EQ(c.target_position.x, 1.0f);
    EXPECT_FLOAT_EQ(c.target_position.z, 3.0f);
    EXPECT_EQ(c.target_entity, EntityId(tgt));
    EXPECT_FLOAT_EQ(c.weight, 0.7f);
    EXPECT_FLOAT_EQ(c.pole_vector.z, 1.0f);
    EXPECT_EQ(c.iterations, 12);
    EXPECT_FALSE(comp.enabled);

    // 清除目标实体。
    dse_ik_set_target_entity(id, idx, UINT32_MAX);
    EXPECT_EQ(comp.chains[0].target_entity, UINT32_MAX);
}

TEST_F(DseApiBindingsTest, BoneAttach_AddOffsetRemoveEcs) {
    Entity e = world_.CreateEntity();
    Entity tgt = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_bone_attach_add(id, EntityId(tgt), "hand_r");
    dse_bone_attach_set_offset(id, 1.0f, 2.0f, 3.0f,
                               0.0f, 0.0f, 0.0f, 1.0f,
                               2.0f, 2.0f, 2.0f);

    auto& comp = world_.registry().get<dse::BoneAttachmentComponent>(e);
    EXPECT_EQ(comp.target_entity, tgt);
    EXPECT_EQ(comp.bone_name, "hand_r");
    EXPECT_FLOAT_EQ(comp.offset_position.y, 2.0f);
    EXPECT_FLOAT_EQ(comp.offset_scale.x, 2.0f);

    dse_bone_attach_set_bone(id, "hand_l");
    EXPECT_EQ(world_.registry().get<dse::BoneAttachmentComponent>(e).bone_name, "hand_l");

    // get_world_pos: 无 skel 缓存时安全返回 0。
    float xyz[3] = {9, 9, 9};
    EXPECT_EQ(dse_bone_attach_get_world_pos(EntityId(tgt), "hand_l", xyz), 0);
    EXPECT_FLOAT_EQ(xyz[0], 0.0f);

    dse_bone_attach_remove(id);
    EXPECT_FALSE(world_.registry().all_of<dse::BoneAttachmentComponent>(e));
}

TEST_F(DseApiBindingsTest, Morph_AddTargetWeightEcs) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_morph_add_component(id);
    // 2 个顶点，每顶点 6 float（dp.xyz, dn.xyz）。
    const float deltas[12] = {1, 0, 0, 0, 1, 0,
                              2, 0, 0, 0, 1, 0};
    dse_morph_add_target(id, "smile", deltas, 12);
    EXPECT_EQ(dse_morph_get_target_count(id), 1);

    dse_morph_set_weight(id, "smile", 0.6f);
    EXPECT_FLOAT_EQ(dse_morph_get_weight(id, "smile"), 0.6f);
    dse_morph_set_weight_index(id, 0, 0.3f);
    EXPECT_FLOAT_EQ(dse_morph_get_weight(id, "smile"), 0.3f);

    auto& comp = world_.registry().get<dse::MorphTargetComponent>(e);
    ASSERT_EQ(comp.targets.size(), 1u);
    EXPECT_EQ(comp.targets[0].name, "smile");
    ASSERT_EQ(comp.targets[0].deltas.size(), 2u);
    EXPECT_FLOAT_EQ(comp.targets[0].deltas[1].delta_position.x, 2.0f);
    EXPECT_EQ(comp.vertex_count, 2);
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

// ============================================================
// Task 6: phys3d C ABI 上移（组件创建 / 标量 setter / 重叠查询）
// ============================================================

TEST_F(DseApiBindingsTest, Phys3D_AddComponents_MatchInlineValues) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);

    dse_rigidbody3d_add(id, static_cast<int>(dse::RigidBody3DType::Static), 5.0f);
    const auto& rb = world_.registry().get<dse::RigidBody3DComponent>(e);
    EXPECT_EQ(rb.type, dse::RigidBody3DType::Static);
    EXPECT_FLOAT_EQ(rb.mass, 5.0f);

    dse_box_collider3d_add(id, 2.0f, 3.0f, 4.0f);
    const auto& box = world_.registry().get<dse::BoxCollider3DComponent>(e);
    EXPECT_FLOAT_EQ(box.size.x, 2.0f);
    EXPECT_FLOAT_EQ(box.size.z, 4.0f);

    dse_capsule_collider3d_add(id, 0.5f, 1.8f, 1, 1);
    const auto& cap = world_.registry().get<dse::CapsuleCollider3DComponent>(e);
    EXPECT_FLOAT_EQ(cap.radius, 0.5f);
    EXPECT_TRUE(cap.is_trigger);
}

TEST_F(DseApiBindingsTest, Phys3D_JointSettersAndQueries) {
    Entity a = world_.CreateEntity();
    const uint32_t id = EntityId(a);

    dse_joint3d_add(id, 7u, static_cast<int>(dse::Joint3DType::Hinge),
                    1.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 100.0f, 50.0f);
    dse_joint3d_set_hinge_limits(id, -30.0f, 60.0f);
    dse_joint3d_set_spring(id, 200.0f, 0.5f);
    dse_joint3d_set_distance(id, 1.0f, 4.0f);

    const auto& jc = world_.registry().get<dse::Joint3DComponent>(a);
    EXPECT_EQ(jc.type, dse::Joint3DType::Hinge);
    EXPECT_EQ(jc.connected_entity_id, 7u);
    EXPECT_TRUE(jc.use_limits);
    EXPECT_FLOAT_EQ(jc.lower_limit, -30.0f);
    EXPECT_FLOAT_EQ(jc.upper_limit, 60.0f);
    EXPECT_FLOAT_EQ(jc.spring_stiffness, 200.0f);
    EXPECT_FLOAT_EQ(jc.max_distance, 4.0f);
    EXPECT_EQ(dse_joint3d_is_broken(id), 0);
}

TEST_F(DseApiBindingsTest, Phys3D_ColliderTriggerAndMaterial_AllTypes) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);
    dse_box_collider3d_add(id, 1.0f, 1.0f, 1.0f);
    dse_sphere_collider3d_add(id, 0.5f);

    dse_collider_set_trigger(id, 1);
    dse_collider_set_material(id, 0.8f, 0.2f);

    const auto& box = world_.registry().get<dse::BoxCollider3DComponent>(e);
    const auto& sph = world_.registry().get<dse::SphereCollider3DComponent>(e);
    EXPECT_TRUE(box.is_trigger);
    EXPECT_TRUE(sph.is_trigger);
    EXPECT_FLOAT_EQ(box.friction, 0.8f);
    EXPECT_FLOAT_EQ(sph.bounciness, 0.2f);
}

TEST_F(DseApiBindingsTest, Phys3D_CollisionLayerRoundTrip) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);
    dse_rigidbody3d_add(id, static_cast<int>(dse::RigidBody3DType::Dynamic), 1.0f);
    dse_collision_set_layer(id, 3, 0x00FF);
    const auto& rb = world_.registry().get<dse::RigidBody3DComponent>(e);
    EXPECT_EQ(rb.collision_layer, 3u);
    EXPECT_EQ(rb.collision_mask, 0x00FFu);
}

TEST_F(DseApiBindingsTest, Phys3D_TerrainHeightmapDataAndQuery) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);
    // 2x2 grid, block_size 1, origin (0,0)
    dse_terrain_heightmap_add(id, 0.0f, 0.0f, 1.0f, 2, 2, 1.0f, 0);
    float heights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    dse_terrain_heightmap_set_data(id, heights, 4);
    const auto& hm = world_.registry().get<dse::TerrainHeightmapComponent>(e);
    ASSERT_EQ(hm.heights.size(), 4u);
    EXPECT_EQ(hm.cols, 2);

    float y = -123.0f;
    EXPECT_EQ(dse_terrain_get_height(0.5f, 0.5f, &y), 1);
    EXPECT_FLOAT_EQ(y, 0.0f);
}

TEST_F(DseApiBindingsTest, Phys3D_OverlapSphere_FindsEntitiesInRange) {
    Entity inside = world_.CreateEntity();
    auto& t1 = world_.registry().emplace<TransformComponent>(inside);
    t1.position = glm::vec3(0.0f, 0.0f, 0.0f);
    t1.scale = glm::vec3(1.0f);
    auto& s1 = world_.registry().emplace<dse::SphereCollider3DComponent>(inside);
    s1.radius = 0.5f;

    Entity outside = world_.CreateEntity();
    auto& t2 = world_.registry().emplace<TransformComponent>(outside);
    t2.position = glm::vec3(100.0f, 0.0f, 0.0f);
    t2.scale = glm::vec3(1.0f);
    auto& s2 = world_.registry().emplace<dse::SphereCollider3DComponent>(outside);
    s2.radius = 0.5f;

    uint32_t hits[16];
    int n = dse_physics3d_overlap_sphere(0.0f, 0.0f, 0.0f, 1.0f, hits, 16);
    ASSERT_EQ(n, 1);
    EXPECT_EQ(hits[0], EntityId(inside));
}

TEST_F(DseApiBindingsTest, Phys3D_OverlapBox_FindsBoxOverlap) {
    Entity e = world_.CreateEntity();
    auto& t = world_.registry().emplace<TransformComponent>(e);
    t.position = glm::vec3(0.0f, 0.0f, 0.0f);
    t.scale = glm::vec3(1.0f);
    auto& b = world_.registry().emplace<dse::BoxCollider3DComponent>(e);
    b.size = glm::vec3(2.0f, 2.0f, 2.0f);

    uint32_t hits[16];
    int n = dse_physics3d_overlap_box(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, hits, 16);
    ASSERT_EQ(n, 1);
    EXPECT_EQ(hits[0], EntityId(e));

    int none = dse_physics3d_overlap_box(50.0f, 50.0f, 50.0f, 51.0f, 51.0f, 51.0f, hits, 16);
    EXPECT_EQ(none, 0);
}
