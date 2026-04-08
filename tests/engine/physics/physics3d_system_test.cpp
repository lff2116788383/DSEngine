#include "catch/catch.hpp"
#include "engine/physics/physics3d/physics3d_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_2d.h"

using dse::RigidBody3DType;
using dse::RigidBody3DComponent;
using dse::BoxCollider3DComponent;
using dse::physics3d::Physics3DSystem;
using dse::physics3d::RaycastResult;

TEST_CASE("Given_DefaultRaycastResult_When_Created_Then_FieldsAreZeroInitialized", "[engine][unit][physics3d]") {
    RaycastResult result;

    REQUIRE_FALSE(result.hit);
    REQUIRE(result.entity == static_cast<entt::entity>(entt::null));
    REQUIRE(result.hit_point == glm::vec3(0.0f));
    REQUIRE(result.hit_normal == glm::vec3(0.0f));
    REQUIRE(result.distance == Approx(0.0f));
}

TEST_CASE("Given_Physics3DSystemWithoutInit_When_ApisCalled_Then_NoCrashAndDefaultsReturned", "[engine][unit][physics3d]") {
    World world;
    Physics3DSystem system;

    REQUIRE_NOTHROW(system.FixedUpdate(world, 1.0f / 60.0f));
    REQUIRE_NOTHROW(system.Shutdown());

    const RaycastResult result = system.Raycast(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), 10.0f);
    REQUIRE_FALSE(result.hit);
    REQUIRE(result.entity == static_cast<entt::entity>(entt::null));
}

TEST_CASE("Given_Physics3DComponents_When_Configured_Then_ValuesPersistBeforeRuntimeInit", "[engine][unit][physics3d]") {
    World world;
    auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(1.0f, 2.0f, 3.0f);

    auto& rigidbody = world.registry().emplace<RigidBody3DComponent>(entity);
    rigidbody.type = RigidBody3DType::Kinematic;
    rigidbody.mass = 3.5f;
    rigidbody.drag = 0.25f;
    rigidbody.angular_drag = 0.75f;

    auto& collider = world.registry().emplace<BoxCollider3DComponent>(entity);
    collider.size = glm::vec3(2.0f, 4.0f, 6.0f);
    collider.center = glm::vec3(1.0f, 0.5f, -1.0f);
    collider.is_trigger = true;

    REQUIRE(transform.position == glm::vec3(1.0f, 2.0f, 3.0f));
    REQUIRE(rigidbody.type == RigidBody3DType::Kinematic);
    REQUIRE(rigidbody.mass == Approx(3.5f));
    REQUIRE(rigidbody.drag == Approx(0.25f));
    REQUIRE(rigidbody.angular_drag == Approx(0.75f));
    REQUIRE(collider.size == glm::vec3(2.0f, 4.0f, 6.0f));
    REQUIRE(collider.center == glm::vec3(1.0f, 0.5f, -1.0f));
    REQUIRE(collider.is_trigger);
}
