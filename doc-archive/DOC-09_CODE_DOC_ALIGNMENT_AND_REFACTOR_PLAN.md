# DOC-09 代码与文档对齐检查及架构拆分建议

本文档基于当前仓库代码、测试与现有主线文档的交叉检查结果，目的有两个：

1. 判断现有文档口径与实际代码进度是否一致
2. 给出接下来更适合当前阶段的架构调整与拆分计划

本文档只描述当前仓库里已经能从代码和测试中确认的事实，以及基于这些事实的工程化建议，不把远期目标写成已完成事实。

---

## 1. 检查范围

本次对齐检查重点参考了以下文档：

- [`doc-archive/DOC-01_ARCHITECTURE.md`](doc-archive/DOC-01_ARCHITECTURE.md)
- [`doc-archive/DOC-03_EDITOR.md`](doc-archive/DOC-03_EDITOR.md)
- [`doc-archive/DOC-07_ROADMAP.md`](doc-archive/DOC-07_ROADMAP.md)
- [`doc-archive/DOC-08_3D_ROADMAP.md`](doc-archive/DOC-08_3D_ROADMAP.md)

并结合以下代码与测试进行交叉判断：

- [`engine/runtime/engine_app.cpp`](engine/runtime/engine_app.cpp)
- [`engine/runtime/frame_pipeline.cpp`](engine/runtime/frame_pipeline.cpp)
- [`engine/ecs/world.cpp`](engine/ecs/world.cpp)
- [`engine/assets/asset_manager.cpp`](engine/assets/asset_manager.cpp)
- [`engine/scripting/lua/lua_runtime.cpp`](engine/scripting/lua/lua_runtime.cpp)
- [`engine/scripting/lua/bindings/lua_binding_context.cpp`](engine/scripting/lua/bindings/lua_binding_context.cpp)
- [`engine/render/rhi/rhi_device.h`](engine/render/rhi/rhi_device.h)
- [`engine/render/rhi/rhi_device.cpp`](engine/render/rhi/rhi_device.cpp)
- [`engine/core/module.h`](engine/core/module.h)
- [`modules/gameplay_2d/ui/ui_system.cpp`](modules/gameplay_2d/ui/ui_system.cpp)
- [`apps/editor_cpp/src/main.cpp`](apps/editor_cpp/src/main.cpp)
- [`apps/editor_cpp/CMakeLists.txt`](apps/editor_cpp/CMakeLists.txt)
- [`tests/engine/scripting/lua_runtime_test.cpp`](tests/engine/scripting/lua_runtime_test.cpp)
- [`tests/modules/gameplay_2d/ui/ui_system_test.cpp`](tests/modules/gameplay_2d/ui/ui_system_test.cpp)
- [`tests/engine/scene/prefab_test.cpp`](tests/engine/scene/prefab_test.cpp)
- [`tests/modules/gameplay_3d/rendering/3d_smoke_test.cpp`](tests/modules/gameplay_3d/rendering/3d_smoke_test.cpp)

---

## 2. 总体结论

总体判断：**当前主线文档与代码实际状态大体一致，口径整体是收敛且可信的。**

尤其以下判断与代码高度一致：

- 2D Runtime 是当前更成熟的主线
- Lua / C++ 双宿主已经可用
- Editor 已可用，但工程化不足
- 资源系统已有工程化基础，但仍保留全局态
- 3D 已接入，但不是默认稳定主线
- 当前更适合做收口和拆分，不适合继续无节制扩功能面

不过，仍有几处需要补充或校正文档表述，主要问题不是“文档吹过头”，而是：

- 个别能力在代码中已经比文档写得更细
- 个别边界在代码中暴露得比文档更明显
- 个别工程门禁项在文档里是目标，但代码现状说明还没有完全收紧

因此建议：**保留现有主线文档口径，同时新增一份偏执行层的拆分计划文档。**

本文档即承担这部分作用。

---

## 3. 文档与代码一致的部分

### 3.1 Runtime 主线已形成
文档在 [`doc-archive/DOC-01_ARCHITECTURE.md`](doc-archive/DOC-01_ARCHITECTURE.md) 中将 Runtime 描述为当前主线之一，这与代码一致。

