#include "catch/catch.hpp"
#include "modules/gameplay_2d/animation/animation_system.h"
#include "engine/ecs/components_2d.h"

// 正向测试：满足转换条件时应切换到目标状态并推进帧索引。
TEST_CASE("Given_TransitionConditionMet_When_Update_Then_StateSwitchesAndFrameAdvances", "[engine][unit][animation]") {
    World world;
    auto entity = world.CreateEntity();
    auto& animator = world.registry().emplace<AnimatorComponent>(entity);
    auto& sprite = world.registry().emplace<SpriteRendererComponent>(entity);
    (void)sprite;

    AnimationState idle;
    idle.name = "idle";
    idle.frame_handles = {1, 2};
    idle.frame_rate = 10.0f;
    idle.loop = true;

    AnimationState run;
    run.name = "run";
    run.frame_handles = {3, 4};
    run.frame_rate = 10.0f;
    run.loop = true;

    animator.states["idle"] = idle;
    animator.states["run"] = run;
    animator.current_state = "idle";
    animator.transitions["idle"].push_back(AnimationTransition{"run", "moving", true});
    animator.bool_params["moving"] = true;

    AnimationSystem system;
    system.Update(world, 0.2f);

    REQUIRE(animator.current_state == "run");
    REQUIRE(animator.current_frame >= 0);
    REQUIRE(animator.current_frame <= 1);
    REQUIRE((sprite.texture_handle == 3 || sprite.texture_handle == 4));
}

// 边界测试：非循环动画到结尾时应停止在最后一帧。
TEST_CASE("Given_NonLoopAnimation_When_ReachesEnd_Then_PlaybackStopsAtLastFrame", "[engine][unit][animation]") {
    World world;
    auto entity = world.CreateEntity();
    auto& animator = world.registry().emplace<AnimatorComponent>(entity);
    world.registry().emplace<SpriteRendererComponent>(entity);

    AnimationState once;
    once.name = "once";
    once.frame_handles = {11, 12, 13};
    once.frame_rate = 10.0f;
    once.loop = false;
    animator.states["once"] = once;
    animator.current_state = "once";
    animator.playing = true;

    AnimationSystem system;
    system.Update(world, 1.0f);

    REQUIRE_FALSE(animator.playing);
    REQUIRE(animator.current_frame == 2);
}

// 反向测试：状态为空或无有效帧时，更新不应修改播放状态并保持稳定。
TEST_CASE("Given_EmptyAnimationData_When_Update_Then_StateRemainsStable", "[engine][unit][animation]") {
    World world;
    auto entity = world.CreateEntity();
    auto& animator = world.registry().emplace<AnimatorComponent>(entity);
    world.registry().emplace<SpriteRendererComponent>(entity);
    animator.states["empty"] = AnimationState{};
    animator.current_state = "empty";
    animator.current_frame = 0;
    animator.playing = true;

    AnimationSystem system;
    system.Update(world, 0.5f);

    REQUIRE(animator.current_frame == 0);
    REQUIRE(animator.playing);
}
