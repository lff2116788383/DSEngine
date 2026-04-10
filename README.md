# Dark Soul Engine (DSEngine)

DSEngine 是一个面向**个人独立开发者**与**中小型工作室**的轻量级高性能 C++ 游戏引擎。当前主线聚焦：

- **2D Runtime**
- **Lua / C++ 双业务宿主**
- **原生 C++ 编辑器 (`apps/editor_cpp/`)**
- **Windows + CMake + Visual Studio 2022** 开发环境

当前最准确的定位是：**2D 主线已基本成型并接近 Stable，编辑器基础可用，测试体系已建立，3D 已接入但不是默认稳定主线。**

## 当前项目定位

DSEngine 不是“大而全”的通用商业引擎。当前更适合以下目标：

- 2D 游戏原型开发
- 小型到中小体量 2D 项目开发
- Lua 驱动的快速玩法迭代
- 需要 C++ 性能与轻量工作流的团队

当前不建议对外表述为：

- 完整商用品质 3D 引擎
- 完整资源数据库 / UUID / meta 工作流引擎
- 完整大型团队协作编辑器平台

## 当前主线

当前仓库主线由三部分组成：

1. **Runtime (`apps/runtime/`)**
   - 提供 `cpp_host` 与 `lua_host`
   - 是当前运行时实际入口

2. **Editor (`apps/editor_cpp/`)**
   - 采用 **C++ + Dear ImGui + GLFW + OpenGL**
   - 直接链接 `dse_engine`
   - 当前是仓库内唯一主线编辑器实现

3. **Launcher (`apps/launcher_tauri/`)**
   - 采用 **Tauri + React + TypeScript**
   - 当前属于辅助入口与后续整合方向

## 当前已可用能力

### Runtime 与基础设施
- C++ / Lua 双业务宿主
- 主循环、固定步长更新、基础帧流水线
- OpenGL-first 渲染后端
- 资源根目录配置、基础 Bundle / VFS 能力
- Lua 启动脚本、实例生命周期、基础热重载状态恢复

### 2D 主线模块
- Sprite 渲染
- 2D Camera
- Box2D 物理
- Animation
- Spine 集成
- Tilemap
- UI（按钮、文本、布局、Anchor、CanvasScaler、动画）
- Localization（语言切换、字体映射、回退）
- Particle（发射器、曲线、随机参数、最小可用碰撞语义）
- CPU / Memory / Render Profiler

> 当前更准确的表述是：**2D 主线功能已基本完整，已具备小型到中小体量项目的主链开发条件；仍未完成的主要是更高阶工程化能力，而不是主链功能缺失。**

### Editor 当前能力
- 基础 DockSpace 编辑器框架
- Hierarchy / Inspector / Scene / Game / Profiler 面板
- 基础实体创建、删除、选中
- 基础 Transform 编辑
- 场景保存 / 加载
- ImGuizmo 基础操作
- Profiler 曲线与导出入口

### 测试
- CTest 统一入口
- `engine.unit`
- `engine.lua_runtime`
- `engine.spine`
- 若干 2D smoke 测试标签
- 部分 3D 组件与冒烟测试入口

## 当前边界

以下能力**不应**被表述为当前已经稳定完成：

- 完整 3D 商用品质闭环
- 完整 Prefab 工作流
- 完整 Undo / Redo 指令体系
- 完整 Play In Editor 隔离
- 完整资源数据库与 UUID / meta 体系
- 完整性能基线平台与 CI 性能门禁

## 3D 当前状态

3D 相关代码、组件、样例和测试入口已经存在，但当前状态更准确地应描述为：

- **已接入**：组件层、部分渲染路径、编辑器部分检视能力、部分测试入口
- **未收口**：默认构建、资源工作流、编辑器闭环、稳定性门禁、最小 MVP 验证流程

因此：

- 3D **不是**当前默认稳定主线
- 3D 当前更适合作为 **MVP 收口中的能力方向**，而不是对外承诺的成熟产品能力

## 快速开始（Windows）

### 1. 构建 Runtime

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build_vs2022 --config Debug --target dse_engine
cmake --build build_vs2022 --config Debug --target DSEngine_c++ DSEngine_lua
```

### 2. 运行 Lua Host

```bash
bin\DSEngine_lua_debug.exe
```

### 3. 开启测试

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_ENGINE_TESTS=ON
cmake --build build_vs2022 --config Debug --target dse_engine_unit_tests dse_lua_runtime_tests dse_spine_tests
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L engine
```

