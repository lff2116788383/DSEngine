# DOC-14 当前架构优化方案

本文档基于当前仓库的实际代码结构、构建方式、测试组织和主线文档，对现阶段 DSEngine 的架构优化方向给出一个**渐进式、可落地**的方案。

本文档目标不是推翻现有实现，更不是为了追求“看起来更先进”的抽象，而是围绕当前真实问题做收口：

- 降低运行时总控耦合
- 推进实例化上下文
- 稳定 2D 主线并隔离 3D 收口路径
- 让 Editor / Runtime / Tests 共用一致的装配边界

相关背景文档建议结合阅读：

- [`README.md`](README.md)
- [`doc/DOC-01_ARCHITECTURE.md`](doc/DOC-01_ARCHITECTURE.md)
- [`doc/DOC-07_ROADMAP.md`](doc/DOC-07_ROADMAP.md)
- [`doc/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md`](doc/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md)

---

## 1. 当前架构问题概括

结合当前代码，可以把主要问题归纳为以下几类。

### 1.1 运行时总控偏重

当前 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 同时承担了过多职责，包括：

- 2D 系统实例持有与调度
- 3D 模块生命周期挂接
- [`RhiDevice`](engine/render/rhi/rhi_device.h) 管理
- Render Target 与 Pipeline State 管理
- Render Graph Pass 构建与执行
- Lua / C++ 业务模式切换
- 性能统计与窗口标题更新

这导致 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 更像一个“超级对象”，而不是一个清晰的编排层。

### 1.2 单例与实例化并存

当前项目已经开始向实例化方向演进，例如 [`EngineRunConfig`](engine/runtime/engine_app.h:16) 与 [`EngineInstance`](engine/runtime/engine_app.h:31) 已经提供了注入式运行壳。

但同时，类似 [`World::Instance()`](engine/ecs/world.h:29) 这样的全局接口仍然存在，说明项目还处于：

- 新路径开始实例化
- 旧路径仍依赖全局态
- 两种模式并存

这种状态本身并不错误，但如果长期不收口，会不断增加维护成本。

### 1.3 2D 主线和 3D 能力线的装配边界仍不够明确

从构建配置 [`CMakeLists.txt`](CMakeLists.txt) 和 [`modules/gameplay_3d/gameplay_3d_module.h`](modules/gameplay_3d/gameplay_3d_module.h:22) 可以看出：

- 2D 是当前默认开启的稳定主线
- 3D 是默认关闭、按需启用的能力线
- 3D 正在往模块化装配推进

但当前边界还没有完全收紧，导致部分 3D 逻辑仍会影响总控设计。

### 1.4 编辑器已可用，但工程化边界仍需加强

文档已经明确编辑器当前处于“基础可用，但仍需模块化”的阶段。这个问题的核心不是“功能没有”，而是：

- 编辑器壳层和引擎运行层边界还不够清楚
- 高频面板和状态管理需要拆分
- 后续 Play/Edit 隔离、场景编辑稳定性、Inspector 扩展性都会受影响

### 1.5 测试体系已建立，但应继续对齐架构层次

当前测试覆盖已经不错，见 [`tests/engine/CMakeLists.txt`](tests/engine/CMakeLists.txt:1)，但下一阶段重点不只是“加更多测试”，而是让测试结构更准确表达架构层次。

---

## 2. 优化目标

建议将当前架构优化目标收敛为以下四项。

### 2.1 降低总控耦合

把 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 从“持有一切、管理一切”的对象，收缩成**薄编排器**。

### 2.2 推进运行时上下文实例化

让 world、asset manager、RHI、脚本 API、平台回调等依赖逐步统一走显式注入，而不是散落在全局单例和调用方隐式约定里。

### 2.3 稳住 2D 主线，隔离 3D 收口路径

继续保持：

- 2D 作为默认稳定主线
- 3D 作为模块化能力线
- 不让 3D 收口过程反复污染 2D 主线边界

