# DSEngine 渲染管线现状与可编程化路线

> 更新日期：2026-05-27
> 状态：当前架构评估 / 可编程渲染管线实施路线
> 关联文档：[`GPU_DRIVEN_MODULE_REFACTOR_PLAN.md`](GPU_DRIVEN_MODULE_REFACTOR_PLAN.md)、[`GPU_DRIVEN_UNIFICATION_PLAN.md`](../analysis/GPU_DRIVEN_UNIFICATION_PLAN.md)

---

## 1. 当前结论

DSEngine 当前已经不再是单纯固定 Forward 管线的早期状态。经过 RenderGraph、Clustered Forward+、后处理栈、Probe/IBL、RenderScene 队列和 GPU Driven 收口后，当前更准确的描述是：

```text
FramePipeline
    -> 准备 frame context / render resources / RenderScene / GPU Scene
RenderGraph
    -> 根据 pass 读写关系调度
Builtin Passes
    -> 消费 RenderScene queues + GPU-driven buffers
RHI 后端
    -> OpenGL / Vulkan / D3D11
```

当前已经具备可编程渲染管线的底座：

- **RenderGraph**：已有 pass DAG、资源声明、依赖排序、自动剔除、执行调度。
- **RenderScene**：Gameplay3D 与模块渲染开始从直接被 pass 调用过渡到生成队列供 pass 消费。
- **GPU Driven**：默认 Gameplay3D 场景下已能真实 active，三后端 fresh 视觉验证通过。
- **统一 shader 资产链路**：GLSL 450 源码经工具生成 SPIR-V / GLSL / HLSL。
- **三后端 RHI**：OpenGL / Vulkan / D3D11 主路径均可运行。

但当前仍不是完整的可编程渲染管线。主要缺口是：

```text
已有：RenderGraph 执行引擎
缺少：PipelineProfile / PassRegistry / RenderBlackboard / PipelineValidator / 可配置管线资产
```

因此推荐目标不是马上重写为 Deferred，也不是直接做 Unity SRP 级别的图形化编辑器，而是先将当前固定 `FramePipeline` 抽象为：

```text
PipelineProfile + PassRegistry + RenderBlackboard + RenderScene QueueBuilder
```

---

## 2. 当前渲染管线状态

### 2.1 主渲染路径

当前默认主路径仍是 Forward+：

```text
PreZ
  -> Shadow Passes
  -> GPU Cull / Hi-Z 相关路径
  -> Forward Scene
  -> SSAO / Contact Shadow / Bloom / Composite / FXAA / UI
  -> Present
```

Forward Scene 内部已经不只是传统 CPU draw：

```text
ForwardScenePass
  |-- eligible opaque mesh       -> GPU Driven indirect draw
  |-- non-eligible mesh          -> CPU mesh queue
  |-- terrain / grass / hair     -> 专用 callback/path
  |-- particles                  -> 专用 path
  `-- transparent                -> WBOIT / transparent path
