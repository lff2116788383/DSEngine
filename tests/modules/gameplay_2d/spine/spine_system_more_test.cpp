#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "modules/gameplay_2d/spine/spine_system.h"

using dse::gameplay2d::SpineSystem;

TEST_CASE("Given_MultipleSpineComponents_When_ShutdownCalled_Then_AllRuntimeStateIsCleared", "[engine][unit][spine]") {
    World world;
    SpineSystem system;

    auto a = world.CreateEntity();
    auto& spine_a = world.registry().emplace<SpineRendererComponent>(a);
    spine_a.skeleton_data_path = "assets/a.skel";
    spine_a.atlas_path = "assets/a.atlas";
    spine_a.current_animation = "idle";
    spine_a.dirty_animation = true;
    spine_a.textures.push_back({});

    auto b = world.CreateEntity();
    auto& spine_b = world.registry().emplace<SpineRendererComponent>(b);
    spine_b.skeleton_data_path = "assets/b.json";
    spine_b.atlas_path = "assets/b.atlas";
    spine_b.current_animation = "run";
    spine_b.dirty_animation = true;
    spine_b.textures.push_back({});
    spine_b.textures.push_back({});

    system.Shutdown(world.registry());

    REQUIRE_FALSE(spine_a.runtime);
    REQUIRE_FALSE(spine_b.runtime);
    REQUIRE(spine_a.textures.empty());
    REQUIRE(spine_b.textures.empty());
    REQUIRE_FALSE(spine_a.dirty_animation);
    REQUIRE_FALSE(spine_b.dirty_animation);
}

TEST_CASE("Given_SpineComponentWithoutAnimationName_When_UpdateWithoutAssets_Then_DirtyFlagRemainsFalse", "[engine][unit][spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);
    spine.visible = true;
    spine.dirty_animation = false;

    SpineSystem system;
    REQUIRE_NOTHROW(system.Update(world.registry(), 0.033f));
    REQUIRE_FALSE(spine.runtime);
    REQUIRE_FALSE(spine.dirty_animation);
    REQUIRE(spine.visible);
}

TEST_CASE("Given_SpineComponentWithMissingAssets_When_UpdateRepeatedly_Then_StateRemainsStable", "[engine][unit][spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);
    spine.skeleton_data_path = "missing/hero.skel";
    spine.atlas_path = "missing/hero.atlas";
    spine.current_animation = "idle";
    spine.dirty_animation = true;

    SpineSystem system;
    REQUIRE_THROWS(system.Update(world.registry(), 0.016f));
    REQUIRE_FALSE(spine.runtime);
    REQUIRE(spine.textures.empty());

    REQUIRE_THROWS(system.Update(world.registry(), 0.016f));
    REQUIRE_FALSE(spine.runtime);
    REQUIRE(spine.textures.empty());
    REQUIRE(spine.current_animation == "idle");
}
