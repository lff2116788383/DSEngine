/**
 * @file behavior_tree_deep_test.cpp
 * @brief P6: 行为树深度测试 — 节点组合、装饰器、黑板交互、多帧 Running
 */

#include <gtest/gtest.h>
#include "engine/ai/behavior_tree.h"
#include "engine/ai/blackboard.h"
#include <string>
#include <vector>

using namespace dse::ai;

// ═══════════════════════════════════════════════════════════
// Blackboard 深度
// ═══════════════════════════════════════════════════════════

class BlackboardDeepTest : public ::testing::Test {
protected:
    Blackboard bb;
};

TEST_F(BlackboardDeepTest, AllTypes) {
    bb.SetBool("alive", true);
    bb.SetInt("hp", 100);
    bb.SetFloat("speed", 5.5f);
    bb.SetString("name", "NPC");
    bb.SetVec3("pos", glm::vec3(1, 2, 3));

    EXPECT_TRUE(bb.GetBool("alive"));
    EXPECT_EQ(bb.GetInt("hp"), 100);
    EXPECT_FLOAT_EQ(bb.GetFloat("speed"), 5.5f);
    EXPECT_EQ(bb.GetString("name"), "NPC");
    auto pos = bb.GetVec3("pos");
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
    EXPECT_FLOAT_EQ(pos.y, 2.0f);
    EXPECT_FLOAT_EQ(pos.z, 3.0f);
}

TEST_F(BlackboardDeepTest, DefaultValues) {
    EXPECT_FALSE(bb.GetBool("missing"));
    EXPECT_EQ(bb.GetInt("missing"), 0);
    EXPECT_FLOAT_EQ(bb.GetFloat("missing"), 0.0f);
    EXPECT_EQ(bb.GetString("missing"), "");
}

TEST_F(BlackboardDeepTest, HasAndErase) {
    bb.SetInt("key", 42);
    EXPECT_TRUE(bb.Has("key"));
    bb.Erase("key");
    EXPECT_FALSE(bb.Has("key"));
}

TEST_F(BlackboardDeepTest, Clear) {
    bb.SetBool("a", true);
    bb.SetInt("b", 1);
    bb.SetFloat("c", 2.0f);
    EXPECT_EQ(bb.Size(), 3u);
    bb.Clear();
    EXPECT_EQ(bb.Size(), 0u);
}

TEST_F(BlackboardDeepTest, OverwriteValue) {
    bb.SetInt("x", 1);
    bb.SetInt("x", 999);
    EXPECT_EQ(bb.GetInt("x"), 999);
}

// ═══════════════════════════════════════════════════════════
// Action / Condition 叶节点
// ═══════════════════════════════════════════════════════════

TEST(BTLeafDeepTest, ActionReturnsStatus) {
    auto action = std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }, "AlwaysSuccess");
    Blackboard bb;
    EXPECT_EQ(action->Tick(0.016f, bb), BTStatus::Success);
}

TEST(BTLeafDeepTest, ActionRunningMultiFrame) {
    int frame = 0;
    auto action = std::make_shared<BTAction>(
        [&frame](float, Blackboard&) {
            return (++frame >= 3) ? BTStatus::Success : BTStatus::Running;
        }, "WaitFrames");
    Blackboard bb;

    EXPECT_EQ(action->Tick(0.016f, bb), BTStatus::Running);
    EXPECT_EQ(action->Tick(0.016f, bb), BTStatus::Running);
    EXPECT_EQ(action->Tick(0.016f, bb), BTStatus::Success);
}

TEST(BTLeafDeepTest, ConditionTrue) {
    auto cond = std::make_shared<BTCondition>(
        [](const Blackboard& bb) { return bb.GetBool("ready"); }, "IsReady");
    Blackboard bb;
    bb.SetBool("ready", true);
    EXPECT_EQ(cond->Tick(0.0f, bb), BTStatus::Success);
}

TEST(BTLeafDeepTest, ConditionFalse) {
    auto cond = std::make_shared<BTCondition>(
        [](const Blackboard& bb) { return bb.GetBool("ready"); }, "IsReady");
    Blackboard bb;
    EXPECT_EQ(cond->Tick(0.0f, bb), BTStatus::Failure);
}

