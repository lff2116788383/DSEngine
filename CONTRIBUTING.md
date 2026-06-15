# 贡献指南 / Contributing to DSEngine

感谢你对 DSEngine 的关注！本文件说明参与贡献的流程与约定。
Thanks for your interest in DSEngine! This document describes how to contribute.

> 详细的开发环境、构建/测试命令、目录结构与代码规范，以 [`AGENTS.md`](AGENTS.md) 为权威来源；
> 本文件只覆盖**贡献流程**。更细的工具规则见 [`.trae/rules/`](.trae/rules/) 与 [`.codebuddy/rules/`](.codebuddy/rules/)。
>
> For detailed dev setup, build/test commands, project layout and coding standards, see [`AGENTS.md`](AGENTS.md)
> (the authoritative source). This file only covers the **contribution workflow**.

---

## 提问题 / Reporting issues

- 先搜索已有 issue，避免重复。
- 使用 [issue 模板](.github/ISSUE_TEMPLATE/)（bug / feature / question）。
- bug 请附:复现步骤、期望/实际结果、平台与后端(OpenGL / D3D11 / Vulkan / Web)、构建配置(Debug/Release)、相关日志。
- **安全漏洞请勿公开提 issue**——见 [`SECURITY.md`](SECURITY.md)。

## 开发环境 / Development setup

简要(完整见 [`AGENTS.md`](AGENTS.md)):

1. 克隆后初始化子模块(in-tree 依赖,必须):
   ```bash
   git submodule update --init --recursive
   ```
2. 两条构建路径任选其一:
   - **CMakePresets**(推荐,Ninja → `out/build/<preset>`):见 [`CMakePresets.json`](CMakePresets.json)。
   - **build_vs2022**(Visual Studio 2022 生成器,脚本/CI 使用):Windows 脚本在 [`scripts/win/`](scripts/win/)。
3. 着色器为预编译内嵌(`.frag → .spv → .gen.h`):**改 `.frag` 由 CMake 自动重生成,切勿手改 `*.gen.h`**。

## 分支与 PR 流程 / Branching & pull requests

- **不要直接 push 到 `master`**。从最新 `master` 切出特性分支,改完开 PR 合回 `master`。
- 一个 PR 聚焦一件事;保持改动最小、可评审。
- PR 请填写 [PR 模板](.github/pull_request_template.md):摘要、变更类型、关联 issue、测试说明、checklist。
- 合并前需本地测试通过(见下);CI 绿灯后再合并。

## 提交信息 / Commit messages

遵循 [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>
```

常用 `type`:`feat` / `fix` / `docs` / `build` / `refactor` / `test` / `chore`。
例:`fix(vulkan): grow per-object UBO ring for high-instance demos`。

## 测试 / Testing

- 提交前确保 GoogleTest 全绿:
  ```bash
  ctest -L gtest --output-on-failure
  ```
  (测试位于 `tests/gtest/{unit,integration,smoke}` 与 `tests/{http,net,serialize,regression}`。)
- 新增功能/修复 bug 请配套加测试。
- 不要为了过测试而修改测试断言(除非该测试本身有误,并在 PR 中说明)。

## 代码规范 / Coding standards

- 遵循 [`AGENTS.md`](AGENTS.md) 的代码约定与分层依赖(`apps/ → modules/ → engine/ → depends/`,`engine/` 不反向依赖)。
- **多后端渲染改动需三端同步**(OpenGL / D3D11 / Vulkan),并尽量在真机验证一致。
- 文档改动请保持中英 README 同步([`README.md`](README.md) / [`README.en.md`](README.en.md))。
- 用户可见的行为变更请更新 [`CHANGELOG.md`](CHANGELOG.md)。

## 许可证 / License

提交贡献即表示你同意你的贡献以本项目的 [MIT License](LICENSE) 授权。
By contributing, you agree that your contributions will be licensed under the project's [MIT License](LICENSE).
