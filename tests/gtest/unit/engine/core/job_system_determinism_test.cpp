/**
* @file job_system_determinism_test.cpp
* @brief JobSystem 确定性校验 harness
*
* 验证：同输入跑两遍，结果一致。用于保证并行化改动不破坏帧确定性。
* 测试策略：
* - 提交一组有依赖关系的任务，各任务写入独立位置
* - 两次运行使用相同的任务结构
* - 比较两次输出是否完全一致
* - 同时验证 ParallelFor 的确定性
*/

#include <gtest/gtest.h>
#include "engine/core/job_system.h"
#include <atomic>
#include <vector>
#include <numeric>
#include <random>

using namespace dse::core;

class JobSystemDeterminismTest : public ::testing::Test {
protected:
    JobSystem js_;
    void SetUp() override { js_.Init(); }
    void TearDown() override { js_.Shutdown(); }
};

// 测试 确定性：依赖链两次运行结果一致
TEST_F(JobSystemDeterminismTest, DependencyChainDeterministic) {
    constexpr int N = 100;

    auto run_once = [this]() -> std::vector<int> {
        std::vector<int> results(N, 0);
        std::atomic<int> counter{0};

        // 创建一条依赖链：每个任务依赖前一个
        JobHandle prev;
        for (int i = 0; i < N; ++i) {
            auto task = [&results, &counter, i]() {
                int val = counter.fetch_add(1);
                results[i] = val;
            };

            if (!prev.is_valid()) {
                prev = js_.Submit(task, JobPriority::Normal);
            } else {
                prev = js_.SubmitWithDependency(task, {prev}, JobPriority::Normal);
            }
        }
        js_.Wait(prev);
        return results;
    };

    auto run1 = run_once();
    auto run2 = run_once();

    // 两次运行的依赖链应该产生相同的执行顺序
    // （因为有严格的依赖链，每个任务必须等前一个完成）
    EXPECT_EQ(run1.size(), run2.size());
    for (size_t i = 0; i < run1.size(); ++i) {
        EXPECT_EQ(run1[i], run2[i]) << "Mismatch at index " << i;
    }
}

// 测试 确定性：ParallelFor 两次运行结果一致
TEST_F(JobSystemDeterminismTest, ParallelForDeterministic) {
    constexpr size_t N = 10000;

    auto run_once = [this]() -> std::vector<int> {
        std::vector<std::atomic<int>> results(N);
        for (auto& r : results) r.store(0);

        js_.ParallelFor(0, N, 64, [&results](size_t i) {
            results[i].store(static_cast<int>(i * 2 + 1));
        });

        std::vector<int> out(N);
        for (size_t i = 0; i < N; ++i) {
            out[i] = results[i].load();
        }
        return out;
    };

    auto run1 = run_once();
    auto run2 = run_once();

    EXPECT_EQ(run1.size(), run2.size());
    for (size_t i = 0; i < run1.size(); ++i) {
        EXPECT_EQ(run1[i], run2[i]) << "Mismatch at index " << i;
    }
}

// 测试 确定性：扇出/扇入两次运行结果一致
TEST_F(JobSystemDeterminismTest, FanOutFanInDeterministic) {
    constexpr int FANOUT = 20;

    auto run_once = [this]() -> int {
        std::atomic<int> sum{0};

        auto root = js_.Submit([]() {}, JobPriority::Normal);

        std::vector<JobHandle> children;
        children.reserve(FANOUT);
        for (int i = 0; i < FANOUT; ++i) {
            children.push_back(js_.SubmitWithDependency([&sum, i]() {
                sum.fetch_add(i + 1);
            }, {root}, JobPriority::Normal));
        }

        auto join = js_.SubmitWithDependency([&sum]() {
            sum.fetch_add(1000);
        }, children, JobPriority::High);

        js_.Wait(join);
        return sum.load();
    };

    int result1 = run_once();
    int result2 = run_once();

    // 扇出/扇入的总和应该是确定的（所有 i+1 之和 + 1000）
    int expected = 1000;
    for (int i = 0; i < FANOUT; ++i) expected += (i + 1);

    EXPECT_EQ(result1, expected);
    EXPECT_EQ(result2, expected);
}

// 测试 确定性：高并发无依赖任务结果一致
TEST_F(JobSystemDeterminismTest, HighConcurrencyNoDepsDeterministic) {
    constexpr int N = 500;

    auto run_once = [this]() -> std::vector<int> {
        std::vector<std::atomic<int>> results(N);
        for (auto& r : results) r.store(0);

        std::vector<JobHandle> handles;
        handles.reserve(N);
        for (int i = 0; i < N; ++i) {
            handles.push_back(js_.Submit([&results, i]() {
                results[i].store(i * i);
            }, JobPriority::Normal));
        }
        for (auto& h : handles) js_.Wait(h);

        std::vector<int> out(N);
        for (int i = 0; i < N; ++i) {
            out[i] = results[i].load();
        }
        return out;
    };

    auto run1 = run_once();
    auto run2 = run_once();

    EXPECT_EQ(run1.size(), run2.size());
    for (size_t i = 0; i < run1.size(); ++i) {
        EXPECT_EQ(run1[i], run2[i]) << "Mismatch at index " << i;
    }
}

// 测试 Worker 统计：忙/闲时间被记录
TEST_F(JobSystemDeterminismTest, WorkerStatsRecorded) {
    constexpr int N = 100;
    std::atomic<int> counter{0};

    std::vector<JobHandle> handles;
    for (int i = 0; i < N; ++i) {
        handles.push_back(js_.Submit([&counter]() {
            counter.fetch_add(1);
            // 做一些工作让忙时间可测量
            volatile int sum = 0;
            for (int j = 0; j < 1000; ++j) sum += j;
        }, JobPriority::Normal));
    }
    for (auto& h : handles) js_.Wait(h);

    EXPECT_EQ(counter.load(), N);

    // 验证 worker 统计被记录
    const auto& stats = js_.GetWorkerStats();
    EXPECT_FALSE(stats.empty());

    double total_busy = 0.0;
    uint64_t total_jobs = 0;
    for (const auto& s : stats) {
        total_busy += s.busy_time_ms;
        total_jobs += s.jobs_executed;
    }

    // 至少有任务被执行
    EXPECT_GT(total_jobs, 0u);
    // 忙时间应该 > 0
    EXPECT_GT(total_busy, 0.0);
}