### 2.4 统一 Editor / Runtime / Tests 的装配边界

让编辑器、运行时、测试都基于相同的 Runtime 组装方式，而不是分别维护不同的隐式初始化路径。

---

## 3. 推荐目标架构

建议在现有目录基础上，将运行时相关结构继续收敛到下面这个方向：

```text
apps/
  runtime/
  editor_cpp/
  launcher_tauri/

engine/
  core/
  runtime/
    engine_app.*
    runtime_context.*
    runtime_services.*
    system_graph.*
    render_pipeline_resources.*
    business_runtime_bridge.*
  ecs/
  scene/
  assets/
  scripting/
  render/
  physics/

modules/
  gameplay_2d/
  gameplay_3d/
```

这里最关键的新增概念有五个：

- [`EngineInstance`](engine/runtime/engine_app.h:31) 继续保留，作为运行时生命周期外壳
- `RuntimeContext`：统一共享上下文
- `RuntimeServices`：统一依赖注入集合
- `SystemGraph`：统一系统装配与阶段调度
- `RenderPipelineResources`：统一 GPU 资源持有
- `BusinessRuntimeBridge`：统一 Lua / C++ 宿主桥接

这些名字当前不一定全部存在，但它们是最合理的重构承接点。

---

## 4. 分模块优化方案

## 4.1 运行时总控拆分

当前 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 的问题不是“代码坏掉了”，而是职责密度过高。建议拆分为以下几个协作对象。

### 4.1.1 `RuntimeContext`

职责：保存运行时共享上下文，不负责复杂生命周期逻辑。

建议包含：

- [`World*`](engine/ecs/world.h:23)
- [`AssetManager*`](engine/assets/asset_manager.h)
- [`RhiDevice*`](engine/render/rhi/rhi_device.h)
- business mode
- editor mode
- window title setter
- profiler / stats 读取接口
- 可能的 filesystem / vfs 接口

优化价值：

- Lua、模块、Editor、测试都可以基于统一上下文取依赖
- 避免各系统直接依赖整个 [`FramePipeline`](engine/runtime/frame_pipeline.h:42)
- 为后续去单例化提供基础承载对象

### 4.1.2 `RuntimeServices`

职责：区分“配置参数”和“运行依赖”。

建议把当前 [`EngineRunConfig`](engine/runtime/engine_app.h:16) 中与运行依赖有关的部分逐步迁出，例如：

- [`World*`](engine/ecs/world.h:23)
- [`AssetManager*`](engine/assets/asset_manager.h)
- 平台回调
- 可选 editor hooks
- logger / filesystem 等后续服务

而 [`EngineRunConfig`](engine/runtime/engine_app.h:16) 尽量只保留启动配置：

- 窗口宽高
- 标题
- business mode
- 是否启用 editor
- 启动脚本路径
- feature flags

### 4.1.3 `SystemGraph`

职责：只负责系统装配与分阶段调度。

它只关心：

- 初始化顺序
- Update 阶段调用顺序
- FixedUpdate 阶段调用顺序
- Render 阶段调用顺序
- 模块生命周期钩子执行顺序

它不应该直接知道：

- 窗口标题逻辑
- 启动脚本路径
- 具体 GPU 资源分配细节

优化价值：

- 把系统“如何执行”从“谁持有上下文”里分离出来
- 后续为 2D 稳定链、3D 模块链、测试链分别组织更容易

### 4.1.4 `RenderPipelineResources`

职责：集中持有 GPU 相关对象。

建议把当前 [`FramePipeline`](engine/runtime/frame_pipeline.h:208) 到 [`FramePipeline`](engine/runtime/frame_pipeline.h:223) 这类状态迁出，包括：

- scene/main/ui/prez render target
- bloom mip RT
- shadow RT
- pipeline state
- scene texture / main texture 查询所需句柄

优化价值：

