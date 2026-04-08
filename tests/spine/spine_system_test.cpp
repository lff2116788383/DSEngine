#include "catch/catch.hpp"
#include "modules/gameplay_2d/spine/spine_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/assets/asset_manager.h"
#include <cstdio>

namespace {

void PrintSpineTestDiag(const char* label) {
    std::printf("[spine-test] %s\n", label ? label : "(null)");
    std::fflush(stdout);
}

struct SpineTestFileProbe {
    SpineTestFileProbe() {
        PrintSpineTestDiag("file static init begin");
    }

    ~SpineTestFileProbe() {
        PrintSpineTestDiag("file static deinit begin");
    }
};

SpineTestFileProbe g_spine_test_file_probe;

} // namespace

using dse::gameplay2d::SpineSystem;

// 边界测试：未配置资源路径时，Update 应为 no-op，不应污染组件默认状态。
TEST_CASE("Given_EmptySpinePaths_When_Update_Then_ComponentStateRemainsUnchanged", "[engine][unit][spine]") {
    PrintSpineTestDiag("case1 enter");
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);
    spine.current_animation = "idle";
    spine.dirty_animation = true;
    spine.visible = false;

    SpineSystem system;
    PrintSpineTestDiag("case1 before update");
    system.Update(world.registry(), 1.0f / 60.0f);
    PrintSpineTestDiag("case1 after update");

    PrintSpineTestDiag("case1 before asserts");
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
    PrintSpineTestDiag("case1 return");
}

TEST_CASE("Given_EmptySpineComponent_When_NoUpdate_Then_ComponentStateRemainsUnchanged", "[engine][unit][spine]") {
    PrintSpineTestDiag("control-case enter");
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);
    spine.current_animation = "idle";
    spine.dirty_animation = true;
    spine.visible = false;

    PrintSpineTestDiag("control-case before asserts");
    const bool runtime_null = !spine.runtime;
    const bool animation_idle = spine.current_animation == "idle";
    const bool dirty_animation = spine.dirty_animation;
    const bool visible_false = !spine.visible;
    PrintSpineTestDiag("control-case after bool snapshots");
    if (!runtime_null) {
        FAIL("runtime should remain null in control case");
    }
    if (!animation_idle) {
        FAIL("current_animation should remain idle in control case");
    }
    if (!dirty_animation) {
        FAIL("dirty_animation should remain true in control case");
    }
    if (!visible_false) {
        FAIL("visible should remain false in control case");
    }
    PrintSpineTestDiag("control-case before explicit component remove");
    world.registry().remove<SpineRendererComponent>(entity);
    PrintSpineTestDiag("control-case after explicit component remove");
    PrintSpineTestDiag("control-case return");
}

TEST_CASE("Given_EmptySpineComponent_When_ManualEarlyReturnPath_Then_ComponentStateRemainsUnchanged", "[engine][unit][spine]") {
    PrintSpineTestDiag("manual-case enter");
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);
    spine.current_animation = "idle";
    spine.dirty_animation = true;
    spine.visible = false;

    PrintSpineTestDiag("manual-case before manual view");
    auto view = world.registry().view<SpineRendererComponent>();
    for (auto e : view) {
        auto& comp = view.get<SpineRendererComponent>(e);
        const bool has_complete_paths = !comp.skeleton_data_path.empty() && !comp.atlas_path.empty();
        const bool has_runtime = static_cast<bool>(comp.runtime);
        const bool needs_assets = has_complete_paths && !has_runtime;
        const bool animation_idle = comp.current_animation == "idle";
        const bool dirty_animation = comp.dirty_animation;
        const bool visible_false = !comp.visible;
        std::printf("[spine-test] manual-case entity=%u comp=%p has_complete_paths=%d has_runtime=%d needs_assets=%d animation_idle=%d dirty=%d visible_false=%d\n",
                    static_cast<unsigned>(e),
                    static_cast<void*>(&comp),
                    has_complete_paths ? 1 : 0,
                    has_runtime ? 1 : 0,
                    needs_assets ? 1 : 0,
                    animation_idle ? 1 : 0,
                    dirty_animation ? 1 : 0,
                    visible_false ? 1 : 0);
        std::fflush(stdout);
    }
    PrintSpineTestDiag("manual-case after manual view");

    const bool runtime_null = !spine.runtime;
    const bool animation_idle = spine.current_animation == "idle";
    const bool dirty_animation = spine.dirty_animation;
    const bool visible_false = !spine.visible;
    PrintSpineTestDiag("manual-case after bool snapshots");
    if (!runtime_null) {
        FAIL("runtime should remain null in manual case");
    }
    if (!animation_idle) {
        FAIL("current_animation should remain idle in manual case");
    }
    if (!dirty_animation) {
        FAIL("dirty_animation should remain true in manual case");
    }
    if (!visible_false) {
        FAIL("visible should remain false in manual case");
    }
    PrintSpineTestDiag("manual-case before explicit component remove");
    world.registry().remove<SpineRendererComponent>(entity);
    PrintSpineTestDiag("manual-case after explicit component remove");
    PrintSpineTestDiag("manual-case return");
}

