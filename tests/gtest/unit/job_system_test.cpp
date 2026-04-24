/**
 * @file job_system_test.cpp
 * @brief JobSystem 的单元测试
 *
 * 覆盖场景：
 * - 实例化 JobSystem 的初始化和关闭
 * - 异步任务执行
 * - 无线程池时同步执行
 * - 静态兼容接口
 * - 多任务并发执行
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

TEST_F(JobSystemInstanceTest, 初始化后执行异步任务) {
    JobSystem js;
    js.Init();

    std::atomic<int> counter{0};
    js.Execute([&counter]() { counter.fetch_add(1); });

    // 等待任务完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(counter.load(), 1);

    js.Shutdown();
}

TEST_F(JobSystemInstanceTest, 多个异步任务全部执行) {
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


TEST_F(JobSystemInstanceTest, 未初始化时同步执行) {
    JobSystem js; // 不调用 Init()

    int value = 0;
    js.Execute([&value]() { value = 42; });
    EXPECT_EQ(value, 42);
}

TEST_F(JobSystemInstanceTest, 关闭后同步执行) {
    JobSystem js;
    js.Init();
    js.Shutdown();

    int value = 0;
    js.Execute([&value]() { value = 99; });
    EXPECT_EQ(value, 99);
}

TEST_F(JobSystemInstanceTest, 空任务不崩溃) {
    JobSystem js;
    js.Init();
    js.Execute(nullptr);
    js.Shutdown();
    SUCCEED();
}

TEST_F(JobSystemInstanceTest, 重复初始化不崩溃) {
    JobSystem js;
    js.Init();
    js.Init(); // 重复初始化
    js.Shutdown();
    SUCCEED();
}

TEST_F(JobSystemInstanceTest, 重复关闭不崩溃) {
    JobSystem js;
    js.Init();
    js.Shutdown();
    js.Shutdown(); // 重复关闭
    SUCCEED();
}

// ============================================================
// 静态兼容接口测试
// ============================================================

class JobSystemStaticTest : public ::testing::Test {
protected:
    void TearDown() override {
        // 确保清理
        auto* existing = ServiceLocator::Instance().Get<JobSystem>();
        if (existing) {
            existing->Shutdown();
        }
        ServiceLocator::Instance().Reset<JobSystem>();
    }
};

TEST_F(JobSystemStaticTest, InitStatic创建并注册JobSystem) {
    JobSystem::InitStatic();
    EXPECT_TRUE(ServiceLocator::Instance().Has<JobSystem>());

    std::atomic<int> counter{0};
    std::mutex mutex;
    std::condition_variable condition;
    bool finished = false;

    JobSystem::ExecuteStatic([&counter, &mutex, &condition, &finished]() {
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

    JobSystem::ShutdownStatic();
}


TEST_F(JobSystemStaticTest, ExecuteStatic无实例时同步执行) {
    // 不调用 InitStatic，直接 ExecuteStatic
    int value = 0;
    JobSystem::ExecuteStatic([&value]() { value = 55; });
    EXPECT_EQ(value, 55);
}

TEST_F(JobSystemStaticTest, ShutdownStatic清理ServiceLocator) {
    JobSystem::InitStatic();
    EXPECT_TRUE(ServiceLocator::Instance().Has<JobSystem>());

    JobSystem::ShutdownStatic();
    EXPECT_FALSE(ServiceLocator::Instance().Has<JobSystem>());
}
