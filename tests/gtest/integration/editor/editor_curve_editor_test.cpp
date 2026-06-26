/**
 * @file editor_curve_editor_test.cpp
 * @brief 曲线编辑器纯逻辑核心（editor_curve_editor_core）的无头测试。
 *
 * 验证 EditorCurve::Evaluate（线性 / 三次 Hermite 插值、边界钳制、退化段）、
 * EditorCurve::SortKeys、MakeDefaultCurve。ImGui 画布交互（DrawCurveEditor）不在此覆盖。
 */

#include <gtest/gtest.h>

#include <cmath>

#include "editor_curve_editor.h"

using namespace dse::editor;

namespace {
constexpr float kEps = 1e-4f;

EditorCurve MakeKeys(std::vector<CurveKey> keys, CurveInterp interp) {
    EditorCurve c;
    c.keys = std::move(keys);
    c.interp = interp;
    return c;
}
}  // namespace

// ── Evaluate：退化输入 ────────────────────────────────────────────────────────

TEST(CurveEditor, EvaluateEmptyReturnsZero) {
    EditorCurve c;
    EXPECT_FLOAT_EQ(c.Evaluate(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(c.Evaluate(5.0f), 0.0f);
}

TEST(CurveEditor, EvaluateSingleKeyIsConstant) {
    auto c = MakeKeys({{0.5f, 3.0f, 0.0f, 0.0f}}, CurveInterp::Cubic);
    EXPECT_FLOAT_EQ(c.Evaluate(-1.0f), 3.0f);
    EXPECT_FLOAT_EQ(c.Evaluate(0.5f), 3.0f);
    EXPECT_FLOAT_EQ(c.Evaluate(10.0f), 3.0f);
}

// ── Evaluate：边界钳制 ────────────────────────────────────────────────────────

TEST(CurveEditor, EvaluateClampsBelowFirstAndAboveLast) {
    auto c = MakeKeys({{0.0f, 1.0f, 0.0f, 0.0f}, {1.0f, 5.0f, 0.0f, 0.0f}}, CurveInterp::Linear);
    EXPECT_FLOAT_EQ(c.Evaluate(-2.0f), 1.0f);   // before first -> front value
    EXPECT_FLOAT_EQ(c.Evaluate(0.0f), 1.0f);    // exactly first
    EXPECT_FLOAT_EQ(c.Evaluate(1.0f), 5.0f);    // exactly last
    EXPECT_FLOAT_EQ(c.Evaluate(3.0f), 5.0f);    // after last -> back value
}

// ── Evaluate：线性插值 ────────────────────────────────────────────────────────

TEST(CurveEditor, EvaluateLinearMidpoint) {
    auto c = MakeKeys({{0.0f, 0.0f, 0.0f, 0.0f}, {2.0f, 10.0f, 0.0f, 0.0f}}, CurveInterp::Linear);
    EXPECT_NEAR(c.Evaluate(1.0f), 5.0f, kEps);     // halfway
    EXPECT_NEAR(c.Evaluate(0.5f), 2.5f, kEps);     // quarter
    EXPECT_NEAR(c.Evaluate(1.5f), 7.5f, kEps);     // three-quarter
}

TEST(CurveEditor, EvaluateLinearMultiSegment) {
    auto c = MakeKeys({{0.0f, 0.0f, 0.0f, 0.0f},
                       {1.0f, 10.0f, 0.0f, 0.0f},
                       {2.0f, 0.0f, 0.0f, 0.0f}}, CurveInterp::Linear);
    EXPECT_NEAR(c.Evaluate(0.5f), 5.0f, kEps);     // rising segment
    EXPECT_NEAR(c.Evaluate(1.0f), 10.0f, kEps);    // peak (exact key)
    EXPECT_NEAR(c.Evaluate(1.5f), 5.0f, kEps);     // falling segment
}

// ── Evaluate：三次 Hermite ───────────────────────────────────────────────────

TEST(CurveEditor, EvaluateCubicHitsKeyValuesExactly) {
    // 零切线的三次 Hermite 在关键帧处应精确等于关键帧值（h00=1, h01=0 at s=0）。
    auto c = MakeKeys({{0.0f, 2.0f, 0.0f, 0.0f}, {1.0f, 8.0f, 0.0f, 0.0f}}, CurveInterp::Cubic);
    EXPECT_NEAR(c.Evaluate(0.0f), 2.0f, kEps);
    EXPECT_NEAR(c.Evaluate(1.0f), 8.0f, kEps);
}

TEST(CurveEditor, EvaluateCubicZeroTangentMidpointIsSmoothstep) {
    // 零切线 Hermite 在 s=0.5: h00=h01=0.5 -> 平均值。
    auto c = MakeKeys({{0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 10.0f, 0.0f, 0.0f}}, CurveInterp::Cubic);
    EXPECT_NEAR(c.Evaluate(0.5f), 5.0f, kEps);
}

TEST(CurveEditor, EvaluateCubicMonotonicBetweenKeys) {
    // 升序关键帧 + 零切线：插值应单调不减且落在端点值之间。
    auto c = MakeKeys({{0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 10.0f, 0.0f, 0.0f}}, CurveInterp::Cubic);
    float prev = c.Evaluate(0.0f);
    for (float t = 0.05f; t <= 1.0f + kEps; t += 0.05f) {
        float v = c.Evaluate(t);
        EXPECT_GE(v + kEps, prev);
        EXPECT_GE(v, -kEps);
        EXPECT_LE(v, 10.0f + kEps);
        prev = v;
    }
}

TEST(CurveEditor, EvaluateDegenerateSegmentReturnsLeftValue) {
    // 两个关键帧 time 相同（dt < 1e-6）：应返回左侧值，不除零。
    auto c = MakeKeys({{1.0f, 4.0f, 0.0f, 0.0f}, {1.0f, 9.0f, 0.0f, 0.0f}}, CurveInterp::Linear);
    // t==1.0 hits front-clamp (t <= front.time) -> front value 4.
    EXPECT_FLOAT_EQ(c.Evaluate(1.0f), 4.0f);
}

// ── SortKeys ─────────────────────────────────────────────────────────────────

TEST(CurveEditor, SortKeysOrdersByTime) {
    auto c = MakeKeys({{2.0f, 20.0f, 0.0f, 0.0f},
                       {0.0f, 0.0f, 0.0f, 0.0f},
                       {1.0f, 10.0f, 0.0f, 0.0f}}, CurveInterp::Linear);
    c.SortKeys();
    ASSERT_EQ(c.keys.size(), 3u);
    EXPECT_FLOAT_EQ(c.keys[0].time, 0.0f);
    EXPECT_FLOAT_EQ(c.keys[1].time, 1.0f);
    EXPECT_FLOAT_EQ(c.keys[2].time, 2.0f);
    // values carried with their keys
    EXPECT_FLOAT_EQ(c.keys[0].value, 0.0f);
    EXPECT_FLOAT_EQ(c.keys[2].value, 20.0f);
}

TEST(CurveEditor, EvaluateAfterSortIsCorrect) {
    auto c = MakeKeys({{2.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
                       {1.0f, 10.0f, 0.0f, 0.0f}}, CurveInterp::Linear);
    c.SortKeys();
    EXPECT_NEAR(c.Evaluate(0.5f), 5.0f, kEps);
    EXPECT_NEAR(c.Evaluate(1.5f), 5.0f, kEps);
}

// ── MakeDefaultCurve ─────────────────────────────────────────────────────────

TEST(CurveEditor, MakeDefaultCurveIsUnitRamp) {
    auto c = MakeDefaultCurve("alpha");
    EXPECT_EQ(c.name, "alpha");
    ASSERT_EQ(c.keys.size(), 2u);
    EXPECT_FLOAT_EQ(c.keys[0].time, 0.0f);
    EXPECT_FLOAT_EQ(c.keys[0].value, 0.0f);
    EXPECT_FLOAT_EQ(c.keys[1].time, 1.0f);
    EXPECT_FLOAT_EQ(c.keys[1].value, 1.0f);
}

TEST(CurveEditor, MakeDefaultCurveCustomRange) {
    auto c = MakeDefaultCurve("h", 100.0f, 200.0f);
    ASSERT_EQ(c.keys.size(), 2u);
    EXPECT_FLOAT_EQ(c.keys[0].value, 100.0f);
    EXPECT_FLOAT_EQ(c.keys[1].value, 200.0f);
    // default interp is Cubic; with zero tangents midpoint = average
    EXPECT_NEAR(c.Evaluate(0.5f), 150.0f, kEps);
}
