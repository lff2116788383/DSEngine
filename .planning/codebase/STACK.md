# DSEngine Codebase Stack

## 概览

DSEngine 当前是一个以 **C++20 + CMake** 为核心的本地游戏引擎仓库，默认开发与验证基线是 **Windows + Visual Studio 2022 + CTest**。仓库同时维护多种宿主与工具链：

- `apps/runtime/cpp_host/`：C++ 业务宿主
- `apps/runtime/lua_host/`：Lua 业务宿主
- `apps/editor_cpp/`：原生 C++ 编辑器
- `apps/tools/asset_builder/`：离线资产工具
- `apps/launcher_tauri/`：Tauri + React + TypeScript 桌面 Launcher 子工程

## 主要语言与运行时

- **C++20**：引擎核心、运行时、编辑器、资产工具、动态模块主线
- **Lua**：脚本业务层与样例脚本
- **TypeScript / React 19**：Launcher 前端层
- **Rust（Tauri）**：Launcher 桌面壳依赖
- **Batch / PowerShell**：Windows 本地构建脚本与验证入口

## 构建系统

### 顶层构建

顶层由 `CMakeLists.txt` 驱动，当前关键开关包括：

- `DSE_ENABLE_2D`
- `DSE_ENABLE_3D`
- `DSE_ENABLE_PHYSX`
- `DSE_ENABLE_SPINE`
- `DSE_BUILD_EDITOR`
- `DSE_BUILD_LAUNCHER`
- `DSE_BUILD_GTESTS`

当前输出目录统一到 `bin/`，并通过本地 vendored 依赖而不是网络拉取来完成主要构建。

### Windows 构建脚本

仓库当前的第一层工作流是 Windows 批处理脚本：

- `build_all.bat`：全量配置、构建与标签测试入口
- `build_fast_cpp.bat`：快速构建 C++ 主线
- `build_fast_editor.bat`：快速构建编辑器
- `build_fast_launcher.bat`：快速构建 Launcher
- `build_fast_lua.bat`：快速跑 Lua 相关链路
- `build_fast_tests.bat`：快速构建并执行最小 GTest 集合

其中 `build_all.bat` 会检查 `cmake`、`python`、`node`、`npm`、`cargo` 等环境，体现出“本地一键构建优先”的仓库风格。

## 核心原生依赖

从顶层 `CMakeLists.txt` 与 `depends/` 结构可确认，当前主线依赖主要以源码内置方式集成：

- `depends/entt-3.13.0`：ECS
- `depends/box2d-2.4.1`：2D 物理
- `depends/glfw-3.3-3.4`：窗口与输入
- `depends/glad`：OpenGL loader
- `depends/imgui`：编辑器 UI
- `third_party/imguizmo`：编辑器 Gizmo
- `depends/spine-runtimes`：Spine 运行时
- `depends/freetype-2.11.0`：字体
- `depends/rapidjson`：JSON
- `depends/spdlog`：日志
- `depends/sol2-3.2.2` + `depends/lua`：Lua 绑定层
- `depends/luasocket-3.0.0`：Lua 网络能力
- `depends/tinygltf`：glTF 支持
- `depends/assimp`：离线资产导入（条件启用）
- `depends/physx`：3D 物理（条件启用）
- `depends/googletest-1.17.0`：本地 GTest 源码依赖

## 渲染与平台栈

从 `engine/runtime/engine_app.cpp`、`engine/runtime/frame_pipeline.cpp` 与编辑器入口可以确认：

- 当前渲染后端是 **OpenGL-first**
- 窗口层使用 **GLFW**
- Runtime 与 Editor 都依赖 `glad + GLFW + OpenGL`
- 编辑器使用 **Dear ImGui + ImGuizmo** 构建原生工具 UI

## 脚本与业务宿主

仓库同时维护 **Lua / C++ 双业务宿主**：

