#include "modules/gameplay_3d/animation/animation_state_machine.h"

namespace dse {
namespace gameplay3d {

void AnimationStateMachine::AddState(const AnimState& state) {
    states_[state.name] = state;
    if (default_state_.empty()) {
        default_state_ = state.name;
    }
}

void AnimationStateMachine::SetDefaultState(const std::string& state_name) {
    if (states_.find(state_name) != states_.end()) {
        default_state_ = state_name;
    }
}

void AnimationStateMachine::AddParameter(const std::string& name, AnimParamType type, float default_val) {
    parameters_[name] = {type, default_val};
}

void AnimationStateMachine::AddParameter(const std::string& name, AnimParamType type, int default_val) {
    parameters_[name] = {type, default_val};
}

void AnimationStateMachine::AddParameter(const std::string& name, AnimParamType type, bool default_val) {
    parameters_[name] = {type, default_val};
}

void AnimationStateMachine::AddTrigger(const std::string& name) {
    parameters_[name] = {AnimParamType::Trigger, false, false};
}

void AnimationStateMachine::SetFloat(const std::string& name, float value) {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second.type == AnimParamType::Float) {
        it->second.value = value;
    }
}

void AnimationStateMachine::SetInt(const std::string& name, int value) {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second.type == AnimParamType::Int) {
        it->second.value = value;
    }
}

void AnimationStateMachine::SetBool(const std::string& name, bool value) {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second.type == AnimParamType::Bool) {
        it->second.value = value;
    }
}

void AnimationStateMachine::SetTrigger(const std::string& name) {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second.type == AnimParamType::Trigger) {
        it->second.is_triggered = true;
    }
}

void AnimationStateMachine::ResetTrigger(const std::string& name) {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second.type == AnimParamType::Trigger) {
        it->second.is_triggered = false;
    }
}

float AnimationStateMachine::GetFloat(const std::string& name) const {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second.type == AnimParamType::Float) {
        return std::get<float>(it->second.value);
    }
    return 0.0f;
}

int AnimationStateMachine::GetInt(const std::string& name) const {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second.type == AnimParamType::Int) {
        return std::get<int>(it->second.value);
    }
    return 0;
}

bool AnimationStateMachine::GetBool(const std::string& name) const {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second.type == AnimParamType::Bool) {
        return std::get<bool>(it->second.value);
    }
    return false;
}

bool AnimationStateMachine::EvaluateTransition(const AnimTransition& transition, float normalized_time) const {
    if (transition.has_exit_time && normalized_time < transition.exit_time) {
        return false;
    }
    
    for (const auto& cond : transition.conditions) {
        auto it = parameters_.find(cond.parameter_name);
        if (it == parameters_.end()) return false;
        
        const auto& param = it->second;
        switch (cond.mode) {
            case AnimConditionMode::Greater:
                if (param.type == AnimParamType::Float && std::get<float>(param.value) <= cond.threshold) return false;
                if (param.type == AnimParamType::Int && std::get<int>(param.value) <= static_cast<int>(cond.threshold)) return false;
                break;
            case AnimConditionMode::Less:
                if (param.type == AnimParamType::Float && std::get<float>(param.value) >= cond.threshold) return false;
                if (param.type == AnimParamType::Int && std::get<int>(param.value) >= static_cast<int>(cond.threshold)) return false;
                break;
            case AnimConditionMode::Equals:
                if (param.type == AnimParamType::Int && std::get<int>(param.value) != cond.int_value) return false;
                break;
            case AnimConditionMode::NotEqual:
                if (param.type == AnimParamType::Int && std::get<int>(param.value) == cond.int_value) return false;
                break;
            case AnimConditionMode::If:
                if (param.type == AnimParamType::Bool && !std::get<bool>(param.value)) return false;
                if (param.type == AnimParamType::Trigger && !param.is_triggered) return false;
                break;
            case AnimConditionMode::IfNot:
                if (param.type == AnimParamType::Bool && std::get<bool>(param.value)) return false;
                break;
        }
    }
    return true;
}

} // namespace gameplay3d
} // namespace dse