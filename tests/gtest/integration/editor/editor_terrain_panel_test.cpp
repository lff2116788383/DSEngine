/**
 * @file editor_terrain_panel_test.cpp
 * @brief 地形雕刻 / splat 绘制纯核心（editor_terrain_panel_core）的无头测试。
 *
 * 覆盖：WorldToScreen / ScreenToWorldOnTerrain（坐标投影往返）、WorldToTerrainGrid（世界↔格点）、
 * GaussianFalloff（硬边/软边衰减）、ApplyBrush（Raise/Lower/Smooth/Flatten 高度笔刷）、
 * EnsureSplatData（4 层权重初始化）、ApplySplatBrush（active 层累加 + 其余归一化）。
 * ImGui 面板 / 视口接线不在此覆盖。
 */

#include <gtest/gtest.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "editor_terrain_panel.h"
#include "editor_terrain_panel_core.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/transform.h"

using namespace dse;
using namespace dse::editor;

namespace {
constexpr float kEps = 1e-3f;

// 64x64、100x100 单位、max_height 20 的平地地形（height_data 全 0）。
TerrainComponent MakeFlatTerrain(int res = 5, float extent = 4.0f) {
    TerrainComponent t;
    t.width = extent;
    t.depth = extent;
    t.max_height = 20.0f;
    t.resolution_x = res;
    t.resolution_z = res;
    t.height_data.assign(static_cast<size_t>(res) * res, 0.0f);
    return t;
}

TransformComponent MakeTransformAt(float x, float y, float z) {
    TransformComponent tf;
    tf.position = glm::vec3(x, y, z);
    return tf;
}
}  // namespace

// ── GaussianFalloff ──────────────────────────────────────────────────────────

TEST(TerrainCore, FalloffZeroBeyondRadius) {
    EXPECT_FLOAT_EQ(GaussianFalloff(5.0f, 5.0f, 0.5f), 0.0f);
    EXPECT_FLOAT_EQ(GaussianFalloff(6.0f, 5.0f, 1.0f), 0.0f);
}

