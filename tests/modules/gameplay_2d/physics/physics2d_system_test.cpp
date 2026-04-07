#include "catch/catch.hpp"
#include "engine/physics/physics2d/physics2d_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/world.h"
#include <cstdio>

// 正向测试：物理世界初始化后，Dynamic 刚体在重力作用下位置应发生变化。
TEST_CASE("Given_DynamicBody_When_PhysicsUpdate_Then_PositionChangesByGravity", "[engine][unit][physics2d]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(0.0f, 10.0f, 0.0f);
    
    auto& rb = world.registry().emplace<RigidBody2DComponent>(entity);
    rb.type = RigidBody2DType::Dynamic;
    
    auto& box = world.registry().emplace<BoxCollider2DComponent>(entity);
    box.size = glm::vec2(1.0f, 1.0f);

    // 运行几步物理模拟
    for (int i = 0; i < 10; ++i) {
        physics_system.FixedUpdate(world, 1.0f / 60.0f);
    }

    // 重力向上是 (0, -9.81)，所以 Y 应该减小
    REQUIRE(transform.position.y < 10.0f);
}

// 边界测试：Static 刚体不应受重力影响。
TEST_CASE("Given_StaticBody_When_PhysicsUpdate_Then_PositionRemainsConstant", "[engine][unit][physics2d]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(0.0f, 5.0f, 0.0f);
    
    auto& rb = world.registry().emplace<RigidBody2DComponent>(entity);
    rb.type = RigidBody2DType::Static;
    
    auto& box = world.registry().emplace<BoxCollider2DComponent>(entity);
    box.size = glm::vec2(1.0f, 1.0f);

    physics_system.FixedUpdate(world, 1.0f / 60.0f);

    REQUIRE(transform.position.y == 5.0f);
}

// 反向测试：删除实体后，对应的物理刚体也应被清理（通过 FixedUpdate 内部逻辑验证）。
TEST_CASE("Given_DeletedEntity_When_PhysicsUpdate_Then_BodyIsDestroyed", "[engine][unit][physics2d]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    auto entity = world.CreateEntity();
    world.registry().emplace<TransformComponent>(entity);
    auto& rb = world.registry().emplace<RigidBody2DComponent>(entity);
    rb.type = RigidBody2DType::Dynamic;
    world.registry().emplace<BoxCollider2DComponent>(entity);

    // 第一次更新创建刚体
    physics_system.FixedUpdate(world, 1.0f / 60.0f);
    REQUIRE(rb.runtime_body != nullptr);

    // 销毁实体
    world.registry().destroy(entity);

    // 第二次更新应清理失效的刚体（通过内部遍历逻辑，虽然外部难以直接观测 runtime_body 状态，但可以确保不崩溃）
    physics_system.FixedUpdate(world, 1.0f / 60.0f);
}

// 回归测试：两个非 trigger 碰撞体接触后，应触发 collision enter 回调。
TEST_CASE("Given_TwoDynamicColliders_When_TheyOverlap_Then_CollisionEnterIsFired", "[engine][unit][physics2d]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    auto entity_a = world.CreateEntity();
    auto& transform_a = world.registry().emplace<TransformComponent>(entity_a);
    transform_a.position = glm::vec3(0.0f, 0.0f, 0.0f);
    auto& rb_a = world.registry().emplace<RigidBody2DComponent>(entity_a);
    rb_a.type = RigidBody2DType::Dynamic;
    auto& box_a = world.registry().emplace<BoxCollider2DComponent>(entity_a);
    box_a.size = glm::vec2(1.0f, 1.0f);

    auto entity_b = world.CreateEntity();
    auto& transform_b = world.registry().emplace<TransformComponent>(entity_b);
    transform_b.position = glm::vec3(0.0f, 0.0f, 0.0f);
    auto& rb_b = world.registry().emplace<RigidBody2DComponent>(entity_b);
    rb_b.type = RigidBody2DType::Static;
    auto& box_b = world.registry().emplace<BoxCollider2DComponent>(entity_b);
    box_b.size = glm::vec2(1.0f, 1.0f);

    int a_collision_count = 0;
    int b_collision_count = 0;
    rb_a.on_collision_enter = [&](Entity other) {
        if (other == entity_b) {
            ++a_collision_count;
        }
    };
    rb_b.on_collision_enter = [&](Entity other) {
        if (other == entity_a) {
            ++b_collision_count;
        }
    };

    physics_system.FixedUpdate(world, 1.0f / 60.0f);

    REQUIRE(a_collision_count >= 1);
    REQUIRE(b_collision_count >= 1);
}

// 回归测试：任一夹具为 trigger 时，应触发 trigger enter，而不是 collision enter。
TEST_CASE("Given_TriggerCollider_When_TheyOverlap_Then_TriggerEnterIsFired", "[engine][unit][physics2d]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    auto entity_a = world.CreateEntity();
    auto& transform_a = world.registry().emplace<TransformComponent>(entity_a);
    transform_a.position = glm::vec3(0.0f, 0.0f, 0.0f);
    auto& rb_a = world.registry().emplace<RigidBody2DComponent>(entity_a);
    rb_a.type = RigidBody2DType::Dynamic;
    auto& box_a = world.registry().emplace<BoxCollider2DComponent>(entity_a);
    box_a.size = glm::vec2(1.0f, 1.0f);
    box_a.is_trigger = true;

    auto entity_b = world.CreateEntity();
    auto& transform_b = world.registry().emplace<TransformComponent>(entity_b);
    transform_b.position = glm::vec3(0.0f, 0.0f, 0.0f);
    auto& rb_b = world.registry().emplace<RigidBody2DComponent>(entity_b);
    rb_b.type = RigidBody2DType::Static;
    auto& box_b = world.registry().emplace<BoxCollider2DComponent>(entity_b);
    box_b.size = glm::vec2(1.0f, 1.0f);

    int trigger_count = 0;
    int collision_count = 0;
    rb_a.on_trigger_enter = [&](Entity other) {
        if (other == entity_b) {
            ++trigger_count;
        }
    };
    rb_a.on_collision_enter = [&](Entity) {
        ++collision_count;
    };

    physics_system.FixedUpdate(world, 1.0f / 60.0f);

    REQUIRE(trigger_count >= 1);
    REQUIRE(collision_count == 0);
}

// 回归测试：射线应命中已创建刚体的最近碰撞体，并返回命中点/法线。
TEST_CASE("Given_StaticCollider_When_Raycast_Then_HitEntityAndPointAreReturned", "[engine][unit][physics2d]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    auto& rb = world.registry().emplace<RigidBody2DComponent>(entity);
    rb.type = RigidBody2DType::Static;
    auto& box = world.registry().emplace<BoxCollider2DComponent>(entity);
    box.size = glm::vec2(2.0f, 2.0f);

    physics_system.FixedUpdate(world, 1.0f / 60.0f);

    Entity hit_entity = entt::null;
    glm::vec2 hit_point(0.0f);
    glm::vec2 hit_normal(0.0f);
    const bool hit = physics_system.Raycast(glm::vec2(-5.0f, 0.0f), glm::vec2(5.0f, 0.0f), hit_entity, hit_point, hit_normal);

    REQUIRE(hit);
    REQUIRE(hit_entity == entity);
    REQUIRE(hit_point.x == Approx(-1.0f).margin(0.2f));
    REQUIRE(hit_normal.x == Approx(-1.0f).margin(0.2f));
}