```

### 2.2 已完成能力

| 能力 | 当前状态 | 说明 |
|---|---|---|
| RenderGraph | 已可用 | 负责 pass 依赖、资源声明和执行顺序 |
| Clustered Forward+ | 已完成 | 支持点光/聚光 SSBO 与 cluster 遍历 |
| GPU Driven | 阶段性完成 | Gameplay3D 默认场景下 active，三后端视觉通过 |
| PreZ / Shadow GPU indirect | 已接入 | GPU-driven mesh 可参与 depth-only / shadow pass |
| SSAO / FXAA / TAA | 已接入 | 后处理栈已较完整 |
| Bloom / Auto Exposure / LUT / Film Grain / Vignette | 已接入 | Composite 路径统一处理 |
| CSM 级联过渡 / PCSS / Contact Shadow | 已接入 | 阴影质量已明显提升 |
| Light Probe / Reflection Probe / IBL | 已接入 | 已有运行时 bake / 查询 / Split-Sum IBL |
| OpenGL / Vulkan / D3D11 | 主路径通过 | GPU Driven 1000 实体视觉验证通过 |

### 2.3 GPU Driven 最新验证

2026-05-26 fresh 验证结果：

| 后端 | 日志 | 截图 | 结果 |
|---|---|---|---|
| Vulkan | `tmp/gpu_driven_refactor/vulkan_visual_final.log` | `tmp/gpu_driven_refactor/main1000_vulkan_visual_final.png` | `exit code 0`，无 validation warning，`gpu_driven_active=true` |
| OpenGL | `tmp/gpu_driven_refactor/opengl_visual_final.log` | `tmp/gpu_driven_refactor/main1000_opengl_visual_final.png` | `exit code 0`，`gpu_driven_active=true` |
| DX11 | `tmp/gpu_driven_refactor/dx11_visual_final.log` | `tmp/gpu_driven_refactor/main1000_dx11_visual_final.png` | `exit code 0`，`gpu_driven_active=true` |

共同指标：

```text
gpu_indirect_draws=175
gpu_instances=175
```

### 2.4 Vulkan validation 状态

近期清理已解决：

- `Invalid VkDescriptorSet`
- descriptor binding 类型冲突
- `vkCmdPushConstants` stage flags 不匹配
- empty frame acquire semaphore 未消费
- swapchain image layout 未正确推进

当前 Vulkan GPU Driven 主路径 validation clean。

---

## 3. 当前架构主要问题

### 3.1 FramePipeline 仍硬编码管线

虽然底层有 RenderGraph，但 `FramePipeline` 仍负责直接组织 builtin passes、资源和大量上下文状态。当前结构更像：

```text
固定 C++ pipeline
    -> RenderGraph 只负责执行和依赖调度
```

而不是：

```text
Pipeline Profile / Asset
    -> PassRegistry 创建 pass
    -> RenderGraph 编译执行
```

### 3.2 RenderPassContext 持续膨胀

`RenderPassContext` 承载了 render targets、pipeline states、GPU Driven state、Hi-Z state、module callbacks、editor state、postprocess state 以及 RHI/global state 指针。

长期继续追加字段会导致：

- pass 之间隐式耦合变多。
- 新 pass 需要改公共 context。
- 配置化和可编程化困难。

建议逐步拆成：

```text
RenderBlackboard
  |-- Resource handles
  |-- Frame constants
  |-- Pipeline settings
  |-- Scene / queue references
  `-- Debug / editor settings
```

### 3.3 Pass 对模块仍有残留依赖

当前已开始使用 RenderScene，但部分路径仍依赖 callback 或模块桥接。长期目标应是：

```text
Gameplay / Module / System
    -> RenderScene / RenderObjectRegistry
    -> QueueBuilder
    -> RenderGraph Pass
```

而不是：

```text
RenderPass
    -> 直接调用 Gameplay module 渲染
```

### 3.4 Pass 缺少统一 metadata

真正可编程 pipeline 需要每个 pass 声明：

- pass name
- 输入资源
- 输出资源
- 是否允许关闭
- 执行阶段
- 依赖的队列
- 依赖的 feature
- 参数 schema
- 后端限制

当前这些信息主要散落在 C++ 实现里。

---

## 4. 什么是 DSE 需要的可编程渲染管线

### 4.1 不推荐的方向

不建议让脚本或外部配置直接执行底层渲染命令：

```text
Lua 直接创建 Vulkan descriptor set
Lua 直接绑定 RHI buffer
Lua 直接 DrawIndexed / Dispatch
Lua 直接写资源 layout / barrier
```

这会破坏三后端一致性，尤其 Vulkan 会非常容易触发 layout、descriptor、同步错误。

### 4.2 推荐定义

DSE 的可编程渲染管线应定义为：

```text
用户/项目/模块可以声明 pass 组合、开关、参数和资源依赖；
C++ 引擎负责验证、编译为 RenderGraph，并通过 RHI 执行。
```

也就是：

```text
Lua / JSON / C++ PipelineProfile
    -> PipelineLoader
    -> PipelineValidator
    -> PassRegistry
    -> CompiledRenderPipeline
    -> RenderGraph
    -> RHI
```

### 4.3 分级目标

