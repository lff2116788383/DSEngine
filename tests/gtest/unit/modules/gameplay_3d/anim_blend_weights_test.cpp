/**
 * @file anim_blend_weights_test.cpp
 * @brief blend space 权重计算（1D 线性插值 / 2D 反距离加权）纯函数单测
 *
 * 测试策略：
 * - ComputeBlend1DWeights：空/单点/区间内插值/端点钳制/精确命中/相等阈值
 * - ComputeBlend2DWeights：空/单点/精确命中/对称中点/最近点主导/归一化求和为 1
 */

#include <gtest/gtest.h>
#include <numeric>

#include "modules/gameplay_3d/animation/anim_blend_weights.h"

using dse::gameplay3d::anim_blend::ComputeBlend1DWeights;
using dse::gameplay3d::anim_blend::ComputeBlend2DWeights;

namespace {
float Sum(const std::vector<float>& v) {
    return std::accumulate(v.begin(), v.end(), 0.0f);
}
} // namespace

// ============================================================
// 1D blend space
// ============================================================

// 测试 1D 权重：空输入返回空
TEST(AnimBlend1DTest, EmptyReturnsEmpty) {
    EXPECT_TRUE(ComputeBlend1DWeights({}, 0.5f).empty());
}

// 测试 1D 权重：单采样点恒为 1
TEST(AnimBlend1DTest, SingleNodeFullWeight) {
    auto w = ComputeBlend1DWeights({3.0f}, 100.0f);
    ASSERT_EQ(w.size(), 1u);
    EXPECT_FLOAT_EQ(w[0], 1.0f);
}

// 测试 1D 权重：参数低于最小阈值钳制到首点
TEST(AnimBlend1DTest, BelowRangeClampsToFirst) {
    auto w = ComputeBlend1DWeights({0.0f, 1.0f, 2.0f}, -5.0f);
    ASSERT_EQ(w.size(), 3u);
    EXPECT_FLOAT_EQ(w[0], 1.0f);
    EXPECT_FLOAT_EQ(w[1], 0.0f);
    EXPECT_FLOAT_EQ(w[2], 0.0f);
}

// 测试 1D 权重：参数高于最大阈值钳制到末点
TEST(AnimBlend1DTest, AboveRangeClampsToLast) {
    auto w = ComputeBlend1DWeights({0.0f, 1.0f, 2.0f}, 99.0f);
    ASSERT_EQ(w.size(), 3u);
    EXPECT_FLOAT_EQ(w[2], 1.0f);
    EXPECT_FLOAT_EQ(w[0], 0.0f);
    EXPECT_FLOAT_EQ(w[1], 0.0f);
}

// 测试 1D 权重：精确命中中间阈值
TEST(AnimBlend1DTest, ExactHitMiddle) {
    auto w = ComputeBlend1DWeights({0.0f, 1.0f, 2.0f}, 1.0f);
    ASSERT_EQ(w.size(), 3u);
    EXPECT_FLOAT_EQ(w[1], 1.0f);
    EXPECT_FLOAT_EQ(w[0], 0.0f);
    EXPECT_FLOAT_EQ(w[2], 0.0f);
}

// 测试 1D 权重：区间中点线性插值（0.5/0.5）
TEST(AnimBlend1DTest, MidpointLerp) {
    auto w = ComputeBlend1DWeights({0.0f, 2.0f}, 1.0f);
    ASSERT_EQ(w.size(), 2u);
    EXPECT_FLOAT_EQ(w[0], 0.5f);
    EXPECT_FLOAT_EQ(w[1], 0.5f);
    EXPECT_FLOAT_EQ(Sum(w), 1.0f);
}

// 测试 1D 权重：区间内 25% 处插值（0.75/0.25）
TEST(AnimBlend1DTest, QuarterLerp) {
    auto w = ComputeBlend1DWeights({0.0f, 4.0f}, 1.0f);
    ASSERT_EQ(w.size(), 2u);
    EXPECT_NEAR(w[0], 0.75f, 1e-5f);
    EXPECT_NEAR(w[1], 0.25f, 1e-5f);
}

// 测试 1D 权重：相等阈值不产生除零
TEST(AnimBlend1DTest, EqualThresholdsNoDivByZero) {
    auto w = ComputeBlend1DWeights({1.0f, 1.0f}, 1.0f);
    ASSERT_EQ(w.size(), 2u);
    EXPECT_FLOAT_EQ(Sum(w), 1.0f);
    EXPECT_TRUE(std::isfinite(w[0]) && std::isfinite(w[1]));
}

// ============================================================
// 2D blend space
// ============================================================

// 测试 2D 权重：空输入返回空
TEST(AnimBlend2DTest, EmptyReturnsEmpty) {
    EXPECT_TRUE(ComputeBlend2DWeights({}, glm::vec2(0.0f)).empty());
}

// 测试 2D 权重：单采样点恒为 1
TEST(AnimBlend2DTest, SingleNodeFullWeight) {
    auto w = ComputeBlend2DWeights({glm::vec2(5.0f, 5.0f)}, glm::vec2(0.0f));
    ASSERT_EQ(w.size(), 1u);
    EXPECT_FLOAT_EQ(w[0], 1.0f);
}

// 测试 2D 权重：精确命中某点该点为 1
TEST(AnimBlend2DTest, ExactHitFullWeight) {
    std::vector<glm::vec2> pts{{0.0f, 0.0f}, {1.0f, 1.0f}, {2.0f, 0.0f}};
    auto w = ComputeBlend2DWeights(pts, glm::vec2(1.0f, 1.0f));
    ASSERT_EQ(w.size(), 3u);
    EXPECT_FLOAT_EQ(w[1], 1.0f);
    EXPECT_FLOAT_EQ(w[0], 0.0f);
    EXPECT_FLOAT_EQ(w[2], 0.0f);
}

// 测试 2D 权重：对称中点两点等权
TEST(AnimBlend2DTest, SymmetricMidpointEqualWeights) {
    std::vector<glm::vec2> pts{{0.0f, 0.0f}, {2.0f, 0.0f}};
    auto w = ComputeBlend2DWeights(pts, glm::vec2(1.0f, 0.0f));
    ASSERT_EQ(w.size(), 2u);
    EXPECT_NEAR(w[0], 0.5f, 1e-5f);
    EXPECT_NEAR(w[1], 0.5f, 1e-5f);
}

// 测试 2D 权重：更近的采样点权重更大
TEST(AnimBlend2DTest, NearestDominates) {
    std::vector<glm::vec2> pts{{0.0f, 0.0f}, {10.0f, 0.0f}};
    auto w = ComputeBlend2DWeights(pts, glm::vec2(1.0f, 0.0f));
    ASSERT_EQ(w.size(), 2u);
    EXPECT_GT(w[0], w[1]);
    EXPECT_NEAR(Sum(w), 1.0f, 1e-5f);
}

// 测试 2D 权重：任意布局归一化求和为 1
TEST(AnimBlend2DTest, WeightsSumToOne) {
    std::vector<glm::vec2> pts{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}};
    auto w = ComputeBlend2DWeights(pts, glm::vec2(0.3f, 0.7f));
    ASSERT_EQ(w.size(), 4u);
    EXPECT_NEAR(Sum(w), 1.0f, 1e-5f);
    for (float x : w) EXPECT_GE(x, 0.0f);
}
