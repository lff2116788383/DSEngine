/**
 * @file goap_planner_test.cpp
 * @brief GOAP 规划器单元测试
 */

#include <gtest/gtest.h>
#include "engine/ai/goap_planner.h"

using namespace dse::ai;

class GOAPPlannerTest : public ::testing::Test {
protected:
    GOAPPlanner planner;

    void SetUp() override {
        // 经典 FPS AI 动作集
        GOAPAction get_weapon;
        get_weapon.name = "get_weapon";
        get_weapon.cost = 2.0f;
        get_weapon.preconditions = {};
        get_weapon.effects = {{"has_weapon", true}};
        planner.AddAction(get_weapon);

        GOAPAction attack;
        attack.name = "attack";
        attack.cost = 1.0f;
        attack.preconditions = {{"has_weapon", true}, {"in_range", true}};
        attack.effects = {{"enemy_dead", true}};
        planner.AddAction(attack);

        GOAPAction move_to_enemy;
        move_to_enemy.name = "move_to_enemy";
        move_to_enemy.cost = 1.0f;
        move_to_enemy.preconditions = {};
        move_to_enemy.effects = {{"in_range", true}};
        planner.AddAction(move_to_enemy);
    }
};

TEST_F(GOAPPlannerTest, Plan_AlreadySatisfied) {
    GOAPState current = {{"enemy_dead", true}};
    GOAPState goal = {{"enemy_dead", true}};
    auto plan = planner.Plan(current, goal);
    EXPECT_TRUE(plan.valid);
    EXPECT_EQ(plan.actions.size(), 0u);
}

TEST_F(GOAPPlannerTest, Plan_SimpleOneAction) {
    GOAPState current = {{"has_weapon", true}, {"in_range", true}};
    GOAPState goal = {{"enemy_dead", true}};
    auto plan = planner.Plan(current, goal);
    EXPECT_TRUE(plan.valid);
    EXPECT_EQ(plan.actions.size(), 1u);
    EXPECT_EQ(plan.actions[0]->name, "attack");
}

TEST_F(GOAPPlannerTest, Plan_MultiStep) {
    GOAPState current = {};
    GOAPState goal = {{"enemy_dead", true}};
    auto plan = planner.Plan(current, goal);
    EXPECT_TRUE(plan.valid);
    EXPECT_GE(plan.actions.size(), 3u); // get_weapon + move_to_enemy + attack
    // 最后一个动作必须是 attack
    EXPECT_EQ(plan.actions.back()->name, "attack");
}

TEST_F(GOAPPlannerTest, Plan_Impossible) {
    GOAPPlanner empty_planner;
    GOAPState current = {};
    GOAPState goal = {{"impossible_state", true}};
    auto plan = empty_planner.Plan(current, goal);
    EXPECT_FALSE(plan.valid);
}

TEST_F(GOAPPlannerTest, Plan_WithCostOptimization) {
    // 添加一个更便宜的直接击杀动作
    GOAPAction cheap_kill;
    cheap_kill.name = "cheap_kill";
    cheap_kill.cost = 0.5f;
    cheap_kill.preconditions = {{"in_range", true}};
    cheap_kill.effects = {{"enemy_dead", true}};
    planner.AddAction(cheap_kill);

    GOAPState current = {{"in_range", true}};
    GOAPState goal = {{"enemy_dead", true}};
    auto plan = planner.Plan(current, goal);
    EXPECT_TRUE(plan.valid);
    // 应优先选择代价更低的 cheap_kill
    EXPECT_EQ(plan.actions.size(), 1u);
    EXPECT_EQ(plan.actions[0]->name, "cheap_kill");
}

TEST_F(GOAPPlannerTest, AddAndRemoveAction) {
    EXPECT_EQ(planner.ActionCount(), 3u);
    planner.RemoveAction("attack");
    EXPECT_EQ(planner.ActionCount(), 2u);
    planner.ClearActions();
    EXPECT_EQ(planner.ActionCount(), 0u);
}

TEST_F(GOAPPlannerTest, Plan_MaxDepthLimit) {
    GOAPPlanner chain_planner;
    // 创建一条链：需要 15 步的计划，每步消耗前置条件
    for (int i = 0; i < 15; ++i) {
        GOAPAction a;
        a.name = "step_" + std::to_string(i);
        a.cost = 1.0f;
        a.preconditions = {{("s" + std::to_string(i)), true}};
        a.effects = {{("s" + std::to_string(i)), false}, {("s" + std::to_string(i + 1)), true}};
        chain_planner.AddAction(a);
    }
    GOAPState current = {{"s0", true}};
    GOAPState goal = {{"s15", true}};
    // max_depth = 5 应该找不到（需要 15 步）
    auto plan = chain_planner.Plan(current, goal, 5);
    EXPECT_FALSE(plan.valid);
}

TEST_F(GOAPPlannerTest, Plan_ProceduralPrecondition) {
    GOAPPlanner p;
    bool ammo_available = false;
    GOAPAction shoot;
    shoot.name = "shoot";
    shoot.cost = 1.0f;
    shoot.effects = {{"enemy_dead", true}};
    shoot.procedural_precondition = [&ammo_available]() { return ammo_available; };
    p.AddAction(shoot);

    GOAPState current = {};
    GOAPState goal = {{"enemy_dead", true}};

    // Without ammo
    auto plan1 = p.Plan(current, goal);
    EXPECT_FALSE(plan1.valid);

    // With ammo
    ammo_available = true;
    auto plan2 = p.Plan(current, goal);
    EXPECT_TRUE(plan2.valid);
    EXPECT_EQ(plan2.actions[0]->name, "shoot");
}