| 层级 | 能力 | 建议 |
|---|---|---|
| Level 1：可配置管线 | 控制 builtin pass 开关、质量参数、profile 选择 | 立即推进 |
| Level 2：可扩展管线 | 模块/插件注册 C++ pass，由 profile 组合 | 下一阶段推进 |
| Level 3：可资产化管线 | Lua/JSON/编辑器图形化定义 pass graph | 中长期推进 |
| Level 4：完全脚本化渲染 | 脚本直接下发渲染命令 | 不推荐 |

---

## 5. 推荐目标架构

### 5.1 核心对象

```cpp
struct RenderPipelineProfile {
    std::string name;
    std::vector<RenderPipelinePassConfig> passes;
    std::unordered_map<std::string, PipelineValue> settings;
};

struct RenderPipelinePassConfig {
    std::string name;
    bool enabled = true;
    std::unordered_map<std::string, PipelineValue> params;
};

struct RenderPassMetadata {
    std::string name;
    std::vector<std::string> reads;
    std::vector<std::string> writes;
    std::vector<std::string> requires;
    std::vector<std::string> produces;
    bool optional = true;
};
```

### 5.2 关键系统

| 系统 | 职责 |
|---|---|
| `PassRegistry` | 注册 builtin pass 和插件 pass，按名称创建 pass |
| `PipelineProfile` | 描述本项目/本相机/本质量档位启用哪些 pass |
| `PipelineValidator` | 检查 pass 依赖、资源读写、后端限制、必需 pass |
| `PipelineCompiler` | 将 profile 编译成稳定的 C++ 执行对象 |
| `RenderBlackboard` | 存储资源句柄、frame constants、settings、debug 信息 |
| `RenderScene / QueueBuilder` | 统一收集 renderable 并分类为各类队列 |

### 5.3 数据流

```text
World / Modules / ECS
    -> Renderable collection
    -> RenderScene / RenderObjectRegistry
    -> QueueBuilder
    -> PipelineProfile selects passes
    -> PassRegistry creates pass instances
    -> RenderGraph compiles resource dependencies
    -> CommandBuffer / RHI executes
```

---

## 6. Lua 配置 pass 的建议边界

Lua 可以用于配置 pass，但必须是声明式配置，不应直接执行底层渲染。

### 6.1 第一阶段 Lua 格式

```lua
return {
    name = "ForwardPlusDefault",

    settings = {
        gpu_driven = true,
        shadows = true,
        postprocess = true,
    },

    passes = {
        { name = "pre_z", enabled = true },
        { name = "csm_shadow", enabled = true },
        { name = "spot_shadow", enabled = false },
        { name = "point_shadow", enabled = false },
        { name = "gpu_cull", enabled = true },
        { name = "forward_scene", enabled = true },
        { name = "ssao", enabled = true, radius = 1.0 },
        { name = "bloom", enabled = true, intensity = 0.6 },
        { name = "composite", enabled = true },
        { name = "fxaa", enabled = true },
        { name = "ui", enabled = true },
        { name = "present", enabled = true },
    }
}
```

### 6.2 Lua 允许做的事

- 选择 pipeline profile。
- 启用/禁用 builtin pass。
- 设置 pass 参数。
- 根据后端、平台、质量等级选择 profile。
- 配置 debug view / editor view / runtime view。

### 6.3 Lua 禁止做的事

- 直接创建 RHI 资源。
- 直接访问 Vulkan/DX11/OpenGL 对象。
- 直接调用 draw/dispatch。
- 直接设置 descriptor/layout/barrier。
- 每帧解释 Lua 来构建 pipeline。

### 6.4 安全要求

- Pipeline Lua 使用 sandbox 环境。
- 禁用 `os.execute`、任意 `io`、`debug`、动态 `loadfile/dofile`。
- Lua 加载失败时回退 C++ 内置默认 profile。
- Lua profile 只在启动、热重载或切换 profile 时解析。
- 每帧执行的是 `CompiledRenderPipeline`，不是 Lua table。

### 6.5 Lua PipelineProfile MVP

第一版 Lua 配置渲染管线的目标不是完整 SRP，也不是自定义渲染命令，而是：

```text
Lua 配置现有 builtin pass 的开关、顺序、质量参数。
```

MVP 必需能力：

