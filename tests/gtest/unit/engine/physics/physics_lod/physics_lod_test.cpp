/**
 * @file physics_lod_test.cpp
 * @brief GTest for P3: Physics LOD / Sleep System
 */

#include <gtest/gtest.h>
#include "engine/physics/physics3d/physics_lod.h"

using namespace dse::physics3d;

class PhysicsLODTest : public ::testing::Test {
protected:
    void SetUp() override {
        PhysicsLODConfig cfg;
        cfg.full_distance = 50.0f;
        cfg.reduced_distance = 150.0f;
        cfg.simplified_distance = 300.0f;
        cfg.reduced_frequency = 2;
        cfg.simplified_frequency = 4;
        cfg.sleep_velocity_threshold = 0.05f;
        cfg.wake_velocity_threshold = 0.1f;
        cfg.hysteresis_factor = 1.1f;
        system_.Init(cfg);
    }
    void TearDown() override { system_.Shutdown(); }
    PhysicsLODSystem system_;
};

TEST_F(PhysicsLODTest, RegisterAndUnregister) {
    system_.RegisterBody(1, glm::vec3(0, 0, 0), 1.0f);
    system_.RegisterBody(2, glm::vec3(10, 0, 0), 2.0f);
    EXPECT_EQ(system_.GetRegisteredBodyCount(), 2u);
    system_.UnregisterBody(1);
    EXPECT_EQ(system_.GetRegisteredBodyCount(), 1u);
}

TEST_F(PhysicsLODTest, FullLevel_NearCamera) {
    system_.RegisterBody(1, glm::vec3(20, 0, 0), 1.0f);
    system_.Evaluate(glm::vec3(0, 0, 0), 0);
    EXPECT_EQ(system_.GetBodyLevel(1), PhysicsLODLevel::Full);
    EXPECT_FALSE(system_.IsBodySleeping(1));
}

TEST_F(PhysicsLODTest, ReducedLevel_MediumDistance) {
    system_.RegisterBody(1, glm::vec3(100, 0, 0), 1.0f);
    system_.UpdateBodyState(1, glm::vec3(100, 0, 0), 0.0f);
    system_.Evaluate(glm::vec3(0, 0, 0), 0);
    EXPECT_EQ(system_.GetBodyLevel(1), PhysicsLODLevel::Reduced);
}

TEST_F(PhysicsLODTest, SimplifiedLevel_FarDistance) {
    system_.RegisterBody(1, glm::vec3(250, 0, 0), 1.0f);
    system_.UpdateBodyState(1, glm::vec3(250, 0, 0), 0.0f);
    system_.Evaluate(glm::vec3(0, 0, 0), 0);
    EXPECT_EQ(system_.GetBodyLevel(1), PhysicsLODLevel::Simplified);
}

TEST_F(PhysicsLODTest, SleepLevel_VeryFarAndStill) {
    system_.RegisterBody(1, glm::vec3(500, 0, 0), 1.0f);
    system_.UpdateBodyState(1, glm::vec3(500, 0, 0), 0.01f); // Below sleep threshold
    system_.Evaluate(glm::vec3(0, 0, 0), 0);
    EXPECT_EQ(system_.GetBodyLevel(1), PhysicsLODLevel::Sleep);
    EXPECT_TRUE(system_.IsBodySleeping(1));
}

TEST_F(PhysicsLODTest, NoSleep_HighVelocity) {
    system_.RegisterBody(1, glm::vec3(500, 0, 0), 1.0f);
    system_.UpdateBodyState(1, glm::vec3(500, 0, 0), 5.0f); // High velocity
    system_.Evaluate(glm::vec3(0, 0, 0), 0);
    EXPECT_NE(system_.GetBodyLevel(1), PhysicsLODLevel::Sleep);
    EXPECT_FALSE(system_.IsBodySleeping(1));
}

TEST_F(PhysicsLODTest, FrequencyReduction_ReducedLevel) {
    system_.RegisterBody(1, glm::vec3(100, 0, 0), 1.0f);
    system_.UpdateBodyState(1, glm::vec3(100, 0, 0), 0.0f);

    // Frame 0: should simulate (0 % 2 == 0)
    auto active0 = system_.Evaluate(glm::vec3(0, 0, 0), 0);
    EXPECT_EQ(active0.size(), 1u);

    // Frame 1: should not simulate (1 % 2 != 0)
    EXPECT_FALSE(system_.ShouldSimulateThisFrame(1, 1));

    // Frame 2: should simulate (2 % 2 == 0)
    EXPECT_TRUE(system_.ShouldSimulateThisFrame(1, 2));
}