从 [`EngineInstance::Init()`](engine/runtime/engine_app.cpp:62)、[`EngineInstance::Tick()`](engine/runtime/engine_app.cpp:146)、[`EngineInstance::Run()`](engine/runtime/engine_app.cpp:182) 可确认：

- 已有运行时实例外壳
- 已有 editor/runtime 模式切换
- 已有固定步长更新
- 已有 GLFW/OpenGL 初始化闭环

从 [`FramePipeline::Init()`](engine/runtime/frame_pipeline.cpp:63)、[`FramePipeline::Update()`](engine/runtime/frame_pipeline.cpp:298)、[`FramePipeline::FixedUpdate()`](engine/runtime/frame_pipeline.cpp:332)、[`FramePipeline::Render()`](engine/runtime/frame_pipeline.cpp:351) 可确认：

- 帧级系统调度完整存在
- 资源系统、宿主系统、物理、UI、渲染已接入同一主循环
- 运行时统计链路已接入

这说明文档把 Runtime 定位为“已形成主线”是准确的。

### 3.2 2D 主线比 3D 更成熟
文档把 2D 定位为稳定主线，把 3D 定位为“已接入但未收口”，这和代码现状一致。

从 [`modules/gameplay_2d/ui/ui_system.cpp`](modules/gameplay_2d/ui/ui_system.cpp)、测试目录结构、以及 [`tests/modules/gameplay_2d/ui/ui_system_test.cpp`](tests/modules/gameplay_2d/ui/ui_system_test.cpp) 等文件可确认：

- 2D UI/布局/点击/Mask/本地化已经有功能级实现
- 2D 模块测试覆盖更深入
- 2D 主线是实装并带回归保护的

相对地，3D 虽然存在模块、组件、测试和样例，但从 [`engine/core/module.h`](engine/core/module.h)、[`FramePipeline::BuildRenderGraph()`](engine/runtime/frame_pipeline.cpp:430) 以及 [`tests/modules/gameplay_3d/rendering/3d_smoke_test.cpp`](tests/modules/gameplay_3d/rendering/3d_smoke_test.cpp) 看，3D 更像“接入中的体系”，而不是稳定产品线。

### 3.3 Editor 文档口径基本准确
[`doc-archive/DOC-03_EDITOR.md`](doc-archive/DOC-03_EDITOR.md) 将编辑器描述为：

- 当前主线编辑器
- 已进入可用阶段
- 但仍处于工程化收口阶段

这一点与 [`apps/editor_cpp/src/main.cpp`](apps/editor_cpp/src/main.cpp) 的现状高度一致。

尤其搜索结果显示：

- [`g_backup_registry`](apps/editor_cpp/src/main.cpp:129) 已用于 Edit/Play 切换快照
- [`CopyRegistry()`](apps/editor_cpp/src/main.cpp:455) 已承担当前场景快照复制逻辑
- Inspector、Hierarchy、3D 组件检视、Profiler、本地化预览都在同一文件中持续堆积

因此文档关于“可用但集中度过高”的判断是准确的。

### 3.4 资源系统已有工程化基础，但仍保留全局态
[`doc-archive/DOC-01_ARCHITECTURE.md`](doc-archive/DOC-01_ARCHITECTURE.md) 与 [`doc-archive/DOC-07_ROADMAP.md`](doc-archive/DOC-07_ROADMAP.md) 中对资源系统的描述基本与代码一致。

从 [`AssetManager::ConfigureDataRoot()`](engine/assets/asset_manager.cpp:104)、[`AssetManager::PackBundle()`](engine/assets/asset_manager.cpp:169)、[`AssetManager::MountBundle()`](engine/assets/asset_manager.cpp:206)、[`AssetManager::ReleaseGpuResources()`](engine/assets/asset_manager.cpp:631) 可确认：

- data root 管理存在
- bundle 打包/挂载存在
- GPU 资源释放链路存在
- 异步回调统计存在

