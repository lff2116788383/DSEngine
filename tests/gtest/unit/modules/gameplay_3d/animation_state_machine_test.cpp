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
#include <cstdio>
#include <string>
#include "modules/gameplay_3d/animation/animation_state_machine.h"
#include "engine/ecs/animation_state_machine_serialize.h"

using namespace dse::gameplay3d;

class AnimStateMachineTest : public ::testing::Test {
protected:
    AnimationStateMachine sm;
};

// ============================================================
// 状态管理
// ============================================================

// 测试 动画状态状态机：添加到状态
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

// 测试 动画状态状态机：设置上默认状态
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

// 测试 动画状态状态机：浮点参数读取且写入
TEST_F(AnimStateMachineTest, FloatParameterReadingAndWriting) {
    sm.AddParameter("speed", AnimParamType::Float, 0.0f);
    sm.SetFloat("speed", 5.0f);
    EXPECT_FLOAT_EQ(sm.GetFloat("speed"), 5.0f);
}

// 测试 动画状态状态机：整数参数读取且写入
TEST_F(AnimStateMachineTest, IntParameterReadingAndWriting) {
    sm.AddParameter("count", AnimParamType::Int, 0);
    sm.SetInt("count", 3);
    EXPECT_EQ(sm.GetInt("count"), 3);
}

// 测试 动画状态状态机：布尔参数读取且写入
TEST_F(AnimStateMachineTest, BoolParameterReadingAndWriting) {
    sm.AddParameter("alive", AnimParamType::Bool, true);
    EXPECT_TRUE(sm.GetBool("alive"));
    sm.SetBool("alive", false);
    EXPECT_FALSE(sm.GetBool("alive"));
}

// 测试 动画状态状态机：触发Setup且重置
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

// 测试 动画状态状态机：过渡大于
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

// 测试 动画状态状态机：过渡若布尔
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

// 测试 动画状态状态机：过渡无带时间
TEST_F(AnimStateMachineTest, Transition_WithoutWithTime) {
    AnimTransition trans;
    trans.has_exit_time = true;
    trans.exit_time = 0.8f;
    // 无条件，仅检查退出时间
    EXPECT_TRUE(sm.EvaluateTransition(trans, 0.9f));
    EXPECT_FALSE(sm.EvaluateTransition(trans, 0.5f));
}

// ============================================================
// SelectTransition / ConsumeTransitionTriggers（运行时选择逻辑）
// ============================================================

// 声明顺序即优先级：首个满足的过渡胜出。
TEST_F(AnimStateMachineTest, SelectTransition_PicksFirstMatchByDeclarationOrder) {
    sm.AddParameter("speed", AnimParamType::Float, 5.0f);

    AnimState idle;
    idle.name = "Idle";

    AnimTransition to_walk;  // 条件不满足
    to_walk.target_state = "Walk";
    to_walk.has_exit_time = false;
    AnimTransitionCondition c_walk;
    c_walk.parameter_name = "speed";
    c_walk.mode = AnimConditionMode::Less;
    c_walk.threshold = 1.0f;
    to_walk.conditions.push_back(c_walk);

    AnimTransition to_run_a;  // 满足
    to_run_a.target_state = "RunA";
    to_run_a.has_exit_time = false;
    AnimTransitionCondition c_run;
    c_run.parameter_name = "speed";
    c_run.mode = AnimConditionMode::Greater;
    c_run.threshold = 1.0f;
    to_run_a.conditions.push_back(c_run);

    AnimTransition to_run_b = to_run_a;  // 同样满足，但声明在后
    to_run_b.target_state = "RunB";

    idle.transitions = {to_walk, to_run_a, to_run_b};

    int idx = sm.SelectTransition(idle, 1.0f);
    ASSERT_EQ(idx, 1);
    EXPECT_EQ(idle.transitions[idx].target_state, "RunA");
}

TEST_F(AnimStateMachineTest, SelectTransition_ReturnsMinusOneWhenNoneApply) {
    AnimState s;
    s.name = "S";
    AnimTransition t;
    t.target_state = "Other";
    t.has_exit_time = true;
    t.exit_time = 0.9f;  // exit-time 未到
    s.transitions = {t};

    EXPECT_EQ(sm.SelectTransition(s, 0.5f), -1);
    EXPECT_EQ(sm.SelectTransition(s, 0.95f), 0);
}

