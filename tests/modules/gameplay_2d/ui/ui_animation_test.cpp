/**
 * @file ui_animation_test.cpp
 * @brief UIAnimationComponent 单元测试
 */

#include "catch/catch.hpp"
#include "gameplay_2d/ui/ui_system.h"
#include "engine/ecs/components_2d.h"
#include <entt/entt.hpp>

using namespace dse::gameplay2d;

// ─── Helper ──────────────────────────────────────────────────────────────
static entt::entity MakeAnimatedUI(entt::registry& reg, glm::vec2 pos) {
    auto e = reg.create();
    auto& ui = reg.emplace<UIRendererComponent>(e);
    ui.position = pos;
    ui.size = glm::vec2(100.0f, 50.0f);
    ui.visible = true;
    ui.interactable = false;
    return e;
}

// ─── Position Animation ─────────────────────────────────────────────────
TEST_CASE("UIAnimation - position animation interpolates", "[ui][animation]") {
    entt::registry reg;
    UISystem sys;

    auto e = MakeAnimatedUI(reg, {0.0f, 0.0f});
    auto& anim = reg.emplace<UIAnimationComponent>(e);
    anim.animate_position = true;
    anim.start_position = glm::vec2(0.0f, 0.0f);
    anim.target_position = glm::vec2(100.0f, 200.0f);
    anim.duration = 1.0f;
    anim.elapsed = 0.0f;
    anim.playing = true;
    anim.easing = 0;  // linear

    // Advance half way
    sys.Update(reg, 0.5f, {800, 600}, {0, 0}, false);

    auto& ui = reg.get<UIRendererComponent>(e);
    REQUIRE(ui.position.x == Approx(50.0f).margin(1.0f));
    REQUIRE(ui.position.y == Approx(100.0f).margin(1.0f));
}

TEST_CASE("UIAnimation - animation completes and stops", "[ui][animation]") {
    entt::registry reg;
    UISystem sys;

    auto e = MakeAnimatedUI(reg, {0.0f, 0.0f});
    auto& anim = reg.emplace<UIAnimationComponent>(e);
    anim.animate_position = true;
    anim.start_position = glm::vec2(0.0f, 0.0f);
    anim.target_position = glm::vec2(100.0f, 0.0f);
    anim.duration = 0.5f;
    anim.playing = true;
    anim.easing = 0;

    // Advance past duration
    sys.Update(reg, 1.0f, {800, 600}, {0, 0}, false);

    REQUIRE_FALSE(anim.playing);
    REQUIRE(reg.get<UIRendererComponent>(e).position.x == Approx(100.0f).margin(0.1f));
}

// ─── Alpha Animation ────────────────────────────────────────────────────
TEST_CASE("UIAnimation - alpha fade out", "[ui][animation]") {
    entt::registry reg;
    UISystem sys;

    auto e = MakeAnimatedUI(reg, {0.0f, 0.0f});
    reg.get<UIRendererComponent>(e).color = glm::vec4(1.0f);
    auto& anim = reg.emplace<UIAnimationComponent>(e);
    anim.animate_alpha = true;
    anim.start_alpha = 1.0f;
    anim.target_alpha = 0.0f;
    anim.duration = 1.0f;
    anim.playing = true;
    anim.easing = 0;

    sys.Update(reg, 0.5f, {800, 600}, {0, 0}, false);

    REQUIRE(reg.get<UIRendererComponent>(e).color.a == Approx(0.5f).margin(0.05f));
}

// ─── Loop ────────────────────────────────────────────────────────────────
TEST_CASE("UIAnimation - loop resets elapsed", "[ui][animation]") {
    entt::registry reg;
    UISystem sys;

    auto e = MakeAnimatedUI(reg, {0.0f, 0.0f});
    auto& anim = reg.emplace<UIAnimationComponent>(e);
    anim.animate_position = true;
    anim.start_position = glm::vec2(0.0f, 0.0f);
    anim.target_position = glm::vec2(100.0f, 0.0f);
    anim.duration = 0.5f;
    anim.loop = true;
    anim.playing = true;
    anim.easing = 0;

    // Advance past one full cycle
    sys.Update(reg, 0.6f, {800, 600}, {0, 0}, false);

    // Should still be playing
    REQUIRE(anim.playing);
    // Elapsed should have been reset
    REQUIRE(anim.elapsed < anim.duration);
}

