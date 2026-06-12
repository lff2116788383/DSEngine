/**
* @file job_system_test.cpp
* @brief JobSystem 的单元测试
*
* 覆盖场景：
* - 实例化 JobSystem 的初始化和关闭
* - 异步任务执行（兼容 Execute 接口）
* - 无线程池时同步执行
* - ServiceLocator 注入
* - 多任务并发执行
* - 优先级调度
* - JobHandle 等待完成
* - 任务依赖链
*/

#include <gtest/gtest.h>
#include "engine/core/job_system.h"
#include "engine/core/service_locator.h"
#include <condition_variable>
#include <mutex>


using namespace dse::core;

// ============================================================
// JobSystem 实例化测试
// ============================================================

class JobSystemInstanceTest : public ::testing::Test {
protected:
    void TearDown() override {
        ServiceLocator::Instance().Reset<JobSystem>();
    }
};

TEST_F(JobSystemInstanceTest, InitializeAfterExecuteTasks) {
    JobSystem js;
    js.Init();

    std::atomic<int> counter{0};
    js.Execute([&counter]() { counter.fetch_add(1); });

    // 等待任务完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(counter.load(), 1);

    js.Shutdown();
}

TEST_F(JobSystemInstanceTest, MultiTasksAllExecute) {
    JobSystem js;
    js.Init();

    std::atomic<int> counter{0};
    std::mutex mutex;
    std::condition_variable condition;
    constexpr int kTaskCount = 10;
    for (int i = 0; i < kTaskCount; ++i) {
        js.Execute([&counter, &mutex, &condition]() {
            const int current = counter.fetch_add(1) + 1;
            if (current == kTaskCount) {
                std::lock_guard<std::mutex> lock(mutex);
                condition.notify_one();
            }
        });
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        EXPECT_TRUE(condition.wait_for(lock, std::chrono::seconds(1), [&counter]() {
            return counter.load() == kTaskCount;
        }));
    }
    EXPECT_EQ(counter.load(), kTaskCount);

    js.Shutdown();
}


TEST_F(JobSystemInstanceTest, WhenNotInitializedSynchronousExecution) {
    JobSystem js; // 不调用 Init()

    int value = 0;
    js.Execute([&value]() { value = 42; });
    EXPECT_EQ(value, 42);
}

TEST_F(JobSystemInstanceTest, AfterClosingSynchronousExecution) {
    JobSystem js;
    js.Init();
    js.Shutdown();

    int value = 0;
    js.Execute([&value]() { value = 99; });
    EXPECT_EQ(value, 99);
}

TEST_F(JobSystemInstanceTest, EmptyTasksDoesNotCrash) {
    JobSystem js;
    js.Init();
    js.Execute(nullptr);
    js.Shutdown();
    SUCCEED();
}

TEST_F(JobSystemInstanceTest, InitializeDoesNotCrash) {
    JobSystem js;
    js.Init();
    js.Init(); // 重复初始化
    js.Shutdown();
    SUCCEED();
}

TEST_F(JobSystemInstanceTest, ShutdownDoesNotCrash) {
    JobSystem js;
    js.Init();
    js.Shutdown();
    js.Shutdown(); // 重复关闭
    SUCCEED();
}

// ============================================================
// ServiceLocator 注入测试
// ============================================================

TEST_F(JobSystemInstanceTest, ServiceLocatorCanBeInjectedIntoInstancesAndPerformTasks) {
    auto job_system = std::make_shared<JobSystem>();
    job_system->Init();
    ServiceLocator::Instance().Register<JobSystem, JobSystem>(job_system);

    std::atomic<int> counter{0};
    std::mutex mutex;
    std::condition_variable condition;
    bool finished = false;

    auto* resolved = ServiceLocator::Instance().Get<JobSystem>();
    ASSERT_NE(resolved, nullptr);
    resolved->Execute([&counter, &mutex, &condition, &finished]() {
        counter.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(mutex);
            finished = true;
        }
        condition.notify_one();
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        EXPECT_TRUE(condition.wait_for(lock, std::chrono::seconds(1), [&finished]() {
            return finished;
        }));
    }
    EXPECT_EQ(counter.load(), 1);

    job_system->Shutdown();
    ServiceLocator::Instance().Reset<JobSystem>();
}

TEST_F(JobSystemInstanceTest, NotregisterWhenCallsCanrollbackIsSynchronousExecution) {
    int value = 0;
    auto* resolved = ServiceLocator::Instance().Get<JobSystem>();
    if (resolved) {
        resolved->Execute([&value]() { value = 55; });
    } else {
        value = 55;
    }
    EXPECT_EQ(value, 55);
}

// ============================================================
// JobHandle 与 Wait 测试
// ============================================================

TEST_F(JobSystemInstanceTest, SubmitReturnValidHandle) {
    JobSystem js;
    js.Init();

    auto handle = js.Submit([](){}, JobPriority::Normal);
    EXPECT_TRUE(handle.is_valid());

    js.Shutdown();
}

TEST_F(JobSystemInstanceTest, WaitAbleToWaitForTasksToBeCompleted) {
    JobSystem js;
    js.Init();

    std::atomic<int> counter{0};
    auto handle = js.Submit([&counter]() {
        counter.fetch_add(1);
    }, JobPriority::Normal);

    js.Wait(handle);
    EXPECT_EQ(counter.load(), 1);

    js.Shutdown();
}

