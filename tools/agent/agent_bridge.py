#!/usr/bin/env python3
"""
Agent Bridge for DSEngine Editor (v2.0).

Unified entry point replacing ai_chat_bridge.py.
All messages go through the Agent state machine:
- Simple requests -> classify -> direct (same behavior as old Chat)
- Complex tasks -> classify -> checkpoint -> plan -> approve -> execute -> verify -> summarize

Stdin/stdout JSON-lines protocol (superset of old ai_chat_bridge.py):
  Editor -> Bridge:
    {"type": "user_message", "content": "...", "force_agent_mode": false, "constraints": [...]}
    {"type": "agent_approve", "status": "approved|rejected|modified", "feedback": "..."}
    {"type": "agent_rollback"}
    {"type": "cancel"}
    {"type": "clear_history"}
    {"type": "tool_result", "call_id": "...", "result": "..."}
  Bridge -> Editor:
    (all original stream_start/stream_chunk/stream_end/tool_call/error/status/token_usage)
    {"type": "agent_plan", "plan": [...], "task_count": N}
    {"type": "agent_task_status", "task_id": "T1", "status": "running|done|failed|retrying"}
    {"type": "agent_complete", "summary": {...}}
    {"type": "agent_checkpoint_created", "path": "..."}
    {"type": "agent_safety_blocked", "tool": "...", "reason": "..."}

Environment:
  OPENAI_API_KEY     - Required
  OPENAI_BASE_URL    - Optional (for compatible APIs like DeepSeek)
  OPENAI_MODEL       - Optional (default: gpt-4o)
  HTTP_PROXY         - Optional proxy URL
"""

import asyncio
import json
import os
import sys
import threading
from pathlib import Path

# Add parent dir to path so we can import agent modules
sys.path.insert(0, str(Path(__file__).parent.parent))

try:
    import openai
except ImportError:
    sys.stderr.write("openai package not installed. Run: pip install openai\n")
    sys.exit(1)

from agent.agent_orchestrator import (
    run_agent, set_tool_call_fn, direct_node, classify_node,
    AgentState, _emit, _load_tool_schemas,
)
from agent.agent_specialists import GENERAL_PROMPT

# ─── Tool Call Routing ───────────────────────────────────────────────────────

# Pending tool results from C++ (tool_call_id -> asyncio.Future)
_pending_tool_results: dict[str, asyncio.Future] = {}
_tool_call_counter = 0


async def call_tool_via_cpp(tool_name: str, args: dict) -> dict:
    """Execute a tool call by sending it to C++ via JSON-lines and waiting for result.

    This is the same mechanism used by the original ai_chat_bridge.py:
    1. Bridge emits {"type": "tool_call", ...} to stdout
    2. C++ executes via ControlServer::DispatchTool (same process)
    3. C++ sends back {"type": "tool_result", "call_id": ..., "result": ...} to stdin
    """
    global _tool_call_counter
    _tool_call_counter += 1
    call_id = f"agent_tc_{_tool_call_counter}"

    # Create future for this tool call
    loop = asyncio.get_running_loop()
    future = loop.create_future()
    _pending_tool_results[call_id] = future

    # Emit tool call to C++
    _emit({
        "type": "tool_call",
        "name": tool_name,
        "arguments": json.dumps(args, ensure_ascii=False),
        "call_id": call_id,
    })

    # Wait for C++ to send back the result
    try:
        result_str = await asyncio.wait_for(future, timeout=30.0)
    except asyncio.TimeoutError:
        _pending_tool_results.pop(call_id, None)
        return {"error": f"Tool call timeout: {tool_name}"}
    finally:
        _pending_tool_results.pop(call_id, None)

    # Parse result
    try:
        return json.loads(result_str)
    except json.JSONDecodeError:
        return {"result": result_str}


# Register the tool call handler with the orchestrator
set_tool_call_fn(call_tool_via_cpp)

# ─── Session State ───────────────────────────────────────────────────────────

_conversation_history: list[dict] = []
_active_agent_state: dict = {}


# ─── Message Routing ─────────────────────────────────────────────────────────

