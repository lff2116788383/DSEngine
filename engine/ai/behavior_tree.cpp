/**
 * @file behavior_tree.cpp
 * @brief 行为树节点实现
 */

#include "engine/ai/behavior_tree.h"

namespace dse {
namespace ai {

// ============================================================
// BTSequence
// ============================================================

BTStatus BTSequence::Tick(float dt, Blackboard& bb) {
    while (current_index_ < children_.size()) {
        BTStatus status = children_[current_index_]->Tick(dt, bb);
        if (status == BTStatus::Running) return BTStatus::Running;
        if (status == BTStatus::Failure) {
            current_index_ = 0;
            return BTStatus::Failure;
        }
        ++current_index_;
    }
    current_index_ = 0;
    return BTStatus::Success;
}

void BTSequence::Reset() {
    current_index_ = 0;
    for (auto& child : children_) child->Reset();
}

// ============================================================
// BTSelector
// ============================================================

BTStatus BTSelector::Tick(float dt, Blackboard& bb) {
    while (current_index_ < children_.size()) {
        BTStatus status = children_[current_index_]->Tick(dt, bb);
        if (status == BTStatus::Running) return BTStatus::Running;
        if (status == BTStatus::Success) {
            current_index_ = 0;
            return BTStatus::Success;
        }
        ++current_index_;
    }
    current_index_ = 0;
    return BTStatus::Failure;
}

void BTSelector::Reset() {
    current_index_ = 0;
    for (auto& child : children_) child->Reset();
}

// ============================================================
// BTParallel
// ============================================================

BTStatus BTParallel::Tick(float dt, Blackboard& bb) {
    int success_count = 0;
    int failure_count = 0;
    bool any_running = false;

    for (auto& child : children_) {
        BTStatus status = child->Tick(dt, bb);
        switch (status) {
            case BTStatus::Success: ++success_count; break;
            case BTStatus::Failure: ++failure_count; break;
            case BTStatus::Running: any_running = true; break;
        }
    }

    if (success_policy_ == ParallelPolicy::RequireAll) {
        if (failure_count > 0) return BTStatus::Failure;
        if (any_running) return BTStatus::Running;
        return BTStatus::Success;
    } else { // RequireOne
        if (success_count > 0) return BTStatus::Success;
        if (any_running) return BTStatus::Running;
        return BTStatus::Failure;
    }
}

void BTParallel::Reset() {
    for (auto& child : children_) child->Reset();
}

// ============================================================
// Decorators
// ============================================================

BTStatus BTInverter::Tick(float dt, Blackboard& bb) {
    BTStatus status = child_->Tick(dt, bb);
    if (status == BTStatus::Success) return BTStatus::Failure;
    if (status == BTStatus::Failure) return BTStatus::Success;
    return BTStatus::Running;
}

void BTInverter::Reset() { child_->Reset(); }

BTStatus BTRepeater::Tick(float dt, Blackboard& bb) {
    if (max_repeats_ >= 0 && count_ >= max_repeats_) {
        return BTStatus::Success;
    }
    BTStatus status = child_->Tick(dt, bb);
    if (status == BTStatus::Running) return BTStatus::Running;
    ++count_;
    if (max_repeats_ >= 0 && count_ >= max_repeats_) {
        return BTStatus::Success;
    }
    child_->Reset();
    return BTStatus::Running;
}

void BTRepeater::Reset() {
    count_ = 0;
    child_->Reset();
}

BTStatus BTSucceeder::Tick(float dt, Blackboard& bb) {
    BTStatus status = child_->Tick(dt, bb);
    if (status == BTStatus::Running) return BTStatus::Running;
    return BTStatus::Success;
}

void BTSucceeder::Reset() { child_->Reset(); }

// ============================================================
// Leaf Nodes
// ============================================================

BTStatus BTCondition::Tick(float /*dt*/, Blackboard& bb) {
    if (func_ && func_(bb)) return BTStatus::Success;
    return BTStatus::Failure;
}

BTStatus BTAction::Tick(float dt, Blackboard& bb) {
    if (func_) return func_(dt, bb);
    return BTStatus::Failure;
}

// ============================================================
// BehaviorTree
// ============================================================

BTStatus BehaviorTree::Tick(float dt) {
    if (!root_) return BTStatus::Failure;
    return root_->Tick(dt, blackboard_);
}

void BehaviorTree::Reset() {
    if (root_) root_->Reset();
}

} // namespace ai
} // namespace dse
