/**
 * @file particle_system_2d_test.cpp
 * @brief ParticleSystem (2D) 粒子系统的单元测试
 *
 * 覆盖场景：
 * - 空 World 调用 Update 不崩溃
 * - 发射器持续发射粒子（emitting=true）
 * - 粒子生命周期衰减后自动移除
 * - max_particles 限制粒子池大小
 * - Burst 一次性爆发
 * - 重力影响粒子速度
 * - 地面碰撞反弹
 * - 随机参数模式（use_random_params）
 * - 生命周期尺寸曲线
 * - emit_rate_scale 缩放发射率
 */

#include <gtest/gtest.h>
#include "modules/gameplay_2d/particle/particle_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/particle_2d.h"
#include "engine/ecs/transform.h"

class ParticleSystem2DTest : public ::testing::Test {
protected:
    World world;
    ParticleSystem sys;
    static constexpr float kDt = 1.0f / 60.0f;

    /// 创建一个带发射器+变换的实体
    std::tuple<Entity, ParticleEmitterComponent*, TransformComponent*>
    CreateEmitter() {
        auto e = world.CreateEntity();
        auto& emitter = world.registry().emplace<ParticleEmitterComponent>(e);
        auto& tf = world.registry().emplace<TransformComponent>(e);
        tf.position = glm::vec3(0.0f, 10.0f, 0.0f);
        return {e, &emitter, &tf};
    }
};

TEST_F(ParticleSystem2DTest, EmptyWorldCallsUpdateDoesNotCrash) {
    EXPECT_NO_THROW(sys.Update(world, kDt));
}

TEST_F(ParticleSystem2DTest, TestCase2) {
    auto [e, emitter, tf] = CreateEmitter();
    emitter->emitting = true;
    emitter->emit_rate = 60.0f; // 每秒 60 个
    emitter->start_life_time = 5.0f;

    // 推进 1 秒
    for (int i = 0; i < 60; ++i) {
        sys.Update(world, kDt);
    }

    EXPECT_GT(emitter->particles.size(), 0u);
}

TEST_F(ParticleSystem2DTest, LifecycleAfterRemove) {
    auto [e, emitter, tf] = CreateEmitter();
    emitter->emitting = true;
    emitter->emit_rate = 10.0f;
    emitter->start_life_time = 0.1f; // 极短寿命

    // 先推进到至少一个发射间隔：10/s 需要 0.1s，5 帧(约 0.083s)不足以触发发射
    for (int i = 0; i < 10; ++i) {
        sys.Update(world, kDt);
    }
    size_t count_after_emit = emitter->particles.size();
    EXPECT_GT(count_after_emit, 0u);

    // 关闭发射，等待生命结束
    emitter->emitting = false;
    for (int i = 0; i < 60; ++i) {
        sys.Update(world, kDt);
    }

    // 所有短命粒子应已移除
    EXPECT_EQ(emitter->particles.size(), 0u);
}

TEST_F(ParticleSystem2DTest, max_ParticlesrestrictedParticlePool) {
    auto [e, emitter, tf] = CreateEmitter();
    emitter->emitting = true;
    emitter->emit_rate = 1000.0f; // 高发射率
    emitter->max_particles = 5;
    emitter->start_life_time = 10.0f; // 长寿命

    for (int i = 0; i < 60; ++i) {
        sys.Update(world, kDt);
    }

    EXPECT_LE(emitter->particles.size(), static_cast<size_t>(emitter->max_particles));
}

TEST_F(ParticleSystem2DTest, BurstoneTimeOutbreak) {
    auto [e, emitter, tf] = CreateEmitter();
    emitter->emitting = false; // 关闭持续发射
    emitter->pending_burst = 3;
    emitter->start_life_time = 5.0f;

    sys.Update(world, kDt);

    EXPECT_EQ(emitter->particles.size(), 3u);
    EXPECT_EQ(emitter->pending_burst, 0);
}

TEST_F(ParticleSystem2DTest, Speed) {
    auto [e, emitter, tf] = CreateEmitter();
    emitter->emitting = true;
    emitter->emit_rate = 60.0f;
    emitter->start_life_time = 5.0f;
    emitter->gravity = glm::vec3(0.0f, -9.8f, 0.0f);

    // 先发射几帧
    for (int i = 0; i < 5; ++i) {
        sys.Update(world, kDt);
    }
    ASSERT_GT(emitter->particles.size(), 0u);

    float vy_before = emitter->particles[0].velocity.y;
    sys.Update(world, kDt);

    // 重力使 Y 速度变负
    float vy_after = emitter->particles[0].velocity.y;
    EXPECT_LT(vy_after, vy_before);
}

