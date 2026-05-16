/**
 * @file particle3d_system_test.cpp
 * @brief Particle3DSystem + 组件数据结构单元测试（无 GPU）
 *
 * 测试策略：
 * - GPUParticleData / ParticleSystem3DComponent 默认值
 * - 组件参数修改
 * - Particle3DSystem 构造/Init/Shutdown 安全性
 * - 空 World 不崩溃
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/particles/particle3d_system.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include <glm/glm.hpp>

using namespace dse;
using namespace dse::gameplay3d;

// ============================================================
// GPUParticleData 默认值
// ============================================================

TEST(GPUParticleDataTest, 默认值) {
    GPUParticleData p;
    EXPECT_FLOAT_EQ(p.position.x, 0.0f);
    EXPECT_FLOAT_EQ(p.position.y, 0.0f);
    EXPECT_FLOAT_EQ(p.position.z, 0.0f);
    EXPECT_FLOAT_EQ(p.color.r, 1.0f);
    EXPECT_FLOAT_EQ(p.color.g, 1.0f);
    EXPECT_FLOAT_EQ(p.color.b, 1.0f);
    EXPECT_FLOAT_EQ(p.color.a, 1.0f);
    EXPECT_FLOAT_EQ(p.size, 0.0f);
    EXPECT_FLOAT_EQ(p.velocity.x, 0.0f);
    EXPECT_FLOAT_EQ(p.velocity.y, 0.0f);
    EXPECT_FLOAT_EQ(p.velocity.z, 0.0f);
    EXPECT_FLOAT_EQ(p.life, 0.0f);
}

// ============================================================
// ParticleSystem3DComponent 默认值
// ============================================================

TEST(Particle3DSystemComponentTest, 默认值) {
    ParticleSystem3DComponent ps;
    EXPECT_TRUE(ps.enabled);
    EXPECT_EQ(ps.max_particles, 1000);
    EXPECT_FLOAT_EQ(ps.emission_rate, 100.0f);
    EXPECT_FLOAT_EQ(ps.emission_accumulator, 0.0f);
}

TEST(Particle3DSystemComponentTest, 发射参数默认值) {
    ParticleSystem3DComponent ps;
    EXPECT_FLOAT_EQ(ps.start_life_min, 1.0f);
    EXPECT_FLOAT_EQ(ps.start_life_max, 2.0f);
    EXPECT_FLOAT_EQ(ps.start_size_min, 0.1f);
    EXPECT_FLOAT_EQ(ps.start_size_max, 0.5f);
    EXPECT_FLOAT_EQ(ps.start_speed_min, 1.0f);
    EXPECT_FLOAT_EQ(ps.start_speed_max, 5.0f);
    EXPECT_FLOAT_EQ(ps.start_color.r, 1.0f);
    EXPECT_FLOAT_EQ(ps.start_color.a, 1.0f);
}

TEST(Particle3DSystemComponentTest, 动力学参数默认值) {
    ParticleSystem3DComponent ps;
    EXPECT_FLOAT_EQ(ps.gravity.x, 0.0f);
    EXPECT_FLOAT_EQ(ps.gravity.y, -9.81f);
    EXPECT_FLOAT_EQ(ps.gravity.z, 0.0f);
}

TEST(Particle3DSystemComponentTest, GPU资源默认值) {
    ParticleSystem3DComponent ps;
    EXPECT_TRUE(ps.texture_path.empty());
    EXPECT_EQ(ps.texture_handle, 0u);
    EXPECT_EQ(ps.instance_vbo, 0u);
    EXPECT_TRUE(ps.particles.empty());
    EXPECT_EQ(ps.active_particle_count, 0);
    EXPECT_FALSE(ps.initialized);
}

TEST(Particle3DSystemComponentTest, 修改参数) {
    ParticleSystem3DComponent ps;
    ps.max_particles = 500;
    ps.emission_rate = 50.0f;
    ps.start_life_min = 0.5f;
    ps.start_life_max = 1.0f;
    ps.gravity = glm::vec3(0.0f, -20.0f, 0.0f);
    EXPECT_EQ(ps.max_particles, 500);
    EXPECT_FLOAT_EQ(ps.emission_rate, 50.0f);
    EXPECT_FLOAT_EQ(ps.gravity.y, -20.0f);
}

// ============================================================
// Particle3DSystem 构造与生命周期
// ============================================================

TEST(Particle3DSystemTest, 默认构造安全) {
    Particle3DSystem sys;
    (void)sys;
}

TEST(Particle3DSystemTest, Init_nullptr安全) {
    Particle3DSystem sys;
    World world;
    sys.Init(world, nullptr);
}

TEST(Particle3DSystemTest, Shutdown_nullptr_rhi安全) {
    Particle3DSystem sys;
    World world;
    sys.Init(world, nullptr);
    sys.Shutdown(world);
}

TEST(Particle3DSystemTest, SetAssetManager_nullptr安全) {
    Particle3DSystem sys;
    sys.SetAssetManager(nullptr);
}

// ============================================================
// Particle3DSystem 空 World
// ============================================================

TEST(Particle3DSystemTest, 空World_Shutdown不崩溃) {
    Particle3DSystem sys;
    World world;
    sys.Init(world, nullptr);
    sys.Shutdown(world);
}
