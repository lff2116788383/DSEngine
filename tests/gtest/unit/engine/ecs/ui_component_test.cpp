/**
 * @file ui_component_test.cpp
 * @brief UI 组件数据结构的单元测试
 *
 * 覆盖场景：
 * - UIRendererComponent 默认值
 * - UIPanelComponent 默认值
 * - UIButtonComponent 默认值
 * - UILabelComponent 默认值与数字模式
 * - UIMaskComponent 默认值
 * - UIRichTextComponent 默认值
 * - UIJoystickComponent 默认值
 * - UIAnchorComponent 默认值
 * - UIGridLayoutComponent 默认值
 * - UICanvasScalerComponent 默认值
 * - UIAnimationComponent 默认值
 */

#include <gtest/gtest.h>
#include <entt/entt.hpp>
#include "engine/ecs/ui.h"

// ============================================================
// UIRendererComponent
// ============================================================

TEST(UIRendererComponentTest, 默认值) {
    UIRendererComponent ui;
    EXPECT_EQ(ui.texture_handle, 0u);
    EXPECT_FLOAT_EQ(ui.color.r, 1.0f);
    EXPECT_FLOAT_EQ(ui.color.a, 1.0f);
    EXPECT_EQ(ui.order, 0);
    EXPECT_TRUE(ui.visible);
    EXPECT_TRUE(ui.interactable);
    EXPECT_FALSE(ui.is_hovered);
    EXPECT_FALSE(ui.is_pressed);
    EXPECT_FLOAT_EQ(ui.scale, 1.0f);
    EXPECT_FLOAT_EQ(ui.hover_scale, 1.08f);
    EXPECT_FLOAT_EQ(ui.pressed_scale, 0.94f);
}

// ============================================================
// UIPanelComponent
// ============================================================

TEST(UIPanelComponentTest, 默认不阻挡输入) {
    UIPanelComponent panel;
    EXPECT_FALSE(panel.blocks_input);
}

// ============================================================
// UIButtonComponent
// ============================================================

TEST(UIButtonComponentTest, 默认颜色) {
    UIButtonComponent btn;
    EXPECT_FLOAT_EQ(btn.normal_color.r, 1.0f);
    EXPECT_FLOAT_EQ(btn.hover_color.r, 1.1f);
    EXPECT_FLOAT_EQ(btn.pressed_color.r, 0.8f);
}

// ============================================================
// UILabelComponent
// ============================================================

TEST(UILabelComponentTest, 默认值) {
    UILabelComponent label;
    EXPECT_TRUE(label.text.empty());
    EXPECT_FALSE(label.use_localization);
    EXPECT_FALSE(label.numeric_mode);
    EXPECT_EQ(label.number_value, 0);
    EXPECT_EQ(label.atlas_cols, 16);
    EXPECT_EQ(label.atlas_rows, 6);
    EXPECT_EQ(label.ascii_start, 32);
    EXPECT_TRUE(label.dirty);
}

TEST(UILabelComponentTest, 数字模式) {
    UILabelComponent label;
    label.numeric_mode = true;
    label.number_value = 12345;
    EXPECT_TRUE(label.numeric_mode);
    EXPECT_EQ(label.number_value, 12345);
}

// ============================================================
// UIMaskComponent
// ============================================================

TEST(UIMaskComponentTest, 默认值) {
    UIMaskComponent mask;
    EXPECT_TRUE(mask.enabled);
    EXPECT_TRUE(mask.block_outside_input);
    EXPECT_FLOAT_EQ(mask.size.x, 0.0f);
}

// ============================================================
// UIRichTextComponent
// ============================================================

TEST(UIRichTextComponentTest, 默认值) {
    UIRichTextComponent rich;
    EXPECT_TRUE(rich.text.empty());
    EXPECT_FLOAT_EQ(rich.default_color.r, 1.0f);
    EXPECT_FALSE(rich.enable_shadow);
    EXPECT_FALSE(rich.enable_outline);
    EXPECT_TRUE(rich.dirty);
}

// ============================================================
// UIJoystickComponent
// ============================================================

TEST(UIJoystickComponentTest, 默认值) {
    UIJoystickComponent joystick;
    EXPECT_FLOAT_EQ(joystick.direction.x, 0.0f);
    EXPECT_FLOAT_EQ(joystick.max_radius, 64.0f);
    EXPECT_TRUE(joystick.follow_pointer);
    EXPECT_TRUE(joystick.reset_on_release);
    EXPECT_FALSE(joystick.is_dragging);
}

// ============================================================
// UIAnchorComponent
// ============================================================

TEST(UIAnchorComponentTest, 默认值) {
    UIAnchorComponent anchor;
    EXPECT_EQ(anchor.anchor, 5);
    EXPECT_FLOAT_EQ(anchor.offset.x, 0.0f);
}

// ============================================================
// UIGridLayoutComponent
// ============================================================

TEST(UIGridLayoutComponentTest, 默认值) {
    UIGridLayoutComponent grid;
    EXPECT_EQ(grid.columns, 1);
    EXPECT_EQ(grid.rows, 0);
    EXPECT_FLOAT_EQ(grid.cell_size.x, 100.0f);
    EXPECT_FLOAT_EQ(grid.spacing.x, 10.0f);
}

// ============================================================
// UICanvasScalerComponent
// ============================================================

TEST(UICanvasScalerComponentTest, 默认值) {
    UICanvasScalerComponent scaler;
    EXPECT_FLOAT_EQ(scaler.reference_resolution.x, 1920.0f);
    EXPECT_FLOAT_EQ(scaler.reference_resolution.y, 1080.0f);
    EXPECT_FLOAT_EQ(scaler.scale_factor, 1.0f);
    EXPECT_TRUE(scaler.match_width_or_height);
}

// ============================================================
// UIAnimationComponent
// ============================================================

TEST(UIAnimationComponentTest, 默认值) {
    UIAnimationComponent anim;
    EXPECT_FLOAT_EQ(anim.duration, 0.3f);
    EXPECT_FLOAT_EQ(anim.elapsed, 0.0f);
    EXPECT_FALSE(anim.playing);
    EXPECT_FALSE(anim.loop);
    EXPECT_FALSE(anim.ping_pong);
    EXPECT_EQ(anim.easing, 0);
}
