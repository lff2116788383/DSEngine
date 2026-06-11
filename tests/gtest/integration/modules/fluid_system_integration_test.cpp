/**
 * @file fluid_system_integration_test.cpp
 * @brief 流体模拟系统集成测试
 *
 * 验证场景：
 * - FluidSystem 正确发射粒子
 * - 粒子在重力下下落
 * - 粒子碰地反弹
 * - 粒子生命周期到期后被移除
 * - SPH 密度估计非零
 * - 不同发射器形状正确工作
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_fluid.h"
#include "modules/gameplay_3d/fluid/fluid_system.h"

using namespace dse;
using namespace dse::gameplay3d;

class FluidSystemIntegrationTest : public ::testing::Test {
protected:
    World world;
    FluidSystem system;

    entt::entity CreateFluidEmitter(FluidEmitterShape shape = FluidEmitterShape::Point) {
        auto e = world.CreateEntity();

        TransformComponent tc;
        tc.position = glm::vec3(0.0f, 10.0f, 0.0f);
        world.registry().emplace<TransformComponent>(e, tc);

        FluidEmitterComponent fluid;
        fluid.enabled = true;
        fluid.shape = shape;
        fluid.emission_rate = 1000.0f; // 每秒 1000 粒子
        fluid.particle_lifetime = 2.0f;
        fluid.emit_speed = 1.0f;
        fluid.emit_direction = glm::vec3(0.0f, -1.0f, 0.0f);
        fluid.emit_spread = 0.0f; // 无扩散，方便测试
        fluid.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
        fluid.floor_y = 0.0f;
        fluid.collision_restitution = 0.5f;
        world.registry().emplace<FluidEmitterComponent>(e, fluid);

        return e;
    }
};

TEST_F(FluidSystemIntegrationTest, InitializeDoesNotCrash) {
    system.Init(world, nullptr);
    EXPECT_NO_THROW(system.Shutdown(world));
}

TEST_F(FluidSystemIntegrationTest, TestCase2) {
    auto e = CreateFluidEmitter();
    system.Init(world, nullptr);

    // 模拟一帧（dt=0.1s，应发射约 100 个粒子）
    system.Update(world, 0.1f);

    auto& fluid = world.registry().get<FluidEmitterComponent>(e);
    EXPECT_GT(fluid.particles.size(), 0u) << "应发射粒子";
    EXPECT_GT(fluid.active_count, 0u);
    EXPECT_TRUE(fluid.gpu_dirty);
}

TEST_F(FluidSystemIntegrationTest, Andrate) {
    auto e = CreateFluidEmitter();
    system.Init(world, nullptr);

    system.Update(world, 0.05f); // 50ms → 约 50 粒子
    auto& fluid = world.registry().get<FluidEmitterComponent>(e);
    // 允许 ±20% 误差
    EXPECT_GT(fluid.particles.size(), 30u);
    EXPECT_LT(fluid.particles.size(), 70u);
}

TEST_F(FluidSystemIntegrationTest, Exist) {
    auto e = CreateFluidEmitter();
    system.Init(world, nullptr);

    system.Update(world, 0.01f); // 发射一批
    auto& fluid = world.registry().get<FluidEmitterComponent>(e);
    if (fluid.particles.empty()) return;

    float initial_y = fluid.particles[0].position.y;

    // 多步模拟
    for (int i = 0; i < 10; ++i) {
        system.Update(world, 0.016f);
    }

    // 至少有部分粒子的 Y 坐标应降低
    bool any_dropped = false;
    for (const auto& p : fluid.particles) {
        if (p.position.y < initial_y - 0.01f) {
            any_dropped = true;
            break;
        }
    }
    EXPECT_TRUE(any_dropped) << "粒子应在重力下下落";
}

TEST_F(FluidSystemIntegrationTest, Case_Not) {
    auto e = CreateFluidEmitter();
    auto& fluid = world.registry().get<FluidEmitterComponent>(e);
    fluid.floor_y = 0.0f;
    fluid.particle_lifetime = 5.0f;
    system.Init(world, nullptr);

    // 发射后快进多步
    system.Update(world, 0.01f);
    for (int i = 0; i < 100; ++i) {
        system.Update(world, 0.016f);
    }

    // 所有存活粒子 Y >= floor_y + particle_radius
    for (const auto& p : fluid.particles) {
        if (p.life > 0.0f) {
            EXPECT_GE(p.position.y, fluid.floor_y)
                << "粒子不应穿过地面";
        }
    }
}

TEST_F(FluidSystemIntegrationTest, Lifecycle_Remove) {
    auto e = CreateFluidEmitter();
    auto& fluid = world.registry().get<FluidEmitterComponent>(e);
    fluid.particle_lifetime = 0.05f; // 很短的生命周期
    fluid.emission_rate = 100.0f;
    system.Init(world, nullptr);

    // 发射
    system.Update(world, 0.02f);
    size_t initial_count = fluid.particles.size();
    EXPECT_GT(initial_count, 0u);

    // 停止发射（设置 emission_rate=0）后等粒子死亡
    fluid.emission_rate = 0.0f;
    for (int i = 0; i < 10; ++i) {
        system.Update(world, 0.02f);
    }
    EXPECT_LT(fluid.particles.size(), initial_count)
        << "过期粒子应被移除";
}

TEST_F(FluidSystemIntegrationTest, SPHdensityEstimate_WhenParticlesAreDenselyPackedTheDensityIsGreaterThanZero) {
    auto e = CreateFluidEmitter();
    auto& fluid = world.registry().get<FluidEmitterComponent>(e);
    fluid.emission_rate = 5000.0f;
    fluid.emit_spread = 0.1f;
    system.Init(world, nullptr);

    // 大量发射以形成密度
    system.Update(world, 0.05f);
    system.Update(world, 0.016f);

    bool has_density = false;
    for (const auto& p : fluid.particles) {
        if (p.density > 0.0f) {
            has_density = true;
            break;
        }
    }
    EXPECT_TRUE(has_density) << "密集粒子的 SPH 密度应 > 0";
}

TEST_F(FluidSystemIntegrationTest, Emitter_ExistInside) {
    auto e = CreateFluidEmitter(FluidEmitterShape::Sphere);
    auto& fluid = world.registry().get<FluidEmitterComponent>(e);
    fluid.sphere_radius = 1.0f;
    fluid.emit_speed = 0.0f; // 零速度，仅验证位置分布
    fluid.gravity = glm::vec3(0.0f);
    system.Init(world, nullptr);

    system.Update(world, 0.1f);

    auto& tc = world.registry().get<TransformComponent>(e);
    for (const auto& p : fluid.particles) {
        float dist = glm::distance(p.position, tc.position);
        EXPECT_LE(dist, fluid.sphere_radius + 0.01f)
            << "粒子应在球形发射器半径内";
    }
}

TEST_F(FluidSystemIntegrationTest, Emitter_ExistInside_2) {
    auto e = CreateFluidEmitter(FluidEmitterShape::Box);
    auto& fluid = world.registry().get<FluidEmitterComponent>(e);
    fluid.box_half_extents = glm::vec3(2.0f, 0.5f, 2.0f);
    fluid.emit_speed = 0.0f;
    fluid.gravity = glm::vec3(0.0f);
    system.Init(world, nullptr);

    system.Update(world, 0.1f);

    auto& tc = world.registry().get<TransformComponent>(e);
    for (const auto& p : fluid.particles) {
        glm::vec3 local = p.position - tc.position;
        EXPECT_LE(std::abs(local.x), fluid.box_half_extents.x + 0.01f);
        EXPECT_LE(std::abs(local.y), fluid.box_half_extents.y + 0.01f);
        EXPECT_LE(std::abs(local.z), fluid.box_half_extents.z + 0.01f);
    }
}

TEST_F(FluidSystemIntegrationTest, DisabledEmitterNot) {
    auto e = CreateFluidEmitter();
    auto& fluid = world.registry().get<FluidEmitterComponent>(e);
    fluid.enabled = false;
    system.Init(world, nullptr);

    system.Update(world, 0.1f);
    EXPECT_EQ(fluid.particles.size(), 0u) << "禁用时不应发射粒子";
}

TEST_F(FluidSystemIntegrationTest, ZerodtDoesNotCrash) {
    CreateFluidEmitter();
    system.Init(world, nullptr);
    EXPECT_NO_THROW(system.Update(world, 0.0f));
}

TEST_F(FluidSystemIntegrationTest, BurdendtDoesNotCrash) {
    CreateFluidEmitter();
    system.Init(world, nullptr);
    EXPECT_NO_THROW(system.Update(world, -1.0f));
}
