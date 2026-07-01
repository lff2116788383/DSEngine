/**
 * @file lua_binding_ai.cpp
 * @brief AI 行为树 + GOAP 规划器 Lua 绑定
 *
 * 全局表 `ai`:
 *   ai.create_tree(name)                → tree_id
 *   ai.destroy_tree(tree_id)
 *   ai.tick_tree(tree_id, dt)           → "success"|"failure"|"running"
 *   ai.reset_tree(tree_id)
 *
 *   -- 黑板
 *   ai.bb_set_bool(tree_id, key, value)
 *   ai.bb_set_int(tree_id, key, value)
 *   ai.bb_set_float(tree_id, key, value)
 *   ai.bb_set_string(tree_id, key, value)
 *   ai.bb_set_vec3(tree_id, key, x, y, z)
 *   ai.bb_get_bool(tree_id, key)        → bool
 *   ai.bb_get_int(tree_id, key)         → int
 *   ai.bb_get_float(tree_id, key)       → float
 *   ai.bb_get_string(tree_id, key)      → string
 *   ai.bb_get_vec3(tree_id, key)        → x,y,z
 *
 *   -- 树构建（栈式 API）
 *   ai.begin_sequence(tree_id, name)
 *   ai.begin_selector(tree_id, name)
 *   ai.begin_parallel(tree_id, policy, name)   policy: "all"|"one"
 *   ai.end_composite(tree_id)
 *   ai.add_condition(tree_id, name, lua_func)   lua_func(bb_table) → bool
 *   ai.add_action(tree_id, name, lua_func)      lua_func(dt, bb_table) → "success"|"failure"|"running"
 *   ai.add_inverter(tree_id, name)               — 下一个节点被包装
 *   ai.add_succeeder(tree_id, name)
 *   ai.add_repeater(tree_id, name, max_repeats)
 *
 *   -- GOAP
 *   ai.goap_create()                    → planner_id
 *   ai.goap_destroy(planner_id)
 *   ai.goap_add_action(planner_id, action_table)
 *   ai.goap_plan(planner_id, current_state, goal) → {action_names} | nil
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ai/behavior_tree.h"
#include "engine/ai/goap_planner.h"

#include <unordered_map>
#include <stack>
#include <memory>

extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

using namespace dse::ai;
using namespace helper;

// ============================================================
// 行为树实例管理
// ============================================================

struct LuaBTInstance {
    BehaviorTree tree;
    std::string name;
    // 构建状态
    std::stack<std::shared_ptr<BTNode>> composite_stack;
    // pending decorator（下一个 leaf/composite 被包装）
    enum class DecoratorType { None, Inverter, Succeeder, Repeater };
    DecoratorType pending_decorator = DecoratorType::None;
    std::string pending_decorator_name;
    int pending_repeater_count = -1;
};

static std::unordered_map<int, std::unique_ptr<LuaBTInstance>> s_bt_instances;
static int s_next_bt_id = 1;

static std::unordered_map<int, std::unique_ptr<GOAPPlanner>> s_goap_instances;
static int s_next_goap_id = 1;

void AddChildToTop(LuaBTInstance& inst, BTNodePtr node) {
    // 如果有 pending decorator，包装之
    if (inst.pending_decorator == LuaBTInstance::DecoratorType::Inverter) {
        node = std::make_shared<BTInverter>(node, inst.pending_decorator_name);
    } else if (inst.pending_decorator == LuaBTInstance::DecoratorType::Succeeder) {
        node = std::make_shared<BTSucceeder>(node, inst.pending_decorator_name);
    } else if (inst.pending_decorator == LuaBTInstance::DecoratorType::Repeater) {
        node = std::make_shared<BTRepeater>(node, inst.pending_repeater_count, inst.pending_decorator_name);
    }
    inst.pending_decorator = LuaBTInstance::DecoratorType::None;

    if (inst.composite_stack.empty()) {
        inst.tree.SetRoot(node);
    } else {
        auto& top = inst.composite_stack.top();
        if (auto* seq = dynamic_cast<BTSequence*>(top.get())) seq->AddChild(node);
        else if (auto* sel = dynamic_cast<BTSelector*>(top.get())) sel->AddChild(node);
        else if (auto* par = dynamic_cast<BTParallel*>(top.get())) par->AddChild(node);
    }
}

// ============================================================
// Lua Callback BT Nodes
// ============================================================

/// Lua 回调 Condition 节点
class LuaConditionNode : public BTNode {
public:
    LuaConditionNode(lua_State* L, int func_ref, const std::string& name)
        : BTNode(name), L_(L), func_ref_(func_ref) {}

    ~LuaConditionNode() override {
        if (L_ && func_ref_ != LUA_NOREF) luaL_unref(L_, LUA_REGISTRYINDEX, func_ref_);
    }

    BTStatus Tick(float /*dt*/, Blackboard& bb) override {
        if (!L_ || func_ref_ == LUA_NOREF) return BTStatus::Failure;
        lua_rawgeti(L_, LUA_REGISTRYINDEX, func_ref_);
        // 传入黑板简化表（仅传空表，由用户通过 bb_get 访问）
        lua_newtable(L_);
        if (lua_pcall(L_, 1, 1, 0) != LUA_OK) {
            lua_pop(L_, 1);
            return BTStatus::Failure;
        }
        bool result = lua_toboolean(L_, -1) != 0;
        lua_pop(L_, 1);
        return result ? BTStatus::Success : BTStatus::Failure;
    }

