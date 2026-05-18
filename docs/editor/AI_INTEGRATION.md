# DSEngine 编辑器 AI 集成方案

> 更新日期: 2026-05-18 (最近修订: Phase 2 资产生成 Tool 完成)
> 目标：让开发者通过对话驱动游戏开发，资产由 AI 生成

---

## 一、核心洞察：已有的天然接入点

DSEngine 编辑器已具备 AI 集成的关键基础设施：

| 已有能力 | 文件 | AI 利用方式 |
|----------|------|------------|
| **Lua REPL** | `editor_lua_console.cpp` → `ExecuteLuaString()` | AI 生成 Lua 代码 → 直接执行，操控整个引擎 |
| **场景 JSON 序列化** | `editor_scene_io.cpp` (63KB) | AI 生成/修改场景 JSON → `LoadScene()` |
| **16 个 Lua 绑定模块** | `dse.ecs` / `dse.audio` / `dse.assets` 等 | AI 通过 Lua API 控制 ECS 全部子系统 |
| **DSSL 材质系统** | `lua_binding_dssl.cpp` | AI 生成 shader 材质 |
| **Prefab 导入导出** | `editor_prefab.cpp` | AI 生成预制体 |
| **Undo/Redo** | `editor_undo.h` (Command Pattern) | AI 操作可撤销 |
| **Lua 热重载** | `PumpLuaScriptHotReloads()` | AI 写脚本后自动生效 |

**关键发现**：`ExecuteLuaString()` 是完整的 "AI → 引擎" 桥梁。AI 只需生成 Lua 代码字符串，就能创建实体、设置组件、修改场景、调整渲染参数。

---

## 二、方案选择：Tool Provider 优于内建 Chat

> **✅ Phase 1+2 已完成** — Engine Control Server + MCP adapter + 资产生成 Tool 已全部实现。
> 当前共暴露 **23 个 Tool**（18 个引擎 + 5 个资产），覆盖场景操控、实体 CRUD、组件 CRUD、编辑器控制、脚本执行、截图、**资产导入、材质创建、AI 纹理/模型/音效生成**。

### 不推荐：在 C++ ImGui 里造 AI Chat

| 问题 | 说明 |
|------|------|
| 造轮子 | 对话框、流式输出、Markdown 渲染、代码高亮，ImGui 做这些极其痛苦 |
| C++ HTTP 客户端 | SSE 流式、重试、auth token，C++ 不适合做这些 |
| 自建 Agent 框架 | Tool Calling 路由、上下文管理、对话裁剪 — LangChain/Vercel AI SDK 已成熟 |
| 封闭 | 只有打开编辑器才能用 AI，无法与外部工具协作 |

### 推荐：引擎做 Tool Provider（MCP / WebSocket）

**核心思路：DSEngine 应该是 "AI 可控的引擎"，而不是 "内建 AI 的引擎"。**

```
┌─────────────────────────────────────────────────────────┐
│ Layer 1: AI 客户端（不用自己造）                           │
│                                                           │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌─────────┐ │
│  │  Cursor   │ │ Windsurf  │ │  Claude   │ │ 自研Web │ │
│  │  IDE      │ │  IDE      │ │  Desktop  │ │ Chat    │ │
│  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └────┬────┘ │
│        └──────┬──────┴──────┬──────┘            │       │
│               ▼             ▼                   │       │
│          MCP Protocol    HTTP/WS ◄──────────────┘       │
└───────────────┼─────────────┼───────────────────────────┘
                │             │
┌───────────────┼─────────────┼───────────────────────────┐
│ Layer 2: DSEngine Control Server（核心开发量）             │
│               ▼             ▼                           │
│  ┌──────────────────────────────────────────────────┐   │
│  │  engine_control_server.cpp  (~800 行)              │   │
│  │                                                    │   │
│  │  协议: JSON-RPC over WebSocket (或 MCP stdio)      │   │
│  │                                                    │   │
│  │  暴露的 Tools (18 个, Phase 1 已全部实现):          │   │
│  │  ├─ ping              → 连通性测试               ✅ │   │
│  │  ├─ scene.get_state   → 场景摘要 (20 种组件)     ✅ │   │
│  │  ├─ scene.load / save → 场景读写                 ✅ │   │
│  │  ├─ entity.create     → 创建实体 + 13 种组件     ✅ │   │
│  │  ├─ entity.modify     → 修改 transform + 组件属性 ✅ │   │
│  │  ├─ entity.delete     → 删除实体                 ✅ │   │
│  │  ├─ entity.add_component    → 添加组件            ✅ │   │
│  │  ├─ entity.remove_component → 移除组件            ✅ │   │
│  │  ├─ entity.get_components   → 查询组件 + 属性     ✅ │   │
│  │  ├─ lua.execute       → ExecuteLuaString()       ✅ │   │
│  │  ├─ script.create     → 写入 .lua 文件 + 热重载  ✅ │   │
│  │  ├─ editor.get_state  → 编辑器状态 + 实体计数     ✅ │   │
│  │  ├─ editor.play/stop  → 控制 Play 模式           ✅ │   │
│  │  ├─ editor.screenshot → 截图返回 base64 PNG      ✅ │   │
│  │  ├─ editor.undo/redo  → 撤销/重做                ✅ │   │
│  │  ├─ material.create   → 生成 PBR 材质文件        ✅ │   │
│  │  └─ asset.import      → 导入资产文件到项目        ✅ │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                           │
┌──────────────────────────┼───────────────────────────────┐
│ Layer 3: DSEngine Editor（已有，几乎不改）                  │
│                          ▼                               │
│  ExecuteLuaString() / LoadScene() / SaveScene()          │
│  UndoRedoManager / SceneTabManager / AssetManager        │
└─────────────────────────────────────────────────────────┘
```