// Trigger 被消费后必须复位，保证只触发一次。
TEST_F(AnimStateMachineTest, ConsumeTransitionTriggers_ResetsTriggerOnce) {
    sm.AddTrigger("jump");
    sm.SetTrigger("jump");

    AnimState s;
    s.name = "Ground";
    AnimTransition t;
    t.target_state = "Air";
    t.has_exit_time = false;
    AnimTransitionCondition c;
    c.parameter_name = "jump";
    c.mode = AnimConditionMode::If;
    t.conditions.push_back(c);
    s.transitions = {t};

    int idx = sm.SelectTransition(s, 0.0f);
    ASSERT_EQ(idx, 0);
    sm.ConsumeTransitionTriggers(s.transitions[idx]);

    // 消费后同一 trigger 不应再次命中过渡。
    EXPECT_EQ(sm.SelectTransition(s, 0.0f), -1);
}

// ============================================================
// .dasm 序列化契约
// ============================================================

namespace {

AnimationStateMachine MakeSampleStateMachine() {
    AnimationStateMachine m;

    AnimState idle;
    idle.name = "Idle";
    idle.danim_path = "anim/idle.danim";
    idle.speed = 1.0f;
    idle.loop = true;

    AnimState locomotion;
    locomotion.name = "Locomotion";
    locomotion.is_blend_tree = true;
    locomotion.blend_parameter = "speed";
    locomotion.blend_nodes.push_back({"anim/walk.danim", 0.0f});
    locomotion.blend_nodes.push_back({"anim/run.danim", 1.0f});

    AnimTransition to_loco;
    to_loco.target_state = "Locomotion";
    to_loco.has_exit_time = false;
    to_loco.exit_time = 0.5f;
    to_loco.transition_duration = 0.2f;
    AnimTransitionCondition cond;
    cond.parameter_name = "speed";
    cond.mode = AnimConditionMode::Greater;
    cond.threshold = 0.1f;
    to_loco.conditions.push_back(cond);
    idle.transitions.push_back(to_loco);

    m.AddState(idle);
    m.AddState(locomotion);
    m.SetDefaultState("Idle");

    m.AddParameter("speed", AnimParamType::Float, 2.5f);
    m.AddParameter("ammo", AnimParamType::Int, 7);
    m.AddParameter("grounded", AnimParamType::Bool, true);
    m.AddTrigger("jump");
    m.SetTrigger("jump");
    return m;
}

void ExpectEquivalent(const AnimationStateMachine& a, const AnimationStateMachine& b) {
    EXPECT_EQ(a.GetDefaultState(), b.GetDefaultState());
    ASSERT_EQ(a.GetStates().size(), b.GetStates().size());
    for (const auto& [name, sa] : a.GetStates()) {
        ASSERT_TRUE(b.GetStates().count(name) > 0u) << "missing state " << name;
        const AnimState& sb = b.GetStates().at(name);
        EXPECT_EQ(sa.danim_path, sb.danim_path);
        EXPECT_FLOAT_EQ(sa.speed, sb.speed);
        EXPECT_EQ(sa.loop, sb.loop);
        EXPECT_EQ(sa.is_blend_tree, sb.is_blend_tree);
        EXPECT_EQ(sa.blend_parameter, sb.blend_parameter);
        ASSERT_EQ(sa.blend_nodes.size(), sb.blend_nodes.size());
        for (size_t i = 0; i < sa.blend_nodes.size(); ++i) {
            EXPECT_EQ(sa.blend_nodes[i].danim_path, sb.blend_nodes[i].danim_path);
            EXPECT_FLOAT_EQ(sa.blend_nodes[i].threshold, sb.blend_nodes[i].threshold);
        }
        ASSERT_EQ(sa.transitions.size(), sb.transitions.size());
        for (size_t i = 0; i < sa.transitions.size(); ++i) {
            EXPECT_EQ(sa.transitions[i].target_state, sb.transitions[i].target_state);
            EXPECT_EQ(sa.transitions[i].has_exit_time, sb.transitions[i].has_exit_time);
            EXPECT_FLOAT_EQ(sa.transitions[i].exit_time, sb.transitions[i].exit_time);
            ASSERT_EQ(sa.transitions[i].conditions.size(), sb.transitions[i].conditions.size());
        }
    }
    ASSERT_EQ(a.GetParameters().size(), b.GetParameters().size());
}

}  // namespace

