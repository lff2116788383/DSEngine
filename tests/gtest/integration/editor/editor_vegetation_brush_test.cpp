/**
 * @file editor_vegetation_brush_test.cpp
 * @brief 植被刷纯核心（editor_vegetation_brush_core）与密度遮罩采样（SampleVegetationMask）的无头测试。
 *
 * 覆盖：SampleVegetationMask（inactive→1 / 范围外→0 / 双线性插值）、
 * EnsureVegetationMask（按范围+分辨率初始化、范围变化重建、未变化保留）、
 * ApplyVegetationBrush（Plant 向 1、Clear 向 0、[0,1] 钳制、硬边半径、空遮罩 no-op）。
 * ImGui 面板 / 视口接线不在此覆盖。
 */

#include <gtest/gtest.h>

#include <cmath>

#include "editor_vegetation_brush_core.h"
#include "engine/ecs/vegetation_mask.h"

using namespace dse;
using namespace dse::editor;

namespace {
constexpr float kEps = 1e-4f;

// 以原点为中心、size x size、res x res、全 init 值的遮罩。
VegetationDensityMask MakeMask(int res, float size, float init) {
    VegetationDensityMask m;
    EnsureVegetationMask(m, glm::vec2(-size * 0.5f), glm::vec2(size), res, res, init);
    return m;
}
}  // namespace

// ── SampleVegetationMask ─────────────────────────────────────────────────────

