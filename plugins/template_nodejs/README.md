# AI Texture Generator Plugin (Node.js Template)

使用 OpenAI DALL-E 3 生成 PBR 纹理并导入 DSEngine 的编辑器插件模板。

## 功能

- 通过文字描述生成高质量纹理
- 自动导入生成的纹理到引擎资产系统
- 支持单次生成和交互模式
- 演示 Node.js 插件的 WebSocket JSON-RPC 协议用法

## 安装

```bash
npm install
```

## 环境变量

```bash
set OPENAI_API_KEY=sk-your-key-here
```

## 使用

### 单次生成

```bash
# 生成一张锈迹金属纹理
node index.js --prompt "seamless rust metal PBR texture, 1024x1024" --output data/textures/rust_metal.png

# 生成高清风格化纹理
node index.js --prompt "hand-painted stone wall game texture" --size 1024x1024 --quality hd --style vivid
```

### 交互模式

```bash
node index.js --interactive
# 然后逐行输入描述，每行生成一张纹理
```

### 通过 Plugin Manager

1. 将此目录放入 `plugins/` 下
2. 运行 `npm install` 安装依赖
3. 在编辑器中打开 **Plugin Manager** 面板
4. 点击 **Start** 启动插件

## 协议说明

插件通过 WebSocket 连接到 `ws://127.0.0.1:9527`，使用 JSON-RPC 2.0 协议。

### 请求示例

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "dsengine_asset_import",
    "params": {
        "path": "textures/rust_metal.png",
        "type": "texture"
    }
}
```

### 响应示例

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "success": true,
        "asset_path": "textures/rust_metal.png",
        "texture_handle": 12345
    }
}
```

## 作为模板使用

1. 复制此目录为新插件名称
2. 修改 `plugin.json` 中的元数据
3. 修改 `package.json` 的 name/description
4. 在 `index.js` 中实现你的逻辑
5. 使用 `DSEngineClient` 类调用任意 Control Server Tool

## 可用 API (23 个 Tool)

| Tool | 功能 |
|------|------|
| `dsengine_ping` | 连通性测试 |
| `dsengine_scene_get_state` | 获取场景实体列表 |
| `dsengine_scene_save` / `scene_load` | 场景读写 |
| `dsengine_entity_create` | 创建实体 |
| `dsengine_entity_modify` | 修改实体 |
| `dsengine_entity_delete` | 删除实体 |
| `dsengine_entity_add_component` | 添加组件 |
| `dsengine_entity_remove_component` | 移除组件 |
| `dsengine_entity_get_components` | 查询组件 |
| `dsengine_lua_execute` | 执行 Lua 代码 |
| `dsengine_script_create` | 创建 Lua 脚本 |
| `dsengine_editor_get_state` | 编辑器状态 |
| `dsengine_editor_play` / `stop` | Play 模式控制 |
| `dsengine_editor_undo` / `redo` | 撤销/重做 |
| `dsengine_editor_screenshot` | 截图 |
| `dsengine_asset_import` | 导入资产 |
| `dsengine_material_create` | 创建材质 |
| `dsengine_asset_generate_texture` | AI 纹理生成 |
| `dsengine_asset_generate_model` | AI 模型生成 |
| `dsengine_asset_generate_sfx` | AI 音效生成 |

完整 API 参考见 `docs/editor/PLUGIN_DEVELOPMENT.md`。
