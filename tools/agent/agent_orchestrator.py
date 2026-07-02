"""
Agent Orchestrator for DSEngine Editor.

LangGraph-based state machine that coordinates the full Agent workflow:
classify -> direct/checkpoint -> plan -> approve -> execute -> verify -> summarize

This is the core of the Agent architecture (v2.0).
"""

import json
import os
import sys
import time
import uuid
from typing import Optional

try:
    from langgraph.graph import StateGraph, END
    from langgraph.checkpoint.sqlite import SqliteSaver
    HAS_LANGGRAPH = True
except ImportError:
    HAS_LANGGRAPH = False

try:
    import openai
except ImportError:
    openai = None

from .agent_planner import plan_tasks
from .agent_executor import execute_task
from .agent_specialists import SPECIALIST_PROMPTS, GENERAL_PROMPT
from .agent_safety import AgentSafetyPolicy
from .agent_audit import AuditLog
from .agent_context import compute_scene_diff
from .agent_recovery import RecoveryManager, classify_error, ErrorCategory

# ─── Tool Schema Builder ─────────────────────────────────────────────────────

# Tool definitions imported from dsengine_mcp.py's MCP_TOOLS
_tool_schemas_cache: Optional[list[dict]] = None


def _load_tool_schemas() -> list[dict]:
    """Load tool schemas from dsengine_mcp.py MCP_TOOLS definition."""
    global _tool_schemas_cache
    if _tool_schemas_cache is not None:
        return _tool_schemas_cache

    try:
        from pathlib import Path
        mcp_path = Path(__file__).parent.parent / "mcp_adapter" / "dsengine_mcp.py"
        if mcp_path.exists():
            import importlib.util
            spec = importlib.util.spec_from_file_location("dsengine_mcp", str(mcp_path))
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            mcp_tools = getattr(mod, "MCP_TOOLS", [])

            schemas = []
            for tool in mcp_tools:
                name = tool.get("name", "")
                # Filter out blocked tools
                if name in AgentSafetyPolicy.BLOCKED_TOOLS:
                    continue
                schemas.append({
                    "type": "function",
                    "function": {
                        "name": name,
                        "description": tool.get("description", ""),
                        "parameters": tool.get("inputSchema", {"type": "object", "properties": {}}),
                    },
                })
            _tool_schemas_cache = schemas
            return schemas
    except Exception as e:
        sys.stderr.write(f"[Agent] Failed to load tool schemas: {e}\n")

    _tool_schemas_cache = []
    return []


# ─── Agent State ──────────────────────────────────────────────────────────────

class AgentState(dict):
    """TypedDict-like state for the Agent state machine.

    Keys:
        session_id: str
        user_goal: str
        user_constraints: list[str]
        force_agent_mode: bool
        execution_mode: str  # "direct" | "full"
        messages: list[dict]
        task_plan: list[dict]
        current_task_index: int
        total_tokens: int
        error_count: int
        scene_checkpoint_path: str
        approval_status: str  # "" | "approved" | "rejected" | "modified"
        approval_feedback: str
        max_retries: int
    """
    pass


# ─── Emit Helper ──────────────────────────────────────────────────────────────

def _emit(obj: dict):
    """Write JSON-line to stdout for C++ to consume."""
    sys.stdout.write(json.dumps(obj, ensure_ascii=False) + "\n")
    sys.stdout.flush()


# ─── Tool Call Helper ─────────────────────────────────────────────────────────

# This will be set by agent_bridge.py to route tool calls through C++
_tool_call_fn = None


def set_tool_call_fn(fn):
    """Set the function used to execute tool calls via JSON-lines to C++."""
    global _tool_call_fn
    _tool_call_fn = fn


async def call_tool(name: str, args: dict) -> dict:
    """Execute a tool call through the configured handler."""
    if _tool_call_fn:
        return await _tool_call_fn(name, args)
    return {"error": "Tool call handler not configured"}


# ─── State Machine Nodes ─────────────────────────────────────────────────────

import re

COMPLEX_TASK_PATTERNS = [
    r"做一个.+游戏",
    r"创建完整.+场景",
    r"实现.+功能",
    r"(make|create|build)\s+a\s+.+(game|level|scene|system)",
    r"(完整|完全|全部)(实现|创建|搭建)",
    r"(设计|构建).+(关卡|世界|环境)",
    r"搭建.+场景",
    r"制作.+demo",
]


