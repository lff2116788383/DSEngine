/**
 * @file particle_advanced_test.cpp
 * @brief 高级粒子系统单元测试：随机参数、生命周期曲线、重力、碰撞
 */

#include "catch/catch.hpp"
#include "modules/gameplay_2d/particle/particle_system.h"
#include "engine/ecs/components_2d.h"

// ─── Random Parameters ──────────────────────────────────────────────────
TEST_CASE("Particle - random params produce varied velocities", "[particle][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    emitter.emit_rate = 100.0f;
    emitter.max_particles = 20;
    emitter.use_random_params = true;
    emitter.velocity_min = glm::vec3(-5.0f, 1.0f, 0.0f);
    emitter.velocity_max = glm::vec3(5.0f, 10.0f, 0.0f);
    emitter.life_time_min = 1.0f;
    emitter.life_time_max = 3.0f;
    emitter.size_min = 0.5f;
    emitter.size_max = 2.0f;

    ParticleSystem system;
    system.Update(world, 0.5f);

    REQUIRE(emitter.particles.size() > 1);

    // Check that not all velocities are identical (randomness)
    bool has_different_velocity = false;
    const auto& first_vel = emitter.particles[0].velocity;
    for (size_t i = 1; i < emitter.particles.size(); ++i) {
        if (glm::length(emitter.particles[i].velocity - first_vel) > 0.01f) {
            has_different_velocity = true;
            break;
        }
    }
    REQUIRE(has_different_velocity);
}

TEST_CASE("Particle - random life times are within range", "[particle][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    emitter.emit_rate = 50.0f;
    emitter.max_particles = 10;
    emitter.use_random_params = true;
    emitter.life_time_min = 2.0f;
    emitter.life_time_max = 4.0f;

    ParticleSystem system;
    system.Update(world, 0.5f);

    for (const auto& p : emitter.particles) {
        REQUIRE(p.life_time >= 2.0f);
        REQUIRE(p.life_time <= 4.0f);
    }
}

// ─── Gravity ────────────────────────────────────────────────────────────
TEST_CASE("Particle - gravity accelerates particles downward", "[particle][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity).position = glm::vec3(0.0f, 10.0f, 0.0f);
    emitter.pending_burst = 1;
    emitter.emitting = false;
    emitter.start_life_time = 5.0f;
    emitter.gravity = glm::vec3(0.0f, -9.8f, 0.0f);

    ParticleSystem system;
    system.Update(world, 0.0f);  // spawn
    REQUIRE(emitter.particles.size() == 1);

    float initial_vy = emitter.particles[0].velocity.y;
    system.Update(world, 1.0f);  // 1 second of gravity

    // Velocity should have decreased by ~9.8
    REQUIRE(emitter.particles[0].velocity.y < initial_vy - 5.0f);
}

// ─── Ground Collision ───────────────────────────────────────────────────
TEST_CASE("Particle - ground collision bounces particle", "[particle][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity).position = glm::vec3(0.0f, 1.0f, 0.0f);
    emitter.pending_burst = 1;
    emitter.emitting = false;
    emitter.start_life_time = 10.0f;
    emitter.gravity = glm::vec3(0.0f, -20.0f, 0.0f);
    emitter.use_ground_collision = true;
    emitter.ground_y = 0.0f;
    emitter.collision_bounce = 0.5f;
    emitter.collision_friction = 0.0f;
    emitter.collision_life_loss = 0.0f;

    ParticleSystem system;
    system.Update(world, 0.0f);  // spawn at y=1
    system.Update(world, 1.0f);  // should fall and hit ground

    REQUIRE(emitter.particles.size() == 1);
    // Particle should be at or above ground
    REQUIRE(emitter.particles[0].position.y >= 0.0f);
    // Velocity should be positive (bounced)
    REQUIRE(emitter.particles[0].velocity.y > 0.0f);
}

TEST_CASE("Particle - collision life loss kills particle", "[particle][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity).position = glm::vec3(0.0f, 0.5f, 0.0f);
    emitter.pending_burst = 1;
    emitter.emitting = false;
    emitter.start_life_time = 1.0f;
    emitter.gravity = glm::vec3(0.0f, -50.0f, 0.0f);
    emitter.use_ground_collision = true;
    emitter.ground_y = 0.0f;
    emitter.collision_bounce = 0.0f;
    emitter.collision_life_loss = 100.0f;  // instant kill on collision

    ParticleSystem system;
    system.Update(world, 0.0f);  // spawn
    REQUIRE(emitter.particles.size() == 1);
    system.Update(world, 0.5f);  // fall and hit ground

    // Particle should be dead (removed next frame)
    system.Update(world, 0.01f);
    REQUIRE(emitter.particles.empty());
}

// ─── Size Curve ─────────────────────────────────────────────────────────
TEST_CASE("Particle - size curve shrinks particle over lifetime", "[particle][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    emitter.pending_burst = 1;
    emitter.emitting = false;
    emitter.start_life_time = 2.0f;
    emitter.start_size = 10.0f;
    emitter.use_size_curve = true;
    emitter.size_curve_end = 0.0f;

    ParticleSystem system;
    system.Update(world, 0.0f);  // spawn
    REQUIRE(emitter.particles.size() == 1);

    system.Update(world, 1.0f);  // half life
    // Size should be approximately half
    REQUIRE(emitter.particles[0].size < 7.0f);
    REQUIRE(emitter.particles[0].size > 3.0f);
}

// ─── Alpha Curve ────────────────────────────────────────────────────────
TEST_CASE("Particle - alpha curve fades particle", "[particle][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    emitter.pending_burst = 1;
    emitter.emitting = false;
    emitter.start_life_time = 2.0f;
    emitter.start_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    emitter.use_alpha_curve = true;
    emitter.alpha_curve_end = 0.0f;

    ParticleSystem system;
    system.Update(world, 0.0f);  // spawn
    system.Update(world, 1.0f);  // half life

    REQUIRE(emitter.particles[0].color.a < 0.7f);
    REQUIRE(emitter.particles[0].color.a > 0.3f);
}

// ─── Rotation ───────────────────────────────────────────────────────────
TEST_CASE("Particle - angular velocity rotates particle", "[particle][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    emitter.pending_burst = 1;
    emitter.emitting = false;
    emitter.start_life_time = 5.0f;
    emitter.use_random_params = true;
    emitter.angular_velocity_min = 3.14f;
    emitter.angular_velocity_max = 3.14f;  // fixed for test
    emitter.rotation_min = 0.0f;
    emitter.rotation_max = 0.0f;

    ParticleSystem system;
    system.Update(world, 0.0f);  // spawn
    REQUIRE(emitter.particles.size() == 1);
    REQUIRE(emitter.particles[0].rotation == Approx(0.0f).margin(0.01f));

    system.Update(world, 1.0f);
    // Should have rotated ~3.14 radians
    REQUIRE(emitter.particles[0].rotation == Approx(3.14f).margin(0.1f));
}
