/**
 * @file job_system_conservation_test.cpp
 * @brief JobSystem 工作守恒回归测试：每个提交的 job 必须恰好执行一次。
 *
 * 由来：performance_baseline_test 的 JobSystem 吞吐用例被改成 best-of-N 并跑 15 轮后，
 * 捕获到 counter 少 2（15000 提交只执行 14998）。也就是说  丢任务且 Wait() 报完成。
 * 该用例原先只跑 1 轮 1000 个 job，丢 1/100000 的概率基本测不出来；本文件用高轮次把它
 * 变成确定性可复现的门禁。
 *
 * 断言的是「守恒性」而非耗时：这类 bug 与机器负载无关，任何一次出现都是真 bug。
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <vector>

#include "engine/core/job_system.h"

using dse::core::JobHandle;
using dse::core::JobPriority;
using dse::core::JobSystem;

namespace {

/// 提交 kJobs，等全部完成，返回「执行次数 != 1」的 job 数。
/// 同时把异常样本打印出来（未执行 / 重复执行），便于定位。
int RunRound(JobSystem& js, int round, std::vector<std::atomic<int>>& ran) {
    const int kJobs = static_cast<int>(ran.size());
    for (auto& a : ran) a.store(0, std::memory_order_relaxed);

    std::vector<JobHandle> handles;
    handles.reserve(static_cast<std::size_t>(kJobs));
    for (int i = 0; i < kJobs; ++i) {
        handles.push_back(js.Submit([&ran, i] {
            ran[static_cast<std::size_t>(i)].fetch_add(1, std::memory_order_relaxed);
        }, JobPriority::Normal));
    }

    for (auto& h : handles) js.Wait(h);

    int bad = 0;
    for (int i = 0; i < kJobs; ++i) {
        const int c = ran[static_cast<std::size_t>(i)].load(std::memory_order_relaxed);
        if (c != 1) {
            ++bad;
            if (bad <= 8) {
                std::printf("[CONSERVATION] round=%d job=%d executed=%d handle_valid=%d\n", round, i, c,
                            handles[static_cast<std::size_t>(i)].is_valid() ? 1 : 0);
                std::fflush(stdout);
            }
        }
    }
    return bad;
}

}  // namespace

// 高强度守恒检查：100 轮 x 1000 job = 100000 个 job，全部必须恰好执行一次。
TEST(JobSystemConservation, EverySubmittedJobRunsExactlyOnce) {
    JobSystem js;
    js.Init();

    constexpr int kRounds = 100;
    constexpr int kJobs = 1000;
    std::vector<std::atomic<int>> ran(kJobs);

    int bad_total = 0;
    for (int r = 0; r < kRounds; ++r) {
        bad_total += RunRound(js, r, ran);
    }

    js.Shutdown();
    EXPECT_EQ(bad_total, 0) << "丢失/重复执行的 job 总数（100 轮 x 1000 job）";
}
