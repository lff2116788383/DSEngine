# Requirements: DSEngine

**Defined:** 2026-04-22
**Core Value:** 在保持轻量、高性能和可控复杂度的前提下，把 2D Runtime + Lua/C++ 双宿主 + 原生编辑器 + 最小可执行测试门禁 做成可持续迭代的开发闭环

## v1 Requirements

### Runtime Core

- [ ] **RT-01**: 引擎运行时可稳定支持 C++ 与 Lua 双宿主启动链路
- [ ] **RT-02**: `EngineInstance` 与 `FramePipeline` 的关键装配路径在默认主线上保持可回归验证
- [ ] **RT-03**: 资源根目录、启动场景与运行时模块装配行为可通过明确配置控制
- [ ] **RT-04**: Runtime 关键中枢改动后可通过最小 smoke 或回归 gate 发现主链破坏

### Editor Workflow

- [ ] **EDIT-01**: 原生编辑器可稳定完成场景加载、保存、基础实体操作与常用检视流程
- [ ] **EDIT-02**: 编辑器高频使用路径不依赖临时手工补救即可完成基础开发闭环
- [ ] **EDIT-03**: 编辑器宿主层复杂度控制在可持续维护范围内，新增功能优先复用现有面板结构

### 2D Stable

- [ ] **2D-01**: 2D 主线高频子系统（UI、Physics2D、Particle、Localization、Spine）保持最小回归门禁
- [ ] **2D-02**: Lua runtime、资源注入与 2D 模块协作链路在 Windows 本地持续可验证
- [ ] **2D-03**: 2D 稳定主线优先级高于新功能扩面，避免默认交付能力被回归破坏

### 3D MVP

- [ ] **3D-01**: 最小 3D MVP 场景可被场景 gate 与 runtime smoke 稳定加载
- [ ] **3D-02**: 3D 相关改动应受 `engine.3d.unit`、`engine.3d.scene_mvp`、`engine.3d.runtime_mvp_smoke` 等最小矩阵保护
- [ ] **3D-03**: 3D 资产导入与烹饪链至少对当前声明已支持的 glTF/GLB 主路径保持一致
- [ ] **3D-04**: 3D 路线图必须持续明确其为 MVP 收口范围，而非默认稳定主线

### Asset & Tooling

- [ ] **AST-01**: 资产导入工具可将最小 glTF/GLB/FBX 输入烹饪为运行时资产格式
- [ ] **AST-02**: 资产导入链的已支持能力与未接入边界需保持文档可追踪
- [ ] **AST-03**: 关键 build/test 脚本继续作为 Windows 本地主线工作流的可靠入口

### Verification & Docs

- [ ] **QA-01**: 所有核心改动至少关联一类真实验证（构建、CTest、专项回归或可执行 smoke）
- [ ] **QA-02**: 测试门禁命名、脚本入口与文档口径保持一致
- [ ] **QA-03**: README、架构文档、测试文档与实际代码状态在主线能力表述上保持同步
- [ ] **QA-04**: 规划与实现时应显式区分主代码、第三方依赖与 reference 区域，降低误判风险

## v2 Requirements

### Advanced Editor

- **ED2-01**: 完整 Play In Editor 隔离与稳定状态回滚
- **ED2-02**: 更完整的 Prefab、资源浏览与复杂编辑器工作流
- **ED2-03**: 更强的 Undo / Redo 指令系统与跨面板一致性

### Advanced 3D

- **3DV2-01**: 更成熟的 3D 资源工作流与编辑器闭环
- **3DV2-02**: 更完整的 3D 商用品质渲染、工具链与稳定性门禁
- **3DV2-03**: 更广泛的 3D runtime 与内容生产能力

## Out of Scope

| Feature | Reason |
|---------|--------|
| 将当前项目定义为成熟商用品质 3D 引擎 | 与 README、测试门禁和当前代码状态不符 |
| 在未收口主线前大规模扩展新子系统 | 当前复杂度压力主要来自中枢收口与已有链路稳定性 |
| 以全新技术栈重写现有 runtime/editor 主链 | 现有代码与测试资产体量已较大，重写成本过高 |
| 把 Launcher/Tauri 作为当前最核心交付物 | 当前仓库主线价值仍在引擎 runtime 与原生编辑器 |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| RT-01 | Phase 1 | Pending |
| RT-02 | Phase 1 | Pending |
| RT-03 | Phase 1 | Pending |
| RT-04 | Phase 1 | Pending |
| EDIT-01 | Phase 2 | Pending |
| EDIT-02 | Phase 2 | Pending |
| EDIT-03 | Phase 2 | Pending |
| 2D-01 | Phase 3 | Pending |
| 2D-02 | Phase 3 | Pending |
| 2D-03 | Phase 3 | Pending |
| 3D-01 | Phase 4 | Pending |
| 3D-02 | Phase 4 | Pending |
| 3D-03 | Phase 4 | Pending |
| 3D-04 | Phase 4 | Pending |
| AST-01 | Phase 5 | Pending |
| AST-02 | Phase 5 | Pending |
| AST-03 | Phase 5 | Pending |
| QA-01 | Phase 6 | Pending |
| QA-02 | Phase 6 | Pending |
| QA-03 | Phase 6 | Pending |
| QA-04 | Phase 6 | Pending |

**Coverage:**
- v1 requirements: 21 total
- Mapped to phases: 21
- Unmapped: 0 ✓

---
*Requirements defined: 2026-04-22*
*Last updated: 2026-04-22 after brownfield initialization*
