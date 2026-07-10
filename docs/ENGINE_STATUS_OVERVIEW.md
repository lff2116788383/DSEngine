# DSEngine 现状梳理 · 架构组织 · 工作流程 · 收尾分析

> 分支 `feature/engine-lib`，对照仓库 HEAD 逐文件核实。用于收尾阶段判断"哪些是主路径、哪些是多路径、哪些是半成品/实验特性"。

---

## 一、分层架构组织

DSEngine 是一个 ECS 内核 + 多后端 RHI + 分层模块 的 C++ 游戏引擎，脚本用 Lua（C ABI 桥接），可选 C#。自底向上分五层：

```
┌──────────────────────────────────────────────────────────────────────┐
│  应用层 apps/                                                          │
│   standalone · runtime · editor_cpp · launcher_tauri · web_host        │
│   android_host · tools(AssetBuilder 等离线工具)                        │
├──────────────────────────────────────────────────────────────────────┤
│  脚本 / C ABI 层  engine/scripting/native_api (88 文件, dse_* 导出)    │
│   Lua(ON) · C#/CoreCLR(OFF) · 世界系统 C ABI(Ocean/Spline/VSM/EQS...)  │
├──────────────────────────────────────────────────────────────────────┤
│  Gameplay 模块层 modules/                                              │
│   gameplay_3d (rendering/animation/character/vehicle/cloth/fluid/...)  │
│   gameplay_2d (rendering/particle/tilemap/lighting/spine/...)          │
│   runtime_bridge                                                       │
│   —— 经 IBuiltinModules + IModule 接口挂到 FramePipeline 更新图         │
├──────────────────────────────────────────────────────────────────────┤
│  引擎运行时 engine/runtime/                                            │
│   EngineApp(启动/服务注册) → FramePipeline(帧编排, Pimpl RenderState)   │
│   RenderThreadManager(渲染线程) · UpdateGraph(逻辑更新) · Snapshot      │
├──────────────────────────────────────────────────────────────────────┤
│  引擎核心子系统 engine/*                                               │
│   core(ServiceLocator/EventBus/JobSystem/Module/memory)                │
│   ecs(World/registry, 52 文件) · scene · assets(48) · reflect          │
│   render(393 文件: RHI 4后端 + 渲染图 + 高级特性) · physics · ai        │
│   navigation · audio · net · http · terrain · ui · video · input       │
└──────────────────────────────────────────────────────────────────────┘
```

### 关键编排点
- **依赖注入**：`core::ServiceLocator` 是全局服务注册表。引擎自有、开机/模块初始化创建的长生命周期系统注册进去（见下）。
- **两套系统持有模式**（这是之前 Ocean 问题的根因，已澄清）：
  - **A. 引擎自有 → 注册进 ServiceLocator**：`engine_app.cpp`(FramePipeline/World/EventBus/JobSystem/SceneManager/Localization/FontService/FileSystem)、`frame_pipeline.cpp`(Physics3D/NavMesh/Streaming)、`service_registration.cpp`(HttpClient/DSSLMaterialLoader/LuaDebugger/CrashReporter)、`gameplay_3d_module.cpp`(HLOD/VirtualTexture/GeometryClipmap/GlobalSDF/AILodScheduler/WorldPartition/WorldStatePersistence/CutscenePlayer)。
  - **B. 脚本按需创建 → C ABI 文件级全局(非单例)**：`dse_api_world_systems.cpp` 的 Ocean/Spline/VSM/EQS/WorldEditorTools/AssetDistribution。由 Lua `dse_*_init` 创建、`dse_*_update` 每帧驱动，唯一消费方是同 TU 的 C ABI，故不进 ServiceLocator。**这是有意设计，正常。**

---

## 二、帧工作流程

