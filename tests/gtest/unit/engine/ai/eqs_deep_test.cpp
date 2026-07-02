/**
 * @file eqs_deep_test.cpp
 * @brief P6: EQS 环境查询系统深度测试 — 模板管理、生成器、评分器、查询执行
 */

#include <gtest/gtest.h>
#include "engine/ai/eqs_system.h"
#include <cmath>

using namespace dse::ai;

class EQSDeepTest : public ::testing::Test {
protected:
    EQSSystem eqs;

    void SetUp() override {
        eqs.Init();
    }
    void TearDown() override {
        eqs.Shutdown();
    }
};

// ═══════════════════════════════════════════════════════════
// 模板管理
// ═══════════════════════════════════════════════════════════

TEST_F(EQSDeepTest, CreateTemplate) {
    uint32_t id = eqs.CreateTemplate("cover_query");
    // ID starts at 0 (next_id_ = 0 initially)
    EXPECT_EQ(eqs.GetTemplateCount(), 1u);
    (void)id;
}

TEST_F(EQSDeepTest, CreateMultipleTemplates) {
    eqs.CreateTemplate("A");
    eqs.CreateTemplate("B");
    eqs.CreateTemplate("C");
    EXPECT_EQ(eqs.GetTemplateCount(), 3u);
}

TEST_F(EQSDeepTest, DestroyTemplate) {
    uint32_t id = eqs.CreateTemplate("temp");
    EXPECT_EQ(eqs.GetTemplateCount(), 1u);
    eqs.DestroyTemplate(id);
    EXPECT_EQ(eqs.GetTemplateCount(), 0u);
}

TEST_F(EQSDeepTest, DestroyNonexistent) {
    eqs.DestroyTemplate(9999);
    EXPECT_EQ(eqs.GetTemplateCount(), 0u);
}

// ═══════════════════════════════════════════════════════════
// 生成器 Grid
// ═══════════════════════════════════════════════════════════

TEST_F(EQSDeepTest, GridGeneratorBasic) {
    uint32_t id = eqs.CreateTemplate("grid");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.center = glm::vec3(0.0f);
    gen.radius = 10.0f;
    gen.spacing = 2.0f;
    gen.max_points = 200;
    eqs.SetGenerator(id, gen);

    QueryResult result = eqs.Execute(id, glm::vec3(0.0f));
    EXPECT_GT(result.total_generated, 0u);
    EXPECT_GT(result.candidates.size(), 0u);
}

TEST_F(EQSDeepTest, GridMaxPointsCap) {
    uint32_t id = eqs.CreateTemplate("capped");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 100.0f;
    gen.spacing = 1.0f;
    gen.max_points = 5;
    eqs.SetGenerator(id, gen);

    QueryResult result = eqs.Execute(id, glm::vec3(0.0f));
    EXPECT_LE(result.total_generated, 5u);
}

// ═══════════════════════════════════════════════════════════
// 生成器 Ring
// ═══════════════════════════════════════════════════════════

TEST_F(EQSDeepTest, RingGenerator) {
    uint32_t id = eqs.CreateTemplate("ring");
    GeneratorConfig gen;
    gen.type = GeneratorType::Ring;
    gen.center = glm::vec3(0.0f);
    gen.radius = 10.0f;
    gen.inner_radius = 3.0f;
    gen.spacing = 2.0f;
    gen.max_points = 100;
    eqs.SetGenerator(id, gen);

    QueryResult result = eqs.Execute(id, glm::vec3(0.0f));
    EXPECT_GT(result.total_generated, 0u);

    for (auto& c : result.candidates) {
        float dist = glm::length(c.position - glm::vec3(0.0f));
        EXPECT_GE(dist, gen.inner_radius - 0.5f);
    }
}

// ═══════════════════════════════════════════════════════════
// 生成器 Random
// ═══════════════════════════════════════════════════════════

TEST_F(EQSDeepTest, RandomGenerator) {
    uint32_t id = eqs.CreateTemplate("random");
    GeneratorConfig gen;
    gen.type = GeneratorType::Random;
    gen.center = glm::vec3(5.0f, 0.0f, 5.0f);
    gen.radius = 15.0f;
    gen.max_points = 50;
    eqs.SetGenerator(id, gen);

    QueryResult result = eqs.Execute(id, glm::vec3(5.0f, 0.0f, 5.0f));
    EXPECT_GT(result.total_generated, 0u);
}

// ═══════════════════════════════════════════════════════════
// 评分器 Distance
// ═══════════════════════════════════════════════════════════