### 方案对比

| | 方案 A: 内建 Chat | 方案 B: Tool Provider（推荐） |
|---|---|---|
| **核心** | 编辑器内建 AI | 引擎暴露控制协议 |
| **C++ 新增** | ~2000 行 + Chat UI | **~800 行**，无 UI |
| **开发周期** | 2-3 周 | **1 周** |
| **AI 客户端** | 自造 | Cursor/Windsurf/Claude/自研 |
| **LLM 选择** | 代码里写死 | 用户用什么客户端就用什么 LLM |
| **可扩展性** | 低 | **极高**（MCP 生态） |
| **维护成本** | 高 | **低** |

---

## 三、MCP Tool 定义

### 3.1 scene.get_state

返回当前场景的结构化摘要，用于 AI 理解上下文。

```json
{
  "name": "dsengine_scene_get_state",
  "description": "获取 DSEngine 当前场景的实体列表和组件摘要",
  "inputSchema": {
    "type": "object",
    "properties": {
      "include_components": {
        "type": "boolean",
        "description": "是否包含每个实体的组件详情",
        "default": true
      }
    }
  }
}
```

### 3.2 entity.create

```json
{
  "name": "dsengine_entity_create",
  "description": "在场景中创建实体并设置组件",
  "inputSchema": {
    "type": "object",
    "properties": {
      "name":     { "type": "string", "description": "实体名称" },
      "position": { "type": "array", "items": {"type":"number"}, "minItems": 3 },
      "rotation": { "type": "array", "items": {"type":"number"}, "minItems": 3 },
      "scale":    { "type": "array", "items": {"type":"number"}, "minItems": 3 },
      "mesh":     { "type": "string", "description": "mesh 资产路径" },
      "material": { "type": "string", "description": "DSSL 材质名" },
      "components": {
        "type": "array",
        "items": {
          "type": "object",
          "properties": {
            "type": { "type": "string" },
            "properties": { "type": "object" }
          }
        }
      }
    },
    "required": ["name"]
  }
}
```

### 3.3 lua.execute

最通用的 Tool — AI 可以通过它完成任何操作。

```json
{
  "name": "dsengine_lua_execute",
  "description": "在 DSEngine 中执行 Lua 代码。可使用 dse.ecs / dse.audio / dse.assets 等全部 API",
  "inputSchema": {
    "type": "object",
    "properties": {
      "code": { "type": "string", "description": "要执行的 Lua 代码" }
    },
    "required": ["code"]
  }
}
```

### 3.4 script.create

```json
{
  "name": "dsengine_script_create",
  "description": "创建或覆盖 Lua 脚本文件，编辑器自动热重载",
  "inputSchema": {
    "type": "object",
    "properties": {
      "path":    { "type": "string", "description": "脚本路径（相对于项目根目录）" },
      "content": { "type": "string", "description": "Lua 脚本内容" }
    },
    "required": ["path", "content"]
  }
}
```

### 3.5 editor.screenshot

