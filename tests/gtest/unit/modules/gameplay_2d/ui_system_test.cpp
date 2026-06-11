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
#include "engine/ecs/ui_serializer.h"
#include "engine/ecs/ui.h"
#include "engine/ecs/transform.h"

#include <glm/glm.hpp>

using namespace dse::gameplay2d;

// ============================================================
// UIAnchorData / GridLayoutData / CanvasScalerData 默认值
// ============================================================

TEST(UIAnchorDataTest, DefaultValues) {
    UIAnchorData a;
    EXPECT_EQ(a.anchor, UIAnchor::MiddleCenter);
    EXPECT_FLOAT_EQ(a.offset.x, 0.0f);
    EXPECT_FLOAT_EQ(a.offset.y, 0.0f);
    EXPECT_FLOAT_EQ(a.size.x, 100.0f);
    EXPECT_FLOAT_EQ(a.size.y, 100.0f);
}

TEST(GridLayoutDataTest, DefaultValues) {
    GridLayoutData g;
    EXPECT_EQ(g.columns, 1);
    EXPECT_EQ(g.rows, 0);
    EXPECT_FLOAT_EQ(g.cell_size.x, 100.0f);
    EXPECT_FLOAT_EQ(g.cell_size.y, 100.0f);
    EXPECT_FLOAT_EQ(g.spacing.x, 10.0f);
    EXPECT_FLOAT_EQ(g.spacing.y, 10.0f);
    EXPECT_EQ(g.alignment, GridLayoutAlignment::UpperLeft);
}

TEST(CanvasScalerDataTest, DefaultValues) {
    CanvasScalerData cs;
    EXPECT_FLOAT_EQ(cs.reference_resolution.x, 1920.0f);
    EXPECT_FLOAT_EQ(cs.reference_resolution.y, 1080.0f);
    EXPECT_FLOAT_EQ(cs.scale_factor, 1.0f);
    EXPECT_TRUE(cs.match_width_or_height);
    EXPECT_FLOAT_EQ(cs.match, 0.5f);   // 默认均衡，等价旧"宽高平均"
    EXPECT_FALSE(cs.pixel_snap);
}

// ============================================================
// UIButtonComponent / UIPanelComponent 默认值
// ============================================================

TEST(UIButtonComponentTest, DefaultValues) {
    UIButtonComponent btn;
    EXPECT_FLOAT_EQ(btn.normal_color.r, 1.0f);
    EXPECT_FLOAT_EQ(btn.hover_color.r, 1.1f);
    EXPECT_FLOAT_EQ(btn.pressed_color.r, 0.8f);
}

TEST(UIPanelComponentTest, DefaultValues) {
    UIPanelComponent panel;
    EXPECT_FALSE(panel.blocks_input);
}

// ============================================================
// UILayoutSystem::CalculateScaleFactor
// ============================================================

TEST(UILayoutSystemTest, ScaleFactor_SameResolution) {
    UILayoutSystem layout;
    CanvasScalerData cs;
    cs.reference_resolution = glm::vec2(1920.0f, 1080.0f);
    float sf = layout.CalculateScaleFactor(glm::vec2(1920.0f, 1080.0f), cs);
    EXPECT_NEAR(sf, 1.0f, 0.01f);
}

TEST(UILayoutSystemTest, ScaleFactor_HalfResolution) {
    UILayoutSystem layout;
    CanvasScalerData cs;
    cs.reference_resolution = glm::vec2(1920.0f, 1080.0f);
    float sf = layout.CalculateScaleFactor(glm::vec2(960.0f, 540.0f), cs);
    EXPECT_NEAR(sf, 0.5f, 0.01f);
}

TEST(UILayoutSystemTest, ScaleFactor_DoubleResolution) {
    UILayoutSystem layout;
    CanvasScalerData cs;
    cs.reference_resolution = glm::vec2(1920.0f, 1080.0f);
    float sf = layout.CalculateScaleFactor(glm::vec2(3840.0f, 2160.0f), cs);
    EXPECT_NEAR(sf, 2.0f, 0.01f);
}

