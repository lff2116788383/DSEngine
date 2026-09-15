/**
 * @file anim_perf_benchmark_test.cpp
 * @brief 动画系统性能基准测试
 *
 * 测量场景：
 * - AnimLayerBlendSystem::Update N 个实体 × M 层
 * - IKSolverSystem::Update N 个实体 × K 条链
 * - anim_clip_eval Interpolate 吞吐量
 *
 * 注: 这些测试仅验证性能不退化 (执行时间在合理范围)，
 *     不含视觉正确性验证 (由 anim_layer_ik_test.cpp 覆盖)。
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>

#include "tests/gtest/support/perf_probe.h"

#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_3d/animation/anim_layer_blend_system.h"
#include "modules/gameplay_3d/animation/ik_solver_system.h"
#include "modules/gameplay_3d/animation/anim_clip_eval.h"

using namespace dse;
using namespace gameplay3d;

// ============================================================
// 计时：统一走 tests/gtest/support/perf_probe.h 的 best-of-N 探针
// ============================================================
// 此前这里是一个单次采样的 ScopedTimer。单次采样在共享机器上会被无关负载放大
// 10~100 倍，实测本套件曾随机变红（同一个用例单独跑却全绿）。现全部改为 best-of-N：
// 阈值基于最快一次；真实退化会让每一次都变慢，故仍会被抓住。
//
// 阈值标定（测量时机器已被无关进程压到 80~100% CPU，故数值偏保守）：
//   LayerBlend 100x3  基线 0.023 ms/iter，实测最差 0.155 -> 阈值 1.0（约 43x 基线）
//   IKSolver  100x2   基线 0.024 ms/iter，实测最差 0.255 -> 阈值 2.0（约 84x 基线）
//   Interpolate vec3  基线 12.8 ms -> 阈值 200 ms
//   Interpolate quat  基线 19.7 ms -> 阈值 250 ms
//   AdvanceClipTime   基线 4.35 ms -> 阈值 150 ms
// 两个 ms/iter 指标的测量窗口只有约 2.4 ms，噪声占比天然很大，故余量给得最宽；
// 它们仍能抓住 40 倍以上的真退化。窗口短的用例用 15 次尝试，其余用 5 次。

// ============================================================
// AnimLayerBlendSystem 批量实体性能
// ============================================================

// 测试 动画性能基准：层混合更新情形100实体情形3层
TEST(AnimPerfBenchmark, LayerBlendUpdate_Case100Entity_Case3layer) {
    World world;
    AnimLayerBlendSystem::SetAssetManager(nullptr);

    constexpr int N = 100;
    constexpr int LAYERS = 3;

    for (int i = 0; i < N; ++i) {
        auto e = world.CreateEntity();
        auto& anim = world.registry().emplace<Animator3DComponent>(e);
        anim.enabled = true;
        anim.skel_cache.valid = false;
        auto& lc = world.registry().emplace<AnimLayerComponent>(e);
        for (int j = 0; j < LAYERS; ++j) {
            AnimLayerConfig layer;
            layer.name = "layer_" + std::to_string(j);
            layer.weight = 0.5f;
            layer.source_type = AnimSourceType::SingleClip;
            lc.layers.push_back(std::move(layer));
        }
    }

    // 预热
    AnimLayerBlendSystem::Update(world, 0.016f);

    // 计时：每次尝试跑 ITERS 次 Update，取 best-of-N 后再折算 ms/iter。
    constexpr int ITERS = 100;
    const auto s = dse::test::ProbeMillis(dse::test::PerfAttempts(15), [&] {
        for (int i = 0; i < ITERS; ++i) {
            AnimLayerBlendSystem::Update(world, 0.016f);
        }
    });
    const double per_iter = s.best_ms / ITERS;

    dse::test::PrintPerf("LayerBlend 100 entities x 3 layers (ms/iter)", s, ITERS);

    // 合理性断言: 100 个无资产实体 best-of-N 应在 1.0 ms/iter 内 (无实际混合计算)
    EXPECT_LT(per_iter, 1.0)
        << dse::test::SampleSummary(s, 1.0 * ITERS, "LayerBlend 100 entities x 3 layers");
}

// ============================================================
// IKSolverSystem 批量实体性能
// ============================================================

// 测试 动画性能基准：IK Solver更新情形100实体情形2链
TEST(AnimPerfBenchmark, IKSolverUpdate_Case100Entity_Case2chain) {
    World world;

    constexpr int N = 100;
    constexpr int CHAINS = 2;

    for (int i = 0; i < N; ++i) {
        auto e = world.CreateEntity();
        auto& anim = world.registry().emplace<Animator3DComponent>(e);
        anim.enabled = true;
        anim.skel_cache.valid = false;
        auto& ik = world.registry().emplace<IKChain3DComponent>(e);
        for (int j = 0; j < CHAINS; ++j) {
            IKChainConfig chain;
            chain.name = "chain_" + std::to_string(j);
            chain.type = IKChainType::FABRIK;
            chain.root_bone = "Root";
            chain.tip_bone = "Tip";
            chain.iterations = 10;
            ik.chains.push_back(std::move(chain));
        }
    }

    // 预热
    IKSolverSystem::Update(world, 0.016f);

    // 计时：每次尝试跑 ITERS 次 Update，取 best-of-N 后再折算 ms/iter。
    constexpr int ITERS = 100;
    const auto s = dse::test::ProbeMillis(dse::test::PerfAttempts(15), [&] {
        for (int i = 0; i < ITERS; ++i) {
            IKSolverSystem::Update(world, 0.016f);
        }
    });
    const double per_iter = s.best_ms / ITERS;

    dse::test::PrintPerf("IKSolver 100 entities x 2 chains (ms/iter)", s, ITERS);

    EXPECT_LT(per_iter, 2.0)
        << dse::test::SampleSummary(s, 2.0 * ITERS, "IKSolver 100 entities x 2 chains");
}

// ============================================================
// anim_clip_eval::Interpolate 吞吐量
// ============================================================

// 测试 动画性能基准：插值向量3情形100 K次数
TEST(AnimPerfBenchmark, Interpolate_Vec3_Case100KTimes) {
    constexpr int N = 100000;

    std::vector<float> times = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};
    std::vector<glm::vec3> values = {
        {0,0,0}, {1,2,3}, {4,5,6}, {7,8,9}, {10,11,12}
    };

    volatile float sink = 0.0f;
    const auto s = dse::test::ProbeMillis(dse::test::PerfAttempts(5), [&] {
        for (int i = 0; i < N; ++i) {
            float t = static_cast<float>(i % 200) * 0.01f;
            auto r = anim_util::Interpolate<glm::vec3>(times, values, t);
            sink += r.x;
        }
    });

    dse::test::PrintPerf("Interpolate vec3 100K calls (ms)", s);
    std::cout << "[PERF] Interpolate vec3 throughput: "
              << (N / s.best_ms * 1000.0) << " calls/sec (best-of-N)" << std::endl;

    // 100K 次插值 best-of-N 应在 200 ms 内（实测基线约 12.8 ms）
    EXPECT_LT(s.best_ms, 200.0)
        << dse::test::SampleSummary(s, 200.0, "Interpolate vec3 100K");
    (void)sink;
}

// 测试 动画性能基准：插值Quat情形100 K次数
TEST(AnimPerfBenchmark, Interpolate_Quat_Case100KTimes) {
    constexpr int N = 100000;

    std::vector<float> times = {0.0f, 1.0f, 2.0f};
    glm::quat q0(1, 0, 0, 0);
    glm::quat q1 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    glm::quat q2 = glm::angleAxis(glm::radians(180.0f), glm::vec3(0, 1, 0));
    std::vector<glm::quat> values = {q0, q1, q2};

    volatile float sink = 0.0f;
    const auto s = dse::test::ProbeMillis(dse::test::PerfAttempts(5), [&] {
        for (int i = 0; i < N; ++i) {
            float t = static_cast<float>(i % 200) * 0.01f;
            auto r = anim_util::Interpolate<glm::quat>(times, values, t);
            sink += r.w;
        }
    });

    dse::test::PrintPerf("Interpolate quat 100K calls (ms)", s);
    std::cout << "[PERF] Interpolate quat throughput: "
              << (N / s.best_ms * 1000.0) << " calls/sec (best-of-N)" << std::endl;

    // 实测基线约 19.7 ms
    EXPECT_LT(s.best_ms, 250.0)
        << dse::test::SampleSummary(s, 250.0, "Interpolate quat 100K");
    (void)sink;
}

// 测试 动画性能基准：推进剪辑时间情形1 M次数
TEST(AnimPerfBenchmark, AdvanceClipTime_Case1MTimes) {
    constexpr int N = 1000000;

    volatile float sink = 0.0f;
    const auto s = dse::test::ProbeMillis(dse::test::PerfAttempts(15), [&] {
        for (int i = 0; i < N; ++i) {
            float t = anim_util::AdvanceClipTime(
                static_cast<float>(i % 100) * 0.01f, 0.016f, 30.0f, 2.0f, true);
            sink += t;
        }
    });

    dse::test::PrintPerf("AdvanceClipTime 1M calls (ms)", s);
    std::cout << "[PERF] AdvanceClipTime throughput: "
              << (N / s.best_ms * 1000.0) << " calls/sec (best-of-N)" << std::endl;

    // 1M 次 best-of-N 应在 150 ms 内（实测基线约 4.35 ms）
    EXPECT_LT(s.best_ms, 150.0)
        << dse::test::SampleSummary(s, 150.0, "AdvanceClipTime 1M");
    (void)sink;
}
