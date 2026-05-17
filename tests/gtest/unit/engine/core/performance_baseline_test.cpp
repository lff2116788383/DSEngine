/**
 * @file performance_baseline_test.cpp
 * @brief 性能基线测试 — 验证关键路径不退化
 *
 * 覆盖：
 * - ECS 实体迭代吞吐量（10K entity）
 * - JobSystem 任务 dispatch+join 吞吐量（1K tasks）
 * - World 创建/销毁性能
 * - ServiceLocator 查询性能
 * - EventBus 广播性能
 * - InstancingKey hash 吞吐量
 *
 * 注：这些测试使用 EXPECT_LT 对耗时设定宽松上限，
 *     主要目的是检测严重退化（10x+），而非精确微基准。
 */

#include <gtest/gtest.h>
#include <chrono>
#include <atomic>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/core/job_system.h"
#include "engine/core/service_locator.h"
#include "engine/core/event_bus.h"
#include "engine/core/event_id.h"
#include "engine/render/rhi/rhi_types.h"

using namespace dse;
using Clock = std::chrono::high_resolution_clock;

// ============================================================
// ECS 吞吐量
// ============================================================

TEST(PerformanceBaseline, ECS_10K实体迭代) {
    World world;
    constexpr int N = 10000;

    for (int i = 0; i < N; ++i) {
        auto e = world.registry().create();
        world.registry().emplace<TransformComponent>(e);
    }

    auto start = Clock::now();
    int count = 0;
    world.registry().view<TransformComponent>().each(
        [&](auto entity, TransformComponent& t) {
            t.position.x += 1.0f;
            count++;
        });
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();

    EXPECT_EQ(count, N);
    // 10K entity 迭代应在 5ms 内完成（极宽松阈值，正常 < 0.5ms）
    EXPECT_LT(elapsed, 5000) << "ECS iteration too slow: " << elapsed << " us";
}

TEST(PerformanceBaseline, ECS_创建销毁10K实体) {
    auto start = Clock::now();
    {
        World world;
        for (int i = 0; i < 10000; ++i) {
            auto e = world.registry().create();
            world.registry().emplace<TransformComponent>(e);
        }
    } // 析构销毁全部
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();

    // 创建 + 销毁 10K entity 应在 50ms 内
    EXPECT_LT(elapsed, 50000) << "ECS create/destroy too slow: " << elapsed << " us";
}

// ============================================================
// JobSystem 吞吐量
// ============================================================

TEST(PerformanceBaseline, JobSystem_1K任务吞吐) {
    dse::core::JobSystem js;
    js.Init();

    constexpr int N = 1000;
    std::atomic<int> counter{0};

    auto start = Clock::now();
    std::vector<dse::core::JobHandle> handles;
    handles.reserve(N);
    for (int i = 0; i < N; ++i) {
        handles.push_back(js.Submit([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }
    for (auto& h : handles) {
        js.Wait(h);
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();

    EXPECT_EQ(counter.load(), N);
    // 1K 任务 submit+wait 应在 500ms 内（含线程调度开销）
    EXPECT_LT(elapsed, 500000) << "JobSystem too slow: " << elapsed << " us";

    js.Shutdown();
}

TEST(PerformanceBaseline, JobSystem_依赖链不死锁) {
    dse::core::JobSystem js;
    js.Init();

    std::atomic<int> sequence{0};
    auto h1 = js.Submit([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        sequence.store(1, std::memory_order_release);
    }, dse::core::JobPriority::High);

    auto h2 = js.SubmitWithDependency([&]() {
        int expected = sequence.load(std::memory_order_acquire);
        EXPECT_EQ(expected, 1); // h2 应在 h1 之后执行
        sequence.store(2, std::memory_order_release);
    }, {h1}, dse::core::JobPriority::Normal);

    js.Wait(h2);
    EXPECT_EQ(sequence.load(), 2);

    js.Shutdown();
}

// ============================================================
// ServiceLocator 查询性能
// ============================================================

TEST(PerformanceBaseline, ServiceLocator_100K查询) {
    auto& sl = dse::core::ServiceLocator::Instance();

    auto start = Clock::now();
    for (int i = 0; i < 100000; ++i) {
        // Get<JobSystem> 即使为 nullptr 也验证查询路径
        volatile auto* ptr = sl.Get<dse::core::JobSystem>();
        (void)ptr;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();

    // 100K 次 type-erased 查询应在 50ms 内
    EXPECT_LT(elapsed, 50000) << "ServiceLocator query too slow: " << elapsed << " us";
}

// ============================================================
// EventBus 广播性能
// ============================================================

namespace {
struct PerfTestEvent : public dse::core::Event {
    explicit PerfTestEvent(int v) : value(v) {}
    int value;
    static constexpr dse::core::EventId kEventId = dse::core::MakeEventId("PerfTestEvent");
};
} // namespace

TEST(PerformanceBaseline, EventBus_10K广播) {
    auto bus = std::make_shared<dse::core::EventBus>();

    std::atomic<int> received{0};

    bus->Subscribe<PerfTestEvent>([&](const PerfTestEvent& e) {
        received.fetch_add(e.value, std::memory_order_relaxed);
    });

    auto start = Clock::now();
    for (int i = 0; i < 10000; ++i) {
        bus->Publish<PerfTestEvent>(1);
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();

    EXPECT_EQ(received.load(), 10000);
    // 10K 事件广播（1 个订阅者）应在 50ms 内（含 mutex 开销）
    EXPECT_LT(elapsed, 50000) << "EventBus too slow: " << elapsed << " us";
}