- `apps/runtime/cpp_host/main.cpp` 通过 `ConfigureCppBusinessHooks(...)` 注入 C++ 业务逻辑
- `apps/runtime/lua_host/main.cpp` 通过 `RunEngine(...)` 启动 Lua 业务脚本
- `engine/scripting/cpp/` 与 `engine/scripting/lua/` 分别承载两条桥接链路

## 3D 模块与动态装配

当前 3D 能力不是完全静态耦合在主 runtime 中，而是通过 `DSE_Gameplay3D` 动态模块装配：

- 顶层 `CMakeLists.txt` 在 `DSE_ENABLE_3D=ON` 时构建 `DSE_Gameplay3D`
- `engine/runtime/frame_pipeline.cpp` 根据 `DSE_RUNTIME_MODULES` 和动态库候选名加载 3D 模块
- `CreateModule` / `DestroyModule` 是模块边界的一部分
- PhysX 仅在 `DSE_ENABLE_PHYSX` 且 `DSE_HAS_PHYSX_LIBS` 同时满足时参与 3D 物理路径

这说明当前 3D 栈已经存在，但仍然保持“条件式启用、按模块装配”的工程形态。

## 编辑器与工具层

### 原生编辑器

`apps/editor_cpp/` 是当前主线编辑器：

- C++ 原生可执行程序
- 直接链接 `dse_engine`
- 面板式结构，包含 viewport / hierarchy / inspector / toolbar 等模块

### 资产工具

`apps/tools/asset_builder/` 提供离线资产导入入口，当前重点面向 3D 资产编译链，而不是通用资源数据库系统。

## 前端 / Launcher 子项目

`apps/launcher_tauri/package.json` 显示 Launcher 使用：

- `@tauri-apps/cli`
- `@tauri-apps/api`
- `react`
- `react-dom`
- `typescript`
- `webpack`
- `webpack-dev-server`
- `framer-motion`
- `lucide-react`

它是一个相对独立的桌面前端子工程，不属于引擎核心运行时主线。

## 测试栈

当前仓库中的测试目录已收敛到 `tests/gtest/`，说明现阶段主线测试栈以 **GoogleTest + CTest** 为准：

- `tests/gtest/CMakeLists.txt` 作为入口
- `tests/gtest/unit/CMakeLists.txt` 定义 `dse_gtest_unit_tests`
- `gtest.engine.unit` 是当前明确可见的 `CTest` 条目
- `build_fast_tests.bat` 会构建 `dse_gtest_unit_tests` 并通过 `ctest -L gtest` 执行

## 环境变量与运行时配置

从 `engine_app.cpp` 与 `frame_pipeline.cpp` 可见，当前运行时大量使用环境变量控制行为：

- `DSE_DATA_ROOT`
- `DSE_RUNTIME_MODULES`
- `DSE_ASYNC_UPLOAD_BUDGET`
- `DSE_SCREENSHOT_PATH`
- `DSE_SCREENSHOT_TARGET`
- `DSE_DISABLE_STARTUP_SCENE_REGRESSION`

这说明当前 runtime 更偏向“环境变量驱动的轻量配置”，而不是复杂配置中心。

## 平台定位

当前仓库的真实验证口径明显偏向：

- **Windows**
- **MSVC / Visual Studio 2022**
- **本地批处理脚本 + CTest**

虽然部分依赖具备跨平台潜力，但当前主仓已验证主线仍应视为 Windows 优先。

## 结论

DSEngine 的 stack 不是单一应用栈，而是一个 **引擎核心 + 多宿主 runtime + 原生编辑器 + 动态 3D 模块 + 辅助桌面前端** 的复合仓库：

- 引擎核心：C++20 + CMake + OpenGL + GLFW
- 2D/3D 模块：ECS + Box2D + 条件式 PhysX
- 脚本层：Lua + sol2
- 编辑器层：Dear ImGui + ImGuizmo
- Launcher 层：Tauri + React + TypeScript
- 测试层：GoogleTest + CTest

整体风格是 **源码内置依赖、Windows 优先、以本地可执行工作流为中心**。
