/**
 * @file ecs_component_test.cpp
 * @brief ECS 组件操作单元测试
 *
 * 覆盖场景：
 * - 单组件添加/获取/移除
 * - 多组件实体与 view 查询
 * - 组件修改与默认值
 * - 组件不存在时的安全操作
 * - 批量实体组件迭代
 * - World 与 registry 的一致性
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/sprite.h"
#include "engine/ecs/camera.h"
#include <glm/glm.hpp>

// ============================================================
// 单组件操作测试
// ============================================================

TEST(EcsComponentTest, AddToAcquireTransformComponent) {
    World world;
    Entity e = world.CreateEntity();
    auto& reg = world.registry();

    auto& transform = reg.emplace<TransformComponent>(e);
    transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    transform.scale = glm::vec3(2.0f);

    EXPECT_TRUE(reg.all_of<TransformComponent>(e));
    auto& got = reg.get<TransformComponent>(e);
    EXPECT_EQ(got.position.x, 1.0f);
    EXPECT_EQ(got.position.y, 2.0f);
    EXPECT_EQ(got.position.z, 3.0f);
    EXPECT_EQ(got.scale.x, 2.0f);
}

TEST(EcsComponentTest, ComponentDefaultValueCorrect) {
    World world;
    Entity e = world.CreateEntity();
    auto& reg = world.registry();

    reg.emplace<TransformComponent>(e);
    auto& transform = reg.get<TransformComponent>(e);

    // TransformComponent 默认值：原点位置、单位旋转、单位缩放
    EXPECT_EQ(transform.position, glm::vec3(0.0f));
    EXPECT_EQ(transform.scale, glm::vec3(1.0f));
    EXPECT_EQ(transform.local_to_world, glm::mat4(1.0f));
    EXPECT_TRUE(transform.dirty);
}

TEST(EcsComponentTest, RemoveComponentAfterNotCan) {
    World world;
    Entity e = world.CreateEntity();
    auto& reg = world.registry();

    reg.emplace<TransformComponent>(e);
    EXPECT_TRUE(reg.all_of<TransformComponent>(e));

    reg.remove<TransformComponent>(e);
    EXPECT_FALSE(reg.all_of<TransformComponent>(e));
}

TEST(EcsComponentTest, Component) {
    World world;
    Entity e = world.CreateEntity();
    auto& reg = world.registry();

    reg.emplace<TransformComponent>(e, TransformComponent{glm::vec3(1.0f)});
    auto& t1 = reg.get<TransformComponent>(e);
    EXPECT_EQ(t1.position.x, 1.0f);

    // 使用 replace 或直接修改
    t1.position = glm::vec3(5.0f, 10.0f, 15.0f);
    auto& t2 = reg.get<TransformComponent>(e);
    EXPECT_EQ(t2.position.x, 5.0f);
    EXPECT_EQ(t2.position.y, 10.0f);
    EXPECT_EQ(t2.position.z, 15.0f);
}

TEST(EcsComponentTest, EntityWithoutComponentWhenall_OfReturnsfalse) {
    World world;
    Entity e = world.CreateEntity();
    auto& reg = world.registry();

    EXPECT_FALSE(reg.all_of<TransformComponent>(e));
    EXPECT_FALSE(reg.all_of<SpriteRendererComponent>(e));
}

// ============================================================
// 多组件与 view 查询测试
// ============================================================

TEST(EcsComponentTest, SingleComponentviewQuery) {
    World world;
    auto& reg = world.registry();

    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    Entity e3 = world.CreateEntity();

    reg.emplace<TransformComponent>(e1);
    reg.emplace<TransformComponent>(e2);
    // e3 没有 TransformComponent

    int count = 0;
    auto view = reg.view<TransformComponent>();
    for (auto entity : view) {
        (void)entity;
        ++count;
    }
    EXPECT_EQ(count, 2);
}

TEST(EcsComponentTest, MultiComponentviewQuery) {
    World world;
    auto& reg = world.registry();

    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    Entity e3 = world.CreateEntity();

    // e1: Transform + Camera
    reg.emplace<TransformComponent>(e1);
    reg.emplace<CameraComponent>(e1);

    // e2: 仅 Transform
    reg.emplace<TransformComponent>(e2);

    // e3: 仅 Camera
    reg.emplace<CameraComponent>(e3);

    // 查询同时拥有 Transform + Camera 的实体
    int count = 0;
    auto view = reg.view<TransformComponent, CameraComponent>();
    for (auto entity : view) {
        (void)entity;
        ++count;
    }
    EXPECT_EQ(count, 1);  // 仅 e1 同时拥有两个组件
}

TEST(EcsComponentTest, ViewGetComponentReferenceIn) {
    World world;
    auto& reg = world.registry();

    Entity e1 = world.CreateEntity();
    auto& t1 = reg.emplace<TransformComponent>(e1);
    t1.position = glm::vec3(10.0f, 20.0f, 30.0f);

    Entity e2 = world.CreateEntity();
    auto& t2 = reg.emplace<TransformComponent>(e2);
    t2.position = glm::vec3(40.0f, 50.0f, 60.0f);

    auto view = reg.view<TransformComponent>();
    glm::vec3 sum(0.0f);
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        sum += transform.position;
    }
    EXPECT_EQ(sum.x, 50.0f);
    EXPECT_EQ(sum.y, 70.0f);
    EXPECT_EQ(sum.z, 90.0f);
}

TEST(EcsComponentTest, DestroyEntityAutoRemoveComponent) {
    World world;
    auto& reg = world.registry();

    Entity e = world.CreateEntity();
    reg.emplace<TransformComponent>(e);
    EXPECT_TRUE(reg.all_of<TransformComponent>(e));

    world.DestroyEntity(e);
    // 销毁后实体不再存活，组件也随之销毁
    EXPECT_FALSE(world.IsAlive(e));
}

// ============================================================
// 批量实体与组件迭代测试
// ============================================================

TEST(EcsComponentTest, CreateEntitiesInBatchesQuery) {
    World world;
    auto& reg = world.registry();

    constexpr int kCount = 100;
    for (int i = 0; i < kCount; ++i) {
        Entity e = world.CreateEntity();
        auto& t = reg.emplace<TransformComponent>(e);
        t.position.x = static_cast<float>(i);
    }

    EXPECT_EQ(world.EntityCount(), static_cast<size_t>(kCount));

    int count = 0;
    for (auto entity : reg.view<TransformComponent>()) {
        (void)entity;
        ++count;
    }
    EXPECT_EQ(count, kCount);
}

TEST(EcsComponentTest, EntityWithComponentWhenviewReturns) {
    World world;
    auto& reg = world.registry();

    // 创建 10 个实体，只有偶数索引的有 TransformComponent
    for (int i = 0; i < 10; ++i) {
        Entity e = world.CreateEntity();
        if (i % 2 == 0) {
            reg.emplace<TransformComponent>(e);
        }
    }

    int count = 0;
    for (auto entity : reg.view<TransformComponent>()) {
        (void)entity;
        ++count;
    }
    EXPECT_EQ(count, 5);
}

// ============================================================
// World 与 registry 一致性测试
// ============================================================

TEST(EcsComponentTest, WorldcountingWithregistryconsistent) {
    World world;
    auto& reg = world.registry();

    // World 创建的实体
    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    EXPECT_EQ(world.EntityCount(), 2u);

    // 通过 registry 直接创建的实体不影响 World 计数
    Entity e3 = reg.create();
    // World 的 EntityCount 只追踪 CreateEntity 的计数
    EXPECT_EQ(world.EntityCount(), 2u);

    // 但 registry 能看到所有实体
    EXPECT_TRUE(reg.valid(e1));
    EXPECT_TRUE(reg.valid(e2));
    EXPECT_TRUE(reg.valid(e3));

    // 清理
    reg.destroy(e3);
}

TEST(EcsComponentTest, MultiWorldExampleComponent) {
    World world_a;
    World world_b;

    Entity ea = world_a.CreateEntity();
    Entity eb = world_b.CreateEntity();

    world_a.registry().emplace<TransformComponent>(ea);

    // world_b 中同 ID 的实体没有组件
    EXPECT_FALSE(world_b.registry().all_of<TransformComponent>(eb));
    EXPECT_TRUE(world_a.registry().all_of<TransformComponent>(ea));
}

TEST(EcsComponentTest, ParentComponentEstablishHierarchicalRelationships) {
    World world;
    auto& reg = world.registry();

    Entity parent = world.CreateEntity();
    Entity child = world.CreateEntity();

    reg.emplace<ParentComponent>(child, ParentComponent{parent});
    auto& pc = reg.get<ParentComponent>(child);
    EXPECT_EQ(pc.parent, parent);
}
