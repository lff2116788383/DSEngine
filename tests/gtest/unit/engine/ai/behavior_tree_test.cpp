/**
 * @file behavior_tree_test.cpp
 * @brief 行为树系统单元测试
 */

#include <gtest/gtest.h>
#include "engine/ai/behavior_tree.h"
#include "engine/ai/blackboard.h"

using namespace dse::ai;

// ============================================================
// Blackboard Tests
// ============================================================

class BlackboardTest : public ::testing::Test {
protected:
    Blackboard bb;
};

TEST_F(BlackboardTest, SetAndGetBool) {
    bb.SetBool("flag", true);
    EXPECT_TRUE(bb.GetBool("flag"));
    EXPECT_FALSE(bb.GetBool("nonexistent"));
}

TEST_F(BlackboardTest, SetAndGetInt) {
    bb.SetInt("count", 42);
    EXPECT_EQ(bb.GetInt("count"), 42);
    EXPECT_EQ(bb.GetInt("missing", -1), -1);
}

TEST_F(BlackboardTest, SetAndGetFloat) {
    bb.SetFloat("speed", 3.14f);
    EXPECT_FLOAT_EQ(bb.GetFloat("speed"), 3.14f);
}

TEST_F(BlackboardTest, SetAndGetString) {
    bb.SetString("name", "hello");
    EXPECT_EQ(bb.GetString("name"), "hello");
    EXPECT_EQ(bb.GetString("missing", "default"), "default");
}

TEST_F(BlackboardTest, SetAndGetVec3) {
    bb.SetVec3("pos", glm::vec3(1.0f, 2.0f, 3.0f));
    auto v = bb.GetVec3("pos");
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST_F(BlackboardTest, HasAndErase) {
    bb.SetBool("key", true);
    EXPECT_TRUE(bb.Has("key"));
    bb.Erase("key");
    EXPECT_FALSE(bb.Has("key"));
}

TEST_F(BlackboardTest, Clear) {
    bb.SetBool("a", true);
    bb.SetInt("b", 1);
    bb.Clear();
    EXPECT_EQ(bb.Size(), 0u);
}

TEST_F(BlackboardTest, TypeMismatch_ReturnsDefault) {
    bb.SetBool("flag", true);
    EXPECT_EQ(bb.GetInt("flag", 99), 99); // wrong type → default
}

// ============================================================
// Behavior Tree Node Tests
// ============================================================

class BehaviorTreeTest : public ::testing::Test {
protected:
    Blackboard bb;
};

TEST_F(BehaviorTreeTest, ActionNode_Success) {
    auto action = std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }, "AlwaysSucceed");
    EXPECT_EQ(action->Tick(0.016f, bb), BTStatus::Success);
}

TEST_F(BehaviorTreeTest, ActionNode_Failure) {
    auto action = std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Failure; }, "AlwaysFail");
    EXPECT_EQ(action->Tick(0.016f, bb), BTStatus::Failure);
}

TEST_F(BehaviorTreeTest, ActionNode_Running) {
    int tick_count = 0;
    auto action = std::make_shared<BTAction>(
        [&tick_count](float, Blackboard&) {
            ++tick_count;
            return tick_count >= 3 ? BTStatus::Success : BTStatus::Running;
        }, "CountToThree");

    EXPECT_EQ(action->Tick(0.016f, bb), BTStatus::Running);
    EXPECT_EQ(action->Tick(0.016f, bb), BTStatus::Running);
    EXPECT_EQ(action->Tick(0.016f, bb), BTStatus::Success);
}

TEST_F(BehaviorTreeTest, ConditionNode_True) {
    bb.SetBool("ready", true);
    auto cond = std::make_shared<BTCondition>(
        [](const Blackboard& b) { return b.GetBool("ready"); }, "IsReady");
    EXPECT_EQ(cond->Tick(0.016f, bb), BTStatus::Success);
}

TEST_F(BehaviorTreeTest, ConditionNode_False) {
    bb.SetBool("ready", false);
    auto cond = std::make_shared<BTCondition>(
        [](const Blackboard& b) { return b.GetBool("ready"); }, "IsReady");
    EXPECT_EQ(cond->Tick(0.016f, bb), BTStatus::Failure);
}

TEST_F(BehaviorTreeTest, Sequence_AllSuccess) {
    auto seq = std::make_shared<BTSequence>("Seq");
    seq->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    seq->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    seq->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    EXPECT_EQ(seq->Tick(0.016f, bb), BTStatus::Success);
}

TEST_F(BehaviorTreeTest, Sequence_FirstFails) {
    auto seq = std::make_shared<BTSequence>("Seq");
    seq->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Failure; }));
    seq->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    EXPECT_EQ(seq->Tick(0.016f, bb), BTStatus::Failure);
}

TEST_F(BehaviorTreeTest, Sequence_Running) {
    int call_count = 0;
    auto seq = std::make_shared<BTSequence>("Seq");
    seq->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    seq->AddChild(std::make_shared<BTAction>([&call_count](float, Blackboard&) {
        return (++call_count >= 2) ? BTStatus::Success : BTStatus::Running;
    }));
    EXPECT_EQ(seq->Tick(0.016f, bb), BTStatus::Running);
    EXPECT_EQ(seq->Tick(0.016f, bb), BTStatus::Success);
}