同时从 [`AssetManager::Instance()`](engine/assets/asset_manager.cpp:94) 也能确认，全局态还在。

### 3.5 3D 文档降级为参考文档是合理的
[`doc-archive/DOC-08_3D_ROADMAP.md`](doc-archive/DOC-08_3D_ROADMAP.md) 不再把 3D 当成主线路线图，而是降级为参考口径，这与代码现状匹配。

从 [`frame_pipeline.cpp`](engine/runtime/frame_pipeline.cpp) 中的 [`ResolveRuntimeModules()`](engine/runtime/frame_pipeline.cpp:40) 和 `#ifdef DSE_ENABLE_3D` 条件接入逻辑可见，3D 当前就是可接入、可试跑，但仍不是默认稳定主线。

---

## 4. 文档落后于代码的部分

这里的“落后”不是错误，而是文档还可以写得更精确。

### 4.1 Lua 运行时的健壮性比文档体现得更完整
文档已经说明 Lua 运行时支持启动脚本、实例生命周期、热重载和内存统计，但代码里其实已经走得更深。

从 [`lua_runtime.cpp`](engine/scripting/lua/lua_runtime.cpp) 和 [`tests/engine/scripting/lua_runtime_test.cpp`](tests/engine/scripting/lua_runtime_test.cpp) 可确认：

- 启动失败清理逻辑存在
- 缺失全局 `Update` 时实体脚本仍可继续更新
- 单个坏脚本不会阻断其他实体脚本更新
- 关闭后 Lua 内存统计能回零

这说明 Lua Runtime 已经进入“稳定性增强阶段”，而不只是“功能可用阶段”。

建议后续在 [`doc-archive/DOC-01_ARCHITECTURE.md`](doc-archive/DOC-01_ARCHITECTURE.md) 或 [`doc-archive/DOC-04_TESTING.md`](doc-archive/DOC-04_TESTING.md) 中补一条：

- Lua Runtime 已有失败恢复与多脚本隔离类回归测试

### 4.2 运行时统计与资源回调观测能力值得在文档中明确
[`FramePipeline::Render()`](engine/runtime/frame_pipeline.cpp:351) 已经统计：

- draw calls
- sprite count
- max batch sprites
- physics bodies
- particle emitters
- 平均 update/fixed/render ms
- pending main-thread callbacks

而 [`AssetManager::PendingMainThreadCallbacks()`](engine/assets/asset_manager.cpp:688) 和 [`AssetManager::PendingMainThreadCallbacksHighWatermark()`](engine/assets/asset_manager.cpp:693) 也已经接入。

文档目前提到了 profiler 和性能方向，但没有强调这部分“运行时内建观测指标”已经存在。这个点比文档体现得更成熟。

### 4.3 Editor 的 3D 检视接入程度比文档稍强
[`doc-archive/DOC-03_EDITOR.md`](doc-archive/DOC-03_EDITOR.md) 里对 3D 编辑能力表述得比较克制，这是对的；但从 [`apps/editor_cpp/src/main.cpp`](apps/editor_cpp/src/main.cpp) 实际搜索结果看，已经能检视和操作较多 3D 组件：

- Mesh Renderer
- Camera 3D
- Directional Light
- Point Light
- Skybox
- Animator 3D
- Terrain
- RigidBody 3D
- Collider 3D
- Particle System 3D
- Post Process

因此文档可适度补充一句：

- 编辑器已接入较多 3D 组件 Inspector，但仍不是完整 3D 编辑工作流

---

## 5. 文档超前于代码或需要收紧的部分

### 5.1 测试门禁口径需要更谨慎
[`doc-archive/DOC-07_ROADMAP.md`](doc-archive/DOC-07_ROADMAP.md) 提到将若干测试作为基础门禁，这个方向是对的，但从代码现状看，门禁还没有完全收紧。

最直接的例子是 [`tests/modules/gameplay_2d/ui/ui_system_test.cpp`](tests/modules/gameplay_2d/ui/ui_system_test.cpp:1) 第一行仍然是 `e#include`，这说明至少存在如下几种可能：