// 反向测试：当 atlas 或 skeleton 资源路径缺失时，SpineSystem 应安全返回且不创建运行时对象。
TEST_CASE("Given_MissingSpineAssets_When_Update_Then_RuntimeObjectsRemainNull", "[engine][unit][spine]") {
    PrintSpineTestDiag("case2 enter");
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
    PrintSpineTestDiag("case2 before update");
    system.Update(world.registry(), 1.0f / 60.0f);
    PrintSpineTestDiag("case2 after update");

    PrintSpineTestDiag("case2 before asserts");
    REQUIRE_FALSE(spine.runtime);
    REQUIRE(spine.dirty_animation);
    PrintSpineTestDiag("case2 return");
}

// 回归测试：仅配置 atlas 或 skeleton 任一侧路径时，不应触发半初始化状态。
TEST_CASE("Given_IncompleteSpinePaths_When_Update_Then_RuntimeObjectsRemainNull", "[engine][unit][spine]") {
    PrintSpineTestDiag("case3 enter");
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);

    SpineSystem system;
    AssetManager asset_manager;
    asset_manager.ConfigureDataRoot("data");
    system.SetAssetManager(&asset_manager);

    SECTION("only skeleton path configured") {
        PrintSpineTestDiag("case3 section skeleton only");
        spine.skeleton_data_path = "missing/hero.skel";
        spine.atlas_path.clear();
    }

    SECTION("only atlas path configured") {
        PrintSpineTestDiag("case3 section atlas only");
        spine.skeleton_data_path.clear();
        spine.atlas_path = "missing/hero.atlas";
    }

    PrintSpineTestDiag("case3 before update");
    REQUIRE_NOTHROW(system.Update(world.registry(), 1.0f / 60.0f));
    PrintSpineTestDiag("case3 after update");
    PrintSpineTestDiag("case3 before asserts");
    REQUIRE_FALSE(spine.runtime);
    REQUIRE(spine.textures.empty());
    PrintSpineTestDiag("case3 return");
}

// 回归测试：多个缺失资源的 Spine 组件同时更新时，应彼此独立且保持稳定。
TEST_CASE("Given_MultipleMissingSpineComponents_When_Update_Then_AllComponentsRemainStable", "[engine][unit][spine]") {
    PrintSpineTestDiag("case4 enter");
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
    spine_b.skeleton_data_path = "missing/b.skel";
    spine_b.atlas_path = "missing/b.atlas";
    spine_b.current_animation = "run";
    spine_b.dirty_animation = true;
    spine_b.time_scale = 1.25f;

    AssetManager asset_manager;
    asset_manager.ConfigureDataRoot("data");

    SpineSystem system;
    system.SetAssetManager(&asset_manager);
    PrintSpineTestDiag("case4 before update");
    system.Update(world.registry(), 1.0f / 30.0f);
    PrintSpineTestDiag("case4 after update");

    PrintSpineTestDiag("case4 before asserts");
    REQUIRE_FALSE(spine_a.runtime);
    REQUIRE(spine_a.dirty_animation);
    REQUIRE(spine_a.time_scale == Approx(0.5f));

    REQUIRE_FALSE(spine_b.runtime);
    REQUIRE(spine_b.dirty_animation);
    REQUIRE(spine_b.time_scale == Approx(1.25f));
    PrintSpineTestDiag("case4 return");
}