/**
 * @file tween_test.cpp
 * @brief Tween 补间动画工具单元测试
 *
 * 覆盖场景：
 * - Linear 缓动求值
 * - EaseInQuad / EaseOutQuad / EaseInOutQuad 缓动求值
 * - 边界值 t=0 和 t=1
 * - 超出范围参数 clamp 到 [0,1]
 * - Lerp 插值计算
 */

#include <gtest/gtest.h>
#include "engine/base/tween.h"
#include <cmath>

using namespace dse::utils;

// ============================================================
// Evaluate 缓动函数
// ============================================================

TEST(TweenTest, Linear返回原值) {
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::Linear, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::Linear, 0.5f), 0.5f);
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::Linear, 1.0f), 1.0f);
}

TEST(TweenTest, EaseInQuad平方增长) {
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseInQuad, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseInQuad, 0.5f), 0.25f);
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseInQuad, 1.0f), 1.0f);
}

TEST(TweenTest, EaseOutQuad反向平方) {
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseOutQuad, 0.0f), 0.0f);
    // t*(2-t) = 0.5*(2-0.5) = 0.5*1.5 = 0.75
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseOutQuad, 0.5f), 0.75f);
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseOutQuad, 1.0f), 1.0f);
}

TEST(TweenTest, EaseInOutQuad先慢后快再慢) {
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseInOutQuad, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseInOutQuad, 1.0f), 1.0f);
    // t=0.25 < 0.5 => 2*0.25*0.25 = 0.125
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseInOutQuad, 0.25f), 0.125f);
    // t=0.75 >= 0.5 => -1 + (4-2*0.75)*0.75 = -1 + 2.5*0.75 = -1+1.875 = 0.875
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseInOutQuad, 0.75f), 0.875f);
}

TEST(TweenTest, EaseInOutQuad中点为0点5) {
    // t=0.5 => -1+(4-1)*0.5 = -1+1.5 = 0.5
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseInOutQuad, 0.5f), 0.5f);
}

// ============================================================
// 参数 clamp
// ============================================================

TEST(TweenTest, 超出范围的t被Clamp到01) {
    // t < 0 应 clamp 到 0
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::Linear, -1.0f), 0.0f);
    // t > 1 应 clamp 到 1
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::Linear, 2.0f), 1.0f);
    // EaseInQuad 的 clamp
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseInQuad, -0.5f), 0.0f);
    EXPECT_FLOAT_EQ(Tween::Evaluate(EaseType::EaseInQuad, 1.5f), 1.0f);
}

// ============================================================
// Lerp 线性插值
// ============================================================

TEST(TweenTest, Lerp默认线性插值) {
    EXPECT_FLOAT_EQ(Tween::Lerp(0.0f, 100.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(Tween::Lerp(0.0f, 100.0f, 0.5f), 50.0f);
    EXPECT_FLOAT_EQ(Tween::Lerp(0.0f, 100.0f, 1.0f), 100.0f);
}

TEST(TweenTest, Lerp使用EaseInQuad) {
    // start + (end-start)*eased_t = 0 + 100*0.25 = 25
    EXPECT_FLOAT_EQ(Tween::Lerp(0.0f, 100.0f, 0.5f, EaseType::EaseInQuad), 25.0f);
}

TEST(TweenTest, Lerp负值范围) {
    EXPECT_FLOAT_EQ(Tween::Lerp(-10.0f, 10.0f, 0.5f), 0.0f);
    EXPECT_FLOAT_EQ(Tween::Lerp(-10.0f, 10.0f, 0.0f), -10.0f);
    EXPECT_FLOAT_EQ(Tween::Lerp(-10.0f, 10.0f, 1.0f), 10.0f);
}

TEST(TweenTest, Lerp起止相同时结果不变) {
    EXPECT_FLOAT_EQ(Tween::Lerp(5.0f, 5.0f, 0.0f), 5.0f);
    EXPECT_FLOAT_EQ(Tween::Lerp(5.0f, 5.0f, 0.5f), 5.0f);
    EXPECT_FLOAT_EQ(Tween::Lerp(5.0f, 5.0f, 1.0f), 5.0f);
}
