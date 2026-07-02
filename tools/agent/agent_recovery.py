"""
Agent Error Recovery for DSEngine Editor.

Classifies errors and applies automatic recovery strategies
before escalating to the user.
"""

import asyncio
import json
import sys
from enum import Enum
from typing import Optional


class ErrorCategory(Enum):
    TRANSIENT = "transient"        # Network timeout, rate limit -> retry with backoff
    TOOL_FAILURE = "tool_failure"  # Tool returned error -> undo and retry
    STATE_STALE = "state_stale"    # Scene state out of sync -> refresh and retry
    LLM_ERROR = "llm_error"       # LLM returned invalid output -> retry with lower temp
    FATAL = "fatal"                # Unrecoverable -> escalate to user


def classify_error(error: str, tool_name: Optional[str] = None) -> ErrorCategory:
    """Classify an error into a recovery category."""
    error_lower = error.lower()

    # Transient errors
    if any(kw in error_lower for kw in [
        "rate limit", "timeout", "connection", "429", "503", "502",
        "retry", "temporary", "overloaded",
    ]):
        return ErrorCategory.TRANSIENT

    # Tool-specific failures
    if tool_name:
        if any(kw in error_lower for kw in [
            "entity not found", "invalid entity", "component not found",
            "already exists", "duplicate",
        ]):
            return ErrorCategory.TOOL_FAILURE

        if any(kw in error_lower for kw in [
            "scene", "state", "stale", "out of sync", "not loaded",
        ]):
            return ErrorCategory.STATE_STALE

    # LLM output issues
    if any(kw in error_lower for kw in [
        "json", "parse", "invalid format", "unexpected token",
        "schema", "validation",
    ]):
        return ErrorCategory.LLM_ERROR

    # Default to tool failure if a tool was involved
    if tool_name:
        return ErrorCategory.TOOL_FAILURE

    return ErrorCategory.FATAL


class RecoveryManager:
    """Manages error recovery strategies for Agent execution."""

    def __init__(self, emit_fn=None):
        self._emit = emit_fn or self._default_emit

    @staticmethod
    def _default_emit(obj: dict):
        sys.stdout.write(json.dumps(obj, ensure_ascii=False) + "\n")
        sys.stdout.flush()

    async def attempt_recovery(self, error: str,
                                tool_name: Optional[str] = None,
                                retry_count: int = 0,
                                max_retries: int = 3,
                                call_tool_fn=None) -> dict:
        """Attempt automatic recovery based on error category.

        Returns:
            {"action": "retry"} - retry the operation
            {"action": "retry", "updated_context": ...} - retry with new context
            {"action": "skip"} - skip this task, move to next
            {"action": "escalate_to_user", "error": ...} - ask user for help
        """
        if retry_count >= max_retries:
            return {"action": "escalate_to_user", "error": error}

        category = classify_error(error, tool_name)

        if category == ErrorCategory.TRANSIENT:
            delay = min(2 ** retry_count, 30)
            self._emit({
                "type": "status",
                "message": f"Transient error, retrying in {delay}s ({retry_count + 1}/{max_retries})...",
            })
            await asyncio.sleep(delay)
            return {"action": "retry"}

        if category == ErrorCategory.TOOL_FAILURE:
            if call_tool_fn:
                self._emit({
                    "type": "status",
                    "message": f"Tool error on {tool_name}, attempting undo and retry...",
                })
                await call_tool_fn("dsengine_editor_undo", {})
            return {"action": "retry"}

        if category == ErrorCategory.STATE_STALE:
            if call_tool_fn:
                self._emit({
                    "type": "status",
                    "message": "Scene state stale, refreshing...",
                })
                scene = await call_tool_fn("dsengine_scene_get_state", {})
                return {"action": "retry", "updated_context": scene}
            return {"action": "retry"}

        if category == ErrorCategory.LLM_ERROR:
            self._emit({
                "type": "status",
                "message": f"LLM output error, retrying ({retry_count + 1}/{max_retries})...",
            })
            return {"action": "retry"}

        # FATAL
        return {"action": "escalate_to_user", "error": error}
