# DSEngine Codebase Stack

## 概览

DSEngine 是一个以 **C++ + CMake** 为主线的游戏引擎仓库，当前默认开发平台是 **Windows + Visual Studio 2022 + CTest**。仓库同时包含三类宿主：

- `apps/runtime/cpp_host/`：C++ 业务宿主
- `apps/runtime/lua_host/`：Lua 业务宿主
- `apps/editor_cpp/`：C++ 原生编辑器宿主

此外还存在一个辅助工具端：`apps/launcher_tauri/`，采用 **Tauri + React + TypeScript + Webpack**。

## 主要语言与运行时

- **C++20**：引擎核心、运行时、编辑器、资产工具、测试主线
- **Lua**：脚本业务层与 demo/sample
- **TypeScript / React 19**：Launcher 前端层
- **Rust（通过 Tauri）**：Launcher 桌面壳依赖
- **Batch / PowerShell**：Windows 本地构建与验证脚本

## 构建系统

### 顶层构建

顶层构建由 `CMakeLists.txt` 驱动，核心特征：

- 使用 `project(DSEngine C CXX)` 定义工程
- 默认 `CMAKE_CXX_STANDARD 20`
- 输出目录统一到 `bin/`
- 通过 option 控制主要能力开关：
  - `DSE_ENABLE_2D`
  - `DSE_ENABLE_3D`
  - `DSE_ENABLE_PHYSX`
  - `DSE_ENABLE_SPINE`
  - `DSE_BUILD_EDITOR`
  - `DSE_BUILD_LAUNCHER`
  - `DSE_BUILD_ENGINE_TESTS`

相关文件：

- `CMakeLists.txt`
- `cmake/CMakeLists.txt.glfw`
- `cmake/CMakeLists.txt.FreeType`
- `cmake/CMakeLists.txt.lua_script`
- `cmake/CMakeLists.txt.Physx`

### Windows 构建脚本

仓库以 Windows 本地脚本作为第一层工作流：

- `build_all.bat`：全量构建与验证入口
- `build_fast_cpp.bat`：快速构建 C++ 主线
- `build_fast_editor.bat`：快速构建编辑器
- `build_fast_launcher.bat`：快速构建 Launcher
- `build_fast_lua.bat`：快速跑 Lua 相关链路
- `build_fast_tests.bat`：快速构建并执行最小门禁测试

`build_all.bat` 还会主动检查：

- `cmake`
- `python`
- `node`
- `npm`
- `cargo` / Rust toolchain

说明当前仓库明确面向“本地一键构建 + 分层脚本”的工作模式。

## 核心原生依赖

顶层 `CMakeLists.txt` 与子模块目录表明，当前主线依赖主要以内置源码方式集成，而不是系统包管理器动态装配：

- `depends/entt-3.13.0`：ECS
- `depends/box2d-2.4.1`：2D 物理
- `depends/glfw-3.3-3.4`：窗口与输入
- `depends/glad` / `glad_gl.c`：OpenGL loader
- `depends/imgui`：编辑器 UI
- `third_party/imguizmo`：编辑器 Gizmo
- `depends/spine-runtimes`：Spine 运行时
- `depends/freetype-2.11.0`：字体
- `depends/rapidjson`：JSON 解析
- `depends/spdlog`：日志
- `depends/sol2-3.2.2` 与内置 `depends/lua`：Lua 绑定层
- `depends/luasocket-3.0.0`：Lua 网络能力
- `depends/tinygltf`：glTF 导入支持
- `depends/assimp`：资产导入（条件启用）
- `depends/physx`：3D 物理（条件启用）
- `depends/tiny-AES-c`、`depends/bundle`：资产与工具辅助依赖

## 渲染与平台栈

从 `engine/runtime/engine_app.cpp`、`engine/runtime/frame_pipeline.cpp`、`apps/editor_cpp/CMakeLists.txt` 可见：

