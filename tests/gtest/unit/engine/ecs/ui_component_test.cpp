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

// 测试 UI渲染器组件：默认值
TEST(UIRendererComponentTest, DefaultValues) {
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

// 测试 UI渲染器组件：Jiugongge为禁用按默认
TEST(UIRendererComponentTest, JiugonggeIsDisabledByDefault) {
    UIRendererComponent ui;
    EXPECT_FALSE(ui.nine_slice_enabled);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.x, 0.0f);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.y, 0.0f);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.z, 0.0f);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.w, 0.0f);
    EXPECT_FLOAT_EQ(ui.nine_slice_src_size.x, 0.0f);
    EXPECT_FLOAT_EQ(ui.nine_slice_src_size.y, 0.0f);
}

// 测试 UI渲染器组件：Nine方形网格边框赋值
TEST(UIRendererComponentTest, NineSquareGridBorderAssignment) {
    UIRendererComponent ui;
    ui.nine_slice_enabled = true;
    ui.nine_slice_border = glm::vec4(0.1f, 0.2f, 0.1f, 0.2f);
    EXPECT_TRUE(ui.nine_slice_enabled);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.x, 0.1f);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.y, 0.2f);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.z, 0.1f);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.w, 0.2f);
}

// ============================================================
// UIPanelComponent
// ============================================================

// 测试 UI面板组件：默认不
TEST(UIPanelComponentTest, DefaultNot) {
    UIPanelComponent panel;
    EXPECT_FALSE(panel.blocks_input);
}

// ============================================================
// UIButtonComponent
// ============================================================

// 测试 UI按钮组件：默认
TEST(UIButtonComponentTest, Default) {
    UIButtonComponent btn;
    EXPECT_FLOAT_EQ(btn.normal_color.r, 1.0f);
    EXPECT_FLOAT_EQ(btn.hover_color.r, 1.1f);
    EXPECT_FLOAT_EQ(btn.pressed_color.r, 0.8f);
}

// ============================================================
// UILabelComponent
// ============================================================

// 测试 UI标签组件：默认值
TEST(UILabelComponentTest, DefaultValues) {
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

// 测试 UI标签组件：模型
TEST(UILabelComponentTest, Model) {
    UILabelComponent label;
    label.numeric_mode = true;
    label.number_value = 12345;
    EXPECT_TRUE(label.numeric_mode);
    EXPECT_EQ(label.number_value, 12345);
}

// ============================================================
// UIMaskComponent
// ============================================================

// 测试 UI掩码组件：默认值
TEST(UIMaskComponentTest, DefaultValues) {
    UIMaskComponent mask;
    EXPECT_TRUE(mask.enabled);
    EXPECT_TRUE(mask.block_outside_input);
    EXPECT_FLOAT_EQ(mask.size.x, 0.0f);
}

// ============================================================
// UIRichTextComponent
// ============================================================

// 测试 UI Rich文本组件：默认值
TEST(UIRichTextComponentTest, DefaultValues) {
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

// 测试 UI Joystick组件：默认值
TEST(UIJoystickComponentTest, DefaultValues) {
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

// 测试 UI锚点组件：默认值
TEST(UIAnchorComponentTest, DefaultValues) {
    UIAnchorComponent anchor;
    EXPECT_EQ(anchor.anchor, 5);
    EXPECT_FLOAT_EQ(anchor.offset.x, 0.0f);
}

// ============================================================
// UIGridLayoutComponent
// ============================================================

// 测试 UI网格布局组件：默认值
TEST(UIGridLayoutComponentTest, DefaultValues) {
    UIGridLayoutComponent grid;
    EXPECT_EQ(grid.columns, 1);
    EXPECT_EQ(grid.rows, 0);
    EXPECT_FLOAT_EQ(grid.cell_size.x, 100.0f);
    EXPECT_FLOAT_EQ(grid.spacing.x, 10.0f);
}

// ============================================================
// UICanvasScalerComponent
// ============================================================

// 测试 UI画布缩放器组件：默认值
TEST(UICanvasScalerComponentTest, DefaultValues) {
    UICanvasScalerComponent scaler;
    EXPECT_FLOAT_EQ(scaler.reference_resolution.x, 1920.0f);
    EXPECT_FLOAT_EQ(scaler.reference_resolution.y, 1080.0f);
    EXPECT_FLOAT_EQ(scaler.scale_factor, 1.0f);
    EXPECT_TRUE(scaler.match_width_or_height);
}

// ============================================================
// UIAnimationComponent
// ============================================================

// 测试 UI动画组件：默认值
TEST(UIAnimationComponentTest, DefaultValues) {
    UIAnimationComponent anim;
    EXPECT_FLOAT_EQ(anim.duration, 0.3f);
    EXPECT_FLOAT_EQ(anim.elapsed, 0.0f);
    EXPECT_FALSE(anim.playing);
    EXPECT_FALSE(anim.loop);
    EXPECT_FALSE(anim.ping_pong);
    EXPECT_EQ(anim.easing, 0);
}
