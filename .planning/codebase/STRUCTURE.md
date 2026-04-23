# DSEngine Codebase Structure

## 顶层目录结构

当前仓库顶层目录可按职责理解为：

- `apps/`：宿主、编辑器、Launcher、资产工具
- `cmake/`：第三方依赖与构建辅助脚本
- `data/`：默认运行时数据根目录资源
- `engine/`：引擎核心与运行时基础设施
- `modules/`：2D / 3D gameplay 模块
- `samples/`：C++ / Lua 样例逻辑
- `script/`：脚本层基础脚本与扩展脚本
- `tests/`：当前 GoogleTest 测试入口
- `depends/`：主要第三方依赖源码镜像
- `third_party/`：额外第三方源码（如 `imguizmo`）
- `reference/`：参考工程与资料仓
- `.planning/`：项目规划与代码库地图文档

## 关键目录说明

### `apps/`

核心子目录包括：

- `apps/runtime/cpp_host/`
- `apps/runtime/lua_host/`
- `apps/editor_cpp/`
- `apps/tools/asset_builder/`
- `apps/launcher_tauri/`

命名规律：

- `runtime/*_host`：不同业务宿主
- `editor_cpp`：当前原生编辑器实现
- `tools/*`：离线工具入口
- `launcher_tauri`：独立桌面前端子工程

### `engine/`

`engine/` 是核心目录，内部按子系统拆分：

- `engine/runtime/`：引擎生命周期、帧循环、runtime shell
- `engine/assets/`：资源系统、导入/烹饪链
- `engine/core/`：基础设施（作业系统、事件总线、动态库等）
- `engine/ecs/`：组件与 `World`
- `engine/scene/`：场景、transform、序列化相关
- `engine/render/`：渲染与 RHI
- `engine/scripting/`：Lua / C++ 业务桥接
- `engine/physics/`：2D / 3D 物理
- `engine/platform/`：平台尺寸与基础适配
- `engine/profiler/`：CPU / Memory / Render profiler
- `engine/input/`：输入采集
- `engine/base/`：底层工具函数

### `modules/`

功能模块按领域分层：

- `modules/gameplay_2d/`
- `modules/gameplay_3d/`

两者内部继续按子系统划分，例如：

- `camera/`
- `animation/`
- `rendering/`
- `particle` / `particles`
- `ui/`
- `tilemap/`
- `localization/`
- `ai/`

说明仓库结构偏向“按功能子系统分目录”，而不是把 gameplay 代码混在一个目录中。

### `tests/`

当前 `tests/` 目录已经收敛，只保留：

- `tests/gtest/`
- `tests/gtest/unit/`

这意味着当前真实测试入口是 **GoogleTest + CTest** 的新布局，而不是旧的 `tests/engine/` / `tests/modules/` / `tests/spine/` 分层结构。

### `data/`

`data/` 是当前默认运行时资源根目录，已确认包含：

- `font/`
- `mirror_assets/`
- `models/`

这说明当前默认资源主线更偏向 `data/`，而不是独立顶层 `assets/` 目录。

### `samples/` 与 `script/`

- `samples/cpp/`：C++ 宿主样例逻辑
- `samples/lua/`：Lua 样例与 demo
- `script/`：脚本基础模块与扩展能力

这两层共同承载运行时样例与脚本侧开发入口。

## 入口文件位置

### Runtime 入口

- `apps/runtime/cpp_host/main.cpp`
- `apps/runtime/lua_host/main.cpp`

### Editor 入口

- `apps/editor_cpp/src/main.cpp`

### 资产工具入口

- `apps/tools/asset_builder/main.cpp`

### 顶层构建入口

- `CMakeLists.txt`
- `build_all.bat`
- `build_fast_tests.bat`

## 高价值文件位置

如果要快速理解当前仓库，优先阅读：

- `README.md`
- `CMakeLists.txt`
- `engine/runtime/engine_app.cpp`
- `engine/runtime/frame_pipeline.cpp`
- `modules/gameplay_3d/gameplay_3d_module.h`
- `apps/editor_cpp/src/main.cpp`
- `tests/gtest/unit/CMakeLists.txt`
- `build_fast_tests.bat`
- `build_all.bat`

## 命名与组织规律

### 1. 系统命名

C++ 文件命名主要采用：

- `*_system.cpp/.h`
- `*_module.cpp/.h`
- `*_runtime*.cpp/.h`
- `editor_*.cpp/.h`
- `*_builder*.cpp/.h`

这说明“系统 / 模块 / runtime / 编辑器 / 工具”语义被显式编码在文件名中。

### 2. 测试命名

当前可见测试文件主要是：

- `smoke_gtest.cpp`

这表明仓库已经开始用 GoogleTest 重建测试骨架。后续扩展时，建议沿用：

- `*_test.cpp`
- `*_smoke_test.cpp`
- `*_integration_test.cpp`
- `*_benchmark.cpp`

但这些后续命名应以新 GTest 体系为准，而不是默认继承旧测试布局。

### 3. 文档命名

- `.planning/codebase/*.md`：当前代码库地图文档
- `.planning/ROADMAP.md` / `PROJECT.md` / `REQUIREMENTS.md`：GSD 规划文档
- `doc-archive/DOC-xx_*.md`：历史专题文档

说明当前仓库同时存在“现行规划文档”与“历史归档文档”两条文档线。

## 结构上的特殊点

### `depends/` 很大，但不是主要业务代码

`depends/` 包含大量第三方源码，搜索时容易产生噪音。理解主仓业务代码时应优先查看：

- `apps/`
- `engine/`
- `modules/`
- `tests/`
- `.planning/`
- `README.md`

除非是确认第三方库真实存在或排查集成问题，不建议在 `depends/` 中长时间漫游。

### `reference/` 是参考资料，不是当前主线实现

`reference/` 包含参考工程和外部资料，对齐或迁移时有价值，但不能与当前真实实现混淆。判断当前主线时应优先看：

- `engine/`
- `modules/`
- `apps/`
- `tests/gtest/`

### `apps/launcher_tauri/` 是相对独立的子工程

虽然 Launcher 有独立前端栈，但从构建脚本与主仓重心看，它更像辅助入口和后续整合方向，不是当前核心交付主线。

## 建议的阅读顺序

### 新人 / AI 快速上手顺序

1. `README.md`
2. `CMakeLists.txt`
3. `engine/runtime/engine_app.cpp`
4. `engine/runtime/frame_pipeline.cpp`
5. `modules/gameplay_3d/gameplay_3d_module.h`
6. `apps/editor_cpp/src/main.cpp`
7. `tests/gtest/unit/CMakeLists.txt`
8. `build_fast_tests.bat`

### 针对不同目标

- **看 runtime 主链**：`apps/runtime/` → `engine/runtime/`
- **看编辑器**：`apps/editor_cpp/src/`
- **看 2D 模块**：`modules/gameplay_2d/`
- **看 3D 模块**：`modules/gameplay_3d/` + `CMakeLists.txt` + `frame_pipeline.cpp`
- **看测试门禁**：`tests/gtest/` + `build_fast_tests.bat` + `build_all.bat`

## 结论

DSEngine 的 structure 仍具备较好的仓库级分层：

- 顶层职责清楚
- 模块布局稳定
- 入口文件可预期
- runtime / editor / tool / test 边界基本存在

当前最需要注意的不是目录混乱，而是**旧测试结构认知与当前 GTest 新入口之间的口径漂移**。从结构本身看，仓库已经具备继续工程化治理的基础。
