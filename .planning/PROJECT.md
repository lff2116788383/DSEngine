# DSEngine

## What This Is

DSEngine 是一个面向个人独立开发者与中小型工作室的轻量级高性能 C++ 游戏引擎仓库，当前主线聚焦 **Windows + CMake + Visual Studio 2022** 下的 **2D Runtime、Lua/C++ 双业务宿主、原生 C++ 编辑器**。当前最准确的项目状态是：**2D 主线已基本成型并接近 Stable，编辑器基础可用，测试体系已建立，3D 已接入但仍处于 MVP 收口阶段，不是默认稳定主线。**

## Core Value

在保持轻量、高性能和可控复杂度的前提下，把 **2D Runtime + Lua/C++ 双宿主 + 原生编辑器 + 最小可执行测试门禁** 做成可持续迭代的开发闭环。

## Requirements

### Validated

- ✓ 2D Runtime 主链可运行，支持 C++ / Lua 双宿主 — 现有仓库主线
- ✓ 已具备基础原生编辑器外壳与常用面板 — 现有仓库主线
- ✓ 已建立 `CTest` 驱动的 engine / 2D / 3D / 资产链分层门禁 — 现有仓库主线
- ✓ 已接入 3D 组件、最小场景 gate 与资产导入入口 — 现有仓库主线

### Active

- [ ] 收口 `engine/runtime/` 中枢复杂度，降低 `FramePipeline` 与 runtime 装配风险
- [ ] 提升 `apps/editor_cpp/` 的高频可用性，优先改善场景编辑闭环与宿主层负担
- [ ] 稳定 2D 主线门禁，持续守住 Lua/C++ runtime、资源注入与高频 2D 子系统回归
- [ ] 在受控范围内推进 3D MVP，补齐最小场景、runtime smoke 与资产导入链的一致性
- [ ] 保持 README、测试文档、架构文档与实际实现口径同步

### Out of Scope

- 完整商用品质 3D 引擎定位 — 当前 README 与测试口径均表明 3D 仍是 MVP 收口方向
- 完整大型团队协作编辑器平台 — 当前编辑器更偏单机高频工具壳，不是平台化产品
- 完整资源数据库 / UUID / meta 工作流 — 当前资产链已接入，但尚未形成成熟全链路系统
- 在主线未收口前继续大面积扩展新功能面 — 当前更需要稳定 runtime、editor、测试与资产链主路径

## Context

- 当前仓库是 brownfield 项目，已有 `apps/`、`engine/`、`modules/`、`tests/` 四层清晰结构
- runtime 通过 `EngineInstance` 与 `FramePipeline` 驱动，`FramePipeline` 是当前高耦合中枢
- 编辑器位于 `apps/editor_cpp/`，直接链接 `dse_engine`，当前属于“引擎内嵌编辑器”形态
- 测试体系以 Windows 本地 `CTest` 为统一入口，已包含 2D 主线门禁、3D MVP gate 与资产导入门禁
- 当前仓库搜索噪音较大，`depends/` 与 `reference/` 需要与主代码区分对待
- 后续规划不应脱离既有 codebase map、README 与 `doc-archive/TESTING_CTEST_GUIDE.md` 的真实口径

## Constraints

- **Tech stack**: 核心实现必须延续 C++ / Lua / React(Tauri) 混合架构 — 仓库现状已成型，重写成本过高
- **Platform**: 当前默认验证平台是 Windows + MSVC + CMake — 现有脚本、构建和测试门禁都以此为基线
- **Architecture**: 2D 是默认稳定主线，3D 仅按 MVP 收口推进 — 避免 roadmap 偏离真实交付能力
- **Workflow**: 代码、测试、文档必须保持一致 — 当前仓库文档量大，漂移风险高
- **Complexity**: 优先收口已有关键中枢，而不是继续铺功能面 — `FramePipeline`、编辑器主程序和多链并行已形成复杂度压力

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| 以 brownfield 模式初始化 GSD 项目 | 仓库已有成熟代码与测试资产，不能按 greenfield 假设处理 | ✓ Good |
| 将当前核心价值聚焦为 2D/runtime/editor/testing 闭环 | 这与 README 和现有测试门禁口径一致 | ✓ Good |
| 将 3D 定位为受控 MVP 收口而非默认主线 | 避免 roadmap 高估当前真实能力 | ✓ Good |
| 优先规划 runtime/editor/testing/asset chain 收口 | 这些区域是当前复杂度和风险的真实集中点 | — Pending |

---
*Last updated: 2026-04-22 after brownfield GSD initialization*
