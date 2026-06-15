/**
 * @file physics2d_system_test.cpp
 * @brief Physics2DSystem 二维物理系统单元测试
 *
 * 覆盖场景：
 *   1.  构造/析构不崩溃
 *   2.  Init 创建物理世界（runtime_body 被赋值）
 *   3.  Shutdown 多次调用安全
 *   4.  空场景 FixedUpdate 不崩溃
 *   5.  动态刚体在重力下下落
 *   6.  静态刚体不移动
 *   7.  两个动态物体碰撞后产生接触事件
 *   8.  Raycast 命中物体
 *   9.  Raycast 未命中返回 false
 *   10. 无关节实体调用 DestroyJoint 不崩溃
 *   11. 圆形碰撞体也正常创建
 *   12. 碰撞回调被触发
 */

#include <gtest/gtest.h>
#include "engine/physics/physics2d/physics2d_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_2d.h"
#include <glm/glm.hpp>

// ============================================================
// Fixture：创建 World + Physics2DSystem，提供实体创建辅助
// ============================================================

class Physics2DSystemTest : public ::testing::Test {
protected:
    World world;
    Physics2DSystem physics;

    void SetUp() override {}

    void TearDown() override {
        physics.Shutdown();
    }

    Entity CreateDynamicBox(float x, float y,
                            glm::vec2 box_size = {1.0f, 1.0f}) {
        Entity e = world.CreateEntity();
        auto& tc = world.registry().emplace<TransformComponent>(e);
        tc.position = glm::vec3(x, y, 0.0f);

        auto& rb = world.registry().emplace<RigidBody2DComponent>(e);
        rb.type = RigidBody2DType::Dynamic;

        auto& bc = world.registry().emplace<BoxCollider2DComponent>(e);
        bc.size = box_size;
        return e;
    }

    Entity CreateStaticBox(float x, float y,
                           glm::vec2 box_size = {10.0f, 1.0f}) {
        Entity e = world.CreateEntity();
        auto& tc = world.registry().emplace<TransformComponent>(e);
        tc.position = glm::vec3(x, y, 0.0f);

        auto& rb = world.registry().emplace<RigidBody2DComponent>(e);
        rb.type = RigidBody2DType::Static;

        auto& bc = world.registry().emplace<BoxCollider2DComponent>(e);
        bc.size = box_size;
        return e;
    }

    Entity CreateDynamicCircle(float x, float y, float radius = 0.5f) {
        Entity e = world.CreateEntity();
        auto& tc = world.registry().emplace<TransformComponent>(e);
        tc.position = glm::vec3(x, y, 0.0f);

        auto& rb = world.registry().emplace<RigidBody2DComponent>(e);
        rb.type = RigidBody2DType::Dynamic;

        auto& cc = world.registry().emplace<CircleCollider2DComponent>(e);
        cc.radius = radius;
        return e;
    }
};

// ============================================================
// 1. 构造 / 析构
// ============================================================

// 测试 物理2D系统：且不崩溃
TEST_F(Physics2DSystemTest, AndDoesNotCrash) {
    Physics2DSystem sys;
}

// ============================================================
// 2. Init 创建物理世界
// ============================================================

// 测试 物理2D系统：初始化之后刚体为Obtained运行时刚体
TEST_F(Physics2DSystemTest, InitAfterTheRigidBodyIsObtainedRuntimeBody) {
    Entity e = CreateDynamicBox(0.0f, 5.0f);
    physics.Init(world);

    auto& rb = world.registry().get<RigidBody2DComponent>(e);
    EXPECT_NE(rb.runtime_body, nullptr);
}

// 测试 物理2D系统：初始化后期碰撞刚体Obtained运行时Fixture
TEST_F(Physics2DSystemTest, InitPostCollisionBodyObtainedRuntimeFixture) {
    Entity e = CreateDynamicBox(0.0f, 5.0f);
    physics.Init(world);

    auto& bc = world.registry().get<BoxCollider2DComponent>(e);
    EXPECT_NE(bc.runtime_fixture, nullptr);
}

// ============================================================
// 3. Shutdown 多次调用
// ============================================================

// 测试 物理2D系统：多次数关闭不崩溃
TEST_F(Physics2DSystemTest, MultiTimesShutdownDoesNotCrash) {
    physics.Init(world);
    physics.Shutdown();
    physics.Shutdown();
    physics.Shutdown();
}