def classify_node(state: AgentState) -> dict:
    """Rule-based complexity classification. Zero LLM cost, zero latency."""
    goal = state.get("user_goal", "")

    for pattern in COMPLEX_TASK_PATTERNS:
        if re.search(pattern, goal, re.IGNORECASE):
            return {"execution_mode": "full"}

    if state.get("force_agent_mode", False):
        return {"execution_mode": "full"}

    return {"execution_mode": "direct"}


def route_by_complexity(state: AgentState) -> str:
    if state.get("execution_mode") == "full":
        return "checkpoint"
    return "direct"


async def direct_node(state: AgentState) -> dict:
    """Fast path for simple requests. Equivalent to original ai_chat_bridge.py behavior."""
    if openai is None:
        _emit({"type": "error", "message": "openai package not installed"})
        return state

    api_key = os.environ.get("OPENAI_API_KEY", "")
    if not api_key:
        _emit({"type": "error", "message": "OPENAI_API_KEY not set"})
        return state

    client = openai.AsyncOpenAI(
        api_key=api_key,
        base_url=os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1"),
        timeout=float(os.environ.get("OPENAI_TIMEOUT_MS", "30000")) / 1000.0,
    )

    model = os.environ.get("OPENAI_MODEL", "gpt-4o")
    temperature = float(os.environ.get("OPENAI_TEMPERATURE", "0.7"))
    max_tokens = int(os.environ.get("OPENAI_MAX_TOKENS", "4096"))

    tool_schemas = _load_tool_schemas()
    messages = state.get("messages", [])
    messages.append({"role": "user", "content": state["user_goal"]})

    system_msg = {"role": "system", "content": GENERAL_PROMPT}
    api_messages = [system_msg] + messages

    total_tokens = 0

    _emit({"type": "stream_start"})
    chunk_id = 0

    try:
        for round_idx in range(10):
            full_content = ""
            tc_acc: dict[int, dict] = {}
            finish_reason = None

            stream = await client.chat.completions.create(
                model=model,
                messages=api_messages,
                tools=tool_schemas if tool_schemas else None,
                temperature=temperature,
                max_tokens=max_tokens,
                stream=True,
                stream_options={"include_usage": True},
            )

            async for chunk in stream:
                if chunk.usage:
                    total_tokens += getattr(chunk.usage, "prompt_tokens", 0)
                    total_tokens += getattr(chunk.usage, "completion_tokens", 0)

                if not chunk.choices:
                    continue

                choice = chunk.choices[0]
                if choice.finish_reason:
                    finish_reason = choice.finish_reason

                delta = choice.delta

                if delta.content:
                    full_content += delta.content
                    _emit({"type": "stream_chunk", "content": delta.content,
                           "chunk_id": chunk_id, "is_last": False})
                    chunk_id += 1

                if delta.tool_calls:
                    for tc_delta in delta.tool_calls:
                        idx = tc_delta.index
                        if idx not in tc_acc:
                            tc_acc[idx] = {"id": "", "name": "", "args": ""}
                        if tc_delta.id:
                            tc_acc[idx]["id"] += tc_delta.id
                        if tc_delta.function and tc_delta.function.name:
                            tc_acc[idx]["name"] += tc_delta.function.name
                        if tc_delta.function and tc_delta.function.arguments:
                            tc_acc[idx]["args"] += tc_delta.function.arguments

            if finish_reason == "tool_calls":
                openai_tcs = [
                    {"id": tc_acc[i]["id"], "type": "function",
                     "function": {"name": tc_acc[i]["name"], "arguments": tc_acc[i]["args"]}}
                    for i in sorted(tc_acc)
                ]
                api_messages.append({
                    "role": "assistant",
                    "content": full_content or None,
                    "tool_calls": openai_tcs,
                })

                for tc in openai_tcs:
                    tool_name = tc["function"]["name"]
                    try:
                        tool_args = json.loads(tc["function"]["arguments"])
                    except json.JSONDecodeError:
                        tool_args = {}

                    _emit({
                        "type": "tool_call",
                        "name": tool_name,
                        "arguments": tc["function"]["arguments"],
                        "call_id": tc["id"],
                    })

                    # Safety check
                    safety = AgentSafetyPolicy.check_tool_call(tool_name, tool_args)
                    if not safety.allowed:
                        result_str = json.dumps({"error": f"Safety blocked: {safety.reason}"})
                    else:
                        result = await call_tool(tool_name, tool_args)
                        result_str = json.dumps(result, ensure_ascii=False) if isinstance(result, dict) else str(result)

                    api_messages.append({
                        "role": "tool",
                        "tool_call_id": tc["id"],
                        "content": result_str,
                    })
                continue

            # No tool calls -> done
            messages.append({"role": "assistant", "content": full_content})
            break

        if total_tokens > 0:
            _emit({"type": "token_usage",
                   "input_tokens": total_tokens // 2,
                   "output_tokens": total_tokens - total_tokens // 2,
                   "model": model})

        _emit({"type": "stream_chunk", "content": "", "chunk_id": chunk_id, "is_last": True})
        _emit({"type": "stream_end", "chunk_id": chunk_id})

    except Exception as e:
        _emit({"type": "error", "message": str(e)})

    return {
        **state,
        "messages": messages,
        "total_tokens": state.get("total_tokens", 0) + total_tokens,
    }


