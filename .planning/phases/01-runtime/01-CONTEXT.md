# Phase 1: Runtime 中枢收口 - Context

**Gathered:** 2026-04-22
**Status:** Ready for planning

<domain>
## Phase Boundary

本 phase 只聚焦 `engine/runtime/` 中枢收口：明确 `EngineInstance`、`FramePipeline` 与 runtime context 的职责边界，优先把不该挂在 runtime 初始化中的副作用逻辑移出，同时确保现有 C++ / Lua 双宿主启动链与最小 runtime smoke 主链不退化。该 phase 不扩展新能力，不进入 editor 新功能、2D 新模块或 3D 扩面。

</domain>

<decisions>
## Implementation Decisions

### 角色边界
- **D-01:** 锁定边界为：`EngineInstance` 负责应用生命周期与服务装配。
- **D-02:** `FramePipeline` 聚焦逐帧调度与直接渲染相关初始化，不再作为“什么都装进去”的中枢容器继续膨胀。
- **D-03:** runtime context 只承担依赖与状态载体职责，不承接高层业务决策或额外副作用流程。

### 收口策略
- **D-04:** 本 phase 采用“保守但方向明确”的收口方式：先建立边界，再逐步迁移最不合理的职责，而不是一次性把 `FramePipeline` 激进改造成纯帧执行器。
- **D-05:** 优先处理边界最脏、最不该存在于主初始化路径中的职责，再考虑更重的系统装配拆分。

### 优先移出职责
- **D-06:** `FramePipeline` 中优先移出的职责是回归 / 样例 / 自检类副作用逻辑。
- **D-07:** 像 scene regression 这类逻辑，不应继续挂在 runtime 主初始化路径里。
- **D-08:** 重系统装配逻辑与运行模式分支仍是后续关注点，但在本 phase 中优先级低于副作用逻辑剥离。

### 验收信号
- **D-09:** 本 phase 的首要验收信号不是“拆得多漂亮”，而是现有双宿主启动链不退化。
- **D-10:** 最小 runtime smoke 必须保持可运行，边界调整不能伤到主循环主链。
- **D-11:** 若边界优化与主链稳定性冲突，优先保住 C++ / Lua runtime 主路径可运行性。

### Claude's Discretion
- 具体采用哪些 helper / bootstrap / installer / runner 结构承接被移出的副作用逻辑
- `FramePipeline` 内“直接渲染相关初始化”的精确边界命名
- 研究阶段如何把后续系统装配拆分分为本 phase 可做与后续 phase 再做的部分

</decisions>

<specifics>
## Specific Ideas

- 用户明确选择先讨论并锁定 `EngineInstance / FramePipeline / runtime context` 的职责边界。
- 用户认可推荐边界：`EngineInstance` 管生命周期与服务装配，`FramePipeline` 聚焦帧调度与直接渲染相关初始化，runtime context 只做依赖/状态载体。
- 用户明确要求优先移走 scene regression 一类“回归/样例/自检副作用逻辑”，认为它们不该继续挂在主初始化里。
- 用户把“现有双宿主启动链和最小 runtime smoke 不退化”作为最重要的成功信号。

</specifics>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project scope and phase boundary
- `.planning/PROJECT.md` — 当前仓库主线定位、非目标边界与项目级约束
- `.planning/REQUIREMENTS.md` — `RT-01` ~ `RT-04` 的 phase 需求口径
- `.planning/ROADMAP.md` — `Phase 1: Runtime 中枢收口` 的范围、依赖与原因
- `.planning/STATE.md` — 当前项目焦点与已知真相

### Existing code and architecture
- `.planning/codebase/ARCHITECTURE.md` — runtime / editor / module 当前结构与中枢风险总结
- `.planning/codebase/CONCERNS.md` — `FramePipeline` 偏重、宿主壳层偏重、多主线并行等风险说明
- `.planning/codebase/TESTING.md` — 当前 runtime / smoke / CTest 组织方式与验证基线

### Runtime entrypoints and hot spots
- `engine/runtime/engine_app.cpp` — `EngineInstance` 生命周期、默认服务装配与 runtime 入口行为
- `engine/runtime/frame_pipeline.cpp` — 当前 `FramePipeline` 初始化、系统装配与副作用逻辑集中点
- `engine/runtime/frame_pipeline.h` — `FramePipeline` 对外职责与依赖面
- `apps/runtime/cpp_host/main.cpp` — C++ 宿主主链入口
- `apps/runtime/lua_host/main.cpp` — Lua 宿主主链入口

### Validation anchors
- `tests/engine/runtime/frame_pipeline_static_regression_test.cpp` — runtime / frame pipeline 相关静态回归锚点
- `tests/modules/gameplay_2d/runtime_smoke_snapshot_test.cpp` — runtime smoke 相关现有验证入口
- `tests/engine/scripting/cpp_business_runtime_test.cpp` — C++ 宿主与 runtime 相关回归
- `doc-archive/TESTING_CTEST_GUIDE.md` — 当前 Windows 本地最小门禁与 runtime 相关测试口径

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `EngineInstance`（`engine/runtime/engine_app.cpp`）: 已经承担生命周期与默认 `World` / `AssetManager` 服务装配，是继续上提装配职责的自然落点。
- `FramePipeline`（`engine/runtime/frame_pipeline.cpp`）: 已有清晰的 update / render / fixed update 主链，可在不推翻现状的前提下先剥离副作用职责。
- `apps/runtime/cpp_host/main.cpp` / `apps/runtime/lua_host/main.cpp`: 是双宿主主链的直接验收锚点，可用于定义“不能退化”的最小运行路径。

### Established Patterns
- 当前代码已经在向依赖注入迁移：`EngineInstance` 会注入 `World` / `AssetManager`，`FramePipeline` 依赖注入后的 runtime context 工作。
- 当前仓库强调 Windows 本地 `CTest` + smoke gate 作为主线验证，而不是仅凭代码阅读判断完成。
- runtime 目前允许通过环境变量控制 `DSE_DATA_ROOT`、`DSE_RUNTIME_MODULES`、`DSE_STARTUP_SCENE` 等行为，说明“可配置主链”是既有模式。

### Integration Points
- 边界调整的主要连接点在 `engine/runtime/engine_app.cpp` 与 `engine/runtime/frame_pipeline.cpp` 之间。
- 被移出的副作用逻辑需要找到新的承接位置，但不能破坏 `apps/runtime/*_host/main.cpp` 的现有调用方式。
- 任意收口动作都需要和现有 runtime 测试、smoke 与测试文档口径一起校准。

</code_context>

<deferred>
## Deferred Ideas

- 更大规模地下沉 `FramePipeline` 中的重系统装配逻辑 — 仍在总体方向内，但不是本 phase 第一优先级
- 进一步减少 `FramePipeline` 对 Lua/C++/3D 运行模式组合差异的感知 — 本 phase 可研究，但优先级低于副作用逻辑剥离
- 把 `FramePipeline` 尽快改造成“纯帧执行器” — 当前明确不走激进路线，留待后续 phase 评估

</deferred>

---

*Phase: 01-runtime*
*Context gathered: 2026-04-22*
