/**
 * @file ecs_physics2d_integration_test.cpp
 * @brief ECS + Physics2DSystem 集成测试
 *
 * 验证场景：
 * - 实体挂载物理组件后，Physics2DSystem 初始化与固定帧更新正确
 * - 物理模拟结果同步回 ECS TransformComponent
 * - 碰撞检测与射线投射
 * - 物理实体销毁后系统状态一致性
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/physics_2d.h"
#include "engine/physics/physics2d/physics2d_system.h"
#include <glm/glm.hpp>

// ============================================================
// Physics2DSystem 与 ECS 协作
// ============================================================

class EcsPhysics2DIntegrationTest : public ::testing::Test {
protected:
    World world;
    Physics2DSystem physics_system;

    void SetUp() override {}
    void TearDown() override {
        physics_system.Shutdown();
    }
};

// 测试 ECS物理2D集成：情形1
TEST_F(EcsPhysics2DIntegrationTest, TestCase1) {
    // 创建带物理组件的实体
    Entity e = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(e);
    transform.position = glm::vec3(0.0f, 10.0f, 0.0f);
    transform.dirty = true;

    auto& rb = world.registry().emplace<RigidBody2DComponent>(e);
    rb.type = RigidBody2DType::Dynamic;
    rb.gravity_scale = 1.0f;

    auto& collider = world.registry().emplace<BoxCollider2DComponent>(e);
    collider.size = glm::vec2(1.0f, 1.0f);
    collider.density = 1.0f;

    // 初始化物理系统
    physics_system.Init(world);

    // 模拟若干固定帧
    constexpr float kFixedDt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i) {
        physics_system.FixedUpdate(world, kFixedDt);
    }

    // 物理更新后，Transform 的 Y 坐标应因重力下降
    // 不检查精确值（Box2D 内部运算可能有微小差异），只验证下降趋势
    EXPECT_LT(transform.position.y, 10.0f);
}

// 测试 ECS物理2D集成：不
TEST_F(EcsPhysics2DIntegrationTest, Not) {
    Entity e = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(e);
    transform.position = glm::vec3(0.0f, 5.0f, 0.0f);
    transform.dirty = true;

    auto& rb = world.registry().emplace<RigidBody2DComponent>(e);
    rb.type = RigidBody2DType::Static;

    auto& collider = world.registry().emplace<BoxCollider2DComponent>(e);
    collider.size = glm::vec2(10.0f, 1.0f);

    physics_system.Init(world);

    constexpr float kFixedDt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i) {
        physics_system.FixedUpdate(world, kFixedDt);
    }

    // 静态刚体 Y 坐标不应变化
    EXPECT_FLOAT_EQ(transform.position.y, 5.0f);
}

// 测试 ECS物理2D集成：不能够速度
TEST_F(EcsPhysics2DIntegrationTest, NotCanSpeed) {
    Entity e = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(e);
    transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    transform.dirty = true;

    auto& rb = world.registry().emplace<RigidBody2DComponent>(e);
    rb.type = RigidBody2DType::Kinematic;
    rb.velocity = glm::vec2(5.0f, 0.0f); // 水平运动

    auto& collider = world.registry().emplace<BoxCollider2DComponent>(e);
    collider.size = glm::vec2(1.0f, 1.0f);

    physics_system.Init(world);

    constexpr float kFixedDt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i) {
        physics_system.FixedUpdate(world, kFixedDt);
    }

    // 运动学刚体应沿 X 正方向移动
    EXPECT_GT(transform.position.x, 0.0f);
    // 不应受重力下落
    EXPECT_FLOAT_EQ(transform.position.y, 0.0f);
}

// 测试 ECS物理2D集成：命中
TEST_F(EcsPhysics2DIntegrationTest, Hit) {
    // 创建一个静态地面
    Entity ground = world.CreateEntity();
    auto& gt = world.registry().emplace<TransformComponent>(ground);
    gt.position = glm::vec3(0.0f, -5.0f, 0.0f);
    gt.dirty = true;
    auto& grb = world.registry().emplace<RigidBody2DComponent>(ground);
    grb.type = RigidBody2DType::Static;
    auto& gc = world.registry().emplace<BoxCollider2DComponent>(ground);
    gc.size = glm::vec2(20.0f, 1.0f);

    physics_system.Init(world);

    // 从上方往下做射线检测
    glm::vec2 start(0.0f, 0.0f);
    glm::vec2 end(0.0f, -10.0f);
    Entity hit_entity = entt::null;
    glm::vec2 hit_point;
    glm::vec2 hit_normal;

    bool hit = physics_system.Raycast(start, end, hit_entity, hit_point, hit_normal);

    EXPECT_TRUE(hit);
    EXPECT_TRUE(hit_entity != entt::null);
}

// 测试 ECS物理2D集成：未命中返回false
TEST_F(EcsPhysics2DIntegrationTest, NothitReturnsfalse) {
    Entity ground = world.CreateEntity();
    auto& gt = world.registry().emplace<TransformComponent>(ground);
    gt.position = glm::vec3(0.0f, -5.0f, 0.0f);
    gt.dirty = true;
    auto& grb = world.registry().emplace<RigidBody2DComponent>(ground);
    grb.type = RigidBody2DType::Static;
    auto& gc = world.registry().emplace<BoxCollider2DComponent>(ground);
    gc.size = glm::vec2(1.0f, 1.0f);

    physics_system.Init(world);

    // 射线方向偏离碰撞体
    glm::vec2 start(50.0f, 0.0f);
    glm::vec2 end(50.0f, -10.0f);
    Entity hit_entity = entt::null;
    glm::vec2 hit_point;
    glm::vec2 hit_normal;

    bool hit = physics_system.Raycast(start, end, hit_entity, hit_point, hit_normal);

    EXPECT_FALSE(hit);
}

// 测试 ECS物理2D集成：实体销毁之后系统仍能够法线
TEST_F(EcsPhysics2DIntegrationTest, EntityDestroyAfterSystemStillCanNormal) {
    Entity e1 = world.CreateEntity();
    auto& t1 = world.registry().emplace<TransformComponent>(e1);
    t1.position = glm::vec3(0.0f, 10.0f, 0.0f);
    t1.dirty = true;
    world.registry().emplace<RigidBody2DComponent>(e1).type = RigidBody2DType::Dynamic;
    world.registry().emplace<BoxCollider2DComponent>(e1).size = glm::vec2(1.0f, 1.0f);

    physics_system.Init(world);

    // 模拟一帧
    physics_system.FixedUpdate(world, 1.0f / 60.0f);

    // 销毁实体
    world.DestroyEntity(e1);

    // 系统应仍可运行不崩溃
    physics_system.FixedUpdate(world, 1.0f / 60.0f);
    SUCCEED();
}

// 测试 ECS物理2D集成：之后关闭再次次数初始化不崩溃
TEST_F(EcsPhysics2DIntegrationTest, AfterClosingAgainTimesInitializeDoesNotCrash) {
    Entity e = world.CreateEntity();
    auto& t = world.registry().emplace<TransformComponent>(e);
    t.position = glm::vec3(0.0f, 0.0f, 0.0f);
    t.dirty = true;
    world.registry().emplace<RigidBody2DComponent>(e).type = RigidBody2DType::Static;
    world.registry().emplace<BoxCollider2DComponent>(e).size = glm::vec2(1.0f, 1.0f);

    physics_system.Init(world);
    physics_system.FixedUpdate(world, 1.0f / 60.0f);

    physics_system.Shutdown();

    // 重新初始化
    physics_system.Init(world);
    physics_system.FixedUpdate(world, 1.0f / 60.0f);
    SUCCEED();
}
