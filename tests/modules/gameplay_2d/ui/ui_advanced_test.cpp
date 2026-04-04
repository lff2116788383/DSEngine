/**
 * @file ui_advanced_test.cpp
 * @brief UI 系统高级测试：层级事件穿透、多遮罩、多分辨率布局集成
 */

#include "catch/catch.hpp"
#include "modules/gameplay_2d/ui/ui_system.h"
#include "modules/gameplay_2d/ui/ui_layout.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/core/event_bus.h"

using dse::gameplay2d::UISystem;
using dse::gameplay2d::UILayoutSystem;

// ─── Layer-based event consumption ──────────────────────────────────────
TEST_CASE("UI Advanced - higher order UI consumes click, lower order does not", "[ui][advanced]") {
    World world;
    auto& reg = world.registry();

    // Back button (order 0)
    auto back = world.CreateEntity();
    auto& back_ui = reg.emplace<UIRendererComponent>(back);
    back_ui.position = glm::vec2(100.0f, 100.0f);
    back_ui.size = glm::vec2(200.0f, 200.0f);
    back_ui.visible = true;
    back_ui.interactable = true;
    back_ui.order = 0;
    reg.emplace<UIButtonComponent>(back);
    int back_clicks = 0;
    back_ui.on_click = [&](Entity) { ++back_clicks; };

    // Front button (order 10) overlapping
    auto front = world.CreateEntity();
    auto& front_ui = reg.emplace<UIRendererComponent>(front);
    front_ui.position = glm::vec2(100.0f, 100.0f);
    front_ui.size = glm::vec2(200.0f, 200.0f);
    front_ui.visible = true;
    front_ui.interactable = true;
    front_ui.order = 10;
    reg.emplace<UIButtonComponent>(front);
    int front_clicks = 0;
    front_ui.on_click = [&](Entity) { ++front_clicks; };

    UISystem system;
    const glm::vec2 screen(800.0f, 600.0f);
    // Need to run layout first so runtime_model is set
    system.Update(reg, 0.016f, screen, glm::vec2(100.0f, 100.0f), true);
    system.Update(reg, 0.016f, screen, glm::vec2(100.0f, 100.0f), false);

    REQUIRE(front_clicks == 1);
    REQUIRE(back_clicks == 0);  // consumed by front
}

// ─── Nested masks ───────────────────────────────────────────────────────
TEST_CASE("UI Advanced - nested masks both block outside input", "[ui][advanced]") {
    World world;
    auto& reg = world.registry();

    // Outer mask
    auto outer = world.CreateEntity();
    auto& outer_ui = reg.emplace<UIRendererComponent>(outer);
    outer_ui.position = glm::vec2(200.0f, 200.0f);
    outer_ui.size = glm::vec2(400.0f, 400.0f);
    outer_ui.visible = true;
    outer_ui.interactable = false;
    auto& outer_mask = reg.emplace<UIMaskComponent>(outer);
    outer_mask.enabled = true;
    outer_mask.block_outside_input = true;
    outer_mask.size = glm::vec2(400.0f, 400.0f);

    // Inner mask (smaller)
    auto inner = world.CreateEntity();
    auto& inner_ui = reg.emplace<UIRendererComponent>(inner);
    inner_ui.position = glm::vec2(200.0f, 200.0f);
    inner_ui.size = glm::vec2(100.0f, 100.0f);
    inner_ui.visible = true;
    inner_ui.interactable = false;
    reg.emplace<ParentComponent>(inner).parent = outer;
    auto& inner_mask = reg.emplace<UIMaskComponent>(inner);
    inner_mask.enabled = true;
    inner_mask.block_outside_input = true;
    inner_mask.size = glm::vec2(100.0f, 100.0f);

    // Button inside inner mask
    auto btn = world.CreateEntity();
    auto& btn_ui = reg.emplace<UIRendererComponent>(btn);
    btn_ui.position = glm::vec2(200.0f, 200.0f);
    btn_ui.size = glm::vec2(50.0f, 50.0f);
    btn_ui.visible = true;
    btn_ui.interactable = true;
    reg.emplace<ParentComponent>(btn).parent = inner;
    int clicks = 0;
    btn_ui.on_click = [&](Entity) { ++clicks; };

    UISystem system;
    const glm::vec2 screen(800.0f, 600.0f);

    // Click inside inner mask - should work
    system.Update(reg, 0.016f, screen, glm::vec2(200.0f, 200.0f), true);
    system.Update(reg, 0.016f, screen, glm::vec2(200.0f, 200.0f), false);
    REQUIRE(clicks == 1);

    // Click outside inner mask but inside outer mask - should be blocked by inner mask
    system.Update(reg, 0.016f, screen, glm::vec2(350.0f, 350.0f), true);
    system.Update(reg, 0.016f, screen, glm::vec2(350.0f, 350.0f), false);
    REQUIRE(clicks == 1);  // no additional click
}

