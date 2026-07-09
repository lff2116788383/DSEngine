/**
* @file system_scheduler_test.cpp
* @brief 并行 ECS 系统调度器单元测试
*
* 覆盖场景：
* - 无冲突系统并行执行
* - 有冲突系统串行执行（依赖顺序正确）
* - CommandBuffer 延迟执行
* - 依赖图构建正确性
* - 系统禁用/启用
*/

#include <gtest/gtest.h>
#include "engine/ecs/system_scheduler.h"
#include "engine/ecs/world.h"
#include "engine/core/job_system.h"
#include <atomic>
#include <chrono>
#include <thread>

using namespace dse;
using namespace dse::ecs;

// ============================================================
// 依赖图与并行执行测试
// ============================================================

class SystemSchedulerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto js = std::make_shared<dse::core::JobSystem>();
        js->Init();
        dse::core::ServiceLocator::Instance().Register<dse::core::JobSystem, dse::core::JobSystem>(js);
        js_ptr_ = js.get();
    }

    void TearDown() override {
        js_ptr_->Shutdown();
        dse::core::ServiceLocator::Instance().Reset<dse::core::JobSystem>();
    }

private:
    dse::core::JobSystem* js_ptr_ = nullptr;
};

// 测试 无冲突系统可并行执行
TEST_F(SystemSchedulerTest, NonConflictingSystemsRunInParallel) {
    World world;
    SystemScheduler scheduler;

    std::atomic<int> active_count{0};
    std::atomic<int> max_active{0};

    auto make_system = [&](const std::string& name, ComponentAccess access) {
        scheduler.Register(name, access, [&active_count, &max_active](World&, float) {
            int current = active_count.fetch_add(1) + 1;
            int prev_max = max_active.load();
            while (current > prev_max && !max_active.compare_exchange_weak(prev_max, current)) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            active_count.fetch_sub(1);
        });
    };

    // 三个系统读写不同组件 → 无冲突 → 可并行
    struct CompA {};
    struct CompB {};
    struct CompC {};

    make_system("sys_a", ComponentAccess::WriteOnly<CompA>());
    make_system("sys_b", ComponentAccess::WriteOnly<CompB>());
    make_system("sys_c", ComponentAccess::WriteOnly<CompC>());

    scheduler.Execute(world, 0.016f);

    // 至少有两个系统同时运行过
    EXPECT_GE(max_active.load(), 2);
}

// 测试 有冲突系统串行执行
TEST_F(SystemSchedulerTest, ConflictingSystemsRunSerially) {
    World world;
    SystemScheduler scheduler;

    std::atomic<int> active_count{0};
    std::atomic<int> max_active{0};

    struct SharedComp {};

    auto make_system = [&](const std::string& name, bool is_writer) {
        ComponentAccess access;
        access.reads = {std::type_index(typeid(SharedComp))};
        if (is_writer) {
            access.writes = {std::type_index(typeid(SharedComp))};
        }
        scheduler.Register(name, access, [&active_count, &max_active](World&, float) {
            int current = active_count.fetch_add(1) + 1;
            int prev_max = max_active.load();
            while (current > prev_max && !max_active.compare_exchange_weak(prev_max, current)) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            active_count.fetch_sub(1);
        });
    };

    // 两个系统都写同一组件 → 冲突 → 必须串行
    make_system("writer_a", true);
    make_system("writer_b", true);

    scheduler.Execute(world, 0.016f);

    // 写写冲突 → 不会同时运行
    EXPECT_EQ(max_active.load(), 1);
}

// 测试 读写冲突导致串行
TEST_F(SystemSchedulerTest, ReadWriteConflictCausesSerial) {
    World world;
    SystemScheduler scheduler;

    std::atomic<int> active_count{0};
    std::atomic<int> max_active{0};

    struct SharedComp {};

    // writer 写 SharedComp
    scheduler.Register("writer", {
        {std::type_index(typeid(SharedComp))},  // reads
        {std::type_index(typeid(SharedComp))}   // writes
    }, [&active_count, &max_active](World&, float) {
        int current = active_count.fetch_add(1) + 1;
        int prev_max = max_active.load();
        while (current > prev_max && !max_active.compare_exchange_weak(prev_max, current)) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        active_count.fetch_sub(1);
    });

    // reader 只读 SharedComp → 与 writer 有读写冲突
    scheduler.Register("reader", {
        {std::type_index(typeid(SharedComp))},  // reads
        {}                                       // writes (none)
    }, [&active_count, &max_active](World&, float) {
        int current = active_count.fetch_add(1) + 1;
        int prev_max = max_active.load();
        while (current > prev_max && !max_active.compare_exchange_weak(prev_max, current)) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        active_count.fetch_sub(1);
    });

    scheduler.Execute(world, 0.016f);

    // 读写冲突 → 不会同时运行
    EXPECT_EQ(max_active.load(), 1);
}

