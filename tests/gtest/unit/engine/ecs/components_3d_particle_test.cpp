/**
 * @file components_3d_particle_test.cpp
 * @brief 3D 粒子组件默认值单元测试
 */

#include <gtest/gtest.h>
#include "engine/ecs/components_3d_particle.h"

using namespace dse;

TEST(ParticleSystem3DComponentTest, DefaultValues) {
    ParticleSystem3DComponent ps;
    EXPECT_TRUE(ps.enabled);
    EXPECT_EQ(ps.max_particles, 1000);
    EXPECT_FLOAT_EQ(ps.emission_rate, 100.0f);
    EXPECT_FLOAT_EQ(ps.emission_accumulator, 0.0f);
    EXPECT_FLOAT_EQ(ps.start_life_min, 1.0f);
    EXPECT_FLOAT_EQ(ps.start_life_max, 2.0f);
    EXPECT_FLOAT_EQ(ps.start_size_min, 0.1f);
    EXPECT_FLOAT_EQ(ps.start_size_max, 0.5f);
    EXPECT_FLOAT_EQ(ps.start_speed_min, 1.0f);
    EXPECT_FLOAT_EQ(ps.start_speed_max, 5.0f);
    EXPECT_EQ(ps.texture_handle, 0u);
    EXPECT_EQ(ps.instance_vbo, 0u);
    EXPECT_EQ(ps.active_particle_count, 0);
    EXPECT_FALSE(ps.initialized);
}

TEST(ParticleSystem3DComponentTest, FieldModification) {
    ParticleSystem3DComponent ps;
    ps.enabled = false;
    ps.max_particles = 5000;
    ps.emission_rate = 200.0f;
    ps.gravity = glm::vec3(0.0f, -20.0f, 0.0f);
    EXPECT_FALSE(ps.enabled);
    EXPECT_EQ(ps.max_particles, 5000);
    EXPECT_FLOAT_EQ(ps.emission_rate, 200.0f);
    EXPECT_FLOAT_EQ(ps.gravity.y, -20.0f);
}

TEST(GPUParticleDataTest, Default) {
    GPUParticleData p;
    EXPECT_FLOAT_EQ(p.life, 0.0f);
    EXPECT_FLOAT_EQ(p.size, 0.0f);
}
