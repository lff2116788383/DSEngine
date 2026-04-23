# Requirements: DSEngine

**Defined:** 2026-04-22
**Core Value:** 在保持轻量、高性能和可控复杂度的前提下，把 Runtime + Lua/C++ 双宿主 + 3D 模块 + 资产导入链 + 最小可执行测试门禁 做成可支撑 Lua 驱动 3D 游戏原型的开发闭环。

## v1 Requirements

### Runtime Core

- [ ] **RT-01**: 引擎运行时可稳定支持 C++ 与 Lua 双宿主启动链路
- [ ] **RT-02**: `EngineInstance` 与 `FramePipeline` 的关键装配路径在默认主线上保持可回归验证
- [ ] **RT-03**: 资源根目录、启动场景与运行时模块装配行为可通过明确配置控制
- [ ] **RT-04**: Runtime 关键中枢改动后可通过最小 smoke 或回归 gate 发现主链破坏

### 3D Prototype Readiness

- [ ] **3D-01**: 最小 3D MVP 场景可被场景 gate 与 runtime smoke 稳定加载
- [ ] **3D-02**: 3D 相关改动应受 `engine.3d.unit`、`engine.3d.scene_mvp`、`engine.3d.runtime_mvp_smoke` 等最小矩阵保护
- [ ] **3D-03**: 3D 资产导入与烹饪链至少对当前声明已支持的 glTF/GLB 主路径保持一致
- [ ] **3D-04**: 3D 路线图必须持续明确其为“Lua 3D 原型闭环”优先，而非空泛扩面

### Lua 3D Gameplay

- [ ] **L3D-01**: Lua 侧可稳定驱动最小 3D 场景启动、实体创建/装配或等价 gameplay 闭环
- [ ] **L3D-02**: Lua 脚本可访问 3D 原型开发所需的核心能力边界，且 API 口径来自真实绑定
- [ ] **L3D-03**: 至少存在一个 Lua 驱动的 3D playable prototype / reference demo，能作为后续游戏开发基线
- [ ] **L3D-04**: Lua 3D gameplay 改动具备最小 smoke / 回归入口，避免脚本层能力静默退化

### Asset & Tooling

- [ ] **AST-01**: 资产导入工具可将最小 glTF/GLB/FBX 输入烹饪为运行时资产格式
- [ ] **AST-02**: 资产导入链的已支持能力与未接入边界需保持文档可追踪
- [ ] **AST-03**: 关键 build/test 脚本继续作为 Windows 本地主线工作流的可靠入口

### 2D Baseline Guard

- [ ] **2D-01**: 2D 主线高频子系统（UI、Physics2D、Particle、Localization、Spine）保持最小回归门禁
- [ ] **2D-02**: Lua runtime、资源注入与 2D 模块协作链路在 Windows 本地持续可验证
- [ ] **2D-03**: 3D 推进不得反向破坏 2D 默认稳定主线

### Verification & Docs

- [ ] **QA-01**: 所有核心改动至少关联一类真实验证（构建、CTest、专项回归或可执行 smoke）
- [ ] **QA-02**: 测试门禁命名、脚本入口与文档口径保持一致
- [ ] **QA-03**: README、架构文档、测试文档与实际代码状态在主线能力表述上保持同步
- [ ] **QA-04**: 规划与实现时应显式区分主代码、第三方依赖与 reference 区域，降低误判风险

## v2 Requirements

### Editor Re-evaluation

- **EDIT-01**: 在技术路线明确后，再决定原生编辑器、Tauri 辅助壳层或其他方案的长期定位
- **EDIT-02**: 编辑器后续投入应围绕真实内容生产需求，而不是仅围绕当前 ImGui 壳层继续堆功能
- **EDIT-03**: 编辑器恢复前置优先级前，应先证明其对 Lua 3D 实战开发闭环有明确价值

### Advanced 3D Production

- **3DV2-01**: 更成熟的 3D 资源工作流、调试体验与内容生产闭环
- **3DV2-02**: 更完整的 3D 渲染、性能与稳定性门禁
- **3DV2-03**: 面向真实项目的更广泛 3D runtime 与 gameplay 生产能力

## Out of Scope

| Feature | Reason |
|---------|--------|
| 在当前阶段把 DSEngine 定义为成熟商用品质 3D 引擎 | 与当前代码、测试门禁与文档口径不符 |
| 在 Lua 3D 原型闭环尚未验证前，优先打磨编辑器外观或平台化工具链 | 不符合“尽快投入实战”的当前目标 |
| 在未收口 3D runtime / asset / Lua gameplay 主链前大规模扩展新子系统 | 当前更需要验证最短实战闭环 |
| 以全新技术栈重写现有 runtime / scripting / asset 主链 | 现有代码与测试资产体量已较大，重写成本过高 |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| RT-01 | Phase 1 | Completed |
| RT-02 | Phase 1 | Completed |
| RT-03 | Phase 1 | Completed |
| RT-04 | Phase 1 | Completed |
| 3D-01 | Phase 2 | Pending |
| 3D-02 | Phase 2 | Pending |
| 3D-03 | Phase 3 | Pending |
| 3D-04 | Phase 2 | Pending |
| L3D-01 | Phase 3 | Pending |
| L3D-02 | Phase 3 | Pending |
| L3D-03 | Phase 4 | Pending |
| L3D-04 | Phase 4 | Pending |
| AST-01 | Phase 3 | Pending |
| AST-02 | Phase 6 | Pending |
| AST-03 | Phase 3 | Pending |
| 2D-01 | Phase 5 | Pending |
| 2D-02 | Phase 5 | Pending |
| 2D-03 | Phase 5 | Pending |
| QA-01 | Phase 2 | Pending |
| QA-02 | Phase 6 | Pending |
| QA-03 | Phase 6 | Pending |
| QA-04 | Phase 6 | Pending |
| EDIT-01 | Phase 7 | Pending |
| EDIT-02 | Phase 7 | Pending |
| EDIT-03 | Phase 7 | Pending |

**Coverage:**
- v1 requirements: 22 total
- Mapped to phases: 22
- Unmapped: 0 ✓

---
*Requirements defined: 2026-04-22*
*Last updated: 2026-04-23 after gsd-health repair synchronized traceability and phase mapping*
