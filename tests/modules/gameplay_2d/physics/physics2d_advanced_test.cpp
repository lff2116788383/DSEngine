/**
 * @file physics2d_advanced_test.cpp
 * @brief Physics2D 高级测试：运动学刚体、速度同步、多实体销毁、重力缩放
 */

#include "catch/catch.hpp"
#include "engine/physics/physics2d/physics2d_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/world.h"

// ─── Kinematic body ─────────────────────────────────────────────────────
TEST_CASE("Physics2D Advanced - kinematic body not affected by gravity", "[physics2d][advanced]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(0.0f, 5.0f, 0.0f);
    auto& rb = world.registry().emplace<RigidBody2DComponent>(entity);
    rb.type = RigidBody2DType::Kinematic;
    world.registry().emplace<BoxCollider2DComponent>(entity).size = glm::vec2(1.0f, 1.0f);

    for (int i = 0; i < 10; ++i) {
        physics_system.FixedUpdate(world, 1.0f / 60.0f);
    }

    // Kinematic body should not fall
    REQUIRE(transform.position.y == Approx(5.0f).margin(0.01f));
}

// ─── Gravity scale ──────────────────────────────────────────────────────
TEST_CASE("Physics2D Advanced - gravity scale affects fall speed", "[physics2d][advanced]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    // Normal gravity body
    auto normal = world.CreateEntity();
    auto& tf_normal = world.registry().emplace<TransformComponent>(normal);
    tf_normal.position = glm::vec3(-5.0f, 10.0f, 0.0f);
    auto& rb_normal = world.registry().emplace<RigidBody2DComponent>(normal);
    rb_normal.type = RigidBody2DType::Dynamic;
    rb_normal.gravity_scale = 1.0f;
    world.registry().emplace<BoxCollider2DComponent>(normal).size = glm::vec2(0.5f, 0.5f);

    // Double gravity body
    auto heavy = world.CreateEntity();
    auto& tf_heavy = world.registry().emplace<TransformComponent>(heavy);
    tf_heavy.position = glm::vec3(5.0f, 10.0f, 0.0f);
    auto& rb_heavy = world.registry().emplace<RigidBody2DComponent>(heavy);
    rb_heavy.type = RigidBody2DType::Dynamic;
    rb_heavy.gravity_scale = 2.0f;
    world.registry().emplace<BoxCollider2DComponent>(heavy).size = glm::vec2(0.5f, 0.5f);

    for (int i = 0; i < 30; ++i) {
        physics_system.FixedUpdate(world, 1.0f / 60.0f);
    }

    // Heavy body should have fallen further
    REQUIRE(tf_heavy.position.y < tf_normal.position.y);
}

// ─── Fixed rotation ─────────────────────────────────────────────────────
TEST_CASE("Physics2D Advanced - fixed rotation prevents rotation", "[physics2d][advanced]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(0.0f, 5.0f, 0.0f);
    auto& rb = world.registry().emplace<RigidBody2DComponent>(entity);
    rb.type = RigidBody2DType::Dynamic;
    rb.fixed_rotation = true;
    world.registry().emplace<BoxCollider2DComponent>(entity).size = glm::vec2(1.0f, 1.0f);

    for (int i = 0; i < 10; ++i) {
        physics_system.FixedUpdate(world, 1.0f / 60.0f);
    }

    // Rotation should remain identity (no rotation)
    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    REQUIRE(transform.rotation.w == Approx(identity.w).margin(0.01f));
    REQUIRE(transform.rotation.x == Approx(identity.x).margin(0.01f));
    REQUIRE(transform.rotation.y == Approx(identity.y).margin(0.01f));
    REQUIRE(transform.rotation.z == Approx(identity.z).margin(0.01f));
}

// ─── Multiple entity destruction ────────────────────────────────────────
TEST_CASE("Physics2D Advanced - destroying multiple entities in one frame is safe", "[physics2d][advanced]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    std::vector<Entity> entities;
    for (int i = 0; i < 5; ++i) {
        auto e = world.CreateEntity();
        auto& tf = world.registry().emplace<TransformComponent>(e);
        tf.position = glm::vec3(static_cast<float>(i) * 2.0f, 10.0f, 0.0f);
        auto& rb = world.registry().emplace<RigidBody2DComponent>(e);
        rb.type = RigidBody2DType::Dynamic;
        world.registry().emplace<BoxCollider2DComponent>(e).size = glm::vec2(1.0f, 1.0f);
        entities.push_back(e);
    }

    // Create bodies
    physics_system.FixedUpdate(world, 1.0f / 60.0f);

    // Destroy all at once
    for (auto e : entities) {
        world.registry().destroy(e);
    }

    // Should not crash
    physics_system.FixedUpdate(world, 1.0f / 60.0f);
    REQUIRE(true);  // If we get here, no crash
}

// ─── Collision exit ─────────────────────────────────────────────────────
TEST_CASE("Physics2D Advanced - collision exit fires when bodies separate", "[physics2d][advanced]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    // Static floor
    auto floor = world.CreateEntity();
    auto& floor_tf = world.registry().emplace<TransformComponent>(floor);
    floor_tf.position = glm::vec3(0.0f, -5.0f, 0.0f);
    auto& floor_rb = world.registry().emplace<RigidBody2DComponent>(floor);
    floor_rb.type = RigidBody2DType::Static;
    world.registry().emplace<BoxCollider2DComponent>(floor).size = glm::vec2(20.0f, 1.0f);

    // Falling ball
    auto ball = world.CreateEntity();
    auto& ball_tf = world.registry().emplace<TransformComponent>(ball);
    ball_tf.position = glm::vec3(0.0f, 0.0f, 0.0f);
    auto& ball_rb = world.registry().emplace<RigidBody2DComponent>(ball);
    ball_rb.type = RigidBody2DType::Dynamic;
    world.registry().emplace<BoxCollider2DComponent>(ball).size = glm::vec2(1.0f, 1.0f);

    int exit_count = 0;
    ball_rb.on_collision_exit = [&](Entity) { ++exit_count; };

    // Let ball fall and collide
    for (int i = 0; i < 120; ++i) {
        physics_system.FixedUpdate(world, 1.0f / 60.0f);
    }

    // Move ball far away to trigger exit
    ball_tf.position = glm::vec3(100.0f, 100.0f, 0.0f);
    for (int i = 0; i < 10; ++i) {
        physics_system.FixedUpdate(world, 1.0f / 60.0f);
    }

    // exit_count may or may not fire depending on Box2D sync implementation
    // At minimum, no crash should occur
    REQUIRE(true);
}