// ---- match 0..1 权重（Unity 式宽高匹配） ----

// 非等比分辨率下：match=0 应只跟宽度比，match=1 只跟高度比，match=0.5 取均值。
TEST(UILayoutSystemTest, ScaleFactor_MatchWidthOnly) {
    UILayoutSystem layout;
    CanvasScalerData cs;
    cs.reference_resolution = glm::vec2(1000.0f, 1000.0f);
    cs.match = 0.0f;  // 完全跟宽
    // 屏幕 2000x1000 → width_ratio=2, height_ratio=1
    float sf = layout.CalculateScaleFactor(glm::vec2(2000.0f, 1000.0f), cs);
    EXPECT_NEAR(sf, 2.0f, 0.001f);
}

TEST(UILayoutSystemTest, ScaleFactor_MatchHeightOnly) {
    UILayoutSystem layout;
    CanvasScalerData cs;
    cs.reference_resolution = glm::vec2(1000.0f, 1000.0f);
    cs.match = 1.0f;  // 完全跟高
    float sf = layout.CalculateScaleFactor(glm::vec2(2000.0f, 1000.0f), cs);
    EXPECT_NEAR(sf, 1.0f, 0.001f);
}

TEST(UILayoutSystemTest, ScaleFactor_MatchHalf_EqualsOldAverage) {
    UILayoutSystem layout;
    CanvasScalerData cs;
    cs.reference_resolution = glm::vec2(1000.0f, 1000.0f);
    cs.match = 0.5f;  // 均衡 = 旧行为 (width_ratio+height_ratio)/2
    float sf = layout.CalculateScaleFactor(glm::vec2(2000.0f, 1000.0f), cs);
    EXPECT_NEAR(sf, 1.5f, 0.001f);
}

TEST(UILayoutSystemTest, ScaleFactor_MatchClampedOutOfRange) {
    UILayoutSystem layout;
    CanvasScalerData cs;
    cs.reference_resolution = glm::vec2(1000.0f, 1000.0f);
    cs.match = 5.0f;  // 越界应钳到 1（跟高）
    float sf = layout.CalculateScaleFactor(glm::vec2(2000.0f, 1000.0f), cs);
    EXPECT_NEAR(sf, 1.0f, 0.001f);
}

// ---- 像素吸附 SnapToPixel ----

