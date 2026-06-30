/**
 * @file goap_planner.cpp
 * @brief GOAP 规划器实现 —— A* 前向搜索
 */

#include "engine/ai/goap_planner.h"
#include <algorithm>
#include <queue>

namespace dse {
namespace ai {

void GOAPPlanner::AddAction(const GOAPAction& action) {
    actions_.push_back(action);
}

void GOAPPlanner::RemoveAction(const std::string& name) {
    actions_.erase(
        std::remove_if(actions_.begin(), actions_.end(),
                       [&](const GOAPAction& a) { return a.name == name; }),
        actions_.end());
}

void GOAPPlanner::ClearActions() {
    actions_.clear();
}

bool GOAPPlanner::StateSatisfiesGoal(const GOAPState& state, const GOAPState& goal) {
    for (const auto& [key, value] : goal) {
        auto it = state.find(key);
        if (it == state.end() || it->second != value) return false;
    }
    return true;
}

bool GOAPPlanner::PreconditionsMet(const GOAPAction& action, const GOAPState& state) {
    for (const auto& [key, value] : action.preconditions) {
        auto it = state.find(key);
        if (it == state.end() || it->second != value) return false;
    }
    if (action.procedural_precondition && !action.procedural_precondition()) {
        return false;
    }
    return true;
}

GOAPState GOAPPlanner::ApplyEffects(const GOAPState& state, const GOAPAction& action) {
    GOAPState result = state;
    for (const auto& [key, value] : action.effects) {
        result[key] = value;
    }
    return result;
}

/// A* 搜索节点
struct SearchNode {
    GOAPState state;
    float g_cost = 0.0f;           ///< 已花费代价
    float h_cost = 0.0f;           ///< 启发式估计
    int action_index = -1;         ///< 应用的动作索引
    int parent_index = -1;         ///< 父节点索引
    int depth = 0;

    float f() const { return g_cost + h_cost; }
};

GOAPPlan GOAPPlanner::Plan(const GOAPState& current_state,
                           const GOAPState& goal,
                           int max_depth) const {
    GOAPPlan result;

    if (StateSatisfiesGoal(current_state, goal)) {
        result.valid = true;
        return result;
    }

    if (actions_.empty()) return result;

    // 启发式函数：目标中未满足的条件数
    auto heuristic = [&goal](const GOAPState& state) -> float {
        float h = 0.0f;
        for (const auto& [key, value] : goal) {
            auto it = state.find(key);
            if (it == state.end() || it->second != value) h += 1.0f;
        }
        return h;
    };

    std::vector<SearchNode> open_list;
    std::vector<SearchNode> closed_list;

    // 初始节点
    SearchNode start;
    start.state = current_state;
    start.g_cost = 0.0f;
    start.h_cost = heuristic(current_state);
    open_list.push_back(start);

    while (!open_list.empty()) {
        // 找 f 最小的节点
        auto best_it = std::min_element(open_list.begin(), open_list.end(),
            [](const SearchNode& a, const SearchNode& b) { return a.f() < b.f(); });

        SearchNode current = *best_it;
        open_list.erase(best_it);

        if (current.depth >= max_depth) continue;

        int current_closed_idx = static_cast<int>(closed_list.size());
        closed_list.push_back(current);

        // 尝试每个可用动作
        for (int i = 0; i < static_cast<int>(actions_.size()); ++i) {
            const auto& action = actions_[i];
            if (!PreconditionsMet(action, current.state)) continue;

            GOAPState new_state = ApplyEffects(current.state, action);
            float new_g = current.g_cost + action.cost;

            if (StateSatisfiesGoal(new_state, goal)) {
                // 回溯构建计划
                result.valid = true;
                result.total_cost = new_g;
                result.actions.push_back(&actions_[i]);

                int parent = current_closed_idx;
                while (parent >= 0 && closed_list[parent].action_index >= 0) {
                    result.actions.push_back(&actions_[closed_list[parent].action_index]);
                    parent = closed_list[parent].parent_index;
                }
                std::reverse(result.actions.begin(), result.actions.end());
                return result;
            }

            SearchNode next;
            next.state = new_state;
            next.g_cost = new_g;
            next.h_cost = heuristic(new_state);
            next.action_index = i;
            next.parent_index = current_closed_idx;
            next.depth = current.depth + 1;
            open_list.push_back(next);
        }
    }

    return result; // valid = false
}

} // namespace ai
} // namespace dse