async def checkpoint_node(state: AgentState) -> dict:
    """Save scene snapshot before Agent execution for one-click rollback."""
    session_id = state.get("session_id", "default")
    checkpoint_path = f"_agent_checkpoint_{session_id}.dscene"

    try:
        await call_tool("dsengine_scene_save", {"path": checkpoint_path})
        _emit({
            "type": "agent_checkpoint_created",
            "path": checkpoint_path,
            "message": "Scene checkpoint saved for rollback",
        })
    except Exception as e:
        _emit({"type": "status", "message": f"Checkpoint save failed: {e}"})
        checkpoint_path = ""

    return {**state, "scene_checkpoint_path": checkpoint_path}


async def plan_node(state: AgentState) -> dict:
    """Decompose user goal into sub-tasks."""
    audit = AuditLog()
    start = time.time()

    try:
        scene_result = await call_tool("dsengine_scene_get_state", {})
        scene_state = json.dumps(scene_result, ensure_ascii=False) if isinstance(scene_result, dict) else str(scene_result)
    except Exception:
        scene_state = None

    try:
        tasks, tokens = await plan_tasks(
            user_goal=state["user_goal"],
            constraints=state.get("user_constraints", []),
            scene_state=scene_state,
            emit_fn=_emit,
        )
    except Exception as e:
        _emit({"type": "error", "message": f"Planning failed: {e}"})
        return {**state, "task_plan": [], "error_count": state.get("error_count", 0) + 1}

    elapsed = (time.time() - start) * 1000
    audit.log_event(
        session_id=state["session_id"],
        event_type="plan",
        node_name="plan",
        input_data={"goal": state["user_goal"]},
        output_data={"task_count": len(tasks)},
        tokens_used=tokens,
        duration_ms=elapsed,
    )
    audit.close()

    # Emit plan for C++ UI
    _emit({
        "type": "agent_plan",
        "plan": [
            {
                "id": t["id"],
                "title": t["title"],
                "description": t["description"],
                "specialist": t["specialist"],
                "dependencies": t["dependencies"],
                "estimated_tools": t["estimated_tools"],
            }
            for t in tasks
        ],
        "task_count": len(tasks),
        "estimated_tools": sum(t["estimated_tools"] for t in tasks),
    })

    return {
        **state,
        "task_plan": tasks,
        "current_task_index": 0,
        "total_tokens": state.get("total_tokens", 0) + tokens,
    }


def route_after_plan(state: AgentState) -> str:
    if not state.get("task_plan"):
        return END
    return "approve"


async def approve_node(state: AgentState) -> dict:
    """Wait for user approval (HITL). Uses LangGraph interrupt_before."""
    # The actual interrupt is handled by LangGraph's interrupt_before mechanism
    # or by the bridge polling for approval messages.
    # This node just checks the approval status set by the bridge.
    status = state.get("approval_status", "")
    if status == "approved":
        _emit({"type": "status", "message": "Plan approved, starting execution..."})
    elif status == "rejected":
        _emit({"type": "status", "message": "Plan rejected by user."})
    elif status == "modified":
        feedback = state.get("approval_feedback", "")
        _emit({"type": "status", "message": f"Plan modification requested: {feedback}"})
    return state


