# GPU Driven 与 Gameplay Module 解耦重构方案

> 日期：2026-05-26  
> 状态：阶段性落地 / 三后端视觉验证通过  
> 关联代码：`FramePipeline`、`RenderPassContext`、`BuiltinModulesImpl`、`Gameplay3DModule`、`MeshRenderSystem`、`builtin_passes.cpp`

---

## 1. 背景与当前结论

当前引擎的 GPU Driven 逻辑存在一个架构级问题：**默认 Gameplay3D module 启用后，GPU Driven 实际不会进入有效渲染路径**。

表面上看，`DSE_DISABLE_GPU_DRIVEN=0` 或不设置该变量时，日志可能显示 GPU Driven 能力可用；但真实绘制是否走 GPU Driven 取决于是否执行 `PrepareGPUScene()` 并生成 `gpu_indirect_draw_count > 0`。

当前关键闸门位于：

```cpp
// engine/runtime/frame_pipeline.cpp
const bool gpu_driven_no_module = render_resources_.gpu_driven_supported
    && modules_.empty() && !builtin_gameplay3d_enabled_;
if (gpu_driven_no_module) {
    modules_impl_->PrepareGPUScene(*runtime_context_.world, render_pass_context_);
}
```

而默认模块启用逻辑为：

```cpp
const auto runtime_modules = ResolveRuntimeModules();
const bool enable_gameplay3d = runtime_modules.empty() ||
    std::find(runtime_modules.begin(), runtime_modules.end(), "Gameplay3D") != runtime_modules.end();
```

因此默认情况下：

1. `DSE_RUNTIME_MODULES` 未设置。
2. `enable_gameplay3d == true`。
3. `builtin_gameplay3d_enabled_ == true`。
4. `gpu_driven_no_module == false`。
5. `PrepareGPUScene()` 不执行。
6. `gpu_indirect_draw_count` 保持 0。
7. PreZ / Shadow / Forward / GPUCull pass 的 GPU Driven 分支全部跳过。

结论：**当前环境变量只能控制 GPU Driven 能力检测是否被禁用，不能保证 GPU Driven 实际激活。默认 Gameplay3D 路径下，GPU Driven 实际基本无效。**

---

## 2. 当前开关语义问题

### 2.1 `DSE_DISABLE_GPU_DRIVEN`

当前代码含义：

- 未设置：允许能力检测。
- `DSE_DISABLE_GPU_DRIVEN=0`：允许能力检测。
- `DSE_DISABLE_GPU_DRIVEN=1`：禁用能力检测。

它只影响：

```cpp
render_resources_.gpu_driven_supported
```

不影响：

```cpp
modules_.empty() && !builtin_gameplay3d_enabled_
```

所以 `DSE_DISABLE_GPU_DRIVEN=0` 不是“强制开启 GPU Driven”，而只是“不禁止 GPU Driven”。

### 2.2 `RenderPassContext::gpu_driven_enabled`

当前赋值：

```cpp
render_pass_context_.gpu_driven_enabled = render_resources_.gpu_driven_supported;
```

这个字段名有误导性。它表达的是“后端能力支持”，不是“本帧 GPU Driven 已激活”。

实际激活还必须满足：

```cpp
ctx.gpu_draw_cmd_ssbo != 0
ctx.gpu_mega_vao != 0
ctx.gpu_indirect_draw_count > 0
```

### 2.3 压测脚本输出误导

`examples/stress_test/script/main.lua` 和 `cube_stress.lua` 中：

```lua
os.getenv("DSE_DISABLE_GPU_DRIVEN") == "1" and "OFF" or "ON"
```

这个输出只说明环境变量没有禁用 GPU Driven，不说明引擎实际执行了 GPU Driven draw。

建议后续 profiler 输出新增真实字段：

- `gpu_driven_supported`
- `gpu_driven_scene_prepared`
- `gpu_indirect_draw_count`
- `gpu_total_instances`
- `gpu_texture_bucket_count`
- `gpu_driven_active_this_frame`

---

## 3. 为什么当前设计不合理

当前设计把 GPU Driven 与 module 绑定成互斥关系：

```text
有 module        -> 不 PrepareGPUScene -> 不 GPU Driven
无 module        -> PrepareGPUScene    -> 可 GPU Driven
默认 Gameplay3D -> 有 module          -> 不 GPU Driven
```

这与主流引擎的架构不一致。

主流架构通常是：

```text
Gameplay / Module / Component
        ↓ 产生 renderable
Render Scene / GPU Scene
        ↓ 分类、剔除、排序、批处理
Pass-specific draw submission
```

而不是：

```text
Gameplay module 存在 -> 禁用 GPU Driven
```

