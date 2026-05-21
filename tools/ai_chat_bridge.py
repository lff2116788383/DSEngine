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
    """Call client.chat.completions.create with exponential backoff on transient errors.
    Supports both stream=True (returns AsyncStream) and stream=False (returns ChatCompletion).
    """
    base_delay = 1.0
    for attempt in range(max_retries + 1):
        try:
            result = await client.chat.completions.create(**kwargs)
            return result
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

# ─── Session State ──────────────────────────────────────────────────────

mcp_client = None
_openai_client = None       # 复用 client，避免每条消息重建
_conversation_history = []  # 多轮对话历史，跨消息保持

def _get_openai_client():
    """获取或创建 OpenAI client（按当前环境变量）。"""
    global _openai_client
    api_key  = os.environ.get("OPENAI_API_KEY", "")
    base_url = os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1")
    # 若 key/url 改变则重建
    if (_openai_client is None
            or getattr(_openai_client, "_api_key_snapshot", "") != api_key
            or getattr(_openai_client, "_base_url_snapshot", "") != base_url):
        _openai_client = openai.AsyncOpenAI(api_key=api_key, base_url=base_url)
        _openai_client._api_key_snapshot  = api_key
        _openai_client._base_url_snapshot = base_url
    return _openai_client

# ─── Async Main ─────────────────────────────────────────────────────────

async def handle_message(msg, current_task_ref, stdin_queue):
    """Handle a single message from stdin."""
    global mcp_client, _conversation_history

    msg_type = msg.get("type", "")

    if msg_type == "cancel":
        if current_task_ref[0] and not current_task_ref[0].done():
            current_task_ref[0].cancel()
            emit({"type": "status", "message": "Generation cancelled."})
        return

    if msg_type == "clear_history":
        _conversation_history.clear()
        return

    if msg_type == "tool_result":
        # tool_result 由 handle_tool_round 通过 stdin_queue 消费，不在此处理
        return

    if msg_type == "user_message":
        content  = msg.get("content", "")
        agent_id = msg.get("agent_id", "general")

        api_key = os.environ.get("OPENAI_API_KEY", "")
        if not api_key:
            emit({"type": "error", "message": "OPENAI_API_KEY not set."})
            return

        model = os.environ.get("OPENAI_MODEL", "gpt-4o")
        temperature = float(os.environ.get("OPENAI_TEMPERATURE", "0.7"))
        max_tokens  = int(os.environ.get("OPENAI_MAX_TOKENS", "4096"))

        # 构建 system 消息（含 agent 指令 + 可用工具列表）
        instructions = AGENT_INSTRUCTIONS.get(agent_id, AGENT_INSTRUCTIONS["general"])
        if mcp_client and mcp_client.tools:
            tool_list     = ", ".join(mcp_client.tools.keys())
            instructions += f"\n\nAvailable tools: {tool_list}"
        system_message = {"role": "system", "content": instructions}

        # 追加用户消息到历史
        _conversation_history.append({"role": "user", "content": content})

        # 完整 messages = system + 历史（含本条）
        messages = [system_message] + _conversation_history

        tool_schemas = mcp_client.get_tool_schemas() if mcp_client else []
        tools = tool_schemas if tool_schemas else None

        client     = _get_openai_client()
        emit({"type": "stream_start"})
        chunk_id   = 0
        total_in   = 0
        total_out  = 0

        try:
            # 真正的流式循环：每轮可能产生 tool_calls，直到 finish_reason=stop
            while True:
                full_content = ""
                # tool_calls 以 index 为 key 累积 delta
                tc_acc: dict[int, dict] = {}
                finish_reason = None

                stream = await create_completion_with_retry(
                    client,
                    model=model,
                    messages=messages,
                    tools=tools,
                    temperature=temperature,
                    max_tokens=max_tokens,
                    stream=True,
                    stream_options={"include_usage": True},
                )

                async for chunk in stream:
                    # usage 在最后一个 chunk 返回
                    if chunk.usage:
                        total_in  += getattr(chunk.usage, "prompt_tokens", 0)
                        total_out += getattr(chunk.usage, "completion_tokens", 0)

                    if not chunk.choices:
                        continue

                    choice = chunk.choices[0]
                    if choice.finish_reason:
                        finish_reason = choice.finish_reason

                    delta = choice.delta

                    # 文本 delta：直接转发给 C++ 端
                    if delta.content:
                        full_content += delta.content
                        emit_stream_chunk(delta.content, chunk_id, False)
                        chunk_id += 1

                    # tool_call delta：按 index 累积
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
                    # 将 assistant 轮次（含 tool_calls）追加到 messages
                    openai_tcs = [
                        {"id": tc_acc[i]["id"], "type": "function",
                         "function": {"name": tc_acc[i]["name"], "arguments": tc_acc[i]["args"]}}
                        for i in sorted(tc_acc)
                    ]
                    messages.append({
                        "role": "assistant",
                        "content": full_content or None,
                        "tool_calls": openai_tcs,
                    })

                    for tc in openai_tcs:
                        emit({
                            "type": "tool_call",
                            "name": tc["function"]["name"],
                            "arguments": tc["function"]["arguments"],
                            "call_id": tc["id"],
                        })

                        # 等待 C++ 通过 stdin_queue 返回 tool_result
                        tool_result = "{}"
                        try:
                            while True:
                                result_line = await asyncio.wait_for(
                                    stdin_queue.get(), timeout=30.0)
                                result_msg = json.loads(result_line.strip())
                                if (result_msg.get("type") == "tool_result" and
                                        result_msg.get("call_id") == tc["id"]):
                                    tool_result = result_msg.get("result", "{}")
                                    break
                        except (asyncio.TimeoutError, json.JSONDecodeError):
                            pass

                        messages.append({
                            "role": "tool",
                            "tool_call_id": tc["id"],
                            "content": tool_result,
                        })
                    # 继续下一轮流式请求
                    continue

                # finish_reason == "stop"（或其他），结束循环
                _conversation_history.append({"role": "assistant", "content": full_content})
                break

            # 发送累计 token 统计
            if total_in or total_out:
                emit({"type": "token_usage",
                      "input_tokens": total_in,
                      "output_tokens": total_out,
                      "model": model})

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

    # Bridge stdin to asyncio queue（Fix H: 使用 get_running_loop）
    loop = asyncio.get_running_loop()
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

                # tool_result 消息由 handle_message 内部通过 stdin_queue 消费
                # 这里只为顶层消息创建 task
                if msg.get("type") == "tool_result":
                    # 重新入队，让等待中的 handle_message 消费
                    await stdin_queue.put(line)
                    continue

                current_task_ref[0] = asyncio.create_task(
                    handle_message(msg, current_task_ref, stdin_queue)
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

