#include <gtest/gtest.h>
#include "engine/ecs/ui.h"

// ============================================================
// UITextInputComponent
// ============================================================

TEST(UITextInputComponentTest, DefaultValues) {
    UITextInputComponent c;
    EXPECT_TRUE(c.text.empty());
    EXPECT_TRUE(c.placeholder.empty());
    EXPECT_EQ(c.cursor_position, 0);
    EXPECT_EQ(c.selection_start, -1);
    EXPECT_EQ(c.selection_end, -1);
    EXPECT_EQ(c.max_length, 0);
    EXPECT_FALSE(c.is_focused);
    EXPECT_FALSE(c.is_password);
    EXPECT_FALSE(c.multiline);
    EXPECT_FALSE(c.read_only);
    EXPECT_TRUE(c.submit_on_enter);
    EXPECT_GT(c.cursor_blink_rate, 0.0f);
}

TEST(UITextInputComponentTest, CallbackAssignment) {
    UITextInputComponent c;
    bool called = false;
    c.on_value_changed = [&](entt::entity, const std::string&) { called = true; };
    c.on_value_changed(entt::null, "test");
    EXPECT_TRUE(called);
}

// ============================================================
// UIScrollViewComponent
// ============================================================

TEST(UIScrollViewComponentTest, DefaultValues) {
    UIScrollViewComponent c;
    EXPECT_EQ(c.content_size, glm::vec2(0.0f));
    EXPECT_EQ(c.viewport_size, glm::vec2(0.0f));
    EXPECT_EQ(c.scroll_offset, glm::vec2(0.0f));
    EXPECT_FALSE(c.horizontal);
    EXPECT_TRUE(c.vertical);
    EXPECT_TRUE(c.elastic);
    EXPECT_TRUE(c.inertia);
    EXPECT_TRUE(c.show_scrollbar);
}

TEST(UIScrollViewComponentTest, GetNormalizedPosition_ZeroContent) {
    UIScrollViewComponent c;
    c.viewport_size = glm::vec2(100.0f, 100.0f);
    c.content_size = glm::vec2(100.0f, 100.0f);
    auto pos = c.GetNormalizedPosition();
    EXPECT_FLOAT_EQ(pos.x, 0.0f);
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
}

TEST(UIScrollViewComponentTest, GetNormalizedPosition_Scrolled) {
    UIScrollViewComponent c;
    c.viewport_size = glm::vec2(100.0f, 100.0f);
    c.content_size = glm::vec2(200.0f, 400.0f);
    c.scroll_offset = glm::vec2(50.0f, 150.0f);
    auto pos = c.GetNormalizedPosition();
    EXPECT_FLOAT_EQ(pos.x, 0.5f);
    EXPECT_FLOAT_EQ(pos.y, 0.5f);
}

// ============================================================
// UISliderComponent
// ============================================================

TEST(UISliderComponentTest, DefaultValues) {
    UISliderComponent c;
    EXPECT_FLOAT_EQ(c.value, 0.0f);
    EXPECT_FLOAT_EQ(c.min_value, 0.0f);
    EXPECT_FLOAT_EQ(c.max_value, 1.0f);
    EXPECT_FALSE(c.whole_numbers);
    EXPECT_FALSE(c.vertical);
    EXPECT_FALSE(c.is_dragging);
}

TEST(UISliderComponentTest, GetNormalizedValue) {
    UISliderComponent c;
    c.min_value = 10.0f;
    c.max_value = 20.0f;
    c.value = 15.0f;
    EXPECT_FLOAT_EQ(c.GetNormalizedValue(), 0.5f);
}

TEST(UISliderComponentTest, GetNormalizedValue_ZeroRange) {
    UISliderComponent c;
    c.min_value = 5.0f;
    c.max_value = 5.0f;
    EXPECT_FLOAT_EQ(c.GetNormalizedValue(), 0.0f);
}

TEST(UISliderComponentTest, SetFromNormalized) {
    UISliderComponent c;
    c.min_value = 0.0f;
    c.max_value = 100.0f;
    c.SetFromNormalized(0.75f);
    EXPECT_FLOAT_EQ(c.value, 75.0f);
}

TEST(UISliderComponentTest, SetFromNormalized_WholeNumbers) {
    UISliderComponent c;
    c.min_value = 0.0f;
    c.max_value = 10.0f;
    c.whole_numbers = true;
    c.SetFromNormalized(0.33f);
    EXPECT_FLOAT_EQ(c.value, 3.0f);
}

// ============================================================
// UIToggleComponent
// ============================================================

TEST(UIToggleComponentTest, DefaultValues) {
    UIToggleComponent c;
    EXPECT_FALSE(c.is_on);
    EXPECT_EQ(c.group, -1);
    EXPECT_FLOAT_EQ(c.transition_duration, 0.15f);
    EXPECT_FLOAT_EQ(c.transition_progress, 1.0f);
}

TEST(UIToggleComponentTest, CallbackAssignment) {
    UIToggleComponent c;
    bool received_value = false;
    c.on_value_changed = [&](entt::entity, bool v) { received_value = v; };
    c.on_value_changed(entt::null, true);
    EXPECT_TRUE(received_value);
}

// ============================================================
// UIProgressBarComponent
// ============================================================

TEST(UIProgressBarComponentTest, DefaultValues) {
    UIProgressBarComponent c;
    EXPECT_FLOAT_EQ(c.value, 0.0f);
    EXPECT_FLOAT_EQ(c.max_value, 1.0f);
    EXPECT_FALSE(c.right_to_left);
    EXPECT_FALSE(c.vertical);
}

TEST(UIProgressBarComponentTest, GetFillAmount) {
    UIProgressBarComponent c;
    c.value = 0.5f;
    c.max_value = 1.0f;
    EXPECT_FLOAT_EQ(c.GetFillAmount(), 0.5f);
}

TEST(UIProgressBarComponentTest, GetFillAmount_Clamped) {
    UIProgressBarComponent c;
    c.value = 2.0f;
    c.max_value = 1.0f;
    EXPECT_FLOAT_EQ(c.GetFillAmount(), 1.0f);
}

TEST(UIProgressBarComponentTest, GetFillAmount_ZeroMax) {
    UIProgressBarComponent c;
    c.max_value = 0.0f;
    c.value = 0.5f;
    EXPECT_FLOAT_EQ(c.GetFillAmount(), 0.0f);
}
