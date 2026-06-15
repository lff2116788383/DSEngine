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

#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_3d/animation/anim_layer_blend_system.h"
#include "modules/gameplay_3d/animation/ik_solver_system.h"
#include "modules/gameplay_3d/animation/anim_clip_eval.h"

using namespace dse;
using namespace gameplay3d;

// ============================================================
// Helper: 高精度计时
// ============================================================
class ScopedTimer {
public:
    ScopedTimer() : start_(std::chrono::high_resolution_clock::now()) {}
    double elapsed_ms() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }
private:
    std::chrono::high_resolution_clock::time_point start_;
};

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

    // 计时
    ScopedTimer timer;
    constexpr int ITERS = 100;
    for (int i = 0; i < ITERS; ++i) {
        AnimLayerBlendSystem::Update(world, 0.016f);
    }
    double total_ms = timer.elapsed_ms();
    double per_iter = total_ms / ITERS;

    std::cout << "[PERF] LayerBlend " << N << " entities x " << LAYERS
              << " layers: " << per_iter << " ms/iter ("
              << total_ms << " ms total)" << std::endl;

    // 合理性断言: 100 个无资产实体应在 5ms 内 (无实际混合计算)
    EXPECT_LT(per_iter, 5.0);
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

    ScopedTimer timer;
    constexpr int ITERS = 100;
    for (int i = 0; i < ITERS; ++i) {
        IKSolverSystem::Update(world, 0.016f);
    }
    double total_ms = timer.elapsed_ms();
    double per_iter = total_ms / ITERS;

    std::cout << "[PERF] IKSolver " << N << " entities x " << CHAINS
              << " chains: " << per_iter << " ms/iter ("
              << total_ms << " ms total)" << std::endl;

    EXPECT_LT(per_iter, 5.0);
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
    ScopedTimer timer;
    for (int i = 0; i < N; ++i) {
        float t = static_cast<float>(i % 200) * 0.01f;
        auto r = anim_util::Interpolate<glm::vec3>(times, values, t);
        sink += r.x;
    }
    double total_ms = timer.elapsed_ms();

    std::cout << "[PERF] Interpolate vec3 " << N << " calls: "
              << total_ms << " ms ("
              << (N / total_ms * 1000.0) << " calls/sec)" << std::endl;

    // 100K 次插值应在 50ms 内
    EXPECT_LT(total_ms, 50.0);
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
    ScopedTimer timer;
    for (int i = 0; i < N; ++i) {
        float t = static_cast<float>(i % 200) * 0.01f;
        auto r = anim_util::Interpolate<glm::quat>(times, values, t);
        sink += r.w;
    }
    double total_ms = timer.elapsed_ms();

    std::cout << "[PERF] Interpolate quat " << N << " calls: "
              << total_ms << " ms ("
              << (N / total_ms * 1000.0) << " calls/sec)" << std::endl;

    EXPECT_LT(total_ms, 100.0);
    (void)sink;
}

// 测试 动画性能基准：推进剪辑时间情形1 M次数
TEST(AnimPerfBenchmark, AdvanceClipTime_Case1MTimes) {
    constexpr int N = 1000000;

    volatile float sink = 0.0f;
    ScopedTimer timer;
    for (int i = 0; i < N; ++i) {
        float t = anim_util::AdvanceClipTime(
            static_cast<float>(i % 100) * 0.01f, 0.016f, 30.0f, 2.0f, true);
        sink += t;
    }
    double total_ms = timer.elapsed_ms();

    std::cout << "[PERF] AdvanceClipTime " << N << " calls: "
              << total_ms << " ms ("
              << (N / total_ms * 1000.0) << " calls/sec)" << std::endl;

    // 1M 次应在 50ms 内
    EXPECT_LT(total_ms, 50.0);
    (void)sink;
}
