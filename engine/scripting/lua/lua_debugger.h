#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <mutex>

struct lua_State;
struct lua_Debug;

namespace dse::scripting {

enum class DebugAction {
    Continue,   // resume execution
    StepOver,   // step to next line in same depth
    StepInto,   // step into function call
    StepOut,    // step out of current function
};

struct Breakpoint {
    std::string source;  // normalized source path (e.g. "script/main.lua")
    int line = 0;
};

struct StackFrame {
    std::string source;
    std::string function_name;
    int line = 0;
};

struct WatchVariable {
    std::string name;
    std::string value;
    std::string type;   // "number", "string", "table", "boolean", etc.
};

/// Lua script debugger — attach to a lua_State to enable breakpoints, stepping, and inspection.
class LuaDebugger {
public:
    static LuaDebugger& Instance();

    void Attach(lua_State* L);
    void Detach();
    bool IsAttached() const;

    // Breakpoint management
    void AddBreakpoint(const std::string& source, int line);
    void RemoveBreakpoint(const std::string& source, int line);
    void ClearAllBreakpoints();
    bool HasBreakpoint(const std::string& source, int line) const;
    std::vector<Breakpoint> GetBreakpoints() const;

    // Execution control (thread-safe — UI thread calls these, hook runs in Lua thread)
    bool IsPaused() const;
    void Resume();
    void StepOver();
    void StepInto();
    void StepOut();

    // State inspection (valid only when paused)
    std::vector<StackFrame> GetCallStack() const;
    std::vector<WatchVariable> GetLocals() const;
    std::vector<WatchVariable> EvaluateExpression(const std::string& expr) const;

    // Paused source location
    std::string GetPausedSource() const;
    int GetPausedLine() const;

    // Called by editor to enable/disable the debugger
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

private:
    LuaDebugger() = default;

    static void HookCallback(lua_State* L, lua_Debug* ar);
    void OnHook(lua_State* L, lua_Debug* ar);
    void PauseAndWait();
    std::string NormalizeSource(const char* source) const;

    lua_State* lua_state_ = nullptr;
    bool enabled_ = false;

    // Breakpoints (guarded by mutex for thread-safe access from UI)
    mutable std::mutex bp_mutex_;
    std::unordered_set<std::string> breakpoint_keys_;  // "source:line"
    std::vector<Breakpoint> breakpoints_;

    // Execution state
    mutable std::mutex state_mutex_;
    bool paused_ = false;
    DebugAction pending_action_ = DebugAction::Continue;
    int step_depth_ = 0;
    std::string paused_source_;
    int paused_line_ = 0;

    // Cached call stack / locals (filled when paused)
    mutable std::vector<StackFrame> cached_stack_;
    mutable std::vector<WatchVariable> cached_locals_;
};

} // namespace dse::scripting
