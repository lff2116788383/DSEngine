---
status: completed
phase: 01-runtime
source: 01-SUMMARY.md
started: 2026-04-23T10:34:00
updated: 2026-04-23T10:55:00
---

## Tests

### 1. EngineInstance / FramePipeline 职责边界收紧
expected: engine_app.h 中 EngineInstance 注释明确其负责应用生命周期、默认服务装配与主循环驱动；frame_pipeline.h 中 FramePipeline 注释明确聚焦逐帧调度与直接渲染相关初始化，不承接高层启动期副作用流程
result: PASS
evidence: engine_app.h:37 "@brief 引擎应用运行实例，负责生命周期、默认服务装配与主循环驱动"；frame_pipeline.h:40 "@brief 引擎的主循环流水线，负责逐帧调度与直接渲染相关初始化，不承接高层启动期副作用流程"

### 2. scene regression 副作用迁出 FramePipeline
expected: FramePipeline::Init() 不再直接调用 RunSceneRoundTripRegressionSample 或 RunSceneBackwardCompatibilityRegressionSample；这些调用由 EngineInstance::RunStartupSceneRegressionChecks() 承接
result: PASS
evidence: frame_pipeline.cpp 中不含这两个函数名；engine_app.cpp:150-153 由 RunStartupSceneRegressionChecks() 承接；静态回归测试 5 test cases / 22 assertions 全部通过

### 3. DSE_DISABLE_STARTUP_SCENE_REGRESSION 环境变量开关
expected: 设置 DSE_DISABLE_STARTUP_SCENE_REGRESSION=1 后，EngineInstance::Init() 跳过启动期 scene regression 检查，不报错不崩溃
result: PASS
evidence: engine_app.cpp:56-59 IsStartupSceneRegressionDisabled() 检查环境变量；engine_app.cpp:144-147 当被禁用时 return true 跳过检查

### 4. 静态回归锚点保护
expected: ctest -R "engine.unit" 中 frame_pipeline_static_regression 测试通过，确保 scene regression 函数名不会重新出现在 FramePipeline 源码中
result: PASS
evidence: dse_engine_unit_tests.exe "[runtime][static]" → All tests passed (22 assertions in 5 test cases)

### 5. Lua runtime smoke 可执行目标恢复
expected: dse_lua_runtime_smoke_single_test_v2.exe 存在于 bin/ 目录，可被构建系统产出
result: PASS
evidence: cmake --build target dse_lua_runtime_smoke_single_test_v2 成功产出 exe；编译失败根因（缺少 ScopedLuaApiContextReset 定义）已修复

### 6. 单元测试门禁可运行
expected: engine.lua_runtime 和 engine.resource_injection 通过直接执行验证，输出 "All tests passed"
result: PASS
evidence: dse_lua_runtime_core_single_test.exe → All tests passed (4 assertions)；dse_lua_resource_injection_single_test.exe → All tests passed (2 assertions)

### 7. 测试文档口径同步
expected: doc-archive/TESTING_CTEST_GUIDE.md 包含"Runtime 中枢收口后的验证关注点"说明，明确 FramePipeline::Init() 不再承担 scene regression 启动期副作用、启动期回归检查由 EngineInstance 控制
result: PASS
evidence: TESTING_CTEST_GUIDE.md §5.8 明确写了 FramePipeline::Init() 应聚焦 runtime 初始化、启动期 scene regression 由 EngineInstance 承接

## Summary

total: 7
passed: 7
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps

[none]

## Notes

- engine.unit 中 scene_flow_test 的 blend_nodes weight 失败是既有问题，与本次 Phase 1 修改无关
- engine.lua_runtime.smoke 需图形上下文环境执行完整验证，属于环境依赖型 smoke