```json
{
  "name": "dsengine_editor_screenshot",
  "description": "截取当前 Scene 或 Game 视口截图，返回 base64 PNG",
  "inputSchema": {
    "type": "object",
    "properties": {
      "target": {
        "type": "string",
        "enum": ["scene", "game"],
        "default": "scene"
      }
    }
  }
}
```

---

## 四、AI 资产生成管线

### 外部 API 一览

| 资产类型 | 推荐 API | 价格 | 集成方式 |
|----------|---------|------|---------|
| **2D 纹理** | DALL·E 3 / Flux Pro | $0.04-0.08/张 | HTTP → PNG → AssetManager::LoadTexture |
| **3D 模型** | Meshy.ai / Tripo3D / Rodin | $0.10-0.50/个 | HTTP → GLB/OBJ → Assimp → .dmesh |
| **音效** | ElevenLabs SFX / Suno | $0.01-0.10/条 | HTTP → WAV → FMOD |
| **音乐** | Suno / Udio | $0.05-0.20/首 | HTTP → MP3/WAV → FMOD |
| **Sprite Sheet** | DALL·E 3 + 后处理裁切 | $0.04/张 | 生成 → 自动切片 → Atlas |
| **地形高度图** | Stable Diffusion (本地) | 免费 | 灰度图 → Terrain heightmap |

### 资产生成 Tool

```json
{
  "name": "dsengine_asset_generate_texture",
  "description": "调用 AI 生成纹理并导入项目",
  "inputSchema": {
    "type": "object",
    "properties": {
      "prompt":   { "type": "string", "description": "纹理描述" },
      "size":     { "type": "integer", "enum": [512, 1024, 2048], "default": 1024 },
      "style":    { "type": "string", "enum": ["realistic", "stylized", "pixel"], "default": "realistic" },
      "save_path": { "type": "string", "description": "保存路径（相对于 data/）" },
      "api":      { "type": "string", "enum": ["dalle3", "flux", "local_sd"], "default": "dalle3" }
    },
    "required": ["prompt"]
  }
}
```

---

## 五、Vibe Coding 完整工作流示例

### 示例 1：对话创建场景

```
开发者 (在 Cursor 中):
  "帮我在 DSEngine 场景里创建一个简单的 FPS 关卡原型"

AI 通过 MCP 自动执行:
  ① dsengine_scene_get_state() → 确认当前场景为空
  ② dsengine_entity_create(name="Floor", mesh="plane", scale=[20,1,20])
  ③ dsengine_entity_create(name="Wall_North", mesh="cube", pos=[0,2,10], scale=[20,4,0.5])
  ④ dsengine_entity_create(name="Wall_South", ...) × 3 面墙
  ⑤ dsengine_entity_create(name="Player", pos=[0,1,0], components=[Camera3D, RigidBody3D])
  ⑥ dsengine_entity_create(name="Sun", components=[DirectionalLight])
  ⑦ dsengine_editor_screenshot() → 返回预览图给开发者确认

开发者: "看起来不错，加几个箱子作为掩体"

AI:
  ⑧ dsengine_lua_execute("for i=1,5 do ... end") → 随机放置 5 个箱子
  ⑨ dsengine_editor_screenshot() → 预览
```

### 示例 2：对话生成游戏逻辑

```
开发者: "给玩家加 WASD 移动和鼠标瞄准"

AI:
  ① dsengine_script_create(
      path="samples/lua/fps_controller.lua",
      content="function Update(dt)\n  local speed = 5\n  ..."
    )
  ② dsengine_lua_execute("require('samples/lua/fps_controller')")
  ③ dsengine_editor_play() → 自动进入 Play 模式

开发者: "移动太快了"

AI:
  ④ dsengine_script_create(..., content 中 speed 改为 3)
  → 热重载自动生效
```

### 示例 3：AI 生成资产

```
开发者: "给地板加一个水泥材质"

AI:
  ① dsengine_asset_generate_texture(
      prompt="seamless concrete floor PBR texture, slightly cracked",
      save_path="textures/concrete_floor.png"
    )
  ② dsengine_material_create(
      name="concrete",
      shader="pbr",
      albedo_map="textures/concrete_floor.png",
      roughness=0.8
    )
  ③ dsengine_lua_execute("dse.ecs.set_material(floor_entity, 'concrete')")
```

---

## 六、分阶段实施计划

