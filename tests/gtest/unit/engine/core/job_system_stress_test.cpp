/**
 * @file job_system_stress_test.cpp
 * @brief JobSystem 高并发压力测试
 *
 * 测试策略：
 * - 大量任务并发提交 & 等待
 * - 工作窃取正确性
 * - 依赖链扇出/扇入
 * - 高优先级饥饿检测
 * - 反复 Init/Shutdown 稳定性
 */

#include <gtest/gtest.h>
#include "engine/core/job_system.h"
#include <atomic>
#include <vector>
#include <numeric>

using namespace dse::core;

class JobSystemStressTest : public ::testing::Test {
protected:
    JobSystem js_;
    void SetUp() override { js_.Init(); }
    void TearDown() override { js_.Shutdown(); }
};

// 测试 任务系统压力：情形100全部任务完成
TEST_F(JobSystemStressTest, Case100AllTasksComplete) {
    constexpr int N = 100;
    std::atomic<int> counter{0};
    std::vector<JobHandle> handles;
    handles.reserve(N);
    for (int i = 0; i < N; ++i) {
        handles.push_back(js_.Submit([&counter]() {
            counter.fetch_add(1);
        }, JobPriority::Normal));
    }
    for (auto& h : handles) js_.Wait(h);
    EXPECT_EQ(counter.load(), N);
}

// 测试 任务系统压力：情形1000全部任务完成
TEST_F(JobSystemStressTest, Case1000AllTasksComplete) {
    constexpr int N = 1000;
    std::atomic<int> counter{0};
    std::vector<JobHandle> handles;
    handles.reserve(N);
    for (int i = 0; i < N; ++i) {
        handles.push_back(js_.Submit([&counter]() {
            counter.fetch_add(1);
        }, JobPriority::Normal));
    }
    for (auto& h : handles) js_.Wait(h);
    EXPECT_EQ(counter.load(), N);
}

// 测试 任务系统压力：优先级全部完成
TEST_F(JobSystemStressTest, PriorityAllComplete) {
    constexpr int N = 300;
    std::atomic<int> counter{0};
    std::vector<JobHandle> handles;
    handles.reserve(N);
    for (int i = 0; i < N; ++i) {
        JobPriority p = (i % 3 == 0) ? JobPriority::High
                      : (i % 3 == 1) ? JobPriority::Normal
                                      : JobPriority::Low;
        handles.push_back(js_.Submit([&counter]() {
            counter.fetch_add(1);
        }, p));
    }
    for (auto& h : handles) js_.Wait(h);
    EXPECT_EQ(counter.load(), N);
}

// 测试 任务系统压力：情形多任务单个之前
TEST_F(JobSystemStressTest, Case_MultiTasksOneBefore) {
    std::atomic<int> counter{0};
    auto root = js_.Submit([&counter]() {
        counter.fetch_add(1);
    }, JobPriority::High);

    constexpr int FANOUT = 50;
    std::vector<JobHandle> children;
    children.reserve(FANOUT);
    for (int i = 0; i < FANOUT; ++i) {
        children.push_back(js_.SubmitWithDependency([&counter]() {
            counter.fetch_add(1);
        }, {root}, JobPriority::Normal));
    }
    for (auto& h : children) js_.Wait(h);
    EXPECT_EQ(counter.load(), 1 + FANOUT);
}

// 测试 任务系统压力：情形单个任务多之前
TEST_F(JobSystemStressTest, Case_OneTasksMultiBefore) {
    constexpr int FANIN = 20;
    std::atomic<int> counter{0};
    std::vector<JobHandle> deps;
    deps.reserve(FANIN);
    for (int i = 0; i < FANIN; ++i) {
        deps.push_back(js_.Submit([&counter]() {
            counter.fetch_add(1);
        }, JobPriority::Normal));
    }
    auto final_handle = js_.SubmitWithDependency([&counter]() {
        counter.fetch_add(100);
    }, deps, JobPriority::High);

    js_.Wait(final_handle);
    EXPECT_EQ(counter.load(), FANIN + 100);
}

// 测试 任务系统压力：链
TEST_F(JobSystemStressTest, Chain) {
    constexpr int CHAIN = 50;
    std::atomic<int> counter{0};
    JobHandle prev;
    for (int i = 0; i < CHAIN; ++i) {
        if (!prev.is_valid()) {
            prev = js_.Submit([&counter]() { counter.fetch_add(1); }, JobPriority::Normal);
        } else {
            prev = js_.SubmitWithDependency([&counter]() {
                counter.fetch_add(1);
            }, {prev}, JobPriority::Normal);
        }
    }
    js_.Wait(prev);
    EXPECT_EQ(counter.load(), CHAIN);
}

// ============================================================
// 反复 Init/Shutdown
// ============================================================

// 测试 任务系统Stability：初始化关闭无泄漏
TEST(JobSystemStabilityTest, InitShutdownNoLeak) {
    for (int i = 0; i < 10; ++i) {
        JobSystem js;
        js.Init();
        std::atomic<int> val{0};
        auto h = js.Submit([&val]() { val.fetch_add(1); });
        js.Wait(h);
        EXPECT_EQ(val.load(), 1);
        js.Shutdown();
    }
}

// 测试 任务系统Stability：关闭之后提交同步执行
TEST(JobSystemStabilityTest, ShutdownAfterSubmitSynchronousExecution) {
    JobSystem js;
    js.Init();
    js.Shutdown();

    std::atomic<int> val{0};
    js.Execute([&val]() { val.fetch_add(1); });
    EXPECT_EQ(val.load(), 1);
}
