# DSEngine 插件系统方案

> 更新日期: 2026-05-18 (最近修订: Phase 3 内建 AI Chat Panel)

---

## 一、当前状态

**编辑器和引擎均无正式插件系统。** 但已有三个关键基础设施：

| 基础设施 | 文件 | 作用 |
|----------|------|------|
| `DynamicLibrary` | `engine/core/dynamic_library.h/cpp` | 跨平台 DLL/SO 加载器（Win/Linux/macOS） |
| `ServiceLocator` | `engine/core/service_locator.h` | 类型安全服务容器，`Register<T>()` / `Get<T>()` |
| `IRenderPass` | `engine/render/passes/render_pass_interface.h` | 渲染 Pass 纯虚接口，21 个内建 Pass 实现 |

此外，`modules/` 目录下的 `gameplay_2d` / `gameplay_3d` 是编译期静态链入的"内部模块"。

---

## 二、方案分析与选型

### 2.1 候选方案

#### 方案 A：DLL 进程内插件

```
编辑器进程
  ├── 引擎核心
  ├── ImGui UI
  └── 插件.dll (C++ IPlugin 接口)
```

| 维度 | 评价 |
|------|------|
| 引擎操控 | ✅ 直接内存访问 |
| UI | ✅ 直接用 ImGui |
| ABI 兼容 | ❌ 必须完全相同编译器/CRT/STL |
| 热重载 | ❌ 需卸载 DLL |
| 跨 DLL 内存 | ❌ 宿主 new 的对象不能在插件 delete |
| 安全性 | ❌ 可崩溃宿主进程 |
| 调试 | ❌ 跨 DLL 断点/符号困难 |
| 适用场景 | 需要 Marketplace 二进制分发（Unreal 模式） |
| **结论** | **DSEngine 现阶段不需要** |

#### 方案 B：Lua 进程内插件

```
编辑器进程
  ├── 引擎核心
  ├── ImGui UI
  └── 插件.lua (绑定 dse.imgui.*)
```

| 维度 | 评价 |
|------|------|
| 引擎操控 | ✅ 已有 16 个 Lua 绑定模块 |
| UI | 🟡 需绑定 ImGui 子集，体验差 |
| 标准库 | ❌ 无 HTTP、无 JSON、无正则、无 Unicode |
| 包管理 | ❌ 无 pip/npm 等价物 |
| Async | ❌ 调外部 API 会阻塞主线程 |
| 复杂工具 | ❌ mesh 处理、图片操作做不了 |
| 类型系统 | ❌ 大插件无法维护 |
| 适用场景 | 游戏逻辑脚本（已经是 Lua 的定位） |
| **结论** | **Lua 适合游戏脚本，不适合编辑器插件平台** |

#### 方案 C：Python 嵌入

```
编辑器进程
  ├── 引擎核心
  ├── ImGui UI
  └── CPython 解释器 → 插件.py
```

| 维度 | 评价 |
|------|------|
| 生态 | ✅ pip 生态（numpy, PIL, requests） |
| UI | 🟡 需绑定 ImGui |
| 嵌入复杂度 | ❌ CPython 嵌入 30MB+，GIL 问题 |
| 适用场景 | Blender 模式（深度集成 Python） |
| **结论** | **成本过高，收益不明显** |

#### 方案 D：Control Server 进程外插件 ✅ 推荐

```
编辑器进程                      插件进程（独立）
  ├── 引擎核心                    ├── Python / Node / Rust / ...
  ├── ImGui UI                    ├── 任何语言的完整生态
  └── Control Server ◄──WS──►    └── 可选自带 Web UI
```

| 维度 | 评价 |
|------|------|
| 引擎操控 | ✅ 通过 WebSocket JSON-RPC |
| 语言 | ✅ 任何有 WebSocket 库的语言 |
| 生态 | ✅ pip / npm / cargo 全可用 |
| 外部 API | ✅ HTTP / AI API 无障碍 |
| 安全性 | ✅ 进程隔离，插件崩溃不影响编辑器 |
| 热重载 | ✅ 重启插件进程即可 |
| 调试 | ✅ 独立进程，正常调试 |
| UI | 插件自带 Web UI 或无 UI |
| 先例 | VS Code (LSP/Extension Host)、Blender (Python server) |
| **结论** | **最优方案** |