TEST(BTLeafDeepTest, ActionModifiesBlackboard) {
    auto action = std::make_shared<BTAction>(
        [](float, Blackboard& bb) {
            bb.SetInt("count", bb.GetInt("count") + 1);
            return BTStatus::Success;
        }, "Increment");
    Blackboard bb;
    action->Tick(0.016f, bb);
    action->Tick(0.016f, bb);
    action->Tick(0.016f, bb);
    EXPECT_EQ(bb.GetInt("count"), 3);
}

// ═══════════════════════════════════════════════════════════
// Sequence 组合
// ═══════════════════════════════════════════════════════════

TEST(BTSequenceDeepTest, AllSucceed) {
    auto seq = std::make_shared<BTSequence>("Seq");
    seq->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }));
    seq->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }));
    seq->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }));

    Blackboard bb;
    EXPECT_EQ(seq->Tick(0.016f, bb), BTStatus::Success);
}

TEST(BTSequenceDeepTest, FailsOnFirstFailure) {
    std::vector<int> executed;
    auto seq = std::make_shared<BTSequence>("Seq");
    seq->AddChild(std::make_shared<BTAction>(
        [&](float, Blackboard&) { executed.push_back(0); return BTStatus::Success; }));
    seq->AddChild(std::make_shared<BTAction>(
        [&](float, Blackboard&) { executed.push_back(1); return BTStatus::Failure; }));
    seq->AddChild(std::make_shared<BTAction>(
        [&](float, Blackboard&) { executed.push_back(2); return BTStatus::Success; }));

    Blackboard bb;
    EXPECT_EQ(seq->Tick(0.016f, bb), BTStatus::Failure);
    EXPECT_EQ(executed.size(), 2u);
}

TEST(BTSequenceDeepTest, RunningPausesAtChild) {
    int call_count = 0;
    auto seq = std::make_shared<BTSequence>("Seq");
    seq->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }));
    seq->AddChild(std::make_shared<BTAction>(
        [&call_count](float, Blackboard&) {
            return (++call_count >= 2) ? BTStatus::Success : BTStatus::Running;
        }));
    seq->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }));

    Blackboard bb;
    EXPECT_EQ(seq->Tick(0.016f, bb), BTStatus::Running);
    EXPECT_EQ(seq->Tick(0.016f, bb), BTStatus::Success);
}

// ═══════════════════════════════════════════════════════════
// Selector 组合
// ═══════════════════════════════════════════════════════════

TEST(BTSelectorDeepTest, SucceedsOnFirst) {
    auto sel = std::make_shared<BTSelector>("Sel");
    sel->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }));
    sel->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Failure; }));

    Blackboard bb;
    EXPECT_EQ(sel->Tick(0.016f, bb), BTStatus::Success);
}

TEST(BTSelectorDeepTest, FallsThrough) {
    auto sel = std::make_shared<BTSelector>("Sel");
    sel->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Failure; }));
    sel->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Failure; }));
    sel->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }));

    Blackboard bb;
    EXPECT_EQ(sel->Tick(0.016f, bb), BTStatus::Success);
}

TEST(BTSelectorDeepTest, AllFail) {
    auto sel = std::make_shared<BTSelector>("Sel");
    sel->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Failure; }));
    sel->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Failure; }));

    Blackboard bb;
    EXPECT_EQ(sel->Tick(0.016f, bb), BTStatus::Failure);
}

// ═══════════════════════════════════════════════════════════
// Parallel 组合
// ═══════════════════════════════════════════════════════════

TEST(BTParallelDeepTest, RequireAllSuccess) {
    auto par = std::make_shared<BTParallel>(ParallelPolicy::RequireAll);
    par->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }));
    par->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }));

    Blackboard bb;
    EXPECT_EQ(par->Tick(0.016f, bb), BTStatus::Success);
}

TEST(BTParallelDeepTest, RequireAllOneFails) {
    auto par = std::make_shared<BTParallel>(ParallelPolicy::RequireAll);
    par->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }));
    par->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Failure; }));

    Blackboard bb;
    EXPECT_EQ(par->Tick(0.016f, bb), BTStatus::Failure);
}

TEST(BTParallelDeepTest, RequireOneAnySuccess) {
    auto par = std::make_shared<BTParallel>(ParallelPolicy::RequireOne);
    par->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Failure; }));
    par->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Success; }));

    Blackboard bb;
    EXPECT_EQ(par->Tick(0.016f, bb), BTStatus::Success);
}