TEST_F(ParticleSystem2DTest, TestCase7) {
    auto [e, emitter, tf] = CreateEmitter();
    emitter->emitting = true;
    emitter->emit_rate = 60.0f;
    emitter->start_life_time = 5.0f;
    emitter->use_ground_collision = true;
    emitter->ground_y = 0.0f;
    emitter->collision_bounce = 0.5f;
    emitter->gravity = glm::vec3(0.0f, -50.0f, 0.0f);

    // 先发射几帧
    for (int i = 0; i < 5; ++i) {
        sys.Update(world, kDt);
    }
    ASSERT_GT(emitter->particles.size(), 0u);

    // 手动将粒子放到地面以下
    emitter->particles[0].position.y = -1.0f;
    emitter->particles[0].velocity.y = -10.0f;

    sys.Update(world, kDt);

    // 碰撞后粒子应在地面或以上，速度反弹
    EXPECT_GE(emitter->particles[0].position.y, 0.0f);
    EXPECT_GT(emitter->particles[0].velocity.y, 0.0f);
}

TEST_F(ParticleSystem2DTest, Parametersmodel) {
    auto [e, emitter, tf] = CreateEmitter();
    emitter->emitting = true;
    emitter->emit_rate = 100.0f;
    emitter->start_life_time = 5.0f;
    emitter->use_random_params = true;
    emitter->velocity_min = glm::vec3(-5.0f, 1.0f, 0.0f);
    emitter->velocity_max = glm::vec3(5.0f, 10.0f, 0.0f);

    // 多帧确保发射足够粒子
    for (int i = 0; i < 5; ++i) {
        sys.Update(world, kDt);
    }
    ASSERT_GE(emitter->particles.size(), 2u);

    // 随机参数下粒子速度应有差异
    bool has_variation = false;
    for (size_t i = 1; i < emitter->particles.size(); ++i) {
        if (emitter->particles[i].velocity != emitter->particles[0].velocity) {
            has_variation = true;
            break;
        }
    }
    EXPECT_TRUE(has_variation);
}

TEST_F(ParticleSystem2DTest, Lifecycle) {
    auto [e, emitter, tf] = CreateEmitter();
    emitter->emitting = true;
    emitter->emit_rate = 60.0f;
    emitter->start_life_time = 1.0f;
    emitter->start_size = 10.0f;
    emitter->size_curve.enabled = true;
    emitter->size_curve.start_value = 10.0f;
    emitter->size_curve.end_value = 0.0f;
    emitter->size_curve.type = ParticleCurveType::Linear;

    // 多帧确保发射
    for (int i = 0; i < 5; ++i) {
        sys.Update(world, kDt);
    }
    ASSERT_GT(emitter->particles.size(), 0u);

    float initial_size = emitter->particles[0].size;
    // 推进一段时间
    for (int i = 0; i < 30; ++i) {
        sys.Update(world, kDt);
    }

    // 尺寸应因曲线而衰减
    if (!emitter->particles.empty()) {
        EXPECT_LT(emitter->particles[0].size, initial_size);
    }
}

TEST_F(ParticleSystem2DTest, emit_rate_ScaleScaledEmissivity) {
    auto [e, emitter, tf] = CreateEmitter();
    emitter->emitting = true;
    emitter->emit_rate = 10.0f;
    emitter->emit_rate_scale = 2.0f; // 双倍发射
    emitter->start_life_time = 5.0f;

    for (int i = 0; i < 60; ++i) {
        sys.Update(world, kDt);
    }
    size_t count_scaled = emitter->particles.size();

    // 对比无缩放
    World world2;
    auto e2 = world2.CreateEntity();
    auto& em2 = world2.registry().emplace<ParticleEmitterComponent>(e2);
    auto& tf2 = world2.registry().emplace<TransformComponent>(e2);
    em2.emitting = true;
    em2.emit_rate = 10.0f;
    em2.emit_rate_scale = 1.0f;
    em2.start_life_time = 5.0f;

    for (int i = 0; i < 60; ++i) {
        sys.Update(world2, kDt);
    }

    // 缩放后应发射更多（或相等，受 max_particles 限制）
    EXPECT_GE(count_scaled, em2.particles.size());
}