| 阶段 | 内容 | 工作量 | 新增代码 | 依赖 | 状态 |
|------|------|--------|---------|------|------|
| **Phase 1a** | Engine Control Server（WebSocket JSON-RPC） | 1 周 | ~1200 行 C++ | WebSocket 库 | ✅ 完成 |
| **Phase 1b** | MCP stdio adapter（Python） | 2-3 天 | ~300 行 Python | websocket-client | ✅ 完成 |
| **Phase 2** | 资产生成 Tool（接外部 API） | 2 周 | ~450 行 | requests | ✅ 完成 |
| **Phase 3** | 内建 Chat Panel（ImGui + Python LLM bridge） | 2-3 天 | ~310 行 C++/Python | IXWebSocket (已有) | 🔧 进行中 |
| **总计** | | 5-7 周 | ~3500 行 | | |

### Phase 1a 实际文件（✅ 已完成）

```
apps/editor_cpp/src/
├── editor_control_server.cpp   // WebSocket JSON-RPC 服务器（端口 9527）
├── editor_control_server.h     // ControlServer 类 + Tool 注册 + 消息分发
├── editor_control_tools.cpp    // 20 个 Tool handler 实现（含 Base64 编码 + stb PNG + 资产导入/材质创建）
├── editor_plugin_manager.h     // 插件管理器（Python 进程外插件）
└── editor_plugin_manager.cpp   //

tools/
├── mcp_adapter/
│   └── dsengine_mcp.py         // MCP stdio 适配器（23 个 Tool 定义 + WsBridge + AI 生成）
├── test_control_server.py      // 端到端测试脚本（14 项测试）

.windsurf/
└── mcp.json                    // Windsurf IDE MCP 配置
mcp_config.json                 // 通用 MCP 配置模板（Cursor / Claude Desktop）
```

改动文件：
- `editor_app.cpp` — `ControlServer::Start(9527)` + 每帧 `Poll()`
- `editor_toolbar.h/cpp` — 暴露 `EnterPlayMode` / `ExitPlayMode`
- `CMakeLists.txt` — 新增源文件 + WebSocket 依赖

### Phase 1+2 Tool 清单（✅ 23 个已实现）

| Tool | 功能 | 类别 |
|------|------|------|
| `dsengine_ping` | 连通性测试 | 基础 |
| `dsengine_editor_get_state` | 编辑器状态 + 实体计数 | 编辑器 |
| `dsengine_editor_play` | 进入 Play 模式 | 编辑器 |
| `dsengine_editor_stop` | 退出 Play 模式（恢复场景） | 编辑器 |
| `dsengine_editor_undo` | 撤销 | 编辑器 |
| `dsengine_editor_redo` | 重做 | 编辑器 |
| `dsengine_editor_screenshot` | 截图返回 base64 PNG | 编辑器 |
| `dsengine_scene_get_state` | 场景结构化摘要（20 种组件 + 属性详情） | 场景 |
| `dsengine_scene_save` | 保存场景 | 场景 |
| `dsengine_scene_load` | 加载场景 | 场景 |
| `dsengine_entity_create` | 创建实体 + mesh/color/13 种组件 | 实体 |
| `dsengine_entity_delete` | 删除实体 | 实体 |
| `dsengine_entity_modify` | 修改 transform + 组件属性（8 种） | 实体 |
| `dsengine_entity_add_component` | 向已有实体添加组件（13 种） | 组件 |
| `dsengine_entity_remove_component` | 移除实体上的组件（13 种） | 组件 |
| `dsengine_entity_get_components` | 查询实体全部组件 + 属性详情 | 组件 |
| `dsengine_lua_execute` | 执行 Lua 代码（最通用 Tool） | 脚本 |
| `dsengine_script_create` | 创建 .lua 文件 + 热重载 | 脚本 |
| `dsengine_asset_import` | 导入纹理/mesh/音频/材质到引擎 | 资产 |
| `dsengine_material_create` | 创建 PBR .dmat 材质文件并加载 | 资产 |
| `dsengine_asset_generate_texture` | AI 生成纹理（DALL·E 3）并导入 | AI 生成 |
| `dsengine_asset_generate_model` | AI 生成 3D 模型（Meshy.ai）并下载 | AI 生成 |
| `dsengine_asset_generate_sfx` | AI 生成音效（ElevenLabs）并导入 | AI 生成 |

### 支持的组件类型（13 种可添加/移除，20 种可检测）

