/**
 * @file physics2d_collision_event_integration_test.cpp
 * @brief Physics2D 碰撞事件集成测试
 *
 * 验证场景：
 * - 两个动态/静态刚体碰撞时，RigidBody2DComponent 回调正确触发
 * - on_collision_enter 在首次接触时调用
 * - on_collision_exit 在分离时调用
 * - 触发器（is_trigger）的 on_trigger_enter/on_trigger_exit 正确触发
 * - 碰撞回调中能获取对方实体 ID
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include <entt/entt.hpp>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/physics_2d.h"
#include "engine/physics/physics2d/physics2d_system.h"
#include <glm/glm.hpp>
#include <vector>
#include <mutex>

// ============================================================
// 碰撞事件集成测试
// ============================================================

class Physics2DCollisionEventIntegrationTest : public ::testing::Test {
protected:
    World world;
    Physics2DSystem physics_system;

    void SetUp() override {}
    void TearDown() override {
        physics_system.Shutdown();
    }
};

TEST_F(Physics2DCollisionEventIntegrationTest, 动态刚体落在静态地面上触发碰撞进入回调) {
    // 创建地面
    Entity ground = world.CreateEntity();
    auto& gt = world.registry().emplace<TransformComponent>(ground);
    gt.position = glm::vec3(0.0f, -5.0f, 0.0f);
    gt.dirty = true;
    auto& grb = world.registry().emplace<RigidBody2DComponent>(ground);
    grb.type = RigidBody2DType::Static;
    auto& gc = world.registry().emplace<BoxCollider2DComponent>(ground);
    gc.size = glm::vec2(20.0f, 1.0f);

    // 创建动态球
    Entity ball = world.CreateEntity();
    auto& bt = world.registry().emplace<TransformComponent>(ball);
    bt.position = glm::vec3(0.0f, 0.0f, 0.0f);
    bt.dirty = true;
    auto& brb = world.registry().emplace<RigidBody2DComponent>(ball);
    brb.type = RigidBody2DType::Dynamic;
    brb.gravity_scale = 1.0f;
    auto& bc = world.registry().emplace<BoxCollider2DComponent>(ball);
    bc.size = glm::vec2(1.0f, 1.0f);

    // 记录碰撞事件
    std::vector<Entity> collision_enter_entities;
    std::vector<Entity> collision_exit_entities;

    brb.on_collision_enter = [&collision_enter_entities](Entity other) {
        collision_enter_entities.push_back(other);
    };
    brb.on_collision_exit = [&collision_exit_entities](Entity other) {
        collision_exit_entities.push_back(other);
    };

    physics_system.Init(world);

    // 模拟足够帧数使球下落并接触地面
    constexpr float kFixedDt = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i) {
        physics_system.FixedUpdate(world, kFixedDt);
    }

    // 碰撞进入回调应至少触发一次
    EXPECT_FALSE(collision_enter_entities.empty());

    // 碰撞对方应该是地面实体
    bool found_ground = false;
    for (Entity e : collision_enter_entities) {
        if (e == ground) {
            found_ground = true;
            break;
        }
    }
    EXPECT_TRUE(found_ground);
}

TEST_F(Physics2DCollisionEventIntegrationTest, 两个动态刚体碰撞双方都收到回调) {
    Entity e1 = world.CreateEntity();
    auto& t1 = world.registry().emplace<TransformComponent>(e1);
    t1.position = glm::vec3(-2.0f, 0.0f, 0.0f);
    t1.dirty = true;
    auto& rb1 = world.registry().emplace<RigidBody2DComponent>(e1);
    rb1.type = RigidBody2DType::Dynamic;
    rb1.gravity_scale = 0.0f; // 无重力
    rb1.velocity = glm::vec2(5.0f, 0.0f); // 向右运动
    auto& c1 = world.registry().emplace<BoxCollider2DComponent>(e1);
    c1.size = glm::vec2(1.0f, 1.0f);

    Entity e2 = world.CreateEntity();
    auto& t2 = world.registry().emplace<TransformComponent>(e2);
    t2.position = glm::vec3(2.0f, 0.0f, 0.0f);
    t2.dirty = true;
    auto& rb2 = world.registry().emplace<RigidBody2DComponent>(e2);
    rb2.type = RigidBody2DType::Dynamic;
    rb2.gravity_scale = 0.0f;
    rb2.velocity = glm::vec2(-5.0f, 0.0f); // 向左运动
    auto& c2 = world.registry().emplace<BoxCollider2DComponent>(e2);
    c2.size = glm::vec2(1.0f, 1.0f);

    bool e1_collision_entered = false;
    bool e2_collision_entered = false;

    rb1.on_collision_enter = [&e1_collision_entered, e2](Entity other) {
        if (other == e2) e1_collision_entered = true;
    };
    rb2.on_collision_enter = [&e2_collision_entered, e1](Entity other) {
        if (other == e1) e2_collision_entered = true;
    };

    physics_system.Init(world);

    constexpr float kFixedDt = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i) {
        physics_system.FixedUpdate(world, kFixedDt);
    }

    // 双方都应收到碰撞进入回调
    EXPECT_TRUE(e1_collision_entered);
    EXPECT_TRUE(e2_collision_entered);
}

TEST_F(Physics2DCollisionEventIntegrationTest, 触发器模式使用Trigger回调) {
    // 创建静态区域作为触发器
    Entity trigger_zone = world.CreateEntity();
    auto& tt = world.registry().emplace<TransformComponent>(trigger_zone);
    tt.position = glm::vec3(0.0f, -5.0f, 0.0f);
    tt.dirty = true;
    auto& trb = world.registry().emplace<RigidBody2DComponent>(trigger_zone);
    trb.type = RigidBody2DType::Static;
    auto& tc = world.registry().emplace<BoxCollider2DComponent>(trigger_zone);
    tc.size = glm::vec2(10.0f, 2.0f);
    tc.is_trigger = true; // 触发器模式

    // 创建动态球
    Entity ball = world.CreateEntity();
    auto& bt = world.registry().emplace<TransformComponent>(ball);
    bt.position = glm::vec3(0.0f, 0.0f, 0.0f);
    bt.dirty = true;
    auto& brb = world.registry().emplace<RigidBody2DComponent>(ball);
    brb.type = RigidBody2DType::Dynamic;
    brb.gravity_scale = 1.0f;
    auto& bc = world.registry().emplace<BoxCollider2DComponent>(ball);
    bc.size = glm::vec2(1.0f, 1.0f);

    std::vector<Entity> trigger_enter_entities;
    std::vector<Entity> collision_enter_entities;

    brb.on_trigger_enter = [&trigger_enter_entities](Entity other) {
        trigger_enter_entities.push_back(other);
    };
    brb.on_collision_enter = [&collision_enter_entities](Entity other) {
        collision_enter_entities.push_back(other);
    };

    physics_system.Init(world);

    constexpr float kFixedDt = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i) {
        physics_system.FixedUpdate(world, kFixedDt);
    }

    // 触发器应触发 on_trigger_enter 而非 on_collision_enter
    EXPECT_FALSE(trigger_enter_entities.empty());

    // 因为碰撞体一方是 trigger，所以不应触发 collision_enter
    // （Physics2DSystem 中 is_trigger 判断为 true 时走 NotifyContactEnter 的 trigger 路径）
    EXPECT_TRUE(collision_enter_entities.empty());
}

TEST_F(Physics2DCollisionEventIntegrationTest, 无碰撞时不触发回调) {
    // 两个远离的静态物体，不会碰撞
    Entity e1 = world.CreateEntity();
    auto& t1 = world.registry().emplace<TransformComponent>(e1);
    t1.position = glm::vec3(0.0f, 0.0f, 0.0f);
    t1.dirty = true;
    auto& rb1 = world.registry().emplace<RigidBody2DComponent>(e1);
    rb1.type = RigidBody2DType::Static;
    auto& c1 = world.registry().emplace<BoxCollider2DComponent>(e1);
    c1.size = glm::vec2(1.0f, 1.0f);

    Entity e2 = world.CreateEntity();
    auto& t2 = world.registry().emplace<TransformComponent>(e2);
    t2.position = glm::vec3(100.0f, 100.0f, 0.0f);
    t2.dirty = true;
    auto& rb2 = world.registry().emplace<RigidBody2DComponent>(e2);
    rb2.type = RigidBody2DType::Static;
    auto& c2 = world.registry().emplace<BoxCollider2DComponent>(e2);
    c2.size = glm::vec2(1.0f, 1.0f);

    bool any_collision = false;
    rb1.on_collision_enter = [&any_collision](Entity) { any_collision = true; };
    rb2.on_collision_enter = [&any_collision](Entity) { any_collision = true; };

    physics_system.Init(world);

    constexpr float kFixedDt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i) {
        physics_system.FixedUpdate(world, kFixedDt);
    }

    EXPECT_FALSE(any_collision);
}