def route_after_approve(state: AgentState) -> str:
    status = state.get("approval_status", "")
    if status == "approved":
        return "execute"
    if status == "modified":
        return "plan"  # re-plan with feedback
    return END  # rejected


async def execute_node(state: AgentState) -> dict:
    """Execute the current sub-task."""
    idx = state.get("current_task_index", 0)
    tasks = state.get("task_plan", [])

    if idx >= len(tasks):
        return state

    task = tasks[idx]
    tool_schemas = _load_tool_schemas()
    audit = AuditLog()

    # Collect results from completed tasks
    previous_results = []
    for t in tasks[:idx]:
        if t.get("status") == "done" and t.get("result"):
            previous_results.append(f"{t['title']}: {t['result']}")

    try:
        result = await execute_task(
            task=task,
            user_goal=state["user_goal"],
            tool_schemas=tool_schemas,
            call_tool_fn=call_tool,
            emit_fn=_emit,
            previous_results=previous_results,
            max_iterations=15,
        )

        task["result"] = result.get("result", "")
        task["tools_used"] = result.get("tools_used", [])
        task["tokens_used"] = result.get("tokens_used", 0)

        audit.log_event(
            session_id=state["session_id"],
            event_type="execute",
            node_name="execute",
            task_id=task["id"],
            input_data={"title": task["title"]},
            output_data={"tools_used": len(task["tools_used"])},
            tokens_used=task["tokens_used"],
            duration_ms=result.get("duration_ms", 0),
            specialist=task.get("specialist"),
        )

    except Exception as e:
        task["error"] = str(e)
        task["status"] = "failed"
        _emit({
            "type": "agent_task_status",
            "task_id": task["id"],
            "status": "failed",
            "error": str(e),
        })
        audit.log_event(
            session_id=state["session_id"],
            event_type="error",
            node_name="execute",
            task_id=task["id"],
            error=str(e),
            specialist=task.get("specialist"),
        )

    audit.close()

    return {
        **state,
        "task_plan": tasks,
        "total_tokens": state.get("total_tokens", 0) + task.get("tokens_used", 0),
    }


async def verify_node(state: AgentState) -> dict:
    """Two-level verification: deterministic first, LLM only if needed."""
    idx = state.get("current_task_index", 0)
    tasks = state.get("task_plan", [])

    if idx >= len(tasks):
        return state

    task = tasks[idx]

    # Skip verification for already failed tasks
    if task.get("status") == "failed":
        return {
            **state,
            "task_plan": tasks,
            "current_task_index": idx + 1,
            "error_count": state.get("error_count", 0) + 1,
        }

    # Level 1: Deterministic verification
    has_errors = bool(task.get("error")) or any(
        "error" in str(r).lower() for r in task.get("tool_errors", [])
    )
    tools_were_used = len(task.get("tools_used", [])) > 0

    if not has_errors and tools_were_used and task.get("specialist") != "qa_tester":
        task["status"] = "done"
        _emit({
            "type": "agent_task_status",
            "task_id": task["id"],
            "status": "done",
            "result": task.get("result", ""),
            "verification": "deterministic",
        })
        return {
            **state,
            "task_plan": tasks,
            "current_task_index": idx + 1,
        }

    # Level 2: LLM verification (only for qa_tester or when deterministic fails)
    if openai and os.environ.get("OPENAI_API_KEY"):
        try:
            client = openai.AsyncOpenAI(
                api_key=os.environ.get("OPENAI_API_KEY", ""),
                base_url=os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1"),
            )
            model = os.environ.get("OPENAI_MODEL", "gpt-4o")

            prompt = f"""Verify if this task was completed successfully:
Task: {task['title']}
Description: {task['description']}
Result: {task.get('result', 'N/A')}
Tools used: {task.get('tools_used', [])}
Errors: {has_errors}

Reply with JSON: {{"completed": true/false, "reason": "..."}}"""

            response = await client.chat.completions.create(
                model=model,
                messages=[{"role": "user", "content": prompt}],
                temperature=0,
                max_tokens=256,
            )

            content = response.choices[0].message.content or ""
            tokens = 0
            if response.usage:
                tokens = (response.usage.prompt_tokens or 0) + (response.usage.completion_tokens or 0)

            # Parse verdict
            try:
                # Handle markdown fences
                json_str = content.strip()
                if json_str.startswith("```"):
                    lines = json_str.split("\n")
                    lines = [l for l in lines if not l.strip().startswith("```")]
                    json_str = "\n".join(lines)
                start_idx = json_str.find("{")
                end_idx = json_str.rfind("}") + 1
                if start_idx >= 0 and end_idx > start_idx:
                    verdict = json.loads(json_str[start_idx:end_idx])
                else:
                    verdict = {"completed": not has_errors}
            except json.JSONDecodeError:
                verdict = {"completed": not has_errors}

            if verdict.get("completed", False):
                task["status"] = "done"
                _emit({
                    "type": "agent_task_status",
                    "task_id": task["id"],
                    "status": "done",
                    "result": task.get("result", ""),
                    "verification": "llm",
                })
                return {
                    **state,
                    "task_plan": tasks,
                    "current_task_index": idx + 1,
                    "total_tokens": state.get("total_tokens", 0) + tokens,
                }

        except Exception:
            pass

    # Verification failed or inconclusive -> retry or mark failed
    task["retry_count"] = task.get("retry_count", 0) + 1
    if task["retry_count"] >= state.get("max_retries", 3):
        task["status"] = "failed"
        _emit({
            "type": "agent_task_status",
            "task_id": task["id"],
            "status": "failed",
            "error": task.get("error", "Verification failed"),
        })
        return {
            **state,
            "task_plan": tasks,
            "current_task_index": idx + 1,
            "error_count": state.get("error_count", 0) + 1,
        }

    _emit({
        "type": "agent_task_status",
        "task_id": task["id"],
        "status": "retrying",
        "retry": task["retry_count"],
    })
    return {**state, "task_plan": tasks}


