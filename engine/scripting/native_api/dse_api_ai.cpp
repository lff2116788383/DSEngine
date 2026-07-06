/**
 * @file dse_api_ai.cpp
 * @brief DSEngine Native C ABI — AI 行为树 + GOAP 规划器（手写）
 *
 * 树/规划器实例由本层管理（int 句柄）。Condition/Action 叶节点通过
 * C 函数指针 + user_data 回调，脚本层（Lua/C#）自行持有闭包上下文，
 * 节点销毁时调用 destroy 回调释放 user_data。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/ai/behavior_tree.h"
#include "engine/ai/goap_planner.h"

#include <cstring>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>

using namespace dse::ai;

namespace {

// 拷贝 std::string 到 out 缓冲（null 结尾，按 cap 截断），返回写入长度。
int CopyStr(const std::string& s, char* out, int cap) {
    if (!out || cap <= 0) return 0;
    int n = static_cast<int>(s.size());
    if (n > cap - 1) n = cap - 1;
    std::memcpy(out, s.data(), static_cast<size_t>(n));
    out[n] = '\0';
    return n;
}

// ============================================================
// 回调叶节点
// ============================================================

class CallbackConditionNode : public BTNode {
public:
    CallbackConditionNode(dse_ai_condition_fn fn, void* user_data, dse_ai_destroy_fn destroy,
                          const std::string& name)
        : BTNode(name), fn_(fn), user_data_(user_data), destroy_(destroy) {}

    ~CallbackConditionNode() override {
        if (destroy_) destroy_(user_data_);
    }

    BTStatus Tick(float /*dt*/, Blackboard& /*bb*/) override {
        if (!fn_) return BTStatus::Failure;
        return fn_(user_data_) ? BTStatus::Success : BTStatus::Failure;
    }

private:
    dse_ai_condition_fn fn_;
    void* user_data_;
    dse_ai_destroy_fn destroy_;
};

class CallbackActionNode : public BTNode {
public:
    CallbackActionNode(dse_ai_action_fn fn, void* user_data, dse_ai_destroy_fn destroy,
                       const std::string& name)
        : BTNode(name), fn_(fn), user_data_(user_data), destroy_(destroy) {}

    ~CallbackActionNode() override {
        if (destroy_) destroy_(user_data_);
    }

    BTStatus Tick(float dt, Blackboard& /*bb*/) override {
        if (!fn_) return BTStatus::Failure;
        switch (fn_(dt, user_data_)) {
            case 1: return BTStatus::Success;
            case 2: return BTStatus::Running;
            default: return BTStatus::Failure;
        }
    }

private:
    dse_ai_action_fn fn_;
    void* user_data_;
    dse_ai_destroy_fn destroy_;
};

// ============================================================
// 实例管理
// ============================================================

struct BTInstance {
    BehaviorTree tree;
    std::string name;
    std::stack<std::shared_ptr<BTNode>> composite_stack;
    enum class DecoratorType { None, Inverter, Succeeder, Repeater };
    DecoratorType pending_decorator = DecoratorType::None;
    std::string pending_decorator_name;
    int pending_repeater_count = -1;
};

struct GOAPInstance {
    GOAPPlanner planner;
    GOAPAction pending_action;
    GOAPState current_state;
    GOAPState goal;
};

std::unordered_map<int, std::unique_ptr<BTInstance>> s_bt_instances;
int s_next_bt_id = 1;

std::unordered_map<int, std::unique_ptr<GOAPInstance>> s_goap_instances;
int s_next_goap_id = 1;

BTInstance* GetBT(int id) {
    auto it = s_bt_instances.find(id);
    return it != s_bt_instances.end() ? it->second.get() : nullptr;
}

GOAPInstance* GetGOAP(int id) {
    auto it = s_goap_instances.find(id);
    return it != s_goap_instances.end() ? it->second.get() : nullptr;
}

void AddChildToTop(BTInstance& inst, BTNodePtr node) {
    if (inst.pending_decorator == BTInstance::DecoratorType::Inverter) {
        node = std::make_shared<BTInverter>(node, inst.pending_decorator_name);
    } else if (inst.pending_decorator == BTInstance::DecoratorType::Succeeder) {
        node = std::make_shared<BTSucceeder>(node, inst.pending_decorator_name);
    } else if (inst.pending_decorator == BTInstance::DecoratorType::Repeater) {
        node = std::make_shared<BTRepeater>(node, inst.pending_repeater_count,
                                            inst.pending_decorator_name);
    }
    inst.pending_decorator = BTInstance::DecoratorType::None;

    if (inst.composite_stack.empty()) {
        inst.tree.SetRoot(node);
    } else {
        auto& top = inst.composite_stack.top();
        if (auto* seq = dynamic_cast<BTSequence*>(top.get())) seq->AddChild(node);
        else if (auto* sel = dynamic_cast<BTSelector*>(top.get())) sel->AddChild(node);
        else if (auto* par = dynamic_cast<BTParallel*>(top.get())) par->AddChild(node);
    }
}

}  // namespace

