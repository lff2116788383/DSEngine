# DSEngine Codebase Architecture

## 总体判断

DSEngine 当前属于 **brownfield 引擎仓库**：顶层分层相对清晰，但若干核心枢纽类职责偏重。最准确的高层结构可以概括为：

- `apps/`：宿主与工具入口层
- `engine/`：引擎核心与运行时基础设施
- `modules/`：玩法/功能模块层（2D 与 3D）
- `tests/`：单测、smoke、回归与门禁
- `assets/` / `data/` / `samples/` / `script/`：样例、脚本与运行资源

## 主要架构分层

### 1. 宿主入口层：`apps/`

宿主层负责“如何启动引擎”，而不是承载所有引擎逻辑：

- `apps/runtime/cpp_host/main.cpp`：C++ 业务宿主
- `apps/runtime/lua_host/main.cpp`：Lua 业务宿主
- `apps/editor_cpp/src/main.cpp`：编辑器宿主
- `apps/tools/asset_builder/main.cpp`：离线资产工具
- `apps/launcher_tauri/`：桌面 launcher 子项目

这一层决定了：

- 启动哪种业务模式
- 是否启用编辑器模式
- 如何组织工具 UI 或离线工作流

### 2. 引擎核心层：`engine/`

`engine/` 是仓库的中枢，承载：

- runtime 生命周期
- ECS world 与基础组件
- scene / prefab / transform
- render / RHI / render target / pipeline state
- scripting bridge
- assets / importer / cooker
- input / platform / profiler / core utilities

### 3. 模块层：`modules/`

`modules/` 是功能层扩展，当前分为两大方向：

- `modules/gameplay_2d/`
- `modules/gameplay_3d/`

2D 层包含：

- camera
- animation
- localization
- particle
- tilemap
- ui
- spine
- rendering

3D 层包含：

- animation
- ai
- camera
- particles
- rendering

这里体现的是“核心引擎 + gameplay 模块”的结构，而不是所有逻辑都塞进 `engine/`。

## 运行时主链

### EngineInstance 作为应用运行容器

`engine/runtime/engine_app.cpp` 中的 `EngineInstance` 是应用层级容器，负责：

- 衔接 `EngineRunConfig`
- 建立默认 `World` 与 `AssetManager`
- 初始化窗口/GL 上下文（非 editor 模式）
- 初始化 `Debug`、`JobSystem`
- 创建并驱动 `FramePipeline`
- 管理 `Init / Tick / Shutdown / Run`

它更像“运行时外壳 + 生命周期协调者”。

### FramePipeline 作为帧主循环中枢

`engine/runtime/frame_pipeline.cpp` 是当前最关键的高耦合点之一，负责：

- 检查 world / asset manager 注入
- 创建 RHI device
- 配置 data root
- 初始化 render targets / pipeline states
- 初始化 physics / audio / spine / mesh rendering 等系统
- 条件加载 Gameplay3D 动态模块
- 执行 scene regression sample
- 承接后续 update / render / fixed update

这意味着 `FramePipeline` 当前同时兼具：

- runtime bootstrap
- render graph 初始化
- 系统装配
- 模块加载
- 部分回归校验

该类是重要的架构枢纽，也是未来拆分风险最高的区域之一。

## 数据与依赖流

### 依赖方向（理想形态）

从目录与引用关系看，当前主要方向应为：

- `apps -> engine`
- `modules -> engine`
- `tests -> apps/engine/modules`

没有明显证据表明存在系统性 `engine -> apps` 反向依赖，这说明顶层方向基本健康。

### 运行时数据中心

在实际执行链中，几个关键共享对象是：

- `World`
- `AssetManager`
- `OpenGLRhiDevice` / RHI 资源
- 各类 runtime context / render resources

这些对象在 `EngineInstance` 与 `FramePipeline` 中被注入、保存并向系统扩散。

## 动态模块架构

3D gameplay 能力不是完全静态耦合到主 runtime 中，而是存在动态模块装配路径：

- `FramePipeline::Init()` 中会解析 `DSE_RUNTIME_MODULES`
- 在 `DSE_ENABLE_3D` 下尝试加载 `DSE_Gameplay3D`
- 通过 `CreateModule` / `DestroyModule` 导出函数进行模块实例化

说明当前 3D 方向具备一定插件化/动态库边界，但并未完全成为仓库默认主线。

关键位置：

- `engine/runtime/frame_pipeline.cpp`
- `modules/gameplay_3d/gameplay_3d_module.h`

## 编辑器架构

`apps/editor_cpp/` 当前是原生编辑器主线，具有明显的“引擎内嵌编辑器”风格：

- 编辑器直接链接 `dse_engine`
- 编辑器主程序同时感知 2D / 3D ECS 组件
- 面板模块拆分为多个 `editor_*` 文件
- UI 层用 ImGui DockSpace + 各面板组成
- 场景 IO 与 undo 功能直接位于编辑器代码中

这不是典型的“完全分层工具应用”，而更接近“对 runtime 直接进行工具化包装”。优点是迭代快，风险是宿主层容易过重。

## 资源与场景链路

当前仓库已形成一条相对完整的资产与场景链：

- 原始资源位于 `assets/`、`data/`、samples 对应目录
- 场景使用 JSON 表达（如 `assets/scenes/*.scene.json`）
- 离线工具 `AssetBuilder` 负责导入并烹饪运行时格式
- runtime / tests 可通过 `DSE_STARTUP_SCENE` 或样例路径驱动最小场景加载

说明场景与资产已经是独立重要子系统，而不只是 demo 辅助文件。

## 测试架构

测试不是“附属脚本”，而是仓库分层的一部分：

- `tests/engine/`：核心引擎、runtime、assets、render、scene 等测试
- `tests/modules/`：2D / 3D gameplay 模块测试
- `tests/spine/`：Spine 专项测试

`tests/engine/CMakeLists.txt` 中还将一组单目标程序注册成多个 `CTest` gate，形成“同一测试二进制 + 多个标签入口”的组织方式。

## 当前架构特征总结

### 优点

- 顶层目录职责较清晰
- 引擎、模块、宿主、测试四层边界基本存在
- 2D / 3D 能力已经形成模块化组织
- 测试与脚本/资源链路已进入主线，而不是散落 demo
- 编辑器与 runtime 共用核心系统，减少重复实现

### 风险

- `FramePipeline` 职责明显偏重
- 编辑器主程序感知过多底层细节
- 运行时仍保留部分全局态/单例兼容痕迹
- 3D 是“已接入但未完全收口”的架构状态
- 动态模块、资源链、编辑器、runtime 多链并存，系统复杂度较高

## 结论

DSEngine 的 architecture 更像一个 **正在从“单体引擎原型”向“可持续工程化引擎仓库”过渡的结构**：

- 分层已经形成
- 主链已经明确
- 测试与工具链正在成体系
- 但关键中枢类和编辑器壳层仍需进一步降耦与职责收口

对于后续 GSD 规划，这意味着应优先围绕：

- runtime 中枢收口
- 编辑器高频闭环
- 资产/测试/3D MVP 主链稳定性

而不是盲目扩展新功能面。
