/**
 * @file jobsystem_eventbus_integration_test.cpp
 * @brief JobSystem + EventBus 异步协作集成测试
 *
 * 验证场景：
 * - JobSystem 执行异步任务，完成后通过 EventBus 发布事件
 * - 多个异步任务完成后事件按序或按约束触发
 * - 依赖链：Job A 完成后触发事件，事件回调中提交 Job B
 * - 线程安全：EventBus 在多线程发布/订阅场景下的正确性
 * - JobSystem 生命周期与 ServiceLocator 注入
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/core/job_system.h"
#include "engine/core/event_bus.h"
#include "engine/core/event_id.h"
#include "engine/core/service_locator.h"
#include <atomic>
#include <chrono>
#include <vector>
#include <mutex>

using namespace dse::core;

// ============================================================
// 测试用事件
// ============================================================

struct JobCompletedEvent : public Event {
    explicit JobCompletedEvent(int job_id) : job_id(job_id) {}
    int job_id = 0;
    static constexpr EventId kEventId = MakeEventId("JobCompletedEvent");
};

struct DataReadyEvent : public Event {
    explicit DataReadyEvent(std::vector<int> data) : data(std::move(data)) {}
    std::vector<int> data;
    static constexpr EventId kEventId = MakeEventId("DataReadyEvent");
};

// ============================================================
// JobSystem + EventBus 异步协作
// ============================================================

class JobSystemEventBusIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        job_system_ = std::make_shared<JobSystem>();
        job_system_->Init();
        event_bus_ = std::make_shared<EventBus>();

        ServiceLocator::Instance().Register<JobSystem, JobSystem>(job_system_);
        ServiceLocator::Instance().Register<EventBus, EventBus>(event_bus_);
    }

    void TearDown() override {
        job_system_->Shutdown();
        ServiceLocator::Instance().Reset<JobSystem>();
        ServiceLocator::Instance().Reset<EventBus>();
    }

    std::shared_ptr<JobSystem> job_system_;
    std::shared_ptr<EventBus> event_bus_;
};

TEST_F(JobSystemEventBusIntegrationTest, TasksAfterPublishesevent) {
    std::atomic<int> event_received{0};

    event_bus_->Subscribe<JobCompletedEvent>([&event_received](const JobCompletedEvent& e) {
        event_received.store(e.job_id);
    });

    auto handle = job_system_->Submit([this]() {
        // 模拟异步计算
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        event_bus_->Publish<JobCompletedEvent>(42);
    }, JobPriority::Normal);

    job_system_->Wait(handle);

    // 等待事件传播（EventBus 发布是同步的，在 Job 线程中执行）
    EXPECT_EQ(event_received.load(), 42);
}

TEST_F(JobSystemEventBusIntegrationTest, MultiTasksAfterPublishesevent) {
    std::atomic<int> event_count{0};
    std::mutex vec_mutex;
    std::vector<int> completed_job_ids;

    event_bus_->Subscribe<JobCompletedEvent>([&](const JobCompletedEvent& e) {
        std::lock_guard<std::mutex> lock(vec_mutex);
        completed_job_ids.push_back(e.job_id);
        event_count.fetch_add(1);
    });

    constexpr int kJobCount = 10;
    std::vector<JobHandle> handles;
    for (int i = 0; i < kJobCount; ++i) {
        handles.push_back(job_system_->Submit([this, i]() {
            event_bus_->Publish<JobCompletedEvent>(i);
        }, JobPriority::Normal));
    }

    for (auto& h : handles) {
        job_system_->Wait(h);
    }

    // 所有事件应已收到
    EXPECT_EQ(event_count.load(), kJobCount);
    EXPECT_EQ(completed_job_ids.size(), static_cast<size_t>(kJobCount));
}

TEST_F(JobSystemEventBusIntegrationTest, ChainTasksAfterPublishesevent) {
    std::atomic<int> final_result{0};

    event_bus_->Subscribe<DataReadyEvent>([&final_result](const DataReadyEvent& e) {
        int sum = 0;
        for (int v : e.data) { sum += v; }
        final_result.store(sum);
    });

    // Job A：生成数据
    auto job_a = job_system_->Submit([this]() {
        // 模拟数据准备
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }, JobPriority::High);

    // Job B：依赖 A，处理数据并发布事件
    auto job_b = job_system_->SubmitWithDependency([this]() {
        std::vector<int> data{1, 2, 3, 4, 5};
        event_bus_->Publish<DataReadyEvent>(std::move(data));
    }, {job_a}, JobPriority::Normal);

    job_system_->Wait(job_b);

    // 1+2+3+4+5 = 15
    EXPECT_EQ(final_result.load(), 15);
}

TEST_F(JobSystemEventBusIntegrationTest, SubmitANewTaskInTheEventCallback) {
    std::atomic<int> chain_result{0};

    event_bus_->Subscribe<JobCompletedEvent>([&](const JobCompletedEvent& e) {
        // 在事件回调中提交新的 Job
        auto* js = ServiceLocator::Instance().Get<JobSystem>();
        if (js) {
            js->Submit([&chain_result, e]() {
                chain_result.store(e.job_id * 10);
            }, JobPriority::High);
        }
    });

    auto handle = job_system_->Submit([this]() {
        event_bus_->Publish<JobCompletedEvent>(7);
    });

    job_system_->Wait(handle);

    // 等待链式任务完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 7 * 10 = 70
    EXPECT_EQ(chain_result.load(), 70);
}

// ============================================================
// 线程安全性
// ============================================================

TEST_F(JobSystemEventBusIntegrationTest, MultiPublisheseventDoesNotCrash) {
    std::atomic<int> total_events{0};

    event_bus_->Subscribe<JobCompletedEvent>([&total_events](const JobCompletedEvent&) {
        total_events.fetch_add(1);
    });

    constexpr int kConcurrentJobs = 50;
    std::vector<JobHandle> handles;
    for (int i = 0; i < kConcurrentJobs; ++i) {
        handles.push_back(job_system_->Submit([this, i]() {
            event_bus_->Publish<JobCompletedEvent>(i);
        }, JobPriority::Normal));
    }

    for (auto& h : handles) {
        job_system_->Wait(h);
    }

    EXPECT_EQ(total_events.load(), kConcurrentJobs);
}

// ============================================================
// 通过 ServiceLocator 协作
// ============================================================

TEST_F(JobSystemEventBusIntegrationTest, PassServiceLocatorAcquire) {
    auto* js = ServiceLocator::Instance().Get<JobSystem>();
    auto* bus = ServiceLocator::Instance().Get<EventBus>();

    ASSERT_NE(js, nullptr);
    ASSERT_NE(bus, nullptr);

    std::atomic<bool> event_fired{false};
    bus->Subscribe<JobCompletedEvent>([&event_fired](const JobCompletedEvent&) {
        event_fired.store(true);
    });

    auto handle = js->Submit([bus]() {
        bus->Publish<JobCompletedEvent>(1);
    });

    js->Wait(handle);
    EXPECT_TRUE(event_fired.load());
}

// ============================================================
// 优先级任务与事件交互
// ============================================================

TEST_F(JobSystemEventBusIntegrationTest, PriorityTasksPublishesevent) {
    std::vector<int> event_order;
    std::mutex order_mutex;

    event_bus_->Subscribe<JobCompletedEvent>([&](const JobCompletedEvent& e) {
        std::lock_guard<std::mutex> lock(order_mutex);
        event_order.push_back(e.job_id);
    });

    // 提交多个优先级不同的任务
    job_system_->Submit([this]() {
        event_bus_->Publish<JobCompletedEvent>(1); // Low
    }, JobPriority::Low);

    job_system_->Submit([this]() {
        event_bus_->Publish<JobCompletedEvent>(2); // High
    }, JobPriority::High);

    job_system_->Submit([this]() {
        event_bus_->Publish<JobCompletedEvent>(3); // Normal
    }, JobPriority::Normal);

    // 给足够时间让任务完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 所有事件都应收到
    EXPECT_EQ(event_order.size(), 3u);
}
