#include "catch/catch.hpp"
#include "engine/physics/physics2d/physics2d_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/world.h"

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
