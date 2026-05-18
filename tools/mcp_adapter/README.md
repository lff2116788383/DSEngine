# DSEngine MCP Adapter

MCP (Model Context Protocol) stdio 适配器，让 AI 编程助手（Cursor / Windsurf / Claude Desktop）直接控制 DSEngine 编辑器。

## 架构

```
AI 客户端 (Cursor/Windsurf) ←─ MCP stdio ─→ dsengine_mcp.py ←─ WebSocket ─→ DSEngine Editor
```

## 前置条件

1. Python 3.8+
2. `pip install websocket-client`
3. DSEngine 编辑器已启动（Control Server 默认监听 `ws://127.0.0.1:9527`）

## 配置

### Cursor / Windsurf

在项目根目录 `.cursor/mcp.json` 或 IDE 设置中添加：

```json
{
    "mcpServers": {
        "dsengine": {
            "command": "python",
            "args": ["tools/mcp_adapter/dsengine_mcp.py"]
        }
    }
}
```

### Claude Desktop

编辑 `~/Library/Application Support/Claude/claude_desktop_config.json`（macOS）或对应 Windows 路径：

```json
{
    "mcpServers": {
        "dsengine": {
            "command": "python",
            "args": ["C:/path/to/DSEngine/tools/mcp_adapter/dsengine_mcp.py"]
        }
    }
}
```

## 可用 Tools

| Tool | 说明 |
|------|------|
| `dsengine_ping` | 连通性测试 |
| `dsengine_lua_execute` | 执行 Lua 代码（完整引擎 API） |
| `dsengine_scene_get_state` | 获取场景实体列表 + 组件 |
| `dsengine_entity_create` | 创建实体 |
| `dsengine_entity_delete` | 删除实体 |
| `dsengine_script_create` | 写入 Lua 脚本（自动热重载） |
| `dsengine_editor_get_state` | 获取编辑器状态 |

## 测试

```bash
# 先启动编辑器，然后：
python tools/test_control_server.py
```

## 自定义端口

```bash
python tools/mcp_adapter/dsengine_mcp.py --port 9528
```