- `RenderPipelineProfile` 数据结构。
- builtin pass 名称映射或最小 `PassRegistry`。
- 最小 `PipelineValidator`。
- Lua sandbox loader。
- Lua table -> `RenderPipelineProfile`。
- `FramePipeline` 根据 profile 构建/启用 pass。
- 加载失败回退 C++ 内置 `ForwardPlusDefault`。
- 输出 compiled pipeline dump，便于定位实际执行顺序。

MVP 不包含：

- Lua 自定义 pass 执行函数。
- Lua 直接 `Draw` / `Dispatch`。
- Lua 直接创建 RHI resource / descriptor / barrier。
- Lua 自定义 shader path。
- Lua 自定义 render target format。
- 插件 pass 热插拔。
- 编辑器 UI / 节点图。
- Deferred / Hybrid pipeline。
- 完整 `RenderObjectRegistry`。
- 完整 `RenderBlackboard` 替换。

建议第一版 Lua 格式保持纯 table：

```lua
return {
    name = "ForwardPlusLite",

    settings = {
        gpu_driven = true,
        shadows = true,
        shadow_quality = "medium",
        postprocess_quality = "lite",
    },

    passes = {
        { name = "pre_z", enabled = true },
        { name = "csm_shadow", enabled = true },
        { name = "spot_shadow", enabled = false },
        { name = "point_shadow", enabled = false },
        { name = "gpu_cull", enabled = true },
        { name = "forward_scene", enabled = true },
        { name = "ssao", enabled = false },
        { name = "contact_shadow", enabled = false },
        { name = "bloom", enabled = true, intensity = 0.5 },
        { name = "composite", enabled = true },
        { name = "fxaa", enabled = true },
        { name = "ui", enabled = true },
        { name = "present", enabled = true },
    }
}
```

建议通过环境变量或项目配置选择 profile：

```text
DSE_RENDER_PIPELINE_PROFILE=forward_plus
DSE_RENDER_PIPELINE_PROFILE=lite
DSE_RENDER_PIPELINE_PROFILE=debug_depth
DSE_RENDER_PIPELINE_PROFILE=custom_lite
```

示例 Lua profile 建议放在 `samples/lua/pipelines/`；运行时短名查找仍兼容 `data/pipelines/`。

MVP 的最小 validator 应检查：

- pass name 是否已注册。
- required pass 是否缺失。
- `present` 或等价最终输出是否存在。
- disabled pass 是否导致后续 pass 读取缺失资源。
- 当前后端是否支持该 pass。
- 参数类型是否正确。
- profile 编译失败时是否能回退默认 profile。

MVP 与阶段关系：

```text
Lua Pipeline MVP 必需：
    Phase 1 PipelineProfile
    Phase 2 PassRegistry / metadata 最小版
    Phase 5 Lua sandbox loader
    PipelineValidator 最小版

Lua Pipeline 完整版建议：
    Phase 3 RenderBlackboard
    Phase 4 RenderObjectRegistry / QueueBuilder
```

因此，Lua 配置现有 builtin pass 的 MVP 不需要等待 Deferred、编辑器节点图或完整 RenderObjectRegistry。

---

## 7. 分阶段实施路线

### Phase 1：PipelineProfile 控制 builtin pass

目标：先不允许任意 pass，只把当前固定管线改为可配置 profile。

改动：

- 新增 `RenderPipelineProfile`。
- 新增内置 `ForwardPlusDefault` profile。
- `FramePipeline` 根据 profile 决定启用哪些 builtin pass。
- 支持质量档位参数：shadow、SSAO、Bloom、FXAA/TAA、GPU Driven 等。

预计改动量：中等。

收益：

- 低端/高端/编辑器/调试 profile 可分离。
- 不再所有场景强制跑同一条重管线。
- 风险低，适合第一步。

### Phase 2：PassRegistry 与 pass metadata

目标：把 pass 创建从硬编码改为注册表。

改动：

- 每个 builtin pass 注册名称和 metadata。
- `FramePipeline` 不直接知道所有 pass 类型。
- `PipelineValidator` 检查 profile 合法性。

预计改动量：中到大。

收益：