```
                         EngineApp::Run (主循环)
                                  │
        ┌─────────────────────────┼──────────────────────────┐
        │  逻辑线程 (Update)       │      渲染线程 (RenderThreadManager)
        ▼                         │                            ▼
  ┌───────────────────┐           │              ┌────────────────────────────┐
  │ FixedUpdateGraph  │  (定步长)  │              │  消费 RenderThinSnapshot     │
  │  · 2D 模块 fixed   │           │              │  (双缓冲 snapshot_pool_[2])  │
  │  · 插件 OnFixed    │           │              │                              │
  │  · FloatingOrigin │           │              │  FramePipeline::Render:      │
  │  · Physics3D(Jolt)│           │              │   1. GPUSkinningSystem       │
  └─────────┬─────────┘           │              │      Dispatch(compute)       │
            ▼                     │              │   2. 渲染图 AddPass 执行:     │
  ┌───────────────────┐           │              │      Shadow → Cluster光照 →   │
  │  UpdateGraph      │           │              │      Meshlet Cull(GPU驱动)→   │
  │  · 2D 模块 update  │           │              │      GBuffer/Forward →        │
  │  · 插件 OnUpdate   │  snapshot │              │      DDGI/Probe GI →          │
  │  · 3D 模块 update: ├───────────┼──build──────>│      Atmosphere/Sky →         │
  │    Transform/动画  │           │              │      PostFX → UI              │
  │    /Camera/HLOD/VT │           │              │   3. CommandBuffer(立即转发)  │
  │    /Culling/粒子   │           │              │      → RhiDevice::Real*()     │
  └───────────────────┘           │              │   4. Present (swapchain)      │
                                  │              └────────────────────────────┘
                          RHI 后端: OpenGL(默认) / Vulkan / D3D11 / WebGPU
```

要点：
- **逻辑/渲染分线程**，通过 `RenderThinSnapshot`（双缓冲）解耦；逻辑线程产出快照，渲染线程消费。
- **CommandBuffer 是"立即转发"**：`ForwardingCommandBuffer` 基类，四后端每条命令直接委托 `RhiDevice::Real*()` 立即执行；无录制、无多线程录制（见下 §四·P-架构）。
- **渲染图**：`passes/builtin_passes*.cpp` 用 `graph.AddPass(name)` 注册内置 pass（阴影/光照/后处理/大气），profile 可开关（`render_pipeline_profile`）。
- **GPU-driven**：Meshlet cull/indirect draw、GPU 粒子、GPU 蒙皮均为 compute → 间接绘制路径。

---

## 三、RHI 多后端 parity

| 后端 | 开关 | 状态 | 说明 |
|:-----|:-----|:-----|:-----|
| **OpenGL** | 默认(`RhiBackend::Default`) | ✅ 主路径、最完整 | 桌面主力，功能基准 |
| **Vulkan** | 编译期 | ✅ 完整 | 但 CommandBuffer 被降级成立即转发，未用其原生录制能力 |
| **D3D11** | 编译期 | ✅ 完整 | Windows 平台 |
| **WebGPU** | `DSE_ENABLE_WEBGPU=OFF` | 🟡 bring-up/实验 | Web/Emscripten(Dawn)，另有 `DSE_WEBGPU_SELFTEST` 自测竖井(含独立 WGSL 副本，仅诊断) |

四后端统一实现纯虚 `CommandBuffer` + `RhiDevice`。接口本身已抽象完整（BindPipeline/Draw/Compute/Indirect/PushConstants）。

---

## 四、多路径 / 半成品 / 实验特性 分析

### 4.1 多路径子系统（同一职责有多条实现，多为有意冗余/回退）

| 子系统 | 路径 | 定性 | 建议 |
|:-------|:-----|:-----|:-----|
| **蒙皮/Morph** | GPU compute(`GPUSkinningSystem`, morph 已统一进来) + CPU 回退(`mesh_render_system`) | ✅ 有意回退，合理 | 保留。冗余的 `MorphTargetSystem` 已删(79b3bd86) |
| **物理 3D** | Jolt(ON) vs PhysX(OFF)，均在 `IPhysics3DSystem` 接口后 | ✅ 可插拔后端 | Jolt 为出货路径；PhysX 保留为可选 |
| **脚本** | Lua(ON) vs C#/CoreCLR(OFF) | 🟡 C# 未启用 | 收尾若不做 C#，可标注"实验" |
| **模型导入** | Assimp(ON, FBX/OBJ) vs 内置 glTF-only | ✅ 编译期取舍 | 保留 |
| **粒子** | `engine/render` GPUParticleSystem(3D GPU) + `gameplay_3d/particles` particle3d_system + `gameplay_2d/particle` | ⚠️ 三处实现，分层但职责有重叠 | **建议梳理**：明确 3D 走 GPU、2D 走 CPU 的边界，避免后续混淆 |
| **间接光/GI** | DDGI(实时) + Lightmap(离线烘焙) + LightProbe + ReflectionProbe + GlobalSDF | ⚠️ 多套并存，部分互补部分重叠 | **建议出一份"何时用哪套 GI"的说明**；确认默认 profile 只启用一致的一套 |
| **渲染分发** | engine 核心渲染图 + gameplay_2d/rendering + gameplay_3d/rendering(mesh_render_system) | ✅ 分层，非竞争 | 保留 |