- 把“调度层”和“渲染资源层”分开
- 降低 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 横向复杂度
- 为后续 editor texture 输出与 render graph 清理边界

### 4.1.5 `BusinessRuntimeBridge`

职责：统一 Lua / C++ 双宿主生命周期桥接。

建议抽出以下职责：

- bootstrap
- tick
- shutdown
- startup script path 传入
- Lua API context 配置

优化价值：

- 避免业务宿主切换逻辑散落在总控中
- 使 Lua / C++ 宿主成为“可替换桥接层”而不是总控分支

### 4.1.6 最终形态：`FramePipeline` 退化为薄编排器

最终 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 可以保留为兼容外观层，对外仍提供：

- [`Init()`](engine/runtime/frame_pipeline.h:53)
- [`Update()`](engine/runtime/frame_pipeline.h:61)
- [`FixedUpdate()`](engine/runtime/frame_pipeline.h:67)
- [`Render()`](engine/runtime/frame_pipeline.h:72)
- [`Shutdown()`](engine/runtime/frame_pipeline.h:55)

但内部不再直接堆积所有系统、GPU 资源和业务桥接状态。

---

## 4.2 推进实例化上下文，逐步淘汰全局态主路径

当前像 [`World::Instance()`](engine/ecs/world.h:29) 这样的全局接口，不建议直接硬删。更现实的方案是把它们收缩为兼容层。

### 4.2.1 第一阶段：保留旧接口，冻结新用法

规则建议如下：

- 老代码允许继续调用 [`World::Instance()`](engine/ecs/world.h:29)
- 新增 runtime、editor、module 代码不再新增对单例接口的依赖
- 新文档统一以 context / services 注入路径为准

### 4.2.2 第二阶段：优先替换高价值路径

优先收口如下路径：

- Lua 绑定层获取 world 的方式
- 资源系统中高频全局访问路径
- 编辑器对全局 world / 资源系统的直接访问
- 测试中依赖单例导致互相污染的初始化路径

### 4.2.3 第三阶段：将单例标为兼容 API

最终让：

- [`World::Instance()`](engine/ecs/world.h:29)
- `AssetManager::Instance()`

只作为历史兼容入口存在，而不再成为主文档与新代码推荐方式。

---

## 4.3 2D 与 3D 采用双轨装配策略

当前最合理的方向不是强行把 2D 和 3D 立即统一成一套完全对称架构，而是保持主次明确。

### 4.3.1 2D：继续作为内建稳定主线

当前 2D 已经具备较高成熟度，建议继续以内建系统方式参与 [`SystemGraph`](engine/runtime/system_graph.h) 调度，例如：

- Camera
- Sprite
- UI
- Tilemap
- Particle
- Animation
- Localization
- Physics2D
- Spine

目标是：

- 调用顺序稳定
- 回归链路稳定
- 工具链与编辑器体验稳定

### 4.3.2 3D：彻底转向模块化接入

当前 [`Gameplay3DModule`](modules/gameplay_3d/gameplay_3d_module.h:22) 已经说明 3D 正在往 [`IModule`](engine/core/module.h) 方向走，这是正确路线。

建议继续推进为：

- 3D 系统不再由 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 直接持有
- 3D 生命周期统一走 [`IModule`](engine/core/module.h) 接口
- 3D 专有 camera/light/shadow 配置优先在 3D 模块内部收口
- 3D 专有 smoke / MVP 资源路径不污染 2D 主线默认路径

这样可以保证：

- 2D 主线继续稳定
- 3D 能独立推进 MVP
- 新 3D 需求不会反复把总控做得更重

---

## 4.4 Editor 侧优化方案

根据当前路线，编辑器应被视为“应用壳 + 工具模块集合”，而不是引擎总控的一部分。

### 4.4.1 目标原则

建议明确以下原则：

