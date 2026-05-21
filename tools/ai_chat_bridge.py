#!/usr/bin/env python3
"""
AI Chat Bridge for DSEngine Editor.

Integrates OpenAI API with MCP (Model Context Protocol) to provide
AI capabilities with automatic tool discovery from dsengine_mcp.py.

Stdin/stdout JSON-lines protocol:
  Editor -> Bridge:
    {"type": "user_message", "content": "...", "agent_id": "..."}
    {"type": "cancel"}
    {"type": "tool_result", "call_id": "...", "result": "..."}
  Bridge -> Editor:
    {"type": "stream_start"}
    {"type": "stream_chunk", "content": "...", "chunk_id": 123, "is_last": false}
    {"type": "stream_end", "chunk_id": 123}
    {"type": "tool_call", "name": "...", "arguments": "...", "call_id": "..."}
    {"type": "error", "message": "..."}
    {"type": "status", "message": "..."}
    {"type": "token_usage", "input_tokens": 100, "output_tokens": 50, "model": "gpt-4o"}

Environment:
  OPENAI_API_KEY     - Required
  OPENAI_BASE_URL    - Optional (for compatible APIs)
  OPENAI_MODEL       - Optional (default: gpt-4o)
  HTTP_PROXY         - Optional proxy URL
"""

import asyncio
import json
import os
import sys
import threading
from pathlib import Path

try:
    import openai
except ImportError:
    sys.stderr.write("openai package not installed. Run: pip install openai\n")
    sys.exit(1)

try:
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
except ImportError:
    sys.stderr.write("mcp package not installed. Run: pip install mcp\n")
    sys.exit(1)

# ─── Agent Definitions ─────────────────────────────────────────────────────

AGENT_INSTRUCTIONS = {
    "general": """You are a helpful assistant for the DSEngine game editor.
You can create, modify, and delete entities, execute Lua code, manage scenes, and more.
Always use the available tools to perform actions. Be concise in responses.""",

    "scene_architect": """You are a scene architect for DSEngine game editor.
Focus on composition, lighting, and visual hierarchy.
Use dsengine_entity_create to build scenes.
Use dsengine_scene_get_state to inspect the current scene.
Use dsengine_editor_screenshot to visually inspect the scene before making changes.""",

    "script_writer": """You are a Lua scripting expert for DSEngine.
Write clean, efficient game logic code following DSEngine conventions.
Use dsengine_lua_execute to test code and dsengine_script_create to save files.""",

    "asset_manager": """You are an asset management expert for DSEngine.
You can import assets, create materials, and generate textures/models using AI.
Use dsengine_asset_import to import files.
Use dsengine_material_create to create PBR materials.
Use dsengine_asset_generate_texture to generate textures with DALL-E.
Use dsengine_asset_generate_model to generate 3D models with Meshy.""",
}

# ─── Retry Helper ───────────────────────────────────────────────────────

async def create_completion_with_retry(client, max_retries=3, **kwargs):
    """Call client.chat.completions.create with exponential backoff on transient errors."""
    base_delay = 1.0
    for attempt in range(max_retries + 1):
        try:
            return await client.chat.completions.create(**kwargs)
        except openai.AuthenticationError as e:
            emit({"type": "error", "message": f"认证失败：请检查 API Key。({e})"})
            raise
        except openai.RateLimitError as e:
            if attempt == max_retries:
                emit({"type": "error", "message": f"请求频率超限，已重试 {max_retries} 次。"})
                raise
            retry_after = float(getattr(e, 'retry_after', None) or base_delay * (2 ** attempt))
            emit({"type": "status", "message": f"请求频率超限，{retry_after:.0f}s 后重试 ({attempt+1}/{max_retries})..."})
            await asyncio.sleep(retry_after)
        except (openai.APIConnectionError, openai.APITimeoutError) as e:
            if attempt == max_retries:
                emit({"type": "error", "message": f"网络连接失败，已重试 {max_retries} 次：{e}"})
                raise
            delay = base_delay * (2 ** attempt)
            emit({"type": "status", "message": f"网络错误，{delay:.0f}s 后重试 ({attempt+1}/{max_retries})..."})
            await asyncio.sleep(delay)
        except openai.BadRequestError as e:
            emit({"type": "error", "message": f"请求参数错误：{e}"})
            raise
        except openai.InternalServerError as e:
            if attempt == max_retries:
                emit({"type": "error", "message": f"OpenAI 服务器错误，已重试 {max_retries} 次。"})
                raise
            delay = base_delay * (2 ** attempt)
            emit({"type": "status", "message": f"服务器错误，{delay:.0f}s 后重试 ({attempt+1}/{max_retries})..."})
            await asyncio.sleep(delay)

