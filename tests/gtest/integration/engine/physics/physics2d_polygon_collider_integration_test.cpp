/**
 * @file physics2d_polygon_collider_integration_test.cpp
 * @brief PolygonCollider2DComponent 集成测试（使用真实 Box2D 世界）
 *
 * 覆盖场景：
 *   1. 三角形碰撞体动态刚体受重力下落
 *   2. 多边形碰撞体能与矩形碰撞体发生碰撞
 *   3. 多边形碰撞体射线检测命中
 *   4. 多边形碰撞体 is_trigger 仅触发不产生碰撞力
 *   5. 顶点数不足 3 时不创建 fixture
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

class Physics2DPolygonColliderTest : public ::testing::Test {
protected:
    World world;
    Physics2DSystem sys;

    void TearDown() override { sys.Shutdown(); }

    Entity MakePolygonBody(float x, float y,
                           const std::vector<glm::vec2>& verts,
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
        auto& pc = world.registry().emplace<PolygonCollider2DComponent>(e);
        pc.vertices = verts;
        pc.density = density;
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

    // 单位正三角形
    std::vector<glm::vec2> Triangle() {
        return {{0.0f, 0.5f}, {-0.5f, -0.5f}, {0.5f, -0.5f}};
    }

    // 五边形
    std::vector<glm::vec2> Pentagon() {
        return {{0.0f, 1.0f}, {-0.95f, 0.31f}, {-0.59f, -0.81f},
                {0.59f, -0.81f}, {0.95f, 0.31f}};
    }
};

TEST_F(Physics2DPolygonColliderTest, 三角形碰撞体受重力下落) {
    Entity tri = MakePolygonBody(0.0f, 10.0f, Triangle());
    sys.Init(world);

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i) {
        sys.FixedUpdate(world, kDt);
    }

    auto& t = world.registry().get<TransformComponent>(tri);
    EXPECT_LT(t.position.y, 10.0f);
}

TEST_F(Physics2DPolygonColliderTest, 多边形与矩形碰撞) {
    // 地面
    MakeBoxBody(0.0f, -5.0f, 20.0f, 1.0f);
    // 五边形
    Entity pent = MakePolygonBody(0.0f, 5.0f, Pentagon());

    sys.Init(world);

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i) {
        sys.FixedUpdate(world, kDt);
    }

    // 五边形应被地面拦住
    auto& t = world.registry().get<TransformComponent>(pent);
    EXPECT_GT(t.position.y, -5.5f);
    EXPECT_LT(t.position.y, 5.0f);
}

TEST_F(Physics2DPolygonColliderTest, 射线检测命中多边形碰撞体) {
    MakePolygonBody(0.0f, 0.0f, Pentagon(), RigidBody2DType::Static);
    sys.Init(world);

    glm::vec2 start(0.0f, 5.0f);
    glm::vec2 end(0.0f, -5.0f);
    Entity hit_entity = entt::null;
    glm::vec2 hit_point, hit_normal;

    bool hit = sys.Raycast(start, end, hit_entity, hit_point, hit_normal);
    EXPECT_TRUE(hit);
    EXPECT_TRUE(hit_entity != entt::null);
}

TEST_F(Physics2DPolygonColliderTest, 多边形触发器不产生碰撞力) {
    MakeBoxBody(0.0f, -5.0f, 20.0f, 1.0f);

    Entity tri = MakePolygonBody(0.0f, 0.0f, Triangle());
    world.registry().get<PolygonCollider2DComponent>(tri).is_trigger = true;

    sys.Init(world);

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i) {
        sys.FixedUpdate(world, kDt);
    }

    // 触发器不产生碰撞力，应穿过地面继续下落
    auto& t = world.registry().get<TransformComponent>(tri);
    EXPECT_LT(t.position.y, -5.0f);
}

TEST_F(Physics2DPolygonColliderTest, 顶点数不足3时不创建fixture) {
    // 只有2个顶点
    std::vector<glm::vec2> bad_verts = {{0.0f, 0.0f}, {1.0f, 0.0f}};
    Entity e = MakePolygonBody(0.0f, 0.0f, bad_verts, RigidBody2DType::Static);
    sys.Init(world);

    auto& pc = world.registry().get<PolygonCollider2DComponent>(e);
    EXPECT_EQ(pc.runtime_fixture, nullptr);
}

TEST_F(Physics2DPolygonColliderTest, 实体销毁后系统正常) {
    Entity tri = MakePolygonBody(0.0f, 5.0f, Triangle());
    sys.Init(world);
    sys.FixedUpdate(world, 1.0f / 60.0f);

    world.DestroyEntity(tri);

    sys.FixedUpdate(world, 1.0f / 60.0f);
    SUCCEED();
}