合理模型应该是：

```text
Gameplay3DModule
  TerrainSystem       -> 专用 terrain path
  MeshRenderSystem    -> eligible opaque mesh 走 GPU Driven，其余走 CPU per-item
  GrassSystem         -> 专用 grass / foliage path
  HairSystem          -> 专用 hair path
  ParticleSystem      -> 专用 particle path
  Transparent meshes  -> WBOIT / transparent path
```

GPU Driven 应该是 MeshRenderSystem 的一种提交路径，而不是与 Gameplay3DModule 互斥。

---

## 4. 主流引擎参考模型

### 4.1 Unreal Engine

Unreal 中 gameplay module / plugin / component 不决定 GPU Scene 是否启用。

典型路径：

1. `UPrimitiveComponent` 注册到 scene。
2. 生成 `FPrimitiveSceneProxy`。
3. Renderer 维护 `FScene`。
4. 每个 primitive 持有 bounds、material、render state、shadow state。
5. 静态 mesh、skeletal mesh、Nanite、translucent、instanced mesh 分流。
6. GPU Scene / HZB / occlusion / indirect draw 按 primitive 类型与 pass 能力启用。

核心原则：**是否 GPU driven 取决于 renderable 类型和 pass 能力，不取决于对象来自哪个 gameplay module。**

### 4.2 Unity SRP / Entities Graphics

Unity 中 GameObject / Component 产生 Renderer，SRP 收集后按：

- RenderQueue
- material
- shader pass
- bounds
- layer
- shadow caster
- instancing compatibility
- transparency

进行分类。GPU Instancing、SRP Batcher、BatchRendererGroup、Entities Graphics 也是按 renderer 类型和数据布局启用。

核心原则同样是：**module 不互斥，renderable 分类分流。**

---

## 5. DSE 目标架构

### 5.1 目标

1. 默认 Gameplay3D 启用时，GPU Driven 可以正常参与渲染。
2. GPU Driven 只接管 eligible opaque mesh。
3. 非 eligible renderable 保持现有路径。
4. 每个实体每个 pass 只走一条路径，避免双重绘制。
5. 支持三后端逐步验证与回退。
6. 开关语义清晰：supported、requested、active 分离。

### 5.2 新状态模型

建议拆分当前 `gpu_driven_enabled`：

```cpp
struct GpuDrivenFrameState {
    bool requested = false;             // 用户/配置请求启用
    bool supported = false;             // RHI 能力支持
    bool allowed_with_modules = false;  // 当前策略允许 module 协同
    bool scene_prepared = false;        // PrepareGPUScene 是否成功执行
    bool active_this_frame = false;     // 本帧是否存在 indirect draw
    int indirect_draw_count = 0;
    int total_instances = 0;
};
```

最低侵入版本可以先在 `RenderPassContext` 中增加：

```cpp
bool gpu_driven_supported = false;
bool gpu_driven_requested = false;
bool gpu_driven_scene_prepared = false;
bool gpu_driven_active_this_frame = false;
```

保留旧字段一段时间，但逐步减少使用。

---

## 6. Renderable 分类策略

### 6.1 GPU Driven eligible opaque mesh

初始阶段继续使用当前判定：

```cpp
bool MeshRenderSystem::IsGPUDrivenEligible(World& world, entt::entity entity,
                                           const MeshRendererComponent& mr) {
    if (!mr.visible) return false;
    if (mr.temp_vertices.empty() || mr.temp_indices.empty()) return false;
    if (!mr.local_bounds_valid) return false;
    if (mr.color.a < 0.999f) return false;
    if (world.registry().all_of<Animator3DComponent>(entity)) {
        const auto& a = world.registry().get<Animator3DComponent>(entity);
        if (a.enabled && !a.final_bone_matrices.empty()) return false;
    }
    return true;
}
```

该类进入：

- GPU Scene instance SSBO
- material SSBO
- draw command SSBO
- AABB SSBO
- texture buckets
- PreZ GPU indirect
- Shadow GPU indirect
- Forward GPU indirect
- GPUCullPass

### 6.2 CPU per-item opaque mesh

以下继续 CPU path：

- 蒙皮 mesh
- morph 复杂路径
- bounds 不可靠 mesh
- 未加载完成 mesh
- alpha 非 1 的 mesh
- 特殊材质暂未支持的 mesh
- debug/editor 特殊绘制

由 `MeshRenderSystem::Render()` 提交。

当 `gpu_driven_active_ == true` 时，`Render()` 必须跳过 GPU-driven eligible 实体，避免双重绘制。

### 6.3 Static batch

当前存在 `static_batch_items_`。GPU Driven 开启时必须避免 static batch 与 GPU Driven 重叠。