private:
    lua_State* L_;
    int func_ref_;
};

/// Lua 回调 Action 节点
class LuaActionNode : public BTNode {
public:
    LuaActionNode(lua_State* L, int func_ref, const std::string& name)
        : BTNode(name), L_(L), func_ref_(func_ref) {}

    ~LuaActionNode() override {
        if (L_ && func_ref_ != LUA_NOREF) luaL_unref(L_, LUA_REGISTRYINDEX, func_ref_);
    }

    BTStatus Tick(float dt, Blackboard& /*bb*/) override {
        if (!L_ || func_ref_ == LUA_NOREF) return BTStatus::Failure;
        lua_rawgeti(L_, LUA_REGISTRYINDEX, func_ref_);
        lua_pushnumber(L_, dt);
        lua_newtable(L_); // bb table placeholder
        if (lua_pcall(L_, 2, 1, 0) != LUA_OK) {
            lua_pop(L_, 1);
            return BTStatus::Failure;
        }
        const char* result_str = lua_tostring(L_, -1);
        BTStatus status = BTStatus::Failure;
        if (result_str) {
            if (result_str[0] == 's') status = BTStatus::Success;
            else if (result_str[0] == 'r') status = BTStatus::Running;
        }
        lua_pop(L_, 1);
        return status;
    }

private:
    lua_State* L_;
    int func_ref_;
};

// ============================================================
// Lua C Functions — Behavior Tree
// ============================================================

int L_AICreateTree(lua_State* L) {
    const char* name = luaL_optstring(L, 1, "unnamed");
    int id = s_next_bt_id++;
    s_bt_instances[id] = std::make_unique<LuaBTInstance>();
    s_bt_instances[id]->name = name;
    lua_pushinteger(L, id);
    return 1;
}

int L_AIDestroyTree(lua_State* L) {
    int id = CheckInt(L, 1);
    s_bt_instances.erase(id);
    return 0;
}

int L_AITickTree(lua_State* L) {
    int id = CheckInt(L, 1);
    float dt = CheckFloat(L, 2);
    auto it = s_bt_instances.find(id);
    if (it == s_bt_instances.end()) { lua_pushstring(L, "failure"); return 1; }
    BTStatus status = it->second->tree.Tick(dt);
    switch (status) {
        case BTStatus::Success: lua_pushstring(L, "success"); break;
        case BTStatus::Running: lua_pushstring(L, "running"); break;
        default: lua_pushstring(L, "failure"); break;
    }
    return 1;
}

int L_AIResetTree(lua_State* L) {
    int id = CheckInt(L, 1);
    auto it = s_bt_instances.find(id);
    if (it != s_bt_instances.end()) it->second->tree.Reset();
    return 0;
}

// --- Blackboard ---

#define GET_BT_INST(id) \
    auto it = s_bt_instances.find(id); \
    if (it == s_bt_instances.end()) return 0; \
    auto& inst = *it->second

int L_AIBBSetBool(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    inst.tree.GetBlackboard().SetBool(luaL_checkstring(L, 2), lua_toboolean(L, 3) != 0);
    return 0;
}

int L_AIBBSetInt(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    inst.tree.GetBlackboard().SetInt(luaL_checkstring(L, 2), CheckInt(L, 3));
    return 0;
}

int L_AIBBSetFloat(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    inst.tree.GetBlackboard().SetFloat(luaL_checkstring(L, 2), CheckFloat(L, 3));
    return 0;
}

