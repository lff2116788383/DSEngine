# 安全策略 / Security Policy

## 受支持版本 / Supported Versions

DSEngine 目前处于 **alpha** 阶段。安全修复仅针对最新的预发布版本。

DSEngine is currently in **alpha**. Security fixes target the latest pre-release only.

| 版本 / Version | 受支持 / Supported |
| -------------- | ------------------ |
| 0.1.x-alpha    | 是 / Yes |
| < 0.1.0        | 否 / No  |

## 上报漏洞 / Reporting a Vulnerability

**请勿为安全漏洞公开提交 issue 或 PR。**
**Please do NOT open a public issue or pull request for security vulnerabilities.**

推荐方式 / Preferred:

- 使用 GitHub 的私密上报通道:仓库 **Security → Report a vulnerability**(GitHub Security Advisories)。
  Use GitHub's private reporting: repository **Security → Report a vulnerability** (GitHub Security Advisories).
- 或通过 GitHub 私信联系维护者 [@lff2116788383](https://github.com/lff2116788383)。
  Or contact the maintainer [@lff2116788383](https://github.com/lff2116788383) privately via GitHub.

上报时请尽量包含 / Please include when possible:

- 受影响的版本、平台与后端(OpenGL / D3D11 / Vulkan / Web)。
- 复现步骤或概念验证(PoC)。
- 影响评估(如可能造成的危害)。

## 处理流程 / Process

- 我们会尽快确认收到上报,并评估其影响与可复现性。
  We will acknowledge the report as soon as possible and assess its impact and reproducibility.
- 在修复发布前,请对漏洞细节保密(负责任披露)。
  Please keep vulnerability details confidential until a fix is released (responsible disclosure).
- 修复后会在 [`CHANGELOG.md`](CHANGELOG.md) 与对应 Release 说明中致谢(如你愿意)。
  Once fixed, we will credit you in the [`CHANGELOG.md`](CHANGELOG.md) and release notes (if you wish).
