/**
* @file animation_state_machine_test.cpp
* @brief AnimationStateMachine 三维动画状态机的单元测试
*
* 覆盖场景：
* - 添加状态与参数
* - 设置默认状态
* - Float / Int / Bool / Trigger 参数读写
* - EvaluateTransition 条件评估
*/

#include <gtest/gtest.h>
#include "modules/gameplay_3d/animation/animation_state_machine.h"

using namespace dse::gameplay3d;

class AnimStateMachineTest : public ::testing::Test {
protected:
    AnimationStateMachine sm;
};

// ============================================================
// 状态管理
// ============================================================

TEST_F(AnimStateMachineTest, AddToState) {
    AnimState state;
    state.name = "Idle";
    state.danim_path = "anim/idle.danim";
    state.loop = true;
    sm.AddState(state);

    auto& states = sm.GetStates();
    ASSERT_TRUE(states.count("Idle") > 0u);
    EXPECT_EQ(states.at("Idle").danim_path, "anim/idle.danim");
}

TEST_F(AnimStateMachineTest, SetUpDefaultState) {
    AnimState state;
    state.name = "Walk";
    sm.AddState(state);
    sm.SetDefaultState("Walk");
    EXPECT_EQ(sm.GetDefaultState(), "Walk");
}

// ============================================================
// 参数管理
// ============================================================

TEST_F(AnimStateMachineTest, FloatParameterReadingAndWriting) {
    sm.AddParameter("speed", AnimParamType::Float, 0.0f);
    sm.SetFloat("speed", 5.0f);
    EXPECT_FLOAT_EQ(sm.GetFloat("speed"), 5.0f);
}

TEST_F(AnimStateMachineTest, IntParameterReadingAndWriting) {
    sm.AddParameter("count", AnimParamType::Int, 0);
    sm.SetInt("count", 3);
    EXPECT_EQ(sm.GetInt("count"), 3);
}

TEST_F(AnimStateMachineTest, BoolParameterReadingAndWriting) {
    sm.AddParameter("alive", AnimParamType::Bool, true);
    EXPECT_TRUE(sm.GetBool("alive"));
    sm.SetBool("alive", false);
    EXPECT_FALSE(sm.GetBool("alive"));
}

TEST_F(AnimStateMachineTest, TriggerSetupAndReset) {
    sm.AddTrigger("jump");
    sm.SetTrigger("jump");
    auto& params = sm.GetParameters();
    EXPECT_TRUE(params.at("jump").is_triggered);

    sm.ResetTrigger("jump");
    EXPECT_FALSE(params.at("jump").is_triggered);
}

// ============================================================
// 条件评估
// ============================================================

TEST_F(AnimStateMachineTest, Transition_Greater) {
    sm.AddParameter("speed", AnimParamType::Float, 0.0f);
    sm.SetFloat("speed", 5.0f);

    AnimTransition trans;
    trans.target_state = "Run";
    AnimTransitionCondition cond;
    cond.parameter_name = "speed";
    cond.mode = AnimConditionMode::Greater;
    cond.threshold = 3.0f;
    trans.conditions.push_back(cond);

    EXPECT_TRUE(sm.EvaluateTransition(trans, 1.0f));
}

TEST_F(AnimStateMachineTest, Transition_IfBool) {
    sm.AddParameter("grounded", AnimParamType::Bool, true);

    AnimTransition trans;
    AnimTransitionCondition cond;
    cond.parameter_name = "grounded";
    cond.mode = AnimConditionMode::If;
    trans.conditions.push_back(cond);

    EXPECT_TRUE(sm.EvaluateTransition(trans, 1.0f));

    sm.SetBool("grounded", false);
    EXPECT_FALSE(sm.EvaluateTransition(trans, 1.0f));
}

TEST_F(AnimStateMachineTest, Transition_WithoutWithTime) {
    AnimTransition trans;
    trans.has_exit_time = true;
    trans.exit_time = 0.8f;
    // 无条件，仅检查退出时间
    EXPECT_TRUE(sm.EvaluateTransition(trans, 0.9f));
    EXPECT_FALSE(sm.EvaluateTransition(trans, 0.5f));
}