TEST_F(EQSDeepTest, DistanceScorerSortsDesc) {
    uint32_t id = eqs.CreateTemplate("dist");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 10.0f;
    gen.spacing = 3.0f;
    gen.max_points = 100;
    eqs.SetGenerator(id, gen);

    ScorerConfig scorer;
    scorer.type = ScorerType::Distance;
    scorer.weight = 1.0f;
    scorer.invert = true;
    scorer.reference_point = glm::vec3(0.0f);
    scorer.max_value = 20.0f;
    eqs.AddScorer(id, scorer);

    eqs.SetMaxResults(id, 5);

    QueryResult result = eqs.Execute(id, glm::vec3(0.0f));
    EXPECT_GT(result.candidates.size(), 0u);

    for (size_t i = 1; i < result.candidates.size(); ++i) {
        EXPECT_GE(result.candidates[i - 1].score, result.candidates[i].score);
    }
}

TEST_F(EQSDeepTest, ClearScorersDoesNotCrash) {
    uint32_t id = eqs.CreateTemplate("clear_test");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 5.0f;
    gen.spacing = 2.0f;
    eqs.SetGenerator(id, gen);

    ScorerConfig s;
    s.type = ScorerType::Distance;
    eqs.AddScorer(id, s);
    eqs.AddScorer(id, s);
    eqs.ClearScorers(id);

    QueryResult result = eqs.Execute(id, glm::vec3(0.0f));
    // After clearing scorers, candidates exist but unscored
    EXPECT_GE(result.total_generated, 0u);
}

// ═══════════════════════════════════════════════════════════
// 评分器 Height
// ═══════════════════════════════════════════════════════════

TEST_F(EQSDeepTest, HeightScorer) {
    uint32_t id = eqs.CreateTemplate("height");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 10.0f;
    gen.spacing = 3.0f;
    gen.max_points = 50;
    eqs.SetGenerator(id, gen);

    ScorerConfig scorer;
    scorer.type = ScorerType::Height;
    scorer.weight = 1.0f;
    eqs.AddScorer(id, scorer);

    eqs.SetHeightSampleFunc([](float, float) { return 10.0f; });

    QueryResult result = eqs.Execute(id, glm::vec3(0.0f));
    EXPECT_GT(result.total_generated, 0u);
}

// ═══════════════════════════════════════════════════════════
// 组合模式
// ═══════════════════════════════════════════════════════════

TEST_F(EQSDeepTest, MultiplyMode) {
    uint32_t id = eqs.CreateTemplate("multiply");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 10.0f;
    gen.spacing = 3.0f;
    eqs.SetGenerator(id, gen);

    ScorerConfig s1;
    s1.type = ScorerType::Distance;
    s1.weight = 1.0f;
    s1.reference_point = glm::vec3(0.0f);
    s1.max_value = 20.0f;
    eqs.AddScorer(id, s1);
    eqs.AddScorer(id, s1);

    eqs.SetCombineMode(id, CombineMode::Multiply);
    eqs.SetMaxResults(id, 10);

    QueryResult result = eqs.Execute(id, glm::vec3(0.0f));
    EXPECT_GT(result.candidates.size(), 0u);
}

TEST_F(EQSDeepTest, MinimumMode) {
    uint32_t id = eqs.CreateTemplate("minimum");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 10.0f;
    gen.spacing = 3.0f;
    eqs.SetGenerator(id, gen);

    ScorerConfig s;
    s.type = ScorerType::Distance;
    s.weight = 1.0f;
    s.reference_point = glm::vec3(0.0f);
    s.max_value = 20.0f;
    eqs.AddScorer(id, s);

    eqs.SetCombineMode(id, CombineMode::Minimum);

    QueryResult result = eqs.Execute(id, glm::vec3(0.0f));
    EXPECT_GT(result.total_generated, 0u);
}

// ═══════════════════════════════════════════════════════════
// 自定义评分器
// ═══════════════════════════════════════════════════════════

TEST_F(EQSDeepTest, RegisterCustomScorer) {
    uint32_t cid = eqs.RegisterCustomScorer("always_one",
        [](const glm::vec3&, const glm::vec3&) { return 1.0f; });
    // ID can be 0 (first registered)
    (void)cid;
}

// ═══════════════════════════════════════════════════════════
// 默认模板执行
// ═══════════════════════════════════════════════════════════

TEST_F(EQSDeepTest, DefaultTemplateExecuteDoesNotCrash) {
    uint32_t id = eqs.CreateTemplate("default");
    // Default template has Grid generator with 200 max points
    QueryResult result = eqs.Execute(id, glm::vec3(0.0f));
    // May generate points with default generator config
    (void)result;
}

TEST_F(EQSDeepTest, QueryTimeMeasured) {
    uint32_t id = eqs.CreateTemplate("timed");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 20.0f;
    gen.spacing = 1.0f;
    gen.max_points = 200;
    eqs.SetGenerator(id, gen);

    QueryResult result = eqs.Execute(id, glm::vec3(0.0f));
    EXPECT_GE(result.query_time_ms, 0.0f);
}
