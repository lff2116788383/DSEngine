#include "engine/scripting/lua/lua_debugger.h"
#include "engine/base/debug.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>

namespace dse::scripting {

LuaDebugger& LuaDebugger::Instance() {
    static LuaDebugger instance;
    return instance;
}

void LuaDebugger::Attach(lua_State* L) {
    lua_state_ = L;
    if (L && enabled_) {
        lua_sethook(L, HookCallback, LUA_MASKLINE | LUA_MASKCALL | LUA_MASKRET, 0);
    }
}

void LuaDebugger::Detach() {
    if (lua_state_) {
        lua_sethook(lua_state_, nullptr, 0, 0);
    }
    lua_state_ = nullptr;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        paused_ = false;
        pending_action_ = DebugAction::Continue;
    }
}

bool LuaDebugger::IsAttached() const {
    return lua_state_ != nullptr;
}

void LuaDebugger::SetEnabled(bool enabled) {
    enabled_ = enabled;
    if (lua_state_) {
        if (enabled) {
            lua_sethook(lua_state_, HookCallback, LUA_MASKLINE | LUA_MASKCALL | LUA_MASKRET, 0);
        } else {
            lua_sethook(lua_state_, nullptr, 0, 0);
            // If paused, resume
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (paused_) {
                paused_ = false;
                pending_action_ = DebugAction::Continue;
            }
        }
    }
}

bool LuaDebugger::IsEnabled() const {
    return enabled_;
}

// ─── Breakpoints ────────────────────────────────────────────────────────────

static std::string MakeKey(const std::string& source, int line) {
    return source + ":" + std::to_string(line);
}

void LuaDebugger::AddBreakpoint(const std::string& source, int line) {
    std::lock_guard<std::mutex> lock(bp_mutex_);
    std::string key = MakeKey(source, line);
    if (breakpoint_keys_.insert(key).second) {
        breakpoints_.push_back({source, line});
    }
}

void LuaDebugger::RemoveBreakpoint(const std::string& source, int line) {
    std::lock_guard<std::mutex> lock(bp_mutex_);
    std::string key = MakeKey(source, line);
    if (breakpoint_keys_.erase(key)) {
        breakpoints_.erase(
            std::remove_if(breakpoints_.begin(), breakpoints_.end(),
                [&](const Breakpoint& bp) { return bp.source == source && bp.line == line; }),
            breakpoints_.end());
    }
}

void LuaDebugger::ClearAllBreakpoints() {
    std::lock_guard<std::mutex> lock(bp_mutex_);
    breakpoint_keys_.clear();
    breakpoints_.clear();
}

bool LuaDebugger::HasBreakpoint(const std::string& source, int line) const {
    std::lock_guard<std::mutex> lock(bp_mutex_);
    return breakpoint_keys_.count(MakeKey(source, line)) > 0;
}

std::vector<Breakpoint> LuaDebugger::GetBreakpoints() const {
    std::lock_guard<std::mutex> lock(bp_mutex_);
    return breakpoints_;
}

// ─── Execution Control ──────────────────────────────────────────────────────

bool LuaDebugger::IsPaused() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return paused_;
}

void LuaDebugger::Resume() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    pending_action_ = DebugAction::Continue;
    paused_ = false;
}

void LuaDebugger::StepOver() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    pending_action_ = DebugAction::StepOver;
    paused_ = false;
}

void LuaDebugger::StepInto() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    pending_action_ = DebugAction::StepInto;
    paused_ = false;
}

void LuaDebugger::StepOut() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    pending_action_ = DebugAction::StepOut;
    paused_ = false;
}

std::string LuaDebugger::GetPausedSource() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return paused_source_;
}

int LuaDebugger::GetPausedLine() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return paused_line_;
}

// ─── Inspection ─────────────────────────────────────────────────────────────

std::vector<StackFrame> LuaDebugger::GetCallStack() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return cached_stack_;
}

std::vector<WatchVariable> LuaDebugger::GetLocals() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return cached_locals_;
}

std::vector<WatchVariable> LuaDebugger::EvaluateExpression(const std::string& expr) const {
    std::vector<WatchVariable> results;
    if (!lua_state_ || !paused_) return results;

    std::string code = "return " + expr;
    if (luaL_loadstring(lua_state_, code.c_str()) == LUA_OK) {
        if (lua_pcall(lua_state_, 0, 1, 0) == LUA_OK) {
            WatchVariable var;
            var.name = expr;
            var.type = lua_typename(lua_state_, lua_type(lua_state_, -1));
            if (lua_isstring(lua_state_, -1)) {
                var.value = lua_tostring(lua_state_, -1);
            } else if (lua_isnumber(lua_state_, -1)) {
                var.value = std::to_string(lua_tonumber(lua_state_, -1));
            } else if (lua_isboolean(lua_state_, -1)) {
                var.value = lua_toboolean(lua_state_, -1) ? "true" : "false";
            } else if (lua_isnil(lua_state_, -1)) {
                var.value = "nil";
            } else {
                std::ostringstream oss;
                oss << var.type << ": " << lua_topointer(lua_state_, -1);
                var.value = oss.str();
            }
            lua_pop(lua_state_, 1);
            results.push_back(var);
        } else {
            WatchVariable var;
            var.name = expr;
            var.type = "error";
            var.value = lua_tostring(lua_state_, -1);
            lua_pop(lua_state_, 1);
            results.push_back(var);
        }
    } else {
        lua_pop(lua_state_, 1);
    }
    return results;
}