短期策略：

```cpp
if (!static_batch_items_.empty() && !gpu_driven_active_) {
    cmd_buffer.DrawMeshBatch(static_batch_items_);
}
```

中期策略：

- 将 static batch 作为 CPU fallback 优化。
- GPU Driven active 时，eligible static mesh 进入 GPU Driven。
- non-eligible static mesh 仍可保留独立 fallback batch，但需要按 entity eligibility 分拆 static batch。

### 6.4 Terrain

Terrain 不建议直接塞进普通 mesh GPU Driven 第一阶段。

原因：

- terrain 通常有 chunk / patch / LOD / splat 材质。
- bounds 应按 chunk 而非整地形。
- shader 与普通 PBR mesh 不完全等价。

短期策略：terrain 保持 `terrain_system_.Render()`。

中期策略：新增 terrain chunk renderable：

```text
TerrainChunkRenderable
  chunk_id
  local_bounds
  world_bounds
  height/splat resources
  lod_level
  material_id
```

可选进入专用 Terrain GPU Driven path。

### 6.5 Grass / foliage

Grass 适合 GPU Driven，但不建议复用普通 mesh GPU Driven 第一阶段。

短期策略：保持 `grass_system_.Render()` / `RenderShadow()`。

中期策略：按 patch/cell 做 GPU culling 和 indirect draw。

```text
GrassPatch
  bounds
  instance_count
  wind params
  lod params
  material/texture
```

### 6.6 Hair

Hair 为专用模拟和渲染路径，保持独立。

短期策略：不进入普通 GPU Driven。

### 6.7 Particles

Particle 以 emitter/system bounds 管理，保持专用 path。

短期策略：不进入普通 GPU Driven。

### 6.8 Transparent mesh

透明物体不进入第一阶段 GPU Driven opaque path。

原因：

- 透明需要排序或 OIT。
- 通常不写 depth。
- 不能作为 occluder。
- blending 顺序影响结果。

短期策略：

- `IsGPUDrivenEligible()` 排除 alpha 非 1。
- WBOIT / transparent path 继续处理。
- transparent 仍可做 CPU frustum culling。

中期策略：独立 transparent GPU list：

```text
TransparentRenderable
  bounds
  sort_key / depth
  material
  mesh entry
```

后续可做 GPU sort / per-tile binning / OIT。

### 6.9 Bounds

所有 renderable 都应该有 bounds，但用途不同。

| 类型 | Bounds 策略 | GPU Driven 关系 |
|---|---|---|
| static opaque mesh | asset local bounds + transform | 第一阶段支持 |
| dynamic transform mesh | local bounds + 每帧 world transform | 第一阶段可支持 |
| skinned mesh | animation/clip conservative bounds | 后续支持 |
| terrain | chunk bounds | 专用路径 |
| grass | patch/cell bounds | 专用路径 |
| particles | emitter/system bounds | 专用路径 |
| transparent | object bounds + sort depth | 独立透明路径 |

---

## 7. Pass 协同设计

### 7.1 Prepare 阶段

当前：

```cpp
if (gpu_driven_no_module) {
    modules_impl_->PrepareGPUScene(...);
}
```

目标：

```cpp
const bool can_prepare_gpu_scene = render_resources_.gpu_driven_supported
    && render_pass_context_.gpu_driven_requested
    && ShouldPrepareGpuSceneWithCurrentModules();

if (can_prepare_gpu_scene) {
    const int prepared = modules_impl_->PrepareGPUScene(*runtime_context_.world, render_pass_context_);
    render_pass_context_.gpu_driven_scene_prepared = prepared > 0;
    render_pass_context_.gpu_driven_active_this_frame = prepared > 0;
    SyncGpuSceneHandlesToRenderResources();
}
```

第一阶段 `ShouldPrepareGpuSceneWithCurrentModules()` 可以是：

```cpp
bool FramePipeline::ShouldPrepareGpuSceneWithCurrentModules() const {
    if (!render_resources_.gpu_driven_supported) return false;
    if (gpu_driven_policy_ == Disabled) return false;
    if (gpu_driven_policy_ == Force) return true;
    return builtin_gameplay3d_enabled_ || modules_.empty();
}
```

但更推荐显式策略：

```text
DSE_GPU_DRIVEN_POLICY=off|auto|force|with_modules
```

### 7.2 PreZPass

逻辑保持：

```cpp
if (ctx_.gpu_driven_active_this_frame && ctx_.gpu_indirect_draw_count > 0) {
    GPU indirect depth-only draw
}

for modules:
    OnRenderPreZ() // 内部 MeshRenderSystem 跳过 eligible mesh
```