// 测试 物理2D系统：关闭稍后Initworks Fine
TEST_F(Physics2DSystemTest, ShutdownLaterInitworksFine) {
    Entity e = CreateDynamicBox(0.0f, 0.0f);
    physics.Init(world);
    physics.Shutdown();

    physics.Init(world);
    auto& rb = world.registry().get<RigidBody2DComponent>(e);
    EXPECT_NE(rb.runtime_body, nullptr);
}

// ============================================================
// 4. 空场景 FixedUpdate
// ============================================================

// 测试 物理2D系统：空场景固定更新不崩溃
TEST_F(Physics2DSystemTest, EmptySceneFixedUpdateDoesNotCrash) {
    physics.Init(world);
    physics.FixedUpdate(world, 1.0f / 60.0f);
}

// 测试 物理2D系统：当不已初始化固定更新不崩溃
TEST_F(Physics2DSystemTest, WhenNotInitializedFixedUpdateDoesNotCrash) {
    physics.FixedUpdate(world, 1.0f / 60.0f);
}

// ============================================================
// 5. 动态刚体在重力下下落
// ============================================================

// 测试 物理2D系统：存在Y
TEST_F(Physics2DSystemTest, ExistY) {
    Entity e = CreateDynamicBox(0.0f, 10.0f);
    physics.Init(world);

    const float initial_y = world.registry().get<TransformComponent>(e).position.y;

    constexpr float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i) {
        physics.FixedUpdate(world, dt);
    }

    float final_y = world.registry().get<TransformComponent>(e).position.y;
    EXPECT_LT(final_y, initial_y) << "动态刚体经过1秒模拟应在重力下下落";
}

// ============================================================
// 6. 静态刚体不移动
// ============================================================

// 测试 物理2D系统：Constant
TEST_F(Physics2DSystemTest, Constant) {
    Entity e = CreateStaticBox(0.0f, 0.0f);
    physics.Init(world);

    float initial_y = world.registry().get<TransformComponent>(e).position.y;

    constexpr float dt = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i) {
        physics.FixedUpdate(world, dt);
    }

    float final_y = world.registry().get<TransformComponent>(e).position.y;
    EXPECT_FLOAT_EQ(final_y, initial_y);
}

// ============================================================
// 7. 碰撞检测：动态物体落到静态平台上会停住
// ============================================================

// 测试 物理2D系统：到
TEST_F(Physics2DSystemTest, To) {
    CreateStaticBox(0.0f, -5.0f, {20.0f, 1.0f});
    Entity dynamic_e = CreateDynamicBox(0.0f, 5.0f, {1.0f, 1.0f});
    physics.Init(world);

    constexpr float dt = 1.0f / 60.0f;
    for (int i = 0; i < 600; ++i) {
        physics.FixedUpdate(world, dt);
    }

    float final_y = world.registry().get<TransformComponent>(dynamic_e).position.y;
    EXPECT_GT(final_y, -5.5f) << "动态物体应被静态平台托住，不会无限下落";
}

// ============================================================
// 8. Raycast 命中
// ============================================================

// 测试 物理2D系统：射线检测命中已存在对象
TEST_F(Physics2DSystemTest, RaycastHitAnExistingObject) {
    CreateStaticBox(0.0f, 0.0f, {4.0f, 4.0f});
    physics.Init(world);

    Entity hit_entity = entt::null;
    glm::vec2 hit_point{0.0f};
    glm::vec2 hit_normal{0.0f};

    bool hit = physics.Raycast(
        glm::vec2(-10.0f, 0.0f),
        glm::vec2(10.0f, 0.0f),
        hit_entity, hit_point, hit_normal);

    EXPECT_TRUE(hit);
    EXPECT_TRUE(hit_entity != entt::null);
}

// ============================================================
// 9. Raycast 未命中
// ============================================================

// 测试 物理2D系统：射线检测空场景返回false
TEST_F(Physics2DSystemTest, RaycastEmptySceneReturnfalse) {
    physics.Init(world);

    Entity hit_entity = entt::null;
    glm::vec2 hit_point{0.0f};
    glm::vec2 hit_normal{0.0f};

    bool hit = physics.Raycast(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(10.0f, 0.0f),
        hit_entity, hit_point, hit_normal);

    EXPECT_FALSE(hit);
}

// 测试 物理2D系统：射线检测Deviating对象返回false
TEST_F(Physics2DSystemTest, RaycastDeviatingObjectReturnsfalse) {
    CreateStaticBox(0.0f, 0.0f, {2.0f, 2.0f});
    physics.Init(world);

    Entity hit_entity = entt::null;
    glm::vec2 hit_point{0.0f};
    glm::vec2 hit_normal{0.0f};

    bool hit = physics.Raycast(
        glm::vec2(-10.0f, 100.0f),
        glm::vec2(10.0f, 100.0f),
        hit_entity, hit_point, hit_normal);

    EXPECT_FALSE(hit);
}

