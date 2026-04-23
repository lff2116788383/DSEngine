# DSEngine Codebase Architecture

## 总体判断

DSEngine 当前属于 **brownfield 引擎仓库**：顶层分层仍然清晰，但核心运行链与工具链处于“主线已形成、局部正在收口”的状态。当前最准确的高层结构可概括为：

- `apps/`：宿主、编辑器、桌面 Launcher、资产工具入口层
- `engine/`：引擎核心、runtime、RHI、脚本桥接、资产与基础设施
- `modules/`：2D / 3D gameplay 功能模块
- `tests/`：当前收敛后的 GoogleTest 测试入口
- `data/` / `samples/` / `script/`：默认运行资源、样例与脚本层资源
- `depends/` / `third_party/`：本地 vendored 第三方依赖

## 主要架构分层

### 1. 宿主入口层：`apps/`

宿主层负责“如何启动或包装引擎”，而不是承载全部引擎逻辑：

- `apps/runtime/cpp_host/main.cpp`：C++ 业务宿主
- `apps/runtime/lua_host/main.cpp`：Lua 业务宿主
- `apps/editor_cpp/src/main.cpp`：原生编辑器宿主
- `apps/tools/asset_builder/main.cpp`：离线资产导入与烹饪入口
- `apps/launcher_tauri/`：桌面 Launcher 子工程

这一层决定：

- 使用哪种业务宿主
- 是否启用编辑器模式
- 是否进入离线工具流或桌面辅助入口

### 2. 引擎核心层：`engine/`

`engine/` 是仓库的中枢，承载：

- runtime 生命周期
- ECS world 与组件基础设施
- 场景、transform 与序列化
- render / RHI / render target / pipeline state
- scripting bridge
- assets / importer / cooker
- input / platform / profiler / core utilities
- 2D / 3D 物理基础层

### 3. 模块层：`modules/`

`modules/` 是 gameplay 扩展层，当前分为：

- `modules/gameplay_2d/`
- `modules/gameplay_3d/`

2D 层包含 camera / animation / localization / particle / tilemap / ui / spine / rendering 等子系统；3D 层包含 animation / ai / camera / particles / rendering 等子系统。

这里体现的是“核心引擎 + gameplay 模块”的结构，而不是把所有能力都塞进 `engine/`。

## 运行时主链

### EngineInstance 作为运行容器

`engine/runtime/engine_app.cpp` 中的 `EngineInstance` 当前负责：

- 组装 `EngineRunConfig`
- 注入或创建默认 `World` 与 `AssetManager`
- 初始化 GLFW / glad（非 editor 模式）
- 初始化 `Debug` 与 `JobSystem`
- 创建并驱动 `FramePipeline`
- 执行 `Init / Tick / Shutdown / Run`
- 触发 startup scene regression 与截图输出等 runtime 级辅助行为

它是“应用外壳 + 生命周期协调者”。

### FramePipeline 作为帧主循环中枢

`engine/runtime/frame_pipeline.cpp` 是当前最关键的高耦合点之一，负责：

- world / asset manager 注入校验
- RHI device 创建与数据根配置
- render target 与 pipeline state 初始化
- 2D 物理、音频、Spine、mesh render 等系统装配
- 条件加载 `DSE_Gameplay3D` 动态模块
- 业务 bootstrap
- update / fixed update / render 主链执行

同时，仓库已经开始把部分帧逻辑拆到：

- `runtime_frame_ops.cpp`
- `runtime_update_graph.cpp`
- `runtime_render_shell.cpp`

说明 runtime 中枢已有收口趋势，但 `FramePipeline` 仍然是关键复杂度中心。

## 动态模块架构

3D gameplay 能力不是完全静态耦合在主 runtime 中，而是保留了动态模块边界：

