/**
 * @file spine_system_test.cpp
 * @brief Spine 系统测试：资源缺失处理、动画切换、循环/非循环边界
 */

#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "modules/gameplay_2d/spine/spine_system.h"
#include <stdexcept>
#include <cstdio>

using dse::gameplay2d::SpineSystem;

namespace {

void PrintSpineTestDiag(const char* label) {
    std::printf("[spine-test] %s\n", label ? label : "(null)");
    std::fflush(stdout);
}

} // namespace

// Note: SpineSystem depends on spine-cpp runtime which requires actual .skel/.atlas files.
// These tests validate the ECS component state machine behavior without loading real Spine data.
// For integration tests with real assets, use the samples/ directory.

// ─── Component defaults ─────────────────────────────────────────────────
TEST_CASE("Spine - default component state is valid", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    REQUIRE_FALSE(spine.runtime);
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

TEST_CASE("Spine - shutdown clears runtime state and keeps configuration", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    spine.skeleton_data_path = "assets/characters/hero.skel";
    spine.atlas_path = "assets/characters/hero.atlas";
    spine.current_animation = "idle";
    spine.loop = false;
    spine.time_scale = 1.5f;
    spine.dirty_animation = true;

    SpineSystem system;
    system.Shutdown(world.registry());

    REQUIRE_FALSE(spine.runtime);
    REQUIRE(spine.textures.empty());
    REQUIRE_FALSE(spine.dirty_animation);
    REQUIRE(spine.skeleton_data_path == "assets/characters/hero.skel");
    REQUIRE(spine.atlas_path == "assets/characters/hero.atlas");
    REQUIRE(spine.current_animation == "idle");
    REQUIRE_FALSE(spine.loop);
    REQUIRE(spine.time_scale == Approx(1.5f));
}

TEST_CASE("Spine - update without asset paths leaves component unchanged", "[spine]") {
    PrintSpineTestDiag("case1 scope begin :: probe_v2_unique");
    {
        World world;
        auto entity = world.CreateEntity();
        auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

        spine.current_animation = "idle";
        spine.dirty_animation = true;
        spine.visible = false;

        PrintSpineTestDiag("case1 before system construct");
        SpineSystem system;
        PrintSpineTestDiag("case1 after system construct");
        system.Update(world.registry(), 1.0f / 60.0f);
        PrintSpineTestDiag("case1 after update");

        const bool runtime_null = !spine.runtime;
        const bool animation_idle = spine.current_animation == "idle";
        const bool dirty_animation = spine.dirty_animation;
        const bool visible_false = !spine.visible;
        PrintSpineTestDiag("case1 after bool snapshots");
        if (!runtime_null) {
            FAIL("runtime should remain null");
        }
        if (!animation_idle) {
            FAIL("current_animation should remain idle");
        }
        if (!dirty_animation) {
            FAIL("dirty_animation should remain true");
        }
        if (!visible_false) {
            FAIL("visible should remain false");
        }
        PrintSpineTestDiag("case1 before explicit component remove");
        world.registry().remove<SpineRendererComponent>(entity);
        PrintSpineTestDiag("case1 after explicit component remove");
        PrintSpineTestDiag("case1 before inner scope end");
    }
    PrintSpineTestDiag("case1 after inner scope end");
}

TEST_CASE("Spine - update with partial asset paths does not require asset manager", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    SpineSystem system;

    SECTION("only skeleton path configured") {
        spine.skeleton_data_path = "assets/characters/hero.skel";
        spine.atlas_path.clear();
    }

    SECTION("only atlas path configured") {
        spine.skeleton_data_path.clear();
        spine.atlas_path = "assets/characters/hero.atlas";
    }

    REQUIRE_NOTHROW(system.Update(world.registry(), 1.0f / 60.0f));
    REQUIRE_FALSE(spine.runtime);
    REQUIRE(spine.textures.empty());
}

TEST_CASE("Spine - update with configured asset paths but no asset manager throws", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    spine.skeleton_data_path = "assets/characters/hero.skel";
    spine.atlas_path = "assets/characters/hero.atlas";

    SpineSystem system;

    REQUIRE_THROWS_AS(system.Update(world.registry(), 1.0f / 60.0f), std::runtime_error);
    REQUIRE_FALSE(spine.runtime);
    REQUIRE(spine.textures.empty());
}

TEST_CASE("Spine - shutdown is idempotent for cleared runtime state", "[spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    spine.skeleton_data_path = "assets/characters/hero.skel";
    spine.atlas_path = "assets/characters/hero.atlas";
    spine.current_animation = "idle";
    spine.dirty_animation = true;
    spine.visible = false;

    SpineSystem system;
    system.Shutdown(world.registry());
    REQUIRE_NOTHROW(system.Shutdown(world.registry()));

    REQUIRE_FALSE(spine.runtime);
    REQUIRE(spine.textures.empty());
    REQUIRE_FALSE(spine.dirty_animation);
    REQUIRE_FALSE(spine.visible);
    REQUIRE(spine.current_animation == "idle");
    REQUIRE(spine.skeleton_data_path == "assets/characters/hero.skel");
    REQUIRE(spine.atlas_path == "assets/characters/hero.atlas");
}