// 测试 所有系统都被执行
TEST_F(SystemSchedulerTest, AllSystemsExecuted) {
    World world;
    SystemScheduler scheduler;

    std::atomic<int> counter{0};

    struct A {}; struct B {}; struct C {};

    scheduler.Register("a", ComponentAccess::WriteOnly<A>(), [&counter](World&, float) {
        counter.fetch_add(1);
    });
    scheduler.Register("b", ComponentAccess::WriteOnly<B>(), [&counter](World&, float) {
        counter.fetch_add(10);
    });
    scheduler.Register("c", ComponentAccess::WriteOnly<C>(), [&counter](World&, float) {
        counter.fetch_add(100);
    });

    scheduler.Execute(world, 0.016f);

    EXPECT_EQ(counter.load(), 111);
}

// 测试 禁用系统不执行
TEST_F(SystemSchedulerTest, DisabledSystemNotExecuted) {
    World world;
    SystemScheduler scheduler;

    std::atomic<int> counter{0};

    struct A {}; struct B {};

    scheduler.Register("a", ComponentAccess::Make<A>(), [&counter](World&, float) {
        counter.fetch_add(1);
    });
    scheduler.Register("b", ComponentAccess::Make<B>(), [&counter](World&, float) {
        counter.fetch_add(10);
    });

    scheduler.SetEnabled("b", false);
    scheduler.Execute(world, 0.016f);

    EXPECT_EQ(counter.load(), 1);
}

// 测试 CommandBuffer 延迟创建实体
TEST_F(SystemSchedulerTest, CommandBufferDeferredCreate) {
    World world;
    SystemScheduler scheduler;

    struct Marker {};

    scheduler.Register("creator", ComponentAccess{}, [&scheduler](World&, float) {
        // 通过 CommandBuffer 收集创建命令
        scheduler.GetCommandBuffer().Enqueue(
            StructuralCommand::Type::CreateEntity,
            entt::null,
            std::type_index(typeid(void)),
            [](entt::registry& reg, Entity) {
                auto e = reg.create();
                reg.emplace<Marker>(e);
            });
    });

    scheduler.Execute(world, 0.016f);

    // flush 前：无 Marker 实体
    auto view_before = world.registry().view<Marker>();
    EXPECT_EQ(view_before.size(), 0u);

    // flush 后：1 个 Marker 实体
    scheduler.FlushCommands(world.registry());
    auto view_after = world.registry().view<Marker>();
    EXPECT_EQ(view_after.size(), 1u);
}

// 测试 依赖链顺序正确
TEST_F(SystemSchedulerTest, DependencyChainOrderCorrect) {
    World world;
    SystemScheduler scheduler;

    struct Shared {};

    std::vector<std::string> execution_order;
    std::mutex order_mutex;

    auto make_system = [&](const std::string& name, bool is_writer) {
        ComponentAccess access;
        access.reads = {std::type_index(typeid(Shared))};
        if (is_writer) {
            access.writes = {std::type_index(typeid(Shared))};
        }
        scheduler.Register(name, access, [&execution_order, &order_mutex, name](World&, float) {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(name);
        });
    };

    // 三个系统都写 Shared → 必须按注册顺序串行执行
    make_system("first", true);
    make_system("second", true);
    make_system("third", true);

    scheduler.Execute(world, 0.016f);

    ASSERT_EQ(execution_order.size(), 3u);
    // 由于依赖图保证 i < j 时 j 依赖 i，执行顺序应为注册顺序
    EXPECT_EQ(execution_order[0], "first");
    EXPECT_EQ(execution_order[1], "second");
    EXPECT_EQ(execution_order[2], "third");
}

// 测试 混合冲突和无冲突
TEST_F(SystemSchedulerTest, MixedConflictsAndNonConflicts) {
    World world;
    SystemScheduler scheduler;

    struct A {}; struct B {}; struct C {};

    std::atomic<int> counter{0};

    // sys_a 和 sys_b 写不同组件 → 无冲突 → 可并行
    // sys_c 写 A → 与 sys_a 冲突，必须在 sys_a 后
    scheduler.Register("sys_a", {
        {},  // reads
        {std::type_index(typeid(A))}  // writes A
    }, [&counter](World&, float) {
        counter.fetch_add(1);
    });

    scheduler.Register("sys_b", {
        {},  // reads
        {std::type_index(typeid(B))}  // writes B
    }, [&counter](World&, float) {
        counter.fetch_add(10);
    });

    scheduler.Register("sys_c", {
        {std::type_index(typeid(A))},  // reads A
        {std::type_index(typeid(C))}   // writes C
    }, [&counter](World&, float) {
        counter.fetch_add(100);
    });

    scheduler.Execute(world, 0.016f);

    // 所有系统都执行了
    EXPECT_EQ(counter.load(), 111);
}