// ============================================================
// 行为树
// ============================================================

extern "C" int dse_ai_tree_create(const char* name) {
    int id = s_next_bt_id++;
    s_bt_instances[id] = std::make_unique<BTInstance>();
    s_bt_instances[id]->name = name ? name : "unnamed";
    return id;
}

extern "C" void dse_ai_tree_destroy(int tree_id) {
    s_bt_instances.erase(tree_id);
}

extern "C" int dse_ai_tree_tick(int tree_id, float dt) {
    auto* inst = GetBT(tree_id);
    if (!inst) return 0;
    switch (inst->tree.Tick(dt)) {
        case BTStatus::Success: return 1;
        case BTStatus::Running: return 2;
        default: return 0;
    }
}

extern "C" void dse_ai_tree_reset(int tree_id) {
    if (auto* inst = GetBT(tree_id)) inst->tree.Reset();
}

extern "C" void dse_ai_shutdown(void) {
    s_bt_instances.clear();
    s_next_bt_id = 1;
    s_goap_instances.clear();
    s_next_goap_id = 1;
}

// --- 黑板 ---

extern "C" void dse_ai_bb_set_bool(int tree_id, const char* key, int v) {
    auto* inst = GetBT(tree_id);
    if (inst && key) inst->tree.GetBlackboard().SetBool(key, v != 0);
}

extern "C" void dse_ai_bb_set_int(int tree_id, const char* key, int v) {
    auto* inst = GetBT(tree_id);
    if (inst && key) inst->tree.GetBlackboard().SetInt(key, v);
}

extern "C" void dse_ai_bb_set_float(int tree_id, const char* key, float v) {
    auto* inst = GetBT(tree_id);
    if (inst && key) inst->tree.GetBlackboard().SetFloat(key, v);
}

extern "C" void dse_ai_bb_set_string(int tree_id, const char* key, const char* v) {
    auto* inst = GetBT(tree_id);
    if (inst && key && v) inst->tree.GetBlackboard().SetString(key, v);
}

extern "C" void dse_ai_bb_set_vec3(int tree_id, const char* key, float x, float y, float z) {
    auto* inst = GetBT(tree_id);
    if (inst && key) inst->tree.GetBlackboard().SetVec3(key, glm::vec3(x, y, z));
}

extern "C" int dse_ai_bb_get_bool(int tree_id, const char* key) {
    auto* inst = GetBT(tree_id);
    return (inst && key && inst->tree.GetBlackboard().GetBool(key)) ? 1 : 0;
}

extern "C" int dse_ai_bb_get_int(int tree_id, const char* key) {
    auto* inst = GetBT(tree_id);
    return (inst && key) ? inst->tree.GetBlackboard().GetInt(key) : 0;
}

extern "C" float dse_ai_bb_get_float(int tree_id, const char* key) {
    auto* inst = GetBT(tree_id);
    return (inst && key) ? inst->tree.GetBlackboard().GetFloat(key) : 0.0f;
}

extern "C" int dse_ai_bb_get_string(int tree_id, const char* key, char* out, int cap) {
    auto* inst = GetBT(tree_id);
    if (!inst || !key) return CopyStr(std::string(), out, cap);
    return CopyStr(inst->tree.GetBlackboard().GetString(key), out, cap);
}

extern "C" int dse_ai_bb_get_vec3(int tree_id, const char* key, float* out_xyz) {
    auto* inst = GetBT(tree_id);
    if (!inst || !key) return 0;
    const glm::vec3 v = inst->tree.GetBlackboard().GetVec3(key);
    if (out_xyz) {
        out_xyz[0] = v.x;
        out_xyz[1] = v.y;
        out_xyz[2] = v.z;
    }
    return 1;
}

// --- 树构建 ---

extern "C" void dse_ai_begin_sequence(int tree_id, const char* name) {
    auto* inst = GetBT(tree_id);
    if (!inst) return;
    auto node = std::static_pointer_cast<BTNode>(
        std::make_shared<BTSequence>(name ? name : "Sequence"));
    AddChildToTop(*inst, node);
    inst->composite_stack.push(node);
}

extern "C" void dse_ai_begin_selector(int tree_id, const char* name) {
    auto* inst = GetBT(tree_id);
    if (!inst) return;
    auto node = std::static_pointer_cast<BTNode>(
        std::make_shared<BTSelector>(name ? name : "Selector"));
    AddChildToTop(*inst, node);
    inst->composite_stack.push(node);
}

