/**
 * @file anim_clip_time_test.cpp
 * @brief anim_util::AdvanceClipTime 时间推进/循环/夹取逻辑单元测试
 *
 * 该函数是运行时（animator_system / layer blend）推进 clip 播放时间的唯一实现，
 * 决定 loop 环绕、非 loop 夹取、反向播放、零时长等行为。锁定其行为可保证
 * 运行时播放与（复用同函数的）预览一致。
 */

#include <gtest/gtest.h>
#include <cmath>
#include "modules/gameplay_3d/animation/anim_clip_eval.h"

using dse::gameplay3d::anim_util::AdvanceClipTime;

TEST(AnimClipTime, AdvancesWithinDuration) {
    // 0.0 + 0.5s * speed1 = 0.5，未超过 dur=2.0
    EXPECT_FLOAT_EQ(AdvanceClipTime(0.0f, 0.5f, 1.0f, 2.0f, true), 0.5f);
}

TEST(AnimClipTime, LoopWrapsPastDuration) {
    // 1.8 + 0.5 = 2.3，dur=2.0，loop → fmod(2.3,2.0)=0.3
    float t = AdvanceClipTime(1.8f, 0.5f, 1.0f, 2.0f, true);
    EXPECT_NEAR(t, 0.3f, 1e-5f);
}

TEST(AnimClipTime, NonLoopClampsAtDuration) {
    // 1.8 + 0.5 = 2.3，dur=2.0，非 loop → 夹取到 2.0
    float t = AdvanceClipTime(1.8f, 0.5f, 1.0f, 2.0f, false);
    EXPECT_FLOAT_EQ(t, 2.0f);
}

TEST(AnimClipTime, SpeedScalesDelta) {
    // 0.0 + 0.5s * speed2 = 1.0
    EXPECT_FLOAT_EQ(AdvanceClipTime(0.0f, 0.5f, 2.0f, 4.0f, true), 1.0f);
}

TEST(AnimClipTime, ReverseLoopWrapsToPositive) {
    // 0.2 + (-0.5) = -0.3，dur=2.0，loop → 2.0 + fmod(-0.3,2.0) = 1.7
    float t = AdvanceClipTime(0.2f, -0.5f, 1.0f, 2.0f, true);
    EXPECT_NEAR(t, 1.7f, 1e-5f);
}

TEST(AnimClipTime, ReverseNonLoopClampsAtZero) {
    // 0.2 + (-0.5) = -0.3，非 loop → 夹取到 0.0
    float t = AdvanceClipTime(0.2f, -0.5f, 1.0f, 2.0f, false);
    EXPECT_FLOAT_EQ(t, 0.0f);
}

TEST(AnimClipTime, ZeroDurationReturnsZero) {
    EXPECT_FLOAT_EQ(AdvanceClipTime(1.0f, 0.5f, 1.0f, 0.0f, true), 0.0f);
    EXPECT_FLOAT_EQ(AdvanceClipTime(1.0f, 0.5f, 1.0f, 0.0f, false), 0.0f);
}