TEST_F(BehaviorTreeTest, Selector_FirstSuccess) {
    auto sel = std::make_shared<BTSelector>("Sel");
    sel->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    sel->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Failure; }));
    EXPECT_EQ(sel->Tick(0.016f, bb), BTStatus::Success);
}

TEST_F(BehaviorTreeTest, Selector_AllFail) {
    auto sel = std::make_shared<BTSelector>("Sel");
    sel->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Failure; }));
    sel->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Failure; }));
    EXPECT_EQ(sel->Tick(0.016f, bb), BTStatus::Failure);
}

TEST_F(BehaviorTreeTest, Selector_SecondSuccess) {
    auto sel = std::make_shared<BTSelector>("Sel");
    sel->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Failure; }));
    sel->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    EXPECT_EQ(sel->Tick(0.016f, bb), BTStatus::Success);
}

TEST_F(BehaviorTreeTest, Parallel_RequireAll_AllSuccess) {
    auto par = std::make_shared<BTParallel>(ParallelPolicy::RequireAll, "Par");
    par->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    par->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    EXPECT_EQ(par->Tick(0.016f, bb), BTStatus::Success);
}

TEST_F(BehaviorTreeTest, Parallel_RequireAll_OneFails) {
    auto par = std::make_shared<BTParallel>(ParallelPolicy::RequireAll, "Par");
    par->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    par->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Failure; }));
    EXPECT_EQ(par->Tick(0.016f, bb), BTStatus::Failure);
}

TEST_F(BehaviorTreeTest, Parallel_RequireOne_OneSuccess) {
    auto par = std::make_shared<BTParallel>(ParallelPolicy::RequireOne, "Par");
    par->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Failure; }));
    par->AddChild(std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    EXPECT_EQ(par->Tick(0.016f, bb), BTStatus::Success);
}

TEST_F(BehaviorTreeTest, Inverter_InvertsSuccess) {
    auto child = std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; });
    auto inv = std::make_shared<BTInverter>(child, "Inv");
    EXPECT_EQ(inv->Tick(0.016f, bb), BTStatus::Failure);
}

TEST_F(BehaviorTreeTest, Inverter_InvertsFailure) {
    auto child = std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Failure; });
    auto inv = std::make_shared<BTInverter>(child, "Inv");
    EXPECT_EQ(inv->Tick(0.016f, bb), BTStatus::Success);
}

TEST_F(BehaviorTreeTest, Inverter_PassesRunning) {
    auto child = std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Running; });
    auto inv = std::make_shared<BTInverter>(child, "Inv");
    EXPECT_EQ(inv->Tick(0.016f, bb), BTStatus::Running);
}

TEST_F(BehaviorTreeTest, Succeeder_AlwaysSuccess) {
    auto child = std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Failure; });
    auto suc = std::make_shared<BTSucceeder>(child, "Suc");
    EXPECT_EQ(suc->Tick(0.016f, bb), BTStatus::Success);
}

TEST_F(BehaviorTreeTest, Repeater_RepeatsTwice) {
    int count = 0;
    auto child = std::make_shared<BTAction>([&count](float, Blackboard&) {
        ++count;
        return BTStatus::Success;
    });
    auto rep = std::make_shared<BTRepeater>(child, 2, "Rep2");
    // First tick: child succeeds, count=1, repeater returns Running (count<2 after reset)
    EXPECT_EQ(rep->Tick(0.016f, bb), BTStatus::Running);
    // Second tick: child succeeds, count=2, repeater returns Success
    EXPECT_EQ(rep->Tick(0.016f, bb), BTStatus::Success);
    EXPECT_EQ(count, 2);
}

TEST_F(BehaviorTreeTest, BehaviorTree_FullTreeExecution) {
    BehaviorTree tree;
    auto& bb_ref = tree.GetBlackboard();
    bb_ref.SetBool("has_target", true);

    auto root = std::make_shared<BTSequence>("Root");
    root->AddChild(std::make_shared<BTCondition>(
        [](const Blackboard& b) { return b.GetBool("has_target"); }, "CheckTarget"));
    root->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard& b) {
            b.SetBool("attacked", true);
            return BTStatus::Success;
        }, "Attack"));
    tree.SetRoot(root);

    EXPECT_EQ(tree.Tick(0.016f), BTStatus::Success);
    EXPECT_TRUE(bb_ref.GetBool("attacked"));
}

TEST_F(BehaviorTreeTest, BehaviorTree_Reset) {
    BehaviorTree tree;
    int count = 0;
    auto action = std::make_shared<BTAction>([&count](float, Blackboard&) {
        return (++count >= 2) ? BTStatus::Success : BTStatus::Running;
    });
    tree.SetRoot(action);
    EXPECT_EQ(tree.Tick(0.016f), BTStatus::Running);
    tree.Reset();
    count = 0;
    EXPECT_EQ(tree.Tick(0.016f), BTStatus::Running);
}
