# DSEngine 插件开发指南

> 更新日期: 2026-05-18

---

## 一、概述

DSEngine 编辑器通过 **Control Server**（WebSocket JSON-RPC）提供插件接口。插件是独立进程，可用任何语言编写，通过 WebSocket 连接到编辑器并调用 23 个 Tool API。

**架构**：

```
编辑器进程 (C++)                    插件进程 (任意语言)
┌──────────────────┐               ┌──────────────────┐
│ Engine Core      │               │ Python / Node.js │
│ ImGui UI         │               │ / Rust / Go ...  │
│ Control Server   │◄──WebSocket──►│ websocket client │
│ (port 9527)      │  JSON-RPC 2.0 │                  │
└──────────────────┘               └──────────────────┘
```

**核心优势**：
- 语言无关 — 任何有 WebSocket 库的语言均可
- 进程隔离 — 插件崩溃不影响编辑器
- 生态自由 — pip / npm / cargo 全可用
- 热重载 — 重启插件进程即可

---

## 二、快速开始

### 2.1 从模板创建插件

**Python 插件**：
```bash
cd plugins
cp -r template_python my_plugin
cd my_plugin
pip install -r requirements.txt
# 编辑 plugin.json 和 main.py
```

**Node.js 插件**：
```bash
cd plugins
cp -r template_nodejs my_plugin
cd my_plugin
npm install
# 编辑 plugin.json 和 index.js
```

### 2.2 最小示例 (Python)

```python
import json
import websocket

ws = websocket.create_connection("ws://127.0.0.1:9527", timeout=10)

# 发送 JSON-RPC 请求
ws.send(json.dumps({
    "jsonrpc": "2.0",
    "id": 1,
    "method": "dsengine_ping",
    "params": {}
}))

# 接收响应
response = json.loads(ws.recv())
print(response["result"])  # {"status": "ok", "editor": "DSEngine", ...}

ws.close()
```

### 2.3 最小示例 (Node.js)

```javascript
const WebSocket = require('ws');
const ws = new WebSocket('ws://127.0.0.1:9527');

ws.on('open', () => {
    ws.send(JSON.stringify({
        jsonrpc: '2.0',
        id: 1,
        method: 'dsengine_ping',
        params: {}
    }));
});

ws.on('message', (data) => {
    const response = JSON.parse(data);
    console.log(response.result);
    ws.close();
});
```

---

## 三、plugin.json 格式

每个插件目录必须包含 `plugin.json`，编辑器通过它发现和管理插件。

```json
{
    "name": "My Plugin",
    "version": "1.0.0",
    "author": "Your Name",
    "description": "Plugin description",
    "runtime": "python",
    "entry": "main.py",
    "requires_ui": false,
    "ui_port": 0
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `name` | string | ✅ | 插件显示名称 |
| `version` | string | ✅ | 语义化版本号 (SemVer) |
| `author` | string | ✅ | 作者名 |
| `description` | string | ✅ | 功能描述 |
| `runtime` | string | ✅ | 运行时: `"python"` / `"node"` / `"executable"` |
| `entry` | string | ✅ | 入口文件（相对于插件目录） |
| `requires_ui` | bool | ❌ | 是否有 Web UI（默认 false） |
| `ui_port` | int | ❌ | Web UI 端口（仅 requires_ui=true 时有效） |

### runtime 类型

| runtime | 启动方式 | 说明 |
|---------|---------|------|
| `python` | `python <entry>` | 自动查找 python3/python |
| `node` | `node <entry>` | 需要 Node.js 18+ |
| `executable` | 直接执行 `<entry>` | 编译好的二进制 |

---

## 四、WebSocket JSON-RPC 协议

### 4.1 连接

- 地址: `ws://127.0.0.1:9527`
- 协议: JSON-RPC 2.0 (单行 JSON，换行分隔)
- 编辑器启动时自动启动 Control Server

### 4.2 请求格式

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "dsengine_<tool_name>",
    "params": { ... }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `jsonrpc` | string | 固定 `"2.0"` |
| `id` | int/string | 请求标识（响应中原样返回） |
| `method` | string | Tool 名称（见 API 参考） |
| `params` | object | Tool 参数（按 inputSchema 填写） |

### 4.3 成功响应

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": { ... }
}
```

### 4.4 错误响应

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "error": {
        "code": -32600,
        "message": "Invalid request"
    }
}
```

### 4.5 错误码

| 码 | 含义 |
|----|------|
| -32700 | Parse error（无效 JSON） |
| -32600 | Invalid request |
| -32601 | Method not found |
| -32602 | Invalid params |
| -32603 | Internal error |