TEST(VegetationMask, InactiveReturnsFullDensity) {
    VegetationDensityMask m;  // 空 → inactive
    EXPECT_FALSE(m.active());
    EXPECT_FLOAT_EQ(SampleVegetationMask(m, 0.0f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(SampleVegetationMask(m, 123.0f, -50.0f), 1.0f);
}

TEST(VegetationMask, OutsideExtentsReturnsZero) {
    auto m = MakeMask(5, 4.0f, 1.0f);  // 覆盖 [-2,2]x[-2,2]
    EXPECT_TRUE(m.active());
    EXPECT_FLOAT_EQ(SampleVegetationMask(m, 100.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(SampleVegetationMask(m, 0.0f, -100.0f), 0.0f);
}

TEST(VegetationMask, UniformMaskSamplesConstant) {
    auto m = MakeMask(8, 10.0f, 0.5f);
    EXPECT_NEAR(SampleVegetationMask(m, 0.0f, 0.0f), 0.5f, kEps);
    EXPECT_NEAR(SampleVegetationMask(m, -3.0f, 2.0f), 0.5f, kEps);
}

TEST(VegetationMask, BilinearInterpolatesBetweenCells) {
    // 2x2 遮罩，左列 0、右列 1，覆盖 [0,1]x[0,1]
    VegetationDensityMask m;
    m.resolution_x = 2;
    m.resolution_z = 2;
    m.world_min = glm::vec2(0.0f);
    m.world_size = glm::vec2(1.0f);
    m.weights = {0.0f, 1.0f,   // z=0 行: (x0,x1)
                 0.0f, 1.0f};  // z=1 行
    ASSERT_TRUE(m.active());
    EXPECT_NEAR(SampleVegetationMask(m, 0.0f, 0.0f), 0.0f, kEps);
    EXPECT_NEAR(SampleVegetationMask(m, 1.0f, 0.0f), 1.0f, kEps);
    EXPECT_NEAR(SampleVegetationMask(m, 0.5f, 0.0f), 0.5f, kEps);
    EXPECT_NEAR(SampleVegetationMask(m, 0.25f, 1.0f), 0.25f, kEps);
}

// ── EnsureVegetationMask ─────────────────────────────────────────────────────

TEST(VegetationMaskCore, EnsureInitializesSizeAndValue) {
    VegetationDensityMask m;
    EnsureVegetationMask(m, glm::vec2(-5.0f), glm::vec2(10.0f), 16, 16, 1.0f);
    EXPECT_TRUE(m.active());
    EXPECT_EQ(static_cast<int>(m.weights.size()), 16 * 16);
    for (float w : m.weights) EXPECT_FLOAT_EQ(w, 1.0f);
    EXPECT_FLOAT_EQ(m.world_min.x, -5.0f);
    EXPECT_FLOAT_EQ(m.world_size.x, 10.0f);
}

TEST(VegetationMaskCore, EnsureClampsResolutionToMinTwo) {
    VegetationDensityMask m;
    EnsureVegetationMask(m, glm::vec2(0.0f), glm::vec2(10.0f), 1, 1, 1.0f);
    EXPECT_EQ(m.resolution_x, 2);
    EXPECT_EQ(m.resolution_z, 2);
    EXPECT_EQ(static_cast<int>(m.weights.size()), 4);
}

TEST(VegetationMaskCore, EnsurePreservesWhenUnchanged) {
    auto m = MakeMask(8, 10.0f, 1.0f);
    m.weights[10] = 0.25f;  // 模拟已绘制内容
    EnsureVegetationMask(m, glm::vec2(-5.0f), glm::vec2(10.0f), 8, 8, 1.0f);
    EXPECT_FLOAT_EQ(m.weights[10], 0.25f);  // 未被重置
}

TEST(VegetationMaskCore, EnsureRebuildsOnExtentChange) {
    auto m = MakeMask(8, 10.0f, 1.0f);
    m.weights[10] = 0.25f;
    EnsureVegetationMask(m, glm::vec2(-5.0f), glm::vec2(10.0f), 16, 16, 1.0f);  // 分辨率变化
    EXPECT_EQ(static_cast<int>(m.weights.size()), 16 * 16);
    for (float w : m.weights) EXPECT_FLOAT_EQ(w, 1.0f);  // 全部重建为 init
}

// ── ApplyVegetationBrush ─────────────────────────────────────────────────────

TEST(VegetationBrushCore, NoOpOnInactiveMask) {
    VegetationDensityMask m;  // 空
    ApplyVegetationBrush(m, glm::vec3(0.0f), 5.0f, 1.0f, 0.0f, true, 1.0f);  // 不应崩溃
    EXPECT_FALSE(m.active());
}

TEST(VegetationBrushCore, PlantRaisesTowardOne) {
    auto m = MakeMask(33, 32.0f, 0.0f);  // 全空
    ApplyVegetationBrush(m, glm::vec3(0.0f), 4.0f, 1.0f, 0.0f, /*plant=*/true, 1.0f / 30.0f);
    float center = SampleVegetationMask(m, 0.0f, 0.0f);
    EXPECT_GT(center, 0.0f);
    EXPECT_LE(center, 1.0f);
    // 笔刷半径之外保持为 0
    EXPECT_FLOAT_EQ(SampleVegetationMask(m, 14.0f, 0.0f), 0.0f);
}

TEST(VegetationBrushCore, ClearLowersTowardZero) {
    auto m = MakeMask(33, 32.0f, 1.0f);  // 全满
    ApplyVegetationBrush(m, glm::vec3(0.0f), 4.0f, 1.0f, 0.0f, /*plant=*/false, 1.0f / 30.0f);
    float center = SampleVegetationMask(m, 0.0f, 0.0f);
    EXPECT_LT(center, 1.0f);
    EXPECT_GE(center, 0.0f);
    // 半径之外仍为 1
    EXPECT_NEAR(SampleVegetationMask(m, 14.0f, 0.0f), 1.0f, kEps);
}

TEST(VegetationBrushCore, PlantClampsAtOne) {
    auto m = MakeMask(33, 32.0f, 0.0f);
    // 强度极大 + 多次绘制 → 不应越过 1
    for (int i = 0; i < 20; ++i)
        ApplyVegetationBrush(m, glm::vec3(0.0f), 4.0f, 100.0f, 0.0f, true, 1.0f);
    for (float w : m.weights) {
        EXPECT_LE(w, 1.0f + kEps);
        EXPECT_GE(w, 0.0f);
    }
}

TEST(VegetationBrushCore, ClearClampsAtZero) {
    auto m = MakeMask(33, 32.0f, 1.0f);
    for (int i = 0; i < 20; ++i)
        ApplyVegetationBrush(m, glm::vec3(0.0f), 4.0f, 100.0f, 0.0f, false, 1.0f);
    for (float w : m.weights) {
        EXPECT_GE(w, -kEps);
        EXPECT_LE(w, 1.0f);
    }
}

TEST(VegetationBrushCore, HardEdgeFalloffOnlyAffectsInsideRadius) {
    auto m = MakeMask(65, 64.0f, 0.0f);  // 格距 1
    ApplyVegetationBrush(m, glm::vec3(0.0f), 5.0f, 1.0f, /*falloff=*/0.0f, true, 1.0f);
    // 半径内中心被抬升
    EXPECT_GT(SampleVegetationMask(m, 0.0f, 0.0f), 0.0f);
    // 半径外明显处仍为 0
    EXPECT_FLOAT_EQ(SampleVegetationMask(m, 20.0f, 20.0f), 0.0f);
}