- 后续新增 pass 不需要硬改主 pipeline。
- 模块/插件可以注册 C++ pass。
- 为 Lua/JSON pipeline asset 打基础。

### Phase 3：RenderBlackboard 替代膨胀 Context

目标：降低 pass 对 `RenderPassContext` 的隐式耦合。

改动：

- 将资源句柄迁移到 blackboard。
- 将 settings / debug / feature flags 与 resource handles 分离。
- 保留兼容字段，分批迁移 pass。

预计改动量：中到大。

收益：

- pass 参数和资源依赖更清晰。
- 自定义 pass 更容易接入。
- validation 更容易实现。

### Phase 4：RenderScene / QueueBuilder 稳定化

目标：pass 不再直接理解 module。

改动：

- 引入稳定 `RenderObjectId`。
- 建立 renderable registry。
- 统一分类：`OpaqueGpuDrivenQueue`、`OpaqueCpuQueue`、`SkinnedQueue`、`TransparentQueue`、`TerrainQueue`、`FoliageQueue`、`HairQueue`、`ParticleQueue`、`DebugQueue`。

预计改动量：大。

收益：

- GPU Driven、CPU fallback、terrain/grass/hair/particle 统一分流。
- 避免双绘和漏绘。
- 为 Deferred/Hybrid pipeline 打基础。

### Phase 5：Lua PipelineProfile

目标：允许项目通过 Lua 配置 pipeline profile。

改动：

- 新增 pipeline config sandbox loader。
- Lua table -> `RenderPipelineProfile`。
- Validator 输出清晰错误。
- 失败回退默认 profile。

预计改动量：中。

收益：

- 项目可自定义管线。
- 调试 pipeline 可快速切换。
- 后续编辑器可以生成 Lua/JSON profile。

### Phase 6：可选 Deferred / Hybrid Pipeline

目标：在可编程管线底座稳定后，再实现 Deferred 作为一个 profile，而不是替代整个架构。

可能 profile：

```text
ForwardPlusPipeline
DeferredPipeline
HybridPipeline
MobileForwardPipeline
EditorPipeline
DebugPipeline
```

Deferred 建议作为中长期目标，不应阻塞 PipelineProfile / PassRegistry。

---

## 8. Deferred 是否必须

结论：**不是必须，且不应作为第一优先级。**

当前 DSE 的 Forward+ 仍然适合默认主线：

- 透明路径天然友好。
- terrain / grass / hair / particle 专用路径更容易保留。
- 三后端一致性成本较低。
- GPU Driven 已经在 Forward+ 主路径打通。
- 大部分现有效果已基于 Forward+ 工作。

Deferred 的收益主要在：

- 大量动态光源。
- SSR / SSGI / 屏幕空间高级效果。
- 材质分类与 GBuffer debug。

因此推荐：

```text
先做可编程 Forward+ pipeline
再把 Deferred 作为一个可选 PipelineProfile 接入
```

---

## 9. 风险与防护

| 风险 | 影响 | 防护 |
|---|---|---|
| Lua pass 顺序错误 | 黑屏、读未初始化资源、Vulkan layout 错误 | `PipelineValidator` + RenderGraph 依赖声明 |
| Lua 权限过大 | 安全问题、运行时状态污染 | sandbox，只允许声明 profile |
| 每帧解析 Lua | GC 抖动、CPU 开销 | 只在加载/热重载时解析，缓存 compiled pipeline |
| RenderPassContext 继续膨胀 | 可维护性下降 | 引入 `RenderBlackboard` |
| Pass metadata 不完整 | 依赖错误难定位 | 每个 pass 强制注册 reads/writes/requires |
| Vulkan barrier/layout 错误 | validation error 或崩溃 | 禁止绕过 RenderGraph 直接操作资源 |
| 插件 pass 破坏三后端一致性 | 某后端不可用 | metadata 声明 backend support，validator 拒绝不兼容 profile |
| Deferred 过早引入 | 大范围重构拖慢主线 | 先做 profile/registry，再做 deferred |

---

## 10. 收益评估

### 10.1 架构收益

当前：

```text
FramePipeline 持有大量 pass 和状态
```

目标：

