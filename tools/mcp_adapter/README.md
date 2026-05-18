# DSEngine MCP Adapter

MCP (Model Context Protocol) stdio 适配器，让 AI 编程助手（Cursor / Windsurf / Claude Desktop）直接控制 DSEngine 编辑器。

## 架构

```
AI 客户端 (Cursor/Windsurf) ←─ MCP stdio ─→ dsengine_mcp.py ←─ WebSocket ─→ DSEngine Editor
```

## 前置条件

1. Python 3.8+
2. `pip install websocket-client requests`
3. DSEngine 编辑器已启动（Control Server 默认监听 `ws://127.0.0.1:9527`）
4. （可选）AI 资产生成需要设置环境变量：
   - `OPENAI_API_KEY` — DALL·E 3 纹理生成
   - `MESHY_API_KEY` — Meshy.ai 3D 模型生成
   - `ELEVENLABS_API_KEY` — ElevenLabs 音效生成

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

## 可用 Tools（23 个）

### 引擎 Tool（转发到 C++ Control Server）

| Tool | 说明 |
|------|------|
| `dsengine_ping` | 连通性测试 |
| `dsengine_lua_execute` | 执行 Lua 代码（完整引擎 API） |
| `dsengine_scene_get_state` | 获取场景实体列表 + 组件 |
| `dsengine_entity_create` | 创建实体 + mesh/color/13 种组件 |
| `dsengine_entity_delete` | 删除实体 |
| `dsengine_entity_modify` | 修改 transform + 组件属性 |
| `dsengine_entity_add_component` | 添加组件 |
| `dsengine_entity_remove_component` | 移除组件 |
| `dsengine_entity_get_components` | 查询组件详情 |
| `dsengine_script_create` | 写入 Lua 脚本（自动热重载） |
| `dsengine_editor_get_state` | 获取编辑器状态 |
| `dsengine_editor_play/stop` | 进入/退出 Play 模式 |
| `dsengine_editor_undo/redo` | 撤销/重做 |
| `dsengine_editor_screenshot` | 截图返回 base64 PNG |
| `dsengine_scene_save/load` | 保存/加载场景 |
| `dsengine_asset_import` | 导入纹理/mesh/音频/材质到引擎 |
| `dsengine_material_create` | 创建 PBR .dmat 材质文件 |

### AI 资产生成 Tool（Python 本地处理，调用外部 API）

| Tool | 说明 | 需要 API Key |
|------|------|------------|
| `dsengine_asset_generate_texture` | AI 生成纹理（DALL·E 3）并导入引擎 | `OPENAI_API_KEY` |
| `dsengine_asset_generate_model` | AI 生成 3D 模型（Meshy.ai）并下载 | `MESHY_API_KEY` |
| `dsengine_asset_generate_sfx` | AI 生成音效（ElevenLabs）并导入 | `ELEVENLABS_API_KEY` |

## 测试

```bash
# 先启动编辑器，然后：
python tools/test_control_server.py
```

## 自定义端口

```bash
python tools/mcp_adapter/dsengine_mcp.py --port 9528
```