---

## 五、API 参考（23 个 Tool）

共 23 个 Tool，其中 **20 个**可通过 WebSocket 直连调用（插件标准路径），**3 个** AI 资产生成 Tool 仅通过 MCP adapter 可用（见 5.8 节说明）。

### 5.1 基础

#### dsengine_ping

测试连通性。

- **参数**：无
- **返回**：`{"status": "ok", "editor": "DSEngine", "version": "..."}`

---

### 5.2 场景

#### dsengine_scene_get_state

获取当前场景的实体列表和组件摘要。

- **参数**：

| 参数 | 类型 | 默认 | 说明 |
|------|------|------|------|
| `include_components` | bool | true | 是否包含组件详情 |

- **返回**：

```json
{
    "scene_name": "Untitled",
    "entity_count": 5,
    "entities": [
        {
            "id": 1,
            "name": "MainCamera",
            "components": ["Transform", "Camera3D"]
        }
    ]
}
```

#### dsengine_scene_save

保存当前场景到文件。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `path` | string | ❌ | 保存路径（省略则保存到当前文件） |

#### dsengine_scene_load

加载场景文件。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `path` | string | ✅ | 场景文件路径 |

---

### 5.3 实体

#### dsengine_entity_create

创建实体，可选设置 mesh、颜色和组件。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `name` | string | ✅ | 实体名称 |
| `position` | [x,y,z] | ❌ | 世界位置 |
| `rotation` | [pitch,yaw,roll] | ❌ | 欧拉角（度） |
| `scale` | [x,y,z] | ❌ | 缩放 |
| `mesh` | string | ❌ | Mesh 资产路径（自动添加 MeshRenderer） |
| `color` | [r,g,b,a] | ❌ | 颜色 (0-1) |
| `components` | array | ❌ | 组件列表（见下方） |

**components 数组元素格式**：
- 字符串: `"Camera3D"` — 使用默认属性
- 对象: `{"type": "PointLight", "properties": {"intensity": 2.0}}` — 指定属性

**支持的组件类型**（13 种）：
MeshRenderer, Camera3D, DirectionalLight, PointLight, SpotLight, RigidBody3D, BoxCollider3D, SphereCollider3D, AudioSource, AudioListener, SkyLight, Skybox, PostProcess

- **返回**：`{"entity_id": 42, "name": "MyEntity"}`

#### dsengine_entity_delete

删除实体。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `entity_id` | int | ✅ | 实体 ID (uint32) |

#### dsengine_entity_modify

修改实体的名称、Transform 和/或组件属性。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `entity_id` | int | ✅ | 实体 ID |
| `name` | string | ❌ | 新名称 |
| `position` | [x,y,z] | ❌ | 新位置 |
| `rotation` | [pitch,yaw,roll] | ❌ | 新旋转（度） |
| `scale` | [x,y,z] | ❌ | 新缩放 |
| `modify_component` | object | ❌ | 修改单个组件 `{type, properties}` |
| `modify_components` | array | ❌ | 批量修改 `[{type, properties}, ...]` |
| `add_components` | array | ❌ | 添加组件 |
| `remove_components` | [string] | ❌ | 移除组件（类型名数组） |

**modify_component 可修改的属性**：

| 组件 | 可修改属性 |
|------|-----------|
| MeshRenderer | mesh_path, color, metallic, roughness |
| Camera3D | fov, near_clip, far_clip |
| DirectionalLight | intensity, color, direction |
| PointLight | intensity, color, range |
| SpotLight | intensity, range, inner_cone, outer_cone |
| RigidBody3D | mass, body_type |
| AudioSource | volume, pitch, loop |

---

### 5.4 组件

#### dsengine_entity_add_component

向已有实体添加组件。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `entity_id` | int | ✅ | 实体 ID |
| `type` | string | ✅ | 组件类型名 |
| `properties` | object | ❌ | 初始属性 |

#### dsengine_entity_remove_component

移除实体上的组件。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `entity_id` | int | ✅ | 实体 ID |
| `type` | string | ✅ | 组件类型名 |

#### dsengine_entity_get_components

查询实体的所有组件及属性详情。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `entity_id` | int | ✅ | 实体 ID |
| `detailed` | bool | ❌ | 包含属性值（默认 true） |

- **返回**：

```json
{
    "entity_id": 42,
    "components": [
        {
            "type": "MeshRenderer",
            "properties": {
                "mesh_path": "models/cube.dmesh",
                "color": [1, 0, 0, 1],
                "metallic": 0.5,
                "roughness": 0.8
            }
        }
    ]
}
```

