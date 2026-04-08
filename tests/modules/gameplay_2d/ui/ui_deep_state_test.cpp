#include "catch/catch.hpp"
#include "modules/gameplay_2d/ui/ui_system.h"
#include "modules/gameplay_2d/ui/ui_layout.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"

using dse::gameplay2d::UISystem;
using dse::gameplay2d::UILayoutSystem;

TEST_CASE("Given_RichTextWithShadowAndOutline_When_Update_Then_RuntimeGlyphsIncludeDecorations", "[engine][unit][ui][rich_text]") {
    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.visible = true;
    ui.texture_handle = 77;

    auto& label = world.registry().emplace<UILabelComponent>(entity);
    label.glyph_size = glm::vec2(16.0f);
    label.text = "AB";
    label.dirty = true;

    auto& rich = world.registry().emplace<UIRichTextComponent>(entity);
    rich.text = "<color=#ff0000>A</color>B";
    rich.default_color = glm::vec4(1.0f);
    rich.enable_shadow = true;
    rich.enable_outline = true;
    rich.outline_width = 1.0f;
    rich.dirty = true;

    UISystem system;
    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(-1.0f), false);

    REQUIRE(label.runtime_glyph_entities.size() == 12);
    REQUIRE_FALSE(label.dirty);
    REQUIRE_FALSE(rich.dirty);
}

TEST_CASE("Given_JoystickFollowPointerEnabled_When_Dragging_Then_DirectionIsNormalizedAndAnchorTracksPointer", "[engine][unit][ui][joystick]") {
    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.visible = true;
    ui.interactable = true;
    ui.position = glm::vec2(100.0f, 120.0f);
    ui.size = glm::vec2(100.0f, 100.0f);
    ui.anchor_min = glm::vec2(0.0f);
    ui.anchor_max = glm::vec2(0.0f);
    ui.pivot = glm::vec2(0.0f);

    auto& joystick = world.registry().emplace<UIJoystickComponent>(entity);
    joystick.follow_pointer = true;
    joystick.max_radius = 50.0f;

    UISystem system;
    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(110.0f, 130.0f), true);
    REQUIRE(joystick.is_dragging);
    REQUIRE(joystick.drag_anchor == glm::vec2(110.0f, 130.0f));

    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(160.0f, 130.0f), true);
    REQUIRE(joystick.direction.x == Approx(1.0f));
    REQUIRE(joystick.direction.y == Approx(0.0f));

    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(160.0f, 130.0f), false);
    REQUIRE_FALSE(joystick.is_dragging);
    REQUIRE(joystick.direction == glm::vec2(0.0f));
}

TEST_CASE("Given_JoystickFixedAnchorAndNoReset_When_Released_Then_LastDirectionPersists", "[engine][unit][ui][joystick]") {
    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.visible = true;
    ui.interactable = true;
    ui.position = glm::vec2(40.0f, 60.0f);
    ui.size = glm::vec2(80.0f, 80.0f);
    ui.anchor_min = glm::vec2(0.0f);
    ui.anchor_max = glm::vec2(0.0f);
    ui.pivot = glm::vec2(0.0f);

    auto& joystick = world.registry().emplace<UIJoystickComponent>(entity);
    joystick.follow_pointer = false;
    joystick.reset_on_release = false;
    joystick.max_radius = 40.0f;

    UISystem system;
    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(60.0f, 80.0f), true);
    REQUIRE(joystick.drag_anchor == glm::vec2(40.0f, 60.0f));

    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(80.0f, 100.0f), true);
    const glm::vec2 direction_before_release = joystick.direction;
    REQUIRE(glm::length(direction_before_release) > 0.0f);

    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(80.0f, 100.0f), false);
    REQUIRE_FALSE(joystick.is_dragging);
    REQUIRE(joystick.direction == direction_before_release);
}

TEST_CASE("Given_CanvasScalerAndAnchors_When_UpdateLayoutCalled_Then_AnchorAndGridUseScaledCoordinates", "[engine][unit][ui][layout]") {
    World world;

    auto canvas = world.CreateEntity();
    auto& scaler = world.registry().emplace<UICanvasScalerComponent>(canvas);
    scaler.reference_resolution = glm::vec2(1000.0f, 500.0f);
    scaler.scale_factor = 2.0f;
    scaler.match_width_or_height = false;

    auto anchored = world.CreateEntity();
    auto& anchored_ui = world.registry().emplace<UIRendererComponent>(anchored);
    auto& anchor = world.registry().emplace<UIAnchorComponent>(anchored);
    anchor.anchor = 0;
    anchor.offset = glm::vec2(10.0f, 20.0f);

    auto parent = world.CreateEntity();
    auto& parent_ui = world.registry().emplace<UIRendererComponent>(parent);
    parent_ui.position = glm::vec2(50.0f, 70.0f);
    auto& grid = world.registry().emplace<UIGridLayoutComponent>(parent);
    grid.columns = 2;
    grid.cell_size = glm::vec2(100.0f, 20.0f);
    grid.spacing = glm::vec2(10.0f, 5.0f);
    grid.alignment = 0;

    auto child_a = world.CreateEntity();
    world.registry().emplace<ParentComponent>(child_a).parent = parent;
    world.registry().emplace<UIRendererComponent>(child_a);

    auto child_b = world.CreateEntity();
    world.registry().emplace<ParentComponent>(child_b).parent = parent;
    world.registry().emplace<UIRendererComponent>(child_b);

    UILayoutSystem layout;
    layout.Update(world.registry(), glm::vec2(2000.0f, 1000.0f));

    REQUIRE(anchored_ui.position == glm::vec2(40.0f, 80.0f));

    const auto& child_ui_a = world.registry().get<UIRendererComponent>(child_a);
    const auto& child_ui_b = world.registry().get<UIRendererComponent>(child_b);
    REQUIRE(child_ui_a.size == glm::vec2(200.0f, 40.0f));
    REQUIRE(child_ui_b.size == glm::vec2(200.0f, 40.0f));
    REQUIRE(child_ui_b.position.x > child_ui_a.position.x);
    REQUIRE(child_ui_a.position.y == Approx(child_ui_b.position.y));
}