# ─── MCP Client ─────────────────────────────────────────────────────────

class MCPToolClient:
    """Manages MCP connection and tool execution."""
    
    def __init__(self):
        self.session = None
        self.tools = {}
    
    async def call_tool(self, name, arguments):
        """Call a tool via MCP."""
        if not self.session:
            return {"error": "MCP not connected"}
        
        try:
            result = await self.session.call_tool(name, arguments)
            return {"result": str(result.content)}
        except Exception as e:
            return {"error": str(e)}
    
    def get_tool_schemas(self):
        """Get OpenAI function calling schemas for MCP tools."""
        schemas = []
        for name, tool in self.tools.items():
            schemas.append({
                "type": "function",
                "function": {
                    "name": name,
                    "description": tool.description,
                    "parameters": tool.inputSchema
                }
            })
        return schemas

# ─── Protocol Functions ─────────────────────────────────────────────────

def emit(obj):
    sys.stdout.write(json.dumps(obj, ensure_ascii=False) + "\n")
    sys.stdout.flush()

def emit_stream_chunk(content, chunk_id, is_last=False):
    emit({
        "type": "stream_chunk",
        "content": content,
        "chunk_id": chunk_id,
        "is_last": is_last
    })

# ─── MCP Server Path ─────────────────────────────────────────────────────

def get_mcp_adapter_path():
    """Find dsengine_mcp.py relative to this script."""
    script_dir = Path(__file__).parent
    mcp_adapter = script_dir / "mcp_adapter" / "dsengine_mcp.py"
    if not mcp_adapter.exists():
        return None
    return str(mcp_adapter)

# ─── Async Main ─────────────────────────────────────────────────────────

mcp_client = None

async def handle_message(msg, current_task_ref):
    """Handle a single message from stdin."""
    global mcp_client
    
    msg_type = msg.get("type", "")

    if msg_type == "cancel":
        if current_task_ref[0] and not current_task_ref[0].done():
            current_task_ref[0].cancel()
            emit({"type": "status", "message": "Generation cancelled."})
        return
    
    if msg_type == "tool_result":
        # Handle tool result from C++ (for MCP tools that need async callback)
        # This is a placeholder - actual implementation would need to correlate
        # tool results with pending tool calls
        return

    if msg_type == "user_message":
        content = msg.get("content", "")
        agent_id = msg.get("agent_id", "general")

        # Get agent instructions
        instructions = AGENT_INSTRUCTIONS.get(agent_id, AGENT_INSTRUCTIONS["general"])
        
        # Build system message with agent role and available tools
        system_content = instructions
        if mcp_client and mcp_client.tools:
            tool_list = ", ".join(mcp_client.tools.keys())
            system_content += f"\n\nAvailable tools: {tool_list}"
        
        system_message = {
            "role": "system",
            "content": system_content
        }

        # Configure OpenAI client
        api_key = os.environ.get("OPENAI_API_KEY", "")
        if not api_key:
            emit({"type": "error", "message": "OPENAI_API_KEY not set."})
            return

        base_url = os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1")
        model = os.environ.get("OPENAI_MODEL", "gpt-4o")

        client = openai.AsyncOpenAI(
            api_key=api_key,
            base_url=base_url,
        )

        # Build messages
        messages = [system_message, {"role": "user", "content": content}]

        # Get tool schemas (None if no tools, to avoid passing empty list to OpenAI)
        tool_schemas = mcp_client.get_tool_schemas() if mcp_client else []
        tools = tool_schemas if tool_schemas else None

        # Start streaming
        emit({"type": "stream_start"})
        chunk_id = 0
        full_content = ""

        try:
            # First, get completion with tool calls
            response = await create_completion_with_retry(
                client,
                model=model,
                messages=messages,
                tools=tools,
            )

            # Check for tool calls
            if response.choices[0].message.tool_calls:
                # Handle tool calls
                for tool_call in response.choices[0].message.tool_calls:
                    tool_name = tool_call.function.name
                    tool_args = json.loads(tool_call.function.arguments)
                    
                    emit({
                        "type": "tool_call",
                        "name": tool_name,
                        "arguments": json.dumps(tool_args),
                        "call_id": tool_call.id
                    })
                    
                    # Forward tool call to C++ for execution
                    # C++ will execute via ControlServer and send back tool_result
                    
                    # Wait for tool result from C++ via stdin
                    tool_result = None
                    try:
                        result_line = await asyncio.get_event_loop().run_in_executor(
                            None, sys.stdin.readline
                        )
                        if result_line:
                            result_msg = json.loads(result_line.strip())
                            tool_result = result_msg.get("result", "{}")
                        else:
                            tool_result = "{}"
                    except:
                        tool_result = "{}"
                    
                    # Add result to messages for next turn
                    messages.append({
                        "role": "assistant",
                        "tool_calls": [tool_call]
                    })
                    messages.append({
                        "role": "tool",
                        "tool_call_id": tool_call.id,
                        "content": tool_result
                    })
                
                # Get final response after tool calls
                final_response = await create_completion_with_retry(
                    client,
                    model=model,
                    messages=messages,
                )
                final_content = final_response.choices[0].message.content
            else:
                final_content = response.choices[0].message.content

            # Emit token usage (accumulated across tool-call rounds)
            usage = getattr(response, 'usage', None)
            if usage:
                emit({
                    "type": "token_usage",
                    "input_tokens":  getattr(usage, 'prompt_tokens', 0),
                    "output_tokens": getattr(usage, 'completion_tokens', 0),
                    "model": model
                })

            # Stream the final content
            for i in range(0, len(final_content), 10):
                chunk = final_content[i:i+10]
                emit_stream_chunk(chunk, chunk_id, False)
                chunk_id += 1
                await asyncio.sleep(0.01)  # Simulate streaming

            emit_stream_chunk("", chunk_id, True)
            emit({"type": "stream_end", "chunk_id": chunk_id})

        except asyncio.CancelledError:
            emit({"type": "status", "message": "Generation cancelled."})
        except Exception as e:
            emit({"type": "error", "message": str(e)})