### 2.2 对比总结

| | DLL | Lua | Python 嵌入 | **Control Server** |
|---|---|---|---|---|
| 语言自由 | ❌ C++ only | ❌ Lua only | ❌ Python only | **✅ 任意** |
| 生态 | ❌ | ❌ | ✅ | **✅** |
| 进程隔离 | ❌ | ❌ | ❌ | **✅** |
| ABI 问题 | ❌ | ✅ | 🟡 | **✅** |
| 新增代码 | ~800 行 | ~850 行 | ~2000 行 | **~800 行** |
| 维护成本 | 高 | 中 | 高 | **低** |
| 与 AI 方案协同 | ❌ | 🟡 | 🟡 | **✅ 同一套协议** |

---

## 三、推荐方案：Control Server 即插件协议

### 3.1 核心洞察

AI_INTEGRATION.md 中定义的 Control Server（WebSocket JSON-RPC + **23 个 Tool**）换个角度看就是**通用插件 API**。

> **✅ Control Server 已完成（端口 9527）**，插件管理器 + Plugin Manager 面板也已实现。

**一套协议同时服务三个场景**：
1. AI 客户端（Cursor / Claude Desktop）→ 对话驱动开发
2. 外部插件（Python / Node.js）→ 编辑器功能扩展
3. 自动化测试（CI 脚本）→ 无头测试

### 3.2 插件协议 = Control Server API

已在 AI_INTEGRATION.md 定义的 Tool 即为插件可调用的 API：

```
ping                 → 连通性测试                        ✅
scene.get_state      → 获取场景实体/组件摘要                ✅
scene.load / save    → 场景读写                            ✅
entity.create        → 创建实体 + Transform                 ✅
entity.modify        → 修改实体属性 (name/pos/rot/scale)    ✅
entity.delete        → 删除实体                            ✅
lua.execute          → 执行任意 Lua（万能后门）               ✅
script.create        → 写 .lua 文件 + 热重载                ✅
editor.get_state     → 编辑器状态 + 实体计数               ✅
editor.play / stop   → 控制 Play 模式                      ✅
editor.screenshot    → 截图返回 base64 PNG                 ✅
editor.undo / redo   → 撤销/重做                           ✅
material.create      → 生成 DSSL 材质              (Phase 2)
asset.import         → 导入资产                     (Phase 2)
```

### 3.3 插件示例

#### Python 插件：批量重命名

```python
# plugins/batch_rename.py
import websocket, json

def main():
    ws = websocket.create_connection("ws://localhost:9527")

    # 获取场景
    ws.send(json.dumps({"jsonrpc": "2.0", "id": 1, "method": "dsengine_scene_get_state", "params": {}}))
    state = json.loads(ws.recv())

    # 批量重命名所有 Enemy 实体
    for entity in state["result"]["entities"]:
        if "Enemy" in entity["name"]:
            ws.send(json.dumps({
                "jsonrpc": "2.0",
                "id": 2,
                "method": "dsengine_entity_modify",
                "params": {
                    "entity_id": entity["id"],
                    "name": entity["name"].replace("Enemy", "Mob")
                }
            }))
            ws.recv()

    ws.close()
    print("Done: renamed all Enemy → Mob")

if __name__ == "__main__":
    main()
```

#### Node.js 插件：AI 资产生成器

```javascript
// plugins/ai_texture_gen/index.js
const WebSocket = require('ws');
const OpenAI = require('openai');
const fs = require('fs');

const ws = new WebSocket('ws://localhost:9527');
const openai = new OpenAI();

ws.on('open', async () => {
    // 生成纹理
    const image = await openai.images.generate({
        model: "dall-e-3",
        prompt: "seamless rust metal PBR texture, 1024x1024",
    });

    // 下载保存
    const response = await fetch(image.data[0].url);
    const buffer = await response.arrayBuffer();
    fs.writeFileSync('data/textures/rust_metal.png', Buffer.from(buffer));

    // 通知引擎导入
    ws.send(JSON.stringify({
        jsonrpc: "2.0",
        id: 1,
        method: "dsengine_lua_execute",
        params: { code: "dse.assets.load_texture('textures/rust_metal.png')" }
    }));
});
```

#### Python 插件：带 Web UI 的关卡生成器

