#include "catch/catch.hpp"
#include "modules/gameplay_2d/spine/spine_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"

using dse::gameplay2d::SpineSystem;

// 边界测试：未配置资源路径时，Update 应为 no-op，不应污染组件默认状态。
TEST_CASE("Given_EmptySpinePaths_When_Update_Then_ComponentStateRemainsUnchanged", "[engine][unit][spine]") {
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

    SpineSystem system;
    system.Update(world.registry(), 1.0f / 60.0f);

    REQUIRE(spine.atlas == nullptr);
    REQUIRE(spine.skeleton_data == nullptr);
    REQUIRE(spine.skeleton == nullptr);
    REQUIRE(spine.animation_state == nullptr);
    REQUIRE(spine.dirty_animation);
}