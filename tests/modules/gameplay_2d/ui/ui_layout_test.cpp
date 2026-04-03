/**
 * @file ui_layout_test.cpp
 * @brief UILayoutSystem 单元测试
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "gameplay_2d/ui/ui_layout.h"
#include "engine/ecs/components_2d.h"
#include <entt/entt.hpp>

using namespace dse::gameplay2d;
using Catch::Matchers::WithinAbs;

// ─── Canvas Scaler ───────────────────────────────────────────────────────
TEST_CASE("UILayoutSystem - canvas scaler adjusts positions", "[ui][layout]") {
    entt::registry reg;
    UILayoutSystem layout_sys;

    // Create canvas scaler entity
    auto scaler_entity = reg.create();
    auto& scaler = reg.emplace<UICanvasScalerComponent>(scaler_entity);
    scaler.reference_resolution = glm::vec2(1920.0f, 1080.0f);
    scaler.scale_factor = 1.0f;
    scaler.match_width_or_height = true;

    // Create anchored element at top-left
    auto elem = reg.create();
    auto& anchor = reg.emplace<UIAnchorComponent>(elem);
    anchor.anchor = 0;  // TopLeft
    anchor.offset = glm::vec2(50.0f, 30.0f);
    auto& ui = reg.emplace<UIRendererComponent>(elem);
    ui.size = glm::vec2(100.0f, 50.0f);

    // Screen is half the reference resolution
    layout_sys.Update(reg, glm::vec2(960.0f, 540.0f));

    // Scale factor should be 0.5 (average of 0.5 and 0.5)
    // Position = (0, 0) + (50, 30) * 0.5 = (25, 15)
    REQUIRE_THAT(ui.position.x, WithinAbs(25.0, 0.5));
    REQUIRE_THAT(ui.position.y, WithinAbs(15.0, 0.5));
}

TEST_CASE("UILayoutSystem - canvas scaler width-only mode", "[ui][layout]") {
    entt::registry reg;
    UILayoutSystem layout_sys;

    auto scaler_entity = reg.create();
    auto& scaler = reg.emplace<UICanvasScalerComponent>(scaler_entity);
    scaler.reference_resolution = glm::vec2(1920.0f, 1080.0f);
    scaler.scale_factor = 1.0f;
    scaler.match_width_or_height = false;  // width only

    auto elem = reg.create();
    auto& anchor = reg.emplace<UIAnchorComponent>(elem);
    anchor.anchor = 5;  // MiddleCenter
    anchor.offset = glm::vec2(100.0f, 0.0f);
    auto& ui = reg.emplace<UIRendererComponent>(elem);
    ui.size = glm::vec2(100.0f, 50.0f);

    // Screen width is half reference, height is full
    layout_sys.Update(reg, glm::vec2(960.0f, 1080.0f));

    // Scale = 960/1920 = 0.5
    // Position = (480, 540) + (100, 0) * 0.5 = (530, 540)
    REQUIRE_THAT(ui.position.x, WithinAbs(530.0, 0.5));
    REQUIRE_THAT(ui.position.y, WithinAbs(540.0, 0.5));
}

// ─── Anchor Positions ────────────────────────────────────────────────────
TEST_CASE("UILayoutSystem - anchor positions without scaler", "[ui][layout]") {
    entt::registry reg;
    UILayoutSystem layout_sys;
    const glm::vec2 screen(800.0f, 600.0f);

    SECTION("TopLeft") {
        auto e = reg.create();
        auto& a = reg.emplace<UIAnchorComponent>(e);
        a.anchor = 0;  // TopLeft
        a.offset = glm::vec2(10.0f, 20.0f);
        auto& ui = reg.emplace<UIRendererComponent>(e);
        layout_sys.Update(reg, screen);
        REQUIRE_THAT(ui.position.x, WithinAbs(10.0, 0.1));
        REQUIRE_THAT(ui.position.y, WithinAbs(20.0, 0.1));
    }

    SECTION("MiddleCenter") {
        auto e = reg.create();
        auto& a = reg.emplace<UIAnchorComponent>(e);
        a.anchor = 5;  // MiddleCenter
        a.offset = glm::vec2(0.0f, 0.0f);
        auto& ui = reg.emplace<UIRendererComponent>(e);
        layout_sys.Update(reg, screen);
        REQUIRE_THAT(ui.position.x, WithinAbs(400.0, 0.1));
        REQUIRE_THAT(ui.position.y, WithinAbs(300.0, 0.1));
    }

    SECTION("BottomRight") {
        auto e = reg.create();
        auto& a = reg.emplace<UIAnchorComponent>(e);
        a.anchor = 8;  // BottomRight
        a.offset = glm::vec2(-10.0f, -10.0f);
        auto& ui = reg.emplace<UIRendererComponent>(e);
        layout_sys.Update(reg, screen);
        REQUIRE_THAT(ui.position.x, WithinAbs(790.0, 0.1));
        REQUIRE_THAT(ui.position.y, WithinAbs(590.0, 0.1));
    }
}

// ─── Grid Layout ─────────────────────────────────────────────────────────
TEST_CASE("UILayoutSystem - grid layout arranges children", "[ui][layout]") {
    entt::registry reg;
    UILayoutSystem layout_sys;

    // Parent with grid layout
    auto parent = reg.create();
    auto& parent_ui = reg.emplace<UIRendererComponent>(parent);
    parent_ui.position = glm::vec2(400.0f, 300.0f);
    parent_ui.size = glm::vec2(400.0f, 400.0f);
    auto& grid = reg.emplace<UIGridLayoutComponent>(parent);
    grid.columns = 2;
    grid.rows = 0;  // auto
    grid.cell_size = glm::vec2(50.0f, 50.0f);
    grid.spacing = glm::vec2(10.0f, 10.0f);
    grid.alignment = 0;  // UpperLeft

    // Create 4 children
    std::vector<entt::entity> children;
    for (int i = 0; i < 4; ++i) {
        auto child = reg.create();
        reg.emplace<ParentComponent>(child).parent = parent;
        reg.emplace<UIRendererComponent>(child);
        children.push_back(child);
    }

    layout_sys.Update(reg, glm::vec2(1920.0f, 1080.0f));

    // All children should have been repositioned
    for (auto child : children) {
        auto& ui = reg.get<UIRendererComponent>(child);
        // Size should match cell_size (scaled by 1.0 since no scaler)
        REQUIRE_THAT(ui.size.x, WithinAbs(50.0, 0.1));
        REQUIRE_THAT(ui.size.y, WithinAbs(50.0, 0.1));
    }

    // Children should be in different positions
    auto& c0 = reg.get<UIRendererComponent>(children[0]);
    auto& c1 = reg.get<UIRendererComponent>(children[1]);
    auto& c2 = reg.get<UIRendererComponent>(children[2]);
    REQUIRE(c1.position.x > c0.position.x);  // col 1 > col 0
    REQUIRE_THAT(c2.position.y - c0.position.y, !WithinAbs(0.0, 0.1));  // different row
}

TEST_CASE("UILayoutSystem - grid layout respects row limit", "[ui][layout]") {
    entt::registry reg;
    UILayoutSystem layout_sys;

    auto parent = reg.create();
    auto& parent_ui = reg.emplace<UIRendererComponent>(parent);
    parent_ui.position = glm::vec2(0.0f, 0.0f);
    auto& grid = reg.emplace<UIGridLayoutComponent>(parent);
    grid.columns = 2;
    grid.rows = 1;  // only 1 row
    grid.cell_size = glm::vec2(50.0f, 50.0f);
    grid.spacing = glm::vec2(10.0f, 10.0f);

    // Create 4 children but only 2 should be laid out (1 row * 2 cols)
    for (int i = 0; i < 4; ++i) {
        auto child = reg.create();
        reg.emplace<ParentComponent>(child).parent = parent;
        auto& ui = reg.emplace<UIRendererComponent>(child);
        ui.position = glm::vec2(-999.0f);  // sentinel
    }

    layout_sys.Update(reg, glm::vec2(1920.0f, 1080.0f));

    // Collect children
    std::vector<entt::entity> children;
    auto pv = reg.view<ParentComponent>();
    for (auto e : pv) {
        if (pv.get<ParentComponent>(e).parent == parent) children.push_back(e);
    }
    std::sort(children.begin(), children.end(), [](entt::entity a, entt::entity b) {
        return static_cast<uint32_t>(a) < static_cast<uint32_t>(b);
    });

    // First 2 should be repositioned, last 2 should remain at sentinel
    REQUIRE(children.size() == 4);
    REQUIRE(reg.get<UIRendererComponent>(children[0]).position.x != -999.0f);
    REQUIRE(reg.get<UIRendererComponent>(children[1]).position.x != -999.0f);
    REQUIRE_THAT(reg.get<UIRendererComponent>(children[2]).position.x, WithinAbs(-999.0, 0.1));
    REQUIRE_THAT(reg.get<UIRendererComponent>(children[3]).position.x, WithinAbs(-999.0, 0.1));
}