def route_after_verify(state: AgentState) -> str:
    idx = state.get("current_task_index", 0)
    tasks = state.get("task_plan", [])
    current_task = tasks[idx - 1] if 0 < idx <= len(tasks) else None

    # Retrying current task
    if current_task and current_task.get("status") not in ("done", "failed"):
        return "execute"

    # More tasks remaining
    if idx < len(tasks):
        return "execute"

    return "summarize"


async def summarize_node(state: AgentState) -> dict:
    """Generate final execution report."""
    tasks = state.get("task_plan", [])

    completed = sum(1 for t in tasks if t.get("status") == "done")
    failed = sum(1 for t in tasks if t.get("status") == "failed")
    skipped = sum(1 for t in tasks if t.get("status") == "skipped")

    all_tools = []
    for t in tasks:
        all_tools.extend(t.get("tools_used", []))

    report = {
        "type": "agent_complete",
        "summary": {
            "goal": state.get("user_goal", ""),
            "total_tasks": len(tasks),
            "completed": completed,
            "failed": failed,
            "skipped": skipped,
            "total_tokens": state.get("total_tokens", 0),
            "tools_used": list(set(all_tools)),
            "tasks": [
                {
                    "id": t["id"],
                    "title": t["title"],
                    "status": t.get("status", "unknown"),
                    "result": t.get("result", "")[:200],
                }
                for t in tasks
            ],
        },
        "checkpoint_path": state.get("scene_checkpoint_path", ""),
    }

    # Take final screenshot
    try:
        screenshot = await call_tool("dsengine_editor_screenshot", {"target": "scene"})
        report["final_screenshot"] = screenshot.get("screenshot", "") if isinstance(screenshot, dict) else ""
    except Exception:
        pass

    _emit(report)

    # Log to audit
    audit = AuditLog()
    audit.log_event(
        session_id=state["session_id"],
        event_type="summarize",
        node_name="summarize",
        output_data=report["summary"],
        tokens_used=state.get("total_tokens", 0),
    )
    audit.close()

    return state


# ─── Graph Builder ────────────────────────────────────────────────────────────