| 组件 | create | add | remove | modify | detect |
|------|--------|-----|--------|--------|--------|
| MeshRenderer | ✅ | ✅ | ✅ | ✅ mesh_path/color/metallic/roughness | ✅ |
| Camera3D | ✅ | ✅ | ✅ | ✅ fov/near_clip/far_clip | ✅ |
| DirectionalLight | ✅ | ✅ | ✅ | ✅ intensity/color/direction | ✅ |
| PointLight | ✅ | ✅ | ✅ | ✅ intensity/color/range | ✅ |
| SpotLight | ✅ | ✅ | ✅ | ✅ intensity/range/cone angles | ✅ |
| RigidBody3D | ✅ | ✅ | ✅ | ✅ mass/body_type | ✅ |
| BoxCollider3D | ✅ | ✅ | ✅ | — | ✅ |
| SphereCollider3D | ✅ | ✅ | ✅ | — | ✅ |
| AudioSource | ✅ | ✅ | ✅ | ✅ volume/pitch/loop | ✅ |
| AudioListener | ✅ | ✅ | ✅ | — | ✅ |
| SkyLight | ✅ | ✅ | ✅ | — | ✅ |
| Skybox | ✅ | ✅ | ✅ | — | ✅ |
| PostProcess | ✅ | ✅ | ✅ | — | ✅ |
| SpriteRenderer | — | — | — | — | ✅ |
| Animator3D | — | — | — | — | ✅ |
| Water | — | — | — | — | ✅ |
| Terrain | — | — | — | — | ✅ |
| Decal | — | — | — | — | ✅ |

### Phase 3 说明

如确需编辑器内建 Chat，**复用已有 `launcher_tauri`**（React + framer-motion + lucide-react），新增 Chat 页面通过 WebSocket 连接 Engine Control Server。**0 行 C++ UI 代码**。

---

## 七、成本与收益

### 开发成本

| 阶段 | 人力 | API 密钥 |
|------|------|---------|
| Phase 1 (Control Server) | 1 人 × 1.5 周 | 无 |
| Phase 2 (资产生成) | 1 人 × 2 周 | 各平台 API Key |
| Phase 3 (内建 Chat) | 1 人 × 1-2 周 | 无 |

### 运行成本（每月每开发者）

| 使用强度 | 月 API 成本 | 说明 |
|----------|-----------|------|
| 轻度（50 次对话/天） | ~$30-50 | 主要是 LLM 对话 |
| 中度（200 次/天 + 资产） | ~$100-200 | LLM + 少量图片/模型 |
| 重度（全 AI 驱动原型） | ~$300-500 | 大量资产生成 |

### 收益

| 收益 | 量化 | 说明 |
|------|------|------|
| **原型速度** | **10-50x** | "30 分钟对话出 demo" vs "2 天手搭" |
| **学习曲线** | **大幅降低** | 不需学 Lua API / ECS，对话即可 |
| **竞争差异** | **极高** | 独立引擎中几乎没有先例 |
| **独立开发者** | **极高** | 一人 = 策划 + 程序 + 美术 |
| **生态黏性** | **高** | AI 对 DSEngine API 训练形成壁垒 |

### 竞品对比

| 引擎 | AI 集成方式 | 评价 |
|------|------------|------|
| Unity | Muse Chat（内建） | 体验一般，锁死在 Editor 内 |
| Unreal | Copilot 插件 | 仅代码辅助，不能操控编辑器 |
| Godot | 无官方 AI | 社区插件质量参差 |
| **DSEngine** | **MCP Server + 可选内建 Chat** | **最开放，兼容所有 AI 客户端** |

---

## 八、风险与缓解

| 风险 | 缓解措施 |
|------|---------|
| LLM 生成代码不可靠 | 所有操作走 Undo 系统，用户可一键回滚 |
| API 费用失控 | 编辑器内显示 token 用量 + 每日预算上限 |
| 外部 API 不稳定 | 资产生成 Tool 支持多 provider fallback |
| 安全风险 | Control Server 默认 localhost only + 可选 auth token |
| 场景状态被破坏 | 每次 AI 操作前自动创建场景快照 |

---

## 九、总结

**最优策略**：Phase 1（Control Server + MCP adapter）和 Phase 2（资产生成 Tool）均已完成。引擎已可被任何 AI 客户端驱动，并支持 AI 生成纹理/3D 模型/音效。Phase 3（内建 Chat Panel）为可选项。

**核心原则**：引擎做 Tool Provider，AI 能力由生态提供。这比自建 AI Chat 更灵活、更低成本、更高扩展性。