async def route_message(msg: dict, stdin_queue: asyncio.Queue):
    """Unified message router. All messages go through Agent state machine."""
    global _conversation_history, _active_agent_state

    msg_type = msg.get("type", "")

    if msg_type == "user_message":
        content = msg.get("content", "")
        force_agent = msg.get("force_agent_mode", False)
        constraints = msg.get("constraints", [])

        # Run through Agent state machine
        # classify node will determine direct vs full mode
        result = await run_agent(
            user_goal=content,
            constraints=constraints,
            force_agent_mode=force_agent,
            approval_status="approved",  # auto-approve for now; HITL comes in Phase 2
        )
        _active_agent_state = result if isinstance(result, dict) else {}

    elif msg_type == "agent_approve":
        status = msg.get("status", "approved")
        feedback = msg.get("feedback", "")

        if _active_agent_state:
            _active_agent_state["approval_status"] = status
            _active_agent_state["approval_feedback"] = feedback
            # Re-run from approve node
            result = await run_agent(
                user_goal=_active_agent_state.get("user_goal", ""),
                constraints=_active_agent_state.get("user_constraints", []),
                force_agent_mode=True,
                approval_status=status,
            )
            _active_agent_state = result if isinstance(result, dict) else {}

    elif msg_type == "agent_rollback":
        checkpoint_path = _active_agent_state.get("scene_checkpoint_path", "")
        if checkpoint_path:
            result = await call_tool_via_cpp("dsengine_scene_load", {"path": checkpoint_path})
            _emit({
                "type": "status",
                "message": f"Scene rolled back to checkpoint: {checkpoint_path}",
            })
        else:
            _emit({"type": "error", "message": "No checkpoint available for rollback"})


# ─── Async Main ──────────────────────────────────────────────────────────────

async def main():
    model = os.environ.get("OPENAI_MODEL", "gpt-4o")
    _emit({"type": "status", "message": f"Agent bridge ready ({model}). Unified mode v2.0."})

    # Load tool schemas to verify they're available
    schemas = _load_tool_schemas()
    if schemas:
        _emit({"type": "status", "message": f"Loaded {len(schemas)} tool definitions"})
    else:
        _emit({"type": "status", "message": "No tool schemas loaded (dsengine_mcp.py not found)"})

    # Bridge stdin to asyncio queue
    loop = asyncio.get_running_loop()
    stdin_queue = asyncio.Queue()

    def stdin_reader():
        for line in sys.stdin:
            asyncio.run_coroutine_threadsafe(stdin_queue.put(line), loop)

    threading.Thread(target=stdin_reader, daemon=True).start()

    active_task = None

    while True:
        try:
            # Poll queue: short timeout if task active, else block
            if active_task and not active_task.done():
                try:
                    line = await asyncio.wait_for(stdin_queue.get(), timeout=0.05)
                except asyncio.TimeoutError:
                    continue
            else:
                line = await stdin_queue.get()

            if not line:
                break

            try:
                msg = json.loads(line.strip())
            except json.JSONDecodeError:
                continue

            msg_type = msg.get("type", "")

            # Cancel: immediately cancel active task
            if msg_type == "cancel":
                if active_task and not active_task.done():
                    active_task.cancel()
                    try:
                        await active_task
                    except asyncio.CancelledError:
                        pass
                    _emit({"type": "status", "message": "Generation cancelled."})
                active_task = None
                continue

            # Clear history
            if msg_type == "clear_history":
                _conversation_history.clear()
                continue

            # Tool result: resolve pending future
            if msg_type == "tool_result":
                call_id = msg.get("call_id", "")
                result = msg.get("result", "{}")
                future = _pending_tool_results.get(call_id)
                if future and not future.done():
                    future.set_result(result)
                else:
                    # Re-queue for other consumers
                    await stdin_queue.put(line)
                continue

            # User message or agent commands: route through state machine
            if msg_type in ("user_message", "agent_approve", "agent_rollback"):
                if active_task and not active_task.done():
                    await active_task
                active_task = asyncio.create_task(route_message(msg, stdin_queue))
                continue

        except Exception as e:
            _emit({"type": "error", "message": str(e)})


if __name__ == "__main__":
    asyncio.run(main())
