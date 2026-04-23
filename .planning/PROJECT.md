# DSEngine

## What This Is

DSEngine 是一个面向个人独立开发者与中小型工作室的轻量级高性能 C++ 游戏引擎仓库。当前主线不再以“先把编辑器打磨成核心入口”为第一目标，而是优先验证：**现有 runtime、3D 模块、Lua 脚本层与资产链，是否已经足以支撑一个可持续迭代的 3D 游戏原型开发闭环**。

结合当前仓库真实状态，最准确的项目定位应调整为：**Windows + CMake + Visual Studio 2022 下，以 Lua/C++ 双宿主为基础，优先推进 3D runtime、3D 场景、3D 资产导入与 Lua gameplay 驱动的实战原型能力；2D 继续作为稳定基线守护；原生编辑器后置到技术路线明确后再集中收口。**

## Core Value

在保持轻量、高性能和可控复杂度的前提下，把 **Runtime + Lua/C++ 双宿主 + 3D 模块 + 资产导入链 + 最小可执行测试门禁** 做成可支撑 **Lua 驱动 3D 游戏原型** 的开发闭环，并以此验证引擎进入真实项目实战的可行性。

## Requirements

### Validated

- ✓ Runtime 主链可运行，支持 C++ / Lua 双宿主 — 现有仓库主线
- ✓ 已建立 `CTest` 驱动的 engine / 2D / 3D / 资产链分层门禁 — 现有仓库主线
- ✓ 已接入 3D 组件、最小场景 gate、runtime smoke 与资产导入入口 — 现有仓库主线
- ✓ 已具备基础原生编辑器外壳与场景桥接能力 — 当前存在，但暂不作为近期主目标

### Active

- [ ] 收口 `engine/runtime/` 中枢复杂度，降低 `FramePipeline` 与 runtime 装配风险
- [ ] 优先验证 3D runtime、3D 场景、3D 资产链与 Lua 脚本层能否形成真实原型闭环
- [ ] 补齐 Lua 驱动 3D gameplay 所需的核心绑定与最小工作流
- [ ] 稳定 3D MVP 门禁，避免 3D 路径在原型推进中被静默破坏
- [ ] 维持 2D 主线门禁与既有稳定能力，不让 3D 推进反向破坏默认基线
- [ ] 保持 README、测试文档、架构文档与实际实现口径同步

### Out of Scope

- 在当前阶段把原生编辑器打造成成熟主线产品 — 当前 ImGui 方案视觉与技术路线未定，不适合作为近期最高优先级
- 将当前项目定义为成熟商用品质 3D 引擎 — 现有仓库更接近“具备 3D 实战潜力的 MVP 引擎底座”
- 在未验证 Lua 3D gameplay 原型闭环前，大规模扩展复杂工具链或平台化编辑器能力
- 以全新技术栈重写现有 runtime / scripting / asset 主链

## Context

- 当前仓库是 brownfield 项目，已有 `apps/`、`engine/`、`modules/`、`tests/` 四层清晰结构
- runtime 通过 `EngineInstance` 与 `FramePipeline` 驱动，`FramePipeline` 是当前高耦合中枢
- 3D 能力已分布在 `modules/gameplay_3d/`、`assets/scenes/3d_mvp_minimal.scene.json`、reference demo scene、3D 测试门禁与资产导入链中
- Lua 已是重要业务脚本层，且仓库已有多个 3D Lua demo / sample，可作为实战原型的真实切入点
- 编辑器位于 `apps/editor_cpp/`，当前基础可用，但近期应从“前置主线”降级为“路线待定的后置能力”
- 测试体系以 Windows 本地 `CTest` 为统一入口，已包含 3D MVP gate、Lua runtime gate 与资产导入 gate
- 后续规划不应脱离现有 codebase map、README 与 `doc-archive/TESTING_CTEST_GUIDE.md` 的真实口径

## Constraints

- **Tech stack**: 核心实现必须延续 C++ / Lua / React(Tauri) 混合架构 — 仓库现状已成型，重写成本过高
- **Platform**: 当前默认验证平台是 Windows + MSVC + CMake — 现有脚本、构建和测试门禁都以此为基线
- **Architecture**: 当前最现实的目标是把 3D 做到“可支撑 Lua 游戏原型”，而不是提前承诺成熟商用品质
- **Workflow**: 代码、测试、文档必须保持一致 — 当前仓库文档量大，漂移风险高
- **Priority**: 编辑器后置，但不能让 3D 推进反向破坏 2D 基线、runtime 主链和资产链真实性

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| 以 brownfield 模式初始化 GSD 项目 | 仓库已有成熟代码与测试资产，不能按 greenfield 假设处理 | ✓ Good |
| 将当前核心价值调整为 runtime + Lua 3D 原型闭环 | 更贴合“尽快投入实战、优先推进 3D”的真实目标 | ✓ Good |
| 将编辑器从近期前置主线调整为后置阶段 | 当前 ImGui 路线体验与技术方向未定，继续前置投入性价比低 | ✓ Good |
| 将 3D 从“远期 MVP 收口项”提升为近期主线 | 当前最重要的是验证引擎是否足以支撑 Lua 驱动的 3D 游戏原型 | ✓ Good |
| 保留 2D 与测试/文档对齐作为守护性工作 | 避免 3D 推进中破坏仓库现有稳定基线 | — Pending |

---
*Last updated: 2026-04-23 after strategy shift toward Lua-driven 3D prototype readiness*