---

### 5.5 脚本

#### dsengine_lua_execute

在引擎中执行 Lua 代码。可使用 `dse.ecs` / `dse.audio` / `dse.assets` / `dse.render` 等全部 API。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | string | ✅ | Lua 代码 |

- **返回**：`{"output": "...", "success": true}`

**示例**：
```json
{
    "method": "dsengine_lua_execute",
    "params": {
        "code": "for i=1,5 do local e = dse.ecs.create() dse.ecs.set_name(e, 'Box_'..i) end"
    }
}
```

#### dsengine_script_create

创建或覆盖 Lua 脚本文件，编辑器自动热重载。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `path` | string | ✅ | 脚本路径（相对于项目根目录） |
| `content` | string | ✅ | Lua 脚本内容 |

---

### 5.6 编辑器

#### dsengine_editor_get_state

获取编辑器状态。

- **参数**：无
- **返回**：

```json
{
    "mode": "edit",
    "entity_count": 12,
    "selected_entity": 5,
    "scene_path": "data/scenes/level1.json"
}
```

#### dsengine_editor_play

进入 Play 模式。

- **参数**：无

#### dsengine_editor_stop

退出 Play 模式（恢复场景状态）。

- **参数**：无

#### dsengine_editor_undo

撤销上一步操作。

- **参数**：无

#### dsengine_editor_redo

重做上一步被撤销的操作。

- **参数**：无

#### dsengine_editor_screenshot

截取视口截图，返回 base64 编码的 PNG。

- **参数**：

| 参数 | 类型 | 默认 | 说明 |
|------|------|------|------|
| `target` | string | "scene" | `"scene"` 或 `"game"` |

- **返回**：

```json
{
    "image": "iVBORw0KGgo...",
    "width": 1280,
    "height": 720,
    "format": "png"
}
```

---

### 5.7 资产

#### dsengine_asset_import

导入资产文件到引擎。类型从扩展名自动检测。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `path` | string | ✅ | 资产路径（相对于 data/ 或绝对路径） |
| `type` | string | ❌ | `"texture"` / `"mesh"` / `"audio"` / `"material"` / `"auto"` |
| `material_index` | int | ❌ | .dmat 文件的材质索引（默认 0） |

#### dsengine_material_create

创建 PBR 材质文件并加载到引擎。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `name` | string | ✅ | 材质名称 |
| `save_path` | string | ❌ | 保存路径（默认 `data/materials/<name>.dmat`） |
| `shader_variant` | string | ❌ | 着色器变体（默认 `"MESH_PBR"`） |
| `base_color` | [r,g,b,a] | ❌ | 基础色 (0-1) |
| `emissive` | [r,g,b] | ❌ | 自发光颜色 |
| `metallic` | float | ❌ | 金属度 (0-1) |
| `roughness` | float | ❌ | 粗糙度 (0-1) |
| `base_color_texture` | string | ❌ | Albedo 纹理路径 |
| `normal_texture` | string | ❌ | 法线贴图路径 |
| `metallic_roughness_texture` | string | ❌ | 金属-粗糙度贴图路径 |
| `double_sided` | bool | ❌ | 双面渲染（默认 false） |

---

### 5.8 AI 资产生成

> **注意**：以下 3 个 Tool 由 MCP adapter (`tools/mcp_adapter/dsengine_mcp.py`) 本地处理，
> **不经过** C++ Control Server。插件通过 WebSocket 直连 `ws://127.0.0.1:9527` 无法调用它们。
> 如需在插件中使用 AI 生成功能，可直接在插件代码中调用对应的外部 API（参考 MCP adapter 源码）。

#### dsengine_asset_generate_texture

使用 DALL-E 3 生成纹理并导入引擎。需要 `OPENAI_API_KEY` 环境变量。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `prompt` | string | ✅ | 纹理描述 |
| `save_path` | string | ✅ | 保存路径（相对于 data/） |
| `size` | string | ❌ | `"1024x1024"` / `"512x512"` / `"1792x1024"` |
| `quality` | string | ❌ | `"standard"` / `"hd"` |
| `style` | string | ❌ | `"natural"` / `"vivid"` |

#### dsengine_asset_generate_model