int L_AIBBSetString(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    inst.tree.GetBlackboard().SetString(luaL_checkstring(L, 2), luaL_checkstring(L, 3));
    return 0;
}

int L_AIBBSetVec3(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    inst.tree.GetBlackboard().SetVec3(luaL_checkstring(L, 2),
        glm::vec3(CheckFloat(L, 3), CheckFloat(L, 4), CheckFloat(L, 5)));
    return 0;
}

int L_AIBBGetBool(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    lua_pushboolean(L, inst.tree.GetBlackboard().GetBool(luaL_checkstring(L, 2)));
    return 1;
}

int L_AIBBGetInt(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    lua_pushinteger(L, inst.tree.GetBlackboard().GetInt(luaL_checkstring(L, 2)));
    return 1;
}

int L_AIBBGetFloat(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    lua_pushnumber(L, inst.tree.GetBlackboard().GetFloat(luaL_checkstring(L, 2)));
    return 1;
}

int L_AIBBGetString(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    lua_pushstring(L, inst.tree.GetBlackboard().GetString(luaL_checkstring(L, 2)).c_str());
    return 1;
}

int L_AIBBGetVec3(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    auto v = inst.tree.GetBlackboard().GetVec3(luaL_checkstring(L, 2));
    lua_pushnumber(L, v.x); lua_pushnumber(L, v.y); lua_pushnumber(L, v.z);
    return 3;
}

// --- Tree Construction ---

int L_AIBeginSequence(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    const char* name = luaL_optstring(L, 2, "Sequence");
    auto node = std::make_shared<BTSequence>(name);
    auto node_ptr = std::static_pointer_cast<BTNode>(node);
    AddChildToTop(inst, node_ptr);
    inst.composite_stack.push(node_ptr);
    return 0;
}

int L_AIBeginSelector(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    const char* name = luaL_optstring(L, 2, "Selector");
    auto node = std::make_shared<BTSelector>(name);
    auto node_ptr = std::static_pointer_cast<BTNode>(node);
    AddChildToTop(inst, node_ptr);
    inst.composite_stack.push(node_ptr);
    return 0;
}

int L_AIBeginParallel(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    const char* policy_str = luaL_optstring(L, 2, "all");
    const char* name = luaL_optstring(L, 3, "Parallel");
    ParallelPolicy policy = (policy_str[0] == 'o') ? ParallelPolicy::RequireOne : ParallelPolicy::RequireAll;
    auto node = std::make_shared<BTParallel>(policy, name);
    auto node_ptr = std::static_pointer_cast<BTNode>(node);
    AddChildToTop(inst, node_ptr);
    inst.composite_stack.push(node_ptr);
    return 0;
}

int L_AIEndComposite(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    if (!inst.composite_stack.empty()) inst.composite_stack.pop();
    return 0;
}

int L_AIAddCondition(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    const char* name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    auto node = std::make_shared<LuaConditionNode>(L, ref, name);
    AddChildToTop(inst, node);
    return 0;
}

int L_AIAddAction(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    const char* name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    auto node = std::make_shared<LuaActionNode>(L, ref, name);
    AddChildToTop(inst, node);
    return 0;
}

int L_AIAddInverter(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    inst.pending_decorator = LuaBTInstance::DecoratorType::Inverter;
    inst.pending_decorator_name = luaL_optstring(L, 2, "Inverter");
    return 0;
}

int L_AIAddSucceeder(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    inst.pending_decorator = LuaBTInstance::DecoratorType::Succeeder;
    inst.pending_decorator_name = luaL_optstring(L, 2, "Succeeder");
    return 0;
}

int L_AIAddRepeater(lua_State* L) {
    int id = CheckInt(L, 1); GET_BT_INST(id);
    inst.pending_decorator = LuaBTInstance::DecoratorType::Repeater;
    inst.pending_decorator_name = luaL_optstring(L, 2, "Repeater");
    inst.pending_repeater_count = OptInt(L, 3, -1);
    return 0;
}

// ============================================================
// Lua C Functions — GOAP
// ============================================================

int L_GOAPCreate(lua_State* L) {
    int id = s_next_goap_id++;
    s_goap_instances[id] = std::make_unique<GOAPPlanner>();
    lua_pushinteger(L, id);
    return 1;
}

int L_GOAPDestroy(lua_State* L) {
    int id = CheckInt(L, 1);
    s_goap_instances.erase(id);
    return 0;
}

