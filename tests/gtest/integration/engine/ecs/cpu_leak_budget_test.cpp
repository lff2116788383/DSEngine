/**
 * @file cpu_leak_budget_test.cpp
 * @brief P2-2 headless CPU-side leak + performance-budget gates.
 *
 * Complements the editor undo/redo stability suite with engine-level checks that
 * repeated create/destroy churn of ECS entities and components returns exactly to
 * baseline (no unbounded growth) and that entity throughput stays within a budget.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>

#include "tests/gtest/support/perf_probe.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"

namespace {

std::size_t TransformStorageSize(World& world) {
    return world.registry().view<TransformComponent>().size();
}

}  // namespace

// Repeated create/destroy cycles must return the world to exactly zero alive
// entities and zero component storage — i.e. no per-cycle accumulation.
TEST(CpuLeakBudget, EntityChurnReturnsToBaseline) {
    World world;
    ASSERT_EQ(world.EntityCount(), 0u);
    ASSERT_EQ(TransformStorageSize(world), 0u);

    constexpr int kCycles = 50;
    constexpr int kEntitiesPerCycle = 2000;

    for (int cycle = 0; cycle < kCycles; ++cycle) {
        std::vector<Entity> entities;
        entities.reserve(kEntitiesPerCycle);
        for (int i = 0; i < kEntitiesPerCycle; ++i) {
            Entity e = world.CreateEntity();
            auto& t = world.registry().emplace<TransformComponent>(e);
            t.position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
            entities.push_back(e);
        }
        EXPECT_EQ(world.EntityCount(), static_cast<std::size_t>(kEntitiesPerCycle));
        EXPECT_EQ(TransformStorageSize(world), static_cast<std::size_t>(kEntitiesPerCycle));

        for (Entity e : entities) {
            world.DestroyEntity(e);
        }
        EXPECT_EQ(world.EntityCount(), 0u) << "leak after cycle " << cycle;
        EXPECT_EQ(TransformStorageSize(world), 0u) << "component leak after cycle " << cycle;
    }
}

// World::Clear() must also fully reset alive count and component storage.
TEST(CpuLeakBudget, ClearReleasesEverything) {
    World world;
    for (int i = 0; i < 5000; ++i) {
        Entity e = world.CreateEntity();
        world.registry().emplace<TransformComponent>(e);
    }
    ASSERT_EQ(world.EntityCount(), 5000u);
    world.Clear();
    EXPECT_EQ(world.EntityCount(), 0u);
    EXPECT_EQ(TransformStorageSize(world), 0u);
}

// Entity create+destroy throughput budget: a large batch must complete well
// within a generous wall-clock bound on the CI/host machine.
TEST(CpuLeakBudget, EntityChurnThroughputWithinBudget) {
    constexpr int kEntities = 100000;

    // best-of-N 取代单次采样。原先用「单次墙钟采样 + 2000ms 固定预算」，在共享机器
    // （CI runner / 并行构建 / 杀毒扫描）上会随机变红：实测同一用例在 CPU 99% 占用时
    // 报 3350ms，而隔离运行 5/5 全绿。该断言的目标是「抓数量级退化」，故取多次运行的
    // 最小值  调度噪声只会让某几次变慢，真实退化会让每一次都变慢。
    // 每次尝试都建/毁一个独立 World（操作可重复），并顺带验证 churn 之后回到基线。
    bool returned_to_baseline = true;
    const auto s = dse::test::ProbeMillis(dse::test::PerfAttempts(5), [&] {
        World world;
        std::vector<Entity> entities;
        entities.reserve(kEntities);
        for (int i = 0; i < kEntities; ++i) {
            Entity e = world.CreateEntity();
            world.registry().emplace<TransformComponent>(e);
            entities.push_back(e);
        }
        for (Entity e : entities) {
            world.DestroyEntity(e);
        }
        if (world.EntityCount() != 0u) returned_to_baseline = false;
    });

    dse::test::PrintPerf("ECS create+destroy 100K entities (ms)", s);
    EXPECT_TRUE(returned_to_baseline) << "churn 之后未回到基线（实体未被清空）";
    // 预算标定（测量时机器 CPU 已达 99%）：基线 542 ms（best-of-15），实测最差 1140 ms，
    // 单次采样曾出现 3350 ms。取 6000 ms = 既是基线的 11 倍，也高于实测最差的 3 倍，
    // 与「抓数量级退化」的目标一致，同时不会因机器负载随机变红。
    EXPECT_LT(s.best_ms, 6000.0)
        << dse::test::SampleSummary(s, 6000.0, "ECS churn 100K entities");
}