TEST(TerrainCore, FalloffHardEdgeIsConstantOne) {
    // falloff=0 → 硬边，半径内恒 1
    EXPECT_FLOAT_EQ(GaussianFalloff(0.0f, 5.0f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(GaussianFalloff(2.5f, 5.0f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(GaussianFalloff(4.99f, 5.0f, 0.0f), 1.0f);
}

TEST(TerrainCore, FalloffSoftEdgeDecays) {
    // falloff=1 → 高斯，中心 1，向边缘单调递减
    EXPECT_FLOAT_EQ(GaussianFalloff(0.0f, 5.0f, 1.0f), 1.0f);
    float mid = GaussianFalloff(2.5f, 5.0f, 1.0f);
    float edge = GaussianFalloff(4.5f, 5.0f, 1.0f);
    EXPECT_LT(mid, 1.0f);
    EXPECT_LT(edge, mid);
    EXPECT_GT(edge, 0.0f);
}

// ── WorldToTerrainGrid ───────────────────────────────────────────────────────

TEST(TerrainCore, WorldToGridCenterMapsToMiddle) {
    auto t = MakeFlatTerrain(5, 4.0f);  // res 5, extent 4 → 格距 1
    auto tf = MakeTransformAt(0, 0, 0);
    float gx, gz;
    bool inside = WorldToTerrainGrid(glm::vec3(0, 0, 0), t, tf, gx, gz);
    EXPECT_TRUE(inside);
    EXPECT_NEAR(gx, 2.0f, kEps);  // (0 + 2)/4 * (5-1) = 2
    EXPECT_NEAR(gz, 2.0f, kEps);
}

TEST(TerrainCore, WorldToGridCornerAndOutside) {
    auto t = MakeFlatTerrain(5, 4.0f);
    auto tf = MakeTransformAt(0, 0, 0);
    float gx, gz;
    EXPECT_TRUE(WorldToTerrainGrid(glm::vec3(-2, 0, -2), t, tf, gx, gz));
    EXPECT_NEAR(gx, 0.0f, kEps);
    EXPECT_NEAR(gz, 0.0f, kEps);
    // 明显在范围之外
    EXPECT_FALSE(WorldToTerrainGrid(glm::vec3(100, 0, 0), t, tf, gx, gz));
}

TEST(TerrainCore, WorldToGridRespectsTransformOffset) {
    auto t = MakeFlatTerrain(5, 4.0f);
    auto tf = MakeTransformAt(10, 0, -5);
    float gx, gz;
    bool inside = WorldToTerrainGrid(glm::vec3(10, 0, -5), t, tf, gx, gz);
    EXPECT_TRUE(inside);
    EXPECT_NEAR(gx, 2.0f, kEps);
    EXPECT_NEAR(gz, 2.0f, kEps);
}

// ── WorldToScreen / ScreenToWorldOnTerrain 往返 ──────────────────────────────

TEST(TerrainCore, ScreenWorldRoundTrip) {
    glm::mat4 view = glm::lookAt(glm::vec3(0, 20, 20), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 1000.0f);
    glm::vec2 win(0, 0), size(800, 600);

    glm::vec3 world(3.0f, 0.0f, -2.0f);
    glm::vec2 screen = WorldToScreen(world, view, proj, win, size);
    // 投影点应落在视口内
    EXPECT_GT(screen.x, 0.0f);
    EXPECT_LT(screen.x, size.x);

    glm::vec3 back = ScreenToWorldOnTerrain(screen, view, proj, win, size, 0.0f);
    EXPECT_NEAR(back.x, world.x, 1e-2f);
    EXPECT_NEAR(back.z, world.z, 1e-2f);
    EXPECT_NEAR(back.y, 0.0f, 1e-2f);
}

TEST(TerrainCore, WorldToScreenBehindCameraSentinel) {
    glm::mat4 view(1.0f);  // identity → 相机在原点看 -Z
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    glm::vec2 win(0, 0), size(800, 600);
    // w 退化 → 远点哨兵
    glm::vec2 s = WorldToScreen(glm::vec3(0, 0, 0), view, proj, win, size);
    EXPECT_LT(s.x, -5000.0f);
}

// ── ApplyBrush ───────────────────────────────────────────────────────────────

TEST(TerrainCore, ApplyBrushRaiseIncreasesCenter) {
    auto t = MakeFlatTerrain(9, 8.0f);
    auto tf = MakeTransformAt(0, 0, 0);
    TerrainEditorState s;
    s.brush_mode = TerrainBrushMode::Raise;
    s.brush_radius = 2.0f;
    s.brush_strength = 1.0f;
    s.brush_falloff = 0.0f;  // 硬边方便断言
    ApplyBrush(t, tf, glm::vec3(0, 0, 0), s, 1.0f / 30.0f);  // strength*delta*30 = 1

    int center = 4 * t.resolution_x + 4;
    EXPECT_GT(t.height_data[center], 0.0f);
    EXPECT_TRUE(t.is_dirty);
}

TEST(TerrainCore, ApplyBrushLowerClampsAtZero) {
    auto t = MakeFlatTerrain(9, 8.0f);
    auto tf = MakeTransformAt(0, 0, 0);
    TerrainEditorState s;
    s.brush_mode = TerrainBrushMode::Lower;
    s.brush_radius = 2.0f;
    s.brush_strength = 1.0f;
    s.brush_falloff = 0.0f;
    ApplyBrush(t, tf, glm::vec3(0, 0, 0), s, 1.0f / 30.0f);
    // 平地从 0 下挖，钳到 0
    for (float h : t.height_data) EXPECT_GE(h, 0.0f);
}

TEST(TerrainCore, ApplyBrushRaiseClampsAtMaxHeight) {
    auto t = MakeFlatTerrain(9, 8.0f);
    t.max_height = 1.0f;
    auto tf = MakeTransformAt(0, 0, 0);
    TerrainEditorState s;
    s.brush_mode = TerrainBrushMode::Raise;
    s.brush_radius = 2.0f;
    s.brush_strength = 100.0f;  // 远超 max
    s.brush_falloff = 0.0f;
    ApplyBrush(t, tf, glm::vec3(0, 0, 0), s, 1.0f);
    for (float h : t.height_data) EXPECT_LE(h, 1.0f + kEps);
}

TEST(TerrainCore, ApplyBrushFlattenMovesTowardTarget) {
    auto t = MakeFlatTerrain(9, 8.0f);
    // 起始全 5
    for (float& h : t.height_data) h = 5.0f;
    auto tf = MakeTransformAt(0, 0, 0);
    TerrainEditorState s;
    s.brush_mode = TerrainBrushMode::Flatten;
    s.flatten_target_height = 2.0f;
    s.brush_radius = 2.0f;
    s.brush_strength = 0.5f;
    s.brush_falloff = 0.0f;
    int center = 4 * t.resolution_x + 4;
    float before = t.height_data[center];
    ApplyBrush(t, tf, glm::vec3(0, 0, 0), s, 1.0f / 30.0f);
    // 向 target(2) 靠拢 → 比之前低，但不越过 target
    EXPECT_LT(t.height_data[center], before);
    EXPECT_GE(t.height_data[center], 2.0f);
}

TEST(TerrainCore, ApplyBrushSmoothLeavesUniformFieldUnchanged) {
    auto t = MakeFlatTerrain(9, 8.0f);
    for (float& h : t.height_data) h = 7.0f;  // 均匀场，smooth 后仍 7
    auto tf = MakeTransformAt(0, 0, 0);
    TerrainEditorState s;
    s.brush_mode = TerrainBrushMode::Smooth;
    s.brush_radius = 2.0f;
    s.brush_strength = 1.0f;
    s.brush_falloff = 0.0f;
    ApplyBrush(t, tf, glm::vec3(0, 0, 0), s, 1.0f / 30.0f);
    int center = 4 * t.resolution_x + 4;
    EXPECT_NEAR(t.height_data[center], 7.0f, kEps);
}

TEST(TerrainCore, ApplyBrushNoOpOnEmptyHeightData) {
    TerrainComponent t;  // height_data 空
    auto tf = MakeTransformAt(0, 0, 0);
    TerrainEditorState s;
    ApplyBrush(t, tf, glm::vec3(0, 0, 0), s, 1.0f);  // 不应崩溃
    EXPECT_TRUE(t.height_data.empty());
}

// ── EnsureSplatData / ApplySplatBrush ────────────────────────────────────────

TEST(TerrainCore, EnsureSplatDataInitsLayer0) {
    auto t = MakeFlatTerrain(4, 4.0f);
    EnsureSplatData(t);
    EXPECT_EQ(static_cast<int>(t.splat_data.size()), 4 * 4 * 4);
    for (int i = 0; i < 4 * 4; i++) {
        EXPECT_FLOAT_EQ(t.splat_data[i * 4 + 0], 1.0f);
        EXPECT_FLOAT_EQ(t.splat_data[i * 4 + 1], 0.0f);
        EXPECT_FLOAT_EQ(t.splat_data[i * 4 + 2], 0.0f);
        EXPECT_FLOAT_EQ(t.splat_data[i * 4 + 3], 0.0f);
    }
}

TEST(TerrainCore, ApplySplatBrushIncreasesActiveLayerAndKeepsSumNormalized) {
    auto t = MakeFlatTerrain(9, 8.0f);
    auto tf = MakeTransformAt(0, 0, 0);
    TerrainEditorState s;
    s.active_splat_layer = 1;
    s.brush_radius = 2.0f;
    s.brush_falloff = 0.0f;
    s.splat_brush_opacity = 0.5f;
    ApplySplatBrush(t, tf, glm::vec3(0, 0, 0), s, 1.0f / 30.0f);

    int center = (4 * t.resolution_x + 4) * 4;
    EXPECT_GT(t.splat_data[center + 1], 0.0f);          // layer1 上升
    EXPECT_LT(t.splat_data[center + 0], 1.0f);          // layer0 被挤压
    float sum = t.splat_data[center + 0] + t.splat_data[center + 1] +
                t.splat_data[center + 2] + t.splat_data[center + 3];
    EXPECT_NEAR(sum, 1.0f, kEps);                       // 和保持 ~1
    EXPECT_TRUE(t.splat_dirty);
}