def build_agent_graph():
    """Build the LangGraph state machine for Agent orchestration.

    Flow:
        START -> classify -> direct -> END  (simple requests)
        START -> classify -> checkpoint -> plan -> approve -> execute -> verify -> summarize -> END
    """
    if not HAS_LANGGRAPH:
        return None

    graph = StateGraph(AgentState)

    # Add nodes
    graph.add_node("classify", classify_node)
    graph.add_node("direct", direct_node)
    graph.add_node("checkpoint", checkpoint_node)
    graph.add_node("plan", plan_node)
    graph.add_node("approve", approve_node)
    graph.add_node("execute", execute_node)
    graph.add_node("verify", verify_node)
    graph.add_node("summarize", summarize_node)

    # Entry point
    graph.set_entry_point("classify")

    # Conditional edges
    graph.add_conditional_edges("classify", route_by_complexity, {
        "direct": "direct",
        "checkpoint": "checkpoint",
    })

    graph.add_edge("direct", END)
    graph.add_edge("checkpoint", "plan")

    graph.add_conditional_edges("plan", route_after_plan, {
        "approve": "approve",
        END: END,
    })

    graph.add_conditional_edges("approve", route_after_approve, {
        "execute": "execute",
        "plan": "plan",
        END: END,
    })

    graph.add_edge("execute", "verify")

    graph.add_conditional_edges("verify", route_after_verify, {
        "execute": "execute",
        "summarize": "summarize",
    })

    graph.add_edge("summarize", END)

    return graph.compile()


async def run_agent(user_goal: str,
                    constraints: Optional[list[str]] = None,
                    force_agent_mode: bool = False,
                    approval_status: str = "approved") -> dict:
    """Run the Agent state machine.

    This is the main entry point called by agent_bridge.py.
    """
    session_id = str(uuid.uuid4())[:8]

    initial_state = AgentState({
        "session_id": session_id,
        "user_goal": user_goal,
        "user_constraints": constraints or [],
        "force_agent_mode": force_agent_mode,
        "execution_mode": "",
        "messages": [],
        "task_plan": [],
        "current_task_index": 0,
        "total_tokens": 0,
        "error_count": 0,
        "scene_checkpoint_path": "",
        "approval_status": approval_status,
        "approval_feedback": "",
        "max_retries": 3,
    })

    graph = build_agent_graph()
    if graph is None:
        # Fallback: run without LangGraph (direct mode only)
        _emit({"type": "status", "message": "LangGraph not available, using direct mode"})
        classify_result = classify_node(initial_state)
        initial_state.update(classify_result)
        if initial_state["execution_mode"] == "direct":
            return await direct_node(initial_state)
        else:
            _emit({"type": "error", "message": "Agent mode requires LangGraph. Install: pip install langgraph langchain-openai"})
            return initial_state

    # Run the graph
    try:
        # Use SQLite checkpointing for persistence
        project_dir = os.environ.get("DSE_PROJECT_DIR", ".")
        dse_dir = os.path.join(project_dir, ".dse")
        os.makedirs(dse_dir, exist_ok=True)
        db_path = os.path.join(dse_dir, "agent_checkpoints.db")

        with SqliteSaver.from_conn_string(db_path) as checkpointer:
            compiled = build_agent_graph()
            if compiled is None:
                return initial_state
            # Note: compiled graph from build_agent_graph doesn't use checkpointer
            # We rebuild with checkpointer
            graph_builder = StateGraph(AgentState)
            graph_builder.add_node("classify", classify_node)
            graph_builder.add_node("direct", direct_node)
            graph_builder.add_node("checkpoint", checkpoint_node)
            graph_builder.add_node("plan", plan_node)
            graph_builder.add_node("approve", approve_node)
            graph_builder.add_node("execute", execute_node)
            graph_builder.add_node("verify", verify_node)
            graph_builder.add_node("summarize", summarize_node)
            graph_builder.set_entry_point("classify")
            graph_builder.add_conditional_edges("classify", route_by_complexity, {
                "direct": "direct", "checkpoint": "checkpoint",
            })
            graph_builder.add_edge("direct", END)
            graph_builder.add_edge("checkpoint", "plan")
            graph_builder.add_conditional_edges("plan", route_after_plan, {
                "approve": "approve", END: END,
            })
            graph_builder.add_conditional_edges("approve", route_after_approve, {
                "execute": "execute", "plan": "plan", END: END,
            })
            graph_builder.add_edge("execute", "verify")
            graph_builder.add_conditional_edges("verify", route_after_verify, {
                "execute": "execute", "summarize": "summarize",
            })
            graph_builder.add_edge("summarize", END)

            compiled_with_cp = graph_builder.compile(checkpointer=checkpointer)

            config = {"configurable": {"thread_id": session_id}}
            final_state = await compiled_with_cp.ainvoke(initial_state, config=config)
            return final_state

    except Exception as e:
        _emit({"type": "error", "message": f"Agent execution error: {e}"})
        # Fallback to direct mode
        return await direct_node(initial_state)
