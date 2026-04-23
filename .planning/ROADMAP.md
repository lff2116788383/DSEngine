# Roadmap: DSEngine

**Created:** 2026-04-22
**Source:** `.planning/PROJECT.md` + `.planning/REQUIREMENTS.md` + `.planning/codebase/`

## Milestone v1 — 收口当前主线并建立可持续演进基线

目标：围绕现有 brownfield 仓库的真实状态，优先收口 runtime / editor / 2D / 3D MVP / asset chain / testing-doc alignment 六条主线，避免继续扩散复杂度。

| Phase | Name | Description | Requirements |
|------|------|-------------|--------------|
| 1 | Runtime 中枢收口 | 降低 `engine/runtime/` 关键装配链复杂度，守住双宿主与主循环主线 | RT-01, RT-02, RT-03, RT-04 |
| 2 | Editor 高频闭环增强 | 提升原生编辑器高频流程稳定性，降低宿主壳层维护压力 | EDIT-01, EDIT-02, EDIT-03 |
| 3 | 2D Stable 门禁巩固 | 守住 2D 主线高频模块、Lua/runtime/资源注入协作链路 | 2D-01, 2D-02, 2D-03 |
| 4 | 3D MVP 受控收口 | 在不冒充成熟主线的前提下，补齐 3D MVP 的最小稳定闭环 | 3D-01, 3D-02, 3D-03, 3D-04 |
| 5 | 资产链与工具链对齐 | 稳定资产导入/烹饪链，并维持 Windows 本地脚本工作流可靠性 | AST-01, AST-02, AST-03 |
| 6 | 测试与文档口径对齐 | 保持测试 gate、README、专题文档与当前实现持续一致 | QA-01, QA-02, QA-03, QA-04 |

## Phase Details

### Phase 1: Runtime 中枢收口

**Why now:** `FramePipeline` 与 runtime 装配链是当前最大的复杂度与风险源，且会影响 editor、2D、3D 与测试的所有后续工作。

**Scope:**
- 梳理 `EngineInstance` / `FramePipeline` / runtime context 边界
- 明确依赖注入与旧兼容路径的取舍
- 让关键运行链路具备更稳定的可回归验证入口

**Depends on:** None

### Phase 2: Editor 高频闭环增强

**Why now:** 原生编辑器已是当前主线宿主之一，但 `apps/editor_cpp/src/main.cpp` 仍偏重，若不先稳住高频闭环，后续功能扩展会持续放大壳层复杂度。

**Scope:**
- 聚焦场景加载/保存、常用面板、实体操作与检视主路径
- 优先改善高频使用摩擦与宿主层过载问题
- 保持“直接复用 runtime 核心能力”的当前路线

**Depends on:** Phase 1

### Phase 3: 2D Stable 门禁巩固

**Why now:** 2D 是当前默认稳定主线，必须优先守住 Lua/C++ runtime、资源注入与高频子系统的回归门禁。

**Scope:**
- 固化 UI / Physics2D / Particle / Localization / Spine 的主门禁
- 减少 2D 主线因其他链路改动被静默破坏
- 明确哪些 gate 是当前日常最小验证集合

**Depends on:** Phase 1

### Phase 4: 3D MVP 受控收口

**Why now:** 3D 已接入且已有最小测试矩阵，但当前只能以 MVP 口径推进，必须受控收口而不是扩面。

**Scope:**
- 维持最小 3D 场景 gate 与 runtime smoke 的真实性
- 收紧 3D 资产链、runtime 与测试的边界口径
- 避免对外或对内把 3D 当成默认稳定主线

**Depends on:** Phase 1, Phase 5

### Phase 5: 资产链与工具链对齐

**Why now:** 资产导入与烹饪链是 runtime 与 3D MVP 的基础，而且 `AssetBuilder` 与测试 gate 已经存在，应优先让其稳定、可验证、边界清晰。

**Scope:**
- 稳定 glTF/GLB/FBX 到运行时资产的最小主链
- 对齐已支持能力、未接入边界与工具用法
- 保持 `build_*.bat` 脚本在 Windows 主线上可靠可用

**Depends on:** Phase 1

### Phase 6: 测试与文档口径对齐

**Why now:** 当前仓库文档量大、测试门禁多，若代码/测试/文档不同步，后续任何规划都容易被错误上下文拖偏。

**Scope:**
- 对齐 README、测试指南、专题文档与实际实现口径
- 明确主线门禁、推荐命令与现状边界
- 降低 AI / 人工在 brownfield 仓库中被旧文档误导的概率

**Depends on:** Phase 2, Phase 3, Phase 4, Phase 5

## Summary

- Total phases: 6
- Current recommendation: 从 `Phase 1` 开始
- Suggested next command: `/gsd-plan-phase 1`

---
*Last updated: 2026-04-22 after initial brownfield roadmap creation*
