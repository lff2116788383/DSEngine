# STATE.md

## Project Reference

See: `.planning/PROJECT.md` (updated 2026-04-23)

**Core value:** 在保持轻量、高性能和可控复杂度的前提下，把 Runtime + Lua/C++ 双宿主 + 3D 模块 + 资产导入链 + 最小可执行测试门禁 做成可支撑 Lua 驱动 3D 游戏原型的开发闭环
**Current focus:** Phase 1 已完成；当前进入 Phase 2 — 引擎稳定性与 3D 崩溃清零

## Current State

- 项目类型：brownfield 引擎仓库
- 已完成：`.planning/codebase/` 代码库映射、`PROJECT.md`、`REQUIREMENTS.md`、`ROADMAP.md`、`config.json`、Phase 1 核心执行
- 当前状态：Phase 1 UAT 7/7 通过，runtime 中枢首轮收口已完成
- 当前主目标：优先跑通 runtime / Lua / 3D / asset 最小测试矩阵，定位并修复阻塞原型推进的 bug、崩溃、超时与目标缺失问题
- 待补：`engine.lua_runtime.smoke` 需在图形上下文环境中执行最终验证（环境依赖型 smoke）

## Known Truths

- 2D 仍是当前默认稳定主线
- 3D 已接入且已有最小 gate，但尚未证明足以支撑完整 Lua 3D 游戏开发
- `FramePipeline` 是当前 runtime 复杂度核心枢纽
- Lua 是原型开发的重要业务脚本层，所有能力判断必须基于真实绑定而非假设
- Windows + CTest + bat 脚本是当前默认验证基线
- 编辑器基础可用，但近期已降级为后置能力，等待技术路线明确

## Watch Items

- 避免把“3D 已接入”误判成“3D 已成熟可量产”
- 避免在 `depends/` 与 `reference/` 噪音中误判主代码状态
- 后续 phase 规划应优先围绕稳定性清零、runtime、3D MVP、Lua gameplay、asset chain、2D guard、doc/test alignment
- 所有 Lua 3D 能力判断都必须先确认真实绑定与样例路径，不可臆造 API

## Next Up

- 进入 Phase 2：`引擎稳定性与 3D 崩溃清零`
- 优先执行 runtime / Lua / 3D / asset 最小矩阵，确认当前真实性与失败边界
- 在具备图形上下文的环境中执行 `ctest -R "engine.lua_runtime.smoke"` 完成 runtime 最终闭环
- Phase 2 完成后进入 Lua 3D gameplay 与资产链闭环分析

---
*Last updated: 2026-04-23 after gsd-health repair aligned state with stability-first execution*
