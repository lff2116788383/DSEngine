/**
 * @file bezier_test.cpp
 * @brief BezierCurve2D 单元测试
 *
 * 覆盖场景：
 * - 二次贝塞尔曲线端点值正确
 * - 三次贝塞尔曲线端点值正确
 * - 中间参数插值正确
 * - 边界参数 t=0 和 t=1
 * - 线性贝塞尔（退化为直线）
 */

#include <gtest/gtest.h>
#include "engine/base/bezier.h"
#include <glm/glm.hpp>

using namespace dse::math;

// ============================================================
// 二次贝塞尔曲线
// ============================================================

TEST(BezierCurve2DTest, 二次贝塞尔起点等于p0) {
    glm::vec2 p0(0.0f, 0.0f);
    glm::vec2 p1(1.0f, 2.0f);
    glm::vec2 p2(2.0f, 0.0f);
    auto result = BezierCurve2D::EvaluateQuadratic(0.0f, p0, p1, p2);
    EXPECT_FLOAT_EQ(result.x, p0.x);
    EXPECT_FLOAT_EQ(result.y, p0.y);
}

TEST(BezierCurve2DTest, 二次贝塞尔终点等于p2) {
    glm::vec2 p0(0.0f, 0.0f);
    glm::vec2 p1(1.0f, 2.0f);
    glm::vec2 p2(2.0f, 0.0f);
    auto result = BezierCurve2D::EvaluateQuadratic(1.0f, p0, p1, p2);
    EXPECT_FLOAT_EQ(result.x, p2.x);
    EXPECT_FLOAT_EQ(result.y, p2.y);
}

TEST(BezierCurve2DTest, 二次贝塞尔中点值正确) {
    // t=0.5 时二次贝塞尔公式: (1-t)^2*p0 + 2*(1-t)*t*p1 + t^2*p2
    glm::vec2 p0(0.0f, 0.0f);
    glm::vec2 p1(1.0f, 2.0f);
    glm::vec2 p2(2.0f, 0.0f);
    auto result = BezierCurve2D::EvaluateQuadratic(0.5f, p0, p1, p2);
    // 0.25*0 + 2*0.5*0.5*1 + 0.25*2 = 0 + 0.5 + 0.5 = 1.0
    EXPECT_FLOAT_EQ(result.x, 1.0f);
    // 0.25*0 + 2*0.5*0.5*2 + 0.25*0 = 0 + 1.0 + 0 = 1.0
    EXPECT_FLOAT_EQ(result.y, 1.0f);
}

TEST(BezierCurve2DTest, 二次贝塞尔退化为直线) {
    // 当控制点在起终点连线上时，曲线退化为直线
    glm::vec2 p0(0.0f, 0.0f);
    glm::vec2 p1(1.0f, 1.0f); // p1 在 p0->p2 的连线上
    glm::vec2 p2(2.0f, 2.0f);
    auto result = BezierCurve2D::EvaluateQuadratic(0.25f, p0, p1, p2);
    EXPECT_FLOAT_EQ(result.x, 0.5f);
    EXPECT_FLOAT_EQ(result.y, 0.5f);
}

// ============================================================
// 三次贝塞尔曲线
// ============================================================

TEST(BezierCurve2DTest, 三次贝塞尔起点等于p0) {
    glm::vec2 p0(0.0f, 0.0f);
    glm::vec2 p1(1.0f, 3.0f);
    glm::vec2 p2(3.0f, 3.0f);
    glm::vec2 p3(4.0f, 0.0f);
    auto result = BezierCurve2D::EvaluateCubic(0.0f, p0, p1, p2, p3);
    EXPECT_FLOAT_EQ(result.x, p0.x);
    EXPECT_FLOAT_EQ(result.y, p0.y);
}

TEST(BezierCurve2DTest, 三次贝塞尔终点等于p3) {
    glm::vec2 p0(0.0f, 0.0f);
    glm::vec2 p1(1.0f, 3.0f);
    glm::vec2 p2(3.0f, 3.0f);
    glm::vec2 p3(4.0f, 0.0f);
    auto result = BezierCurve2D::EvaluateCubic(1.0f, p0, p1, p2, p3);
    EXPECT_FLOAT_EQ(result.x, p3.x);
    EXPECT_FLOAT_EQ(result.y, p3.y);
}

TEST(BezierCurve2DTest, 三次贝塞尔中点值正确) {
    glm::vec2 p0(0.0f, 0.0f);
    glm::vec2 p1(0.0f, 2.0f);
    glm::vec2 p2(2.0f, 2.0f);
    glm::vec2 p3(2.0f, 0.0f);
    auto result = BezierCurve2D::EvaluateCubic(0.5f, p0, p1, p2, p3);
    // (1-0.5)^3*0 + 3*(1-0.5)^2*0.5*0 + 3*(1-0.5)*0.5^2*2 + 0.5^3*2
    // x = 0 + 0 + 3*0.25*0.25*2 + 0.125*2 = 0 + 0 + 0.375 + 0.25 = 0.625
    // wait let me recalculate:
    // x: u3*p0.x + 3*u2*t*p1.x + 3*u*t2*p2.x + t3*p3.x
    //    0.125*0 + 3*0.25*0.5*0 + 3*0.5*0.25*2 + 0.125*2
    //    0 + 0 + 0.75 + 0.25 = 1.0
    EXPECT_FLOAT_EQ(result.x, 1.0f);
    // y: 0.125*0 + 3*0.25*0.5*2 + 3*0.5*0.25*2 + 0.125*0
    //    0 + 0.75 + 0.75 + 0 = 1.5
    EXPECT_FLOAT_EQ(result.y, 1.5f);
}

TEST(BezierCurve2DTest, 三次贝塞尔单调递增参数) {
    glm::vec2 p0(0.0f, 0.0f);
    glm::vec2 p1(1.0f, 1.0f);
    glm::vec2 p2(2.0f, 1.0f);
    glm::vec2 p3(3.0f, 0.0f);
    auto r0 = BezierCurve2D::EvaluateCubic(0.0f, p0, p1, p2, p3);
    auto r1 = BezierCurve2D::EvaluateCubic(0.25f, p0, p1, p2, p3);
    auto r2 = BezierCurve2D::EvaluateCubic(0.5f, p0, p1, p2, p3);
    auto r3 = BezierCurve2D::EvaluateCubic(0.75f, p0, p1, p2, p3);
    auto r4 = BezierCurve2D::EvaluateCubic(1.0f, p0, p1, p2, p3);
    // X 坐标应随 t 单调递增
    EXPECT_LT(r0.x, r1.x);
    EXPECT_LT(r1.x, r2.x);
    EXPECT_LT(r2.x, r3.x);
    EXPECT_LT(r3.x, r4.x);
}