### 7.3 ShadowPass

逻辑保持：

```cpp
if (ctx_.gpu_driven_active_this_frame) {
    GPU indirect shadow draw
}

for modules:
    OnRenderShadow() // 内部 MeshRenderSystem 跳过 eligible mesh
```

注意：GPUCullPass 会按主相机修改 draw command，shadow pass 必须继续位于 GPUCullPass 前，使用未被主相机剔除的 draw commands。

### 7.4 GPUCullPass

只在 active 时执行：

```cpp
if (!ctx_.gpu_driven_active_this_frame) return;
```

不要只看 supported。

### 7.5 ForwardScenePass

逻辑保持：

1. 绑定 light SSBO / cluster SSBO。
2. GPU Driven indirect 绘制 eligible opaque mesh。
3. module path 绘制 non-eligible 和专用系统。
4. 透明在后续 WBOIT/Transparent pass。

### 7.6 WBOITPass

短期保持 CPU/module transparent path。

需要注意当前 `WBOITPass` 同时调用：

```cpp
ctx_.render_transparent_meshes(...)
for modules: mod.instance->OnRenderTransparent(...)
```

在 `builtin_gameplay3d_enabled_` 为 true 时，`render_transparent_meshes` 可能调用 fallback mesh system，而 module 又调用 Gameplay3D 的 mesh system。需要确认两者不会重复同一批组件。

建议后续统一为：

- 有 Gameplay3D module 时只走 module transparent。
- 无 Gameplay3D module 时才走 fallback transparent。

---

## 8. 模块与 GPU Scene Provider 抽象

当前 `IBuiltinModules::PrepareGPUScene()` 已经存在，但语义是全局 fallback。

建议引入更明确的接口：

```cpp
class IGpuSceneProvider {
public:
    virtual int PrepareGpuScene(World& world, dse::render::RenderPassContext& ctx) = 0;
    virtual bool HasGpuSceneContent(World& world) const = 0;
};
```

短期不必新增虚基类，可以先扩展 `IBuiltinModules`：

```cpp
virtual bool HasGameplay3DGpuSceneProvider() const = 0;
virtual int PrepareGameplay3DGpuScene(World& world, RenderPassContext& ctx) = 0;
```

长期目标：

```text
FramePipeline
  collects providers:
    Builtin Gameplay3D MeshRenderSystem
    Dynamic modules implementing IGpuSceneProvider
    Future terrain/foliage providers
  builds unified RenderScene / GPU Scene
```

---

## 9. 环境变量与配置重构

### 9.1 保留旧变量

保留：

```text
DSE_DISABLE_GPU_DRIVEN=1
```

作为最高优先级禁用。

### 9.2 新增策略变量

建议新增：

```text
DSE_GPU_DRIVEN_POLICY=auto|off|force|with_modules
```

语义：

| 值 | 含义 |
|---|---|
| `off` | 完全关闭 GPU Driven |
| `auto` | 默认策略，后端支持且场景 provider 支持时启用 |
| `with_modules` | 允许 Gameplay3D/module 协同 GPU Driven |
| `force` | 强制尝试 PrepareGPUScene，失败只报警不崩溃 |

兼容规则：

1. 如果 `DSE_DISABLE_GPU_DRIVEN=1`，强制 off。
2. 否则读取 `DSE_GPU_DRIVEN_POLICY`。
3. 未设置时默认为 `auto`。

### 9.3 新增诊断变量

建议新增：

```text
DSE_GPU_DRIVEN_DIAG=1
```

输出：

```text
[GpuDriven] requested=1 supported=1 policy=auto with_modules=1 scene_prepared=1 active=1 draws=128 instances=128 buckets=12 backend=dx11
```

### 9.4 修正压测输出

Lua benchmark 不应只根据 `DSE_DISABLE_GPU_DRIVEN` 打印 ON/OFF。

建议通过 C++ runtime stats 暴露：

```lua
local stats = app.get_render_stats()
print(string.format("GPU-Driven: supported=%s active=%s draws=%d instances=%d",
    tostring(stats.gpu_driven_supported),
    tostring(stats.gpu_driven_active),
    stats.gpu_indirect_draw_count,
    stats.gpu_total_instances))
```

---

## 10. 分阶段落地计划

### Phase 0：诊断修正，无行为改变

目标：让日志和 stats 反映真实状态。

修改点：

- `RenderPassContext` 新增 active/prepared 字段。
- `FramePipeline` 在 `PrepareGPUScene()` 前后写入真实状态。
- `GPUCullPass` 和各 draw pass 使用 `active_this_frame` 判断。
- 日志输出 supported/requested/active。
- 压测脚本改为读取真实 stats，或至少打印 `gpu_indirect_draw_count`。

