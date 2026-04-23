# Roadmap: DSEngine

**Created:** 2026-04-22
**Source:** `.planning/PROJECT.md` + `.planning/REQUIREMENTS.md` + `.planning/codebase/`

## Milestone v1 — Lua 驱动 3D 原型实战化闭环

目标：围绕现有 brownfield 仓库的真实状态，在已完成 runtime 中枢首轮收口的基础上，优先解决 **引擎内部 bug、崩溃、超时、目标缺失与核心回归问题**，并据此验证 **3D runtime + 3D 场景 + 3D 资产链 + Lua gameplay 脚本层** 是否足以支撑一个可持续迭代的 3D 游戏原型；2D 继续作为稳定基线守护；编辑器延后到技术路线明确后再集中决策。

| Phase | Name | Description | Requirements |
|------|------|-------------|--------------|
| 1 | Runtime 中枢收口 | 降低 `engine/runtime/` 关键装配链复杂度，守住双宿主与主循环主线 | RT-01, RT-02, RT-03, RT-04 |
| 2 | GTest 全引擎测试基线 | 基于新引入的本地 GoogleTest，对 `dse_engine` 建立核心单元测试骨架与可扩展 CTest 标签体系 | QA-01, QA-02, QA-04 |
| 3 | GMock 集成测试与系统交互验证 | 在 GTest 基线稳定后，引入 GMock 覆盖 runtime、asset、module、Lua/C++ bridge 等系统交互边界 | QA-01, QA-02, RT-02, RT-03 |
| 4 | 引擎稳定性与 3D 崩溃清零 | 基于新测试基线跑通 runtime / 3D / Lua / asset 最小矩阵，定位并修复阻塞原型推进的内部问题 | 3D-01, 3D-02, 3D-04, QA-01 |
| 5 | Lua 3D Gameplay 与资产链闭环 | 补齐 Lua 驱动 3D gameplay 所需的核心绑定、脚本工作流与资产导入主链 | 3D-03, L3D-01, L3D-02, AST-01, AST-03 |
| 6 | 3D Playable Prototype 基线 | 用一个 Lua 驱动的 3D playable prototype / reference demo 固化实战基线 | L3D-03, L3D-04 |
| 7 | 2D Stable 基线守护 | 保持 2D 主线与既有 Lua/runtime/资源注入链路不被 3D 推进破坏 | 2D-01, 2D-02, 2D-03 |
| 8 | Google Benchmark 性能专项 | 经用户确认引入 Google Benchmark 后，为 ECS、Asset、Lua binding、render/update 等关键路径建立性能基线 | QA-01, QA-02 |
| 9 | 测试与文档口径对齐 | 对齐 GTest/GMock/Benchmark、3D、Lua、资产链与主线文档/测试口径，收紧真实能力边界 | AST-02, QA-02, QA-03, QA-04 |
| 10 | Editor 路线重估与后置收口 | 在技术路线明确后，再决定编辑器该如何服务真实内容生产 | EDIT-01, EDIT-02, EDIT-03 |

## Phase Details

### Phase 1: Runtime 中枢收口

**Why now:** `FramePipeline` 与 runtime 装配链是所有宿主、3D runtime、Lua 启动链与资产链的共同底座；若 runtime 中枢持续失控，后续任何 3D 实战推进都会建立在不稳地基上。

**Scope:**
- 梳理 `EngineInstance` / `FramePipeline` / runtime context 边界
- 明确依赖注入与旧兼容路径的取舍
- 让关键运行链路具备更稳定的可回归验证入口

**Depends on:** None

### Phase 2: 引擎稳定性与 3D 崩溃清零

**Why now:** 在继续扩展 3D gameplay 或评估“能否做实战原型”之前，必须先确认当前引擎底座不会因为 bug、崩溃、超时、目标缺失或假阳性而给出错误信号；否则后续所有 3D 结论都不可靠。

**Scope:**
- 跑通 `engine.cpp_runtime`、`engine.lua_runtime`、`engine.resource_injection`、`engine.unit`、`engine.lua_runtime.smoke` 的最小 runtime 回归
- 固化 `engine.3d.unit`、`engine.3d.scene_mvp`、`engine.3d.runtime_mvp_smoke` 最小矩阵
- 补跑 `engine.asset_compiler` 并确认资产链主路径真实性
- 优先修复阻塞 Lua / runtime / 3D 原型推进的 bug、崩溃、超时与目标缺失问题
- 明确哪些能力是“已稳定”、哪些只是“已接入未收口”

**Depends on:** Phase 1

### Phase 3: Lua 3D Gameplay 与资产链闭环

**Why now:** 只有在稳定性问题先被收口后，Lua 侧对 3D gameplay 的真实能力边界才值得继续补齐；否则会把问题从 runtime 层错误地归咎到脚本层。

**Scope:**
- 梳理并补齐 Lua 侧对 3D 场景、实体、组件、相机或等价 gameplay 能力的真实绑定
- 固化 glTF/GLB/FBX → 运行时资产的最小主链
- 让 Lua 3D sample / demo 能作为后续原型开发的真实起点

**Depends on:** Phase 1, Phase 2

### Phase 4: 3D Playable Prototype 基线

**Why now:** 引擎是否能投入实战，不能只靠组件清单和门禁名判断；必须用一个真实可玩的 Lua 3D prototype 来验证 gameplay、场景、资产与宿主链是否形成闭环。

**Scope:**
- 选择一个 Lua 驱动的 3D sample / reference demo 作为 playable prototype 基线
- 为其补最小 smoke / 回归入口与验证说明
- 明确“当前能做什么、还不能做什么”的真实实战边界

**Depends on:** Phase 2, Phase 3

### Phase 5: 2D Stable 基线守护

**Why now:** 2D 仍是仓库当前默认稳定主线；若在推进 3D 的过程中破坏 2D、Lua runtime 或资源注入链，整体工程反而会退步。

**Scope:**
- 守住 UI / Physics2D / Particle / Localization / Spine 的主门禁
- 保持 Lua runtime、资源注入与 2D 协作链路可验证
- 明确 3D 推进过程中必须长期保留的默认稳定基线

**Depends on:** Phase 1

### Phase 6: 测试与文档口径对齐

**Why now:** 一旦稳定性矩阵、3D、Lua、资产链成为近期主线，README / 测试文档 / 规划口径就必须同步修正，否则后续实现和判断会被过时上下文拖偏。

**Scope:**
- 对齐 README、测试指南、专题文档与实际实现口径
- 明确主线门禁、推荐命令与当前 3D / Lua 原型能力边界
- 降低 AI / 人工在 brownfield 仓库中被旧文档误导的概率

**Depends on:** Phase 2, Phase 3, Phase 4, Phase 5

### Phase 7: Editor 路线重估与后置收口

**Why now:** 编辑器并非没有价值，而是当前 ImGui 方案视觉体验与技术路线都未确定；在 3D 原型主线验证前继续前置投入，性价比偏低。

**Scope:**
- 重新评估原生编辑器、Tauri 辅助壳层或其他路线的长期价值
- 明确编辑器是否真正服务 Lua 3D 内容生产闭环
- 在路线明确后再决定是否恢复高优先级投入

**Depends on:** Phase 4, Phase 6

## Summary

- Total phases: 7
- Current recommendation: `Phase 1` 已完成后，进入 `Phase 2`
- Suggested next command: `/gsd-plan-phase 2`

---
*Last updated: 2026-04-23 after gsd-health repair aligned roadmap with stability-first execution*
