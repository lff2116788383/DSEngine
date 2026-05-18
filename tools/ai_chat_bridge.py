#!/usr/bin/env python3
"""
AI Chat Bridge for DSEngine Editor.

Stdin/stdout JSON-lines protocol:
  Editor -> Bridge:
    {"type": "user_message", "content": "..."}
    {"type": "tool_result", "call_id": "...", "result": "..."}
  Bridge -> Editor:
    {"type": "assistant_message", "content": "..."}
    {"type": "tool_call", "name": "...", "arguments": "...", "call_id": "..."}
    {"type": "error", "message": "..."}
    {"type": "status", "message": "..."}

Environment:
  OPENAI_API_KEY     - Required
  OPENAI_BASE_URL    - Optional (for compatible APIs)
  OPENAI_MODEL       - Optional (default: gpt-4o)
"""

import json
import os
import sys

TOOL_DEFINITIONS = [
    {"type": "function", "function": {"name": "dsengine_ping", "description": "Test connectivity with DSEngine Editor", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "dsengine_scene_get_state", "description": "Get current scene entity list with component summaries", "parameters": {"type": "object", "properties": {"include_components": {"type": "boolean", "default": True}}}}},
    {"type": "function", "function": {"name": "dsengine_entity_create", "description": "Create a new entity. Supported components: MeshRenderer, Camera3D, DirectionalLight, PointLight, SpotLight, RigidBody3D, BoxCollider3D, SphereCollider3D, AudioSource, AudioListener, SkyLight, Skybox, PostProcess.", "parameters": {"type": "object", "properties": {"name": {"type": "string"}, "position": {"type": "array", "items": {"type": "number"}}, "rotation": {"type": "array", "items": {"type": "number"}}, "scale": {"type": "array", "items": {"type": "number"}}, "mesh": {"type": "string"}, "color": {"type": "array", "items": {"type": "number"}}, "components": {"type": "array"}}, "required": ["name"]}}},
    {"type": "function", "function": {"name": "dsengine_entity_delete", "description": "Delete an entity by ID", "parameters": {"type": "object", "properties": {"entity_id": {"type": "integer"}}, "required": ["entity_id"]}}},
    {"type": "function", "function": {"name": "dsengine_entity_modify", "description": "Modify entity name, transform, and/or component properties", "parameters": {"type": "object", "properties": {"entity_id": {"type": "integer"}, "name": {"type": "string"}, "position": {"type": "array"}, "rotation": {"type": "array"}, "scale": {"type": "array"}, "modify_component": {"type": "object"}, "modify_components": {"type": "array"}, "add_components": {"type": "array"}, "remove_components": {"type": "array"}}, "required": ["entity_id"]}}},
    {"type": "function", "function": {"name": "dsengine_entity_add_component", "description": "Add a component to an entity", "parameters": {"type": "object", "properties": {"entity_id": {"type": "integer"}, "type": {"type": "string"}, "properties": {"type": "object"}}, "required": ["entity_id", "type"]}}},
    {"type": "function", "function": {"name": "dsengine_entity_remove_component", "description": "Remove a component from an entity", "parameters": {"type": "object", "properties": {"entity_id": {"type": "integer"}, "type": {"type": "string"}}, "required": ["entity_id", "type"]}}},
    {"type": "function", "function": {"name": "dsengine_entity_get_components", "description": "Get all components on an entity", "parameters": {"type": "object", "properties": {"entity_id": {"type": "integer"}, "detailed": {"type": "boolean", "default": True}}, "required": ["entity_id"]}}},
    {"type": "function", "function": {"name": "dsengine_lua_execute", "description": "Execute Lua code in the engine", "parameters": {"type": "object", "properties": {"code": {"type": "string"}}, "required": ["code"]}}},
    {"type": "function", "function": {"name": "dsengine_script_create", "description": "Create or overwrite a Lua script file", "parameters": {"type": "object", "properties": {"path": {"type": "string"}, "content": {"type": "string"}}, "required": ["path", "content"]}}},
    {"type": "function", "function": {"name": "dsengine_editor_get_state", "description": "Get editor state (mode, entity count, selection)", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "dsengine_editor_play", "description": "Enter Play mode", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "dsengine_editor_stop", "description": "Exit Play mode", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "dsengine_editor_undo", "description": "Undo last action", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "dsengine_editor_redo", "description": "Redo last undone action", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "dsengine_editor_screenshot", "description": "Capture viewport screenshot (base64 PNG)", "parameters": {"type": "object", "properties": {"target": {"type": "string", "enum": ["scene", "game"]}}}}},
    {"type": "function", "function": {"name": "dsengine_scene_save", "description": "Save current scene", "parameters": {"type": "object", "properties": {"path": {"type": "string"}}}}},
    {"type": "function", "function": {"name": "dsengine_scene_load", "description": "Load a scene file", "parameters": {"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}}},
    {"type": "function", "function": {"name": "dsengine_asset_import", "description": "Import an asset file", "parameters": {"type": "object", "properties": {"path": {"type": "string"}, "type": {"type": "string"}}, "required": ["path"]}}},
    {"type": "function", "function": {"name": "dsengine_material_create", "description": "Create a PBR material", "parameters": {"type": "object", "properties": {"name": {"type": "string"}, "save_path": {"type": "string"}, "base_color": {"type": "array"}, "metallic": {"type": "number"}, "roughness": {"type": "number"}}, "required": ["name"]}}},
]

SYSTEM_PROMPT = """You are an AI assistant integrated into the DSEngine game editor.
You can create, modify, and delete entities, execute Lua code, manage scenes, and more.
Always use the available tools to perform actions. Be concise in responses.
When the user asks to create something, use dsengine_entity_create.
When modifying, use dsengine_entity_modify. Use dsengine_scene_get_state to inspect the scene.
For complex batch operations, prefer dsengine_lua_execute with Lua code."""


def emit(obj):
    sys.stdout.write(json.dumps(obj, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def main():
    api_key = os.environ.get("OPENAI_API_KEY", "")
    base_url = os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1")
    model = os.environ.get("OPENAI_MODEL", "gpt-4o")

    if not api_key:
        emit({"type": "error", "message": "OPENAI_API_KEY not set. Set it in environment variables."})
        return

    try:
        import openai
    except ImportError:
        emit({"type": "error", "message": "openai package not installed. Run: pip install openai"})
        return

    client = openai.OpenAI(api_key=api_key, base_url=base_url)
    conversation = [{"role": "system", "content": SYSTEM_PROMPT}]

    emit({"type": "status", "message": f"AI ready ({model}). Ask me to build your scene!"})

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            continue

        msg_type = msg.get("type", "")

        if msg_type == "user_message":
            content = msg.get("content", "")
            conversation.append({"role": "user", "content": content})

            # LLM call loop (may iterate for tool calls)
            while True:
                try:
                    response = client.chat.completions.create(
                        model=model,
                        messages=conversation,
                        tools=TOOL_DEFINITIONS,
                        tool_choice="auto",
                    )
                except Exception as e:
                    emit({"type": "error", "message": str(e)})
                    break

                choice = response.choices[0]
                message = choice.message

                # Append assistant message to conversation
                conversation.append(message.model_dump())

                if message.tool_calls:
                    # Send each tool call to editor for execution
                    for tc in message.tool_calls:
                        emit({
                            "type": "tool_call",
                            "name": tc.function.name,
                            "arguments": tc.function.arguments,
                            "call_id": tc.id
                        })

                        # Wait for tool result from editor
                        result_line = sys.stdin.readline().strip()
                        if not result_line:
                            break

                        try:
                            result_msg = json.loads(result_line)
                        except json.JSONDecodeError:
                            result_msg = {"result": result_line}

                        tool_result = result_msg.get("result", "{}")
                        conversation.append({
                            "role": "tool",
                            "tool_call_id": tc.id,
                            "content": tool_result
                        })

                    # Continue loop to let LLM process tool results
                    continue
                else:
                    # Final text response
                    text = message.content or ""
                    emit({"type": "assistant_message", "content": text})
                    break

        elif msg_type == "tool_result":
            # Handled inline above during the tool call loop
            pass


if __name__ == "__main__":
    main()