TEST_F(PhysicsLODTest, FrequencyReduction_SimplifiedLevel) {
    system_.RegisterBody(1, glm::vec3(250, 0, 0), 1.0f);
    system_.UpdateBodyState(1, glm::vec3(250, 0, 0), 0.0f);
    system_.Evaluate(glm::vec3(0, 0, 0), 0);

    // Simplified: frequency divider = 4
    EXPECT_TRUE(system_.ShouldSimulateThisFrame(1, 0));
    EXPECT_FALSE(system_.ShouldSimulateThisFrame(1, 1));
    EXPECT_FALSE(system_.ShouldSimulateThisFrame(1, 2));
    EXPECT_FALSE(system_.ShouldSimulateThisFrame(1, 3));
    EXPECT_TRUE(system_.ShouldSimulateThisFrame(1, 4));
}

TEST_F(PhysicsLODTest, ForceWake) {
    system_.RegisterBody(1, glm::vec3(500, 0, 0), 1.0f);
    system_.UpdateBodyState(1, glm::vec3(500, 0, 0), 0.01f);
    system_.Evaluate(glm::vec3(0, 0, 0), 0);
    EXPECT_TRUE(system_.IsBodySleeping(1));

    system_.WakeBody(1);
    EXPECT_FALSE(system_.IsBodySleeping(1));
    EXPECT_EQ(system_.GetBodyLevel(1), PhysicsLODLevel::Full);
}

TEST_F(PhysicsLODTest, ForceSleep) {
    system_.RegisterBody(1, glm::vec3(20, 0, 0), 1.0f);
    system_.Evaluate(glm::vec3(0, 0, 0), 0);
    EXPECT_FALSE(system_.IsBodySleeping(1));

    system_.SleepBody(1);
    EXPECT_TRUE(system_.IsBodySleeping(1));
    EXPECT_EQ(system_.GetBodyLevel(1), PhysicsLODLevel::Sleep);
}

TEST_F(PhysicsLODTest, LevelStats) {
    system_.RegisterBody(1, glm::vec3(20, 0, 0), 1.0f);    // Full
    system_.RegisterBody(2, glm::vec3(100, 0, 0), 1.0f);   // Reduced
    system_.RegisterBody(3, glm::vec3(250, 0, 0), 1.0f);   // Simplified
    system_.RegisterBody(4, glm::vec3(500, 0, 0), 1.0f);   // Sleep
    system_.UpdateBodyState(4, glm::vec3(500, 0, 0), 0.01f);

    system_.Evaluate(glm::vec3(0, 0, 0), 0);
    auto stats = system_.GetLevelStats();
    EXPECT_EQ(stats.full, 1u);
    EXPECT_EQ(stats.reduced, 1u);
    EXPECT_EQ(stats.simplified, 1u);
    EXPECT_EQ(stats.sleeping, 1u);
}

TEST_F(PhysicsLODTest, RebaseOrigin) {
    system_.RegisterBody(1, glm::vec3(100, 0, 0), 1.0f);
    system_.UpdateBodyState(1, glm::vec3(100, 0, 0), 0.0f);

    // Before rebase: distance=100 → Reduced
    system_.Evaluate(glm::vec3(0, 0, 0), 0);
    EXPECT_EQ(system_.GetBodyLevel(1), PhysicsLODLevel::Reduced);

    // Rebase by (80,0,0): body moves to (20,0,0) → distance=20 → Full
    system_.RebaseOrigin(glm::vec3(80, 0, 0));
    system_.UpdateBodyState(1, glm::vec3(20, 0, 0), 0.0f);
    system_.Evaluate(glm::vec3(0, 0, 0), 1);
    EXPECT_EQ(system_.GetBodyLevel(1), PhysicsLODLevel::Full);
}

TEST_F(PhysicsLODTest, ActiveBodies_SleepingExcluded) {
    system_.RegisterBody(1, glm::vec3(20, 0, 0), 1.0f);
    system_.RegisterBody(2, glm::vec3(500, 0, 0), 1.0f);
    system_.UpdateBodyState(2, glm::vec3(500, 0, 0), 0.01f);

    auto active = system_.Evaluate(glm::vec3(0, 0, 0), 0);
    // Body 1 should be active, body 2 should be sleeping
    EXPECT_EQ(active.size(), 1u);
    EXPECT_EQ(active[0], 1u);
    EXPECT_EQ(system_.GetActiveBodyCount(), 1u);
    EXPECT_EQ(system_.GetSleepingBodyCount(), 1u);
}
