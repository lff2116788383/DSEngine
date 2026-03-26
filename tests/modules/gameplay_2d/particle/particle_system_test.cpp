#include "catch/catch.hpp"
#include "modules/gameplay_2d/particle/particle_system.h"
#include "engine/ecs/components_2d.h"

// 正向测试：开启发射后更新应生成粒子并写入初始生命周期。
TEST_CASE("Given_EmitterEnabled_When_Update_Then_ParticlesAreEmitted", "[engine][unit][particle]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity).position = glm::vec3(2.0f, 3.0f, 0.0f);
    emitter.emit_rate = 10.0f;
    emitter.emit_rate_scale = 1.0f;
    emitter.start_life_time = 2.0f;
    emitter.max_particles = 8;

    ParticleSystem system;
    system.Update(world, 0.2f);

    REQUIRE_FALSE(emitter.particles.empty());
    REQUIRE(emitter.particles.front().life_time == Approx(2.0f));
}

// 边界测试：发射率极低时仍应被下限保护，不会出现除零与异常发射间隔。
TEST_CASE("Given_VeryLowEmitRate_When_Update_Then_EmitterStillWorksSafely", "[engine][unit][particle]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    emitter.emit_rate = 0.0f;
    emitter.emit_rate_scale = 0.0f;
    emitter.max_particles = 2;
    emitter.emitting = true;

    ParticleSystem system;
    system.Update(world, 0.5f);
    REQUIRE(emitter.particles.size() <= 2);
}

// 反向测试：负数 burst 请求应被修正为 0，避免出现无限补发行为。
TEST_CASE("Given_NegativeBurstRequest_When_Update_Then_PendingBurstIsClamped", "[engine][unit][particle]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    emitter.pending_burst = -5;
    emitter.emitting = false;

    ParticleSystem system;
    system.Update(world, 0.016f);

    REQUIRE(emitter.pending_burst == 0);
}