// 测试 .dasm 内存往返：序列化后再解析应等价
TEST(AnimStateMachineSerializeTest, RoundTrip) {
    AnimationStateMachine src = MakeSampleStateMachine();
    std::string text = SerializeStateMachine(src);
    ASSERT_FALSE(text.empty());

    AnimationStateMachine dst;
    AsmDiagnostics diag;
    ASSERT_TRUE(DeserializeStateMachine(dst, text, diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, kAnimStateMachineSchemaVersion);
    ExpectEquivalent(src, dst);

    EXPECT_FLOAT_EQ(dst.GetFloat("speed"), 2.5f);
    EXPECT_EQ(dst.GetInt("ammo"), 7);
    EXPECT_TRUE(dst.GetBool("grounded"));
    EXPECT_TRUE(dst.GetParameters().at("jump").is_triggered);
}

// 测试 .dasm 文件往返
TEST(AnimStateMachineSerializeTest, FileRoundTrip) {
    AnimationStateMachine src = MakeSampleStateMachine();
    std::string path = std::string(::testing::TempDir()) + "dse_asm_roundtrip.dasm";

    AsmDiagnostics save_diag;
    ASSERT_TRUE(SaveStateMachineToFile(src, path, save_diag));

    AnimationStateMachine dst;
    AsmDiagnostics load_diag;
    ASSERT_TRUE(LoadStateMachineFromFile(dst, path, load_diag));
    ExpectEquivalent(src, dst);
    std::remove(path.c_str());
}

// 测试 前向兼容：更高版本号仍解析并记录 warning
TEST(AnimStateMachineSerializeTest, ForwardCompatibleVersion) {
    std::string json =
        "{\"version\":999,\"state_machine\":{\"default_state\":\"A\","
        "\"parameters\":[],\"states\":[{\"name\":\"A\",\"danim_path\":\"a.danim\"}]}}";
    AnimationStateMachine dst;
    AsmDiagnostics diag;
    ASSERT_TRUE(DeserializeStateMachine(dst, json, diag));
    EXPECT_EQ(diag.source_version, 999);
    EXPECT_FALSE(diag.warnings.empty());
    EXPECT_EQ(dst.GetDefaultState(), "A");
}

// 测试 legacy（无 version、无 state_machine 包裹）迁移
TEST(AnimStateMachineSerializeTest, LegacyNoVersionMigration) {
    std::string json =
        "{\"default_state\":\"A\",\"parameters\":[],"
        "\"states\":[{\"name\":\"A\",\"danim_path\":\"a.danim\"}]}";
    AnimationStateMachine dst;
    AsmDiagnostics diag;
    ASSERT_TRUE(DeserializeStateMachine(dst, json, diag));
    EXPECT_EQ(diag.source_version, 0);
    EXPECT_TRUE(diag.migrated);
    EXPECT_EQ(dst.GetDefaultState(), "A");
}

// 测试 损坏 JSON：失败并给出错误
TEST(AnimStateMachineSerializeTest, CorruptedJsonFails) {
    AnimationStateMachine dst;
    AsmDiagnostics diag;
    EXPECT_FALSE(DeserializeStateMachine(dst, "{not valid json", diag));
    EXPECT_FALSE(diag.errors.empty());
}

// 测试 非对象根：失败
TEST(AnimStateMachineSerializeTest, NonObjectRootFails) {
    AnimationStateMachine dst;
    AsmDiagnostics diag;
    EXPECT_FALSE(DeserializeStateMachine(dst, "[1,2,3]", diag));
    EXPECT_FALSE(diag.errors.empty());
}