- 该测试文件未稳定纳入默认测试编译路径
- 某些测试目标没有持续全量构建
- 或当前快速构建路径并不会覆盖所有测试文件

因此，现阶段文档里更准确的说法应是：

- 测试入口和目标集已建立
- 但默认门禁与持续化校验仍需进一步收紧

### 5.2 Prefab/Undo/PIE 的边界应继续保持克制
文档目前对这三个方向的表述总体克制，这是正确的。

但从代码看：

- [`tests/engine/scene/prefab_test.cpp`](tests/engine/scene/prefab_test.cpp) 说明 Prefab 已经有一部分底层能力
- [`apps/editor_cpp/src/main.cpp`](apps/editor_cpp/src/main.cpp) 中 Edit/Play 是通过 [`g_backup_registry`](apps/editor_cpp/src/main.cpp:129) 和 [`CopyRegistry()`](apps/editor_cpp/src/main.cpp:455) 做浅层快照隔离
- Undo/Redo 在主入口搜索中几乎没有真正成体系的实现

所以这里不应因为存在底层能力或简化做法，就把文档表述抬高为“已完成工作流”。

当前文档这点基本正确，后续应继续维持保守口径。

### 5.3 3D MVP 前不要把“有很多组件和测试”误写成“已接近完成”
3D 相关目录和测试数量不少，但很多测试仍偏组件默认值、基础数据结构或冒烟级。

这意味着：

- 3D 的“覆盖面”不低
- 但“闭环度”未必高

文档当前没有明显吹高，但后续如果继续补文档，必须坚持：

- 以工作流闭环判断 3D 阶段
- 不能只按模块数量判断成熟度

---

## 6. 当前代码进度的更准确判断

结合代码与文档，我认为当前更准确的项目阶段如下：

### 6.1 引擎基础层
- 已过纯 MVP 搭架阶段
- 已进入稳定化与工程化阶段

### 6.2 2D Runtime
- 已形成主链
- 已具备较明显的可交付雏形
- 当前最适合继续做质量收口

### 6.3 Lua / C++ 双宿主
- 已可用
- Lua Runtime 已进入健壮性增强阶段
- 更像正式子系统，而不是附属能力

### 6.4 Editor
- 已经可用
- 但结构上仍偏集中式
- 现在最需要拆分以避免后续维护成本继续上升

### 6.5 3D
- 已接入较多组件、模块、Inspector 和测试
- 但尚未形成统一工作流闭环
- 正确定位仍应是“待收口能力”

---

## 7. 当前最需要调整的架构问题

### 7.1 [`FramePipeline`](engine/runtime/frame_pipeline.cpp) 过胖
[`FramePipeline`](engine/runtime/frame_pipeline.cpp) 当前承担了过多职责：

- 引擎初始化中的一大部分装配逻辑
- AssetManager 与 RHI 对接
- Lua/C++ 业务 bootstrap
- Update / FixedUpdate / Render 三阶段调度
- Render graph pass 构建
- 模块生命周期分发
- 性能统计
- 一部分场景回归样例触发

这会导致：

- 修改任何一个子系统都容易触碰主调度器
- 初始化和运行时逻辑耦合
- 渲染调度与业务调度难以单独演进

### 7.2 Editor 主入口过大
[`apps/editor_cpp/src/main.cpp`](apps/editor_cpp/src/main.cpp) 当前已经明显承担了过多横切职责：

- 初始化
- 主菜单
- 场景树
- Inspector
- 组件创建
- 3D Inspector
- Profiler 曲线
- 本地化预览
- Gizmo 操作
- Play/Edit 状态切换

这一点不只是不优雅，而是已经直接影响后续可维护性。

### 7.3 依赖注入边界仍处于过渡态
代码中实例化与单例同时存在：

- [`World::Instance()`](engine/ecs/world.cpp:8)
- [`AssetManager::Instance()`](engine/assets/asset_manager.cpp:94)
- [`FramePipeline::Instance()`](engine/runtime/frame_pipeline.cpp:58)
- [`GetAssetManager()`](engine/scripting/lua/bindings/lua_binding_context.cpp:46) 中的单例 fallback