- [`apps/editor_cpp/`](apps/editor_cpp/) 只负责 editor app 层
- 引擎生命周期仍由 [`EngineInstance`](engine/runtime/engine_app.h:31) 或 RuntimeContext 提供
- 编辑器不应直接依赖 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 的内部实现细节
- Panel、状态、序列化逻辑应拆为独立模块

### 4.4.2 建议拆分方向

建议至少拆出以下模块：

- `editor_state`
- `scene_io`
- `hierarchy_panel`
- `inspector_panel`
- `viewport_panel`
- `profiler_panel`
- `selection_service`
- `gizmo_controller`

如果后续需要 Play/Edit 隔离，也更适合围绕 editor state 与 runtime instance 做边界控制，而不是继续在一个主文件里堆逻辑。

### 4.4.3 Editor 与 Runtime 的交互建议

建议统一通过以下方式交互：

- scene texture / main texture 查询
- world / scene 的受控访问接口
- runtime tick 控制
- profiler stats 读取接口
- scene save/load service

避免编辑器直接操作运行时内部大量细节对象。

---

## 4.5 资源系统优化方向

资源路径与资源访问方式应继续作为架构规则收口。

### 4.5.1 逻辑路径优先

建议强制统一：

- 业务层一律使用逻辑路径，如 `models/cube.dmesh`、`script/main.lua`
- 禁止新代码继续写 `data/...` 物理路径
- Runtime / Lua / Editor / 3D 模块统一走同一套路径解析规则

### 4.5.2 资源访问显式注入

建议新代码一律通过注入的 [`AssetManager`](engine/assets/asset_manager.h) 访问资源，而不是继续依赖全局静态访问路径。

### 4.5.3 为后续工程化预留，但不提前扩张

在不扰乱当前主线的前提下预留结构即可，包括：

- meta
- uuid
- asset dependency
- asset database

这些能力当前不建议抢占比 2D 主线稳定更高的优先级。

---

## 4.6 测试与构建的对齐方案

当前测试体系已经具备不错基础。下一步应让测试结构与架构边界同步表达。

### 4.6.1 推荐测试分层

建议按以下方式理解和逐步整理测试集合：

#### A. core/unit

适合覆盖：

- ECS
- Scene
- EventBus
- JobSystem
- MemoryPool / ObjectPool
- Input / Time / Tween

#### B. runtime/integration

适合覆盖：

- [`EngineInstance`](engine/runtime/engine_app.h:31)
- [`FramePipeline`](engine/runtime/frame_pipeline.h:42)
- RuntimeContext / RuntimeServices
- Lua / C++ bootstrap
- 资源注入与生命周期

#### C. module/smoke

适合覆盖：

- UI
- Tilemap
- Particle
- Localization
- Physics2D
- 3D smoke / 3D MVP 最小链路

#### D. editor/smoke

适合覆盖：

- 场景保存 / 加载
- 实体创建 / 删除 / 选中
- Inspector 编辑
- Gizmo / Transform 流程

### 4.6.2 构建系统对齐建议

建议逐步让构建目标表达更清晰的边界：

- `dse_engine`：核心引擎
- `dse_runtime_host_*`：宿主入口
- `dse_editor_cpp`：编辑器壳
- `dse_gameplay3d`：3D 模块
- `dse_engine_unit_tests`：基础测试目标
- `dse_runtime_tests` / `dse_editor_tests` / `dse_3d_smoke_tests`：按层次组织

当前不一定需要一次性重命名，但新目标命名建议与架构边界一致。

---

## 5. 建议实施顺序

为了降低风险，建议按阶段推进，而不是一次性大改。

## 5.1 阶段 A：低风险边界收口

目标：不改主行为，只建立未来迁移承接点。

建议动作：

- 新增 `runtime_context.*`
- 新增 `runtime_services.*`
- [`EngineInstance`](engine/runtime/engine_app.h:31) 内统一上下文组装
- 为 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 增加 context 注入路径
- 约束新代码不再增加单例依赖

### 预期收益

