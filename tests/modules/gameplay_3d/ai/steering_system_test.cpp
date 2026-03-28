#include "catch/catch.hpp"
#include "modules/gameplay_3d/ai/steering_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

using namespace dse;
using namespace dse::gameplay3d;

// 正向测试：Seek 行为使实体向目标移动
TEST_CASE("Given_SteeringComponent_When_SeekEnabled_Then_MovesTowardsTarget", "[engine][unit][steering]") {
    World world;
    auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(0.0f);
    
    auto& steering = world.registry().emplace<SteeringComponent>(entity);
    steering.enabled = true;
    steering.seek_enabled = true;
    steering.seek_target = glm::vec3(10.0f, 0.0f, 0.0f);
    steering.max_velocity = 5.0f;
    steering.max_force = 10.0f;
    steering.mass = 1.0f;

    SteeringSystem system;
    system.Update(world, 1.0f);

    // 应该有了向X正方向的速度和位移
    REQUIRE(transform.position.x > 0.0f);
    REQUIRE(steering.velocity.x > 0.0f);
}

// 正向测试：Flee 行为使实体远离目标
TEST_CASE("Given_SteeringComponent_When_FleeEnabled_Then_MovesAwayFromTarget", "[engine][unit][steering]") {
    World world;
    auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(2.0f, 0.0f, 0.0f);
    
    auto& steering = world.registry().emplace<SteeringComponent>(entity);
    steering.enabled = true;
    steering.flee_enabled = true;
    steering.flee_target = glm::vec3(0.0f, 0.0f, 0.0f);
    steering.max_velocity = 5.0f;
    steering.max_force = 10.0f;
    steering.mass = 1.0f;

    SteeringSystem system;
    system.Update(world, 1.0f);

    // 应该远离原点向X正方向移动
    REQUIRE(transform.position.x > 2.0f);
    REQUIRE(steering.velocity.x > 0.0f);
}

// 边界测试：Arrive 行为在接近目标时应减速
TEST_CASE("Given_SteeringComponent_When_ArriveEnabled_Then_DeceleratesNearTarget", "[engine][unit][steering]") {
    World world;
    auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(0.0f);
    
    auto& steering = world.registry().emplace<SteeringComponent>(entity);
    steering.enabled = true;
    steering.arrive_enabled = true;
    steering.arrive_target = glm::vec3(2.0f, 0.0f, 0.0f); // 在减速半径内
    steering.arrive_deceleration_radius = 5.0f;
    steering.max_velocity = 10.0f;
    steering.max_force = 10.0f;
    steering.mass = 1.0f;

    SteeringSystem system;
    system.Update(world, 1.0f);

    // 速度应受到减速半径的约束，不会达到最大速度
    REQUIRE(glm::length(steering.velocity) < steering.max_velocity);
}

// 反向测试：禁用组件时不应更新位置
TEST_CASE("Given_SteeringComponent_When_Disabled_Then_PositionRemainsUnchanged", "[engine][unit][steering]") {
    World world;
    auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(0.0f);
    
    auto& steering = world.registry().emplace<SteeringComponent>(entity);
    steering.enabled = false; // 组件已禁用
    steering.seek_enabled = true;
    steering.seek_target = glm::vec3(10.0f, 0.0f, 0.0f);

    SteeringSystem system;
    system.Update(world, 1.0f);

    REQUIRE(transform.position.x == 0.0f);
    REQUIRE(steering.velocity.x == 0.0f);
}