同时 [`EngineInstance`](engine/runtime/engine_app.cpp) 已经在往注入式方向推进。

问题不在于“还有单例”，而在于依赖获取路径不统一，容易造成：

- 测试隔离困难
- 多实例困难
- editor/runtime 共享上下文时边界不清

### 7.4 测试编排与默认构建集还不够紧
测试文件数量很多，这是优点；但从当前抽样可见，默认构建和测试门禁还没完全收口。这个问题不是功能问题，而是工程可靠性问题。

---

## 8. 推荐拆分计划

以下拆分计划坚持一个原则：**不推倒重来，只做受控重构。**

### 8.1 第一优先级：拆 [`apps/editor_cpp/src/main.cpp`](apps/editor_cpp/src/main.cpp)
建议分两阶段拆。

#### 第一阶段：纯物理拆分，不改行为
先把单文件拆成多个实现文件，仅搬代码，不改接口和流程。

建议文件结构：

```text
apps/editor_cpp/src/
  editor_app.cpp
  editor_state.cpp
  editor_scene_io.cpp
  editor_play_mode.cpp
  editor_hierarchy_panel.cpp
  editor_inspector_panel.cpp
  editor_viewport_panel.cpp
  editor_profiler_panel.cpp
  editor_localization_panel.cpp
  editor_gizmo.cpp
```

可保留一个很薄的 [`main.cpp`](apps/editor_cpp/src/main.cpp)，只负责：

- 程序入口
- 初始化调用
- 主循环转发
- 关闭清理

#### 第二阶段：引入上下文对象
在物理拆分完成后，再引入：

- `EditorContext`
- `EditorSelectionModel`
- `EditorPlaySession`
- `EditorSceneDocument`

把目前散落的全局变量逐步收口进去。

### 8.2 第二优先级：把 [`FramePipeline`](engine/runtime/frame_pipeline.cpp) 降级为总协调者
不建议直接删除或重写 [`FramePipeline`](engine/runtime/frame_pipeline.cpp)，而是建议拆子职责对象。

建议目标结构：

```text
engine/runtime/
  runtime_bootstrapper.h/.cpp
  runtime_update_scheduler.h/.cpp
  runtime_render_pipeline.h/.cpp
  runtime_stats_collector.h/.cpp
  frame_pipeline.h/.cpp
```

职责建议：

- `RuntimeBootstrapper`
  - 初始化 RHI
  - 配置 AssetManager
  - 处理业务 bootstrap
  - 处理模块装载

- `RuntimeUpdateScheduler`
  - 执行 [`Update()`](engine/runtime/frame_pipeline.cpp:298)
  - 执行 [`FixedUpdate()`](engine/runtime/frame_pipeline.cpp:332)
  - 统一系统顺序和阶段划分

- `RuntimeRenderPipeline`
  - 创建 render targets
  - 构建 render graph
  - 提交 command buffer

- `RuntimeStatsCollector`
  - 维护 draw calls / timings / callback backlog 等统计

- `FramePipeline`
  - 保留当前外部接口
  - 内部组合上述子对象

### 8.3 第三优先级：统一 Runtime 上下文注入
建议引入轻量的 `RuntimeContext`，把当前散落依赖统一收口。

建议包含：

- `World*`
- `AssetManager*`
- `RhiDevice*`
- `BusinessMode`
- debug/profiler hooks
- editor/runtime mode flags

原则：

1. 新代码不再新增对 [`AssetManager::Instance()`](engine/assets/asset_manager.cpp:94) 和 [`World::Instance()`](engine/ecs/world.cpp:8) 的直接依赖
2. Lua binding 只从上下文读取依赖
3. 保留旧接口作为兼容层，但不再扩散

### 8.4 第四优先级：把系统更新顺序从硬编码过渡到阶段注册
当前 [`FramePipeline::Update()`](engine/runtime/frame_pipeline.cpp:298) 中的手写顺序短期可接受，但扩展性一般。

建议先引入“轻量阶段化注册”，而不是复杂依赖图：

- `PreSimulation`
- `Simulation`
- `PostSimulation`
- `PreRender`
- `UI`
- `LateUpdate`