- `CMakeLists.txt` 在 `DSE_ENABLE_3D=ON` 时构建 `DSE_Gameplay3D`
- `FramePipeline::Init()` 会读取 `DSE_RUNTIME_MODULES`
- 运行时按候选动态库名尝试加载 `DSE_Gameplay3D`
- 模块通过 `CreateModule` / `DestroyModule` 与 `core::IModule` 接口接入

同时，`dse_engine` 也静态编入了自身直接依赖的 3D 实现文件（如 `mesh_render_system.cpp`、`animation_state_machine.cpp`），说明当前 3D 仍处于“动态模块边界 + 主库局部直连共存”的过渡态。

### PhysX 降级边界

当前 3D 模块的物理后端不是无条件可用：

- `DSE_ENABLE_PHYSX` 控制是否启用 PhysX 方向
- `DSE_HAS_PHYSX_LIBS` 反映本机是否真的存在 PhysX `.lib`
- 无 PhysX 库时，`Gameplay3DModule` 会跳过 `Physics3DSystem` 成员与调用，仍允许 3D 模块构建通过

这表明 3D 模块当前具备“无物理后端降级”的本地适配能力。

## 编辑器架构

`apps/editor_cpp/` 当前是原生编辑器主线，明显属于“引擎内嵌编辑器”风格：

- 编辑器直接链接 `dse_engine`
- 编辑器主程序直接感知 2D / 3D ECS 组件与相机矩阵
- 面板式结构拆分为多个 `editor_*` 文件
- UI 由 ImGui DockSpace + 各面板组成
- 场景 IO、部分导出与本地化数据种子逻辑直接位于编辑器层

优点是迭代快、共用 runtime 能力；风险是宿主层容易过重。

## 资源与场景链路

当前仓库已形成较清晰的资源与场景链：

- 默认运行资源位于 `data/`
- 脚本与 demo 位于 `samples/` 与 `script/`
- 场景使用 JSON 表达
- `AssetBuilder` 负责 glTF / GLB / FBX 到运行时资产格式的离线导入与烹饪
- runtime 可通过环境变量如 `DSE_STARTUP_SCENE` 驱动最小场景启动

这说明“资源与场景”已经是主线子系统，而不是单纯 demo 辅助物。

## 测试架构

当前测试架构已经从旧布局收敛到新入口：

- `tests/` 当前只保留 `tests/gtest/`
- `tests/gtest/unit/` 负责当前可见的单元测试入口
- `dse_gtest_unit_tests` 是当前明确构建目标
- `gtest.engine.unit` 是当前明确注册的 `CTest` 条目

因此当前架构里，测试更准确地应理解为：

- **GoogleTest + CTest 的新入口正在建立中**
- 旧的 `tests/engine/` / `tests/modules/` / `tests/spine/` 布局不应再作为当前真实结构引用

## 当前架构特征总结

### 优点

- 顶层目录职责仍然清晰
- 引擎、模块、宿主、工具的边界基本存在
- runtime 主链已经形成
- 3D 动态模块边界存在，且支持本机缺 PhysX 时降级构建
- 本地 GoogleTest 接入已经落地，测试骨架可继续扩展

### 风险

- `FramePipeline` 仍然职责偏重
- 编辑器主程序感知过多底层细节
- 3D 模块边界与主库直连共存，属于过渡态
- 测试地图与代码现状在近期发生过漂移，需要持续同步
- Launcher / Editor / Runtime / 3D / 资产链并行存在，系统复杂度较高

## 结论

DSEngine 的 architecture 更像一个 **正在从单体引擎原型向可持续工程化仓库演进** 的结构：

- 分层已经形成
- runtime 主链明确
- 编辑器与 3D 能力已接入
- 但测试体系、3D 模块边界和 runtime 中枢仍在收口过程中

对于后续 GSD 规划，这意味着优先级应继续围绕：

- runtime 中枢收口
- GTest 测试基线建设
- 3D 模块与 Lua 资产链主路径稳定性
- 文档与代码口径持续同步