// ─── Ping Pong ───────────────────────────────────────────────────────────
TEST_CASE("UIAnimation - ping pong reverses direction", "[ui][animation]") {
    entt::registry reg;
    UISystem sys;

    auto e = MakeAnimatedUI(reg, {0.0f, 0.0f});
    auto& anim = reg.emplace<UIAnimationComponent>(e);
    anim.animate_position = true;
    anim.start_position = glm::vec2(0.0f, 0.0f);
    anim.target_position = glm::vec2(100.0f, 0.0f);
    anim.duration = 0.5f;
    anim.ping_pong = true;
    anim.loop = true;
    anim.playing = true;
    anim.easing = 0;

    // Complete forward pass
    sys.Update(reg, 0.6f, {800, 600}, {0, 0}, false);
    REQUIRE(anim.reverse);  // Should now be in reverse
    REQUIRE(anim.playing);
}

// ─── Delay ───────────────────────────────────────────────────────────────
TEST_CASE("UIAnimation - delay prevents animation start", "[ui][animation]") {
    entt::registry reg;
    UISystem sys;

    auto e = MakeAnimatedUI(reg, {0.0f, 0.0f});
    auto& anim = reg.emplace<UIAnimationComponent>(e);
    anim.animate_position = true;
    anim.start_position = glm::vec2(0.0f, 0.0f);
    anim.target_position = glm::vec2(100.0f, 0.0f);
    anim.duration = 1.0f;
    anim.delay = 0.5f;
    anim.delay_remaining = 0.5f;
    anim.playing = true;
    anim.easing = 0;

    // Advance 0.3s (still in delay)
    sys.Update(reg, 0.3f, {800, 600}, {0, 0}, false);
    REQUIRE(reg.get<UIRendererComponent>(e).position.x == Approx(0.0f).margin(0.1f));

    // Advance 0.3s more (delay over, 0.1s into animation)
    sys.Update(reg, 0.3f, {800, 600}, {0, 0}, false);
    REQUIRE(reg.get<UIRendererComponent>(e).position.x > 0.0f);
}

// ─── Easing ──────────────────────────────────────────────────────────────
TEST_CASE("UIAnimation - ease-in starts slow", "[ui][animation]") {
    entt::registry reg;
    UISystem sys;

    auto e = MakeAnimatedUI(reg, {0.0f, 0.0f});
    auto& anim = reg.emplace<UIAnimationComponent>(e);
    anim.animate_position = true;
    anim.start_position = glm::vec2(0.0f, 0.0f);
    anim.target_position = glm::vec2(100.0f, 0.0f);
    anim.duration = 1.0f;
    anim.playing = true;
    anim.easing = 1;  // ease-in

    sys.Update(reg, 0.5f, {800, 600}, {0, 0}, false);

    // Ease-in at t=0.5: eased = 0.25, so position should be ~25
    REQUIRE(reg.get<UIRendererComponent>(e).position.x == Approx(25.0f).margin(2.0f));
}

TEST_CASE("UIAnimation - ease-out starts fast", "[ui][animation]") {
    entt::registry reg;
    UISystem sys;

    auto e = MakeAnimatedUI(reg, {0.0f, 0.0f});
    auto& anim = reg.emplace<UIAnimationComponent>(e);
    anim.animate_position = true;
    anim.start_position = glm::vec2(0.0f, 0.0f);
    anim.target_position = glm::vec2(100.0f, 0.0f);
    anim.duration = 1.0f;
    anim.playing = true;
    anim.easing = 2;  // ease-out

    sys.Update(reg, 0.5f, {800, 600}, {0, 0}, false);

    // Ease-out at t=0.5: eased = 0.75, so position should be ~75
    REQUIRE(reg.get<UIRendererComponent>(e).position.x == Approx(75.0f).margin(2.0f));
}
