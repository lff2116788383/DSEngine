/**
 * @file components_3d_fluid_test.cpp
 * @brief 流体模拟组件默认值与字段修改单元测试
 */

#include <gtest/gtest.h>
#include "engine/ecs/components_3d_fluid.h"

using namespace dse;

// ─── FluidParticle ─────────────────────────────────────────────────────

TEST(FluidParticleTest, 默认值) {
    FluidParticle p;
    EXPECT_FLOAT_EQ(p.position.x, 0.0f);
    EXPECT_FLOAT_EQ(p.velocity.x, 0.0f);
    EXPECT_FLOAT_EQ(p.density, 0.0f);
    EXPECT_FLOAT_EQ(p.pressure, 0.0f);
    EXPECT_FLOAT_EQ(p.life, 0.0f);
}

// ─── FluidEmitterShape ─────────────────────────────────────────────────

TEST(FluidEmitterShapeTest, 枚举值) {
    EXPECT_EQ(static_cast<int>(FluidEmitterShape::Point), 0);
    EXPECT_EQ(static_cast<int>(FluidEmitterShape::Sphere), 1);
    EXPECT_EQ(static_cast<int>(FluidEmitterShape::Box), 2);
}

// ─── FluidEmitterComponent ─────────────────────────────────────────────

TEST(FluidEmitterComponentTest, 默认发射配置) {
    FluidEmitterComponent fluid;
    EXPECT_TRUE(fluid.enabled);
    EXPECT_EQ(fluid.shape, FluidEmitterShape::Point);
    EXPECT_FLOAT_EQ(fluid.emission_rate, 500.0f);
    EXPECT_FLOAT_EQ(fluid.particle_lifetime, 3.0f);
    EXPECT_FLOAT_EQ(fluid.particle_radius, 0.05f);
    EXPECT_FLOAT_EQ(fluid.emit_direction.y, -1.0f);
    EXPECT_FLOAT_EQ(fluid.emit_speed, 2.0f);
    EXPECT_FLOAT_EQ(fluid.emit_spread, 0.3f);
}

TEST(FluidEmitterComponentTest, 默认发射器形状参数) {
    FluidEmitterComponent fluid;
    EXPECT_FLOAT_EQ(fluid.sphere_radius, 0.5f);
    EXPECT_FLOAT_EQ(fluid.box_half_extents.x, 0.5f);
    EXPECT_FLOAT_EQ(fluid.box_half_extents.y, 0.1f);
    EXPECT_FLOAT_EQ(fluid.box_half_extents.z, 0.5f);
}

TEST(FluidEmitterComponentTest, 默认物理参数) {
    FluidEmitterComponent fluid;
    EXPECT_FLOAT_EQ(fluid.gravity.y, -9.81f);
    EXPECT_FLOAT_EQ(fluid.viscosity, 0.01f);
    EXPECT_FLOAT_EQ(fluid.surface_tension, 0.05f);
    EXPECT_FLOAT_EQ(fluid.rest_density, 1000.0f);
    EXPECT_FLOAT_EQ(fluid.gas_stiffness, 50.0f);
    EXPECT_FLOAT_EQ(fluid.damping, 0.01f);
}

TEST(FluidEmitterComponentTest, 默认碰撞) {
    FluidEmitterComponent fluid;
    EXPECT_FLOAT_EQ(fluid.collision_restitution, 0.3f);
    EXPECT_FLOAT_EQ(fluid.floor_y, 0.0f);
}

TEST(FluidEmitterComponentTest, 默认渲染参数) {
    FluidEmitterComponent fluid;
    EXPECT_FLOAT_EQ(fluid.color.r, 0.2f);
    EXPECT_FLOAT_EQ(fluid.color.g, 0.5f);
    EXPECT_FLOAT_EQ(fluid.color.b, 0.9f);
    EXPECT_FLOAT_EQ(fluid.color.a, 0.8f);
    EXPECT_FLOAT_EQ(fluid.refraction_strength, 0.3f);
    EXPECT_FLOAT_EQ(fluid.fresnel_power, 2.0f);
    EXPECT_FLOAT_EQ(fluid.specular_intensity, 0.8f);
    EXPECT_FLOAT_EQ(fluid.depth_smoothing_radius, 5.0f);
}

TEST(FluidEmitterComponentTest, 默认运行时状态) {
    FluidEmitterComponent fluid;
    EXPECT_TRUE(fluid.particles.empty());
    EXPECT_FLOAT_EQ(fluid.emit_accumulator, 0.0f);
    EXPECT_EQ(fluid.active_count, 0u);
    EXPECT_EQ(fluid.instance_vbo, 0u);
    EXPECT_FALSE(fluid.gpu_dirty);
}

TEST(FluidEmitterComponentTest, 粒子添加与计数) {
    FluidEmitterComponent fluid;
    FluidParticle p;
    p.position = glm::vec3(1.0f, 2.0f, 3.0f);
    p.velocity = glm::vec3(0.0f, -1.0f, 0.0f);
    p.life = 2.5f;
    fluid.particles.push_back(p);
    fluid.active_count = 1;
    ASSERT_EQ(fluid.particles.size(), 1u);
    EXPECT_FLOAT_EQ(fluid.particles[0].position.y, 2.0f);
    EXPECT_FLOAT_EQ(fluid.particles[0].life, 2.5f);
}

TEST(FluidEmitterComponentTest, 发射器形状切换) {
    FluidEmitterComponent fluid;
    fluid.shape = FluidEmitterShape::Sphere;
    fluid.sphere_radius = 2.0f;
    EXPECT_EQ(fluid.shape, FluidEmitterShape::Sphere);
    EXPECT_FLOAT_EQ(fluid.sphere_radius, 2.0f);

    fluid.shape = FluidEmitterShape::Box;
    fluid.box_half_extents = glm::vec3(1.0f, 0.5f, 1.0f);
    EXPECT_EQ(fluid.shape, FluidEmitterShape::Box);
    EXPECT_FLOAT_EQ(fluid.box_half_extents.y, 0.5f);
}