### 4.2 半成品 / 实验 / 默认关闭（收尾需明确定性）

| 特性 | 开关/接线 | 定性 | 收尾建议 |
|:-----|:----------|:-----|:---------|
| **VirtualGeometry** (Nanite 式) | `DSE_ENABLE_VIRTUAL_GEOMETRY=OFF`，未接帧管线 | 🔬 预研 | 明确标注"实验特性"，保留不接入 |
| **WebGPU 后端** | `web-*-3d` 预设强制 ON（桌面原生无 Dawn，仍 OFF）；emsdk **3.1.64** | 🟢 v1 已启用 | Web 目标；`web-debug-3d` 已编入并链接（7 RHI 目标进 wasm） |
| **PhysX** | `DSE_ENABLE_PHYSX=OFF` | 🟡 可选后端 | Jolt 出货，PhysX 保留 |
| **C# 脚本** | `DSE_ENABLE_CSHARP=ON`（.NET 8 hostfxr/nethost） | 🟢 v1 已启用 | 静态 ctest 10/10 + 共享构建 P/Invoke 端到端验证 |
| **网络 / HTTP** | `DSE_ENABLE_NET=ON` / `DSE_ENABLE_HTTP=ON` | 🟢 v1 已启用 | GNS+libsodium / IXWebSocket+OpenSSL；`get_response` 契约 bug 已修(a5be7942) |
| **Spine(2D 骨骼)** | `DSE_ENABLE_SPINE=ON` | 🟢 v1 已启用 | spine-cpp 运行时，构建+冒烟通过 |
| **Meshlet cluster** | `render_pass_context` 有 `meshlet_enabled` 标志，GPU 驱动路径已接 | 🟢 已接线、按需激活 | 确认真实资产链路验证过 |
| **Impostor / GlobalSDF / VirtualTexture / GeometryClipmap** | 均为 gameplay_3d_module 成员并注册 SL，按组件激活 | 🟢 已接线 | Impostor 已补生命周期冒烟(smoke)；GlobalSDF/VT 已有单测 |
| **AssetBuilder(离线工具)** | 独立可执行 | ✅ 链接问题已修(3cf0056b) | — |

### 4.3 立即转发 CommandBuffer（架构性限制，非 bug）
- 现状：四后端命令调用即执行，**无法多线程录制、无法命令复用**；Vulkan 原生录制能力被浪费。
- 定性：Alpha 阶段务实选择，调试直观。报告建议 Beta 阶段再把 Vulkan 升级为真录制（GL/DX11 保持立即转发）。
- **当前决定：保持现状**（你已确认 P3 暂不做）。

---

## 五、收尾清单建议（按优先级）

1. **已完成**：HTTP 契约修复、删冗余 MorphTargetSystem、AssetBuilder 链接、注释订正。
2. **建议做（低风险、收敛心智模型）**：
   - 梳理**粒子三实现**边界，写一页"2D/3D 粒子职责"说明。
   - 梳理**多套 GI**（DDGI/Lightmap/Probe/SDF）的适用场景 + 默认 profile 实际启用哪套。
   - 给 VirtualGeometry / WebGPU / C# / Spine / PhysX 统一打上"实验/可选"标签（代码注释或 docs），避免收尾后被误认为半成品。
3. **建议补测试（守护）**：Meshlet、Impostor、VirtualTexture、GlobalSDF 这些"已接线但要资产"的路径，各补一个最小冒烟，防回归。
4. **明确不做（Beta 再议）**：CommandBuffer 多线程录制重写。

---

## 六、粒子三实现职责边界

引擎存在三套粒子实现，**互不重叠、各司其职**，并非重复造轮子：

| 实现 | 位置 | 驱动方式 | 适用场景 | 备注 |
|:-----|:-----|:---------|:---------|:-----|
| **GPU 粒子** | `engine/render/particles/gpu_particle_system.*` | Compute Shader 全 GPU（双缓冲 SSBO + atomic counter + indirect draw，无 GPU→CPU 回读） | 大规模（数万+）视觉特效：火焰/烟雾/魔法/环境粒子，含重力/风/涡旋/噪声力场、color/size-over-life、发射形状、碰撞平面 | 需要 Compute 能力；WebGL2 无 Compute 时不可用（Web 走 CPU 路径） |
| **3D 游戏粒子** | `modules/gameplay_3d/particles/particle3d_system.*` | CPU（`Update(World&, dt)`），逐 `ParticleSystem3DComponent` 发射/积分 | 中小规模、需与 3D 场景/资产（AssetManager）耦合、可能参与游戏逻辑的粒子 | 平台无关，作为 GPU 粒子的回退与逻辑耦合路径 |
| **2D 游戏粒子** | `modules/gameplay_2d/particle/particle_system.*` | CPU，2D 空间 | 2D 游戏/UI 特效（`particle_2d.h` 组件） | 与 3D 完全隔离，2D 管线专用 |