extern "C" void dse_ai_begin_parallel(int tree_id, int require_one, const char* name) {
    auto* inst = GetBT(tree_id);
    if (!inst) return;
    const ParallelPolicy policy = require_one ? ParallelPolicy::RequireOne
                                              : ParallelPolicy::RequireAll;
    auto node = std::static_pointer_cast<BTNode>(
        std::make_shared<BTParallel>(policy, name ? name : "Parallel"));
    AddChildToTop(*inst, node);
    inst->composite_stack.push(node);
}

extern "C" void dse_ai_end_composite(int tree_id) {
    auto* inst = GetBT(tree_id);
    if (inst && !inst->composite_stack.empty()) inst->composite_stack.pop();
}

extern "C" void dse_ai_add_condition(int tree_id, const char* name, dse_ai_condition_fn fn,
                                     void* user_data, dse_ai_destroy_fn destroy) {
    auto* inst = GetBT(tree_id);
    if (!inst) {
        if (destroy) destroy(user_data);
        return;
    }
    AddChildToTop(*inst, std::make_shared<CallbackConditionNode>(fn, user_data, destroy,
                                                                 name ? name : "Condition"));
}

extern "C" void dse_ai_add_action(int tree_id, const char* name, dse_ai_action_fn fn,
                                  void* user_data, dse_ai_destroy_fn destroy) {
    auto* inst = GetBT(tree_id);
    if (!inst) {
        if (destroy) destroy(user_data);
        return;
    }
    AddChildToTop(*inst, std::make_shared<CallbackActionNode>(fn, user_data, destroy,
                                                              name ? name : "Action"));
}

extern "C" void dse_ai_add_inverter(int tree_id, const char* name) {
    auto* inst = GetBT(tree_id);
    if (!inst) return;
    inst->pending_decorator = BTInstance::DecoratorType::Inverter;
    inst->pending_decorator_name = name ? name : "Inverter";
}

extern "C" void dse_ai_add_succeeder(int tree_id, const char* name) {
    auto* inst = GetBT(tree_id);
    if (!inst) return;
    inst->pending_decorator = BTInstance::DecoratorType::Succeeder;
    inst->pending_decorator_name = name ? name : "Succeeder";
}

extern "C" void dse_ai_add_repeater(int tree_id, const char* name, int max_repeats) {
    auto* inst = GetBT(tree_id);
    if (!inst) return;
    inst->pending_decorator = BTInstance::DecoratorType::Repeater;
    inst->pending_decorator_name = name ? name : "Repeater";
    inst->pending_repeater_count = max_repeats;
}

// ============================================================
// GOAP
// ============================================================

extern "C" int dse_ai_goap_create(void) {
    int id = s_next_goap_id++;
    s_goap_instances[id] = std::make_unique<GOAPInstance>();
    return id;
}

extern "C" void dse_ai_goap_destroy(int planner_id) {
    s_goap_instances.erase(planner_id);
}

extern "C" void dse_ai_goap_action_begin(int planner_id, const char* name, float cost) {
    auto* inst = GetGOAP(planner_id);
    if (!inst) return;
    inst->pending_action = GOAPAction();
    inst->pending_action.name = name ? name : "";
    inst->pending_action.cost = cost;
}

extern "C" void dse_ai_goap_action_precondition(int planner_id, const char* key, int value) {
    auto* inst = GetGOAP(planner_id);
    if (inst && key) inst->pending_action.preconditions[key] = (value != 0);
}

extern "C" void dse_ai_goap_action_effect(int planner_id, const char* key, int value) {
    auto* inst = GetGOAP(planner_id);
    if (inst && key) inst->pending_action.effects[key] = (value != 0);
}

extern "C" void dse_ai_goap_action_commit(int planner_id) {
    auto* inst = GetGOAP(planner_id);
    if (!inst) return;
    inst->planner.AddAction(inst->pending_action);
    inst->pending_action = GOAPAction();
}

extern "C" void dse_ai_goap_state_clear(int planner_id) {
    auto* inst = GetGOAP(planner_id);
    if (!inst) return;
    inst->current_state.clear();
    inst->goal.clear();
}

extern "C" void dse_ai_goap_state_set(int planner_id, int which, const char* key, int value) {
    auto* inst = GetGOAP(planner_id);
    if (!inst || !key) return;
    if (which == 0) inst->current_state[key] = (value != 0);
    else inst->goal[key] = (value != 0);
}

extern "C" int dse_ai_goap_plan(int planner_id, char* out, int cap) {
    auto* inst = GetGOAP(planner_id);
    if (!inst) return -1;
    const GOAPPlan plan = inst->planner.Plan(inst->current_state, inst->goal);
    if (!plan.valid) return -1;
    std::string joined;
    for (size_t i = 0; i < plan.actions.size(); ++i) {
        if (i > 0) joined += '\n';
        joined += plan.actions[i]->name;
    }
    return CopyStr(joined, out, cap);
}
