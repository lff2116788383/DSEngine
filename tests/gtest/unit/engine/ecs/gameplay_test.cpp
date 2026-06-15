/**
 * @file gameplay_test.cpp
 * @brief GameplayTuningComponent 玩法微调参数的单元测试
 *
 * 覆盖场景：
 * - 默认参数值合理性
 * - 参数可修改
 */

#include <gtest/gtest.h>
#include "engine/ecs/gameplay.h"

// 测试 玩法调参：默认值
TEST(GameplayTuningTest, DefaultValues) {
    GameplayTuningComponent tuning;
    EXPECT_GT(tuning.leaf_min_distance, 0.0f);
    EXPECT_GT(tuning.leaf_move_right, tuning.leaf_move_left);
    EXPECT_GT(tuning.jump_speed_scale, 0.0f);
    EXPECT_GT(tuning.jump_speed_max, 0.0f);
    EXPECT_GT(tuning.camera_follow_damping, 0.0f);
    EXPECT_LT(tuning.camera_follow_damping, 1.0f);
}

// 测试 玩法调参：Defaultleaf有效
TEST(GameplayTuningTest, DefaultleafValid) {
    GameplayTuningComponent tuning;
    EXPECT_LT(tuning.leaf_move_left, tuning.leaf_move_right);
    EXPECT_GT(tuning.leaf_min_distance, 0.0f);
}

// 测试 玩法调参：参数能够修订
TEST(GameplayTuningTest, ParametersCanRevise) {
    GameplayTuningComponent tuning;
    tuning.jump_speed_scale = 20.0f;
    tuning.camera_follow_damping = 0.05f;
    EXPECT_FLOAT_EQ(tuning.jump_speed_scale, 20.0f);
    EXPECT_FLOAT_EQ(tuning.camera_follow_damping, 0.05f);
}

// 测试 玩法调参：默认值2
TEST(GameplayTuningTest, DefaultValues_2) {
    GameplayTuningComponent tuning;
    EXPECT_FLOAT_EQ(tuning.leaf_min_distance, 80.0f);
    EXPECT_FLOAT_EQ(tuning.leaf_move_left, 140.0f);
    EXPECT_FLOAT_EQ(tuning.leaf_move_right, 410.0f);
    EXPECT_FLOAT_EQ(tuning.jump_speed_scale, 15.0f);
    EXPECT_FLOAT_EQ(tuning.jump_speed_max, 18.0f);
    EXPECT_FLOAT_EQ(tuning.camera_follow_damping, 0.02f);
}