阶段内仍可保留手工顺序，先解决“接入位置统一”的问题。

### 8.5 第五优先级：测试门禁分层
建议将测试目标分层，而不是把所有测试都当成一个集合。

建议分组：

- `smoke`
  - 最小运行链路
  - 默认构建后快速验证

- `core`
  - `engine.unit`
  - `engine.lua_runtime`
  - `engine.spine`
  - 关键 2D 回归

- `extended`
  - 3D 组件/模块测试
  - Prefab 回归
  - 更慢的场景级回归

同时应优先修复像 [`ui_system_test.cpp`](tests/modules/gameplay_2d/ui/ui_system_test.cpp:1) 这类明显会破坏门禁可信度的问题。

---

## 9. 建议的执行顺序

推荐按如下顺序落地：

### P0：一周内可做
- 修正测试集中明显破坏默认编译的问题
- 明确默认测试必跑集
- 新增本文件作为执行层文档

### P1：短期最高优先级
- 先拆 [`apps/editor_cpp/src/main.cpp`](apps/editor_cpp/src/main.cpp)
- 保持功能不变，只做物理拆分

### P2：中短期
- 拆 [`FramePipeline`](engine/runtime/frame_pipeline.cpp) 子职责
- 建立 `RuntimeBootstrapper` / `RuntimeRenderPipeline` / `RuntimeUpdateScheduler`

### P3：中期
- 引入统一的 `RuntimeContext`
- 停止新增单例 fallback
- 清理 Lua binding 和系统层的全局依赖扩散

### P4：中后期
- 按 3D MVP 闭环重新定义 3D 验收
- 只在闭环形成后再扩大 3D 能力面

---

## 10. 对现有文档的具体建议

### 10.1 [`doc-archive/DOC-01_ARCHITECTURE.md`](doc-archive/DOC-01_ARCHITECTURE.md)
建议补充：

- Lua Runtime 已有失败恢复与脚本隔离回归
- Runtime 已有更完整的性能统计与 callback backlog 观测

### 10.2 [`doc-archive/DOC-03_EDITOR.md`](doc-archive/DOC-03_EDITOR.md)
建议补充：

- 已接入较多 3D 组件 Inspector
- 当前 Play/Edit 切换使用 registry 快照恢复，属于简化方案，不等同于完整 PIE

### 10.3 [`doc-archive/DOC-07_ROADMAP.md`](doc-archive/DOC-07_ROADMAP.md)
建议保持当前主线不变，但把“测试门禁已建立”的语气再收紧一点，更强调“入口已建立，门禁仍需固化”。

### 10.4 [`doc-archive/DOC-08_3D_ROADMAP.md`](doc-archive/DOC-08_3D_ROADMAP.md)
当前表述基本合适，建议不扩大内容，继续保持参考文档定位。

---

## 11. 最终结论

当前仓库的主线文档与代码进度**总体一致**，而且整体口径是偏克制的，这一点是正确的，也应继续保持。

更具体地说：

- [`doc-archive/DOC-01_ARCHITECTURE.md`](doc-archive/DOC-01_ARCHITECTURE.md) 与实际 Runtime / 模块 / 资源 / Lua / Editor 现状基本一致
- [`doc-archive/DOC-03_EDITOR.md`](doc-archive/DOC-03_EDITOR.md) 对编辑器阶段判断基本准确
- [`doc-archive/DOC-07_ROADMAP.md`](doc-archive/DOC-07_ROADMAP.md) 对后续优先级排序是合理的
- [`doc-archive/DOC-08_3D_ROADMAP.md`](doc-archive/DOC-08_3D_ROADMAP.md) 的降级处理符合当前事实

因此，当前最合理的动作不是重写主线文档，而是：

1. 继续保持现有文档口径
2. 用执行层文档承接下一步的拆分与重构计划
3. 按“先 Editor，再 FramePipeline，再依赖注入边界”的顺序推进受控重构

一句话总结：

**文档没有明显脱离代码，真正需要加速的是工程拆分与门禁收口，而不是重新包装项目叙事。**
