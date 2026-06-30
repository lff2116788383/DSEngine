/**
 * @file behavior_tree.h
 * @brief 行为树系统 —— 节点库、树执行器
 *
 * 节点类型：
 * - Composite：Sequence / Selector / Parallel
 * - Decorator：Inverter / Repeater / Succeeder / RepeatUntilFail
 * - Leaf：Condition / Action（用户继承或通过 lambda/Lua 回调实现）
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "engine/ai/blackboard.h"
#include "engine/core/dse_export.h"

namespace dse {
namespace ai {

/// 节点执行状态
enum class BTStatus : uint8_t {
    Success = 0,
    Failure = 1,
    Running = 2,
};

/// 行为树节点基类
class DSE_EXPORT BTNode {
public:
    explicit BTNode(const std::string& name = "") : name_(name) {}
    virtual ~BTNode() = default;

    virtual BTStatus Tick(float dt, Blackboard& bb) = 0;
    virtual void Reset() {}

    const std::string& GetName() const { return name_; }

protected:
    std::string name_;
};

using BTNodePtr = std::shared_ptr<BTNode>;

// ============================================================
// Composite Nodes
// ============================================================

/// Sequence：依次执行子节点，任一失败则整体失败
class DSE_EXPORT BTSequence : public BTNode {
public:
    explicit BTSequence(const std::string& name = "Sequence") : BTNode(name) {}

    void AddChild(BTNodePtr child) { children_.push_back(std::move(child)); }
    BTStatus Tick(float dt, Blackboard& bb) override;
    void Reset() override;

private:
    std::vector<BTNodePtr> children_;
    size_t current_index_ = 0;
};

/// Selector：依次执行子节点，任一成功则整体成功
class DSE_EXPORT BTSelector : public BTNode {
public:
    explicit BTSelector(const std::string& name = "Selector") : BTNode(name) {}

    void AddChild(BTNodePtr child) { children_.push_back(std::move(child)); }
    BTStatus Tick(float dt, Blackboard& bb) override;
    void Reset() override;

private:
    std::vector<BTNodePtr> children_;
    size_t current_index_ = 0;
};

/// Parallel 策略
enum class ParallelPolicy : uint8_t {
    RequireAll,  ///< 所有子节点成功才成功
    RequireOne,  ///< 任一子节点成功即成功
};

/// Parallel：并行执行所有子节点
class DSE_EXPORT BTParallel : public BTNode {
public:
    BTParallel(ParallelPolicy success_policy = ParallelPolicy::RequireAll,
               const std::string& name = "Parallel")
        : BTNode(name), success_policy_(success_policy) {}

    void AddChild(BTNodePtr child) { children_.push_back(std::move(child)); }
    BTStatus Tick(float dt, Blackboard& bb) override;
    void Reset() override;

private:
    std::vector<BTNodePtr> children_;
    ParallelPolicy success_policy_;
};

// ============================================================
// Decorator Nodes
// ============================================================

/// Inverter：反转子节点结果
class DSE_EXPORT BTInverter : public BTNode {
public:
    explicit BTInverter(BTNodePtr child, const std::string& name = "Inverter")
        : BTNode(name), child_(std::move(child)) {}

    BTStatus Tick(float dt, Blackboard& bb) override;
    void Reset() override;

private:
    BTNodePtr child_;
};

/// Repeater：重复执行子节点 N 次（-1 = 无限）
class DSE_EXPORT BTRepeater : public BTNode {
public:
    BTRepeater(BTNodePtr child, int max_repeats = -1, const std::string& name = "Repeater")
        : BTNode(name), child_(std::move(child)), max_repeats_(max_repeats) {}

    BTStatus Tick(float dt, Blackboard& bb) override;
    void Reset() override;

private:
    BTNodePtr child_;
    int max_repeats_;
    int count_ = 0;
};

/// Succeeder：无论子节点结果如何都返回成功
class DSE_EXPORT BTSucceeder : public BTNode {
public:
    explicit BTSucceeder(BTNodePtr child, const std::string& name = "Succeeder")
        : BTNode(name), child_(std::move(child)) {}

    BTStatus Tick(float dt, Blackboard& bb) override;
    void Reset() override;

private:
    BTNodePtr child_;
};

// ============================================================
// Leaf Nodes
// ============================================================

/// 条件节点回调类型
using BTConditionFunc = std::function<bool(const Blackboard& bb)>;

/// Condition：评估条件，返回 Success 或 Failure
class DSE_EXPORT BTCondition : public BTNode {
public:
    BTCondition(BTConditionFunc func, const std::string& name = "Condition")
        : BTNode(name), func_(std::move(func)) {}

    BTStatus Tick(float dt, Blackboard& bb) override;

private:
    BTConditionFunc func_;
};

/// 动作节点回调类型（返回 BTStatus 支持 Running 多帧）
using BTActionFunc = std::function<BTStatus(float dt, Blackboard& bb)>;

/// Action：执行动作
class DSE_EXPORT BTAction : public BTNode {
public:
    BTAction(BTActionFunc func, const std::string& name = "Action")
        : BTNode(name), func_(std::move(func)) {}

    BTStatus Tick(float dt, Blackboard& bb) override;

private:
    BTActionFunc func_;
};

// ============================================================
// Behavior Tree Container
// ============================================================

/// 行为树：包含根节点 + 关联黑板
class DSE_EXPORT BehaviorTree {
public:
    BehaviorTree() = default;
    ~BehaviorTree() = default;

    void SetRoot(BTNodePtr root) { root_ = std::move(root); }
    BTNodePtr GetRoot() const { return root_; }

    Blackboard& GetBlackboard() { return blackboard_; }
    const Blackboard& GetBlackboard() const { return blackboard_; }

    /// 每帧 Tick
    BTStatus Tick(float dt);

    /// 重置整棵树
    void Reset();

private:
    BTNodePtr root_;
    Blackboard blackboard_;
};

} // namespace ai
} // namespace dse
