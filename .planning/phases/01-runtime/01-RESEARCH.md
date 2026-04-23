# Phase 1 Research: Runtime 中枢收口

**Date:** 2026-04-22
**Phase:** 01-runtime
**Source Inputs:** `01-CONTEXT.md`, `REQUIREMENTS.md`, `ROADMAP.md`, runtime 相关代码与测试锚点

## Research Objective

回答本 phase 的核心问题：

- 在不伤害现有 C++ / Lua 双宿主主链的前提下，如何低风险收口 `EngineInstance`、`FramePipeline` 与 runtime context 的职责边界？
- 哪些职责适合先迁出？
- 现有代码中哪些结构已经为拆分提供了落点？
- 哪些验证点必须守住，才能证明这次收口没有伤主链？

## Current Runtime Shape

### `EngineInstance` 的当前角色

`engine/runtime/engine_app.h` / `engine_app.cpp` 表明 `EngineInstance` 已经承担：

- 接收 `EngineRunConfig`
- 合并 `RuntimeServices` 与兼容字段 `world` / `asset_manager`
- 生成默认 `World` / `AssetManager`
- 初始化窗口与 GL 上下文（非 editor 模式）
- 初始化 `Debug` 与 `JobSystem`
- 配置并调用 `FramePipeline`
- 驱动 `Tick()` / `Run()` 生命周期

研究结论：`EngineInstance` 已经天然是“应用生命周期 + 服务装配”的承接点，不需要凭空发明新的顶层 orchestrator。

### `FramePipeline` 的当前角色

`engine/runtime/frame_pipeline.h` / `frame_pipeline.cpp` 表明 `FramePipeline` 当前同时承担：

- runtime context 持有
- 大量系统对象持有（2D / 3D / audio / physics / UI / rendering）
- RHI device 初始化
- RenderTarget / pipeline state 初始化
- 系统初始化与 shutdown 编排
- 动态模块加载
- scene regression 副作用逻辑
- update / fixed update / render 执行主链

研究结论：`FramePipeline` 当前是“帧执行器 + 初始化装配器 + 副作用入口 + 系统容器”的混合体。Phase 1 不适合一次性拆到纯执行器，但非常适合先移除最不合理的副作用职责。

### runtime graph 抽离现状

`tests/engine/runtime/frame_pipeline_static_regression_test.cpp` 明确表明项目已经做过一轮有价值的收口：

- `Update()` / `FixedUpdate()` 已委托给 `runtime_update_graph.cpp`
- render 生命周期已委托给 `runtime_render_shell.cpp`

研究结论：仓库已经存在“把帧执行逻辑从 `FramePipeline` 大函数中抽离出去”的既有模式。后续继续收口时，最稳妥的做法应是沿用这种“提 helper / shell / graph”的方向，而不是整体翻新 runtime 架构。

## Best Low-Risk Boundary Direction

基于 phase context 与现有代码，最合理的低风险边界是：

- `EngineInstance`：应用生命周期、默认服务装配、平台初始化、主循环控制
- `FramePipeline`：帧调度与直接渲染相关初始化 + 对 runtime graph / render shell 的调用
- runtime context：依赖、资源句柄、状态载体

关键点不是让 `FramePipeline` 立刻“极瘦”，而是先禁止继续吸收新的高层职责，并优先挪走最不该存在的副作用逻辑。

## Priority Offload Candidates

### Priority 1 — 回归 / 样例 / 自检副作用逻辑

在 `frame_pipeline.cpp` 中可以直接看到：

- `scene::RunSceneRoundTripRegressionSample("bin/scene_roundtrip_regression.json")`
- `scene::RunSceneBackwardCompatibilityRegressionSample("bin/scene_backward_compat_regression.json")`

它们位于 `FramePipeline::Init()` 主初始化路径中。

研究结论：这是本 phase 最应该优先处理的内容，原因有三：

1. **边界最脏**：这不是帧调度职责，也不是直接渲染初始化职责。
2. **副作用最明显**：运行时初始化会顺带做回归样例，这会污染“主链初始化”的语义。
3. **迁移风险相对低**：与大规模移动 physics/audio/spine/mesh render 初始化相比，这类逻辑通常更容易通过 helper / bootstrap runner 拆出。

推荐方向：

- 提取为独立 runtime bootstrap 辅助逻辑
- 或移到显式的 debug / validation / sample runner
- 或由 `EngineInstance` 在特定模式/显式开关下触发，而不是由 `FramePipeline` 无条件承载

### Priority 2 — 重系统装配编排

当前 `FramePipeline::Init()` 中还负责：

- `physics2d_system_.Init(...)`
- `spine_system_.SetAssetManager(...)`
- `audio_system_.Initialize(...)`
- `mesh_render_system_.SetAssetManager(...)`
- 动态 3D 模块装配与 `OnInit(...)`

