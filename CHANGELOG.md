# 更新日志 / Changelog

本文件记录 DSEngine 的版本变更。
格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本 SemVer 2.0.0](https://semver.org/lang/zh-CN/)。

## [0.1.0-alpha] - 2026-06-15

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
  - 跨平台构建路径：Windows / Linux / Android / Web（实验性）。

- **平台与出包**
  - Web / WASM（实验性 MVP）：Emscripten + WebGL2 后端，2D 与 3D 前向渲染；`dse build --target web` 一键 configure+build+collect 出包。
  - `dse` headless CLI：`new`（模板 empty/2d/3d/lua/cpp 叠加 platformer/topdown/thirdperson）、`build`（含 `--target web`、`--with-swgl`）、`pack`；dist 提供 win/linux Export Template。
  - 一键出独立游戏：CLI / 编辑器 Build Game / 手动三路同源；`game.dsmanifest` 记录入口脚本，松散 scripts/scenes、双击即玩；端到端 AES-128-CTR 加密打包（`game.bun`）。
  - 启动闪屏：编辑器、运行时与打包游戏的品牌启动画面（Windows + Linux/X11，带淡入淡出）。
  - Linux 资源热重载（inotify FileWatcher）。

### Fixed（修复）

- SDK 打包/验证脚本此前因缺少 UTF-8 BOM 而无法在 Windows PowerShell 5.1 下解析，
  且未启用 `DSE_BUILD_SHARED=ON` 导致安装产物缺失（仅有包配置文件、无库与头文件），
  `find_package(DSEngine)` 无法成功。现已修复，脚本可在无 GPU 的 Windows 环境完整跑通。
- **着色器**：后处理变迭代循环（SSAO/DOF/动模糊/体积光/大气）改用 `textureLod(...,0.0)`，
  消除 FXC `X3570`/`X3511` 编译错误（三后端真机验证渲染一致）。
- **Vulkan**：高实例场景的 per-object UBO ring 与 descriptor pool 扩容；mesh 材质贴图改用 REPEAT 包裹采样器。
- **Lua**：修复关闭运行时时调试器 detach 的 use-after-free（偶发崩溃）；修复 4 个 samples/lua 示例脚本 bug。
- **Web**：emscripten 音频改用 NO_THREADING 使 BGM 可发声；修复 WebGL2 上后处理采样器绑定与 SSBO→UBO 顶点降级问题。

### Known Issues / Notes（已知事项）

- SDK 发行版默认 **不启用 Vulkan**（作为可选能力）；如需启用，
  `package_sdk.ps1 -EnableVulkan`。
- macOS / iOS 平台后端尚未提供。
- 跨三后端的像素级一致性验证需在真实 GPU 上进行；已手动验证三端渲染一致，但自动化回归本测试版未覆盖。
- Web / WASM 为实验性 MVP：3D 仅前向渲染，部分后处理在 WebGL2 上能力受限。

[0.1.0-alpha]: https://github.com/lff2116788383/DSEngine/releases/tag/v0.1.0-alpha