TEST(UILayoutSystemTest, SnapToPixel_RoundsToNearestInteger) {
    EXPECT_FLOAT_EQ(SnapToPixel(glm::vec2(10.4f, 20.6f)).x, 10.0f);
    EXPECT_FLOAT_EQ(SnapToPixel(glm::vec2(10.4f, 20.6f)).y, 21.0f);
    EXPECT_FLOAT_EQ(SnapToPixel(glm::vec2(-3.5f, 0.49f)).y, 0.0f);
    // 已是整数则不变
    EXPECT_FLOAT_EQ(SnapToPixel(glm::vec2(7.0f, -8.0f)).x, 7.0f);
    EXPECT_FLOAT_EQ(SnapToPixel(glm::vec2(7.0f, -8.0f)).y, -8.0f);
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
// UISystem empty registry
// ============================================================

TEST(UISystemTest, DefaultConstructSafe) {
    UISystem sys;
    (void)sys;
}

TEST(UISystemTest, EmptyRegistryNoCrash) {
    UISystem sys;
    entt::registry reg;
    sys.Update(reg, 1.0f / 60.0f, glm::vec2(1920, 1080), glm::vec2(0), false);
}

TEST(UISystemTest, ZeroDtNoCrash) {
    UISystem sys;
    entt::registry reg;
    sys.Update(reg, 0.0f, glm::vec2(1920, 1080), glm::vec2(960, 540), false);
}

// ============================================================
// UILayoutSystem empty registry
// ============================================================

TEST(UILayoutSystemTest, DefaultConstructSafe) {
    UILayoutSystem layout;
    (void)layout;
}

TEST(UILayoutSystemTest, EmptyRegistryNoCrash) {
    UILayoutSystem layout;
    entt::registry reg;
    layout.Update(reg, glm::vec2(1920, 1080));
}

// ============================================================
// Runtime interaction tests
// ============================================================

namespace {
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

TEST(UISystemTest, SliderDragUpdatesValue) {
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
    sys.Update(reg, 0.016f, screen, glm::vec2(960, 540), true);
    sys.Update(reg, 0.016f, screen, glm::vec2(960, 540), true);

    EXPECT_TRUE(slider.is_dragging);
    EXPECT_TRUE(callback_fired);
    EXPECT_NEAR(received_value, 50.0f, 5.0f);
}

TEST(UISystemTest, SliderWholeNumbers) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& slider = reg.emplace<UISliderComponent>(e);
    slider.min_value = 0.0f;
    slider.max_value = 10.0f;
    slider.whole_numbers = true;

    const glm::vec2 screen(1920, 1080);
    sys.Update(reg, 0.016f, screen, glm::vec2(576, 540), true);
    sys.Update(reg, 0.016f, screen, glm::vec2(576, 540), true);

    EXPECT_FLOAT_EQ(slider.value, std::round(slider.value));
}

TEST(UISystemTest, ToggleClickToggle) {
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
    sys.Update(reg, 0.016f, screen, center, true);
    sys.Update(reg, 0.016f, screen, center, false);

    EXPECT_TRUE(toggle.is_on);
    EXPECT_TRUE(callback_fired);
}

TEST(UISystemTest, ToggleMutualExclusion) {
    UISystem sys;
    entt::registry reg;
    auto e1 = MakeFullScreenUI(reg);
    auto& t1 = reg.emplace<UIToggleComponent>(e1);
    t1.is_on = true;
    t1.group = 0;

    auto e2 = MakeFullScreenUI(reg);
    auto& ui2 = reg.get<UIRendererComponent>(e2);
    ui2.order = 10;
    auto& t2 = reg.emplace<UIToggleComponent>(e2);
    t2.is_on = false;
    t2.group = 0;

    const glm::vec2 screen(1920, 1080);
    const glm::vec2 center(960, 540);
    sys.Update(reg, 0.016f, screen, center, true);
    sys.Update(reg, 0.016f, screen, center, false);

    EXPECT_TRUE(t2.is_on);
    EXPECT_FALSE(t1.is_on);
}

TEST(UISystemTest, TextInputCursorBlink) {
    UISystem sys;
    entt::registry reg;
    auto e = reg.create();
    auto& input = reg.emplace<UITextInputComponent>(e);
    input.is_focused = true;
    input.cursor_blink_rate = 0.1f;
    input.cursor_blink_timer = 0.0f;
    input.cursor_visible = true;

    const glm::vec2 screen(1920, 1080);
    for (int i = 0; i < 8; ++i) {
        sys.Update(reg, 0.016f, screen, glm::vec2(0), false);
    }
    EXPECT_FALSE(input.cursor_visible);
}

TEST(UISystemTest, TextInputUnfocusedNoBlink) {
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

TEST(UISystemTest, ScrollViewDragScroll) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& sv = reg.emplace<UIScrollViewComponent>(e);
    sv.content_size = glm::vec2(1920, 3000);
    sv.viewport_size = glm::vec2(0.0f);
    sv.vertical = true;
    sv.horizontal = false;
    sv.inertia = false;

    const glm::vec2 screen(1920, 1080);
    sys.Update(reg, 0.016f, screen, glm::vec2(960, 500), true);
    sys.Update(reg, 0.016f, screen, glm::vec2(960, 300), true);

    EXPECT_TRUE(sv.is_dragging);
    EXPECT_GT(sv.scroll_offset.y, 100.0f);
}

TEST(UISystemTest, ScrollViewElasticBounce) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& sv = reg.emplace<UIScrollViewComponent>(e);
    sv.content_size = glm::vec2(1920, 2000);
    sv.viewport_size = glm::vec2(1920, 1080);
    sv.elastic = true;
    sv.elasticity = 0.5f;
    sv.inertia = false;
    sv.scroll_offset = glm::vec2(0, -100);

    const glm::vec2 screen(1920, 1080);
    for (int i = 0; i < 20; ++i) {
        sys.Update(reg, 0.016f, screen, glm::vec2(0), false);
    }
    EXPECT_NEAR(sv.scroll_offset.y, 0.0f, 1.0f);
}

