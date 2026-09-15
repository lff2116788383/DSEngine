/**
 * @file perf_probe.h
 * @brief 稳定性计时探针 —— 用 min-of-N 取代「单次采样」，消除性能门禁的随机红灯。
 *
 * 背景（本仓库实测）：
 * performance_baseline_test / profiler_test / anim_perf_benchmark_test 三处共 14 条墙钟
 * 阈值断言全部基于「单次采样」。在共享机器上（旁边还有编译进程、杀毒扫描、其他会话），
 * 单次采样会被无关负载放大 10~100 倍。实测连续跑 4 次 unit 套件得到 3 组互不相同的失败
 * 集合，而把这十几个用例单独跑又全绿 —— 门禁在「随机变红」。
 *
 * 一个会随机变红的门禁比没有门禁更糟：它训练所有人忽略红色。这类阈值断言的目标是
 * 「检测 10x+ 级退化」，而不是测量绝对性能，所以正确做法是取多次运行的最小值：
 *
 *   - 最小值逼近机器在当前状态下「可达到的最快速度」—— 调度噪声只会让某几次变慢，
 *     不会让所有次都变快；
 *   - 真实退化（算法复杂度变差、被误加锁、被误加的逐帧分配等）会让**每一次**都变慢，
 *     最小值同样随之变大；
 *   - 故 min-of-N 既保住灵敏度，又消掉随机红。
 *
 * 注意：取最小值只对「可重复执行的纯操作」成立。所以调用方应把 setup 放在探测之外，
 * 只把被测操作本身放进 lambda（见各用例用法）。
 *
 * 用法：
 *   const int attempts = dse::test::PerfAttempts(5);
 *   const auto s = dse::test::ProbeMillis(attempts, [&] { DoWork(); });
 *   EXPECT_LT(s.best_ms, 50.0) << dse::test::SampleSummary(s, 50.0, "ECS 迭代");
 *
 * 环境变量：DSE_PERF_ATTEMPTS 可覆盖尝试次数（CI 机器噪声大时可调大，例如 9）；
 *            设为 1 则退回旧的「单次采样」行为，便于做 A/B 对比。
 */

#ifndef DSE_TESTS_GTEST_SUPPORT_PERF_PROBE_H
#define DSE_TESTS_GTEST_SUPPORT_PERF_PROBE_H

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace dse {
namespace test {

/// 单调时钟：墙钟阈值断言必须用 steady_clock，避免系统时间调整造成负值/跳变。
using PerfClock = std::chrono::steady_clock;

/// 一次探测的全部样本，附带 best/worst/median 便于失败时诊断。
struct PerfSamples {
    std::vector<double> millis;  ///< 每次运行的耗时（ms），顺序 = 运行顺序
    double best_ms = 0.0;        ///< 最小值 —— 断言应基于此值
    double worst_ms = 0.0;       ///< 最大值，仅用于诊断
    double median_ms = 0.0;      ///< 中位数，仅用于诊断
};

/// 尝试次数：默认值可被环境变量 DSE_PERF_ATTEMPTS 覆盖（用于噪声大的 CI）。
inline int PerfAttempts(int default_attempts) {
    if (const char* env = std::getenv("DSE_PERF_ATTEMPTS")) {
        const int v = std::atoi(env);
        if (v > 0) return v;
    }
    return default_attempts < 1 ? 1 : default_attempts;
}

/// 运行 fn() attempts 次，返回每次耗时与 best/worst/median（单位 ms）。
template <typename Fn>
PerfSamples ProbeMillis(int attempts, Fn&& fn) {
    if (attempts < 1) attempts = 1;

    PerfSamples s;
    s.millis.reserve(static_cast<std::size_t>(attempts));
    for (int i = 0; i < attempts; ++i) {
        const auto t0 = PerfClock::now();
        fn();
        const auto t1 = PerfClock::now();
        s.millis.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    std::vector<double> sorted = s.millis;
    std::sort(sorted.begin(), sorted.end());
    s.best_ms = sorted.front();
    s.worst_ms = sorted.back();
    s.median_ms = sorted[sorted.size() / 2];
    return s;
}

/// 便利包装：best-of-N 的毫秒数（用于只关心最快一次的场景）。
template <typename Fn>
double BestMillis(int attempts, Fn&& fn) {
    return ProbeMillis(attempts, std::forward<Fn>(fn)).best_ms;
}

/// 便利包装：best-of-N 的微秒数（整数，便于与既有 us 阈值对齐）。
template <typename Fn>
long long BestMicros(int attempts, Fn&& fn) {
    const double ms = ProbeMillis(attempts, std::forward<Fn>(fn)).best_ms;
    return static_cast<long long>(ms * 1000.0 + 0.5);
}

/// 失败信息：一份足够分清「真退化」还是「机器被压满」的诊断。
/// 若 worst 远大于 best（例如 5 倍以上），基本可以判定是调度噪声而非回归。
inline std::string SampleSummary(const PerfSamples& s, double budget_ms, const char* what) {
    char buf[360];
    std::snprintf(buf, sizeof(buf),
                  "%s: best=%.3f ms / median=%.3f / worst=%.3f over %zu run(s); budget=%.3f ms. "
                  "best 超预算才是真退化；worst 远大于 best 说明是机器负载噪声，"
                  "可用环境变量 DSE_PERF_ATTEMPTS 调大尝试次数。",
                  what ? what : "perf", s.best_ms, s.median_ms, s.worst_ms, s.millis.size(), budget_ms);
    return std::string(buf);
}

/// 打印一行 [PERF] 摘要，保持与既有用例输出风格一致（便于人工比对历史数字）。
/// scale_per_run > 1 时按「每次运行摊到的单位数」折算（例如每次跑 100 次迭代就传 100）。
inline void PrintPerf(const char* what, const PerfSamples& s, double scale_per_run = 1.0) {
    if (scale_per_run <= 0.0) scale_per_run = 1.0;
    std::printf("[PERF] %s: best %.4f (best-of-%zu), median %.4f, worst %.4f\n",
                what ? what : "perf", s.best_ms / scale_per_run, s.millis.size(),
                s.median_ms / scale_per_run, s.worst_ms / scale_per_run);
    std::fflush(stdout);
}

}  // namespace test
}  // namespace dse

#endif  // DSE_TESTS_GTEST_SUPPORT_PERF_PROBE_H
