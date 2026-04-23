# DSEngine Codebase Conventions

## 总体风格

DSEngine 是一个 **C++ 主导、中文文档与注释优先、Windows 本地工作流优先** 的仓库。代码风格并非完全机械统一，但整体有较明确的工程约定。

## 语言层面的约定

### C++ 是主线语言

核心逻辑主要位于：

- `engine/`
- `modules/`
- `apps/editor_cpp/`
- `apps/runtime/`
- `tests/`

说明默认的设计、重构与验证语言都是 C++，其他语言主要是宿主脚本或工具补充。

### Lua 用于脚本业务层

Lua 主要出现在：

- `script/`
- `samples/lua/`
- `engine/scripting/lua/`

约定上，Lua 不应臆造未导出的 API，必须依赖真实绑定层。

### TypeScript 仅用于 Launcher 子工程

`apps/launcher_tauri/` 是相对独立的技术栈，不应把其前端约定误投射到 `engine/` 或 `modules/`。

## 命名约定

### 文件命名

文件命名整体偏蛇形命名（snake_case）：

- `frame_pipeline.cpp`
- `engine_app.cpp`
- `cpp_business_runtime.cpp`
- `mesh_render_system.cpp`
- `editor_toolbar.cpp`

测试文件也遵循此规则：

- `runtime_smoke_snapshot_test.cpp`
- `mesh_render_system_material_resolution_test.cpp`
- `frame_pipeline_static_regression_test.cpp`

### 类型命名

类型命名主要采用 PascalCase：

- `EngineInstance`
- `FramePipeline`
- `AssetManager`
- `World`
- `Camera3DComponent`
- `MeshCooker`

### 函数命名

函数与成员函数主要采用 PascalCase 或动词短语形式：

- `Init()`
- `Shutdown()`
- `RunEngine(...)`
- `ConfigureCppBusinessHooks(...)`
- `SetAssetManager(...)`
- `ReadSceneColorRgba8WithSize()`

### 常量与开关

常量与编译开关大量使用全大写/前缀风格：

- `DSE_ENABLE_3D`
- `DSE_BUILD_ENGINE_TESTS`
- `DSE_STARTUP_SCENE`
- `DSE_DATA_ROOT`

说明仓库对“宏、开关、环境变量”有很清晰的命名分层。

## 架构与实现约定

### 1. 注入优先于隐式全局态

虽然仓库中仍存在兼容性痕迹，但当前明显在向“显式注入”迁移：

- `EngineInstance` 会注入 `World` 与 `AssetManager`
- `FramePipeline` 要求注入 `AssetManager`
- `SpineSystem` 通过 `SetAssetManager(...)` 接收依赖
- `cpp_business_runtime` 要求显式接收 `AssetManager&`

这是一条重要约定：**新代码应优先沿用注入模式，而不是继续扩大单例/全局态。**

### 2. 功能模块按子系统拆分

2D / 3D gameplay 逻辑多数按系统拆分：

- `camera_system`
- `particle_system`
- `tilemap_system`
- `mesh_render_system`
- `animator_system`

新功能若属于已有子系统，应优先延续该结构，而不是新开风格完全不同的文件组织方式。

### 3. 宿主与引擎边界相对明确

宿主层：

- 决定启动模式
- 组装配置
- 触发引擎运行

引擎层：

- 负责世界、场景、渲染、输入、资产、脚本桥接等核心行为

实现新功能时，若不是入口控制逻辑，不应轻易塞回 `apps/runtime/*/main.cpp`。

## 测试编写约定

### Catch 风格测试组织

从 `TEST_CASE(...)` 形态看，仓库测试采用 Catch 风格组织。命名偏向 Given/When/Then：

- `Given_DefaultTerrainComponent_When_Created_Then_IsDirtyIsTrue`
- `Given_EmptySpinePaths_When_Update_Then_ComponentStateRemainsUnchanged`

说明测试命名强调：

- 前置条件
- 触发动作
- 预期结果

### 标签化测试

`tests/engine/CMakeLists.txt` 会将单个测试二进制映射为多个 `CTest` 条目，并通过标签切分能力域：

- `engine.unit`
- `engine.2d.ui`
- `engine.2d.physics2d`
- `engine.3d.unit`
- `engine.3d.runtime_mvp_smoke`

因此新增测试时，通常不仅是写 `TEST_CASE`，还要考虑：

- 标签
- CTest 注册名
- 所属门禁层级

### Windows + Lua 单测经验约定

`doc-archive/TESTING_CTEST_GUIDE.md` 记录了明确的工程经验：

- 某些 Lua runtime 断言写法在 Windows 下可能触发假性挂起
- 推荐先求值到局部变量，再断言

这说明测试代码不仅有“框架层约定”，还有项目特定的稳定性写法约定。

## 文档与表达约定

### 中文优先

README、测试文档、规划文档均以中文为主；说明：

- 工程文档默认中文
- 说明语义优先于模板化英文术语

### 文档编号化沉淀

`doc-archive/` 目录的大量 `DOC-xx_*.md` 文件说明仓库倾向于：

- 用专题文档记录阶段性决策
- 把问题分析、计划、风险和 runbook 沉淀进 Markdown

如果改动涉及架构口径变化，通常应同步更新文档，而不只是改代码。

## 错误处理与系统行为倾向

### 运行时错误处理偏显式日志 / 返回失败

从 `engine/runtime/engine_app.cpp`、`frame_pipeline.cpp` 可见，失败处理经常采用：

- `std::cerr`
- `DEBUG_LOG_ERROR(...)`
- `return false`
- 明确 shutdown 清理

说明主线风格偏“显式失败路径 + 日志输出”，而不是异常驱动业务流。

### 环境变量驱动调试与测试

很多运行/测试特性通过环境变量开启：

- startup scene
- screenshot
- frame limit
- data root
- runtime modules

新增运行时调试入口时，应优先沿用这种风格，而不是平行造一套配置机制。

## 不应做的事（基于现状推断）

- 不要在未确认绑定存在前扩展 Lua API 口径
- 不要把新架构硬塞进 `main.cpp` 宿主层
- 不要绕过现有 `CTest + 标签` 体系只做手工验证
- 不要把 `reference/` 中的参考代码误当成当前主仓实现风格
- 不要在 `depends/` 中修改第三方代码来承载主线需求，除非这是明确的 vendored patch 策略

## 结论

DSEngine 的 conventions 可以概括为：

- C++ 主线、snake_case 文件名、PascalCase 类型/方法
- 运行时显式注入优先
- gameplay 以系统化目录组织
- 测试采用 Catch 风格 + CTest 标签门禁
- 中文文档优先、Windows 本地工作流优先
- 改动代码时通常要同步考虑文档与测试口径

对后续规划来说，最重要的不是死记格式，而是**延续现有结构与依赖注入方向，避免制造新的风格分叉**。
