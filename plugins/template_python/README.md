# Batch Rename Plugin (Python Template)

批量重命名场景实体的 DSEngine 编辑器插件模板。

## 功能

- 通过正则表达式匹配并替换实体名称
- 为实体名称添加前缀/后缀
- 演示 Control Server WebSocket JSON-RPC 协议用法

## 安装

```bash
pip install -r requirements.txt
```

## 使用

### 独立运行

```bash
# 将所有名为 "Enemy*" 的实体重命名为 "Mob*"
python main.py --pattern "Enemy" --replace "Mob"

# 为所有实体添加前缀
python main.py --prefix "Level1_"

# 正则替换 + 保持运行（由 Plugin Manager 管理生命周期）
python main.py --pattern "^Cube_(\d+)$" --replace "Box_\1" --keep-alive
```

### 通过 Plugin Manager

1. 将此目录放入 `plugins/` 下
2. 在编辑器中打开 **Plugin Manager** 面板
3. 点击 **Start** 启动插件

## 协议说明

插件通过 WebSocket 连接到 `ws://127.0.0.1:9527`，使用 JSON-RPC 2.0 协议：

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "dsengine_scene_get_state",
    "params": {"include_components": false}
}
```

响应：

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "entities": [
            {"id": 1, "name": "MainCamera"},
            {"id": 2, "name": "Enemy_01"}
        ]
    }
}
```

## 作为模板使用

1. 复制此目录为新插件名称
2. 修改 `plugin.json` 中的元数据
3. 在 `main.py` 中实现你的逻辑
4. 使用 `DSEngineClient` 类调用任意 Control Server Tool

可用的 23 个 Tool 完整列表见 `docs/editor/PLUGIN_DEVELOPMENT.md`。

## plugin.json 字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | string | 插件显示名称 |
| `version` | string | 语义化版本号 |
| `author` | string | 作者 |
| `description` | string | 功能描述 |
| `runtime` | string | `"python"` / `"node"` / `"executable"` |
| `entry` | string | 入口文件（相对于插件目录） |
| `requires_ui` | bool | 是否有 Web UI |
| `ui_port` | int | Web UI 端口（仅 requires_ui=true 时） |
