/**
 * @file ai_lod_scheduler_test.cpp
 * @brief AI LOD 调度器单元测试
 */

#include <gtest/gtest.h>
#include "engine/ai/ai_lod_scheduler.h"

using namespace dse::ai;

class AILodSchedulerTest : public ::testing::Test {
protected:
    void SetUp() override {
        AILodConfig config;
        config.near_distance = 30.0f;
        config.medium_distance = 80.0f;
        config.far_distance = 200.0f;
        config.medium_skip_frames = 2;
        config.far_skip_frames = 8;
        config.hysteresis = 0.1f;
        scheduler_.Init(config);
    }

    AILodScheduler scheduler_;
};

TEST_F(AILodSchedulerTest, Register_IncreasesCount) {
    scheduler_.Register(1);
    scheduler_.Register(2);
    scheduler_.Register(3);
    EXPECT_EQ(scheduler_.RegisteredCount(), 3u);
}

TEST_F(AILodSchedulerTest, Unregister_DecreasesCount) {
    scheduler_.Register(1);
    scheduler_.Register(2);
    scheduler_.Unregister(1);
    EXPECT_EQ(scheduler_.RegisteredCount(), 1u);
}

TEST_F(AILodSchedulerTest, NearEntity_AlwaysTicks) {
    scheduler_.Register(1);

    std::unordered_map<uint32_t, glm::vec3> positions;
    positions[1] = glm::vec3(10.0f, 0.0f, 0.0f); // distance = 10 < 30 (near)

    scheduler_.Update(glm::vec3(0.0f), positions);

    EXPECT_EQ(scheduler_.GetLevel(1), AILodLevel::Near);
    EXPECT_TRUE(scheduler_.ShouldTick(1));
}

TEST_F(AILodSchedulerTest, FarEntity_SkipsFrames) {
    scheduler_.Register(1);

    std::unordered_map<uint32_t, glm::vec3> positions;
    positions[1] = glm::vec3(150.0f, 0.0f, 0.0f); // distance = 150, far range

    // First update will set level but might tick (counter reset)
    scheduler_.Update(glm::vec3(0.0f), positions);
    EXPECT_EQ(scheduler_.GetLevel(1), AILodLevel::Far);

    // Next frames should mostly skip
    int tick_count = 0;
    for (int i = 0; i < 16; ++i) {
        scheduler_.Update(glm::vec3(0.0f), positions);
        if (scheduler_.ShouldTick(1)) ++tick_count;
    }

    // Far skip = 8 frames → should tick ~2 times in 16 frames
    EXPECT_GE(tick_count, 1);
    EXPECT_LE(tick_count, 4);
}

TEST_F(AILodSchedulerTest, DormantEntity_NeverTicks) {
    scheduler_.Register(1);

    std::unordered_map<uint32_t, glm::vec3> positions;
    positions[1] = glm::vec3(500.0f, 0.0f, 0.0f); // distance = 500 > 200 (dormant)

    scheduler_.Update(glm::vec3(0.0f), positions);
    EXPECT_EQ(scheduler_.GetLevel(1), AILodLevel::Dormant);

    for (int i = 0; i < 100; ++i) {
        scheduler_.Update(glm::vec3(0.0f), positions);
        EXPECT_FALSE(scheduler_.ShouldTick(1));
    }
}

TEST_F(AILodSchedulerTest, ForceActive_OverridesDistance) {
    scheduler_.Register(1);
    scheduler_.SetForceActive(1, true);

    std::unordered_map<uint32_t, glm::vec3> positions;
    positions[1] = glm::vec3(999.0f, 0.0f, 0.0f); // very far

    scheduler_.Update(glm::vec3(0.0f), positions);
    EXPECT_EQ(scheduler_.GetLevel(1), AILodLevel::Near);
    EXPECT_TRUE(scheduler_.ShouldTick(1));
}

TEST_F(AILodSchedulerTest, HighImportance_StaysNear) {
    scheduler_.Register(1, 100.0f); // importance > 10

    std::unordered_map<uint32_t, glm::vec3> positions;
    positions[1] = glm::vec3(150.0f, 0.0f, 0.0f); // normally Far

    scheduler_.Update(glm::vec3(0.0f), positions);
    EXPECT_EQ(scheduler_.GetLevel(1), AILodLevel::Near);
}

TEST_F(AILodSchedulerTest, Update_ReturnsCorrectCounts) {
    scheduler_.Register(1);
    scheduler_.Register(2);
    scheduler_.Register(3);
    scheduler_.Register(4);

    std::unordered_map<uint32_t, glm::vec3> positions;
    positions[1] = glm::vec3(10.0f, 0.0f, 0.0f);   // near
    positions[2] = glm::vec3(50.0f, 0.0f, 0.0f);   // medium
    positions[3] = glm::vec3(150.0f, 0.0f, 0.0f);  // far
    positions[4] = glm::vec3(500.0f, 0.0f, 0.0f);  // dormant

    auto result = scheduler_.Update(glm::vec3(0.0f), positions);
    EXPECT_EQ(result.near_count, 1u);
    EXPECT_EQ(result.medium_count, 1u);
    EXPECT_EQ(result.far_count, 1u);
    EXPECT_EQ(result.dormant_count, 1u);
}

TEST_F(AILodSchedulerTest, Shutdown_ClearsAll) {
    scheduler_.Register(1);
    scheduler_.Register(2);
    scheduler_.Shutdown();
    EXPECT_EQ(scheduler_.RegisteredCount(), 0u);
}
