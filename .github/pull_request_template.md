<!--
请填写以下各节;勾选适用项。详细贡献流程见 CONTRIBUTING.md。
Fill in the sections below and check the applicable items. See CONTRIBUTING.md for the full workflow.
-->

## 摘要 / Summary

<!-- 这个 PR 做了什么、为什么。What does this PR do and why? -->

## 变更类型 / Type of change

- [ ] fix(修复 bug / bug fix)
- [ ] feat(新功能 / new feature)
- [ ] docs(文档 / documentation)
- [ ] refactor / build / chore
- [ ] 破坏性变更 / breaking change

## 关联 issue / Related issues

<!-- 例 / e.g. Closes #123 -->

## 测试 / Testing

<!-- 如何验证?平台/后端/配置。How did you verify? Platform / backend / config. -->

- [ ] `ctest -L gtest` 本地全绿 / passes locally

## Checklist

- [ ] 遵循 [CONTRIBUTING.md](../CONTRIBUTING.md) 与 [AGENTS.md](../AGENTS.md) 的约定
- [ ] 渲染相关改动已**三端同步**(OpenGL / D3D11 / Vulkan),如适用 / rendering changes synced across all 3 backends, if applicable
- [ ] 未手改生成的着色器头(`*.gen.h`) / no hand-edited generated shader headers
- [ ] 用户可见的变更已更新 [CHANGELOG.md](../CHANGELOG.md),如适用 / CHANGELOG updated for user-facing changes, if applicable
- [ ] 中英 README 已同步,如适用 / README.md & README.en.md kept in sync, if applicable
