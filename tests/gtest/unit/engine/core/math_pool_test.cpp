/**
 * @file math_pool_test.cpp
 * @brief 数学工具与内存池/对象池的单元测试
 *
 * 覆盖场景：
 * - BezierCurve2D：二次/三次贝塞尔曲线插值
 * - Tween：缓动函数评估、线性插值
 *
 * 注：内存池/对象池（PoolAllocator / ObjectPool）测试已迁至 memory_test.cpp，
 * 随阶段4 重写为原地构造的池实现。
 */

#include <gtest/gtest.h>
#include "engine/base/bezier.h"
#include "engine/base/tween.h"
#include <glm/glm.hpp>
#include <cmath>

// ============================================================
// BezierCurve2D 测试
// ============================================================

TEST(BezierCurve2DTest, QuadraticBezierStartingPoint) {
    glm::vec2 p0(0.0f, 0.0f), p1(1.0f, 2.0f), p2(3.0f, 0.0f);
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateQuadratic(0.0f, p0, p1, p2);
    EXPECT_NEAR(result.x, 0.0f, 1e-5f);
    EXPECT_NEAR(result.y, 0.0f, 1e-5f);
}

TEST(BezierCurve2DTest, QuadraticBezierEndPoint) {
    glm::vec2 p0(0.0f, 0.0f), p1(1.0f, 2.0f), p2(3.0f, 0.0f);
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateQuadratic(1.0f, p0, p1, p2);
    EXPECT_NEAR(result.x, 3.0f, 1e-5f);
    EXPECT_NEAR(result.y, 0.0f, 1e-5f);
}

TEST(BezierCurve2DTest, QuadraticBezierMidpoint) {
    glm::vec2 p0(0.0f, 0.0f), p1(1.0f, 2.0f), p2(3.0f, 0.0f);
    // t=0.5: (1-0.5)^2*p0 + 2*(1-0.5)*0.5*p1 + 0.5^2*p2 = 0.25*p0 + 0.5*p1 + 0.25*p2
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateQuadratic(0.5f, p0, p1, p2);
    EXPECT_NEAR(result.x, 0.25f * 0.0f + 0.5f * 1.0f + 0.25f * 3.0f, 1e-5f);
    EXPECT_NEAR(result.y, 0.25f * 0.0f + 0.5f * 2.0f + 0.25f * 0.0f, 1e-5f);
}

TEST(BezierCurve2DTest, QuadraticBezierLinearDegeneration) {
    // 控制点在起点终点的连线上，应退化为线性插值
    glm::vec2 p0(0.0f, 0.0f), p1(2.0f, 2.0f), p2(4.0f, 4.0f);
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateQuadratic(0.5f, p0, p1, p2);
    EXPECT_NEAR(result.x, 2.0f, 1e-5f);
    EXPECT_NEAR(result.y, 2.0f, 1e-5f);
}

TEST(BezierCurve2DTest, CubicBezierStartingPoint) {
    glm::vec2 p0(0, 0), p1(1, 3), p2(4, 3), p3(5, 0);
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateCubic(0.0f, p0, p1, p2, p3);
    EXPECT_NEAR(result.x, 0.0f, 1e-5f);
    EXPECT_NEAR(result.y, 0.0f, 1e-5f);
}

TEST(BezierCurve2DTest, TripleBezierEnd) {
    glm::vec2 p0(0, 0), p1(1, 3), p2(4, 3), p3(5, 0);
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateCubic(1.0f, p0, p1, p2, p3);
    EXPECT_NEAR(result.x, 5.0f, 1e-5f);
    EXPECT_NEAR(result.y, 0.0f, 1e-5f);
}

TEST(BezierCurve2DTest, CubicBezierSymmetry) {
    // 反转控制点顺序并在 t→1-t 时应得到相同结果
    glm::vec2 p0(0, 0), p1(1, 3), p2(4, 3), p3(5, 0);
    float t = 0.3f;
    glm::vec2 forward = dse::math::BezierCurve2D::EvaluateCubic(t, p0, p1, p2, p3);
    glm::vec2 backward = dse::math::BezierCurve2D::EvaluateCubic(1.0f - t, p3, p2, p1, p0);
    EXPECT_NEAR(forward.x, backward.x, 1e-4f);
    EXPECT_NEAR(forward.y, backward.y, 1e-4f);
}

// ============================================================
// Tween 测试
// ============================================================

TEST(TweenTest, LineareasingConstant) {
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::Linear, 0.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::Linear, 0.5f), 0.5f, 1e-5f);
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::Linear, 1.0f), 1.0f, 1e-5f);
}

TEST(TweenTest, EaseInQuadValueIsCorrect) {
    float t = 0.5f;
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::EaseInQuad, t), t * t, 1e-5f);
}

TEST(TweenTest, EaseOutQuadValueIsCorrect) {
    float t = 0.5f;
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::EaseOutQuad, t), t * (2.0f - t), 1e-5f);
}

TEST(TweenTest, EaseInOutQuadTheFirstHalfIsEquivalentToEaseIn) {
    float t = 0.3f;
    float io_val = dse::utils::Tween::Evaluate(dse::utils::EaseType::EaseInOutQuad, t);
    float expected = 2.0f * t * t;
    EXPECT_NEAR(io_val, expected, 1e-5f);
}

TEST(TweenTest, EaseInOutQuadTheSecondHalfIsEquivalentToEaseOutdeformation) {
    float t = 0.7f;
    float io_val = dse::utils::Tween::Evaluate(dse::utils::EaseType::EaseInOutQuad, t);
    float expected = -1.0f + (4.0f - 2.0f * t) * t;
    EXPECT_NEAR(io_val, expected, 1e-5f);
}

TEST(TweenTest, TByClamp) {
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::Linear, -1.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::Linear, 2.0f), 1.0f, 1e-5f);
}

TEST(TweenTest, LerpLinearInterpolationIsCorrect) {
    EXPECT_NEAR(dse::utils::Tween::Lerp(0.0f, 100.0f, 0.5f), 50.0f, 1e-5f);
    EXPECT_NEAR(dse::utils::Tween::Lerp(-10.0f, 10.0f, 0.5f), 0.0f, 1e-5f);
}

TEST(TweenTest, LerpCorrectInterpolationWithEasing) {
    float result = dse::utils::Tween::Lerp(0.0f, 100.0f, 0.5f, dse::utils::EaseType::EaseInQuad);
    float eased_t = 0.5f * 0.5f; // EaseInQuad(0.5) = 0.25
    EXPECT_NEAR(result, 100.0f * eased_t, 1e-3f);
}
