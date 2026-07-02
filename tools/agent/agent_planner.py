"""
Agent Task Planner for DSEngine Editor.

Decomposes high-level goals into ordered sub-tasks with specialist assignments
and dependency tracking. Uses LLM with structured output.
"""

import json
import os
import sys
from typing import Optional

try:
    import openai
except ImportError:
    openai = None


PLANNER_SYSTEM_PROMPT = """You are a task planner for the DSEngine game editor.
Your job is to decompose a user's high-level game development goal into concrete,
ordered sub-tasks that can be executed by specialist agents.

Available specialists:
- scene_architect: Creates/modifies entities, sets up scenes, lighting, cameras
- script_writer: Writes Lua scripts for game logic, player controllers, AI
- asset_manager: Imports assets, creates materials, generates textures/models
- qa_tester: Tests gameplay in Play mode, takes screenshots, verifies results
- physics_tuner: Adjusts physics parameters, collision shapes, rigid bodies

Rules:
1. Each task should be achievable with 2-6 tool calls
2. Tasks must have clear, specific descriptions (not vague)
3. Dependencies must form a valid DAG (no cycles)
4. Earlier tasks should not depend on later ones
5. Group related work into single tasks (e.g., "create 5 platforms" = 1 task, not 5)
6. Always end with a qa_tester task to verify the result

Output ONLY valid JSON with this structure:
{
    "tasks": [
        {
            "id": "T1",
            "title": "Short title",
            "description": "Detailed description of what to do",
            "specialist": "scene_architect",
            "dependencies": [],
            "estimated_tools": 3
        }
    ]
}"""


async def plan_tasks(user_goal: str, constraints: list[str],
                     scene_state: Optional[str] = None,
                     emit_fn=None) -> list[dict]:
    """Decompose a user goal into an ordered task plan.

    Args:
        user_goal: The user's high-level goal
        constraints: User-specified constraints
        scene_state: Current scene state JSON string
        emit_fn: Function to emit status messages

    Returns:
        List of task dicts with id, title, description, specialist, dependencies
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

    user_content = f"## Goal\n{user_goal}\n"
    if constraints:
        user_content += f"\n## Constraints\n" + "\n".join(f"- {c}" for c in constraints)
    if scene_state:
        # Truncate scene state to avoid token overflow
        truncated = scene_state[:3000] if len(scene_state) > 3000 else scene_state
        user_content += f"\n\n## Current Scene State\n{truncated}"

    emit({"type": "status", "message": "Planning tasks..."})

    response = await client.chat.completions.create(
        model=model,
        messages=[
            {"role": "system", "content": PLANNER_SYSTEM_PROMPT},
            {"role": "user", "content": user_content},
        ],
        temperature=0.3,
        max_tokens=2048,
    )

    content = response.choices[0].message.content or ""
    tokens_used = 0
    if response.usage:
        tokens_used = (response.usage.prompt_tokens or 0) + (response.usage.completion_tokens or 0)

    # Parse JSON from response (handle markdown code fences)
    json_str = content.strip()
    if json_str.startswith("```"):
        lines = json_str.split("\n")
        # Remove first and last lines (```json and ```)
        lines = [l for l in lines if not l.strip().startswith("```")]
        json_str = "\n".join(lines)

    try:
        plan_data = json.loads(json_str)
    except json.JSONDecodeError:
        # Try to extract JSON from the content
        start = content.find("{")
        end = content.rfind("}") + 1
        if start >= 0 and end > start:
            try:
                plan_data = json.loads(content[start:end])
            except json.JSONDecodeError:
                raise RuntimeError(f"Failed to parse planner output as JSON: {content[:200]}")
        else:
            raise RuntimeError(f"No JSON found in planner output: {content[:200]}")

    tasks = plan_data.get("tasks", [])
    if not tasks:
        raise RuntimeError("Planner returned empty task list")

    # Validate and normalize tasks
    for i, task in enumerate(tasks):
        if "id" not in task:
            task["id"] = f"T{i + 1}"
        if "title" not in task:
            task["title"] = f"Task {i + 1}"
        if "description" not in task:
            task["description"] = task["title"]
        if "specialist" not in task:
            task["specialist"] = "scene_architect"
        if "dependencies" not in task:
            task["dependencies"] = []
        if "estimated_tools" not in task:
            task["estimated_tools"] = 3

        # Add runtime fields
        task["status"] = "pending"
        task["result"] = ""
        task["error"] = ""
        task["retry_count"] = 0
        task["tools_used"] = []
        task["tokens_used"] = 0

    return tasks, tokens_used
