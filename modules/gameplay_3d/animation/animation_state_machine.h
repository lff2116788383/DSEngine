#ifndef DSE_ANIMATION_STATE_MACHINE_H
#define DSE_ANIMATION_STATE_MACHINE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>

namespace dse {
namespace gameplay3d {

// Parameter types for transitions
enum class AnimParamType {
    Float,
    Int,
    Bool,
    Trigger
};

struct AnimParameter {
    AnimParamType type;
    std::variant<float, int, bool> value;
    
    // For Trigger type
    bool is_triggered = false;
};

// Condition for transitioning between states
enum class AnimConditionMode {
    Greater,
    Less,
    Equals,
    NotEqual,
    If,      // For bool/trigger
    IfNot    // For bool
};

struct AnimTransitionCondition {
    std::string parameter_name;
    AnimConditionMode mode;
    float threshold = 0.0f; // Used for Greater/Less comparisons
    int int_value = 0;      // Used for Equals/NotEqual on Int
};

struct AnimTransition {
    std::string target_state;
    bool has_exit_time = true;
    float exit_time = 1.0f; // Normalized time (0.0 to 1.0) of the source state before transitioning
    float transition_duration = 0.25f; // In seconds, for crossfading
    
    std::vector<AnimTransitionCondition> conditions;
};

// Represents a node in a Blend Tree
struct BlendTreeNode {
    std::string danim_path;
    float threshold = 0.0f; // 1D Blend threshold
};

// Represents a state in the state machine (can be a single clip or a 1D blend tree)
struct AnimState {
    std::string name;
    
    // If not empty, it's a single clip state
    std::string danim_path;
    float speed = 1.0f;
    bool loop = true;
    
    // If danim_path is empty, it's a Blend Tree
    bool is_blend_tree = false;
    std::string blend_parameter; // Parameter to drive the 1D blend
    std::vector<BlendTreeNode> blend_nodes;
    
    std::vector<AnimTransition> transitions;
};

class AnimationStateMachine {
public:
    AnimationStateMachine() = default;
    ~AnimationStateMachine() = default;

    void AddState(const AnimState& state);
    void SetDefaultState(const std::string& state_name);
    
    void AddParameter(const std::string& name, AnimParamType type, float default_val = 0.0f);
    void AddParameter(const std::string& name, AnimParamType type, int default_val);
    void AddParameter(const std::string& name, AnimParamType type, bool default_val);
    void AddTrigger(const std::string& name);

    void SetFloat(const std::string& name, float value);
    void SetInt(const std::string& name, int value);
    void SetBool(const std::string& name, bool value);
    void SetTrigger(const std::string& name);
    void ResetTrigger(const std::string& name);

    float GetFloat(const std::string& name) const;
    int GetInt(const std::string& name) const;
    bool GetBool(const std::string& name) const;

    const std::unordered_map<std::string, AnimState>& GetStates() const { return states_; }
    const std::string& GetDefaultState() const { return default_state_; }
    const std::unordered_map<std::string, AnimParameter>& GetParameters() const { return parameters_; }
    
    // Helper to evaluate if a transition's conditions are met
    bool EvaluateTransition(const AnimTransition& transition, float normalized_time) const;

private:
    std::unordered_map<std::string, AnimState> states_;
    std::string default_state_;
    std::unordered_map<std::string, AnimParameter> parameters_;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_ANIMATION_STATE_MACHINE_H