```python
# plugins/level_generator/server.py
from flask import Flask, render_template, request
import websocket, json

app = Flask(__name__)
engine_ws = None

@app.route('/')
def index():
    return render_template('index.html')  # Web UI

@app.route('/api/generate', methods=['POST'])
def generate():
    params = request.json
    ws = websocket.create_connection("ws://localhost:9527")

    # 用 Lua 批量创建实体
    lua_code = f"""
    for i = 1, {params['wall_count']} do
        local e = dse.ecs.create()
        dse.ecs.set_name(e, "Wall_" .. i)
        dse.ecs.set_transform(e, math.random(-20,20), 2, math.random(-20,20))
        dse.ecs.set_mesh(e, "models/cube.dmesh")
    end
    """
    ws.send(json.dumps({"jsonrpc": "2.0", "id": 1, "method": "dsengine_lua_execute", "params": {"code": lua_code}}))
    result = json.loads(ws.recv())
    ws.close()
    return result

if __name__ == '__main__':
    app.run(port=3001)  # 浏览器打开 localhost:3001
```

---

## 四、各层插件定位

| 层级 | 机制 | 语言 | 适用场景 | 现状 |
|------|------|------|---------|------|
| **游戏逻辑** | Lua 脚本 | Lua | 游戏玩法/AI/UI | ✅ 已完整 |
| **引擎模块** | 编译期静态链接 | C++ | 引擎核心功能扩展 | ✅ 已有 modules/ |
| **编辑器/工具插件** | Control Server | 任意 | 编辑器扩展/AI/自动化 | ✅ 已实现 |
| **原生高性能插件** | DLL + 纯 C API | C/C++ | 第三方二进制分发 | 🔲 远期可选 |

---

## 五、实施计划

Control Server 的实施已在 AI_INTEGRATION.md 中规划，此处不重复。插件系统是 Control Server 的自然延伸。

| 阶段 | 内容 | 增量工作 | 状态 |
|------|------|--------|------|
| Phase 1a | Control Server（WebSocket JSON-RPC） | ~1200 行 C++ | ✅ 完成 |
| Phase 1b | 插件发现 + 管理 | ~200 行（PluginManager 扫描 plugins/ + 启停子进程） | ✅ 完成 |
| Phase 1c | 编辑器 Plugin Manager 面板 | ~150 行 ImGui | ✅ 完成 |
| Phase 2 | 插件模板 + 文档 | Python / Node.js 模板项目 + 开发者指南 | ✅ 完成 |
| Phase 3 | 内建 AI Chat Panel | ImGui 面板 + Python LLM bridge (~660 行) | ✅ 完成 |

### Phase 1b：插件管理器

```
plugins/
├── batch_rename/
│   ├── plugin.json        ← 元数据
│   └── main.py
├── ai_texture_gen/
│   ├── plugin.json
│   ├── package.json
│   └── index.js
└── level_generator/
    ├── plugin.json
    ├── requirements.txt
    └── server.py
```

```json
// plugin.json
{
  "name": "AI Texture Generator",
  "version": "1.0.0",
  "author": "DSEngine",
  "description": "Generate PBR textures using AI",
  "runtime": "node",
  "entry": "index.js",
  "requires_ui": true,
  "ui_port": 3001
}
```

编辑器通过 `plugin.json` 发现插件，按 `runtime` 字段启动对应解释器，插件进程通过 WebSocket 连回 Control Server。

---

## 六、总结

| 问题 | 答案 |
|------|------|
| DSEngine 需要插件系统吗？ | 编辑器扩展和 AI 集成需要，但不需要传统 DLL 插件 |
| 最优方案？ | **Control Server（进程外，WebSocket JSON-RPC）** |
| 为什么不用 DLL？ | ABI 地狱、无生态、DSEngine 不需要二进制分发 |
| 为什么不用 Lua？ | 无标准库、无包管理、ImGui 绑定体验差、不适合编辑器工具 |
| Lua 做什么？ | 游戏逻辑脚本（已有，保持不变） |
| 与 AI 方案关系？ | **同一套 Control Server，一次实现，两个场景复用** |
| 工作量？ | ✅ 已完成：Control Server ~1200 行 + 插件管理 ~350 行 + MCP adapter ~300 行 Python |
