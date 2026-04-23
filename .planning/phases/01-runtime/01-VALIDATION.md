---
phase: 01
slug: runtime
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-04-22
---

# Phase 01 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + Catch2-style C++ tests + runtime smoke |
| **Config file** | `tests/engine/CMakeLists.txt`, `tests/spine/CMakeLists.txt`, top-level `CMakeLists.txt` |
| **Quick run command** | `ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.cpp_runtime|engine.lua_runtime|engine.resource_injection|engine.unit|engine.lua_runtime.smoke"` |
| **Full suite command** | `ctest --test-dir build_vs2022 -C Debug --output-on-failure -L engine` |
| **Estimated runtime** | ~120 seconds |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.cpp_runtime|engine.lua_runtime|engine.resource_injection|engine.unit"`
- **After every plan wave:** Run `ctest --test-dir build_vs2022 -C Debug --output-on-failure -L engine`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 180 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 01-01-01 | 01 | 1 | RT-02 | T-01-01 | `EngineInstance` / `FramePipeline` 边界调整不破坏初始化主链 | unit/static | `ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.unit"` | ✅ | ✅ green |
| 01-01-02 | 01 | 1 | RT-02 | T-01-02 | scene regression 副作用不再挂在 `FramePipeline::Init()` 主链中 | static | `ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.unit"` | ✅ | ✅ green |
| 01-01-03 | 01 | 2 | RT-01 | T-01-03 | C++ / Lua 双宿主启动链保持可运行 | runtime/smoke | `ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.cpp_runtime|engine.lua_runtime|engine.resource_injection"` | ✅ | ⚠️ flaky |
| 01-01-04 | 01 | 2 | RT-04 | T-01-04 | runtime 收口后最小 smoke 与门禁仍能发现主链破坏 | smoke | `ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.cpp_runtime|engine.lua_runtime|engine.resource_injection|engine.unit"` | ✅ | ⚠️ flaky |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| 非测试模式下双宿主真实启动体验未退化 | RT-01 | 本地窗口与资源路径行为可能受环境影响 | 分别运行 C++ / Lua host，可观察窗口启动、主循环进入、无即时崩溃 |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 180s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending

## Current Verification Note

- `engine.unit` 相关静态/单元验证继续作为本 phase 的绿色锚点
- **Lua runtime smoke 目标编译阻塞已修复**：`dse_lua_runtime_smoke_single_test_v2.exe` 已成功产出（根因：`lua_runtime_smoke_single_test.cpp` 缺少 `ScopedLuaApiContextReset` 定义，已补齐）
- `engine.lua_runtime` 和 `engine.resource_injection` 已通过直接执行验证（All tests passed）
- `engine.lua_runtime.smoke` 依赖图形上下文与窗口系统，在无窗口环境下暂缓执行，属于环境依赖型 smoke（参见集成测试分层策略）
- 本 phase 核心目标（Runtime 边界收口 + 静态回归锚点 + 构建链恢复）均已达成

