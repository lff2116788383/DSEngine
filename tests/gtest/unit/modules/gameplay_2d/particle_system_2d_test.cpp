/**
 * @file particle_system_2d_test.cpp
 * @brief ParticleSystem (2D) 粒子系统的单元测试
 *
 * 覆盖场景：
 * - Update 调用不崩溃（空 World）
 * - 带粒子发射器组件的实体 Update
 *
 * 注意：Render 需要 CommandBuffer，不在单元测试中覆盖。
 */

#include <gtest/gtest.h>
#include "modules/gameplay_2d/particle/particle_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/particle_2d.h"

TEST(ParticleSystem2DTest, 空World调用Update不崩溃) {
    World world;
    ParticleSystem sys;
    sys.Update(world, 1.0f / 60.0f);
}

TEST(ParticleSystem2DTest, 带ParticleEmitter2D实体Update不崩溃) {
    World world;
    ParticleSystem sys;
    auto e = world.CreateEntity();
    auto& reg = world.registry();
    auto& emitter = reg.emplace<ParticleEmitterComponent>(e);
    emitter.emitting = true;
    emitter.emit_rate = 10.0f;

    sys.Update(world, 1.0f / 60.0f);
}
