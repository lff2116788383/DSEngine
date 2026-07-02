"""
Agent Safety Policy for DSEngine Editor.

Prevents LLM hallucinations from causing destructive operations.
Enforces tool blocklist, rate limiting, and code length constraints.
"""

import time
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class SafetyCheckResult:
    allowed: bool
    reason: Optional[str] = None


class AgentSafetyPolicy:
    """Safety sandbox for Agent mode tool calls."""

    # Tools blocked in Agent mode (too destructive for autonomous use)
    BLOCKED_TOOLS: set[str] = {
        "dsengine_entity_batch_delete",
    }

    # Lua code length limit (characters)
    MAX_LUA_CODE_LENGTH: int = 2000

    # Rate limiting
    MAX_CALLS_PER_SECOND: int = 5
    _call_timestamps: list[float] = []

    @classmethod
    def check_tool_call(cls, tool_name: str, args: dict) -> SafetyCheckResult:
        """Pre-execution safety check for a tool call."""

        # 1. Tool blocklist
        if tool_name in cls.BLOCKED_TOOLS:
            return SafetyCheckResult(
                allowed=False,
                reason=f"Tool '{tool_name}' is blocked in Agent mode. "
                       f"Use individual dsengine_entity_delete instead.",
            )

        # 2. Lua code length limit
        if tool_name == "dsengine_lua_execute":
            code = args.get("code", "")
            if len(code) > cls.MAX_LUA_CODE_LENGTH:
                return SafetyCheckResult(
                    allowed=False,
                    reason=f"Lua code too long ({len(code)} chars, "
                           f"max {cls.MAX_LUA_CODE_LENGTH}). "
                           f"Use dsengine_script_create for large scripts.",
                )

        # 3. Rate limiting
        now = time.time()
        cls._call_timestamps = [t for t in cls._call_timestamps if now - t < 1.0]
        if len(cls._call_timestamps) >= cls.MAX_CALLS_PER_SECOND:
            return SafetyCheckResult(
                allowed=False,
                reason=f"Rate limit: max {cls.MAX_CALLS_PER_SECOND} calls/second",
            )
        cls._call_timestamps.append(now)

        return SafetyCheckResult(allowed=True)

    @classmethod
    def reset(cls):
        """Reset rate limiting state (for testing)."""
        cls._call_timestamps.clear()
