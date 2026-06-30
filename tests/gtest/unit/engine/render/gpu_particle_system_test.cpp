/**
 * @file gpu_particle_system_test.cpp
 * @brief GPU Particle System 单元测试
 *
 * 测试粒子组件配置、发射逻辑和 Manager 的基本行为（不依赖真实 GPU）。
 */

#include <gtest/gtest.h>
#include "engine/render/particles/gpu_particle_system.h"

using namespace dse::render;

// ─── GpuParticleEmitterConfig 测试 ──────────────────────────────────────────

TEST(GpuParticleEmitterConfigTest, DefaultValues) {
    GpuParticleEmitterConfig config;

    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.max_particles, 10000u);
    EXPECT_FLOAT_EQ(config.emission_rate, 500.0f);
    EXPECT_EQ(config.shape, EmitterShape::Point);
    EXPECT_FLOAT_EQ(config.shape_radius, 1.0f);
    EXPECT_FLOAT_EQ(config.cone_angle, 45.0f);
}

TEST(GpuParticleEmitterConfigTest, LifeRange) {
    GpuParticleEmitterConfig config;
    EXPECT_LE(config.life_min, config.life_max);
    EXPECT_GT(config.life_min, 0.0f);
}

TEST(GpuParticleEmitterConfigTest, SpeedRange) {
    GpuParticleEmitterConfig config;
    EXPECT_LE(config.speed_min, config.speed_max);
    EXPECT_GT(config.speed_min, 0.0f);
}

TEST(GpuParticleEmitterConfigTest, SizeOverLife) {
    GpuParticleEmitterConfig config;
    // Start size should be larger than end size for typical fade-out
    EXPECT_GT(config.size_start, config.size_end);
}

TEST(GpuParticleEmitterConfigTest, ColorOverLife) {
    GpuParticleEmitterConfig config;
    // Start color should have full alpha
    EXPECT_FLOAT_EQ(config.color_start.a, 1.0f);
    // End color should have zero alpha (fade out)
    EXPECT_FLOAT_EQ(config.color_end.a, 0.0f);
}

TEST(GpuParticleEmitterConfigTest, CollisionDefaults) {
    GpuParticleEmitterConfig config;
    EXPECT_FALSE(config.collision_enabled);
    EXPECT_FLOAT_EQ(config.collision_plane_y, 0.0f);
    EXPECT_GT(config.collision_bounce, 0.0f);
    EXPECT_LE(config.collision_bounce, 1.0f);
    EXPECT_GT(config.collision_friction, 0.0f);
    EXPECT_LE(config.collision_friction, 1.0f);
}

TEST(GpuParticleEmitterConfigTest, EmitterShapeEnum) {
    // Verify all shape enum values are distinct
    EXPECT_NE(static_cast<int>(EmitterShape::Point), static_cast<int>(EmitterShape::Sphere));
    EXPECT_NE(static_cast<int>(EmitterShape::Sphere), static_cast<int>(EmitterShape::Cone));
    EXPECT_NE(static_cast<int>(EmitterShape::Cone), static_cast<int>(EmitterShape::Ring));
    EXPECT_NE(static_cast<int>(EmitterShape::Ring), static_cast<int>(EmitterShape::Box));
}

// ─── GpuParticleComponent 测试 ──────────────────────────────────────────────

TEST(GpuParticleComponentTest, DefaultState) {
    GpuParticleComponent comp;

    EXPECT_EQ(comp.particle_buffer_a, 0u);
    EXPECT_EQ(comp.particle_buffer_b, 0u);
    EXPECT_EQ(comp.counter_buffer, 0u);
    EXPECT_EQ(comp.indirect_buffer, 0u);
    EXPECT_EQ(comp.texture_handle, 0u);
    EXPECT_TRUE(comp.ping);
    EXPECT_FLOAT_EQ(comp.emit_accumulator, 0.0f);
    EXPECT_FALSE(comp.initialized);
}

TEST(GpuParticleComponentTest, PingPongToggle) {
    GpuParticleComponent comp;
    EXPECT_TRUE(comp.ping);
    comp.ping = !comp.ping;
    EXPECT_FALSE(comp.ping);
    comp.ping = !comp.ping;
    EXPECT_TRUE(comp.ping);
}

TEST(GpuParticleComponentTest, EmitAccumulator) {
    GpuParticleComponent comp;
    float dt = 0.016f; // ~60fps
    float rate = comp.config.emission_rate;

    comp.emit_accumulator += rate * dt;
    EXPECT_GT(comp.emit_accumulator, 0.0f);

    // After one second accumulation should equal emission_rate
    comp.emit_accumulator = 0.0f;
    comp.emit_accumulator += rate * 1.0f;
    EXPECT_FLOAT_EQ(comp.emit_accumulator, rate);
}

// ─── GpuParticleManager 测试（无 GPU） ──────────────────────────────────────

TEST(GpuParticleManagerTest, InitWithoutRhi_ReturnsFalse) {
    GpuParticleManager manager;
    // Passing nullptr should fail gracefully
    bool result = manager.Init(nullptr);
    EXPECT_FALSE(result);
}

TEST(GpuParticleManagerTest, DefaultConstruction) {
    GpuParticleManager manager;
    // Should be safe to construct and destroy without Init
}

TEST(GpuParticleManagerTest, ConfigCopy) {
    GpuParticleEmitterConfig config;
    config.max_particles = 50000;
    config.emission_rate = 1000.0f;
    config.shape = EmitterShape::Sphere;
    config.shape_radius = 5.0f;
    config.gravity = glm::vec3(0.0f, -20.0f, 0.0f);

    GpuParticleComponent comp;
    comp.config = config;

    EXPECT_EQ(comp.config.max_particles, 50000u);
    EXPECT_FLOAT_EQ(comp.config.emission_rate, 1000.0f);
    EXPECT_EQ(comp.config.shape, EmitterShape::Sphere);
    EXPECT_FLOAT_EQ(comp.config.shape_radius, 5.0f);
    EXPECT_FLOAT_EQ(comp.config.gravity.y, -20.0f);
}

TEST(GpuParticleManagerTest, ForceFieldConfig) {
    GpuParticleEmitterConfig config;
    config.turbulence = 2.5f;
    config.vortex_strength = 1.0f;
    config.wind = glm::vec3(3.0f, 0.0f, -1.0f);

    GpuParticleComponent comp;
    comp.config = config;

    EXPECT_FLOAT_EQ(comp.config.turbulence, 2.5f);
    EXPECT_FLOAT_EQ(comp.config.vortex_strength, 1.0f);
    EXPECT_FLOAT_EQ(comp.config.wind.x, 3.0f);
    EXPECT_FLOAT_EQ(comp.config.wind.z, -1.0f);
}
