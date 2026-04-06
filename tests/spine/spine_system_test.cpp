#include "catch/catch.hpp"
#include "modules/gameplay_2d/spine/spine_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/assets/asset_manager.h"

using dse::gameplay2d::SpineSystem;

// 边界测试：未配置资源路径时，Update 应为 no-op，不应污染组件默认状态。
TEST_CASE("Given_EmptySpinePaths_When_Update_Then_ComponentStateRemainsUnchanged", "[engine][unit][spine][diagnostic_single]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);
    spine.time_scale = 2.0f;
    spine.visible = true;

    SpineSystem system;
    system.Update(world.registry(), 0.25f);

    REQUIRE(spine.atlas == nullptr);
    REQUIRE(spine.skeleton_data == nullptr);
    REQUIRE(spine.skeleton == nullptr);
    REQUIRE(spine.animation_state == nullptr);
    REQUIRE(spine.time_scale == Approx(2.0f));
    REQUIRE(spine.visible);
}

// 反向测试：当 atlas 或 skeleton 资源路径缺失时，SpineSystem 应安全返回且不创建运行时对象。
TEST_CASE("Given_MissingSpineAssets_When_Update_Then_RuntimeObjectsRemainNull", "[engine][unit][spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);
    spine.skeleton_data_path = "missing/hero.skel";
    spine.atlas_path = "missing/hero.atlas";
    spine.current_animation = "idle";
    spine.dirty_animation = true;

    AssetManager asset_manager;
    asset_manager.ConfigureDataRoot("data");

    SpineSystem system;
    system.SetAssetManager(&asset_manager);
    system.Update(world.registry(), 1.0f / 60.0f);

    REQUIRE(spine.atlas == nullptr);
    REQUIRE(spine.skeleton_data == nullptr);
    REQUIRE(spine.skeleton == nullptr);
    REQUIRE(spine.animation_state == nullptr);
    REQUIRE(spine.dirty_animation);
}

// 回归测试：仅配置 atlas 或 skeleton 任一侧路径时，不应触发半初始化状态。
TEST_CASE("Given_IncompleteSpinePaths_When_Update_Then_RuntimeObjectsRemainNull", "[engine][unit][spine]") {
    World world;
    auto first = world.CreateEntity();
    auto& only_atlas = world.registry().emplace<SpineRendererComponent>(first);
    only_atlas.atlas_path = "missing/hero.atlas";
    only_atlas.visible = true;
    only_atlas.dirty_animation = true;

    auto second = world.CreateEntity();
    auto& only_skeleton = world.registry().emplace<SpineRendererComponent>(second);
    only_skeleton.skeleton_data_path = "missing/hero.skel";
    only_skeleton.visible = true;
    only_skeleton.dirty_animation = true;

    SpineSystem system;
    system.Update(world.registry(), 1.0f / 60.0f);

    REQUIRE(only_atlas.atlas == nullptr);
    REQUIRE(only_atlas.skeleton_data == nullptr);
    REQUIRE(only_atlas.skeleton == nullptr);
    REQUIRE(only_atlas.animation_state == nullptr);
    REQUIRE(only_atlas.dirty_animation);
    REQUIRE(only_atlas.visible);

    REQUIRE(only_skeleton.atlas == nullptr);
    REQUIRE(only_skeleton.skeleton_data == nullptr);
    REQUIRE(only_skeleton.skeleton == nullptr);
    REQUIRE(only_skeleton.animation_state == nullptr);
    REQUIRE(only_skeleton.dirty_animation);
    REQUIRE(only_skeleton.visible);
}

// 回归测试：多个缺失资源的 Spine 组件同时更新时，应彼此独立且保持稳定。
TEST_CASE("Given_MultipleMissingSpineComponents_When_Update_Then_AllComponentsRemainStable", "[engine][unit][spine]") {
    World world;

    auto a = world.CreateEntity();
    auto& spine_a = world.registry().emplace<SpineRendererComponent>(a);
    spine_a.skeleton_data_path = "missing/a.skel";
    spine_a.atlas_path = "missing/a.atlas";
    spine_a.current_animation = "idle";
    spine_a.dirty_animation = true;
    spine_a.time_scale = 0.5f;

    auto b = world.CreateEntity();
    auto& spine_b = world.registry().emplace<SpineRendererComponent>(b);
    spine_b.skeleton_data_path = "missing/b.json";
    spine_b.atlas_path = "missing/b.atlas";
    spine_b.current_animation = "run";
    spine_b.dirty_animation = true;
    spine_b.visible = false;
    spine_b.time_scale = 2.0f;

    AssetManager asset_manager;
    asset_manager.ConfigureDataRoot("data");

    SpineSystem system;
    system.SetAssetManager(&asset_manager);
    system.Update(world.registry(), 1.0f / 30.0f);

    REQUIRE(spine_a.atlas == nullptr);
    REQUIRE(spine_a.skeleton_data == nullptr);
    REQUIRE(spine_a.skeleton == nullptr);
    REQUIRE(spine_a.animation_state == nullptr);
    REQUIRE(spine_a.dirty_animation);
    REQUIRE(spine_a.time_scale == Approx(0.5f));

    REQUIRE(spine_b.atlas == nullptr);
    REQUIRE(spine_b.skeleton_data == nullptr);
    REQUIRE(spine_b.skeleton == nullptr);
    REQUIRE(spine_b.animation_state == nullptr);
    REQUIRE(spine_b.dirty_animation);
    REQUIRE_FALSE(spine_b.visible);
    REQUIRE(spine_b.time_scale == Approx(2.0f));
}