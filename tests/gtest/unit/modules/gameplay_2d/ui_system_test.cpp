/**
 * @file ui_system_test.cpp
 * @brief UISystem + UILayoutSystem 单元测试
 *
 * 测试策略：
 * - UILayoutSystem::CalculateScaleFactor 缩放因子计算
 * - UILayoutSystem::CalculateAnchorPosition 各锚点位置
 * - UISystem 空 registry 不崩溃
 * - UIAnchorData / GridLayoutData / CanvasScalerData 默认值
 * - UIButtonComponent / UIPanelComponent 默认值
 */

#include <gtest/gtest.h>
#include "modules/gameplay_2d/ui/ui_layout.h"
#include "modules/gameplay_2d/ui/ui_system.h"
#include "engine/ecs/ui.h"

#include <glm/glm.hpp>

using namespace dse::gameplay2d;

// ============================================================
// UIAnchorData / GridLayoutData / CanvasScalerData 默认值
// ============================================================

TEST(UIAnchorDataTest, 默认值) {
    UIAnchorData a;
    EXPECT_EQ(a.anchor, UIAnchor::MiddleCenter);
    EXPECT_FLOAT_EQ(a.offset.x, 0.0f);
    EXPECT_FLOAT_EQ(a.offset.y, 0.0f);
    EXPECT_FLOAT_EQ(a.size.x, 100.0f);
    EXPECT_FLOAT_EQ(a.size.y, 100.0f);
}

TEST(GridLayoutDataTest, 默认值) {
    GridLayoutData g;
    EXPECT_EQ(g.columns, 1);
    EXPECT_EQ(g.rows, 0);
    EXPECT_FLOAT_EQ(g.cell_size.x, 100.0f);
    EXPECT_FLOAT_EQ(g.cell_size.y, 100.0f);
    EXPECT_FLOAT_EQ(g.spacing.x, 10.0f);
    EXPECT_FLOAT_EQ(g.spacing.y, 10.0f);
    EXPECT_EQ(g.alignment, GridLayoutAlignment::UpperLeft);
}

TEST(CanvasScalerDataTest, 默认值) {
    CanvasScalerData cs;
    EXPECT_FLOAT_EQ(cs.reference_resolution.x, 1920.0f);
    EXPECT_FLOAT_EQ(cs.reference_resolution.y, 1080.0f);
    EXPECT_FLOAT_EQ(cs.scale_factor, 1.0f);
    EXPECT_TRUE(cs.match_width_or_height);
}

// ============================================================
// UIButtonComponent / UIPanelComponent 默认值
// ============================================================

TEST(UIButtonComponentTest, 默认值) {
    UIButtonComponent btn;
    EXPECT_FLOAT_EQ(btn.normal_color.r, 1.0f);
    EXPECT_FLOAT_EQ(btn.hover_color.r, 1.1f);
    EXPECT_FLOAT_EQ(btn.pressed_color.r, 0.8f);
}

TEST(UIPanelComponentTest, 默认值) {
    UIPanelComponent panel;
    EXPECT_FALSE(panel.blocks_input);
}

// ============================================================
// UILayoutSystem::CalculateScaleFactor
// ============================================================

TEST(UILayoutSystemTest, ScaleFactor_同分辨率为1) {
    UILayoutSystem layout;
    CanvasScalerData cs;
    cs.reference_resolution = glm::vec2(1920.0f, 1080.0f);
    float sf = layout.CalculateScaleFactor(glm::vec2(1920.0f, 1080.0f), cs);
    EXPECT_NEAR(sf, 1.0f, 0.01f);
}

TEST(UILayoutSystemTest, ScaleFactor_半分辨率约0点5) {
    UILayoutSystem layout;
    CanvasScalerData cs;
    cs.reference_resolution = glm::vec2(1920.0f, 1080.0f);
    float sf = layout.CalculateScaleFactor(glm::vec2(960.0f, 540.0f), cs);
    EXPECT_NEAR(sf, 0.5f, 0.01f);
}

TEST(UILayoutSystemTest, ScaleFactor_双倍分辨率约2) {
    UILayoutSystem layout;
    CanvasScalerData cs;
    cs.reference_resolution = glm::vec2(1920.0f, 1080.0f);
    float sf = layout.CalculateScaleFactor(glm::vec2(3840.0f, 2160.0f), cs);
    EXPECT_NEAR(sf, 2.0f, 0.01f);
}

// ============================================================
// UILayoutSystem::CalculateAnchorPosition
// ============================================================

TEST(UILayoutSystemTest, Anchor_TopLeft) {
    UILayoutSystem layout;
    UIAnchorData a;
    a.anchor = UIAnchor::TopLeft;
    a.offset = glm::vec2(0.0f);
    a.size = glm::vec2(100.0f);
    glm::vec2 pos = layout.CalculateAnchorPosition(a, glm::vec2(1920, 1080), 1.0f);
    // TopLeft: x 靠近 0, y 靠近 0（y-down 坐标系）
    EXPECT_NEAR(pos.x, 0.0f, 60.0f);
    EXPECT_LT(pos.y, 100.0f);
}

TEST(UILayoutSystemTest, Anchor_MiddleCenter) {
    UILayoutSystem layout;
    UIAnchorData a;
    a.anchor = UIAnchor::MiddleCenter;
    a.offset = glm::vec2(0.0f);
    a.size = glm::vec2(100.0f);
    glm::vec2 pos = layout.CalculateAnchorPosition(a, glm::vec2(1920, 1080), 1.0f);
    EXPECT_NEAR(pos.x, 960.0f, 10.0f);
    EXPECT_NEAR(pos.y, 540.0f, 10.0f);
}

TEST(UILayoutSystemTest, Anchor_BottomRight) {
    UILayoutSystem layout;
    UIAnchorData a;
    a.anchor = UIAnchor::BottomRight;
    a.offset = glm::vec2(0.0f);
    a.size = glm::vec2(100.0f);
    glm::vec2 pos = layout.CalculateAnchorPosition(a, glm::vec2(1920, 1080), 1.0f);
    EXPECT_GT(pos.x, 1800.0f);
    EXPECT_GT(pos.y, 900.0f);
}

// ============================================================
// UISystem 空 registry
// ============================================================

TEST(UISystemTest, 默认构造安全) {
    UISystem sys;
    (void)sys;
}

TEST(UISystemTest, 空Registry不崩溃) {
    UISystem sys;
    entt::registry reg;
    sys.Update(reg, 1.0f / 60.0f, glm::vec2(1920, 1080), glm::vec2(0), false);
}

TEST(UISystemTest, 零dt不崩溃) {
    UISystem sys;
    entt::registry reg;
    sys.Update(reg, 0.0f, glm::vec2(1920, 1080), glm::vec2(960, 540), false);
}

// ============================================================
// UILayoutSystem 空 registry
// ============================================================

TEST(UILayoutSystemTest, 默认构造安全) {
    UILayoutSystem layout;
    (void)layout;
}

TEST(UILayoutSystemTest, 空Registry不崩溃) {
    UILayoutSystem layout;
    entt::registry reg;
    layout.Update(reg, glm::vec2(1920, 1080));
}