```text
FramePipeline = pipeline runtime
PipelineProfile = 管线配置
PassRegistry = pass 创建与能力声明
RenderGraph = 资源依赖执行
RenderScene = 渲染对象输入
```

职责更清晰，后续扩展成本更低。

### 10.2 项目收益

不同项目/平台可选择不同 profile：

```text
PC 高画质        Forward+ + GPU Driven + SSAO + TAA + Bloom
低端设备         Forward + FXAA + 无 SSAO/Contact Shadow
编辑器视图       EditorPipeline + Gizmo + Picking + Outline
截图回归         DeterministicPipeline
调试             Depth / Normal / Cluster / Overdraw profile
```

### 10.3 性能收益

可编程管线本身不直接让 shader 更快，但它允许：

- 不需要的 pass 不创建、不执行。
- 按相机/视图切换 pipeline。
- 编辑器与 runtime 使用不同 pass 组合。
- 后处理按质量档裁剪。
- GPU Driven / CPU fallback 分流更明确。

### 10.4 维护收益

新增功能从：

```text
改 FramePipeline
改 RenderPassContext
改 builtin_passes
改多个后端绑定
改脚本开关
```

逐步变为：

```text
注册 pass
声明 metadata
配置 profile
通过 validator
```

---

## 11. 建议的默认 Pipeline Profiles

### 11.1 ForwardPlusDefault

默认运行时高质量 profile：

```text
pre_z
csm_shadow
spot_shadow
point_shadow
gpu_cull
forward_scene
ssao
contact_shadow
bloom
composite
taa_or_fxaa
ui
present
```

### 11.2 ForwardPlusLite

低端或快速启动：

```text
pre_z
csm_shadow
forward_scene
bloom(optional)
composite
fxaa
ui
present
```

### 11.3 EditorPipeline

编辑器视图：

```text
pre_z
forward_scene
outline/picking
gizmo
grid
ui
present
```

### 11.4 DebugPipeline

调试视图：

```text
depth_visualize
normal_visualize
cluster_visualize
overdraw_visualize
gpu_driven_debug
present
```

### 11.5 Future DeferredPipeline

中长期：

```text
pre_z
gbuffer
shadow
deferred_lighting
transparent_forward
ssao/ssr
bloom
composite
ui
present
```

---

## 12. 验证标准

### 12.1 PipelineProfile 阶段

- [ ] 默认 profile 与当前固定管线视觉一致。
- [ ] 禁用 SSAO/Bloom/FXAA 后不创建对应资源和 pass。
- [ ] 无效 pass 名称给出清晰错误并回退默认 profile。
- [ ] Vulkan validation clean。
- [ ] OpenGL / Vulkan / DX11 三后端截图正常。

### 12.2 PassRegistry 阶段

- [ ] 所有 builtin pass 都通过 registry 创建。
- [ ] pass metadata 覆盖 reads/writes/requires。
- [ ] validator 能发现缺失依赖。
- [ ] 插件 pass 可注册但不能绕过 RenderGraph。

### 12.3 Lua profile 阶段

- [ ] Lua sandbox 禁用危险库。
- [ ] Lua profile 只在加载/热重载时解析。
- [ ] 配置错误能定位到 pass 和字段。
- [ ] 失败回退内置 profile。
- [ ] profile 切换后资源正确重建。

### 12.4 RenderScene / QueueBuilder 阶段

- [ ] GPU-driven eligible mesh 不双绘。
- [ ] skinned/transparent/terrain/grass/hair/particle 不漏绘。
- [ ] PreZ / Shadow / Forward 消费队列一致。
- [ ] Vulkan/OpenGL/DX11 三端视觉一致。

---

## 13. 对现有代码影响