TEST(UISystemTest, ProgressBarNoCrash) {
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

// ============================================================
// UIDropdownComponent tests
// ============================================================

TEST(UIDropdownComponentTest, DefaultValues) {
    UIDropdownComponent dd;
    EXPECT_EQ(dd.selected_index, -1);
    EXPECT_FALSE(dd.is_open);
    EXPECT_FLOAT_EQ(dd.item_height, 40.0f);
    EXPECT_EQ(dd.max_visible_items, 5);
    EXPECT_TRUE(dd.options.empty());
    EXPECT_EQ(dd.GetSelectedText(), "");
    EXPECT_EQ(dd.GetSelectedValue(), "");
}

TEST(UIDropdownComponentTest, SelectedItemAccess) {
    UIDropdownComponent dd;
    dd.options.push_back({"Option A", "a"});
    dd.options.push_back({"Option B", "b"});
    dd.selected_index = 1;
    EXPECT_EQ(dd.GetSelectedText(), "Option B");
    EXPECT_EQ(dd.GetSelectedValue(), "b");
}

TEST(UISystemTest, DropdownClickOpen) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& dd = reg.emplace<UIDropdownComponent>(e);
    dd.options.push_back({"Apple", "apple"});
    dd.options.push_back({"Banana", "banana"});
    dd.options.push_back({"Cherry", "cherry"});
    dd.item_height = 40.0f;

    const glm::vec2 screen(1920, 1080);
    const glm::vec2 center(960, 540);

    sys.Update(reg, 0.016f, screen, center, true);
    sys.Update(reg, 0.016f, screen, center, false);

    EXPECT_TRUE(dd.is_open);
}

TEST(UISystemTest, DropdownNoCrash) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    reg.emplace<UIDropdownComponent>(e);
    const glm::vec2 screen(1920, 1080);
    sys.Update(reg, 0.016f, screen, glm::vec2(0), false);
}

// ============================================================
// UIFilledImageComponent tests
// ============================================================

TEST(UIFilledImageComponentTest, DefaultValues) {
    UIFilledImageComponent fi;
    EXPECT_FLOAT_EQ(fi.fill_amount, 1.0f);
    EXPECT_EQ(fi.fill_method, UIFillMethod::Horizontal);
    EXPECT_EQ(fi.fill_origin, UIFillOrigin::Left);
    EXPECT_TRUE(fi.clockwise);
}

TEST(UIFilledImageComponentTest, FillAmountRange) {
    UIFilledImageComponent fi;
    fi.fill_amount = 0.5f;
    EXPECT_FLOAT_EQ(fi.fill_amount, 0.5f);
    fi.fill_method = UIFillMethod::Radial360;
    EXPECT_EQ(fi.fill_method, UIFillMethod::Radial360);
}

// ============================================================
// UIFocusNavigableComponent tests
// ============================================================

