"""
Agent Context Manager for DSEngine Editor.

Manages token budgets, conversation history compression,
and scene diff generation for efficient LLM context usage.
"""

import json
import os
from typing import Optional


def estimate_tokens(text: str) -> int:
    """Rough token estimate: ~4 chars per token for mixed CJK/English."""
    return max(1, len(text) // 4)


def estimate_tokens_messages(messages: list[dict]) -> int:
    """Estimate total tokens for a message list."""
    total = 0
    for msg in messages:
        total += estimate_tokens(msg.get("content", ""))
        total += 4  # role/structure overhead
    return total


class ContextManager:
    """Manages Agent execution context within LLM token limits."""

    def __init__(self, max_context_tokens: int = 128000):
        self.max_context_tokens = max_context_tokens
        self.reserve_tokens = 4096  # reserved for LLM output

    def build_task_context(self, system_prompt: str, user_goal: str,
                           task_description: str,
                           scene_state: Optional[str] = None,
                           previous_results: Optional[list[str]] = None,
                           conversation_history: Optional[list[dict]] = None) -> list[dict]:
        """Build optimized context for a task execution.

        Priority (highest first):
        1. System prompt (specialist) - always included
        2. Current task description - always included
        3. Scene state - always included if available
        4. Previous task results (latest first, truncated)
        5. Conversation history (compressed if needed)
        """
        budget = self.max_context_tokens - self.reserve_tokens
        messages: list[dict] = []

        # 1. System prompt
        messages.append({"role": "system", "content": system_prompt})
        budget -= estimate_tokens(system_prompt)

        # 2. Scene state context
        if scene_state:
            scene_msg = f"[Current Scene State]\n{scene_state}"
            scene_tokens = estimate_tokens(scene_msg)
            if scene_tokens < budget:
                messages.append({"role": "system", "content": scene_msg})
                budget -= scene_tokens

        # 3. Previous task results (latest N that fit)
        if previous_results:
            prev_block = "[Previous Task Results]\n"
            for result in reversed(previous_results):
                truncated = result[:500] if len(result) > 500 else result
                candidate = prev_block + truncated + "\n---\n"
                if estimate_tokens(candidate) < budget * 0.3:  # max 30% of budget
                    prev_block = candidate
                else:
                    break
            if prev_block != "[Previous Task Results]\n":
                messages.append({"role": "system", "content": prev_block})
                budget -= estimate_tokens(prev_block)

        # 4. User goal + task description
        task_msg = f"Overall goal: {user_goal}\n\nCurrent task: {task_description}"
        messages.append({"role": "user", "content": task_msg})
        budget -= estimate_tokens(task_msg)

        return messages

    async def compress_history(self, messages: list[dict], max_tokens: int,
                               llm_invoke=None) -> list[dict]:
        """Compress conversation history to fit within token budget.

        Strategy:
        - Keep the most recent 3 rounds (6 messages) intact
        - Summarize older messages with LLM if available
        - Truncate tool results to 500 characters
        """
        if estimate_tokens_messages(messages) <= max_tokens:
            return messages

        # Truncate tool results first
        compressed = []
        for msg in messages:
            content = msg.get("content", "")
            if msg.get("role") == "tool" and len(content) > 500:
                msg = {**msg, "content": content[:500] + "...(truncated)"}
            compressed.append(msg)

        if estimate_tokens_messages(compressed) <= max_tokens:
            return compressed

        # Keep recent 3 rounds, summarize older
        recent = compressed[-6:]
        older = compressed[:-6]

        if not older:
            return compressed

        if llm_invoke:
            # Use LLM to generate summary
            summary_prompt = "Summarize these conversation messages concisely:\n"
            for msg in older:
                role = msg.get("role", "unknown")
                content = msg.get("content", "")[:200]
                summary_prompt += f"[{role}]: {content}\n"

            summary = await llm_invoke(summary_prompt)
            return [
                {"role": "system", "content": f"[Previous conversation summary]\n{summary}"},
                *recent,
            ]
        else:
            # No LLM available: just keep recent messages
            return recent


def compute_scene_diff(before: dict, after: dict) -> str:
    """Compute a human-readable diff between two scene states.

    Returns a concise description of what changed.
    """
    if not before and not after:
        return "No scene data"
    if not before:
        return "Scene created from empty"
    if not after:
        return "Scene cleared"

    before_entities = {e.get("name", str(e.get("id", "?"))): e
                       for e in before.get("entities", [])}
    after_entities = {e.get("name", str(e.get("id", "?"))): e
                      for e in after.get("entities", [])}

    added = set(after_entities.keys()) - set(before_entities.keys())
    removed = set(before_entities.keys()) - set(after_entities.keys())
    common = set(before_entities.keys()) & set(after_entities.keys())

    changes = []

    if added:
        changes.append(f"Added entities: {', '.join(sorted(added))}")
    if removed:
        changes.append(f"Removed entities: {', '.join(sorted(removed))}")

    modified = []
    for name in common:
        b = json.dumps(before_entities[name], sort_keys=True)
        a = json.dumps(after_entities[name], sort_keys=True)
        if b != a:
            modified.append(name)

    if modified:
        changes.append(f"Modified entities: {', '.join(sorted(modified))}")

    if not changes:
        return "No changes detected"

    return "; ".join(changes)
