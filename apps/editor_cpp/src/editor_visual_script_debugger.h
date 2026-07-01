#pragma once

#include "editor_context.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace dse::editor {

/// Runtime debugging state for the Visual Script editor
enum class VsDebugState { Idle, Running, Paused, StepOver, StepInto };

struct VsBreakpoint {
    int node_id = -1;
    bool enabled = true;
    std::string condition;  // optional conditional expression
    int hit_count = 0;
};

struct VsWatchVariable {
    std::string name;
    std::string value;
    std::string type;
    bool changed = false;  // highlight if value changed this step
};

struct VsCallFrame {
    std::string function_name;
    int node_id = -1;
    int line_number = 0;
};

struct VsDebugSession {
    VsDebugState state = VsDebugState::Idle;
    std::vector<VsBreakpoint> breakpoints;
    std::vector<VsWatchVariable> watch_variables;
    std::vector<VsWatchVariable> local_variables;
    std::vector<VsCallFrame> call_stack;
    int current_node = -1;  // currently executing node (highlighted)
    int current_line = -1;  // line in generated Lua
    std::string last_error;
    float execution_time = 0.0f;
    int step_count = 0;
    // Execution trace (last N executed nodes)
    std::vector<int> execution_trace;
    static constexpr int kMaxTraceSize = 64;
};

/// Draw the Visual Script Debugger panel (breakpoints, call stack, watches, locals)
void DrawVisualScriptDebugger(EditorContext& ctx);

/// Get the debug session (for integration with visual script editor)
VsDebugSession& GetVsDebugSession();

/// Toggle breakpoint on a node
void VsToggleBreakpoint(int node_id);

/// Check if a node has a breakpoint
bool VsHasBreakpoint(int node_id);

/// Debug control commands
void VsDebugStart();
void VsDebugStop();
void VsDebugPause();
void VsDebugStepOver();
void VsDebugStepInto();
void VsDebugContinue();

/// Add a watch expression
void VsDebugAddWatch(const std::string& expr);

// Test accessor (alias for GetVsDebugSession for consistent naming)
struct VsDebuggerTestState {
    VsDebugState debug_state = VsDebugState::Idle;
    std::vector<VsBreakpoint> breakpoints;
};
VsDebuggerTestState& GetVsDebuggerState();

} // namespace dse::editor
