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

TEST(GameplayTuningTest, 默认值合理性) {
    GameplayTuningComponent tuning;
    EXPECT_GT(tuning.leaf_min_distance, 0.0f);
    EXPECT_GT(tuning.leaf_move_right, tuning.leaf_move_left);
    EXPECT_GT(tuning.jump_speed_scale, 0.0f);
    EXPECT_GT(tuning.jump_speed_max, 0.0f);
    EXPECT_GT(tuning.camera_follow_damping, 0.0f);
    EXPECT_LT(tuning.camera_follow_damping, 1.0f);
}

TEST(GameplayTuningTest, 默认leaf区间有效) {
    GameplayTuningComponent tuning;
    EXPECT_LT(tuning.leaf_move_left, tuning.leaf_move_right);
    EXPECT_GT(tuning.leaf_min_distance, 0.0f);
}

TEST(GameplayTuningTest, 参数可修改) {
    GameplayTuningComponent tuning;
    tuning.jump_speed_scale = 20.0f;
    tuning.camera_follow_damping = 0.05f;
    EXPECT_FLOAT_EQ(tuning.jump_speed_scale, 20.0f);
    EXPECT_FLOAT_EQ(tuning.camera_follow_damping, 0.05f);
}

TEST(GameplayTuningTest, 各默认值具体数值) {
    GameplayTuningComponent tuning;
    EXPECT_FLOAT_EQ(tuning.leaf_min_distance, 80.0f);
    EXPECT_FLOAT_EQ(tuning.leaf_move_left, 140.0f);
    EXPECT_FLOAT_EQ(tuning.leaf_move_right, 410.0f);
    EXPECT_FLOAT_EQ(tuning.jump_speed_scale, 15.0f);
    EXPECT_FLOAT_EQ(tuning.jump_speed_max, 18.0f);
    EXPECT_FLOAT_EQ(tuning.camera_follow_damping, 0.02f);
}
