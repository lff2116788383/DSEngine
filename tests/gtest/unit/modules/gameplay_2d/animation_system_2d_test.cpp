/**
 * @file animation_system_2d_test.cpp
 * @brief AnimationSystem (2D) 帧动画系统的单元测试
 *
 * 覆盖场景：
 * - 空 World 调用 Update 不崩溃
 * - 无状态时 Update 不崩溃（current_state 为空自动初始化为第一个状态）
 * - 帧推进：单帧时间推进后 current_frame 递增
 * - 循环播放：播放到末帧后回到首帧
 * - 非循环播放：播放到末帧后停止，playing 置 false
 * - 状态机转换：SetBool 触发条件满足后切换状态
 * - PlaySegment 分段播放
 * - 帧率控制：frame_rate 影响帧推进速度
 * - fired_events 事件触发
 */

#include <gtest/gtest.h>
#include "modules/gameplay_2d/animation/animation_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/animation.h"
#include "engine/ecs/components_2d.h"

class AnimationSystem2DTest : public ::testing::Test {
protected:
    World world;
    AnimationSystem sys;

    /// 创建一个带动画+精灵的实体，返回实体和组件引用
    std::tuple<Entity, AnimatorComponent*, SpriteRendererComponent*>
    CreateAnimatedEntity() {
        auto e = world.CreateEntity();
        auto& anim = world.registry().emplace<AnimatorComponent>(e);
        auto& sprite = world.registry().emplace<SpriteRendererComponent>(e);
        return {e, &anim, &sprite};
    }
};

// 测试 动画系统2D：空世界调用更新不崩溃
TEST_F(AnimationSystem2DTest, EmptyWorldCallsUpdateDoesNotCrash) {
    EXPECT_NO_THROW(sys.Update(world, 1.0f / 60.0f));
}

// 测试 动画系统2D：无状态当更新不崩溃
TEST_F(AnimationSystem2DTest, WithoutStateWhenUpdateDoesNotCrash) {
    auto [e, anim, sprite] = CreateAnimatedEntity();
    // animator.states 为空，Update 应跳过
    EXPECT_NO_THROW(sys.Update(world, 1.0f / 60.0f));
}

// 测试 动画系统2D：单一状态自动Initializecurrent状态
TEST_F(AnimationSystem2DTest, SingleStateAutoInitializecurrent_state) {
    auto [e, anim, sprite] = CreateAnimatedEntity();
    AnimationState state;
    state.name = "idle";
    state.frame_rate = 10.0f;
    state.loop = true;
    state.frame_handles = {1u, 2u, 3u};
    anim->states["idle"] = state;
    anim->current_state = ""; // 空，应自动初始化

    sys.Update(world, 0.001f);
    EXPECT_EQ(anim->current_state, "idle");
}

// 测试 动画系统2D：帧Advancecurrent帧递增
TEST_F(AnimationSystem2DTest, FrameAdvancecurrent_FrameIncrements) {
    auto [e, anim, sprite] = CreateAnimatedEntity();
    AnimationState state;
    state.name = "walk";
    state.frame_rate = 10.0f;
    state.loop = true;
    state.frame_handles = {10u, 20u, 30u, 40u};
    anim->states["walk"] = state;
    anim->current_state = "walk";
    anim->current_frame = 0;
    anim->current_time = 0.0f;

    // 推进超过一帧时长（1/10 = 0.1s）
    sys.Update(world, 0.15f);
    EXPECT_GT(anim->current_frame, 0);
}

// 测试 动画系统2D：循环回放Wraps到首个帧在结束
TEST_F(AnimationSystem2DTest, LoopPlaybackWrapsToFirstFrameAtEnd) {
    auto [e, anim, sprite] = CreateAnimatedEntity();
    AnimationState state;
    state.name = "loop_anim";
    state.frame_rate = 10.0f;
    state.loop = true;
    state.frame_handles = {1u, 2u};
    anim->states["loop_anim"] = state;
    anim->current_state = "loop_anim";
    anim->current_frame = 1; // 末帧
    anim->current_time = 0.0f;

    // 推进超过一帧
    sys.Update(world, 0.15f);
    EXPECT_EQ(anim->current_frame, 0); // 回到首帧
}