// ai.goap_add_action(planner_id, { name="...", cost=1, preconditions={k=bool}, effects={k=bool} })
int L_GOAPAddAction(lua_State* L) {
    int id = CheckInt(L, 1);
    auto it = s_goap_instances.find(id);
    if (it == s_goap_instances.end()) return 0;

    luaL_checktype(L, 2, LUA_TTABLE);
    GOAPAction action;

    lua_getfield(L, 2, "name");
    if (lua_isstring(L, -1)) action.name = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "cost");
    if (lua_isnumber(L, -1)) action.cost = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 2, "preconditions");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_isstring(L, -2)) {
                action.preconditions[lua_tostring(L, -2)] = lua_toboolean(L, -1) != 0;
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "effects");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_isstring(L, -2)) {
                action.effects[lua_tostring(L, -2)] = lua_toboolean(L, -1) != 0;
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    it->second->AddAction(action);
    return 0;
}

// ai.goap_plan(planner_id, current_state_table, goal_table) → {action_names} | nil
int L_GOAPPlan(lua_State* L) {
    int id = CheckInt(L, 1);
    auto it = s_goap_instances.find(id);
    if (it == s_goap_instances.end()) { lua_pushnil(L); return 1; }

    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TTABLE);

    GOAPState current_state;
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) {
        if (lua_isstring(L, -2)) {
            current_state[lua_tostring(L, -2)] = lua_toboolean(L, -1) != 0;
        }
        lua_pop(L, 1);
    }

    GOAPState goal;
    lua_pushnil(L);
    while (lua_next(L, 3) != 0) {
        if (lua_isstring(L, -2)) {
            goal[lua_tostring(L, -2)] = lua_toboolean(L, -1) != 0;
        }
        lua_pop(L, 1);
    }

    GOAPPlan plan = it->second->Plan(current_state, goal);
    if (!plan.valid) { lua_pushnil(L); return 1; }

    lua_createtable(L, static_cast<int>(plan.actions.size()), 0);
    for (int i = 0; i < static_cast<int>(plan.actions.size()); ++i) {
        lua_pushstring(L, plan.actions[i]->name.c_str());
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

} // namespace

void ShutdownAIBindings() {
    s_bt_instances.clear();
    s_next_bt_id = 1;
    s_goap_instances.clear();
    s_next_goap_id = 1;
}

void RegisterAIBindings(lua_State* L) {
    lua_newtable(L);

    // Tree management
    RegisterFn(L, "create_tree", L_AICreateTree);
    RegisterFn(L, "destroy_tree", L_AIDestroyTree);
    RegisterFn(L, "tick_tree", L_AITickTree);
    RegisterFn(L, "reset_tree", L_AIResetTree);

    // Blackboard
    RegisterFn(L, "bb_set_bool", L_AIBBSetBool);
    RegisterFn(L, "bb_set_int", L_AIBBSetInt);
    RegisterFn(L, "bb_set_float", L_AIBBSetFloat);
    RegisterFn(L, "bb_set_string", L_AIBBSetString);
    RegisterFn(L, "bb_set_vec3", L_AIBBSetVec3);
    RegisterFn(L, "bb_get_bool", L_AIBBGetBool);
    RegisterFn(L, "bb_get_int", L_AIBBGetInt);
    RegisterFn(L, "bb_get_float", L_AIBBGetFloat);
    RegisterFn(L, "bb_get_string", L_AIBBGetString);
    RegisterFn(L, "bb_get_vec3", L_AIBBGetVec3);

    // Tree construction
    RegisterFn(L, "begin_sequence", L_AIBeginSequence);
    RegisterFn(L, "begin_selector", L_AIBeginSelector);
    RegisterFn(L, "begin_parallel", L_AIBeginParallel);
    RegisterFn(L, "end_composite", L_AIEndComposite);
    RegisterFn(L, "add_condition", L_AIAddCondition);
    RegisterFn(L, "add_action", L_AIAddAction);
    RegisterFn(L, "add_inverter", L_AIAddInverter);
    RegisterFn(L, "add_succeeder", L_AIAddSucceeder);
    RegisterFn(L, "add_repeater", L_AIAddRepeater);

    // GOAP
    RegisterFn(L, "goap_create", L_GOAPCreate);
    RegisterFn(L, "goap_destroy", L_GOAPDestroy);
    RegisterFn(L, "goap_add_action", L_GOAPAddAction);
    RegisterFn(L, "goap_plan", L_GOAPPlan);

    lua_setglobal(L, "ai");
}

} // namespace dse::runtime::lua_binding