验收：

- 默认 Gameplay3D 下日志明确显示 supported=true active=false。
- `DSE_DISABLE_GPU_DRIVEN=1` 显示 supported=false active=false。
- 不改变视觉结果。

### Phase 1：实验性允许 Gameplay3D + GPU Driven 协同

目标：移除 module 级互斥，但用环境变量保护。

策略：

```text
DSE_GPU_DRIVEN_POLICY=with_modules
```

时允许：

```cpp
render_resources_.gpu_driven_supported && builtin_gameplay3d_enabled_
```

执行 `PrepareGPUScene()`。

保留：

- dynamic external modules 默认仍不参与。
- 只针对内置 Gameplay3D 的 `MeshRenderSystem`。
- DX11 可先通过 backend guard 保守关闭。

验收：

- OpenGL 下 static opaque mesh 进入 GPU Driven。
- terrain/grass/skinned/transparent 正常由 module path 渲染。
- 无双重绘制。

### Phase 2：后端稳定化

目标：三后端一致。

检查项：

- OpenGL：`glMultiDrawElementsIndirect` + SSBO model/material。
- Vulkan：确认 descriptor set、material SSBO、instance SSBO、shadow shader binding。
- DX11：确认 indirect draw 的 draw id / base instance / material 绑定正确。

如果 DX11 仍存在 indirect 限制，策略可以是：

```text
DX11 GPU Driven active only when DSE_GPU_DRIVEN_POLICY=force
```

或使用 DX11 pseudo path 但明确标记：

```text
gpu_driven_submission_mode=pseudo_indirect
```

### Phase 3：默认启用 Gameplay3D GPU Driven

目标：把 `auto` 默认策略改为允许 Gameplay3D 协同。

条件：

- OpenGL/DX11/Vulkan 三端基础 demo 通过。
- stress test 确认 active=true 时性能不退化。
- KF_Framework 地面、城堡、角色、HUD、阴影正常。
- 三端视觉回归无明显新增差异。

### Phase 4：Renderable Registry / Render Scene

目标：从 `MeshRenderSystem::PrepareGPUScene()` 走向统一 render scene。

新增概念：

```text
RenderObjectId
RenderObjectFlags
RenderObjectBounds
RenderObjectMaterial
RenderObjectGeometry
RenderObjectQueue
```

分类队列：

```text
OpaqueGpuDrivenQueue
OpaqueCpuQueue
SkinnedQueue
TransparentQueue
TerrainQueue
FoliageQueue
HairQueue
ParticleQueue
DebugQueue
```

FramePipeline 不再直接关心 module，只收集 render queues。

### Phase 5：透明、蒙皮、terrain/foliage 专用 GPU Driven

后续扩展：

- transparent GPU sort / OIT path
- skinned GPU Driven with bone palette offset
- terrain chunk GPU culling
- grass patch GPU culling
- particle GPU culling

---

## 11. 关键代码修改清单

### 11.1 `engine/render/passes/render_pass_context.h`

新增字段：

```cpp
bool gpu_driven_requested = false;
bool gpu_driven_supported = false;
bool gpu_driven_scene_prepared = false;
bool gpu_driven_active_this_frame = false;
```

保留旧字段 `gpu_driven_enabled`，短期映射到 `gpu_driven_supported`，后续删除或改名。

### 11.2 `engine/runtime/frame_pipeline.cpp`

修改区域：

- GPU Driven 能力检测。
- BuildRenderGraphInternal 中 context 初始化。
- RunRenderInternal / ExecuteRenderFrame 中 PrepareGPUScene 闸门。
- readback 条件。
- stats 输出。

将：

```cpp
modules_.empty() && !builtin_gameplay3d_enabled_
```

替换为策略函数：

```cpp
ShouldPrepareGpuSceneThisFrame()
```

### 11.3 `modules/runtime_bridge/builtin_modules_impl.cpp`

短期保持 `PrepareGPUScene()`，但修正语义：

- 如果 Gameplay3D 启用，明确走 Gameplay3D mesh system。
- 如果 Gameplay3D 未启用，走 fallback mesh system。

当前在 `DSE_ENABLE_3D` 下无条件走 `gameplay3d_module_.mesh_render_system()`，语义不够清晰。

建议增加状态判断或拆分接口。

### 11.4 `modules/gameplay_3d/rendering/mesh_render_system.cpp`

检查点：

- `gpu_driven_active_` 每帧必须重置。
- `PrepareGPUScene()` cmd_index 为 0 时也应设置 `gpu_driven_active_ = false`。
- `Render()` skip 条件必须和 `PrepareGPUScene()` eligibility 完全一致。
- static batch 和 cached opaque path 不能重复绘制 eligible mesh。