TEST(UIFocusNavigableComponentTest, DefaultValues) {
    UIFocusNavigableComponent fn;
    EXPECT_EQ(fn.tab_index, 0);
    EXPECT_TRUE(fn.nav_up == entt::null);
    EXPECT_TRUE(fn.nav_down == entt::null);
    EXPECT_TRUE(fn.nav_left == entt::null);
    EXPECT_TRUE(fn.nav_right == entt::null);
    EXPECT_FALSE(fn.is_focused);
}

TEST(UISystemTest, FocusNavigableNoCrash) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& fn = reg.emplace<UIFocusNavigableComponent>(e);
    fn.tab_index = 0;
    const glm::vec2 screen(1920, 1080);
    sys.Update(reg, 0.016f, screen, glm::vec2(0), false);
}

// ============================================================
// UISerializer tests
// ============================================================

TEST(UISerializerTest, EmptyJsonNoCrash) {
    dse::UISerializer serializer;
    entt::registry reg;
    auto entities = serializer.LoadFromJson(reg, "{}");
    EXPECT_TRUE(entities.empty());
}

TEST(UISerializerTest, InvalidJsonNoCrash) {
    dse::UISerializer serializer;
    entt::registry reg;
    auto entities = serializer.LoadFromJson(reg, "not valid json");
    EXPECT_TRUE(entities.empty());
}

TEST(UISerializerTest, BasicUIEntityParse) {
    dse::UISerializer serializer;
    entt::registry reg;
    const char* json = R"({
        "entities": [
            {
                "id": 1,
                "components": {
                    "UIRenderer": {
                        "position": [100, 200],
                        "size": [300, 50],
                        "color": [1, 0, 0, 1],
                        "order": 5,
                        "visible": true,
                        "interactable": true
                    },
                    "UIButton": {
                        "normal_color": [1, 1, 1, 1]
                    }
                }
            }
        ]
    })";
    auto entities = serializer.LoadFromJson(reg, json);
    ASSERT_EQ(entities.size(), 1u);
    auto e = entities[0];
    ASSERT_TRUE(reg.all_of<UIRendererComponent>(e));
    ASSERT_TRUE(reg.all_of<UIButtonComponent>(e));
    auto& ui = reg.get<UIRendererComponent>(e);
    EXPECT_NEAR(ui.position.x, 100.0f, 0.01f);
    EXPECT_NEAR(ui.position.y, 200.0f, 0.01f);
    EXPECT_EQ(ui.order, 5);
}

TEST(UISerializerTest, ParentChildHierarchy) {
    dse::UISerializer serializer;
    entt::registry reg;
    const char* json = R"({
        "entities": [
            {
                "id": 1,
                "components": {
                    "UIRenderer": { "size": [800, 600] }
                },
                "children": [
                    {
                        "id": 2,
                        "components": {
                            "UIRenderer": { "size": [100, 30] },
                            "UILabel": { "text": "Hello" }
                        }
                    }
                ]
            }
        ]
    })";
    auto entities = serializer.LoadFromJson(reg, json);
    ASSERT_EQ(entities.size(), 2u);
    auto child = entities[1];
    ASSERT_TRUE(reg.all_of<ParentComponent>(child));
    ASSERT_TRUE(reg.all_of<UILabelComponent>(child));
    auto& label = reg.get<UILabelComponent>(child);
    EXPECT_EQ(label.text, "Hello");
}

TEST(UISerializerTest, DropdownSerialization) {
    dse::UISerializer serializer;
    entt::registry reg;
    const char* json = R"({
        "entities": [
            {
                "id": 1,
                "components": {
                    "UIRenderer": {},
                    "UIDropdown": {
                        "item_height": 30,
                        "options": [
                            { "text": "Low", "value": "0" },
                            { "text": "Medium", "value": "1" },
                            { "text": "High", "value": "2" }
                        ],
                        "selected_index": 1
                    }
                }
            }
        ]
    })";
    auto entities = serializer.LoadFromJson(reg, json);
    ASSERT_EQ(entities.size(), 1u);
    auto e = entities[0];
    ASSERT_TRUE(reg.all_of<UIDropdownComponent>(e));
    auto& dd = reg.get<UIDropdownComponent>(e);
    EXPECT_EQ(dd.options.size(), 3u);
    EXPECT_EQ(dd.selected_index, 1);
    EXPECT_EQ(dd.GetSelectedValue(), "1");
    EXPECT_FLOAT_EQ(dd.item_height, 30.0f);
}