- 当前渲染后端是 **OpenGL-first**
- 窗口层使用 **GLFW**
- Runtime 与 Editor 都依赖 `glad + GLFW + OpenGL`
- 编辑器通过 `Dear ImGui + ImGuizmo` 构建原生工具 UI

关键文件：

- `engine/runtime/engine_app.cpp`
- `engine/runtime/frame_pipeline.cpp`
- `engine/render/rhi/rhi_device.cpp`
- `apps/editor_cpp/CMakeLists.txt`
- `apps/editor_cpp/src/main.cpp`

## 脚本与业务宿主

仓库同时支持 **Lua / C++ 双业务宿主**：

- `apps/runtime/cpp_host/main.cpp` 使用 `ConfigureCppBusinessHooks(...)` 注入 C++ 业务逻辑
- `apps/runtime/lua_host/main.cpp` 使用 `RunEngine(...)` + Lua startup script 启动脚本业务
- `engine/scripting/lua/` 与 `engine/scripting/cpp/` 分别承载两条业务桥接链路

关键文件：

- `apps/runtime/cpp_host/main.cpp`
- `apps/runtime/lua_host/main.cpp`
- `engine/scripting/cpp/cpp_business_runtime.cpp`
- `engine/scripting/lua/lua_runtime.cpp`

## 编辑器与工具层

### 原生编辑器

当前主线编辑器是 `apps/editor_cpp/`：

- C++ 原生可执行程序 `dse_editor_cpp`
- 直接链接 `dse_engine`
- 面板式结构，包含 hierarchy / inspector / profiler / toolbar / viewport 等模块

关键文件：

- `apps/editor_cpp/CMakeLists.txt`
- `apps/editor_cpp/src/main.cpp`
- `apps/editor_cpp/src/editor_shell.cpp`
- `apps/editor_cpp/src/editor_toolbar.cpp`
- `apps/editor_cpp/src/editor_scene_io.cpp`

### 资产工具

`apps/tools/asset_builder/main.cpp` 提供离线资产导入/烹饪入口，支持：

- `gltf`
- `glb`
- `fbx`

输出运行时资产格式：

- `.dmesh`
- `.dmat`
- `.danim`
- `.dskel`

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

说明该目录是一个相对独立的前端/桌面应用子工程，而不是引擎核心的一部分。

## 环境变量与运行时配置

从 `engine/runtime/frame_pipeline.cpp`、`engine/runtime/engine_app.cpp`、测试与文档可见，当前运行时已大量使用环境变量控制行为：

- `DSE_DATA_ROOT`
- `DSE_RUNTIME_MODULES`
- `DSE_ASYNC_UPLOAD_BUDGET`
- `DSE_SCREENSHOT_PATH`
- `DSE_SCREENSHOT_TARGET`
- `DSE_STARTUP_SCENE`
- `DSE_MAX_FRAMES`

这说明仓库在“测试门禁 / runtime smoke / 资源路径控制”上偏向环境变量驱动，而不是复杂配置中心。

## 平台定位

当前仓库的默认主线明显偏向：

- **Windows**
- **MSVC / Visual Studio 2022**
- **本地构建脚本 + CTest**

虽然部分依赖本身支持跨平台，但从脚本、README 和测试文档口径看，主仓的真实验证基线仍是 Windows。

## 结论

DSEngine 的 stack 不是“单一应用栈”，而是一个多宿主、多层次仓库：

- 引擎核心：C++20 + CMake + OpenGL + GLFW
- 2D/3D 模块：ECS + Box2D + Spine + 条件式 PhysX/Assimp
- 脚本层：Lua + sol2
- 编辑器层：Dear ImGui + ImGuizmo
- 辅助桌面前端：Tauri + React + TypeScript
- 测试层：CTest + Catch 风格测试组织

整体风格是 **源码内置依赖、Windows 优先、以本地可执行工作流为中心**。