| 模块 | 影响 | 说明 |
|---|---|---|
| `engine/runtime/frame_pipeline.*` | 大 | 从硬编码 pass 组装迁移到 profile/registry |
| `engine/render/passes/*` | 中 | 每个 pass 增加 metadata 和参数 schema |
| `engine/render/passes/render_pass_context.h` | 中到大 | 逐步拆分为 blackboard/settings/resources |
| `engine/render/render_graph.*` | 小到中 | 可能补 validation/debug dump，不建议大改 |
| `engine/render/render_scene.h` | 中到大 | 后续扩展 RenderObjectRegistry / QueueBuilder |
| `engine/render/rhi/*` | 小 | 第一阶段不应新增大量 RHI API |
| `engine/scripting/lua/*` | 中 | Lua profile loader 和 sandbox |
| `apps/editor_cpp` | 中长期 | 后续可增加 pipeline profile 编辑 UI |

---

## 14. 推荐下一步

优先级最高的是：

```text
Phase 1：PipelineProfile 控制 builtin pass 开关/参数
```

原因：

- 改动量可控。
- 不破坏现有三后端稳定性。
- 立刻带来项目/调试/性能配置收益。
- 为 PassRegistry 和 Lua profile 打基础。

建议最小交付：

- C++ 内置 `ForwardPlusDefault` profile。
- 支持 `DSE_RENDER_PIPELINE_PROFILE=default|lite|debug`。
- 支持开启/关闭 SSAO、Bloom、FXAA、Shadow、GPU Driven。
- 输出 compiled pipeline dump。
- 三端 fresh 截图验证。

---

## 15. 最终目标

DSEngine 的最终渲染架构建议收敛为双输入模型：场景侧先稳定产出 render queues，管线侧负责编译 pass graph，二者在 RenderGraph 执行前汇合。

```text
World / ECS / Modules
    -> RenderObjectRegistry
    -> QueueBuilder / RenderQueues

Pipeline Asset / Profile
    -> PassRegistry + PipelineValidator
    -> CompiledRenderPipeline

CompiledRenderPipeline + RenderQueues
    -> RenderGraph
    -> RHI Backend
```

注意：`RenderQueues` 是场景数据输入，不是 RenderGraph 之后的阶段；RenderGraph 负责消费 pipeline profile、pass metadata 与 queues，生成最终 pass/resource 依赖并提交到 RHI。

最终应满足：

- 默认 Forward+ GPU Driven profile 稳定。
- 项目可通过 profile 配置 pass 组合和质量档。
- 模块/插件可注册 C++ pass。
- Lua 可声明 pipeline profile，但不直接执行渲染命令。
- Deferred / Hybrid 作为可选 profile 接入，而不是替代默认架构。
- Vulkan/OpenGL/DX11 三后端持续通过视觉和 validation 回归。

一句话总结：

**DSE 当前已经具备可编程渲染管线的底座，下一步应先把固定 `FramePipeline` 抽象为 `PipelineProfile + PassRegistry + RenderBlackboard`，再逐步开放 Lua 配置和插件 pass。**

---

## 16. 显式迁移债清单

当前方案没有要求绕过 RenderGraph、没有让 Lua 直接操作 RHI，也没有要求立即重写为 Deferred，因此没有新增明显危险技术债。但 DSE 现有渲染架构仍有以下迁移债，需要随 Phase 1-5 逐步消化：

- [ ] `FramePipeline` 仍承担 pass 组装、资源准备、运行时状态同步等多重职责。
- [ ] `RenderPassContext` 已膨胀，需要拆分为 `RenderBlackboard`、pipeline settings、frame constants 与 scene refs。
- [ ] builtin pass 缺少统一 metadata / 参数 schema / backend support 声明。
- [ ] `PipelineValidator` 尚未实现，Lua/profile 配置错误还不能在执行前完整拦截。
- [ ] module callback 路径仍有残留，pass 尚未完全只消费 `RenderQueues`。
- [ ] `RenderScene` 仍是过渡层，尚未升级为稳定的 `RenderObjectRegistry + QueueBuilder`。
- [ ] shader/material pass 体系还不完整，尚未系统化区分 Forward / DepthOnly / Shadow / GBuffer / MotionVector 等 pass。
- [ ] Lua pipeline profile 的 sandbox loader 未实现。
- [ ] Deferred / Hybrid 仍是未来 profile，不应阻塞当前 Forward+ GPU Driven 主线。

这些债务不阻塞当前文档路线，但必须在后续实现中保持显式跟踪，避免误以为 `PipelineProfile` 落地后就等于完整 SRP。