TEST(UISerializerTest, EventPropagationSerialization) {
    dse::UISerializer serializer;
    entt::registry reg;
    const char* json = R"({
        "entities": [{
            "id": 1,
            "components": {
                "UIRenderer": {},
                "UIEventPropagation": { "bubbles_click": true, "bubbles_hover": true }
            }
        }]
    })";
    auto entities = serializer.LoadFromJson(reg, json);
    ASSERT_EQ(entities.size(), 1u);
    ASSERT_TRUE(reg.all_of<UIEventPropagationComponent>(entities[0]));
    auto& ep = reg.get<UIEventPropagationComponent>(entities[0]);
    EXPECT_TRUE(ep.bubbles_click);
    EXPECT_TRUE(ep.bubbles_hover);
}

TEST(UISerializerTest, VisualEffectSerialization) {
    dse::UISerializer serializer;
    entt::registry reg;
    const char* json = R"({
        "entities": [{
            "id": 1,
            "components": {
                "UIRenderer": {},
                "UIVisualEffect": { "corner_radius": 12.0, "blur_radius": 5.0 }
            }
        }]
    })";
    auto entities = serializer.LoadFromJson(reg, json);
    ASSERT_EQ(entities.size(), 1u);
    ASSERT_TRUE(reg.all_of<UIVisualEffectComponent>(entities[0]));
    auto& vfx = reg.get<UIVisualEffectComponent>(entities[0]);
    EXPECT_FLOAT_EQ(vfx.corner_radius, 12.0f);
    EXPECT_FLOAT_EQ(vfx.blur_radius, 5.0f);
}

TEST(UISerializerTest, FilledImageSerialization) {
    dse::UISerializer serializer;
    entt::registry reg;
    const char* json = R"({
        "entities": [
            {
                "id": 1,
                "components": {
                    "UIRenderer": {},
                    "UIFilledImage": {
                        "fill_amount": 0.75,
                        "fill_method": 2,
                        "clockwise": false
                    }
                }
            }
        ]
    })";
    auto entities = serializer.LoadFromJson(reg, json);
    ASSERT_EQ(entities.size(), 1u);
    auto e = entities[0];
    ASSERT_TRUE(reg.all_of<UIFilledImageComponent>(e));
    auto& fi = reg.get<UIFilledImageComponent>(e);
    EXPECT_NEAR(fi.fill_amount, 0.75f, 0.01f);
    EXPECT_EQ(fi.fill_method, UIFillMethod::Radial360);
    EXPECT_FALSE(fi.clockwise);
}

// ============================================================
// P3: UIEventPropagationComponent tests
// ============================================================

TEST(UIEventPropagationComponentTest, DefaultValues) {
    UIEventPropagationComponent ep;
    EXPECT_TRUE(ep.bubbles_click);
    EXPECT_FALSE(ep.bubbles_hover);
    EXPECT_FALSE(ep.stop_propagation);
}

TEST(UISystemTest, EventBubblesToParent) {
    UISystem sys;
    entt::registry reg;

    auto parent = MakeFullScreenUI(reg);
    bool parent_clicked = false;
    reg.get<UIRendererComponent>(parent).on_click = [&](entt::entity) { parent_clicked = true; };

    auto child = MakeFullScreenUI(reg);
    auto& child_ui = reg.get<UIRendererComponent>(child);
    child_ui.order = 10;
    reg.emplace<ParentComponent>(child).parent = parent;
    auto& ep = reg.emplace<UIEventPropagationComponent>(child);
    ep.bubbles_click = true;
    reg.emplace<UIEventPropagationComponent>(parent).bubbles_click = true;

    const glm::vec2 screen(1920, 1080);
    const glm::vec2 center(960, 540);
    sys.Update(reg, 0.016f, screen, center, true);
    sys.Update(reg, 0.016f, screen, center, false);

    EXPECT_TRUE(parent_clicked);
}