使用 Meshy.ai 生成 3D 模型。需要 `MESHY_API_KEY` 环境变量。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `prompt` | string | ✅ | 3D 模型描述 |
| `save_path` | string | ✅ | 保存路径（如 `"models/chair.glb"`） |
| `art_style` | string | ❌ | `"realistic"` / `"cartoon"` / `"low-poly"` / `"sculpture"` / `"pbr"` |
| `topology` | string | ❌ | `"triangle"` / `"quad"` |

#### dsengine_asset_generate_sfx

使用 ElevenLabs 生成音效。需要 `ELEVENLABS_API_KEY` 环境变量。

- **参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `prompt` | string | ✅ | 音效描述 |
| `save_path` | string | ✅ | 保存路径（如 `"audio/sfx/sword.wav"`） |
| `duration_seconds` | float | ❌ | 时长（秒，默认 2.0） |

---

## 六、插件生命周期

### 6.1 由 Plugin Manager 管理

1. 编辑器启动 → `PluginManager::ScanPlugins()` 扫描 `plugins/` 目录
2. 用户在面板点击 **Start** → 编辑器根据 `runtime` 启动子进程
3. 插件进程连接 `ws://127.0.0.1:9527` → 执行逻辑
4. 用户点击 **Stop** 或编辑器关闭 → 终止子进程

### 6.2 独立运行

插件也可以独立于 Plugin Manager 运行：
```bash
python plugins/my_plugin/main.py
```
只要编辑器的 Control Server 在监听即可。

### 6.3 心跳

长运行插件应定期发送 `dsengine_ping` 保持连接活跃（建议 10-30 秒间隔）。

---

## 七、最佳实践

### 7.1 错误处理

```python
try:
    result = client.call("dsengine_entity_create", {"name": "Test"})
except Exception as e:
    print(f"Error: {e}", file=sys.stderr)
```

### 7.2 使用 Undo 支持

通过 Control Server 的操作自动进入 Undo 栈。用户可通过 `Ctrl+Z` 或 `dsengine_editor_undo` 撤销。

### 7.3 日志输出

- 使用 **stderr** 输出日志（Plugin Manager 面板会捕获）
- **stdout** 保留给数据输出

### 7.4 优雅关闭

```python
import signal

def shutdown(sig, frame):
    ws.close()
    sys.exit(0)

signal.signal(signal.SIGTERM, shutdown)
signal.signal(signal.SIGINT, shutdown)
```

### 7.5 批量操作性能

避免逐个请求，优先使用 `dsengine_lua_execute` 批量操作：

```python
client.call("dsengine_lua_execute", {
    "code": """
    for i = 1, 100 do
        local e = dse.ecs.create()
        dse.ecs.set_name(e, "Box_" .. i)
        dse.ecs.set_transform_position(e, math.random(-20,20), 0, math.random(-20,20))
        dse.ecs.set_mesh(e, "models/cube.dmesh")
    end
    """
})
```

---

## 八、插件目录结构

```
plugins/
├── template_python/        ← Python 模板（批量重命名）
│   ├── plugin.json
│   ├── main.py
│   ├── requirements.txt
│   └── README.md
├── template_nodejs/        ← Node.js 模板（AI 纹理生成）
│   ├── plugin.json
│   ├── package.json
│   ├── index.js
│   └── README.md
├── hello_world/            ← 最小示例
│   ├── plugin.json
│   └── main.py
└── my_custom_plugin/       ← 你的插件
    ├── plugin.json
    └── ...
```

---

## 九、与 MCP/AI 客户端的关系

Control Server 同时服务于：

| 客户端类型 | 连接方式 | 使用场景 |
|-----------|---------|---------|
| 外部插件 | 直接 WebSocket | 编辑器功能扩展 |
| AI 客户端 (Cursor/Windsurf) | MCP stdio adapter → WebSocket | 对话驱动开发 |
| CI 自动化脚本 | 直接 WebSocket | 无头测试 |

MCP adapter (`tools/mcp_adapter/dsengine_mcp.py`) 将 MCP stdio 协议转换为 WebSocket JSON-RPC，使同一套 Tool 可被 AI 客户端调用。

---

## 十、故障排查

| 问题 | 解决 |
|------|------|
| 连接被拒绝 | 确认编辑器已启动且 Control Server 在监听（检查编辑器控制台） |
| Method not found | 检查 method 名称拼写，必须以 `dsengine_` 前缀 |
| 超时无响应 | 编辑器可能正在执行耗时操作，增加超时时间 |
| 插件启动后立即退出 | 检查 Plugin Manager 面板中的错误日志 |
| Python 找不到 | 确认 PATH 中有 python 或 python3 |
| Node 找不到 | 确认 PATH 中有 node，版本 >= 18 |