### 4. 构建编辑器

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=ON
cmake --build build_vs2022 --config Debug --target dse_editor_cpp
bin\dsengine-editor.exe
```

## 推荐阅读顺序

- `doc/DOC-00_INDEX.md`
- `doc/DOC-01_ARCHITECTURE.md`
- `doc/DOC-02_BUILD_AND_RUN.md`
- `doc/DOC-03_EDITOR.md`
- `doc/DOC-04_TESTING.md`
- `doc/DOC-07_ROADMAP.md`

专题文档按需阅读：

- `doc/DOC-05_UI_LOCALIZATION.md`
- `doc/DOC-06_PARTICLE_PROFILER.md`

## 后续方向

DSEngine 后续不会继续追求“功能面越铺越宽”，而会坚持以下路线：

1. 先把 **2D 主线**做成更稳的可交付闭环
2. 再把 **Editor** 做成更高频可用的开发工具
3. 再把 **资源链路、测试门禁、性能基线**做扎实
4. 最后把 **3D** 从“已接入”推进到“最小可验证 MVP”

## 文档索引

完整文档入口见：`doc/DOC-00_INDEX.md`

## 当前代码架构评估（2026-04）

以下评估基于当前仓库目录、构建脚本、核心运行时、编辑器入口、模块实现、测试组织与现有文档的交叉审阅，重点关注：模块划分、目录结构、依赖方向是否合理。

### 简明结论

当前项目的代码架构**整体方向是合理的**。

在仓库层面，[`apps/`](apps/) 、[`engine/`](engine/) 、[`modules/`](modules/) 、[`tests/`](tests/) 的分层是清楚的；在产品定位层面，文档与实现对“2D 为当前稳定主线、3D 已接入但未成为默认稳定主线”的表述基本一致。

但从代码实现细节来看，项目仍处于“主线已建立，但尚未完全收口”的阶段。当前最大的风险不在顶层目录结构，而在：

- 运行时总控偏重
- 编辑器应用壳偏重
- 单例与实例化并存
- 构建 target 边界还比较粗

因此可以把当前架构状态概括为：

- **仓库级分层：合理**
- **模块级依赖方向：基本合理，但存在集中耦合点**
- **运行时与编辑器实现：具备可演进基础，但技术债明显**

### 优点

#### 1. 顶层目录分层清晰

当前仓库已经形成较稳定的职责分区：

- [`apps/`](apps/)：应用入口层
- [`engine/`](engine/)：引擎核心与基础设施
- [`modules/`](modules/)：玩法与功能模块层
- [`tests/`](tests/)：测试与回归验证
- [`doc/`](doc/)：架构、构建、测试与路线图文档

这种分层方式是合理的，说明项目已经具备进一步工程化治理的基础。

#### 2. 文档定位与实际代码基本一致

[`README.md`](README.md) 与 [`doc/DOC-01_ARCHITECTURE.md`](doc/DOC-01_ARCHITECTURE.md:1) 对项目当前状态的描述比较克制，明确指出：

- 2D Runtime 是当前稳定主线
- 支持 Lua / C++ 双业务宿主
- 编辑器基础可用，但仍需继续模块化
- 3D 已接入，但不应视为当前默认稳定主线

这种“文档口径与实际能力基本对齐”的状态本身就是架构治理成熟度的体现。

#### 3. 模块层总体遵守 `modules -> engine` 的依赖方向

抽样查看 [`modules/gameplay_2d/tilemap/tilemap_system.cpp`](modules/gameplay_2d/tilemap/tilemap_system.cpp:6) 与 [`modules/gameplay_2d/ui/ui_system.cpp`](modules/gameplay_2d/ui/ui_system.cpp:6)，当前模块主要依赖：

- ECS 组件与 World
- 资源、物理、事件总线等 engine 基础设施
- 少量同层功能模块，例如本地化

整体上没有看到明显的 `engine -> apps` 反向依赖，这说明主依赖方向是健康的。

#### 4. Runtime 已开始从全局态向实例化注入演进

[`engine/runtime/engine_app.cpp`](engine/runtime/engine_app.cpp:45) 中的 `EngineInstance` 会显式注入 `World` 和 `AssetManager`，再创建 [`FramePipeline`](engine/runtime/frame_pipeline.h:42)。这说明项目已经开始从“单纯依赖全局单例”向“运行时实例化上下文”过渡。

这个方向是正确的，而且对后续做编辑器集成、测试隔离、模块装配优化都非常重要。

#### 5. 测试体系已经覆盖到系统行为层

从 [`tests/engine/CMakeLists.txt`](tests/engine/CMakeLists.txt:1) 可以看出，测试不仅覆盖工具层，还覆盖了：

- Runtime 访问与静态回归，如 [`tests/engine/runtime/frame_pipeline_static_regression_test.cpp`](tests/engine/runtime/frame_pipeline_static_regression_test.cpp:30)
- Lua Runtime 生命周期与资源注入，如 [`tests/engine/runtime_smoke_snapshot_test.cpp`](tests/engine/runtime_smoke_snapshot_test.cpp:99)
- 2D UI / Tilemap / Physics2D / Particle / Spine 等模块 smoke/snapshot
- 部分 3D 模块单测与 smoke 入口

这说明项目虽然仍有耦合，但已经具备“用测试守住主线行为”的意识与基础设施。

### 问题

#### 1. [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 职责过重

从 [`engine/runtime/frame_pipeline.h`](engine/runtime/frame_pipeline.h:14) 和 [`engine/runtime/frame_pipeline.cpp`](engine/runtime/frame_pipeline.cpp:64) 看，`FramePipeline` 当前同时负责：

- 持有并初始化多个 2D / 3D 系统
- 管理 RHI 设备与多个 Render Target
- 处理 Lua / C++ bootstrap
- 负责动态模块加载
- 触发场景回归检查
- 构建与执行 Render Graph
- 广播生命周期事件

这意味着它已经不仅是“帧调度器”，而是接近运行时总控对象。

这种设计短期推进效率高，但长期会导致：

- 新模块持续向中心堆积
- 调用顺序与依赖关系越来越隐式
- 架构扩展必须先改一个巨型中心文件
- 真正的模块装配与裁剪会越来越困难

#### 2. 2D 与 3D 的模块化策略不完全一致

当前代码中，3D 一方面走动态模块加载路径，见 [`engine/runtime/frame_pipeline.cpp`](engine/runtime/frame_pipeline.cpp:172)；另一方面 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 头文件里又直接包含 3D 相关头并持有 [`mesh_render_system_`](engine/runtime/frame_pipeline.h:183)。

同时顶层 [`CMakeLists.txt`](CMakeLists.txt:133) 在关闭 [`DSE_ENABLE_3D`](CMakeLists.txt:19) 时，仍会把部分 3D 源加入 `engine_cpp`。

这会让以下边界不够清楚：

- 什么属于内建核心
- 什么属于编译期开关模块
- 什么属于运行时可插拔模块

这种“部分内建、部分插件化”的混合状态，是当前模块边界最明显的不稳定点之一。

#### 3. 单例与实例化并存，运行时语义不够统一

虽然已经有 [`EngineInstance`](engine/runtime/engine_app.cpp:45) 这类实例化运行时外壳，但仓库内仍存在多处全局入口：

- [`FramePipeline::Instance()`](engine/runtime/frame_pipeline.cpp:59)
- [`World::Instance()`](engine/ecs/world.cpp:9)
- [`EventBus::Instance()`](engine/core/event_bus.h:121)
- [`LocalizationSystem::GetInstance()`](modules/gameplay_2d/ui/ui_system.cpp:141)

这说明项目还没有完全建立统一的“上下文注入优先”规则。

长期风险包括：

- 编辑器、运行时、测试环境共享隐式状态
- 多实例 world / preview / PIE 隔离难做
- 新代码难以判断应依赖上下文对象还是直接调用单例

#### 4. 编辑器应用层过重，`[apps/editor_cpp/src/main.cpp](apps/editor_cpp/src/main.cpp:25)` 集中过多职责

[`apps/editor_cpp/src/main.cpp`](apps/editor_cpp/src/main.cpp:25) 直接依赖运行时、ECS 组件、3D 模块、Profiler、本地化、ImGui / ImGuizmo 与多个面板实现，并承载了较多应用装配逻辑。

这说明编辑器已经形成可工作的主程序壳，但还没有把：

- 生命周期控制
- 运行时桥接
- 编辑器状态管理
- 面板视图层

这些职责进一步拆开。

结果是编辑器后续功能越多，主文件越容易成为新的中心耦合点。

#### 5. 构建 target 边界偏粗，`[dse_engine](CMakeLists.txt:184)` 体量过大

[`CMakeLists.txt`](CMakeLists.txt:116) 当前通过 `file(GLOB_RECURSE ...)` 收集大批 [`engine/*.cpp`](CMakeLists.txt:117) 与 [`modules/gameplay_2d/*.cpp`](CMakeLists.txt:118) 并统一构建为 [`dse_engine`](CMakeLists.txt:184)。

这种组织方式的问题在于：

- 目录层次是清楚的，但 target 边界不够清楚
- 子系统的物理隔离更多依赖约定，而不是链接边界
- 测试和应用层更容易直接耦合整个引擎大库
- 后续做模块裁剪、增量构建、独立发布时成本会上升

#### 6. 测试结构健康，但也反向暴露出模块 target 粒度不足

[`tests/engine/CMakeLists.txt`](tests/engine/CMakeLists.txt:3) 将大量 `tests/engine/*`、`tests/modules/*` 与实现源码显式汇总到统一测试目标中。

这说明当前测试覆盖是好的，但也说明：

- 模块化测试更多依赖“大一统测试目标”
- 模块 target 本身还不足以支撑更自然的按边界验证

可以理解为：测试在帮助架构兜底，但模块物理边界仍需进一步强化。

### 建议

#### 1. 先削薄 [`FramePipeline`](engine/runtime/frame_pipeline.h:42)

建议把当前运行时总控拆分为更明确的几层，例如：

- `RuntimeContext`：持有 world、asset manager、rhi、config
- `SystemScheduler`：负责 update / fixed update / render 阶段调度
- `BuiltInModuleAssembler`：决定当前运行时装配哪些系统与模块

这样可以把“模块选择”“生命周期管理”“图形资源初始化”“脚本 bootstrap”“帧执行调度”拆开。

#### 2. 明确 2D 核心、3D 模块 与插件化边界

建议尽快统一以下规则：

- 哪些能力属于 runtime 内建主链
- 哪些能力属于编译期开关模块
- 哪些能力属于运行时动态加载模块

当前最适合的收口方式是：

- 把 2D 主线定义为当前内建核心集合
- 把 3D 明确收敛成独立模块集合
- 避免出现“声明上是插件，代码上仍直接嵌进核心类”的混合状态

#### 3. 逐步收敛单例，建立“上下文注入优先”的规则

不必一次性移除所有单例，但建议从 runtime / gameplay 主链开始：

- 新代码优先依赖显式上下文对象
- 旧单例逐步退到兼容层
- 把 `World`、`FramePipeline`、本地化等能力逐步从全局入口迁移到实例上下文

这样可以先提升语义一致性，再逐步降低历史负担。

#### 4. 把编辑器主程序拆成更清晰的应用壳层

建议后续把 [`apps/editor_cpp/src/main.cpp`](apps/editor_cpp/src/main.cpp:25) 进一步拆分为：

- `EditorApp`
- `EditorRuntimeBridge`
- `EditorSceneService`
- `EditorSelectionService`
- `EditorPlayModeService`
- 仅负责渲染与交互的 panel/view 层

这样能够显著减少主程序继续膨胀的风险。

#### 5. 让 CMake target 分层逐步贴近真实模块边界

后续可以逐步考虑把当前大库演进为更细粒度 target，例如：

- `dse_engine_core`
- `dse_engine_runtime`
- `dse_module_gameplay2d`
- `dse_module_gameplay3d`
- `dse_editor_framework`

即使不能一次性完成，也应避免新能力继续无差别合并进单一超级库。

#### 6. 让测试结构逐步跟随模块边界收敛

当前的 smoke / snapshot 测试应继续保留，因为它们已经是主线回归的重要护栏。下一步更推荐：

- 模块测试优先链接模块 target
- runtime 测试优先验证装配契约与上下文边界
- 静态回归测试只保留关键防回退点

这样测试不仅能“发现问题”，还能反向固化架构边界。

### 最终判断

如果只回答“这个项目的代码架构是否合理”，当前更准确的判断是：

- **整体合理，但仍未达到成熟收口态**
- **仓库分层正确，主依赖方向基本健康**
- **主要技术债集中在运行时总控、编辑器装配、单例残留与构建边界粗粒度**

因此，这不是一个需要推倒重来的架构，而是一个**骨架已经正确、接下来应继续做职责拆分与边界收敛**的项目。

### 建议治理顺序

1. 先拆薄 [`FramePipeline`](engine/runtime/frame_pipeline.h:42)
2. 再明确 2D / 3D 的模块装配与插件化边界
3. 再拆编辑器主程序壳层，降低 [`apps/editor_cpp/src/main.cpp`](apps/editor_cpp/src/main.cpp:25) 复杂度
4. 之后逐步收敛单例与上下文边界
5. 最后推进 CMake target 与测试目标的真实模块化

按这个顺序治理，收益会更集中，也更容易避免大范围返工。
