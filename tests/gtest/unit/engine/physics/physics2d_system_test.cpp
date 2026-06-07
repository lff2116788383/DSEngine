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

TEST_F(Physics2DSystemTest, 构造与析构不崩溃) {
    Physics2DSystem sys;
}

// ============================================================
// 2. Init 创建物理世界
// ============================================================

TEST_F(Physics2DSystemTest, Init后刚体获得RuntimeBody) {
    Entity e = CreateDynamicBox(0.0f, 5.0f);
    physics.Init(world);

    auto& rb = world.registry().get<RigidBody2DComponent>(e);
    EXPECT_NE(rb.runtime_body, nullptr);
}

TEST_F(Physics2DSystemTest, Init后碰撞体获得RuntimeFixture) {
    Entity e = CreateDynamicBox(0.0f, 5.0f);
    physics.Init(world);

    auto& bc = world.registry().get<BoxCollider2DComponent>(e);
    EXPECT_NE(bc.runtime_fixture, nullptr);
}

// ============================================================
// 3. Shutdown 多次调用
// ============================================================

TEST_F(Physics2DSystemTest, 多次Shutdown不崩溃) {
    physics.Init(world);
    physics.Shutdown();
    physics.Shutdown();
    physics.Shutdown();
}

TEST_F(Physics2DSystemTest, Shutdown后再Init可正常工作) {
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

TEST_F(Physics2DSystemTest, 空场景FixedUpdate不崩溃) {
    physics.Init(world);
    physics.FixedUpdate(world, 1.0f / 60.0f);
}

TEST_F(Physics2DSystemTest, 未初始化时FixedUpdate不崩溃) {
    physics.FixedUpdate(world, 1.0f / 60.0f);
}

// ============================================================
// 5. 动态刚体在重力下下落
// ============================================================

TEST_F(Physics2DSystemTest, 动态刚体在重力下Y坐标减小) {
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

TEST_F(Physics2DSystemTest, 静态刚体位置不变) {
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

TEST_F(Physics2DSystemTest, 动态物体落到静态平台停止下落) {
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

TEST_F(Physics2DSystemTest, Raycast命中存在的物体) {
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

TEST_F(Physics2DSystemTest, Raycast空场景返回false) {
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

TEST_F(Physics2DSystemTest, Raycast偏离物体返回false) {
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

TEST_F(Physics2DSystemTest, 未初始化时Raycast返回false) {
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

TEST_F(Physics2DSystemTest, 无关节实体调用DestroyJoint不崩溃) {
    Entity e = CreateDynamicBox(0.0f, 0.0f);
    physics.Init(world);
    physics.DestroyJoint(world, e);
}

TEST_F(Physics2DSystemTest, 对无效实体调用DestroyJoint不崩溃) {
    physics.Init(world);
    physics.DestroyJoint(world, entt::null);
}

TEST_F(Physics2DSystemTest, 未初始化时DestroyJoint不崩溃) {
    Entity e = CreateDynamicBox(0.0f, 0.0f);
    physics.DestroyJoint(world, e);
}

// ============================================================
// 11. 圆形碰撞体
// ============================================================

TEST_F(Physics2DSystemTest, 圆形碰撞体Init后获得RuntimeFixture) {
    Entity e = CreateDynamicCircle(0.0f, 5.0f, 1.0f);
    physics.Init(world);

    auto& cc = world.registry().get<CircleCollider2DComponent>(e);
    EXPECT_NE(cc.runtime_fixture, nullptr);
}

TEST_F(Physics2DSystemTest, 圆形刚体在重力下下落) {
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

TEST_F(Physics2DSystemTest, 碰撞回调被触发) {
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

TEST_F(Physics2DSystemTest, 触发器事件被投递到待处理队列) {
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
