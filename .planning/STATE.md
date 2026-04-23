# STATE.md

## Project Reference

See: `.planning/PROJECT.md` (updated 2026-04-22)

**Core value:** 在保持轻量、高性能和可控复杂度的前提下，把 2D Runtime + Lua/C++ 双宿主 + 原生编辑器 + 最小可执行测试门禁 做成可持续迭代的开发闭环
**Current focus:** Phase 1 — Runtime 中枢收口（UAT 全部通过，可进入下一 phase）

## Current State

- 项目类型：brownfield 引擎仓库
- 已完成：`.planning/codebase/` 代码库映射、`PROJECT.md`、`REQUIREMENTS.md`、`ROADMAP.md`、`config.json`、Phase 1 核心执行
- 当前状态：Phase 1 UAT 7/7 通过，核心目标全部达成
- 待补：`engine.lua_runtime.smoke` 需在图形上下文环境中执行最终验证（环境依赖型 smoke）

## Known Truths

- 2D 是当前默认稳定主线
- Editor 基础可用，但宿主壳层偏重
- `FramePipeline` 是当前 runtime 复杂度核心枢纽
- 3D 已接入但仅按 MVP 收口推进
- Windows + CTest + bat 脚本是当前默认验证基线

## Watch Items

- 避免 roadmap 偏离 README 与测试文档的真实口径
- 避免在 `depends/` 与 `reference/` 噪音中误判主代码状态
- 后续 phase 规划应优先围绕 runtime、editor、2D gate、3D MVP、asset chain、doc-archive/test alignment

## Next Up

- Phase 1 UAT 已通过，可执行 `/gsd-complete-milestone` 或直接进入 Phase 2
- 在具备图形上下文的环境中执行 `ctest -R "engine.lua_runtime.smoke"` 完成最终闭环
- 人工执行双宿主启动体验回归（见 01-VALIDATION.md Manual-Only Verifications）

---
*Last updated: 2026-04-23 after Phase 1 UAT completion and code review fixes*
