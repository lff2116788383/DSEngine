/**
 * @file goap_planner.h
 * @brief GOAP（目标导向行为规划器）
 *
 * 基于 A* 的前向搜索：给定当前世界状态 + 动作集 + 目标条件，
 * 输出最优动作序列（Plan）。
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/core/dse_export.h"

namespace dse {
namespace ai {

/// GOAP 世界状态：键 → bool 值的扁平表示
using GOAPState = std::unordered_map<std::string, bool>;

/// GOAP 动作定义
struct DSE_EXPORT GOAPAction {
    std::string name;                    ///< 动作名称
    float cost = 1.0f;                   ///< 动作代价
    GOAPState preconditions;             ///< 前置条件
    GOAPState effects;                   ///< 执行后效果

    /// 可选：运行时验证（如弹药检查、距离检查）
    std::function<bool()> procedural_precondition;
};

/// GOAP 规划结果
struct GOAPPlan {
    bool valid = false;                  ///< 是否找到有效计划
    std::vector<const GOAPAction*> actions; ///< 动作序列
    float total_cost = 0.0f;             ///< 总代价
};

/// GOAP 规划器
class DSE_EXPORT GOAPPlanner {
public:
    GOAPPlanner() = default;
    ~GOAPPlanner() = default;

    /// 添加可用动作
    void AddAction(const GOAPAction& action);

    /// 移除动作
    void RemoveAction(const std::string& name);

    /// 清空所有动作
    void ClearActions();

    /// 获取动作数量
    size_t ActionCount() const { return actions_.size(); }

    /// 执行规划
    /// @param current_state 当前世界状态
    /// @param goal 目标状态（需要满足的条件集）
    /// @param max_depth 最大搜索深度（防止无限循环，默认 20）
    /// @return 规划结果
    GOAPPlan Plan(const GOAPState& current_state,
                  const GOAPState& goal,
                  int max_depth = 20) const;

private:
    /// 检查状态是否满足目标
    static bool StateSatisfiesGoal(const GOAPState& state, const GOAPState& goal);

    /// 检查动作的前置条件是否满足
    static bool PreconditionsMet(const GOAPAction& action, const GOAPState& state);

    /// 应用动作效果到状态
    static GOAPState ApplyEffects(const GOAPState& state, const GOAPAction& action);

    std::vector<GOAPAction> actions_;
};

} // namespace ai
} // namespace dse