// ─── Pointer enter/exit callbacks ───────────────────────────────────────
TEST_CASE("UI Advanced - pointer enter and exit fire correctly", "[ui][advanced]") {
    World world;
    auto& reg = world.registry();

    auto btn = world.CreateEntity();
    auto& ui = reg.emplace<UIRendererComponent>(btn);
    ui.position = glm::vec2(400.0f, 300.0f);
    ui.size = glm::vec2(100.0f, 50.0f);
    ui.visible = true;
    ui.interactable = true;

    int enter_count = 0;
    int exit_count = 0;
    ui.on_pointer_enter = [&](Entity) { ++enter_count; };
    ui.on_pointer_exit = [&](Entity) { ++exit_count; };

    UISystem system;
    const glm::vec2 screen(800.0f, 600.0f);

    // Move over button
    system.Update(reg, 0.016f, screen, glm::vec2(400.0f, 300.0f), false);
    REQUIRE(enter_count == 1);
    REQUIRE(exit_count == 0);

    // Stay on button (no additional enter)
    system.Update(reg, 0.016f, screen, glm::vec2(400.0f, 300.0f), false);
    REQUIRE(enter_count == 1);

    // Move off button
    system.Update(reg, 0.016f, screen, glm::vec2(0.0f, 0.0f), false);
    REQUIRE(exit_count == 1);
}

// ─── Invisible UI should not receive events ─────────────────────────────
TEST_CASE("UI Advanced - invisible button does not receive clicks", "[ui][advanced]") {
    World world;
    auto& reg = world.registry();

    auto btn = world.CreateEntity();
    auto& ui = reg.emplace<UIRendererComponent>(btn);
    ui.position = glm::vec2(100.0f, 100.0f);
    ui.size = glm::vec2(200.0f, 200.0f);
    ui.visible = false;  // invisible
    ui.interactable = true;
    int clicks = 0;
    ui.on_click = [&](Entity) { ++clicks; };

    UISystem system;
    system.Update(reg, 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(100.0f, 100.0f), true);
    system.Update(reg, 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(100.0f, 100.0f), false);

    REQUIRE(clicks == 0);
}

// ─── Layout + UI integration ────────────────────────────────────────────
TEST_CASE("UI Advanced - layout system positions elements before UI interaction", "[ui][advanced][layout]") {
    World world;
    auto& reg = world.registry();

    // Canvas scaler
    auto scaler = world.CreateEntity();
    auto& sc = reg.emplace<UICanvasScalerComponent>(scaler);
    sc.reference_resolution = glm::vec2(1920.0f, 1080.0f);
    sc.scale_factor = 1.0f;
    sc.match_width_or_height = true;

    // Anchored button at BottomRight
    auto btn = world.CreateEntity();
    auto& anchor = reg.emplace<UIAnchorComponent>(btn);
    anchor.anchor = 8;  // BottomRight
    anchor.offset = glm::vec2(-50.0f, -25.0f);
    auto& ui = reg.emplace<UIRendererComponent>(btn);
    ui.size = glm::vec2(100.0f, 50.0f);
    ui.visible = true;
    ui.interactable = true;
    int clicks = 0;
    ui.on_click = [&](Entity) { ++clicks; };

    UILayoutSystem layout;
    UISystem system;
    const glm::vec2 screen(1920.0f, 1080.0f);

    // Run layout first
    layout.Update(reg, screen);

    // Button should be near bottom-right: (1920-50, 1080-25) = (1870, 1055)
    REQUIRE(ui.position.x > 1800.0f);
    REQUIRE(ui.position.y > 1000.0f);
}

// ─── Rich text color parsing ────────────────────────────────────────────
TEST_CASE("UI Advanced - rich text label generates colored glyphs", "[ui][advanced]") {
    World world;
    auto& reg = world.registry();

    auto label_entity = world.CreateEntity();
    auto& ui = reg.emplace<UIRendererComponent>(label_entity);
    ui.position = glm::vec2(100.0f, 100.0f);
    ui.size = glm::vec2(300.0f, 30.0f);
    ui.visible = true;
    ui.interactable = false;
    auto& label = reg.emplace<UILabelComponent>(label_entity);
    label.text = "AB";
    label.dirty = true;
    auto& rich = reg.emplace<UIRichTextComponent>(label_entity);
    rich.text = "<color=#ff0000>A</color>B";
    rich.default_color = glm::vec4(1.0f);
    rich.dirty = true;

    UISystem system;
    system.Update(reg, 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(0.0f, 0.0f), false);

    // Should have created glyph entities
    REQUIRE(label.runtime_glyph_entities.size() >= 2);
}