// ─── Hook ───────────────────────────────────────────────────────────────────

std::string LuaDebugger::NormalizeSource(const char* source) const {
    if (!source) return "";
    std::string s = source;
    // Lua reports source as "@path" for files
    if (!s.empty() && s[0] == '@') {
        s = s.substr(1);
    }
    // Normalize separators
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

void LuaDebugger::HookCallback(lua_State* L, lua_Debug* ar) {
    LuaDebugger::Instance().OnHook(L, ar);
}

void LuaDebugger::OnHook(lua_State* L, lua_Debug* ar) {
    if (!enabled_) return;

    lua_getinfo(L, "Snl", ar);
    const std::string source = NormalizeSource(ar->source);
    const int line = ar->currentline;

    if (ar->event == LUA_HOOKLINE) {
        bool should_pause = false;

        // Check breakpoints
        {
            std::lock_guard<std::mutex> lock(bp_mutex_);
            should_pause = breakpoint_keys_.count(MakeKey(source, line)) > 0;
        }

        // Check stepping
        if (!should_pause) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (pending_action_ == DebugAction::StepInto) {
                should_pause = true;
            } else if (pending_action_ == DebugAction::StepOver) {
                // Get current stack depth
                int depth = 0;
                lua_Debug d;
                while (lua_getstack(L, depth, &d)) ++depth;
                if (depth <= step_depth_) {
                    should_pause = true;
                }
            }
        }

        if (should_pause) {
            // Capture stack depth for stepping
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                step_depth_ = 0;
                lua_Debug d;
                while (lua_getstack(L, step_depth_, &d)) ++step_depth_;
            }

            // Capture call stack
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                cached_stack_.clear();
                lua_Debug d;
                for (int level = 0; lua_getstack(L, level, &d); ++level) {
                    lua_getinfo(L, "Snl", &d);
                    StackFrame frame;
                    frame.source = NormalizeSource(d.source);
                    frame.function_name = d.name ? d.name : (d.what ? d.what : "?");
                    frame.line = d.currentline;
                    cached_stack_.push_back(frame);
                }
            }

            // Capture locals at level 0
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                cached_locals_.clear();
                const char* name;
                for (int i = 1; (name = lua_getlocal(L, ar, i)) != nullptr; ++i) {
                    WatchVariable var;
                    var.name = name;
                    var.type = lua_typename(L, lua_type(L, -1));
                    if (lua_isnumber(L, -1)) {
                        var.value = std::to_string(lua_tonumber(L, -1));
                    } else if (lua_isstring(L, -1)) {
                        var.value = lua_tostring(L, -1);
                    } else if (lua_isboolean(L, -1)) {
                        var.value = lua_toboolean(L, -1) ? "true" : "false";
                    } else if (lua_isnil(L, -1)) {
                        var.value = "nil";
                    } else {
                        std::ostringstream oss;
                        oss << var.type << ": " << lua_topointer(L, -1);
                        var.value = oss.str();
                    }
                    lua_pop(L, 1);
                    // Skip internal variables starting with '('
                    if (!var.name.empty() && var.name[0] != '(') {
                        cached_locals_.push_back(var);
                    }
                }
            }

            PauseAndWait();
        }
    } else if (ar->event == LUA_HOOKRET) {
        // Step out detection
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (pending_action_ == DebugAction::StepOut) {
            int depth = 0;
            lua_Debug d;
            while (lua_getstack(L, depth, &d)) ++depth;
            if (depth < step_depth_) {
                pending_action_ = DebugAction::StepInto;  // will pause on next line
            }
        }
    }
}

void LuaDebugger::PauseAndWait() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        paused_ = true;
        paused_source_ = cached_stack_.empty() ? "" : cached_stack_[0].source;
        paused_line_ = cached_stack_.empty() ? 0 : cached_stack_[0].line;
        pending_action_ = DebugAction::Continue;  // reset for next action
    }

    DEBUG_LOG_INFO("LuaDebugger: paused at {}:{}", paused_source_, paused_line_);

    // Spin-wait until UI resumes (this blocks the Lua thread)
    while (true) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (!paused_) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

} // namespace dse::scripting