当前风险点：

```cpp
if (!cached_opaque_items_.empty()) {
    cmd_buffer.DrawMeshBatch(cached_opaque_items_);
}
```

需要确认 cached opaque 是否已经排除了 eligible mesh。否则 GPU Driven active 后可能双重绘制。

### 11.5 `engine/render/passes/builtin_passes.cpp`

将 pass 判断从：

```cpp
ctx_.gpu_driven_enabled && ctx_.gpu_indirect_draw_count > 0
```

改为：

```cpp
ctx_.gpu_driven_active_this_frame && ctx_.gpu_indirect_draw_count > 0
```

涉及：

- `PreZPass`
- `CSMShadowPass`
- `SpotShadowPass`
- `PointShadowPass`
- `ForwardScenePass`
- `GPUCullPass`

### 11.6 `examples/stress_test`

修正输出和 CSV：

- 不再用 env 判断 GPU Driven ON/OFF。
- 增加真实 active/draw count 输出。

---

## 12. 验证矩阵

### 12.1 功能场景

| 场景 | 预期 |
|---|---|
| 无 3D camera 标题场景 | 不渲染 3D mesh |
| 普通静态 mesh | GPU Driven active 后可见 |
| 动态 transform mesh | 可见，bounds 正确更新 |
| terrain | 可见，仍走 terrain path |
| grass | 可见，仍走 grass path |
| skinned character | 可见，仍走 CPU/instancing/skinning path |
| transparent mesh | WBOIT 正常，无错误排序新增问题 |
| shadow caster | GPU + CPU path 均投影正常 |
| point/spot shadow | 不丢失，不双绘 |
| editor wireframe/unlit/overdraw | GPU Driven 路径同步响应 scene view mode |

### 12.2 后端

| 后端 | Phase 1 | Phase 2 | Phase 3 |
|---|---|---|---|
| OpenGL | 首先开放 | 默认开放 | 回归基线 |
| Vulkan | 开关开放 | 默认开放 | 回归基线 |
| DX11 | 默认保守 | 修完后开放 | 回归基线 |

### 12.3 指标

必须记录：

- `gpu_driven_supported`
- `gpu_driven_active_this_frame`
- `gpu_indirect_draw_count`
- `gpu_total_instances`
- `draw_calls`
- `instanced_meshes`
- `avg_render_ms`
- `hiz culled count`
- screenshot RMSE

---

## 13. 当前实际开关结论

### 13.1 `DSE_DISABLE_GPU_DRIVEN=1`

有效。它会禁用能力检测，使：

```cpp
render_resources_.gpu_driven_supported = false;
```

### 13.2 `DSE_DISABLE_GPU_DRIVEN=0`

只表示“不禁用能力检测”。它不保证 GPU Driven active。

默认 Gameplay3D 启用时，仍会被以下逻辑挡掉：

```cpp
modules_.empty() && !builtin_gameplay3d_enabled_
```

### 13.3 默认不设置变量

等价于允许能力检测，但默认 Gameplay3D 会启用，所以实际仍通常 inactive。

### 13.4 压测脚本中的 GPU-Driven ON

当前只是环境变量层面的 ON，不是实际渲染路径 ON。

如果要判断真实开启，应该看：

```cpp
gpu_indirect_draw_count > 0
```

或新增：

```cpp
gpu_driven_active_this_frame == true
```

---

## 14. 推荐立即执行的最小修复

如果只做最小修复，建议顺序：

1. 新增真实 active 诊断字段。
2. 修正日志和 stress test 输出。
3. 加 `DSE_GPU_DRIVEN_POLICY=with_modules` 实验开关。
4. 在该开关下允许 builtin Gameplay3D 调用 `PrepareGPUScene()`。
5. 确认 `MeshRenderSystem::Render()` 所有 cached/static 路径不会双绘。
6. 先 OpenGL 验证，再 Vulkan，最后 DX11。

最小代码形态：

```cpp
const bool gpu_driven_with_modules = [] {
    const char* v = std::getenv("DSE_GPU_DRIVEN_POLICY");
    return v && (std::strcmp(v, "with_modules") == 0 || std::strcmp(v, "force") == 0);
}();

const bool can_prepare_gpu_scene = render_resources_.gpu_driven_supported &&
    (modules_.empty() && !builtin_gameplay3d_enabled_ ||
     gpu_driven_with_modules && builtin_gameplay3d_enabled_);
```

这不是最终架构，但可以安全验证 Gameplay3D + GPU Driven 协同是否仍有后端问题。

---

## 15. 最终目标状态

