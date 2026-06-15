/**
 * @file animator_component_test.cpp
 * @brief AnimatorComponent 动画状态机组件的单元测试
 *
 * 覆盖场景：
 * - SetBool / SetFloat 参数设置与读取
 * - PlaySegment 分段播放逻辑
 * - 状态与转换规则的数据结构
 * - fired_events 管理
 */

#include <gtest/gtest.h>
#include "engine/ecs/animation.h"

// ============================================================
// SetBool / SetFloat 参数
// ============================================================

// 测试 动画器组件：设置Boolstore且读取
TEST(AnimatorComponentTest, SetBoolstoreAndRead) {
    AnimatorComponent anim;
    anim.SetBool("is_walking", true);
    anim.SetBool("is_jumping", false);

    EXPECT_TRUE(anim.bool_params["is_walking"]);
    EXPECT_FALSE(anim.bool_params["is_jumping"]);
    EXPECT_EQ(anim.bool_params.count("nonexistent"), 0u);
}

// 测试 动画器组件：设置布尔Overwrite已存在值
TEST(AnimatorComponentTest, SetBoolOverwriteExistingValue) {
    AnimatorComponent anim;
    anim.SetBool("flag", true);
    EXPECT_TRUE(anim.bool_params["flag"]);
    anim.SetBool("flag", false);
    EXPECT_FALSE(anim.bool_params["flag"]);
}

// 测试 动画器组件：设置Floatstore且读取
TEST(AnimatorComponentTest, SetFloatstoreAndRead) {
    AnimatorComponent anim;
    anim.SetFloat("speed", 5.5f);
    anim.SetFloat("health", -10.0f);

    EXPECT_FLOAT_EQ(anim.float_params["speed"], 5.5f);
    EXPECT_FLOAT_EQ(anim.float_params["health"], -10.0f);
}

// 测试 动画器组件：设置浮点Overwrite已存在值
TEST(AnimatorComponentTest, SetFloatOverwriteExistingValue) {
    AnimatorComponent anim;
    anim.SetFloat("speed", 1.0f);
    anim.SetFloat("speed", 99.0f);
    EXPECT_FLOAT_EQ(anim.float_params["speed"], 99.0f);
}

// ============================================================
// PlaySegment 分段播放
// ============================================================

// 测试 动画器组件：播放段设置帧范围
TEST(AnimatorComponentTest, PlaySegmentSetFrameRange) {
    AnimatorComponent anim;
    anim.PlaySegment(3, 8, true);

    EXPECT_EQ(anim.segment_start_frame, 3);
    EXPECT_EQ(anim.segment_end_frame, 8);
    EXPECT_TRUE(anim.segment_loop);
}

// 测试 动画器组件：播放段无循环
TEST(AnimatorComponentTest, PlaySegmentNoLoop) {
    AnimatorComponent anim;
    anim.PlaySegment(0, 5, false);
    EXPECT_FALSE(anim.segment_loop);
}

// 测试 动画器组件：播放段重置时间且帧
TEST(AnimatorComponentTest, PlaySegmentResetTimeAndFrames) {
    AnimatorComponent anim;
    anim.current_time = 10.0f;
    anim.current_frame = 20;

    anim.PlaySegment(2, 6, true);

    EXPECT_FLOAT_EQ(anim.current_time, 0.0f);
    EXPECT_EQ(anim.current_frame, 2);
    EXPECT_TRUE(anim.playing);
}

// 测试 动画器组件：播放Segmentburdenstart Byclamp到0
TEST(AnimatorComponentTest, PlaySegmentburdenstartByclampTo0) {
    AnimatorComponent anim;
    anim.PlaySegment(-5, 10, true);
    EXPECT_EQ(anim.segment_start_frame, 0);
}

// 测试 动画器组件：播放Segmentburdenendnot Beclamp
TEST(AnimatorComponentTest, PlaySegmentburdenendnotBeclamp) {
    AnimatorComponent anim;
    anim.PlaySegment(0, -1, false);
    EXPECT_EQ(anim.segment_end_frame, -1);
}

// ============================================================
// 状态与转换数据结构
// ============================================================

// 测试 动画器组件：添加到状态且过渡
TEST(AnimatorComponentTest, AddToStateAndTransition) {
    AnimatorComponent anim;

    AnimationState walk_state;
    walk_state.name = "walk";
    walk_state.loop = true;
    walk_state.frame_rate = 12.0f;
    anim.states["walk"] = walk_state;

    AnimationTransition trans;
    trans.to_state = "run";
    trans.condition_param = "speed";
    trans.condition_value = true;
    anim.transitions["walk"].push_back(trans);

    ASSERT_EQ(anim.states.size(), 1u);
    EXPECT_EQ(anim.states["walk"].name, "walk");
    EXPECT_FLOAT_EQ(anim.states["walk"].frame_rate, 12.0f);

    ASSERT_EQ(anim.transitions["walk"].size(), 1u);
    EXPECT_EQ(anim.transitions["walk"][0].to_state, "run");
}

// 测试 动画器组件：默认值
TEST(AnimatorComponentTest, DefaultValues) {
    AnimatorComponent anim;
    EXPECT_TRUE(anim.current_state.empty());
    EXPECT_FLOAT_EQ(anim.current_time, 0.0f);
    EXPECT_EQ(anim.current_frame, 0);
    EXPECT_EQ(anim.segment_start_frame, 0);
    EXPECT_EQ(anim.segment_end_frame, -1);
    EXPECT_TRUE(anim.segment_loop);
    EXPECT_TRUE(anim.playing);
    EXPECT_TRUE(anim.fired_events.empty());
}

// ============================================================
// fired_events 管理
// ============================================================

// 测试 动画器组件：fired事件能够应已添加到清空
TEST(AnimatorComponentTest, fired_EventsCanBeAddedToClear) {
    AnimatorComponent anim;
    anim.fired_events.push_back("step_left");
    anim.fired_events.push_back("step_right");
    EXPECT_EQ(anim.fired_events.size(), 2u);

    anim.fired_events.clear();
    EXPECT_TRUE(anim.fired_events.empty());
}
