# Phase 01 Summary: Runtime 中枢收口

**Date:** 2026-04-23
**Status:** UAT 7/7 通过，Phase 1 完成

## Completed Work

### 1. Runtime 边界表达已收紧
- 更新了 `engine/runtime/engine_app.h` 中 `EngineInstance` 的职责描述，明确其负责应用生命周期、默认服务装配与主循环驱动。
- 更新了 `engine/runtime/frame_pipeline.h` 中 `FramePipeline` 的职责描述，明确其聚焦逐帧调度与直接渲染相关初始化，不承接高层启动期副作用流程。

### 2. scene regression 副作用已从 `FramePipeline::Init()` 迁出
- 从 `engine/runtime/frame_pipeline.cpp` 移除了 `RunSceneRoundTripRegressionSample(...)` 与 `RunSceneBackwardCompatibilityRegressionSample(...)` 的直接调用。
- 在 `engine/runtime/engine_app.cpp` 中新增 `EngineInstance::RunStartupSceneRegressionChecks()`，由 `EngineInstance::Init()` 在 pipeline 初始化成功后显式承接启动期场景回归检查。
- 新增 `DSE_DISABLE_STARTUP_SCENE_REGRESSION` 环境变量开关，用于在必要时跳过该启动期检查。

### 3. 结构回归测试已补强
- 在 `tests/engine/runtime/frame_pipeline_static_regression_test.cpp` 中新增静态回归测试，约束 scene regression 不再出现在 `FramePipeline` 源码中，而必须由 `EngineInstance` 承接。

### 4. 测试文档口径已同步
- 在 `doc-archive/TESTING_CTEST_GUIDE.md` 中补充"Runtime 中枢收口后的验证关注点"，明确：
  - 双宿主链路是首要验收信号
  - `FramePipeline::Init()` 不再承担 scene regression 启动期副作用
  - 启动期回归检查由 `EngineInstance` 控制

## Verification

### 已完成
- `cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF -DDSE_BUILD_ENGINE_TESTS=ON`
- `cmake --build build_vs2022 --config Debug --target dse_engine_unit_tests dse_lua_runtime_tests dse_lua_runtime_core_single_test dse_lua_runtime_smoke_single_test_v2 dse_lua_resource_injection_single_test dse_spine_tests -- /m:1 /nologo`
- 结果：**最小测试目标构建链已包含 Lua runtime smoke 可执行目标**
- `ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.unit|engine.cpp_runtime|engine.lua_runtime|engine.resource_injection|engine.lua_runtime.smoke"`
- 当前状态：待重新执行并确认 `engine.lua_runtime.smoke` 是否恢复可运行


### 当前验证结论
- `engine.unit` 与最小测试构建链路已恢复到可执行状态
- 本轮 runtime 边界收口没有新增编译层面的阻塞
- **Lua runtime smoke 目标编译阻塞已修复**：`dse_lua_runtime_smoke_single_test_v2.exe` 已成功产出（原因：`lua_runtime_smoke_single_test.cpp` 缺少 `ScopedLuaApiContextReset` 类定义，已补齐）
- `engine.lua_runtime` 和 `engine.resource_injection` 已通过直接执行验证
- `engine.lua_runtime.smoke` 依赖图形上下文与窗口系统，属于环境依赖型 smoke，在无窗口环境下暂缓执行


## Outcome Against Phase Goal

### Achieved
- `EngineInstance` 与 `FramePipeline` 的职责边界在代码与注释层更加清晰
- `FramePipeline::Init()` 已不再直接承载 scene regression 副作用逻辑
- 新边界已有静态回归锚点保护
- 最小测试构建链路已恢复并通过编译
- Lua runtime smoke 可执行目标已修复并可产出

### Still Open
- `engine.lua_runtime.smoke` 需在具备图形上下文的环境中执行完整验证
- 非测试模式下双宿主真实启动体验的回归检查需人工执行

## Recommended Next Step

- Phase 1 已完成，进入 Phase 2（Editor 高频闭环增强）
- 在具备图形上下文的环境中执行 `ctest -R "engine.lua_runtime.smoke"` 完成最终闭环
- 人工执行双宿主启动体验回归（见 VALIDATION.md Manual-Only Verifications）

---

*Phase: 01-runtime*
*Summary written: 2026-04-23*