最终应达到：

```text
默认 Gameplay3D enabled
默认 GPU Driven supported
默认 opaque non-skinned mesh -> GPU Driven
terrain / grass / hair / particles / transparent / skinned -> 专用或 CPU path
三后端行为一致
日志真实反映 active 状态
benchmark 不再误报 GPU Driven ON
```

一句话：**GPU Driven 应该成为 Gameplay3D 的核心渲染提交路径之一，而不是被 Gameplay3D module 禁用。**

---

## 16. 架构审查结论：最佳方案边界

本方案分为两层：

1. **Phase 0-3：过渡层**  
   用于修正当前开关语义、恢复 Gameplay3D 与 GPU Driven 协同、验证三后端稳定性。

2. **Phase 4-5：终态层**  
   用统一 `RenderScene / RenderQueue / GPU Scene Provider` 替代 pass 直接调用 module 渲染，是长期最佳架构。

因此结论是：

- 只实现 Phase 0-1：不是最佳方案，只是诊断与实验开关修复。
- 实现 Phase 0-3：是可上线的阶段性方案，但仍有架构债。
- 实现 Phase 0-5：才是当前 DSEngine 下接近最佳的完整方案。

真正的目标不是“给 module 开一个 GPU Driven 开关”，而是让所有系统提交 renderable，再由 renderer 统一分类、剔除、排序和提交。

---

## 17. 技术债防护清单

为避免方案实施后停留在过渡状态，必须遵守以下约束：

1. **旧字段清理**  
   `gpu_driven_enabled` 只能作为短期兼容字段。Phase 2 结束前，所有 pass 判断必须改用 `gpu_driven_active_this_frame` 或等价真实 active 字段。

2. **环境变量只做配置覆盖**  
   `DSE_GPU_DRIVEN_POLICY` 只允许在初始化或配置加载阶段解析一次，不应在多个渲染路径里散落 `std::getenv()`。

3. **禁止 module 级互斥回归**  
   不允许再引入 `modules_.empty() && !builtin_gameplay3d_enabled_` 作为 GPU Driven 是否运行的长期条件。

4. **统一 eligibility 判定**  
   `PrepareGPUScene()` 与 CPU fallback skip 必须使用同一个 eligibility 结果。后续建议从 `bool` 升级为带 reject reason 的结构：

   ```cpp
   enum class GpuDrivenRejectReason {
       None,
       Invisible,
       MissingGeometry,
       MissingBounds,
       Transparent,
       Skinned,
       MorphTarget,
       UnsupportedMaterial,
       UnsupportedPipelineState,
       EditorSpecialCase,
   };
   ```

5. **Benchmark 必须读真实 runtime stats**  
   压测输出不得再仅根据 `DSE_DISABLE_GPU_DRIVEN` 判断 ON/OFF。真实开启条件必须来自 `gpu_driven_active_this_frame` 或 `gpu_indirect_draw_count > 0`。

6. **Shadow 与主相机 draw command 副作用隔离**  
   当前 shadow pass 依赖“在 `GPUCullPass` 前执行”以避免主相机 culling 修改 draw commands。长期应拆分：

   ```text
   draw_cmds_original
   draw_cmds_main_camera
   draw_cmds_shadow
   ```

7. **Static batch 必须拆分 fallback**  
   GPU Driven active 时不能简单长期跳过全部 `static_batch_items_`。最终应拆成：

   ```text
   static_gpu_eligible_batch -> GPU Driven
   static_cpu_fallback_batch -> CPU path
   ```

8. **Bounds 与对象 ID 稳定化**  
   GPU AABB、visibility readback、entity 映射不应长期依赖遍历顺序。最终需要稳定 `RenderObjectId` 与 `gpu_scene_index`。

9. **Pass 不直接理解 module**  
   `ForwardScenePass` / Shadow pass 长期不应直接遍历 `ctx_.modules`。最终应消费 `RenderQueue`。

10. **禁止继续扩大 `MeshRenderSystem::PrepareGPUScene()` 职责**  
    在 Phase 4 前，不应把 terrain、grass、hair、particles 等系统硬塞进普通 mesh GPU scene builder。

---

## 18. 最佳终态架构

最佳终态应为：

```text
Gameplay / Modules / Systems
        ↓
RenderableCollector
        ↓
RenderObjectRegistry
        ↓
QueueBuilder
        ↓
OpaqueGpuDrivenQueue
OpaqueCpuQueue
SkinnedQueue
TransparentQueue
TerrainQueue
FoliageQueue
HairQueue
ParticleQueue
DebugQueue
        ↓
RenderGraph Passes
```

各 pass 只消费队列，不直接关心对象来自哪个 module：