- 不破坏主线行为
- 为后续拆总控提供合法承接层
- 使新代码有统一依赖获取方式

---

## 5.2 阶段 B：拆分运行时总控

目标：让 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 瘦身。

建议动作：

- 拆出 `render_pipeline_resources.*`
- 拆出 `business_runtime_bridge.*`
- 拆出 `system_graph.*`
- 保持 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 对外 API 稳定，内部逐步迁移

### 预期收益

- 主循环总控复杂度显著下降
- 2D / 3D / Lua / Render 的边界更清楚
- Editor 和 Tests 更容易复用运行时结构

---

## 5.3 阶段 C：Editor 工程化拆分

目标：把编辑器从“单点可用”推进到“模块化可维护”。

建议动作：

- 拆分主入口
- 引入 editor_state
- 独立 scene_io / hierarchy / inspector / viewport / profiler 模块
- 为高频编辑路径补最小 smoke 测试

### 预期收益

- 降低编辑器维护风险
- 为 Play/Edit 隔离打基础
- 降低后续功能迭代时的回归成本

---

## 5.4 阶段 D：3D MVP 收口

目标：不干扰 2D 主线的前提下完成 3D MVP。

建议动作：

- 3D 生命周期统一走模块接口
- 固化最小 3D smoke 场景
- 统一 3D 逻辑路径资源加载
- 建立 `engine.3d.*` 分组门禁

### 预期收益

- 3D 可以独立演进
- 文档、测试、构建与真实状态一致
- 避免“已有代码”被误表述成“已完成工作流” 

---

## 6. 最小可执行优化包

如果当前只做最小改动，建议优先完成以下五项：

1. 新增 `RuntimeContext`
2. 让 Lua runtime、AssetManager、Editor 通过 context 获取依赖
3. 把 GPU 资源字段从 [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 拆到 `RenderPipelineResources`
4. 停止在新代码中使用 [`World::Instance()`](engine/ecs/world.h:29)
5. 把 3D 接入路径统一收敛到 [`IModule`](engine/core/module.h) 生命周期下

这是当前性价比最高的一组动作，能显著提高可维护性，同时不需要推翻现有主线。

---

## 7. 风险控制建议

在执行优化过程中，建议明确以下约束。

### 7.1 不做一次性大爆炸重构

保持：

- 外部 API 尽量稳定
- 兼容旧路径
- 小步迁移 + 测试回归验证

### 7.2 不在 2D 主线未稳时扩张更多 3D 子系统

当前 3D 的重点应是：

- 收最小闭环
- 建可验证路径
- 建 smoke / MVP 门禁

而不是继续无边界扩展功能面。

### 7.3 文档必须跟着架构阶段同步更新

每完成一个关键阶段，建议同步更新：

- [`README.md`](README.md)
- [`doc/DOC-01_ARCHITECTURE.md`](doc/DOC-01_ARCHITECTURE.md)
- [`doc/DOC-07_ROADMAP.md`](doc/DOC-07_ROADMAP.md)
- [`doc/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md`](doc/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md)

防止“实现已经变了，文档口径还停留在旧阶段”。

---

## 8. 结论

当前 DSEngine 的优化方向不应是“重写引擎”，而应是：

- 围绕运行时上下文统一依赖注入
- 围绕系统装配层拆解 [`FramePipeline`](engine/runtime/frame_pipeline.h:42)
- 围绕应用壳原则整理 Editor
- 围绕模块接口隔离 3D 收口路径
- 围绕测试与构建进一步对齐新边界

最核心的原则可以概括为一句话：

**让运行时总控变薄，让上下文注入变清晰，让 2D 主线更稳定，让 3D 作为独立模块线逐步完成 MVP 收口。**

如果按优先级排序，建议固定为：

1. [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 瘦身
2. RuntimeContext / RuntimeServices 建立
3. Editor 拆分
4. 资源系统去全局化
5. 3D MVP 模块化收口
6. 测试门禁与构建目标继续对齐
