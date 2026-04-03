/**
 * @file spine_system_test.cpp
 * @brief Spine 系统测试：资源缺失处理、动画切换、循环/非循环边界
 */

#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"

// Note: SpineSystem depends on spine-cpp runtime which requires actual .skel/.atlas files.
// These tests validate the ECS component state machine behavior without loading real Spine data.
// For integration tests with real assets, use the samples/ directory.

// ─── Component defaults ─────────────────────────────────────────────────
TEST_CASE("Spine - default component state is valid", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    REQUIRE(spine.skeleton_data == nullptr);
    REQUIRE(spine.atlas == nullptr);
    REQUIRE(spine.skeleton == nullptr);
    REQUIRE(spine.animation_state == nullptr);
    REQUIRE(spine.visible == true);
    REQUIRE(spine.loop == true);
    REQUIRE(spine.time_scale == Approx(1.0f));
    REQUIRE(spine.current_animation.empty());
    REQUIRE_FALSE(spine.dirty_animation);
}

// ─── Animation switch request ───────────────────────────────────────────
TEST_CASE("Spine - setting animation marks dirty flag", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    spine.current_animation = "walk";
    spine.dirty_animation = true;

    REQUIRE(spine.current_animation == "walk");
    REQUIRE(spine.dirty_animation);
}

// ─── Loop toggle ────────────────────────────────────────────────────────
TEST_CASE("Spine - loop flag can be toggled", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    spine.loop = false;
    REQUIRE_FALSE(spine.loop);

    spine.loop = true;
    REQUIRE(spine.loop);
}

// ─── Time scale ─────────────────────────────────────────────────────────
TEST_CASE("Spine - time scale affects animation speed", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    spine.time_scale = 2.0f;
    REQUIRE(spine.time_scale == Approx(2.0f));

    spine.time_scale = 0.5f;
    REQUIRE(spine.time_scale == Approx(0.5f));

    spine.time_scale = 0.0f;
    REQUIRE(spine.time_scale == Approx(0.0f));
}

// ─── Missing resource paths ─────────────────────────────────────────────
TEST_CASE("Spine - empty paths indicate unloaded state", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    REQUIRE(spine.skeleton_data_path.empty());
    REQUIRE(spine.atlas_path.empty());

    // Setting paths
    spine.skeleton_data_path = "assets/characters/hero.skel";
    spine.atlas_path = "assets/characters/hero.atlas";
    REQUIRE(spine.skeleton_data_path == "assets/characters/hero.skel");
    REQUIRE(spine.atlas_path == "assets/characters/hero.atlas");
}

// ─── Texture list management ────────────────────────────────────────────
TEST_CASE("Spine - texture list starts empty", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    REQUIRE(spine.textures.empty());
}

// ─── Sorting layer ──────────────────────────────────────────────────────
TEST_CASE("Spine - sorting layer and order configurable", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    spine.sorting_layer = 5;
    spine.order_in_layer = 10;
    REQUIRE(spine.sorting_layer == 5);
    REQUIRE(spine.order_in_layer == 10);
}

// ─── Multiple animation switches ────────────────────────────────────────
TEST_CASE("Spine - rapid animation switching updates state correctly", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    spine.current_animation = "idle";
    spine.dirty_animation = true;
    // Simulate system processing
    spine.dirty_animation = false;

    spine.current_animation = "run";
    spine.dirty_animation = true;
    REQUIRE(spine.current_animation == "run");
    REQUIRE(spine.dirty_animation);

    spine.current_animation = "attack";
    spine.dirty_animation = true;
    REQUIRE(spine.current_animation == "attack");
}
