---
phase: 01
plan: 02
title: 修复 Lua runtime smoke 目标产物缺失
wave: 2
depends_on:
  - 01-PLAN
autonomous: true
gap_closure: true
files_modified:
  - tests/engine/CMakeLists.txt
  - tests/engine/lua_runtime_smoke_entry.cpp
  - doc-archive/TESTING_CTEST_GUIDE.md
---

# Plan 02: 修复 Lua runtime smoke 目标产物缺失

## Objective

修复 `engine.lua_runtime.smoke` 对应的 `dse_lua_runtime_smoke_single_test_v2.exe` 未产出问题，使 `CTest` 能真正执行 Lua runtime smoke gate，从而闭环 `Phase 1` 的最小验证链路。

## Must Haves

- `engine.lua_runtime.smoke` 注册的可执行目标能真实产出到 `bin/`
- `ctest` 不再因缺少 `dse_lua_runtime_smoke_single_test_v2.exe` 直接中断
- 修复过程不破坏 `engine.lua_runtime`、`engine.resource_injection` 和既有双宿主主链

<threat_model>
## Threat Model

- **T-02-01:** 修复 smoke 目标时误伤已有 Lua runtime 核心测试目标
- **T-02-02:** 只改测试注册名而不解决真实产物问题，导致 `CTest` 仍不可执行
- **T-02-03:** 通过跳过 smoke gate 获得假阳性，而不是恢复真实验证能力
</threat_model>

<tasks>
<task id="01-02-01">
<title>定位 smoke 目标未产出的真实原因</title>
<read_first>
- `tests/engine/CMakeLists.txt`
- `tests/engine/lua_runtime_smoke_entry.cpp`
- `build_vs2022/Testing/Temporary/CTestCostData.txt`
</read_first>
<action>
确认 `dse_lua_runtime_smoke_single_test_v2` 为什么未产出：是目标未被构建、源文件/链接配置有误、输出路径不一致，还是 `CTest` 注册与真实 target 名称/产物不一致。必须拿到明确原因，而不是直接绕开 gate。
</action>
<acceptance_criteria>
- 能明确说明缺失 exe 的直接原因
- 结论可指导最小修复，而不是模糊猜测
</acceptance_criteria>
<automated>
`cmake --build build_vs2022 --config Debug --target dse_lua_runtime_smoke_single_test_v2 -- /m:1 /nologo`
</automated>
</task>

<task id="01-02-02">
<title>修复 smoke 目标与 CTest 的产物闭环</title>
<read_first>
- `tests/engine/CMakeLists.txt`
- `tests/engine/lua_runtime_smoke_entry.cpp`
- `.planning/phases/01-runtime/01-SUMMARY.md`
</read_first>
<action>
基于定位结果做最小修复，让 `engine.lua_runtime.smoke` 对应 target 能被实际构建并被 `CTest` 找到。优先修真实 target / 输出 / 注册链路，不要通过删除或跳过 smoke gate 掩盖问题。
</action>
<acceptance_criteria>
- `dse_lua_runtime_smoke_single_test_v2.exe` 可产出，或 `CTest` 已对齐到真实存在的目标产物
- `engine.lua_runtime.smoke` 可被 `ctest` 真实执行
- 不影响 `engine.lua_runtime`、`engine.resource_injection` 既有验证入口
</acceptance_criteria>
<automated>
`ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.lua_runtime.smoke|engine.lua_runtime|engine.resource_injection"`
</automated>
</task>

<task id="01-02-03">
<title>同步验证文档与阶段结论</title>
<read_first>
- `doc-archive/TESTING_CTEST_GUIDE.md`
- `.planning/phases/01-runtime/01-VALIDATION.md`
- `.planning/phases/01-runtime/01-SUMMARY.md`
</read_first>
<action>
如果 smoke 目标已恢复，则把当前 Phase 1 的验证说明更新为可复现状态；如果修复后仍有环境限制，也要把限制和现状清晰写入 phase 文档，避免后续误判为已完全通过。
</action>
<acceptance_criteria>
- phase 文档与实际 smoke 状态一致
- 后续开发者能知道该 gate 是否已恢复以及如何复现
</acceptance_criteria>
<automated>
`ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.unit|engine.cpp_runtime|engine.lua_runtime|engine.resource_injection|engine.lua_runtime.smoke"`
</automated>
</task>
</tasks>

## Verification Criteria

- Lua runtime smoke gate 不再因缺少 exe 而中断
- `Phase 1` 最小门禁链条可真实执行
- phase 文档准确反映修复后的验证状态
