/**
 * @file blend_tree_crossfade_test.cpp
 * @brief AnimationStateMachine BlendTree + Crossfade 专项测试
 *
 * 测试策略：
 * - BlendTreeNode 数据结构默认值
 * - AnimState 的 BlendTree 配置
 * - AnimTransition crossfade 参数
 * - AnimationStateMachine 参数驱动（Float/Int/Bool/Trigger）
 * - EvaluateTransition 条件评估
 * - 多状态 + Trigger 转换链
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/animation/animation_state_machine.h"

using namespace dse::gameplay3d;

// ============================================================
// BlendTreeNode / AnimState 数据结构
// ============================================================

TEST(BlendTreeNodeTest, DefaultValues) {
    BlendTreeNode node;
    EXPECT_TRUE(node.danim_path.empty());
    EXPECT_FLOAT_EQ(node.threshold, 0.0f);
}

TEST(AnimStateTest, DefaultValues_Single) {
    AnimState state;
    EXPECT_TRUE(state.name.empty());
    EXPECT_TRUE(state.danim_path.empty());
    EXPECT_FLOAT_EQ(state.speed, 1.0f);
    EXPECT_TRUE(state.loop);
    EXPECT_FALSE(state.is_blend_tree);
    EXPECT_TRUE(state.blend_parameter.empty());
    EXPECT_TRUE(state.blend_nodes.empty());
    EXPECT_TRUE(state.transitions.empty());
}

TEST(AnimStateTest, BlendTreeConfiguration) {
    AnimState state;
    state.name = "Locomotion";
    state.is_blend_tree = true;
    state.blend_parameter = "speed";
    state.blend_nodes.push_back({"idle.danim", 0.0f});
    state.blend_nodes.push_back({"walk.danim", 0.5f});
    state.blend_nodes.push_back({"run.danim", 1.0f});

    EXPECT_TRUE(state.is_blend_tree);
    EXPECT_EQ(state.blend_nodes.size(), 3u);
    EXPECT_FLOAT_EQ(state.blend_nodes[0].threshold, 0.0f);
    EXPECT_FLOAT_EQ(state.blend_nodes[2].threshold, 1.0f);
    EXPECT_EQ(state.blend_nodes[1].danim_path, "walk.danim");
}

// ============================================================
// AnimTransition / Crossfade
// ============================================================

TEST(AnimTransitionTest, DefaultValues) {
    AnimTransition t;
    EXPECT_TRUE(t.target_state.empty());
    EXPECT_TRUE(t.has_exit_time);
    EXPECT_FLOAT_EQ(t.exit_time, 1.0f);
    EXPECT_FLOAT_EQ(t.transition_duration, 0.25f);
    EXPECT_TRUE(t.conditions.empty());
}

TEST(AnimTransitionTest, CrossfadeParameters) {
    AnimTransition t;
    t.target_state = "Attack";
    t.has_exit_time = false;
    t.transition_duration = 0.15f;
    AnimTransitionCondition cond;
    cond.parameter_name = "attack_trigger";
    cond.mode = AnimConditionMode::If;
    t.conditions.push_back(cond);

    EXPECT_EQ(t.target_state, "Attack");
    EXPECT_FALSE(t.has_exit_time);
    EXPECT_FLOAT_EQ(t.transition_duration, 0.15f);
    EXPECT_EQ(t.conditions.size(), 1u);
    EXPECT_EQ(t.conditions[0].mode, AnimConditionMode::If);
}

// ============================================================
// AnimationStateMachine 参数系统
// ============================================================

TEST(AnimationStateMachineTest, FloatParameters) {
    AnimationStateMachine sm;
    sm.AddParameter("speed", AnimParamType::Float, 0.0f);
    EXPECT_FLOAT_EQ(sm.GetFloat("speed"), 0.0f);
    sm.SetFloat("speed", 0.75f);
    EXPECT_FLOAT_EQ(sm.GetFloat("speed"), 0.75f);
}

TEST(AnimationStateMachineTest, IntParameters) {
    AnimationStateMachine sm;
    sm.AddParameter("combo", AnimParamType::Int, 0);
    EXPECT_EQ(sm.GetInt("combo"), 0);
    sm.SetInt("combo", 3);
    EXPECT_EQ(sm.GetInt("combo"), 3);
}

TEST(AnimationStateMachineTest, BoolParameters) {
    AnimationStateMachine sm;
    sm.AddParameter("grounded", AnimParamType::Bool, true);
    EXPECT_TRUE(sm.GetBool("grounded"));
    sm.SetBool("grounded", false);
    EXPECT_FALSE(sm.GetBool("grounded"));
}

TEST(AnimationStateMachineTest, TriggerParameters) {
    AnimationStateMachine sm;
    sm.AddTrigger("jump");
    const auto& params = sm.GetParameters();
    ASSERT_NE(params.find("jump"), params.end());
    EXPECT_EQ(params.at("jump").type, AnimParamType::Trigger);
    EXPECT_FALSE(params.at("jump").is_triggered);

    sm.SetTrigger("jump");
    EXPECT_TRUE(sm.GetParameters().at("jump").is_triggered);
    sm.ResetTrigger("jump");
    EXPECT_FALSE(sm.GetParameters().at("jump").is_triggered);
}

// ============================================================
// 状态管理
// ============================================================

TEST(AnimationStateMachineTest, AddStateAndDefaultState) {
    AnimationStateMachine sm;
    AnimState idle;
    idle.name = "Idle";
    idle.danim_path = "idle.danim";
    sm.AddState(idle);
    sm.SetDefaultState("Idle");

    EXPECT_EQ(sm.GetDefaultState(), "Idle");
    EXPECT_EQ(sm.GetStates().size(), 1u);
    EXPECT_NE(sm.GetStates().find("Idle"), sm.GetStates().end());
}

TEST(AnimationStateMachineTest, MultiStateBlendTree) {
    AnimationStateMachine sm;

    AnimState locomotion;
    locomotion.name = "Locomotion";
    locomotion.is_blend_tree = true;
    locomotion.blend_parameter = "speed";
    locomotion.blend_nodes = {{"idle.danim", 0.0f}, {"walk.danim", 0.5f}, {"run.danim", 1.0f}};
    sm.AddState(locomotion);

    AnimState jump;
    jump.name = "Jump";
    jump.danim_path = "jump.danim";
    jump.loop = false;
    sm.AddState(jump);

    sm.SetDefaultState("Locomotion");
    sm.AddParameter("speed", AnimParamType::Float, 0.0f);

    EXPECT_EQ(sm.GetStates().size(), 2u);
    EXPECT_TRUE(sm.GetStates().at("Locomotion").is_blend_tree);
    EXPECT_FALSE(sm.GetStates().at("Jump").is_blend_tree);
    EXPECT_EQ(sm.GetStates().at("Locomotion").blend_nodes.size(), 3u);
}

// ============================================================
// EvaluateTransition
// ============================================================

TEST(AnimationStateMachineTest, EvaluateTransition_Float_Greater) {
    AnimationStateMachine sm;
    sm.AddParameter("speed", AnimParamType::Float, 0.0f);

    AnimTransition t;
    t.target_state = "Run";
    t.has_exit_time = false;
    AnimTransitionCondition cond;
    cond.parameter_name = "speed";
    cond.mode = AnimConditionMode::Greater;
    cond.threshold = 0.5f;
    t.conditions.push_back(cond);

    // speed=0 → 不满足
    EXPECT_FALSE(sm.EvaluateTransition(t, 1.0f));

    sm.SetFloat("speed", 0.8f);
    EXPECT_TRUE(sm.EvaluateTransition(t, 1.0f));
}

TEST(AnimationStateMachineTest, EvaluateTransition_ExitTime) {
    AnimationStateMachine sm;
    AnimTransition t;
    t.target_state = "Idle";
    t.has_exit_time = true;
    t.exit_time = 0.9f;

    // normalized_time=0.5 < exit_time=0.9 → 不满足
    EXPECT_FALSE(sm.EvaluateTransition(t, 0.5f));

    // normalized_time=0.95 >= exit_time=0.9 → 满足
    EXPECT_TRUE(sm.EvaluateTransition(t, 0.95f));
}

TEST(AnimationStateMachineTest, EvaluateTransition_Trigger) {
    AnimationStateMachine sm;
    sm.AddTrigger("attack");

    AnimTransition t;
    t.target_state = "Attack";
    t.has_exit_time = false;
    AnimTransitionCondition cond;
    cond.parameter_name = "attack";
    cond.mode = AnimConditionMode::If;
    t.conditions.push_back(cond);

    // trigger 未激活 → false
    EXPECT_FALSE(sm.EvaluateTransition(t, 1.0f));

    sm.SetTrigger("attack");
    EXPECT_TRUE(sm.EvaluateTransition(t, 1.0f));
}

TEST(AnimationStateMachineTest, EvaluateTransition_MultipleConditionsAND) {
    AnimationStateMachine sm;
    sm.AddParameter("speed", AnimParamType::Float, 0.0f);
    sm.AddParameter("grounded", AnimParamType::Bool, true);

    AnimTransition t;
    t.target_state = "Run";
    t.has_exit_time = false;
    AnimTransitionCondition c1;
    c1.parameter_name = "speed";
    c1.mode = AnimConditionMode::Greater;
    c1.threshold = 0.5f;
    AnimTransitionCondition c2;
    c2.parameter_name = "grounded";
    c2.mode = AnimConditionMode::If;
    t.conditions = {c1, c2};

    // speed=0, grounded=true → speed 不满足
    EXPECT_FALSE(sm.EvaluateTransition(t, 1.0f));

    sm.SetFloat("speed", 0.8f);
    // speed ok, grounded ok
    EXPECT_TRUE(sm.EvaluateTransition(t, 1.0f));

    sm.SetBool("grounded", false);
    // speed ok, grounded not ok
    EXPECT_FALSE(sm.EvaluateTransition(t, 1.0f));
}
