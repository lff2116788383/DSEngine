# DSEngine Codebase Structure

## 顶层目录结构

当前仓库顶层目录可按职责理解为：

- `apps/`：应用入口、编辑器、Launcher、资产工具
- `assets/`：场景与资源样例
- `cmake/`：第三方依赖与构建辅助脚本
- `data/`：运行时默认数据根目录资源
- `doc-archive/`：架构、构建、测试、路线图历史文档
- `engine/`：引擎核心与运行时基础设施
- `modules/`：2D / 3D gameplay 模块
- `samples/`：C++ / Lua 样例逻辑
- `script/`：脚本层附加脚本
- `tests/`：CTest 相关测试代码
- `third_party/`：额外第三方源码（如 `imguizmo`）
- `depends/`：主要第三方依赖源码镜像
- `reference/`：参考引擎/参考资料仓内容

## 关键目录说明

### `apps/`

核心子目录包括：

- `apps/runtime/cpp_host/`
- `apps/runtime/lua_host/`
- `apps/editor_cpp/`
- `apps/tools/asset_builder/`
- `apps/launcher_tauri/`

命名规律很直接：

- `runtime/*_host`：不同业务宿主
- `editor_cpp`：当前主线编辑器实现
- `tools/*`：离线工具入口
- `launcher_tauri`：独立桌面前端应用

### `engine/`

`engine/` 是核心目录，内部大致按子系统拆分：

- `engine/runtime/`：引擎生命周期、帧循环、runtime shell
- `engine/assets/`：资源系统、导入/烹饪相关
- `engine/core/`：基础设施（作业系统、事件总线、动态库等）
- `engine/ecs/`：组件与 `World`
- `engine/scene/`：场景、prefab、transform、序列化相关
- `engine/render/`：渲染与 RHI
- `engine/scripting/`：Lua / C++ 业务桥接
- `engine/physics/`：2D / 3D 物理
- `engine/platform/`：平台尺寸等基础适配
- `engine/profiler/`：CPU / Memory / Render profiler
- `engine/input/`：输入采集
- `engine/base/`：底层工具与基础函数

### `modules/`

功能模块按领域分层：

- `modules/gameplay_2d/`
- `modules/gameplay_3d/`

这两个目录中，再按系统拆分为子目录，比如：

- `camera/`
- `animation/`
- `rendering/`
- `particle(s)/`
- `ui/`
- `tilemap/`
- `localization/`
- `ai/`

说明仓库结构偏向"按功能子系统分文件夹"，而不是"所有 gameplay 混在单一目录中"。

### `tests/`

测试目录延续了主代码结构：

- `tests/engine/`：引擎核心测试
- `tests/modules/`：模块层测试
- `tests/spine/`：Spine 专项测试

这种布局有利于在阅读实现时快速找到对应测试。

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
- `apps/editor_cpp/src/main.cpp`
- `tests/engine/CMakeLists.txt`
- `doc/TESTING_CTEST_GUIDE.md`

## 命名与组织规律

### 1. 系统命名

C++ 文件命名主要采用：

- `*_system.cpp/.h`
- `*_test.cpp`
- `editor_*.cpp/.h`
- `*_runtime*.cpp/.h`
- `*_builder*.cpp/.h`

这说明"系统 / 测试 / 编辑器面板 / runtime / 工具"语义被显式编码到文件名中。

### 2. 测试命名

测试文件多使用：

- `xxx_test.cpp`
- `xxx_smoke_test.cpp`
- `xxx_snapshot_test.cpp`
- `xxx_static_test.cpp`
- `xxx_regression_test.cpp`

表示仓库测试不止有单元测试，还区分：

- smoke
- snapshot
- static regression
- runtime integration

### 3. 文档命名

`doc-archive/` 下文档使用 `DOC-xx_` 前缀，说明该仓库曾经长期以"编号专题文档"方式沉淀架构与计划材料。

例如：

- `doc-archive/DOC-01_ARCHITECTURE.md`
- `doc-archive/DOC-04_TESTING.md`
- `doc-archive/DOC-19_3D_ALIGNMENT_WITH_VSENGINE21_PLAN.md`
- `doc-archive/TESTING_CTEST_GUIDE.md`

## 结构上的特殊点

### `depends/` 很大，但不是主要业务代码

`depends/` 包含大量第三方源码，搜索时很容易产生噪音。对理解主仓业务代码而言，通常应优先查看：

- `apps/`
- `engine/`
- `modules/`
- `tests/`
- `doc-archive/`

除非要确认某个第三方库真实存在或排查集成问题，否则不应在 `depends/` 中长时间漫游。

### `reference/` 是参考资料，不是当前主线实现

`reference/` 包含参考工程与外部资源，对齐或迁移时有价值，但不应与当前主仓实现混淆。做规划时要明确：

- 当前真实实现看 `engine/`、`modules/`、`apps/`
- 参考来源看 `reference/`

### `apps/launcher_tauri/` 是次要子工程

虽然 launcher 有独立前端栈，但从 README 与主仓重心看，它当前更像辅助入口与后续整合方向，而不是最核心交付物。

## 建议的阅读顺序

### 新人/AI 快速上手顺序

1. `README.md`
2. `CMakeLists.txt`
3. `engine/runtime/engine_app.cpp`
4. `engine/runtime/frame_pipeline.cpp`
5. `apps/editor_cpp/src/main.cpp`
6. `tests/engine/CMakeLists.txt`
7. `doc-archive/TESTING_CTEST_GUIDE.md`

### 针对不同目标

- **看 runtime 主链**：`apps/runtime/` → `engine/runtime/`
- **看编辑器**：`apps/editor_cpp/src/`
- **看 2D 模块**：`modules/gameplay_2d/`
- **看 3D MVP**：`modules/gameplay_3d/` + `assets/scenes/3d_mvp_minimal.scene.json`
- **看测试门禁**：`tests/engine/CMakeLists.txt` + `doc-archive/TESTING_CTEST_GUIDE.md`

## 结论

DSEngine 的 structure 具备较好的仓库级分层：

- 顶层职责清楚
- 模块布局基本稳定
- 入口文件可预期
- 测试与实现目录相互映射

真正需要注意的不是"目录是否混乱"，而是某些关键实现类在逻辑职责上偏重；从结构本身看，仓库已经具备继续工程化治理的基础。