**选择规则**：3D 大规模纯视觉 → GPU 粒子；3D 需逻辑耦合/无 Compute → 3D 游戏粒子；2D → 2D 粒子。三者组件类型不同（`GpuParticle*` / `ParticleSystem3DComponent` / 2D 组件），不会同时作用于同一实体。

## 七、GI（全局光照）多套方案适用场景

引擎并存多套 GI，**面向不同光照需求与预算**，按场景选用（非互斥的"半成品堆叠"）：

| 方案 | 位置 | 类型 | 适用场景 | 预算 |
|:-----|:-----|:-----|:---------|:-----|
| **DDGI**（动态漫反射 GI） | `engine/render/gi/ddgi_system.*` | 实时探针体积 | 动态光照/昼夜变化的大场景漫反射间接光 | 中高（每帧探针更新） |
| **Lightmap**（烘焙光照贴图） | `engine/render/gi/lightmap_system.*` + `lightmap_baker.*` | 离线烘焙 | 静态几何的高质量间接光，运行时零开销 | 离线烘焙，运行时低 |
| **LightProbe**（光照探针） | `engine/render/light_probe_system.*` | 探针插值 | 动态物体在静态环境中的漫反射着色 | 低 |
| **ReflectionProbe**（反射探针） | `engine/render/reflection_probe_system.*` | 立方体贴图反射 | 镜面/光泽反射（补充漫反射 GI） | 中（cubemap 捕获） |
| **GlobalSDF**（全局有向距离场） | `engine/render/sdf/global_sdf.*` | 距离场 | 软阴影 / SDF AO / 光线步进反射等，作为其它 GI 的几何查询底座 | 中 |

**选择规则**：静态场景优先 **Lightmap**（质量+性能最佳）；动态/开放世界漫反射 **DDGI**；动态物体着色 **LightProbe**；镜面反射 **ReflectionProbe**；软阴影/AO/步进查询 **GlobalSDF**。它们分层互补：Lightmap/DDGI 提供漫反射间接光，ReflectionProbe 提供镜面，Probe 服务动态物体，SDF 提供几何距离查询。默认 profile 应按目标平台只启用一套漫反射方案（静态=Lightmap，动态=DDGI），避免重复计算。

---

## 八、本次收尾落地（feature/engine-lib）

- **五个 v1 必备开关全部启用并验证**：Spine ✓ / HTTP ✓ / Net ✓ / C# ✓（静态 ctest 10/10 + 共享构建 P/Invoke 端到端）/ WebGPU ✓（`web-debug-3d` 预设，emsdk 3.1.64，7 个 WebGPU RHI 目标编入 `index.wasm` 并链接 `-lwebgpu`）。
- **WebGPU 启用过程中修复 5 个此前未被 CI 验证过的 web-3d 构建缺陷**（CI web job 因额度未真实运行）：
  1. `frame_pipeline.cpp` 无物理后端时缺 `IPhysics3DSystem` 完整定义 → 始终包含接口头；
  2. `video_decoder_plmpeg.cpp` 在 libc++ 下缺 `<cstddef>/<cstdio>`；
  3. `components_3d_physics.h` 中 Ragdoll/Vehicle/Buoyancy 组件被物理宏门控（其兄弟 SoftBody/Rope 却无条件）→ 统一为无条件纯数据结构；
  4. `dse_api_gameplay3d.cpp` 中上述三者的 C ABI 访问器被 `DSE_HAS_PHYSICS3D` 门控（纯组件访问，不含物理调用）→ 无条件编译；
  5. `PhysicsLODSystem`（纯距离 LOD 逻辑，无 Jolt/PhysX 依赖）被物理排除规则误删 → 无物理构建时仍编入。
- **守护测试**：新增 `tests/gtest/smoke/impostor_system_smoke_test.cpp`（Impostor 烘焙→出批次→RenderOpaque→Shutdown 全生命周期，无 GL 环境自动 SKIP，与其余 GL 冒烟一致）。Meshlet/VirtualTexture/GlobalSDF 经核实已有单测覆盖。

*说明：架构梳理部分为现状记录；本次另含上述实际代码改动（均已在 feature/engine-lib 提交）。*