// ═══════════════════════════════════════════════════════════
// 装饰器
// ═══════════════════════════════════════════════════════════

TEST(BTDecoratorDeepTest, InverterSuccess) {
    auto inv = std::make_shared<BTInverter>(
        std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Success; }));
    Blackboard bb;
    EXPECT_EQ(inv->Tick(0.016f, bb), BTStatus::Failure);
}

TEST(BTDecoratorDeepTest, InverterFailure) {
    auto inv = std::make_shared<BTInverter>(
        std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Failure; }));
    Blackboard bb;
    EXPECT_EQ(inv->Tick(0.016f, bb), BTStatus::Success);
}

TEST(BTDecoratorDeepTest, InverterRunningPassthrough) {
    auto inv = std::make_shared<BTInverter>(
        std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Running; }));
    Blackboard bb;
    EXPECT_EQ(inv->Tick(0.016f, bb), BTStatus::Running);
}

TEST(BTDecoratorDeepTest, RepeaterFinite) {
    int count = 0;
    auto rep = std::make_shared<BTRepeater>(
        std::make_shared<BTAction>([&](float, Blackboard&) {
            ++count;
            return BTStatus::Success;
        }), 3);
    Blackboard bb;
    // Repeater may tick once per call (Running) or all at once
    BTStatus status = BTStatus::Running;
    for (int i = 0; i < 10 && status == BTStatus::Running; ++i) {
        status = rep->Tick(0.016f, bb);
    }
    EXPECT_EQ(count, 3);
    EXPECT_EQ(status, BTStatus::Success);
}

TEST(BTDecoratorDeepTest, SucceederAlwaysSucceeds) {
    auto suc = std::make_shared<BTSucceeder>(
        std::make_shared<BTAction>([](float, Blackboard&) { return BTStatus::Failure; }));
    Blackboard bb;
    EXPECT_EQ(suc->Tick(0.016f, bb), BTStatus::Success);
}

// ═══════════════════════════════════════════════════════════
// BehaviorTree 容器
// ═══════════════════════════════════════════════════════════

TEST(BehaviorTreeDeepTest, TickWithBlackboard) {
    BehaviorTree tree;
    auto action = std::make_shared<BTAction>(
        [](float, Blackboard& bb) {
            bb.SetInt("ticks", bb.GetInt("ticks") + 1);
            return BTStatus::Success;
        });
    tree.SetRoot(action);

    tree.Tick(0.016f);
    tree.Tick(0.016f);
    EXPECT_EQ(tree.GetBlackboard().GetInt("ticks"), 2);
}

TEST(BehaviorTreeDeepTest, ResetTree) {
    int reset_count = 0;
    auto action = std::make_shared<BTAction>(
        [](float, Blackboard&) { return BTStatus::Running; }, "RunForever");

    BehaviorTree tree;
    tree.SetRoot(action);
    tree.Tick(0.016f);
    tree.Reset();
    // After reset the tree should be tickable again
    EXPECT_EQ(tree.Tick(0.016f), BTStatus::Running);
}

TEST(BehaviorTreeDeepTest, ComplexTree) {
    // Build: Selector -> [Sequence(Condition, Action), FallbackAction]
    auto seq = std::make_shared<BTSequence>();
    seq->AddChild(std::make_shared<BTCondition>(
        [](const Blackboard& bb) { return bb.GetBool("has_target"); }));
    seq->AddChild(std::make_shared<BTAction>(
        [](float, Blackboard& bb) {
            bb.SetString("action", "attack");
            return BTStatus::Success;
        }));

    auto fallback = std::make_shared<BTAction>(
        [](float, Blackboard& bb) {
            bb.SetString("action", "patrol");
            return BTStatus::Success;
        });

    auto sel = std::make_shared<BTSelector>();
    sel->AddChild(seq);
    sel->AddChild(fallback);

    BehaviorTree tree;
    tree.SetRoot(sel);

    // No target -> patrol
    tree.Tick(0.016f);
    EXPECT_EQ(tree.GetBlackboard().GetString("action"), "patrol");

    // Has target -> attack
    tree.GetBlackboard().SetBool("has_target", true);
    tree.Reset();
    tree.Tick(0.016f);
    EXPECT_EQ(tree.GetBlackboard().GetString("action"), "attack");
}
