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

// ============================================================
// 新组件运行时交互测试
// ============================================================

namespace {
// 创建一个占满屏幕的 interactable UI 实体（便于 hit test）
entt::entity MakeFullScreenUI(entt::registry& reg) {
    auto e = reg.create();
    auto& ui = reg.emplace<UIRendererComponent>(e);
    ui.visible = true;
    ui.interactable = true;
    ui.position = glm::vec2(0.0f);
    ui.size = glm::vec2(1920.0f, 1080.0f);
    ui.anchor_min = glm::vec2(0.0f);
    ui.anchor_max = glm::vec2(0.0f);
    ui.pivot = glm::vec2(0.0f);
    ui.scale = 1.0f;
    return e;
}
}

TEST(UISystemTest, Slider拖拽更新值) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& slider = reg.emplace<UISliderComponent>(e);
    slider.min_value = 0.0f;
    slider.max_value = 100.0f;
    slider.value = 0.0f;

    bool callback_fired = false;
    float received_value = 0.0f;
    slider.on_value_changed = [&](entt::entity, float v) {
        callback_fired = true;
        received_value = v;
    };

    const glm::vec2 screen(1920, 1080);
    // Frame 1: 鼠标按下
    sys.Update(reg, 0.016f, screen, glm::vec2(960, 540), true);
    // Frame 2: 保持按下（拖拽中，鼠标在 50% 位置）
    sys.Update(reg, 0.016f, screen, glm::vec2(960, 540), true);

    EXPECT_TRUE(slider.is_dragging);
    EXPECT_TRUE(callback_fired);
    EXPECT_NEAR(received_value, 50.0f, 5.0f);
}

TEST(UISystemTest, Slider整数步进) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& slider = reg.emplace<UISliderComponent>(e);
    slider.min_value = 0.0f;
    slider.max_value = 10.0f;
    slider.whole_numbers = true;

    const glm::vec2 screen(1920, 1080);
    sys.Update(reg, 0.016f, screen, glm::vec2(576, 540), true);  // press
    sys.Update(reg, 0.016f, screen, glm::vec2(576, 540), true);  // drag at ~30%

    EXPECT_FLOAT_EQ(slider.value, std::round(slider.value));
}

TEST(UISystemTest, Toggle点击切换) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& toggle = reg.emplace<UIToggleComponent>(e);
    toggle.is_on = false;

    bool callback_fired = false;
    toggle.on_value_changed = [&](entt::entity, bool v) {
        callback_fired = true;
        EXPECT_TRUE(v);
    };

    const glm::vec2 screen(1920, 1080);
    const glm::vec2 center(960, 540);
    // 鼠标按下 → 释放 = 点击
    sys.Update(reg, 0.016f, screen, center, true);
    sys.Update(reg, 0.016f, screen, center, false);

    EXPECT_TRUE(toggle.is_on);
    EXPECT_TRUE(callback_fired);
}

TEST(UISystemTest, Toggle互斥组) {
    UISystem sys;
    entt::registry reg;
    auto e1 = MakeFullScreenUI(reg);
    auto& t1 = reg.emplace<UIToggleComponent>(e1);
    t1.is_on = true;
    t1.group = 0;

    auto e2 = MakeFullScreenUI(reg);
    auto& ui2 = reg.get<UIRendererComponent>(e2);
    ui2.order = 10; // 确保 e2 在 e1 上面
    auto& t2 = reg.emplace<UIToggleComponent>(e2);
    t2.is_on = false;
    t2.group = 0;

    const glm::vec2 screen(1920, 1080);
    const glm::vec2 center(960, 540);
    // 点击 e2（order 更高，应被选中）
    sys.Update(reg, 0.016f, screen, center, true);
    sys.Update(reg, 0.016f, screen, center, false);

    EXPECT_TRUE(t2.is_on);
    EXPECT_FALSE(t1.is_on);
}

TEST(UISystemTest, TextInput光标闪烁) {
    UISystem sys;
    entt::registry reg;
    auto e = reg.create();
    auto& input = reg.emplace<UITextInputComponent>(e);
    input.is_focused = true;
    input.cursor_blink_rate = 0.1f;
    input.cursor_blink_timer = 0.0f;
    input.cursor_visible = true;

    const glm::vec2 screen(1920, 1080);
    // 累积 0.12s > blink_rate, 应翻转一次
    for (int i = 0; i < 8; ++i) {
        sys.Update(reg, 0.016f, screen, glm::vec2(0), false);
    }
    EXPECT_FALSE(input.cursor_visible);
}

TEST(UISystemTest, TextInput未聚焦不闪烁) {
    UISystem sys;
    entt::registry reg;
    auto e = reg.create();
    auto& input = reg.emplace<UITextInputComponent>(e);
    input.is_focused = false;
    input.cursor_blink_timer = 0.0f;

    const glm::vec2 screen(1920, 1080);
    sys.Update(reg, 1.0f, screen, glm::vec2(0), false);
    EXPECT_FLOAT_EQ(input.cursor_blink_timer, 0.0f);
}

TEST(UISystemTest, ScrollView拖拽滚动) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& sv = reg.emplace<UIScrollViewComponent>(e);
    sv.content_size = glm::vec2(1920, 3000);
    sv.viewport_size = glm::vec2(0.0f); // auto from UIRenderer
    sv.vertical = true;
    sv.horizontal = false;
    sv.inertia = false;

    const glm::vec2 screen(1920, 1080);
    // 按下
    sys.Update(reg, 0.016f, screen, glm::vec2(960, 500), true);
    // 拖拽：y 从 500 移到 300 = 往上拽 200px → offset 增加
    sys.Update(reg, 0.016f, screen, glm::vec2(960, 300), true);

    EXPECT_TRUE(sv.is_dragging);
    EXPECT_GT(sv.scroll_offset.y, 100.0f);
}

TEST(UISystemTest, ScrollView弹性回弹) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& sv = reg.emplace<UIScrollViewComponent>(e);
    sv.content_size = glm::vec2(1920, 2000);
    sv.viewport_size = glm::vec2(1920, 1080);
    sv.elastic = true;
    sv.elasticity = 0.5f;
    sv.inertia = false;
    sv.scroll_offset = glm::vec2(0, -100); // 超出边界

    const glm::vec2 screen(1920, 1080);
    // 多帧弹性回弹
    for (int i = 0; i < 20; ++i) {
        sys.Update(reg, 0.016f, screen, glm::vec2(0), false);
    }
    EXPECT_NEAR(sv.scroll_offset.y, 0.0f, 1.0f);
}

TEST(UISystemTest, ProgressBar组件存在时不崩溃) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& bar = reg.emplace<UIProgressBarComponent>(e);
    bar.value = 0.5f;
    bar.max_value = 1.0f;

    const glm::vec2 screen(1920, 1080);
    sys.Update(reg, 0.016f, screen, glm::vec2(960, 540), false);
    EXPECT_NEAR(bar.GetFillAmount(), 0.5f, 0.01f);
}
