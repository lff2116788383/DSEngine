---
phase: 01
plan: 01
title: Runtime 边界标定与副作用剥离
wave: 1
depends_on: []
autonomous: true
files_modified:
  - engine/runtime/engine_app.h
  - engine/runtime/engine_app.cpp
  - engine/runtime/frame_pipeline.h
  - engine/runtime/frame_pipeline.cpp
  - tests/engine/runtime/frame_pipeline_static_regression_test.cpp
  - doc-archive/TESTING_CTEST_GUIDE.md
---

# Plan 01: Runtime 边界标定与副作用剥离

## Objective

围绕 `Phase 1` 已锁定的边界，对 `EngineInstance`、`FramePipeline` 与 runtime context 做一次保守但明确的收口：先把 scene regression 一类副作用逻辑从 `FramePipeline::Init()` 主链中剥离出来，再补结构化回归与文档口径，确保双宿主启动链和最小 runtime smoke 不退化。

## Must Haves

- `EngineInstance` 与 `FramePipeline` 的职责边界在代码与注释层更清晰
- `FramePipeline::Init()` 不再直接承担 scene regression 这类副作用逻辑
- 有对应的静态/轻量回归保护，防止副作用逻辑回填
- 双宿主主链与最小 runtime 验证口径保持可用

<threat_model>
## Threat Model

- **T-01:** 为了收口边界而误伤 C++ / Lua 双宿主启动链
- **T-02:** 副作用逻辑迁出后没有新的承接位置，导致场景回归能力静默丢失
- **T-03:** 只做代码搬运但没有回归测试，后续再次把副作用塞回 `FramePipeline`
- **T-04:** 文档与验证命令不更新，造成“代码已变、口径未变”的认知漂移
</threat_model>

<tasks>
<task id="01-01-01">
<title>标定 runtime 三角色边界并固化到代码接口</title>
<read_first>
- `.planning/phases/01-runtime/01-CONTEXT.md`
- `engine/runtime/engine_app.h`
- `engine/runtime/engine_app.cpp`
- `engine/runtime/frame_pipeline.h`
- `engine/runtime/frame_pipeline.cpp`
</read_first>
<action>
梳理并收紧 `EngineInstance`、`FramePipeline`、runtime context 的职责表述和调用边界；优先通过注释、命名、 helper 归位与轻量结构整理让代码显式表达“EngineInstance 管生命周期与服务装配，FramePipeline 聚焦帧调度与直接渲染相关初始化”。避免引入激进大改或破坏现有 host 调用方式。
</action>
<acceptance_criteria>
- 代码结构能清晰体现三者边界
- `EngineInstance` 仍是默认服务装配落点
- `FramePipeline` 未继续吸纳新的高层职责
</acceptance_criteria>
<automated>
`ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.unit"`
</automated>
</task>

<task id="01-01-02">
<title>剥离 FramePipeline 初始化中的 scene regression 副作用</title>
<read_first>
- `.planning/phases/01-runtime/01-CONTEXT.md`
- `.planning/phases/01-runtime/01-RESEARCH.md`
- `engine/runtime/frame_pipeline.cpp`
- `engine/scene/scene.cpp`
- `engine/scene/scene.h`
</read_first>
<action>
将 `FramePipeline::Init()` 中的 scene regression / 样例 / 自检类副作用逻辑迁出主初始化路径，改为更合适的承接位置（如 runtime bootstrap helper、显式 validation runner 或受控入口）。迁移后必须保证 runtime 主初始化职责更纯粹，同时不让相关回归能力静默消失。
</action>
<acceptance_criteria>
- `FramePipeline::Init()` 不再直接执行 scene regression 样例逻辑
- 被迁出的逻辑有清晰的新承接位置
- 迁移后主初始化路径语义更聚焦于 runtime 初始化
</acceptance_criteria>
<automated>
`ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.unit"`
</automated>
</task>

<task id="01-01-03">
<title>补 runtime 结构回归锚点守住新边界</title>
<read_first>
- `tests/engine/runtime/frame_pipeline_static_regression_test.cpp`
- `engine/runtime/frame_pipeline.cpp`
- `engine/runtime/engine_app.cpp`
- `.planning/phases/01-runtime/01-VALIDATION.md`
</read_first>
<action>
扩展或调整 runtime 静态/轻量回归测试，确保新的边界被测试显式约束：例如副作用逻辑不再留在 `FramePipeline::Init()` 主链中、关键 runtime graph 委托关系仍保持。测试应优先选择低成本、可重复、对 Windows 本地主线友好的方式。
</action>
<acceptance_criteria>
- 至少有一条回归能直接保护“副作用已迁出”这一结构性事实
- 既有 runtime 静态测试仍然可读且聚焦
- 新边界不会只能靠人工记忆维持
</acceptance_criteria>
<automated>
`ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.unit"`
</automated>
</task>

<task id="01-01-04">
<title>校准 runtime 验证与文档口径</title>
<read_first>
- `doc-archive/TESTING_CTEST_GUIDE.md`
- `.planning/phases/01-runtime/01-CONTEXT.md`
- `.planning/phases/01-runtime/01-RESEARCH.md`
- `apps/runtime/cpp_host/main.cpp`
- `apps/runtime/lua_host/main.cpp`
</read_first>
<action>
把本次 runtime 收口后的最小验证命令、关注点和边界说明同步到测试文档，使后续开发者与 AI 能明确知道：Phase 1 首要是守住双宿主主链与最小 runtime smoke，而不是只看代码形态。必要时补充说明新的副作用逻辑承接位置及推荐验证方式。
</action>
<acceptance_criteria>
- 测试文档与本次 runtime 收口口径一致
- 推荐命令能覆盖双宿主和最小 runtime 主链
- 文档不会继续暗示 scene regression 仍挂在原初始化路径中
</acceptance_criteria>
<automated>
`ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.cpp_runtime|engine.lua_runtime|engine.resource_injection|engine.unit"`
</automated>
</task>
</tasks>

## Verification Criteria

- 双宿主入口链路不退化
- `FramePipeline::Init()` 语义更纯，scene regression 不再是主初始化副作用
- runtime 结构性边界有测试锚点保护
- 文档、验证命令、代码现状三者一致

## Notes for Executor

- 这是一个保守收口 phase，不是激进重写 phase
- 优先级顺序：副作用剥离 > 结构回归补强 > 文档口径对齐 > 更重装配拆分延后
- 如遇到需要大规模移动 physics/audio/spine/mesh render 初始化的冲动，应先停下来，把它记为后续工作，而不是在本 plan 中扩面