TEST(UISystemTest, StopPropagationBlocksBubble) {
    UISystem sys;
    entt::registry reg;

    auto parent = MakeFullScreenUI(reg);
    bool parent_clicked = false;
    reg.get<UIRendererComponent>(parent).on_click = [&](entt::entity) { parent_clicked = true; };

    auto child = MakeFullScreenUI(reg);
    auto& child_ui = reg.get<UIRendererComponent>(child);
    child_ui.order = 10;
    reg.emplace<ParentComponent>(child).parent = parent;
    auto& ep = reg.emplace<UIEventPropagationComponent>(child);
    ep.bubbles_click = true;
    ep.stop_propagation = true;

    const glm::vec2 screen(1920, 1080);
    const glm::vec2 center(960, 540);
    sys.Update(reg, 0.016f, screen, center, true);
    sys.Update(reg, 0.016f, screen, center, false);

    EXPECT_FALSE(parent_clicked);
}

// ============================================================
// P3: UIVisualEffectComponent tests
// ============================================================

TEST(UIVisualEffectComponentTest, DefaultValues) {
    UIVisualEffectComponent vfx;
    EXPECT_FLOAT_EQ(vfx.corner_radius, 0.0f);
    EXPECT_FLOAT_EQ(vfx.blur_radius, 0.0f);
    EXPECT_FLOAT_EQ(vfx.blur_intensity, 1.0f);
    EXPECT_EQ(vfx.gradient_direction, UIGradientDirection::Vertical);
}

// ============================================================
// P3: UIVirtualScrollComponent tests
// ============================================================

TEST(UIVirtualScrollComponentTest, DefaultValues) {
    UIVirtualScrollComponent vs;
    EXPECT_EQ(vs.total_item_count, 0);
    EXPECT_FLOAT_EQ(vs.item_height, 50.0f);
    EXPECT_EQ(vs.visible_start_index, 0);
    EXPECT_EQ(vs.visible_end_index, 0);
    EXPECT_TRUE(vs.pool_entities.empty());
    EXPECT_TRUE(vs.dirty);
}

TEST(UISystemTest, VirtualScrollCreatesPoolEntities) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    auto& sv = reg.emplace<UIScrollViewComponent>(e);
    sv.content_size = glm::vec2(1920, 10000);
    sv.viewport_size = glm::vec2(1920, 500);
    sv.vertical = true;
    auto& vs = reg.emplace<UIVirtualScrollComponent>(e);
    vs.total_item_count = 200;
    vs.item_height = 50.0f;
    int bind_call_count = 0;
    vs.on_bind_item = [&](entt::entity, int) { bind_call_count++; };

    const glm::vec2 screen(1920, 1080);
    sys.Update(reg, 0.016f, screen, glm::vec2(0), false);

    EXPECT_GT(static_cast<int>(vs.pool_entities.size()), 0);
    EXPECT_GT(bind_call_count, 0);
    EXPECT_EQ(vs.visible_start_index, 0);
    EXPECT_GT(vs.visible_end_index, 0);
}

TEST(UISystemTest, VirtualScrollNoCrash) {
    UISystem sys;
    entt::registry reg;
    auto e = MakeFullScreenUI(reg);
    reg.emplace<UIScrollViewComponent>(e);
    reg.emplace<UIVirtualScrollComponent>(e);
    const glm::vec2 screen(1920, 1080);
    sys.Update(reg, 0.016f, screen, glm::vec2(0), false);
}
