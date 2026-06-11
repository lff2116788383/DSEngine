/**
 * @file particle_curve_test.cpp
 * @brief ParticleCurve::Evaluate() 粒子曲线评估的单元测试
 *
 * 覆盖场景：
 * - Linear：线性插值边界与中点
 * - EaseIn：二次缓入特性
 * - EaseOut：二次缓出特性
 * - EaseInOut：缓入缓出对称性
 * - 参数 t 越界 clamp 到 [0,1]
 * - start_value == end_value 时恒值
 * - 反向曲线（start > end）
 */

#include <gtest/gtest.h>
#include "engine/ecs/particle_2d.h"
#include <cmath>

// ============================================================
// Linear 曲线
// ============================================================

TEST(ParticleCurveTest, LinearendpointValue) {
    ParticleCurve curve{true, ParticleCurveType::Linear, 1.0f, 0.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(0.0f), 1.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(1.0f), 0.0f);
}

TEST(ParticleCurveTest, Linearmidpoint) {
    ParticleCurve curve{true, ParticleCurveType::Linear, 0.0f, 100.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(0.5f), 50.0f);
}

TEST(ParticleCurveTest, LinearquarterPoint) {
    ParticleCurve curve{true, ParticleCurveType::Linear, 0.0f, 1.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(0.25f), 0.25f);
    EXPECT_FLOAT_EQ(curve.Evaluate(0.75f), 0.75f);
}

// ============================================================
// EaseIn 曲线（t^2）
// ============================================================

TEST(ParticleCurveTest, EaseInendpointValue) {
    ParticleCurve curve{true, ParticleCurveType::EaseIn, 1.0f, 0.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(0.0f), 1.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(1.0f), 0.0f);
}

TEST(ParticleCurveTest, EaseInMidpointSlowStart) {
    // EaseIn: shaped_t = 0.5^2 = 0.25, mix(0,1,0.25) = 0.25
    ParticleCurve curve{true, ParticleCurveType::EaseIn, 0.0f, 1.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(0.5f), 0.25f);
}

TEST(ParticleCurveTest, EaseInThePreviousValueIsLessThanLinear) {
    ParticleCurve ease_in{true, ParticleCurveType::EaseIn, 0.0f, 1.0f};
    ParticleCurve linear{true, ParticleCurveType::Linear, 0.0f, 1.0f};
    EXPECT_LT(ease_in.Evaluate(0.3f), linear.Evaluate(0.3f));
}

// ============================================================
// EaseOut 曲线（1 - (1-t)^2）
// ============================================================

TEST(ParticleCurveTest, EaseOutendpointValue) {
    ParticleCurve curve{true, ParticleCurveType::EaseOut, 1.0f, 0.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(0.0f), 1.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(1.0f), 0.0f);
}

TEST(ParticleCurveTest, EaseOutMidpointQuickStart) {
    // EaseOut: shaped_t = 1-(1-0.5)^2 = 1-0.25 = 0.75, mix(0,1,0.75) = 0.75
    ParticleCurve curve{true, ParticleCurveType::EaseOut, 0.0f, 1.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(0.5f), 0.75f);
}

TEST(ParticleCurveTest, EaseOutThePreviousValueIsGreaterThanLinear) {
    ParticleCurve ease_out{true, ParticleCurveType::EaseOut, 0.0f, 1.0f};
    ParticleCurve linear{true, ParticleCurveType::Linear, 0.0f, 1.0f};
    EXPECT_GT(ease_out.Evaluate(0.3f), linear.Evaluate(0.3f));
}

// ============================================================
// EaseInOut 曲线
// ============================================================

TEST(ParticleCurveTest, EaseInOutendpointValue) {
    ParticleCurve curve{true, ParticleCurveType::EaseInOut, 1.0f, 0.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(0.0f), 1.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(1.0f), 0.0f);
}

TEST(ParticleCurveTest, EaseInOutmidpoint) {
    // t=0.5: shaped_t = 2*(0.5)^2 = 0.5, mix(0,1,0.5) = 0.5
    ParticleCurve curve{true, ParticleCurveType::EaseInOut, 0.0f, 1.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(0.5f), 0.5f);
}

TEST(ParticleCurveTest, EaseInOutTheFirstHalfIsSimilarEaseIn) {
    ParticleCurve ease_in_out{true, ParticleCurveType::EaseInOut, 0.0f, 1.0f};
    ParticleCurve linear{true, ParticleCurveType::Linear, 0.0f, 1.0f};
    // 前半段（t<0.5） EaseInOut 增长比 Linear 慢
    EXPECT_LT(ease_in_out.Evaluate(0.25f), linear.Evaluate(0.25f));
}

TEST(ParticleCurveTest, EaseInOutTheSecondHalfIsSimilarEaseOut) {
    ParticleCurve ease_in_out{true, ParticleCurveType::EaseInOut, 0.0f, 1.0f};
    ParticleCurve linear{true, ParticleCurveType::Linear, 0.0f, 1.0f};
    // 后半段（t>0.5） EaseInOut 增长比 Linear 快
    EXPECT_GT(ease_in_out.Evaluate(0.75f), linear.Evaluate(0.75f));
}

// ============================================================
// 通用特性
// ============================================================

TEST(ParticleCurveTest, TwhenCrossingTheLineclampTo01) {
    ParticleCurve curve{true, ParticleCurveType::Linear, 10.0f, 20.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(-1.0f), 10.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(2.0f), 20.0f);
}

TEST(ParticleCurveTest, When) {
    ParticleCurve curve{true, ParticleCurveType::Linear, 5.0f, 5.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(0.0f), 5.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(0.5f), 5.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(1.0f), 5.0f);
}

TEST(ParticleCurveTest, Towardstartend) {
    ParticleCurve curve{true, ParticleCurveType::Linear, 10.0f, 0.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(0.0f), 10.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(0.5f), 5.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(1.0f), 0.0f);
}

TEST(ParticleCurveTest, EaseInCrossTheLineclamp) {
    ParticleCurve curve{true, ParticleCurveType::EaseIn, 0.0f, 100.0f};
    EXPECT_FLOAT_EQ(curve.Evaluate(-0.5f), 0.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(1.5f), 100.0f);
}

TEST(ParticleCurveTest, TheDefaultValueIsSingleDecays) {
    ParticleCurve curve;
    EXPECT_FALSE(curve.enabled);
    EXPECT_EQ(curve.type, ParticleCurveType::Linear);
    EXPECT_FLOAT_EQ(curve.start_value, 1.0f);
    EXPECT_FLOAT_EQ(curve.end_value, 0.0f);
}
