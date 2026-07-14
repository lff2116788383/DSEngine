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
    World world;
    constexpr int kEntities = 100000;

    auto start = std::chrono::steady_clock::now();
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
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_EQ(world.EntityCount(), 0u);
    EXPECT_LT(ms, 2000) << "create+destroy of " << kEntities << " entities took " << ms << "ms";
}