async def main():
    global mcp_client
    
    # Setup MCP client
    mcp_adapter_path = get_mcp_adapter_path()
    mcp_client = MCPToolClient()
    
    if mcp_adapter_path:
        server_params = StdioServerParameters(
            command="python",
            args=[mcp_adapter_path]
        )
        
        # Keep MCP connection alive for the entire session
        stdio_ctx = stdio_client(server_params)
        read, write = await stdio_ctx.__aenter__()
        mcp_client.session = ClientSession(read, write)
        await mcp_client.session.initialize()
        
        # List available tools
        tools_result = await mcp_client.session.list_tools()
        for tool in tools_result.tools:
            mcp_client.tools[tool.name] = tool
        
        if mcp_client.tools:
            emit({"type": "status", "message": f"MCP connected with {len(mcp_client.tools)} tools"})
        else:
            emit({"type": "status", "message": "MCP connected but no tools found"})
    else:
        emit({"type": "status", "message": "MCP adapter not found, running without tools"})

    model = os.environ.get("OPENAI_MODEL", "gpt-4o")
    emit({"type": "status", "message": f"AI ready ({model})."})

    # Bridge stdin to asyncio queue
    loop = asyncio.get_event_loop()
    stdin_queue = asyncio.Queue()
    current_task_ref = [None]

    def stdin_reader():
        for line in sys.stdin:
            asyncio.run_coroutine_threadsafe(stdin_queue.put(line), loop)

    threading.Thread(target=stdin_reader, daemon=True).start()

    try:
        # Main message loop
        while True:
            try:
                line = await stdin_queue.get()
                if not line:
                    break

                try:
                    msg = json.loads(line.strip())
                except json.JSONDecodeError:
                    continue

                current_task_ref[0] = asyncio.create_task(
                    handle_message(msg, current_task_ref)
                )
                await current_task_ref[0]

            except Exception as e:
                emit({"type": "error", "message": str(e)})
    finally:
        # Cleanup MCP connection
        if mcp_adapter_path and mcp_client.session:
            await stdio_ctx.__aexit__(None, None, None)

if __name__ == "__main__":
    asyncio.run(main())

