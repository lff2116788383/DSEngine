# DSEngine 编辑器文档

> `apps/editor_cpp/` — C++ ImGui 编辑器

## 文档索引

| 文档 | 内容 |
|------|------|
| [EDITOR_ARCHITECTURE.md](EDITOR_ARCHITECTURE.md) | 编辑器架构分析：技术栈、代码结构、功能完成度、方案评估、改进建议 |
| [AI_INTEGRATION.md](AI_INTEGRATION.md) | AI 集成方案：MCP/WebSocket Tool Provider、资产生成管线、Vibe Coding 工作流、成本收益 |
| [PLUGIN_SYSTEM.md](PLUGIN_SYSTEM.md) | 插件系统方案：方案选型（DLL/Lua/Python/Control Server）、进程外插件协议、示例 |
| [PLUGIN_DEVELOPMENT.md](PLUGIN_DEVELOPMENT.md) | 插件开发指南：WebSocket JSON-RPC 协议、23 个 Tool API 参考、快速开始 |
| [../design/EDITOR.md](../design/EDITOR.md) | 编辑器改造路线图（Phase 1-7 完整实施记录） |

## 快速概览

- **技术栈**：C++ / Dear ImGui (Docking) / GLFW / OpenGL / ImGuizmo / RapidJSON
- **代码规模**：~110 文件（.cpp+.h），`src/` ~30,664 行，~60 个功能模块/面板（2026-06-10 核实）
- **完成度**：Phase 1-7 全部交付，覆盖完整游戏编辑工作流
- **AI Control Server**：✅ WebSocket JSON-RPC + MCP adapter，**32 个内建 `dsengine_*` Tool**（实体/组件/场景/选中/预制体 CRUD + Lua/截图/资产导入）
- **新增已落地面板**：Shader Graph、Visual Script（数据流）、动画状态机、NavMesh、AI Chat Panel、Lua Debugger、Curve Editor、Multi-Viewport（可选）、Streaming Debug
- **架构改进**：✅ main.cpp → EditorApp 拆分，✅ Inspector 注册式重构 (29 组件)，✅ EditorContext 统一 + 全局变量消除
- **插件系统**：✅ 模板插件 (Python/Node.js) + 开发者指南 (PLUGIN_DEVELOPMENT.md)
- **测试**：编辑器专属 ~222 例（集成 ~206 + 单元 16）全绿
- **待打磨**：编辑器走 RHI、Visual Script 控制流生成、碰撞体可视化拖撞、Multi-viewport 默认开启