// 测试 物理2D系统：当不已初始化射线检测返回false
TEST_F(Physics2DSystemTest, WhenNotInitializedRaycastReturnsfalse) {
    Entity hit_entity = entt::null;
    glm::vec2 hit_point{0.0f};
    glm::vec2 hit_normal{0.0f};

    bool hit = physics.Raycast(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(10.0f, 0.0f),
        hit_entity, hit_point, hit_normal);

    EXPECT_FALSE(hit);
}

// ============================================================
// 10. DestroyJoint 安全性
// ============================================================

// 测试 物理2D系统：无实体调用销毁关节不崩溃
TEST_F(Physics2DSystemTest, WithoutEntityCallsDestroyJointDoesNotCrash) {
    Entity e = CreateDynamicBox(0.0f, 0.0f);
    physics.Init(world);
    physics.DestroyJoint(world, e);
}

// 测试 物理2D系统：无效实体调用销毁关节不崩溃
TEST_F(Physics2DSystemTest, InvalidEntityCallsDestroyJointDoesNotCrash) {
    physics.Init(world);
    physics.DestroyJoint(world, entt::null);
}

// 测试 物理2D系统：当不已初始化销毁关节不崩溃
TEST_F(Physics2DSystemTest, WhenNotInitializedDestroyJointDoesNotCrash) {
    Entity e = CreateDynamicBox(0.0f, 0.0f);
    physics.DestroyJoint(world, e);
}

// ============================================================
// 11. 圆形碰撞体
// ============================================================

// 测试 物理2D系统：初始化之后运行时Fixture
TEST_F(Physics2DSystemTest, InitAfterRuntimeFixture) {
    Entity e = CreateDynamicCircle(0.0f, 5.0f, 1.0f);
    physics.Init(world);

    auto& cc = world.registry().get<CircleCollider2DComponent>(e);
    EXPECT_NE(cc.runtime_fixture, nullptr);
}

// 测试 物理2D系统：存在
TEST_F(Physics2DSystemTest, Exist) {
    Entity e = CreateDynamicCircle(0.0f, 10.0f, 0.5f);
    physics.Init(world);

    float initial_y = world.registry().get<TransformComponent>(e).position.y;

    constexpr float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i) {
        physics.FixedUpdate(world, dt);
    }

    float final_y = world.registry().get<TransformComponent>(e).position.y;
    EXPECT_LT(final_y, initial_y);
}

// ============================================================
// 12. 碰撞回调触发
// ============================================================

// 测试 物理2D系统：按触发
TEST_F(Physics2DSystemTest, ByTriggers) {
    CreateStaticBox(0.0f, -2.0f, {10.0f, 1.0f});
    Entity dynamic_e = CreateDynamicBox(0.0f, 2.0f, {1.0f, 1.0f});
    physics.Init(world);

    bool collision_detected = false;
    auto& rb = world.registry().get<RigidBody2DComponent>(dynamic_e);
    rb.on_collision_enter = [&collision_detected](Entity) {
        collision_detected = true;
    };

    constexpr float dt = 1.0f / 60.0f;
    for (int i = 0; i < 300; ++i) {
        physics.FixedUpdate(world, dt);
        if (collision_detected) break;
    }

    EXPECT_TRUE(collision_detected) << "动态物体落向静态平台应触发碰撞回调";
}

// 测试 物理2D系统：Triggersevent按到
TEST_F(Physics2DSystemTest, TriggerseventByTo) {
    Entity sensor_e = CreateStaticBox(0.0f, -2.0f, {10.0f, 1.0f});
    world.registry().get<BoxCollider2DComponent>(sensor_e).is_trigger = true;

    Entity dynamic_e = CreateDynamicBox(0.0f, 2.0f, {1.0f, 1.0f});
    physics.Init(world);

    constexpr float dt = 1.0f / 60.0f;
    for (int i = 0; i < 300; ++i) {
        physics.FixedUpdate(world, dt);
    }

    auto& rb_dynamic = world.registry().get<RigidBody2DComponent>(dynamic_e);
    EXPECT_FALSE(rb_dynamic.pending_contact_events.empty())
        << "穿过触发器区域应产生待处理接触事件";
}
