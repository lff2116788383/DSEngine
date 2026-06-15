/**
 * @file physics2d_circle_collider_integration_test.cpp
 * @brief CircleCollider2DComponent 集成测试（使用真实 Box2D 世界）
 *
 * 覆盖场景：
 *   1. 圆形碰撞体动态刚体受重力下落
 *   2. 圆形碰撞体能与矩形碰撞体发生碰撞
 *   3. 圆形碰撞体射线检测命中
 *   4. 圆形碰撞体 is_trigger 仅触发不产生碰撞力
 *   5. 同一实体同时挂载 BoxCollider + CircleCollider
 *   6. 实体销毁后系统仍正常
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

class Physics2DCircleColliderTest : public ::testing::Test {
protected:
    World world;
    Physics2DSystem sys;

    void TearDown() override { sys.Shutdown(); }

    Entity MakeCircleBody(float x, float y, float radius,
                          RigidBody2DType type = RigidBody2DType::Dynamic,
                          float density = 1.0f) {
        Entity e = world.CreateEntity();
        TransformComponent tc;
        tc.position = glm::vec3(x, y, 0.0f);
        tc.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        tc.scale = glm::vec3(1.0f);
        world.registry().emplace<TransformComponent>(e, tc);
        auto& rb = world.registry().emplace<RigidBody2DComponent>(e);
        rb.type = type;
        auto& cc = world.registry().emplace<CircleCollider2DComponent>(e);
        cc.radius = radius;
        cc.density = density;
        return e;
    }

    Entity MakeBoxBody(float x, float y, float w, float h,
                       RigidBody2DType type = RigidBody2DType::Static) {
        Entity e = world.CreateEntity();
        TransformComponent tc;
        tc.position = glm::vec3(x, y, 0.0f);
        tc.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        tc.scale = glm::vec3(1.0f);
        world.registry().emplace<TransformComponent>(e, tc);
        auto& rb = world.registry().emplace<RigidBody2DComponent>(e);
        rb.type = type;
        auto& bc = world.registry().emplace<BoxCollider2DComponent>(e);
        bc.size = glm::vec2(w, h);
        return e;
    }
};

// 测试 物理2D圆碰撞体：情形1
TEST_F(Physics2DCircleColliderTest, TestCase1) {
    Entity ball = MakeCircleBody(0.0f, 10.0f, 0.5f);
    sys.Init(world);

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i) {
        sys.FixedUpdate(world, kDt);
    }

    auto& t = world.registry().get<TransformComponent>(ball);
    EXPECT_LT(t.position.y, 10.0f);
}

// 测试 物理2D圆碰撞体：且
TEST_F(Physics2DCircleColliderTest, And) {
    // 地面
    MakeBoxBody(0.0f, -5.0f, 20.0f, 1.0f);
    // 球
    Entity ball = MakeCircleBody(0.0f, 5.0f, 0.5f);

    sys.Init(world);

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i) {
        sys.FixedUpdate(world, kDt);
    }

    // 球应被地面拦住，不会低于地面
    auto& t = world.registry().get<TransformComponent>(ball);
    EXPECT_GT(t.position.y, -5.5f);
    EXPECT_LT(t.position.y, 5.0f);
}

// 测试 物理2D圆碰撞体：命中
TEST_F(Physics2DCircleColliderTest, Hit) {
    MakeCircleBody(0.0f, 0.0f, 1.0f, RigidBody2DType::Static);
    sys.Init(world);

    glm::vec2 start(0.0f, 5.0f);
    glm::vec2 end(0.0f, -5.0f);
    Entity hit_entity = entt::null;
    glm::vec2 hit_point, hit_normal;

    bool hit = sys.Raycast(start, end, hit_entity, hit_point, hit_normal);
    EXPECT_TRUE(hit);
    EXPECT_TRUE(hit_entity != entt::null);
}

// 测试 物理2D圆碰撞体：触发不
TEST_F(Physics2DCircleColliderTest, TriggersNot) {
    MakeBoxBody(0.0f, -5.0f, 20.0f, 1.0f);

    Entity ball = MakeCircleBody(0.0f, 0.0f, 0.5f, RigidBody2DType::Dynamic);
    world.registry().get<CircleCollider2DComponent>(ball).is_trigger = true;

    sys.Init(world);

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i) {
        sys.FixedUpdate(world, kDt);
    }

    // 触发器不产生碰撞力，球应穿过地面继续下落
    auto& t = world.registry().get<TransformComponent>(ball);
    EXPECT_LT(t.position.y, -5.0f);
}

// 测试 物理2D圆碰撞体：单个实体当带盒且圆
TEST_F(Physics2DCircleColliderTest, OneEntityWhenWithBoxAndCircle) {
    Entity e = world.CreateEntity();
    TransformComponent tc;
    tc.position = glm::vec3(0.0f, 10.0f, 0.0f);
    tc.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    tc.scale = glm::vec3(1.0f);
    world.registry().emplace<TransformComponent>(e, tc);
    auto& rb = world.registry().emplace<RigidBody2DComponent>(e);
    rb.type = RigidBody2DType::Dynamic;
    auto& bc = world.registry().emplace<BoxCollider2DComponent>(e);
    bc.size = glm::vec2(1.0f, 1.0f);
    auto& cc = world.registry().emplace<CircleCollider2DComponent>(e);
    cc.radius = 0.5f;

    sys.Init(world);

    // 两个 fixture 都应被创建
    EXPECT_NE(bc.runtime_fixture, nullptr);
    EXPECT_NE(cc.runtime_fixture, nullptr);

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 30; ++i) {
        sys.FixedUpdate(world, kDt);
    }

    auto& t = world.registry().get<TransformComponent>(e);
    EXPECT_LT(t.position.y, 10.0f);
}

// 测试 物理2D圆碰撞体：实体销毁之后系统法线
TEST_F(Physics2DCircleColliderTest, EntityDestroyAfterSystemNormal) {
    Entity ball = MakeCircleBody(0.0f, 5.0f, 0.5f);
    sys.Init(world);
    sys.FixedUpdate(world, 1.0f / 60.0f);

    world.DestroyEntity(ball);

    // 系统应仍可运行不崩溃
    sys.FixedUpdate(world, 1.0f / 60.0f);
    SUCCEED();
}

// 测试 物理2D圆碰撞体：Reinit后期环形碰撞刚体重建
TEST_F(Physics2DCircleColliderTest, ReinitPostCircularCollisionBodyReconstruction) {
    Entity ball = MakeCircleBody(0.0f, 0.0f, 1.0f, RigidBody2DType::Static);
    sys.Init(world);
    EXPECT_NE(world.registry().get<CircleCollider2DComponent>(ball).runtime_fixture, nullptr);

    sys.Init(world);
    EXPECT_NE(world.registry().get<CircleCollider2DComponent>(ball).runtime_fixture, nullptr);
}
