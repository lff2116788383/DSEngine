# DOC-02 构建与运行

本文档只保留当前主线的构建、运行与环境要求，统一以 Windows + CMake + Visual Studio 2022 为首要参考环境。

## 1. 当前构建对象

当前仓库主线包括以下可构建对象：

- `dse_engine`：引擎动态库
- `DSEngine_c++`：C++ Runtime 宿主
- `DSEngine_lua`：Lua Runtime 宿主
- `dse_editor_cpp`：C++ 编辑器
- `dse_engine_unit_tests`：引擎单测入口
- `dse_lua_runtime_tests`：Lua 运行时专项测试
- `dse_spine_tests`：Spine 专项测试
- `apps/launcher_tauri/`：Tauri 启动器前端

## 2. 环境要求

### 2.1 引擎与编辑器

- CMake 3.17+
- C++17 编译器
- Visual Studio 2022
- OpenGL 开发环境

### 2.2 启动器

- Node.js 18+
- npm
- Rust / Cargo
- Tauri CLI

## 3. 关键 CMake 选项

顶层 `CMakeLists.txt` 当前关键选项：

- `DSE_ENABLE_2D=ON`：默认启用
- `DSE_ENABLE_3D=OFF`：默认关闭
- `DSE_BUILD_EDITOR=OFF`：默认关闭
- `DSE_BUILD_LAUNCHER=OFF`：默认关闭
- `DSE_BUILD_ENGINE_TESTS=OFF`：默认关闭

注意：

- 编辑器不是默认构建目标，必须显式传 `-DDSE_BUILD_EDITOR=ON`
- 测试默认不构建，必须显式传 `-DDSE_BUILD_ENGINE_TESTS=ON`
- 3D 不是当前默认稳定构建路径

## 4. 构建 Runtime

在项目根目录执行：

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build_vs2022 --config Debug --target dse_engine
cmake --build build_vs2022 --config Debug --target DSEngine_c++ DSEngine_lua
```

构建完成后，产物默认输出到 `bin/`。

## 5. 运行宿主

### 5.1 C++ 宿主

```bash
bin\DSEngine_c++_debug.exe
```

当前入口：`apps/runtime/cpp_host/main.cpp`

### 5.2 Lua 宿主

```bash
bin\DSEngine_lua_debug.exe
```

当前入口：`apps/runtime/lua_host/main.cpp`

说明：

- 默认启动脚本为 `samples/lua/main.lua`
- 运行时会探测 `script/`、`samples/`、`data/` 路径

## 6. 构建编辑器

编辑器目标默认关闭，需显式开启：

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=ON
cmake --build build_vs2022 --config Debug --target dse_editor_cpp
```

运行：

```bash
bin\dsengine-editor.exe
```

当前编辑器位于：`apps/editor_cpp/`

## 7. 构建与运行测试

### 7.1 构建测试

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_ENGINE_TESTS=ON
cmake --build build_vs2022 --config Debug --target dse_engine_unit_tests dse_lua_runtime_tests dse_spine_tests
```

### 7.2 运行全部引擎测试标签

```bash
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L engine
```

### 7.3 运行资源注入最小门禁

```bash
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.lua_runtime|engine.cpp_runtime|engine.resource_injection|engine.spine"
```

### 7.4 运行 Lua Runtime 专项

```bash
ctest --test-dir build_vs2022 -C Debug -R engine.lua_runtime -V
```

### 7.5 运行关键 2D smoke

```bash
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.2d.(ui|tilemap|particle|physics2d|localization|animation|camera)"
```

## 8. 一键脚本

仓库根目录提供：`build_all.bat`

常用示例：

```bat
build_all.bat --with-tests --no-editor --no-launcher --no-sdk --no-verify-exe
build_fast_tests.bat
```

脚本当前负责：

- 环境检查
- CMake 配置
- Debug 构建
- 可选测试执行
- 可选编辑器与启动器相关流程

说明：该脚本偏重 Windows 本地开发环境，不等同于 CI 流水线定义。

## 9. 启动器构建

启动器位于：`apps/launcher_tauri/`

安装依赖并启动开发模式：

```bash
cd apps/launcher_tauri
npm install
npm run dev
```

打包 Tauri 应用：

```bash
cd apps/launcher_tauri
npm run tauri build
```

## 10. 常见问题

### 10.1 为什么没有生成 `dse_editor_cpp`

因为顶层默认 `DSE_BUILD_EDITOR=OFF`，需要重新执行带 `-DDSE_BUILD_EDITOR=ON` 的 CMake 配置命令。

### 10.2 为什么没有生成测试目标

因为顶层默认 `DSE_BUILD_ENGINE_TESTS=OFF`，需要重新执行带测试开关的配置命令。

### 10.3 当前是否默认构建 3D 主线

不是。当前 `DSE_ENABLE_3D` 默认关闭，3D 不是当前默认稳定构建路径。

## 11. 当前建议开发顺序

- 第一步：先构建 `dse_engine`
- 第二步：运行 `DSEngine_lua` 或 `DSEngine_c++`
- 第三步：按需开启 `dse_editor_cpp`
- 第四步：开启 `DSE_BUILD_ENGINE_TESTS` 跑回归
- 第五步：单独处理 `apps/launcher_tauri/`
