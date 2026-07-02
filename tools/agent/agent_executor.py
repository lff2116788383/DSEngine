"""
Agent Task Executor for DSEngine Editor.

Executes individual sub-tasks using a ReAct loop:
LLM thinks -> tool call -> observe result -> repeat until done.
"""

import json
import os
import sys
import time
from typing import Optional

try:
    import openai
except ImportError:
    openai = None

from .agent_safety import AgentSafetyPolicy
from .agent_specialists import SPECIALIST_PROMPTS, GENERAL_PROMPT
from .agent_context import ContextManager, compute_scene_diff


async def execute_task(task: dict, user_goal: str,
                       tool_schemas: list[dict],
                       call_tool_fn,
                       emit_fn=None,
                       scene_state: Optional[str] = None,
                       previous_results: Optional[list[str]] = None,
                       max_iterations: int = 15) -> dict:
    """Execute a single sub-task using ReAct loop.

    Args:
        task: Task dict with id, title, description, specialist
        user_goal: The overall user goal
        tool_schemas: OpenAI function calling tool schemas
        call_tool_fn: async fn(tool_name, args) -> result dict
        emit_fn: Function to emit status messages
        scene_state: Current scene state JSON string
        previous_results: Results from previously completed tasks
        max_iterations: Max tool call iterations

    Returns:
        dict with: result, tools_used, tokens_used, scene_before, scene_after
    """
    if openai is None:
        raise RuntimeError("openai package not installed")

    def emit(obj):
        if emit_fn:
            emit_fn(obj)
        else:
            sys.stdout.write(json.dumps(obj, ensure_ascii=False) + "\n")
            sys.stdout.flush()

    api_key = os.environ.get("OPENAI_API_KEY", "")
    if not api_key:
        raise RuntimeError("OPENAI_API_KEY not set")

    client = openai.AsyncOpenAI(
        api_key=api_key,
        base_url=os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1"),
        timeout=float(os.environ.get("OPENAI_TIMEOUT_MS", "60000")) / 1000.0,
    )

    model = os.environ.get("OPENAI_MODEL", "gpt-4o")

    # Get specialist prompt
    specialist = task.get("specialist", "scene_architect")
    system_prompt = SPECIALIST_PROMPTS.get(specialist, GENERAL_PROMPT)

    # Build context
    ctx = ContextManager()
    messages = ctx.build_task_context(
        system_prompt=system_prompt,
        user_goal=user_goal,
        task_description=f"{task['title']}: {task['description']}",
        scene_state=scene_state,
        previous_results=previous_results,
    )

    # Snapshot scene before execution
    scene_before = None
    try:
        scene_before = await call_tool_fn("dsengine_scene_get_state", {})
    except Exception:
        pass

    tools_used = []
    total_tokens = 0
    tool_errors = []

    emit({
        "type": "agent_task_status",
        "task_id": task["id"],
        "status": "running",
    })

    start_time = time.time()

    for iteration in range(max_iterations):
        try:
            response = await client.chat.completions.create(
                model=model,
                messages=messages,
                tools=tool_schemas if tool_schemas else None,
                temperature=0.2,
                max_tokens=2048,
            )
        except Exception as e:
            tool_errors.append(str(e))
            emit({"type": "error", "message": f"LLM error in task {task['id']}: {e}"})
            break

        if response.usage:
            total_tokens += (response.usage.prompt_tokens or 0) + (response.usage.completion_tokens or 0)

        choice = response.choices[0]
        message = choice.message

        # Append assistant message to context
        msg_dict = {"role": "assistant", "content": message.content or ""}
        if message.tool_calls:
            msg_dict["tool_calls"] = [
                {
                    "id": tc.id,
                    "type": "function",
                    "function": {
                        "name": tc.function.name,
                        "arguments": tc.function.arguments,
                    },
                }
                for tc in message.tool_calls
            ]
        messages.append(msg_dict)

        # No tool calls -> task complete
        if not message.tool_calls:
            if message.content:
                emit({"type": "stream_chunk", "content": message.content, "chunk_id": 0, "is_last": True})
            break

        # Execute tool calls
        for tc in message.tool_calls:
            tool_name = tc.function.name
            try:
                tool_args = json.loads(tc.function.arguments)
            except json.JSONDecodeError:
                tool_args = {}

            emit({
                "type": "agent_tool_call",
                "task_id": task["id"],
                "tool": tool_name,
                "iteration": iteration,
            })

            # Safety check
            safety = AgentSafetyPolicy.check_tool_call(tool_name, tool_args)
            if not safety.allowed:
                result = {"error": f"Safety blocked: {safety.reason}"}
                tool_errors.append(safety.reason)
                emit({
                    "type": "agent_safety_blocked",
                    "tool": tool_name,
                    "reason": safety.reason,
                })
            else:
                try:
                    result = await call_tool_fn(tool_name, tool_args)
                except Exception as e:
                    result = {"error": str(e)}
                    tool_errors.append(str(e))

            tools_used.append(tool_name)

            # Append tool result to context
            result_str = json.dumps(result, ensure_ascii=False) if isinstance(result, dict) else str(result)
            messages.append({
                "role": "tool",
                "tool_call_id": tc.id,
                "content": result_str[:2000],  # truncate large results
            })

    # Snapshot scene after execution
    scene_after = None
    try:
        scene_after = await call_tool_fn("dsengine_scene_get_state", {})
    except Exception:
        pass

    elapsed_ms = (time.time() - start_time) * 1000

    # Build result summary
    final_content = ""
    for msg in reversed(messages):
        if msg.get("role") == "assistant" and msg.get("content"):
            final_content = msg["content"]
            break

    return {
        "result": final_content,
        "tools_used": tools_used,
        "tokens_used": total_tokens,
        "duration_ms": elapsed_ms,
        "tool_errors": tool_errors,
        "scene_before": scene_before,
        "scene_after": scene_after,
        "scene_diff": compute_scene_diff(
            scene_before if isinstance(scene_before, dict) else {},
            scene_after if isinstance(scene_after, dict) else {},
        ),
    }
