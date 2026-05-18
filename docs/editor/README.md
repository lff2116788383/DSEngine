# DSEngine 编辑器文档

> `apps/editor_cpp/` — C++ ImGui 编辑器

## 文档索引

| 文档 | 内容 |
|------|------|
| [EDITOR_ARCHITECTURE.md](EDITOR_ARCHITECTURE.md) | 编辑器架构分析：技术栈、代码结构、功能完成度、方案评估、改进建议 |
| [AI_INTEGRATION.md](AI_INTEGRATION.md) | AI 集成方案：MCP/WebSocket Tool Provider、资产生成管线、Vibe Coding 工作流、成本收益 |
| [PLUGIN_SYSTEM.md](PLUGIN_SYSTEM.md) | 插件系统方案：方案选型（DLL/Lua/Python/Control Server）、进程外插件协议、示例 |
| [../design/EDITOR.md](../design/EDITOR.md) | 编辑器改造路线图（Phase 1-7 完整实施记录） |

## 快速概览

- **技术栈**：C++ / Dear ImGui (Docking) / GLFW / OpenGL / ImGuizmo / RapidJSON
- **代码规模**：~65 文件，~14,000 行，25 个功能面板
- **完成度**：Phase 1-7 全部交付，覆盖完整游戏编辑工作流
- **AI Control Server**：✅ Phase 1+2 完成 — WebSocket JSON-RPC + MCP adapter + AI 资产生成，23 个 Tool（13 种组件 CRUD + DALL·E/Meshy/ElevenLabs）
- **架构改进**：✅ main.cpp → EditorApp 拆分，✅ Inspector 注册式重构 (29 组件)，✅ EditorContext 统一 + 全局变量消除
- **下一步**：编辑器走 RHI / Phase 3 内建 Chat Panel (可选) / 插件模板+文档