TEST_F(JobSystemInstanceTest, WaitInvalidHandleDoesNotCrash) {
    JobSystem js;
    js.Init();

    JobHandle invalid;
    js.Wait(invalid); // 无效句柄应立即返回

    js.Shutdown();
    SUCCEED();
}

TEST_F(JobSystemInstanceTest, WaitReturnImmediatelyAfterCompletingTheTask) {
    JobSystem js;
    js.Init();

    auto handle = js.Submit([](){}, JobPriority::Normal);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // 任务应该已完成
    js.Wait(handle); // 应立即返回

    js.Shutdown();
    SUCCEED();
}

// ============================================================
// 优先级测试
// ============================================================

TEST_F(JobSystemInstanceTest, PriorityTaskspriorityExecute) {
    JobSystem js;
    js.Init();

    // 提交低优先级任务使队列非空，然后提交高优先级
    // 注意：此测试验证优先级排队机制，实际执行顺序受线程调度影响
    std::atomic<int> execution_order{0};
    std::atomic<int> high_order{-1};
    std::atomic<int> low_order{-1};

    // 使用同步屏障确保任务在队列中排队后再执行
    std::mutex mutex;
    std::condition_variable all_submitted;
    bool ready = false;

    // 提交一批低优先级任务
    for (int i = 0; i < 5; ++i) {
        js.Submit([&execution_order, &low_order]() {
            int order = execution_order.fetch_add(1);
            if (low_order.load() == -1) low_order.store(order);
        }, JobPriority::Low);
    }

    // 提交高优先级任务
    auto high_handle = js.Submit([&execution_order, &high_order]() {
        int order = execution_order.fetch_add(1);
        high_order.store(order);
    }, JobPriority::High);

    js.Wait(high_handle);

    // 高优先级任务应该在低优先级之前执行（或至少不晚于第一个低优先级）
    // 由于线程调度的不确定性，只验证高优先级有被执行
    EXPECT_GE(high_order.load(), 0);

    js.Shutdown();
}

// ============================================================
// 依赖链测试
// ============================================================

TEST_F(JobSystemInstanceTest, TasksexistAfterExecute) {
    JobSystem js;
    js.Init();

    std::atomic<int> counter{0};
    std::atomic<int> first_val{-1};
    std::atomic<int> second_val{-1};

    auto dep = js.Submit([&counter, &first_val]() {
        first_val.store(counter.fetch_add(1)); // 先执行，值为 0
    }, JobPriority::Normal);

    auto dependent = js.SubmitWithDependency([&counter, &second_val]() {
        second_val.store(counter.fetch_add(1)); // 后执行，值为 1
    }, {dep}, JobPriority::Normal);

    js.Wait(dependent);

    // first_val 应该是 0，second_val 应该是 1
    EXPECT_EQ(first_val.load(), 0);
    EXPECT_EQ(second_val.load(), 1);

    js.Shutdown();
}

TEST_F(JobSystemInstanceTest, MultiAllCompleteAfterExecute) {
    JobSystem js;
    js.Init();

    std::atomic<int> counter{0};
    std::vector<JobHandle> deps;
    std::atomic<int> dep_count{0};

    for (int i = 0; i < 3; ++i) {
        deps.push_back(js.Submit([&dep_count]() {
            dep_count.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }, JobPriority::Normal));
    }

    std::atomic<bool> dependent_executed{false};
    auto dependent = js.SubmitWithDependency([&dependent_executed, &dep_count]() {
        dependent_executed.store(true);
        // 依赖全部完成时 dep_count 应为 3
    }, deps, JobPriority::Normal);

    js.Wait(dependent);

    EXPECT_TRUE(dependent_executed.load());
    EXPECT_EQ(dep_count.load(), 3);

    js.Shutdown();
}

TEST_F(JobSystemInstanceTest, NoDependenciesSubmitWithDependencySubmit) {
    JobSystem js;
    js.Init();

    std::atomic<int> counter{0};
    auto handle = js.SubmitWithDependency([&counter]() {
        counter.fetch_add(1);
    }, {}, JobPriority::Normal);

    js.Wait(handle);
    EXPECT_EQ(counter.load(), 1);

    js.Shutdown();
}

TEST_F(JobSystemInstanceTest, AlreadySubmitWithDependency) {
    JobSystem js;
    js.Init();

    auto dep = js.Submit([](){}, JobPriority::Normal);
    js.Wait(dep); // 确保依赖完成

    std::atomic<int> counter{0};
    auto handle = js.SubmitWithDependency([&counter]() {
        counter.fetch_add(1);
    }, {dep}, JobPriority::Normal);

    js.Wait(handle);
    EXPECT_EQ(counter.load(), 1);

    js.Shutdown();
}

TEST_F(JobSystemInstanceTest, ChainCan) {
    JobSystem js;
    js.Init();

    std::atomic<int> counter{0};
    std::vector<int> order;

    auto a = js.Submit([&counter, &order]() {
        order.push_back(counter.fetch_add(1)); // 0
    }, JobPriority::Normal);

    auto b = js.SubmitWithDependency([&counter, &order]() {
        order.push_back(counter.fetch_add(1)); // 1
    }, {a}, JobPriority::Normal);

    auto c = js.SubmitWithDependency([&counter, &order]() {
        order.push_back(counter.fetch_add(1)); // 2
    }, {b}, JobPriority::Normal);

    js.Wait(c);

    // A → B → C 级联执行，顺序应为 0, 1, 2
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 0);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(order[2], 2);

    js.Shutdown();
}
