# DSEngine 文档索引

## 📂 目录结构

```
docs/
├── getting-started/     入门指南（构建 / 运行 / 测试）
├── architecture/        架构与渲染管线设计
├── api/                 C++ / Lua API 参考
├── reference/           术语表
├── roadmap/             路线图 / 进度 / 评估
├── plans/               功能 / 模块 / 集成实施方案
├── analysis/            引擎分析报告
├── design/              子系统设计文档
├── editor/              编辑器架构与插件系统
├── blog/                技术博客（27 篇）
└── _archive/            已归档的历史文档
```

功能 / 模块 / 集成的实施方案集中在 [`plans/`](plans/)（见下「实施方案」一节）。

---

## 🚀 入门指南

| 文档 | 说明 |
|------|------|
| [QUICKSTART.md](getting-started/QUICKSTART.md) | **新手首选**：只用 `dse` 命令行，30 分钟从零做出并导出一个 2D 小游戏 |
| [QUICKSTART.en.md](getting-started/QUICKSTART.en.md) | English quick start (same content, for non-Chinese readers) |
| [FAQ.md](getting-started/FAQ.md) | 常见问题 / FAQ（双语）：出包 / 运行 / 加密 / 软件渲染 等高频问题 |
| [TUTORIAL_2D_FIRST_GAME.md](getting-started/TUTORIAL_2D_FIRST_GAME.md) | 第一篇 2D 教程：金币收集小游戏（移动 / 碰撞 / 计分 / 通关） |
| [GETTING_STARTED.md](getting-started/GETTING_STARTED.md) | 面向贡献者：从源码构建引擎、使用编辑器、运行 Demo、测试 |

## 🏗️ 架构设计

| 文档 | 说明 |
|------|------|
| [ARCHITECTURE.md](architecture/ARCHITECTURE.md) | 引擎架构全景分析 |
| [RENDER_PIPELINE.md](architecture/RENDER_PIPELINE.md) | 渲染管线设计与优化方案 |
| [SHADER_SYSTEM.md](architecture/SHADER_SYSTEM.md) | DSSL 着色语言设计 |
| [SCRIPTABLE_RENDER_PIPELINE.md](architecture/SCRIPTABLE_RENDER_PIPELINE.md) | 可编程渲染管线 |
| [GPU_DRIVEN_MODULE_REFACTOR_PLAN.md](architecture/GPU_DRIVEN_MODULE_REFACTOR_PLAN.md) | GPU-Driven 模块重构方案 |
| [GPU_BUFFER_HANDLE_MIGRATION.md](architecture/GPU_BUFFER_HANDLE_MIGRATION.md) | GPU Buffer 句柄迁移 |
| [LARGE_WORLD_COORDINATES.md](architecture/LARGE_WORLD_COORDINATES.md) | 大世界坐标 |
| [PARALLELIZATION_PLAN.md](architecture/PARALLELIZATION_PLAN.md) | 并行化方案 |

## 📖 API 参考

| 文档 | 说明 |
|------|------|
| [LUA_API.md](api/LUA_API.md) | Lua 脚本 API 参考（严格对照 `engine/scripting/lua/bindings/` 源码） |
| [CPP_API.md](api/CPP_API.md) | C++ 公开 API 参考 |
| [API_GAP_ANALYSIS.md](api/API_GAP_ANALYSIS.md) | API 缺口分析 |
| [GLOSSARY.md](reference/GLOSSARY.md) | 英文技术术语速查表 |

## 🗺️ 路线图与进度

| 文档 | 说明 |
|------|------|
| [ADOPTION_ROADMAP.md](roadmap/ADOPTION_ROADMAP.md) | 采用路线图（基于源码现状核实） |
| [PROGRESS_REPORT.md](roadmap/PROGRESS_REPORT.md) | 当前进度 · 对比主流引擎 · SDK 差距分析 |
| [PROJECT_ASSESSMENT.md](roadmap/PROJECT_ASSESSMENT.md) | 项目评估（完成度 / 架构 / 竞争力 / 定位） |
| [BUSINESS_CLOSED_LOOP.md](roadmap/BUSINESS_CLOSED_LOOP.md) | 商业闭环规划 |

## 📋 实施方案

`plans/` 目录收录功能 / 模块 / 集成的实施方案：

