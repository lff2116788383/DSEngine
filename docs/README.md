# DSEngine 文档索引

## 📂 目录结构

```
docs/
├── getting-started/     入门指南
├── architecture/        架构设计
├── reference/           API 参考
├── roadmap/             路线图与进度
├── analysis/            引擎分析报告
├── design/              子系统设计文档
├── blog/                技术博客（27 篇）
└── _archive/            已归档的历史文档
```

---

## 🚀 入门指南

| 文档 | 说明 |
|------|------|
| [GETTING_STARTED.md](getting-started/GETTING_STARTED.md) | 构建、运行、创建首个项目 |

## 🏗️ 架构设计

| 文档 | 说明 |
|------|------|
| [ARCHITECTURE.md](architecture/ARCHITECTURE.md) | 引擎架构全景分析 |
| [RENDER_PIPELINE.md](architecture/RENDER_PIPELINE.md) | 渲染管线设计与优化方案 |
| [SHADER_SYSTEM.md](architecture/SHADER_SYSTEM.md) | DSSL 着色语言设计 |

## 📖 API 参考

| 文档 | 说明 |
|------|------|
| [LUA_API.md](reference/LUA_API.md) | Lua 脚本 API 参考（145+ API） |
| [GLOSSARY.md](reference/GLOSSARY.md) | 英文技术术语速查表 |

## 🗺️ 路线图与进度

| 文档 | 说明 |
|------|------|
| [PROGRESS_REPORT.md](roadmap/PROGRESS_REPORT.md) | 当前进度 · 对比主流引擎 · SDK 差距分析 |

## 📊 引擎分析报告

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

## 📝 技术博客

`blog/` 目录包含 27 篇面向初学者的技术科普文章，涵盖：
ECS、PBR、阴影、骨骼动画、粒子系统、GPU Instancing、LOD、反走样、Bloom、延迟渲染、Job System、PhysX、Lua 脚本、输入系统、光照模型演进、光子映射、RTXGI/DDGI 等。

## 🗄️ 归档

`_archive/` 包含已过期的会话指令、历史路线图和临时规划文档，仅供追溯参考。
