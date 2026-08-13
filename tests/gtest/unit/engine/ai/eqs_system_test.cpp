/**
 * @file eqs_system_test.cpp
 * @brief EQS 系统测试：NavMesh 生成器、PathPoints 生成器、真实可达性评分
 *
 * 测试策略：
 * - NavMesh 生成器：注入表面采样回调后只保留可走面候选点
 * - PathPoints 生成器：沿方向等距生成候选点
 * - Reachable 评分器：注入可达性回调走真实寻路判定；未注入时回退距离近似
 */

#include <gtest/gtest.h>

#include "engine/ai/eqs_system.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace dse::ai {

// NavMesh 生成器：表面采样回调过滤候选点
TEST(EQSSystemTest, NavMeshGeneratorUsesSurfaceSample) {
    EQSSystem sys;
    sys.Init();

    uint32_t tmpl = sys.CreateTemplate("cover_query");
    GeneratorConfig gen;
    gen.type = GeneratorType::NavMesh;
    gen.center = glm::vec3(0.0f, 0.0f, 0.0f);
    gen.radius = 10.0f;
    gen.spacing = 2.0f;
    gen.max_points = 50;
    sys.SetGenerator(tmpl, gen);

    // 模拟 NavMesh：只接受 x+z 为偶数的点（x 轴 2 单位条带），其余点落在面上
    int surface_hits = 0;
    sys.SetSurfaceSampleFunc([&](const glm::vec3& pos, glm::vec3& out) -> bool {
        if (std::abs(pos.x) <= 2.0f) {  // 仅 x ∈ [-2,2] 的条带可走
            out = pos;
            surface_hits++;
            return true;
        }
        return false;
    });

    ScorerConfig scorer;
    scorer.type = ScorerType::Distance;
    sys.AddScorer(tmpl, scorer);
    sys.SetMaxResults(tmpl, 50);

    auto result = sys.Execute(tmpl, glm::vec3(0.0f, 0.0f, 0.0f));
    // 所有候选都应在可走条带内
    ASSERT_GT(result.candidates.size(), 0u);
    for (const auto& c : result.candidates) {
        EXPECT_LE(std::abs(c.position.x), 2.0f + 1e-3f);
    }
    EXPECT_GT(surface_hits, 0);

    sys.Shutdown();
}

// PathPoints 生成器：沿 direction 等距生成
TEST(EQSSystemTest, PathPointsGeneratorAlongDirection) {
    EQSSystem sys;
    sys.Init();

    uint32_t tmpl = sys.CreateTemplate("patrol_points");
    GeneratorConfig gen;
    gen.type = GeneratorType::PathPoints;
    gen.center = glm::vec3(0.0f, 0.0f, 0.0f);
    gen.direction = glm::vec3(1.0f, 0.0f, 0.0f);
    gen.radius = 6.0f;
    gen.spacing = 2.0f;
    gen.max_points = 20;
    sys.SetGenerator(tmpl, gen);

    ScorerConfig scorer;
    scorer.type = ScorerType::Distance;
    sys.AddScorer(tmpl, scorer);

    auto result = sys.Execute(tmpl, glm::vec3(0.0f, 0.0f, 0.0f));
    ASSERT_GT(result.candidates.size(), 0u);
    for (const auto& c : result.candidates) {
        // 所有点应落在 X 轴上（y/z ≈ 0）
        EXPECT_NEAR(c.position.z, 0.0f, 1e-3f);
        EXPECT_NEAR(c.position.y, gen.height_offset, 1e-3f);
        EXPECT_LE(std::abs(c.position.x), 6.0f + 1e-3f);
    }

    sys.Shutdown();
}

// Reachable 评分器：注入可达性回调 → 真实寻路判定
TEST(EQSSystemTest, ReachableScorerUsesInjectedCallback) {
    EQSSystem sys;
    sys.Init();

    uint32_t tmpl = sys.CreateTemplate("reachable_query");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.center = glm::vec3(0.0f, 0.0f, 0.0f);
    gen.radius = 4.0f;
    gen.inner_radius = 0.0f;
    gen.spacing = 4.0f;   // 生成 3x3 网格
    gen.max_points = 20;
    sys.SetGenerator(tmpl, gen);

    ScorerConfig reachable;
    reachable.type = ScorerType::Reachable;
    reachable.weight = 1.0f;
    sys.AddScorer(tmpl, reachable);

    // 模拟寻路：仅原点到 x>=0 一侧的点可达
    int reach_calls = 0;
    sys.SetReachabilityFunc([&](const glm::vec3& from, const glm::vec3& to) -> bool {
        reach_calls++;
        (void)from;
        return to.x >= 0.0f;
    });

    auto result = sys.Execute(tmpl, glm::vec3(0.0f, 0.0f, 0.0f));
    ASSERT_GT(result.candidates.size(), 0u);
    EXPECT_GT(reach_calls, 0);
    // 所有有效候选（score>0）的 x >= 0；不可达候选 score=0 且 valid=false
    for (const auto& c : result.candidates) {
        if (c.valid) {
            EXPECT_GT(c.score, 0.0f);
            EXPECT_GE(c.position.x, -1e-3f);
        } else {
            EXPECT_LE(c.score, 0.0f);
        }
    }

    sys.Shutdown();
}

// Reachable 评分器：无回调时回退距离近似（向后兼容）
TEST(EQSSystemTest, ReachableScorerFallbackDistance) {
    EQSSystem sys;
    sys.Init();

    uint32_t tmpl = sys.CreateTemplate("fallback_reachable");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.center = glm::vec3(0.0f, 0.0f, 0.0f);
    gen.radius = 4.0f;
    gen.inner_radius = 0.0f;
    gen.spacing = 4.0f;
    gen.max_points = 20;
    sys.SetGenerator(tmpl, gen);

    ScorerConfig reachable;
    reachable.type = ScorerType::Reachable;
    reachable.max_value = 6.0f;  // 距离阈值
    sys.AddScorer(tmpl, reachable);

    // 不注入任何回调：全部候选都应在阈值内 → 全部有效
    auto result = sys.Execute(tmpl, glm::vec3(0.0f, 0.0f, 0.0f));
    ASSERT_GT(result.candidates.size(), 0u);
    for (const auto& c : result.candidates) {
        EXPECT_GT(c.score, 0.0f);
    }

    sys.Shutdown();
}

} // namespace dse::ai
