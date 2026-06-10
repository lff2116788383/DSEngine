# 更新日志 / Changelog

本文件记录 DSEngine 的版本变更。
格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本 SemVer 2.0.0](https://semver.org/lang/zh-CN/)。

## [0.1.0-alpha] - 2026-06-10

首个对外 SDK 测试版（alpha）。引擎以**共享库（DLL）**形式打包，下游项目可通过
`find_package(DSEngine)` + `target_link_libraries(<app> PRIVATE DSEngine::dse_engine)`
集成。

### Added（新增）

- **SDK 打包与分发**
  - `cmake --install ... --component sdk`：安装 `dse_engine` 运行库（DLL/import lib）、
    公共头文件、第三方头（glm / EnTT / Box2D）、Lua 脚本，以及 CMake 包配置
    （`DSEngineConfig.cmake` / `DSEngineConfigVersion.cmake` / `DSEngineTargets.cmake`）。
  - `scripts/package_sdk.ps1`：一键打包为 `DSEngine-SDK-v<版本>-win-x64-<config>.zip`。
  - `scripts/verify_sdk.ps1`：端到端验证（打包 → 安装 → 消费者编译 → 运行），
    覆盖 Minimal（2D）与 Full（3D + Jolt）两种配置 × Debug/Release。
  - `examples/sdk_consumer/`：最小消费者示例工程。
  - `engine/dse_version.h`：编译期版本宏（`DSE_VERSION_MAJOR/MINOR/PATCH`、
    `DSE_VERSION_PRERELEASE`、`DSE_VERSION_STRING`）。

- **运行时能力（本测试版已具备）**
  - 三后端 RHI：OpenGL、Direct3D 11、Vulkan（共享同一份 GLSL 着色器源，离线编译为多目标内嵌头）。
  - 2D 与 3D 运行时路径；3D 物理使用 Jolt。
  - Lua 脚本运行时与绑定。
  - 跨平台构建路径：Windows / Linux / Android。

### Fixed（修复）

- SDK 打包/验证脚本此前因缺少 UTF-8 BOM 而无法在 Windows PowerShell 5.1 下解析，
  且未启用 `DSE_BUILD_SHARED=ON` 导致安装产物缺失（仅有包配置文件、无库与头文件），
  `find_package(DSEngine)` 无法成功。现已修复，脚本可在无 GPU 的 Windows 环境完整跑通。

### Known Issues / Notes（已知事项）

- SDK 发行版默认 **不启用 Vulkan**（作为可选能力）；如需启用，
  `package_sdk.ps1 -EnableVulkan`。
- macOS / iOS 平台后端尚未提供。
- 跨三后端的像素级一致性验证需在真实 GPU 上进行（本测试版未覆盖）。

[0.1.0-alpha]: https://github.com/lff2116788383/DSEngine/releases/tag/v0.1.0-alpha