| 文档 | 说明 |
|------|------|
| [AI_CHAT_INTEGRATION_PLAN.md](plans/AI_CHAT_INTEGRATION_PLAN.md) | AI 对话集成方案 |
| [CROSS_PLATFORM_PLAN.md](plans/CROSS_PLATFORM_PLAN.md) | 跨平台方案 |
| [CSHARP_SCRIPTING_PLAN.md](plans/CSHARP_SCRIPTING_PLAN.md) | C# 脚本方案 |
| [WEB_AND_DOCS_PLAN.md](plans/WEB_AND_DOCS_PLAN.md) | Web/WASM 导出与文档方案 |
| [HTTP_LUA_MODULE.md](plans/HTTP_LUA_MODULE.md) | HTTP Lua 模块 |
| [SERIALIZE_LUA_MODULE.md](plans/SERIALIZE_LUA_MODULE.md) | 序列化 Lua 模块 |
| [NETWORK_GNS_INTEGRATION_PLAN.md](plans/NETWORK_GNS_INTEGRATION_PLAN.md) | 网络层（GameNetworkingSockets）集成方案 |
| [NETWORK_GNS_PROGRESS.md](plans/NETWORK_GNS_PROGRESS.md) | 网络层集成进度 |
| [RHI_UNIFICATION_CLOSEOUT_PLAN.md](plans/RHI_UNIFICATION_CLOSEOUT_PLAN.md) | RHI 统一收尾方案 |

## 📊 引擎分析报告

`analysis/` 目录包含 14 篇分析报告（多为带日期的快照分析），重点：

| 文档 | 说明 |
|------|------|
| [ENGINE_COMPARISON.md](analysis/ENGINE_COMPARISON.md) | 与主流引擎及自研引擎横向比较 |
| [CODE_ANALYSIS.md](analysis/CODE_ANALYSIS.md) | 代码完整度分析 |
| [TEST_COVERAGE.md](analysis/TEST_COVERAGE.md) | 测试覆盖分析 |
| [COMPLETENESS.md](analysis/COMPLETENESS.md) | 图形学博客 vs 引擎实现完成度 |
| [2D_CAPABILITY.md](analysis/2D_CAPABILITY.md) | 2D 功能深度分析 |
| [ANIMATION_SYSTEM.md](analysis/ANIMATION_SYSTEM.md) | 动画系统与前沿技术对比 |
| [RHI_DX12.md](analysis/RHI_DX12.md) | RHI 后端 vs DX12 必要性评估 |
| [VSENGINE_REFERENCE.md](analysis/VSENGINE_REFERENCE.md) | VSEngine2.1 技术参考与可借鉴实践 |

## 🔧 子系统设计文档

| 文档 | 说明 |
|------|------|
| [PHYSICS.md](design/PHYSICS.md) | 高级物理功能规划（布料/流体/破碎） |
| [ANIMATION.md](design/ANIMATION.md) | 动画系统增强方案 |
| [EDITOR.md](design/EDITOR.md) | 编辑器改造路线图 |
| [LUA_3D_DEMOS.md](design/LUA_3D_DEMOS.md) | Lua 3D Demo Backlog 计划 |

## 🛠️ 编辑器

| 文档 | 说明 |
|------|------|
| [README.md](editor/README.md) | 编辑器文档导览 |
| [EDITOR_ARCHITECTURE.md](editor/EDITOR_ARCHITECTURE.md) | 编辑器架构 |
| [AI_INTEGRATION.md](editor/AI_INTEGRATION.md) | AI 集成 |
| [PLUGIN_SYSTEM.md](editor/PLUGIN_SYSTEM.md) | 插件系统 |
| [PLUGIN_DEVELOPMENT.md](editor/PLUGIN_DEVELOPMENT.md) | 插件开发指南 |

## 📦 示例

可运行的 Lua 示例脚本位于仓库顶层 [`examples/lua/`](../examples/lua/)：`deepseek_npc.lua`、`json.lua`、`net_loopback.lua`、`net_message.lua`。

## 📝 技术博客

`blog/` 目录包含 27 篇面向初学者的技术科普文章，涵盖：
ECS、PBR、阴影、骨骼动画、粒子系统、GPU Instancing、LOD、反走样、Bloom、延迟渲染、Job System、PhysX、Lua 脚本、输入系统、光照模型演进、光子映射、RTXGI/DDGI 等。

## 🗄️ 归档

`_archive/` 包含已过期的会话指令、历史路线图和临时规划文档，仅供追溯参考。
