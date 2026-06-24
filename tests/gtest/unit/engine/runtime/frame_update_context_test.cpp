/**
 * @file frame_update_context_test.cpp
 * @brief FrameUpdateContext 统一帧上下文通道的单元测试
 *
 * FrameUpdateContext 是贯穿 update graph 的单一可扩展载体（见
 * docs/design/TIME_SCALE.md）。本测试验证：
 * - 默认值与聚合构造语义（time 子结构 + frame_index）。
 * - 双时间通道（scaled / unscaled）从 frame.time 取出后正确路由：
 *   gameplay（动画）吃 scaled_dt，scale=0 时冻结；UI/真实时间通道走 unscaled_dt
 *   仍前进（暂停菜单不冻结）——这正是各 gameplay 模块 OnUpdate 内部的分流契约。
 * - frame_index 单调字段随上下文携带（未来贯穿式逐帧状态扩展位）。
 */

#include <gtest/gtest.h>

#include "engine/base/frame_update_context.h"
#include "modules/gameplay_2d/animation/animation_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/animation.h"
#include "engine/ecs/components_2d.h"

namespace {

AnimationState MakeWalkState() {
    AnimationState s;
    s.name = "walk";
    s.frame_rate = 10.0f;  // 每帧 0.1s
    s.loop = true;
    s.frame_handles = {1u, 2u, 3u, 4u};
    return s;
}

Entity MakeAnimatedEntity(World& world) {
    auto e = world.CreateEntity();
    auto& anim = world.registry().emplace<AnimatorComponent>(e);
    world.registry().emplace<SpriteRendererComponent>(e);
    anim.states["walk"] = MakeWalkState();
    anim.current_state = "walk";
    anim.current_frame = 0;
    anim.current_time = 0.0f;
    return e;
}

} // namespace

// 默认值：time 默认 time_scale=1、dt=0；frame_index=0。
TEST(FrameUpdateContextTest, Defaults) {
    dse::FrameUpdateContext frame;
    EXPECT_FLOAT_EQ(frame.time.scaled_dt, 0.0f);
    EXPECT_FLOAT_EQ(frame.time.unscaled_dt, 0.0f);
    EXPECT_FLOAT_EQ(frame.time.time_scale, 1.0f);
    EXPECT_EQ(frame.frame_index, 0u);
}

// 聚合构造：time 子结构与 frame_index 原样携带。
TEST(FrameUpdateContextTest, AggregateConstructionCarriesFields) {
    dse::FrameUpdateContext frame{dse::TimeContext{0.5f, 1.0f, 0.5f}, 42u};
    EXPECT_FLOAT_EQ(frame.time.scaled_dt, 0.5f);
    EXPECT_FLOAT_EQ(frame.time.unscaled_dt, 1.0f);
    EXPECT_FLOAT_EQ(frame.time.time_scale, 0.5f);
    EXPECT_EQ(frame.frame_index, 42u);
}

// 通道路由：正常 scale=1 时 gameplay 经 scaled_dt 推进。
TEST(FrameUpdateContextTest, ScaledChannelDrivesGameplay) {
    World world;
    AnimationSystem sys;
    auto e = MakeAnimatedEntity(world);

    dse::FrameUpdateContext frame{dse::TimeContext{0.15f, 0.15f, 1.0f}, 1u};
    sys.Update(world, frame.time.scaled_dt);  // gameplay 通道
    EXPECT_EQ(world.registry().get<AnimatorComponent>(e).current_frame, 1);
}

// 暂停语义：scale=0 时 scaled_dt=0 → gameplay 冻结，但 unscaled_dt 仍 >0（UI 可前进）。
TEST(FrameUpdateContextTest, PausedScaledChannelFreezesGameplayUnscaledStillAdvances) {
    World world;
    AnimationSystem sys;
    auto e = MakeAnimatedEntity(world);

    // 暂停帧：unscaled 真实时间照常，scaled 归零。
    dse::FrameUpdateContext frame{dse::TimeContext{0.0f, 0.5f, 0.0f}, 7u};

    // gameplay 通道：scaled_dt=0 → 动画完全冻结。
    sys.Update(world, frame.time.scaled_dt);
    EXPECT_EQ(world.registry().get<AnimatorComponent>(e).current_frame, 0);
    EXPECT_FLOAT_EQ(world.registry().get<AnimatorComponent>(e).current_time, 0.0f);

    // UI / 真实时间通道：unscaled_dt 仍可用于暂停菜单等不冻结逻辑。
    float ui_accum = 0.0f;
    ui_accum += frame.time.unscaled_dt;
    EXPECT_GT(ui_accum, 0.0f);
}