```text
PreZPass             -> OpaqueGpuDrivenQueue + OpaqueCpuQueue
CSMShadowPass        -> ShadowCaster queues
ForwardScenePass     -> OpaqueGpuDrivenQueue + OpaqueCpuQueue + Terrain/Foliage special queues
WBOITPass            -> TransparentQueue
UIPass               -> UI queue
```

这样才能保证：

- Gameplay3D 默认启用不影响 GPU Driven。
- dynamic module 也可以贡献 renderable。
- renderer 对所有 renderable 做统一剔除、排序和提交。
- GPU Driven 成为 renderer 的提交策略，而不是 module 的特殊分支。

---

## 19. 最终判定标准

本方案完成后，需要满足以下标准才算“最佳方案落地”：

- 默认 Gameplay3D 场景下 `gpu_driven_active_this_frame == true`。
- `DSE_DISABLE_GPU_DRIVEN=0` 不再被误解为强制开启，日志清晰区分 requested/supported/active。
- opaque non-skinned mesh 默认进入 GPU Driven。
- skinned、transparent、terrain、grass、hair、particles 均有明确队列或专用路径。
- PreZ、Shadow、Forward 三类 pass 不双绘、不漏绘。
- shadow culling 与 main camera culling 不通过 pass 顺序隐式耦合。
- benchmark 输出来自真实 runtime stats。
- OpenGL / Vulkan / DX11 三后端完成最小视觉与性能回归。
- 旧 module 级互斥逻辑删除。
- 旧 `gpu_driven_enabled` 误导性语义删除或更名。

---

## 20. 2026-05-26 实施进度更新

### 20.1 已完成

- [x] `RenderPassContext` 区分 GPU Driven `requested / supported / scene_prepared / active_this_frame`。
- [x] `FramePipeline` 默认 Gameplay3D 路径下允许 GPU Scene provider 准备 GPU-driven mesh。
- [x] `BuiltinModulesImpl` / `Gameplay3DModule` / `MeshRenderSystem` 接入 `BuildRenderQueues()` 与 `PrepareGPUScene()`。
- [x] `PreZPass` / `CSMShadowPass` / `ForwardScenePass` 改为消费 `RenderScene` 队列，并在 active 时执行 GPU indirect draw。
- [x] stress test 输出真实 runtime stats：`gpu_driven_requested / supported / prepared / active / indirect_draws / instances`。
- [x] Vulkan GPU-driven descriptor write 与 descriptor set 生命周期完成 validation clean-up。
- [x] Vulkan empty-frame present 路径修正，避免 acquire semaphore 未消费与 swapchain image layout 警告。
- [x] `SpotLightData` 从 `set=2,binding=10` 迁移到 `binding=19`，解除与 `SkinnedInstBuf` 的 descriptor 类型冲突。
- [x] DX11 同步修正 HLSL register 绑定：`TerrainParams=b4`、`SpotLightData=b5`。

### 20.2 最终验证记录

验证命令使用：

```text
DSE_GPU_DRIVEN_POLICY=auto
DSE_ENTITY_COUNT=1000
DSE_ANIM_ENABLED=0
DSE_NO_SHADOW=1
DSE_MAX_FRAMES=60
```

| 后端 | 日志 | 截图 | 结果 |
|---|---|---|---|
| Vulkan | `tmp/gpu_driven_refactor/vulkan_visual_final.log` | `tmp/gpu_driven_refactor/main1000_vulkan_visual_final.png` | `exit code 0`，无 `WARN/ERROR/Validation`，`gpu_driven_active=true` |
| OpenGL | `tmp/gpu_driven_refactor/opengl_visual_final.log` | `tmp/gpu_driven_refactor/main1000_opengl_visual_final.png` | `exit code 0`，截图正常，`gpu_driven_active=true` |
| DX11 | `tmp/gpu_driven_refactor/dx11_visual_final.log` | `tmp/gpu_driven_refactor/main1000_dx11_visual_final.png` | `exit code 0`，截图正常，`gpu_driven_active=true` |

三后端均记录：

```text
gpu_indirect_draws=175
gpu_instances=175
```

### 20.3 当前剩余事项

- [ ] Shadow enabled 场景再补一轮 Vulkan/OpenGL/DX11 视觉验证。
- [ ] skinned/animation fallback 场景验证 `SkinnedInstBuf` 真实数据与 dummy descriptor 均无回归。
- [ ] terrain splat 专项场景验证 DX11 `TerrainParams(b4)` 与 Vulkan `binding=16` 行为一致。
- [ ] 中长期继续推进 `RenderObjectRegistry / QueueBuilder`，减少 pass 对 module callback 的直接依赖。
