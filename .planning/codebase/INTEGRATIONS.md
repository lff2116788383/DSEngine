# DSEngine Codebase Integrations

## 概览

DSEngine 当前不是云服务或后端 API 驱动项目，外部集成主要集中在 **本地图形平台、脚本运行、资产导入、桌面工具链与测试执行**。仓库内未发现主线运行必须依赖远端数据库、认证服务、支付、Webhook 或在线配置中心。

## 运行时图形与平台集成

### OpenGL + GLFW + glad

Runtime 与 Editor 当前都以 OpenGL-first 路线运行：

- `GLFW`：窗口、上下文、输入回调
- `OpenGL`：主要渲染后端
- `glad`：OpenGL 符号加载

关键位置：

- `engine/runtime/engine_app.cpp`
- `engine/runtime/frame_pipeline.cpp`
- `engine/render/rhi/`
- `apps/editor_cpp/`

这是项目最核心的本地平台集成。

## Lua 脚本集成

仓库原生支持 Lua 业务侧运行：

- `apps/runtime/lua_host/main.cpp` 指定 startup script 并启动 Lua 宿主
- `engine/scripting/lua/` 管理 Lua runtime 与绑定
- `script/` 与 `samples/lua/` 提供脚本基础库与样例
- `depends/sol2-3.2.2` 与 `depends/lua` 提供绑定与 VM 基础

重要边界：Lua 可调用 API 必须来自真实 C++ 绑定层，不能凭文档或样例臆造未导出的 API。

## C++ 业务宿主集成

除了 Lua，仓库也支持 C++ 业务逻辑直接接入：

- `apps/runtime/cpp_host/main.cpp` 通过 `ConfigureCppBusinessHooks(...)` 注入业务回调
- `samples/cpp/` 提供 C++ demo 配置和逻辑
- `engine/scripting/cpp/` 承载 C++ business runtime 桥接

这说明业务层可以在 Lua 与 C++ 两种宿主之间切换。

## 动态模块集成

3D gameplay 能力以 `DSE_Gameplay3D` 动态模块形式接入：

- `CMakeLists.txt` 在 `DSE_ENABLE_3D=ON` 时构建 `DSE_Gameplay3D`
- `FramePipeline` 根据 `DSE_RUNTIME_MODULES` 与动态库候选名尝试加载模块
- 模块通过 `CreateModule` / `DestroyModule` 与 `core::IModule` 接口接入 runtime

这属于项目内部插件化集成，不是外部服务集成。

## 资产导入与烹饪集成

### glTF / GLB / FBX 方向

`apps/tools/asset_builder/` 与 `engine/assets/compiler/` 构成离线资产链，当前重点是将外部 3D 资源转换为运行时可用格式。

相关依赖：

- `tinygltf`
- `assimp`（条件启用）

相关实现：

- `apps/tools/asset_builder/main.cpp`
- `engine/assets/compiler/importer.h`
- `engine/assets/compiler/importer.cpp`

该链路应被视为“离线工具集成”，不是 runtime 直接消费第三方原始格式。

## 物理集成

### Box2D

2D 主线默认集成 Box2D：

- `depends/box2d-2.4.1`
- `engine/physics/physics2d/`
- `modules/gameplay_2d/`

### PhysX（条件启用）

3D 物理通过 `DSE_ENABLE_PHYSX` 控制，并且在 MSVC 下还受 `DSE_HAS_PHYSX_LIBS` 影响：

- 有头文件但缺少 PhysX `.lib` 时，`physics3d` 源文件会被排除
- `DSE_Gameplay3D` 在无 PhysX 库时会降级为无 3D 物理后端

相关位置：

- `cmake/CMakeLists.txt.Physx`
- `CMakeLists.txt`
- `engine/physics/physics3d/`
- `modules/gameplay_3d/gameplay_3d_module.h`

## Spine 集成

仓库包含 `depends/spine-runtimes`，并在 2D gameplay 模块中接入 Spine：

- `modules/gameplay_2d/spine/`
- 顶层 `DSE_ENABLE_SPINE` 开关

Spine 当前属于 2D 主线的重要动画能力，而不是孤立 demo。

## 编辑器 UI 集成

原生编辑器使用：

- `Dear ImGui`
- `ImGuizmo`
- `GLFW`
- `OpenGL`

关键目录：

- `apps/editor_cpp/`
- `depends/imgui/`
- `third_party/imguizmo/`

编辑器是对引擎 runtime 的原生工具化包装，直接链接 `dse_engine`。

## Launcher 桌面集成

`apps/launcher_tauri/` 是独立前端/桌面子工程，集成：

- Tauri CLI / API
- React 19
- TypeScript
- Webpack
- Rust / Cargo toolchain

相关文件：

- `apps/launcher_tauri/package.json`
- `build_fast_launcher.bat`
- `build_all.bat`

注意：Launcher 是辅助入口，不应与引擎核心架构混为一谈。

## GoogleTest 本地集成

当前 GoogleTest 使用本地 vendored 源码：

- `depends/googletest-1.17.0`
- `tests/gtest/`
- `DSE_BUILD_GTESTS`

`tests/gtest` 不再需要网络下载 GoogleTest；若本地目录缺失，顶层配置应失败并提示解压路径。

## 测试与截图集成

运行时支持通过环境变量输出截图：

- `DSE_SCREENSHOT_PATH`
- `DSE_SCREENSHOT_TARGET`

相关实现位于 `engine/runtime/engine_app.cpp`。这是本地文件系统级别的测试辅助集成。

## 本仓库未发现的常见外部集成

本轮映射未发现以下能力作为当前主线存在：

- 远端数据库
- 用户认证服务
- 第三方支付
- 云存储 / 对象存储
- Webhook / 消息队列
- 在线配置中心
- REST / GraphQL 业务 API 主线

如果后续引入这些能力，应视为新增架构决策，而不是延续现状。

## 结论

DSEngine 当前 integrations 以 **本地引擎能力集成** 为主：

- 图形平台：OpenGL / GLFW / glad
- 脚本：Lua / sol2
- 资产导入：tinygltf / Assimp
- 动画：Spine
- 物理：Box2D / 条件式 PhysX
- 编辑器：Dear ImGui / ImGuizmo
- 桌面工具：Tauri / React / Rust
- 测试：本地 GoogleTest + CTest

后续规划重点应放在系统边界、构建稳定性、资产链与 runtime 闭环，而不是后端服务编排。