// 测试 动画系统2D：非循环回放Stops在最后帧
TEST_F(AnimationSystem2DTest, NonLoopPlaybackStopsAtLastFrame) {
    auto [e, anim, sprite] = CreateAnimatedEntity();
    AnimationState state;
    state.name = "once";
    state.frame_rate = 10.0f;
    state.loop = false;
    state.frame_handles = {1u, 2u};
    anim->states["once"] = state;
    anim->current_state = "once";
    anim->current_frame = 1; // 末帧
    anim->current_time = 0.0f;
    anim->playing = true;

    sys.Update(world, 0.15f);
    EXPECT_FALSE(anim->playing);
    EXPECT_EQ(anim->current_frame, 1); // 停在末帧
}

// 测试 动画系统2D：设置Booltrigger状态过渡
TEST_F(AnimationSystem2DTest, SetBooltriggerStateTransition) {
    auto [e, anim, sprite] = CreateAnimatedEntity();

    AnimationState idle_state;
    idle_state.name = "idle";
    idle_state.frame_rate = 10.0f;
    idle_state.loop = true;
    idle_state.frame_handles = {1u};
    anim->states["idle"] = idle_state;

    AnimationState walk_state;
    walk_state.name = "walk";
    walk_state.frame_rate = 10.0f;
    walk_state.loop = true;
    walk_state.frame_handles = {2u};
    anim->states["walk"] = walk_state;

    AnimationTransition trans;
    trans.to_state = "walk";
    trans.condition_param = "is_walking";
    trans.condition_value = true;
    anim->transitions["idle"].push_back(trans);

    anim->current_state = "idle";
    anim->current_frame = 0;
    anim->current_time = 0.0f;
    anim->playing = true;

    // 触发转换条件
    anim->SetBool("is_walking", true);
    sys.Update(world, 0.001f);

    EXPECT_EQ(anim->current_state, "walk");
    EXPECT_EQ(anim->current_frame, 0);
}

// 测试 动画系统2D：播放段回放
TEST_F(AnimationSystem2DTest, PlaySegmentSegmentPlayback) {
    auto [e, anim, sprite] = CreateAnimatedEntity();
    AnimationState state;
    state.name = "full";
    state.frame_rate = 10.0f;
    state.loop = true;
    state.frame_handles = {1u, 2u, 3u, 4u, 5u};
    anim->states["full"] = state;
    anim->current_state = "full";

    // 只播放第 2~3 帧
    anim->PlaySegment(2, 3, true);
    EXPECT_EQ(anim->current_frame, 2);
    EXPECT_TRUE(anim->playing);
}

// 测试 动画系统2D：帧速率Controls推进速度
TEST_F(AnimationSystem2DTest, FrameRateControlsAdvanceSpeed) {
    auto [e, anim, sprite] = CreateAnimatedEntity();
    AnimationState state;
    state.name = "slow";
    state.frame_rate = 1.0f; // 1fps，每帧 1 秒
    state.loop = true;
    state.frame_handles = {1u, 2u};
    anim->states["slow"] = state;
    anim->current_state = "slow";
    anim->current_frame = 0;
    anim->current_time = 0.0f;

    // 推进 0.5 秒，不足一帧时长
    sys.Update(world, 0.5f);
    EXPECT_EQ(anim->current_frame, 0); // 仍在第 0 帧

    // 再推进 0.6 秒，超过一帧时长
    sys.Update(world, 0.6f);
    EXPECT_EQ(anim->current_frame, 1); // 推进到第 1 帧
}

// 测试 动画系统2D：时间不推进之后停止回放
TEST_F(AnimationSystem2DTest, TimeDoesNotAdvanceAfterStopPlayback) {
    auto [e, anim, sprite] = CreateAnimatedEntity();
    AnimationState state;
    state.name = "paused";
    state.frame_rate = 10.0f;
    state.loop = true;
    state.frame_handles = {1u, 2u, 3u};
    anim->states["paused"] = state;
    anim->current_state = "paused";
    anim->current_frame = 1;
    anim->playing = false;

    sys.Update(world, 1.0f);
    EXPECT_EQ(anim->current_frame, 1); // 未推进
}