研究结论：这部分确实也是边界压力源，但不适合作为 Phase 1 的第一刀。因为它与 update / render / shutdown 主链强绑定，贸然移动更容易伤双宿主主路径。

建议：

- 在 Phase 1 中只做“识别并分层”
- 把真正的系统装配重构留在后续小步计划或后续 phase

### Priority 3 — 运行模式组合差异

`FramePipeline` 仍感知：

- editor mode
- business mode
- runtime modules
- 3D 动态模块启用与否

研究结论：这是长期值得收口的方向，但当前优先级应低于副作用剥离。因为这类分支通常横跨 runtime、host、asset/test/doc 多区域，一旦过早深入，计划面会膨胀。

## Reusable Existing Patterns

### Pattern 1 — 委托到 runtime 子文件

仓库已经在用：

- `runtime_update_graph.cpp`
- `runtime_frame_ops.cpp`
- `runtime_render_shell.cpp`

来拆分 `FramePipeline` 的核心流程。

研究结论：Phase 1 的计划应继续沿用这种拆分模式。对 planner 来说，这意味着：

- 优先新增小而明确的 runtime helper / bootstrap 文件
- 避免把新逻辑继续塞回 `frame_pipeline.cpp`

### Pattern 2 — `EngineInstance` 作为服务装配落点

`EngineInstance` 已经合并 `services_`、`world`、`asset_manager` 并承担 pipeline 配置。

研究结论：如果要把部分高层装配或条件性启动逻辑从 `FramePipeline` 上提，`EngineInstance` 是自然承接点。

### Pattern 3 — 静态回归测试守结构演化

`frame_pipeline_static_regression_test.cpp` 是一种非常适合本 phase 的保护方式：

- 它不依赖完整 runtime 启动
- 它能明确约束“某些调用关系仍然存在”
- 它适合作为边界收口期间的低成本守门员

研究结论：Phase 1 很适合继续补这类静态回归，尤其用于约束副作用逻辑不再留在 `FramePipeline::Init()` 中。

## Validation Anchors That Must Not Regress

根据 phase context 与现有测试，以下是必须守住的验证锚点：

### 1. 双宿主入口不退化

- `apps/runtime/cpp_host/main.cpp`
- `apps/runtime/lua_host/main.cpp`

这两者是最直接的主链锚点。

### 2. C++ runtime 生命周期回归

- `tests/engine/scripting/cpp_business_runtime_test.cpp`

该测试可守住 C++ 业务 hooks 主链。

### 3. runtime 静态结构回归

- `tests/engine/runtime/frame_pipeline_static_regression_test.cpp`

它适合继续扩展为“副作用逻辑已从 `FramePipeline::Init()` 剥离”的保护测试。

### 4. 最小 runtime smoke / snapshot

- `tests/modules/gameplay_2d/runtime_smoke_snapshot_test.cpp`
- `doc-archive/TESTING_CTEST_GUIDE.md` 中 runtime / smoke 相关推荐命令

研究结论：本 phase 的主验收信号应优先围绕“主链不退化”，而不是“架构形状更优雅”。

## Recommended Planning Implications

基于研究，Phase 1 的可执行计划应满足：

1. **先做边界标定**：明确什么属于 `EngineInstance`、`FramePipeline`、runtime context。
2. **先剥离副作用**：把 scene regression 这类逻辑移出 `FramePipeline::Init()`。
3. **补结构回归测试**：用静态/轻量测试守住新边界，防止后续回填。
4. **最小验证闭环**：至少验证双宿主主链与最小 runtime smoke 未受伤。
5. **暂缓大规模系统装配拆分**：这属于识别到但 defer 的下一层工作。

## Risks During Planning

- 如果 planner 直接把 physics/audio/spine/mesh render 初始化一起大搬家，风险会明显上升。
- 如果只写“边界原则”而不包含实际副作用迁移目标，Phase 1 很可能产出空洞重构。
- 如果只做代码整理不补测试锚点，后续很容易把副作用重新塞回 `FramePipeline`。

## Recommendation Summary

**最推荐的 Phase 1 低风险路径：**

- 利用 `EngineInstance` 继续承接高层生命周期与服务装配角色
- 保留 `FramePipeline` 的帧调度主链与直接渲染初始化职责
- 先从 `FramePipeline::Init()` 中移走 scene regression 一类副作用逻辑
- 用静态回归 + 最小 runtime smoke 守住边界收口后的主链不退化
- 将更重的系统装配重构作为后续逐步收口项，而不是本 phase 第一波动作

## Research Outcome

**结论：Phase 1 不适合做激进 runtime 重写，但非常适合做一次有明确目标的“副作用边界收口 + 结构回归补强”。**
