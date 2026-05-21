# DSEngine 多线程并行化方案

> **一句话：利用多核 CPU 资源，降低主循环每帧耗时，提高 FPS 帧率，不改变画质、不省显存、不加速磁盘。**

> 最后更新：2026-05-21（Phase 0 ✅ + Phase 1 ✅ 已完成并推送。下一步：Phase 3 Pass 并行录制或 Phase 2 ECS System 并行）

## 一、问题现状

### 1.1 当前的线程模型

DSEngine 的运行时主循环位于 `EngineInstance::Run()` / `Tick()`，是严格的**串行流水线**：

```
Tick() — 单线程
├── FixedUpdate(dt)     // 物理（写 ECS: Transform, RigidBody）
│     ├── FixedUpdateGameplay2D
│     ├── modules OnFixedUpdate
│     └── Physics3D.FixedUpdate → 写回 ECS
│
├── Update(dt)          // 逻辑（写 ECS: 脚本、动画、AI）
│     ├── TickBusinessRuntime (Lua)
│     ├── RunRuntimeUpdateGraph
│     │     ├── UpdateGameplay2D
│     │     ├── modules OnUpdate
│     │     └── UpdateGameplay3D
│     └── Streaming / HotReload
│
└── Render()            // 渲染（读 ECS: 38 处 registry::view()）
      ├── BeginFrame
      ├── CollectLights / ClusterGrid / DDGI / GPU Driven 准备
      └── ExecuteRenderGraph
            ├── PreZPass
            ├── ShadowPass
            ├── ForwardScenePass
            ├── PostProcessPasses（Bloom / TAA / FXAA / AutoExposure / SSR / DOF...）
            └── PresentPass
```

帧时间 = `FixedUpdate(≈2ms)` + `Update(≈4ms)` + `Render(≈8ms)` = **≈14ms → ~71 FPS**

### 1.2 已有的多线程能力

DSEngine 已经具备一个完整的 `JobSystem`（位于 [`engine/core/job_system.h`](../../engine/core/job_system.h)）：

- 基于线程池（`硬件并发数 - 1` 个 worker）
- 三级优先级（High / Normal / Low）
- 工作窃取（work stealing）减少竞争
- `JobHandle` 和 `SubmitWithDependency` 支持任务依赖链
- 通过 `ServiceLocator` 管理生命周期

但目前 JobSystem **只用于异步资源加载**（`AssetManager::LoadTextureAsync` 等），**核心游戏循环没有使用 JobSystem**。

### 1.3 被动的"多线程"来源

当前 DSE 中已有的一些并行来源，但它们不是由引擎主动调度的：

| 来源 | 说明 | 是否受引擎控制 |
|------|------|:------------:|
| GPU 驱动 | GPU 驱动内部有自己的提交线程 | ❌ 透明 |
| PhysX/Jolt 物理 | 物理引擎内部有 worker 线程 | ❌ 透明 |
| 资源加载 JobSystem | 通过 `AssetManager::Load*Async` 使用 | ✅ 主动 |

---

## 二、优化目标：这个方案在优化什么？

一句话：**这个方案优化的是 CPU 端的帧时间，而不是 GPU 端的渲染质量。**

### 2.1 优化的对象

```
DSE 一帧的 CPU 工作分解：

  FixedUpdate     Update        Render（CPU 部分）     GPU 执行
  ──────────      ──────        ──────────────────     ────────
  物理计算        脚本/动画      顶点数据上传             像素填充
  ECS 组件写入    ECS 读/写      Draw Call 提交          Shader 执行
  碰撞检测        AI/寻路        CommandBuffer 录制      纹理采样
                                RenderPass 切换

  2ms             4ms           8ms                    ~4~16ms（取决于场景）

  ↑ 这个方案优化这里                               ↑ 这个方案不动这里
```

**方案只优化 CPU 部分。** GPU 部分（分辨率、画质、shader 复杂度、后处理链长度）不在方案范围内。

### 2.2 优化的本质：填满空闲

当前的问题是：**CPU 的时间线是串行的，导致大部分时间只有 1 个核在工作，其他核在闲着。**

```
当前（14ms，71 FPS）：     Core 0: [FixUpd][Update][────Render────]   ← 只有 1 个核忙
                           Core 1: [idle][idle][idle][idle][idle]    ← 其他核闲着
                           Core 2: [idle][idle][idle][idle][idle]
                           Core 3: [idle][idle][idle][idle][idle]

Phase 1 后（8ms，125 FPS）：Core 0: [FixUpd][Update]                 ← 主线程
                            Core 1:        [────Render(上一帧)────]  ← 渲染线程
                            Core 2: [idle][idle][idle][idle][idle]
                            Core 3: [idle][idle][idle][idle][idle]
                            ↑ 原来 Core 1 的空闲时间被利用了

Phase 1+2 后（6ms，166 FPS）：Core 0: [FixUpd][UpdA]                ← 主线程
                              Core 1:  [UpdB][Render(上一帧)]        ← 渲染线程 + ECS 并行
                              Core 2:  [UpdC]                       ← ECS System 并行
                              Core 3:  [UpdD]                       ← ECS System 并行
                              ↑ ECS 的独立 System 分布到空闲的 2/3 号核

Phase 全量后（4ms，250 FPS）：Core 0: [FixUpd][UpdA]                ← 主线程
                              Core 1:  [UpdB][Pass1][Pass4]         ← 渲染线程 + Pass 录制
                              Core 2:  [UpdC][Pass2]                ← ECS 并行 + Pass 录制
                              Core 3:  [UpdD][Pass3][Pass5]         ← ECS 并行 + Pass 录制
                              ↑ Pass 级并行把 Render 也拆到多核
```

**这不是让 CPU 算得更快，而是让 CPU 的空闲核心有事做。**

### 2.2b 各阶段的核利用率

| 阶段 | 用了几核 | 哪几个核在做什么 |
|------|:-------:|----------------|
| 当前 | **1 核** | 一个核跑完所有事，其他核空闲 |
| Phase 1 | **2 核** | 主核 Update + 另一核 Render |
| Phase 1+2 | **3~4 核** | 主核 FixedUpdate + 2~3 个核并行 ECS System |
| Phase 1+2+3 | **4~6 核** | 主核 + ECS 并行核 + Pass 并行录制核 |
| Phase 全量 | **4~8 核** | 主核 + ECS 并行核 + Pass 并行核 + GPU Driven 分布 |

**"只有 2 核"只是 Phase 1。全量方案在设计上能利用 4~8 核。** 具体能利用多少取决于你的 CPU 有多少核——JobSystem 池大小是 `hardware_concurrency - 1`。

### 2.3 这个方案不优化什么

| 方案不做的 | 原因 |
|-----------|------|
| ❌ **降低 GPU 负载** | 不减少三角形数量、不降低分辨率、不减少后处理 Pass |
| ❌ **优化 GPU 渲染时间** | Bloom/TAA/SSR/DOF 的 GPU 执行时间不动 |
| ❌ **减少显存占用** | Texture/Buffer/RT 的显存使用不变 |
| ❌ **降低内存占用** | ECS 存储、Asset 缓存的内存使用不变 |
| ❌ **加速磁盘 I/O** | 资源加载、流式加载速度不变 |
| ❌ **加速物理模拟** | PhysX/Jolt 内部计算速度不变 |

### 2.4 什么时候能看到收益

**CPU 是瓶颈时收益最大，GPU 是瓶颈时没有收益。**

| 你的游戏是什么情况 | 这个方案给你什么 |
|-------------------|----------------|
| CPU 瓶颈（帧率低但 CPU 使用率高、GPU 使用率低） | **帧率翻倍**（Phase 1 即可） |
| GPU 瓶颈（画质拉满、GPU 99% 占用） | **没有帧率收益**（瓶颈在 GPU，不在 CPU） |
| 平衡（CPU 和 GPU 使用率都 ~70%） | **帧率有一定提升，但 GPU 会变成新瓶颈** |
| 需要更高帧率但不想降画质 | **此方案可以帮你释放 CPU 瓶颈** |

### 2.5 FPS 不是唯一收益

除了 FPS，这个方案还有一个隐性收益：**帧时间稳定性**。

```
当前串行：
  帧 N:  Update 突然变重（新角色加入、大量 Lua 脚本）
         → 帧 N 的 Update + Render 都延迟
         → 帧 N 卡顿，帧 N+1 恢复正常

Phase 1 延迟一帧：
  帧 N:  Update 突然变重 → 帧 N 的 Render 不受影响（它读的是帧 N-1 的快照）
         → 帧 N 不卡。卡顿推迟到帧 N+1（旧数据 + 当前渲染）
         → 视觉上更平滑（"卡顿分摊"效果）
```

**结论：这个方案优化的是 CPU 帧时间，主要表现为 FPS 提升和帧时间稳定。它不做画质优化、不省显存、不降 GPU 负载。**

---

## 三、总体架构：四阶段并行

整个方案分为四个递进阶段，每个阶段在前一阶段基础上叠加一个并行维度：

```
阶段            并行维度                          主要收益
───            ──────                            ──────
Phase 0        GPU Driven 默认化                 ≈ 0%（但为后续清除障碍）
Phase 1        流水线并行（Update // Render）     1.5~2.0x
Phase 2        ECS System 并行                  1.2~1.5x（叠加）
Phase 3        Render Pass 并行录制             1.2~1.4x（叠加）
```

---

## 三、Phase 0：GPU Driven 默认化（✅ 基本完成，剩余验证约 1 天）

### 3.1 背景

DSEngine 已有完整的 GPU Driven 渲染基础设施，**三后端（GL/VK/DX11）已全部实现并对齐**：

| 组件 | 位置 | 状态 |
|------|------|------|
| GPUInstanceData / DrawElementsIndirectCommand | [`engine/render/rhi/gpu_scene_types.h`](../../engine/render/rhi/gpu_scene_types.h) | ✅ 已定义 |
| MegaVAO / Instance SSBO / Indirect Draw | [`engine/render/rhi/rhi_gpu_driven.h`](../../engine/render/rhi/rhi_gpu_driven.h) | ✅ 接口已定义，**GL/VK/DX11 三端均有完整实现** |
| PrepareGPUScene | [`engine/runtime/frame_pipeline.cpp`](../../engine/runtime/frame_pipeline.cpp#L1042) | ✅ 已调用 |
| GPU Driven 分支路径 | [`engine/render/passes/builtin_passes.cpp`](../../engine/render/passes/builtin_passes.cpp#L683-L710) | ✅ 条件已就绪 |
| Hi-Z Occlusion Culling | GL/VK/DX11 三端 | ✅ 三端均有完整实现 |
| GPU Cull Compute Shader | [`builtin_passes.cpp`](../../engine/render/passes/builtin_passes.cpp) GPUCullPass | ✅ 已实现 |

### 3.2 为什么需要这一步

`ForwardScenePass::Execute` 中存在一个关键的回调路径：

```cpp
// builtin_passes.cpp:710
if (!use_gpu_indirect) {
    if (ctx_.modules.empty() && ctx_.render_meshes) {
        ctx_.render_meshes(*ctx_.world, cmd_buffer);
    }
}
```

`render_meshes` 回调内部直接遍历 ECS 的 `MeshRenderComponent` 来提交绘制。如果 Phase 1 要让 Render 在另一个线程执行，但这个回调还在读 ECS，就会和主线程的 Update 写 ECS 冲突。

**让 GPU Driven 成为默认路径后，这个回调不再执行**，ECS 的 mesh 渲染数据由 `PrepareGPUScene`（在 Render 准备阶段）一次性提取到 GPU 缓冲区中。

### 3.3 三后端当前状态

**✅ GL/VK/DX11 三端的 `IRhiGpuDriven` 接口已全部实现并对齐**（包括 CreateMegaVAO、UpdateMegaVBO/IBO、BindMegaVAO、MultiDrawIndexedIndirect、CreateStaticMeshVAO、Hi-Z、ReadSSBO 等）。

`gpu_driven_supported` 运行时检测逻辑已就位（检查 `SupportsCompute` + `SupportsIndirectDraw` + `SupportsSSBO`），三后端均返回 `true`。

### 3.4 剩余工作

| 文件 | 改动 |
|------|------|
| 端到端验证 | 运行同样场景，确认三后端 GPU Driven 路径绘制结果与 CPU 路径一致 |
| `frame_pipeline.cpp` | 确认 `gpu_driven_supported` 默认开启时无回归 |

### 3.5 风险 & 验收

| 项 | 内容 |
|------|------|
| 风险 | 极低——只改条件开关，不改架构 |
| 验收 | 运行同样的场景，GPU Driven 路径下的绘制结果与 CPU 路径完全一致 |
| 技术债 | 零 |

---

## 四、Phase 1：薄快照 + 延迟一帧流水线并行（✅ 已完成 2026-05-21）

### 4.1 设计思想

借鉴 Unreal Engine、Unity、Godot 4 等主流引擎的成熟做法：**将 Render 需要的 ECS 数据提前提取为一份极小的"薄快照"，渲染线程只读这份快照，不碰 ECS 本身。由于快照是上一帧的数据，写入线程（主线程 Update）和读取线程（渲染线程 Render）天然无竞争——零锁。**

```
帧 N  主线程:  FixedUpdate(N) → Update(N) → Capture(N) → [等待上一帧 Render] → 下一帧

帧 N  渲染线程:                                               Render(N-1 快照) → 提交 GPU
```

### 4.2 为什么可以只快照 < 1KB——详细证据

Render 中所有对 ECS 的读取可以分为两类：

#### 第一类：渲染准备阶段（RunRenderInternal 中执行）

这些数据**已在 Phase 0 的 GPU Driven 路径中被提取到 GPU 缓冲区**，Render Pass 通过 SSBO/UBO 读取，不再需要 ECS：

| 数据 | 提取时机 | 存储位置 |
|------|---------|---------|
| DirectionalLight3DComponent | `light_buffer_.CollectLights()` → Upload | GPU SSBO |
| SpotLightComponent | `light_buffer_.CollectLights()` → Upload | GPU SSBO |
| PointLightComponent | `light_buffer_.CollectLights()` → Upload | GPU SSBO |
| GIProbeVolumeComponent | DDGI 系统初始化/更新 | DDGI atlas 纹理 |
| MeshRenderComponent | `PrepareGPUScene()` → Upload | GPU Instance/Material SSBO |
| Transform（mesh） | `PrepareGPUScene()` → Upload | GPU InstanceData SSBO |
| HiZAABB | `CachedAABBs()` → Upload | GPU AABB SSBO |

这些在 `RunRenderInternal()` 中已经完成，**不需要快照**。

#### 第二类：ExecuteRenderGraph 中 Pass 直接读的 ECS

这些才是真正需要快照的部分：

| Pass | 读的 ECS 组件 | 每帧读取次数 | 一次读取的实体数 |
|------|-------------|:----------:|:--------------:|
| PreZPass | Camera3DComponent + TransformComponent | 1 次 | 1 |
| ShadowPass | DirectionalLight3DComponent | 1 次 | ~1~3 |
| ShadowPass | SpotLightComponent + TransformComponent | 1 次 | ~0~8 |
| ShadowPass | PointLightComponent + TransformComponent | 1 次 | ~0~8 |
| ForwardScenePass | Camera3DComponent + TransformComponent | 1 次 | 1 |
| ForwardScenePass | SkyboxComponent + TransformComponent | 1 次 | ~0~1 |
| PostProcessPass | PostProcessComponent | 1 次 | 1 |
| BloomPass | PostProcessComponent | 1 次 | 1 |
| TAA Pass | PostProcessComponent | 1 次 | 1 |
| AutoExposurePass | PostProcessComponent | 1 次 | 1 |
| SSR Pass | PostProcessComponent | 1 次 | 1 |
| DOF Pass | PostProcessComponent | 1 次 | 1 |
| 2D Camera fallback | CameraComponent | 1 次 | 1 |

**总计：约 30+ 次查询（含重复），每次读取 1~3 个实体，总数据量 < 2KB。**

关键观察：**同一个 PostProcessComponent 被 7 个不同的 Pass 各查了一次**——这正是快照的价值：查一次，多处复用。

### 4.3 数据结构

```cpp
// engine/render/render_snapshot.h（新建）

#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dse {
namespace render {

/// 渲染线程所需的 ECS 数据薄快照（总量 < 2KB）
/// 包含相机、天空盒、灯光（含阴影投射者）、后处理等。
/// 由主线程在 CaptureThinSnapshot() 中填充，渲染线程只读。
struct RenderThinSnapshot {
    // ── 3D 相机 ──
    struct Camera3D {
        bool valid = false;
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 position;
    } camera_3d;

    // ── 天空盒 ──
    struct Skybox {
        bool valid = false;
        unsigned int cubemap_handle = 0;
        glm::quat rotation;
        bool entity_has_transform = false;  // lua 中是否旋转了天空盒
    } skybox;

    // ── 方向光（CSMShadowPass + ForwardScenePass 读取）──
    struct DirectionalLight {
        bool valid = false;
        bool cast_shadow = false;
        glm::vec3 direction{0.0f, -1.0f, 0.0f};
        glm::vec3 color{1.0f};
        float intensity = 1.0f;
        float ambient_intensity = 0.1f;
        float shadow_strength = 1.0f;
    } directional_light;

    // ── 聚光灯（SpotShadowPass 读取，最多 4 盏投射阴影）──
    static constexpr int kMaxSpotShadowLights = 4;
    struct SpotLight {
        bool valid = false;
        bool cast_shadow = false;
        glm::vec3 position;
        glm::vec3 direction;
        float cutoff_angle = 30.0f;
        float range = 50.0f;
    };
    SpotLight spot_lights[kMaxSpotShadowLights];
    int spot_light_count = 0;

    // ── 点光源（PointShadowPass 读取，最多 4 盏投射阴影）──
    static constexpr int kMaxPointShadowLights = 4;
    struct PointLight {
        bool valid = false;
        bool cast_shadow = false;
        glm::vec3 position;
        float range = 50.0f;
    };
    PointLight point_lights[kMaxPointShadowLights];
    int point_light_count = 0;

    // ── 后处理（合并多个 Pass 的重复查询）──
    struct PostProcess {
        bool taa_enabled = false;
        float taa_blend_factor = 0.1f;
        bool fxaa_enabled = false;
        bool bloom_enabled = false;
        bool auto_exposure_enabled = false;
        bool ssr_enabled = false;
        bool dof_enabled = false;
    } postprocess;

    // ── 2D 相机 fallback ──
    struct Camera2D {
        bool valid = false;
        glm::mat4 view;
        glm::mat4 projection;
    } camera_2d;
};

} // namespace render
} // namespace dse
```

### 4.4 CaptureThinSnapshot 实现

在 `FramePipeline` 中添加：

```cpp
// frame_pipeline.h — 新增
#include "engine/render/render_snapshot.h"

// 双缓冲快照
RenderThinSnapshot snapshot_pool_[2];
int snapshot_write_idx_ = 0;
int snapshot_read_idx_  = 1;

RenderThinSnapshot& write_snapshot() { return snapshot_pool_[snapshot_write_idx_]; }
const RenderThinSnapshot& read_snapshot() const { return snapshot_pool_[snapshot_read_idx_]; }

void CaptureThinSnapshot(World& world, RenderThinSnapshot& out);
void FlipSnapshotIndex();
```

```cpp
// frame_pipeline.cpp — CaptureThinSnapshot 实现（~60 行）
void FramePipeline::CaptureThinSnapshot(World& world, RenderThinSnapshot& out) {
    auto& registry = world.registry();

    // 1. 相机（3D）
    {
        auto view = registry.view<dse::Camera3DComponent>();
        // ... 与 Pass 中相同的相机选择逻辑 ...
        // 选中一个相机后，计算 view/projection，写入 out.camera_3d
    }

    // 2. 天空盒
    {
        auto view = registry.view<dse::SkyboxComponent>();
        // ... 读取第一个 enabled 的 SkyboxComponent ...
    }

    // 3. 后处理
    {
        auto view = registry.view<dse::PostProcessComponent>();
        // ... 读取第一个 enabled 的 PostProcessComponent ...
    }

    // 4. 相机（2D fallback）
    {
        auto view = registry.view<CameraComponent>();
        // ... 选中一个相机，写入 out.camera_2d ...
    }
}
```

### 4.5 Pass 改造

每个 Pass 的 Execute 中的 ECS 查询替换为 snapshot 读取，以 `ForwardScenePass` 为例：

**改造前**（约 50 行 ECS 查询 + view/projection 计算 + 天空盒读取）：

```cpp
// builtin_passes.cpp ~590-670
void ForwardScenePass::Execute(CommandBuffer& cmd) {
    // 编辑器相机
    if (ctx_.editor_mode && ctx_.use_editor_camera) {
        cmd.SetCamera(ctx_.editor_view, ...);
        // 读 skybox...
    } else {
        // 读 Camera3DComponent（~20 行）
        auto camera3d_view = ctx_.world->registry().view<dse::Camera3DComponent>();
        // ... 选中相机、计算 view/projection ...
        // 读 TransformComponent
        // 读 SkyboxComponent（~15 行）
        // 读 CameraComponent fallback（~15 行）
    }
    // ...
}
```

**改造后**（约 10 行）：

```cpp
void ForwardScenePass::Execute(CommandBuffer& cmd) {
    const auto& snap = *ctx_.snapshot;

    if (ctx_.editor_mode && ctx_.use_editor_camera) {
        cmd.SetCamera(ctx_.editor_view, ...);
        // 编辑器模式下的天空盒处理
    } else if (snap.camera_3d.valid) {
        cmd.SetCamera(snap.camera_3d.view, snap.camera_3d.projection);
        if (snap.skybox.valid) {
            if (snap.skybox.entity_has_transform) {
                glm::mat4 sky_rot = glm::mat4_cast(glm::conjugate(snap.skybox.rotation));
                cmd.SetCamera(snap.camera_3d.view * sky_rot, snap.camera_3d.projection);
            }
            cmd.DrawSkybox(snap.skybox.cubemap_handle);
            cmd.SetCamera(snap.camera_3d.view, snap.camera_3d.projection);
        }
    } else if (snap.camera_2d.valid) {
        // 2D fallback
    }
    // ... 后续渲染逻辑不变 ...
}
```

**涉及修改的 Pass：**

| Pass | 文件位置 | 简化前的 ECS 行数 |
|------|---------|:---------------:|
| PreZPass | builtin_passes.cpp:129-170 | ~30 |
| ShadowPass (方向光) | builtin_passes.cpp:177-230 | ~40 |
| ShadowPass (聚光) | builtin_passes.cpp:238-285 | ~40 |
| ShadowPass (点光) | builtin_passes.cpp:289-360 | ~50 |
| ForwardScenePass | builtin_passes.cpp:570-710 | ~80 |
| PostProcessPass | builtin_passes.cpp:760-800 | ~10 |
| BloomPass | builtin_passes.cpp:858 | ~5 |
| TAA Pass | builtin_passes.cpp:1277 | ~5 |
| AutoExposurePass | builtin_passes.cpp:978 | ~5 |
| SSR Pass | builtin_passes.cpp:1044 | ~5 |
| DOF Pass | builtin_passes.cpp:1569 | ~5 |
| OutlinePass | builtin_passes.cpp:1310-1400 | ~30 |
| FogPass | builtin_passes.cpp:1440-1470 | ~20 |

**总计：约 300 行 ECS 相关代码 → 替换为约 150 行 snapshot 读取代码。**

### 4.6 RenderPassContext 改造

```cpp
// render_pass_context.h
struct RenderPassContext {
    // 新增
    const RenderThinSnapshot* snapshot = nullptr;   // 渲染线程只读

    // 保留但 Pass 不再直接读
    World* world = nullptr;                           // Phase 1 后 Pass 不读写 world

    // 其余字段不变
    // ...
};
```

### 4.7 主循环改造

```cpp
// engine_app.cpp — EngineInstance::Run() 改造后

EngineInstance::Run() {
    // ... 初始化 ...

    // 创建 render_fence（std::future<void> 或原子计数器）
    std::shared_ptr<std::atomic<bool>> render_done = std::make_shared<std::atomic<bool>>(true);

    while (!platform_->ShouldClose()) {
        platform_->PollEvents();

        // ═══ 串行阶段（必须） ═══
        FixedUpdate(dt);
        Update(dt);

        // 快照提取（< 0.01ms，主线程）
        pipeline_->CaptureThinSnapshot(world(), pipeline_->write_snapshot());

        // 等待上一帧渲染完成
        while (!render_done->load(std::memory_order_acquire)) {
            // spin-wait（极短，或加 std::this_thread::yield()）
        }

        // 切换快照缓冲区（指针交换，零拷贝）
        pipeline_->FlipSnapshotIndex();

        // 启动渲染线程
        *render_done = false;
        job_system_->Submit([this, render_done]() {
            pipeline_->RenderFromSnapshot(pipeline_->read_snapshot());
            render_done->store(true, std::memory_order_release);
        }, JobPriority::High);

        // 主线程继续！不需要等渲染完成
    }
}

// FramePipeline 新增
void FramePipeline::RenderFromSnapshot(const RenderThinSnapshot& snap) {
    render_pass_context_.snapshot = &snap;     // 指向只读快照
    RunRenderInternal();                        // 原有渲染代码不变
    // RunRenderInternal 结束时 render_pass_context_.snapshot 不再使用
}
```

### 4.8 三后端的处理

| 后端 | 渲染线程兼容性 | 处理方式 |
|------|:------------:|---------|
| Vulkan | ✅ 原生支持 | `vkCmd*` 在任意线程调用安全，提交回主线程 `vkQueueSubmit`（需加 mutex） |
| D3D11 | ⚠️ 有限支持 | `ID3D11DeviceContext` 非线程安全。方案：在 JobSystem worker 中录制命令，主线程执行 `Submit`+`EndFrame`；或使用 Deferred Context |
| OpenGL | ❌ 不支持 | GL 走"延迟数据 + 主线程提交"路径。Render 的数据使用上一帧的 snapshot，但执行仍在主线程。这样安全性已提升（不读正在被写的 ECS），但 CPU 并行不受益 |

### 4.9 同步模型

```
主线程:                       渲染线程:
  CaptureThinSnapshot(N)
  FlipSnapshotIndex()         [可能还在跑 Render(N-1)]
  render_done.wait()  ← 等待   Render(N-1) 完成 → render_done = true
  Submit(Render(N))           Render(N) 开始（读 snapshot[N-1] 改为读 snapshot[N] 了...等等）
```

这里有一个细节：**FlipSnapshotIndex 之后，渲染线程读的是 snapshot[上一帧的 read_idx]，但它已变成当前 write_idx 的内容**。

更精确的做法是**三级缓冲**（triple buffering write/read/submit）：

```cpp
// 三级快照缓冲
RenderThinSnapshot snapshot_pool_[3];
int write_idx_ = 0;   // 主线程正在填充
int read_idx_  = 1;   // 渲染线程正在读取
int done_idx_  = 2;   // GPU 已完成

// 主线程
CaptureThinSnapshot(write_snapshot());
// 等待渲染线程完成前一个读取
while (/* read_idx_ 被占用 */) { }
std::swap(write_idx_, read_idx_);

// 渲染线程
Render(read_snapshot());
// 渲染完成后，标记 done_idx_ 可被回收
```

简化起见，**双缓冲 + fence 已经足够**，fence 等待时间 <= 1 帧：

```
帧 N 主线程:  Capture(N) → Flip → wait(Render(N-1)) → Submit(Render(N))
帧 N 渲染线程:                                        Render(N-1 快照)
```

等待时间通常接近 0（渲染线程还没开始跑下一帧），最坏情况接近一帧渲染时间。

### 4.10 收益预测

> **注意**：并行帧时间 = `max(FixedUpdate + Update, Render)`。当 Render > Update 时，Render 是瓶颈，Phase 1 的帧率提升受限。

| 场景 | 串行帧时间 | 并行帧时间 | 加速比 | 瓶颈 |
|------|:---------:|:---------:|:-----:|:----:|
| 轻量（几十物体, Render 3ms） | 2+4+3=9ms | max(6, 3)=6ms | 1.5x | Update |
| 中等（几百物体, Render 6ms） | 2+4+6=12ms | max(6, 6)=6ms | 2.0x | 平衡 |
| **典型**（几百物体, Render 8ms） | 2+4+8=14ms | max(6, 8)=**8ms** | **1.75x** | **Render** |
| 重度（几千物体, Render 10ms） | 2+4+10=16ms | max(6, 10)=10ms | 1.6x | Render |

> ⚠️ 典型场景下 Phase 1 并行帧时间是 **8ms（125 FPS）** 而非 6ms，因为 Render 耗时超过 Update。要进一步提升需要 Phase 3（Pass 并行录制）压缩 Render 耗时。

### 4.11 改动清单（✅ 已完成）

| 文件 | 操作 | 状态 |
|------|------|:----:|
| `engine/render/render_snapshot.h` | **新建** — 快照结构体含相机/天空盒/光源/阴影/后处理/水面/贴花/LightProbe SH/DDGI | ✅ |
| `engine/runtime/frame_pipeline.h` | 双缓冲快照池 + 线程同步原语 + 7 个新方法声明 | ✅ |
| `engine/runtime/frame_pipeline.cpp` | CaptureThinSnapshot + PrepareRenderFrame/ExecuteRenderFrame + 线程生命周期 | ✅ |
| `engine/runtime/engine_app.cpp` | 注入 context 回调 + SwapBuffers 条件跳过 | ✅ |
| `engine/render/passes/render_pass_context.h` | 添加 snapshot 指针 | ✅ |
| `engine/render/passes/builtin_passes.cpp` | 全部渲染 Pass 从 ECS 查询迁移到快照读取（-613 行 / +194 行） | ✅ |
| `engine/platform/platform_app.h` | 添加 MakeContextCurrent/ReleaseContext 虚方法 | ✅ |
| `engine/platform/glfw/glfw_app.h/cpp` | GL Context 线程管理实现 | ✅ |
| `engine/runtime/runtime_context.h` | 添加 context 管理回调 | ✅ |

**实际统计**：10 files changed, 1219 insertions(+), 616 deletions(-)

### 4.11b 实现细节（与文档规划的差异）

| 规划 | 实际实现 | 说明 |
|------|---------|------|
| JobSystem + atomic fence | std::thread + mutex + condition_variable | 专用渲染线程比 JobSystem 更稳定，避免 worker 抢占 |
| RenderFromSnapshot 包装 | PrepareRenderFrame + ExecuteRenderFrame 拆分 | 更精确的 CPU/GPU 分离 |
| 三级缓冲 | 双缓冲 + WaitForRenderComplete | 双缓冲已足够，WaitDone 延迟接近 0 |
| DSE_RENDER_THREAD=1 环境变量 | ✅ | 编辑器模式自动禁用 |
| 非线程模式完全回退 | ✅ | 零行为变化 |

### 4.12 风险

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| 遗漏某个 Pass 的 ECS 查询 | 中 | 高（data race） | 编译后搜索所有 `registry::view` / `registry::get` 确保全部替换 |
| DX11 Deferred Context 驱动 bug | 中 | 中 | 回退到主线程提交 |
| GL 没有 CPU 并行收益 | 确定 | 低 | 数据安全性已提升（不读活跃 ECS） |

---

## 五、Phase 2：ECS System 级并行（约 2~3 周）

### 5.1 背景

当前 `RunRuntimeUpdateGraph` 串行执行所有 ECS System：

```cpp
// runtime_update_graph.cpp
void RunRuntimeUpdateGraph(::FramePipeline& pipeline, float delta_time) {
    auto& world = pipeline.world();

    pipeline.modules_impl_->UpdateGameplay2D(world, delta_time);     // 写 2D 组件

    for (auto& mod : pipeline.modules_) {                            // 写自定义组件
        if (mod.instance) mod.instance->OnUpdate(world, delta_time);
    }

    pipeline.modules_impl_->UpdateGameplay3D(world, delta_time);     // 写 3D 组件
}
```

这 6ms（典型值）中有多少是可并行的，取决于各个 System 操作的 ECS 组件是否有**交集**。

### 5.2 组件访问分析

每个 System 需要声明它读写哪些组件类型。EnTT 的 `scheduler` 正是为此设计——它分析 System 间组件访问的重叠，自动构建执行 DAG。

**Step 1：提取每个 System 的组件访问集合。**

以 `UpdateGameplay3D` 内部为例（位于 `modules/gameplay_3d/`）：

| ECS System | 读取的组件 | 写入的组件 |
|-----------|-----------|-----------|
| TransformSystem | TransformComponent（读父级） | TransformComponent（写子级） |
| MeshRenderSystem | TransformComponent, MeshRenderComponent | — |
| AnimationSystem | SkeletonComponent, AnimationComponent | TransformComponent（骨骼输出） |
| Particle3DSystem | ParticleSystem3DComponent, TransformComponent | ParticleSystem3DComponent |
| Audio3DSystem | TransformComponent, AudioSourceComponent | — |
| LightSystem | TransformComponent | LightComponent（dirty flag） |

**Step 2：构建冲突矩阵。**

| | Transform | Mesh | Skeleton | Animation | Particle3D | Audio | Light |
|---|---|---|---|---|---|---|---|
| Transform | **W** | — | — | — | — | — | — |
| Mesh | **R** | R | — | — | — | — | — |
| Skeleton | — | — | R | R | — | — | — |
| Animation | **W** | — | **W** | R | — | — | — |
| Particle3D | **R** | — | — | — | **W** | — | — |
| Audio | **R** | — | — | — | — | R | — |
| Light | **R** | — | — | — | — | — | **W** |

**冲突规则**：如果 System A 写组件 T，且 System B 读或写组件 T，则 A 和 B 不能并行。

从矩阵可以得出：

```
可以并行（无重叠组件）：
  MeshRenderSystem ∥ Audio3DSystem ∥ LightSystem
  Audio3DSystem ∥ Particle3DSystem（只要不读写同一个实体的 Transform）
```

**不能并行（共享 TransformComponent 的写操作）：**

```
  TransformSystem：写 Transform（传播父子变换）
  AnimationSystem：写 Transform（骨骼输出）
  → 必须串行（先 TransformSystem，再 AnimationSystem）
```

### 5.3 调度方案：手动 JobSystem DAG

> ⚠️ **注意**：标准 EnTT 发行版**不包含** `scheduler` 类。`entt::organizer` 是实验性 API，不保证稳定。
> DSE 的 `JobSystem::SubmitWithDependency` 已经具备等价能力，推荐直接使用。

```cpp
// 使用 DSE 现有 JobSystem 构建 System DAG
void Gameplay3DModule::Update(World& world, float dt) {
    auto& js = *ServiceLocator::Get<JobSystem>();

    // TransformSystem 必须第一个执行（写 Transform）
    auto tf_job = js.Submit([&]{ TransformSystem::Update(world, dt); }, JobPriority::High);

    // AnimationSystem 依赖 TransformSystem（也写 Transform）
    auto anim_job = js.SubmitWithDependency(
        [&]{ AnimationSystem::Update(world, dt); }, {tf_job}, JobPriority::High);

    // 以下 System 只读 Transform，可与 Animation 并行（如果不读骨骼输出）
    // 或者依赖 anim_job 保证安全
    auto particle_job = js.SubmitWithDependency(
        [&]{ Particle3DSystem::Update(world, dt); }, {tf_job}, JobPriority::Normal);
    auto audio_job = js.SubmitWithDependency(
        [&]{ Audio3DSystem::Update(world, dt); }, {tf_job}, JobPriority::Normal);
    auto light_job = js.SubmitWithDependency(
        [&]{ LightSystem::Update(world, dt); }, {tf_job}, JobPriority::Normal);

    // 等待所有完成
    js.Wait(anim_job);
    js.Wait(particle_job);
    js.Wait(audio_job);
    js.Wait(light_job);
}
```

**优势**：
1. 无需引入新依赖，直接用现有 `JobSystem`
2. 依赖关系显式声明，易于 review 和调试
3. 后续可封装为通用的 `SystemScheduler` 工具类

### 5.4 改动清单

| 文件 | 改动 |
|------|------|
| `modules/gameplay_3d/gameplay_3d_module.cpp` | 将 `Update()` 拆分为独立 System 函数 + JobSystem DAG 调度 |
| `modules/gameplay_2d/gameplay_2d_module.cpp` | 同上 |
| `runtime_update_graph.cpp` | `RunRuntimeUpdateGraph` 确保 Lua 脚本在主线程执行（见 §5.5） |
| `CMakeLists.txt` | 添加 `target_compile_definitions(dse_engine PRIVATE ENTT_USE_ATOMIC)` |

### 5.5 关键约束：Lua 脚本和 IModule 的线程安全

Phase 2 有两个**不可并行**的部分必须留在主线程：

1. **Lua 脚本**（`TickBusinessRuntime`）：`lua_State` 不是线程安全的，所有 Lua 调用（包括通过绑定层访问 ECS）必须在主线程执行
2. **用户自定义 `IModule::OnUpdate`**：无法假设第三方模块是线程安全的

```
RunRuntimeUpdateGraph 的实际并行边界：

  主线程（串行，不可并行）：
    ├── TickBusinessRuntime(Lua)     ← lua_State 不是线程安全的
    └── 用户 IModule::OnUpdate        ← 无法假设线程安全

  JobSystem（可并行）：
    ├── TransformSystem     → AnimationSystem（依赖链）
    ├── Particle3DSystem    （独立）
    ├── Audio3DSystem       （独立）
    └── LightSystem         （独立）
```

**推荐方案**：在每个 Gameplay Module 内部使用 `JobSystem` DAG 调度纯 C++ System（见 §5.3），对外保持 `IBuiltinModules` 接口不变。Lua 脚本和用户模块仍在主线程串行执行。

> 这意味着 Phase 2 能并行化的只是 `UpdateGameplay3D` / `UpdateGameplay2D` 内部的纯 C++ System，而非整个 Update 阶段。收益相应受限。

### 5.6 收益预测

| System 数量 | 串行耗时 | 并行后（4 核） | 加速比 |
|:----------:|:-------:|:-------------:|:-----:|
| 8 个 | 4ms | ~2.5ms | 1.6x |
| 16 个 | 6ms | ~3ms | 2.0x |

收益取决于 System 数量：System 越多、组件重叠越少，加速越明显。

### 5.7 风险

| 风险 | 严重度 | 说明 |
|------|:------:|------|
| 组件冲突分析错误 | 🔴 高 | 如果两个 System 实际共享了某个组件但未在 DAG 中声明依赖，会产生 data race |
| Lua VM 线程不安全 | 🔴 高 | `lua_State` 不可跨线程调用。所有 Lua 绑定必须留在主线程（见 §5.5） |
| 第三方 IModule 线程安全性 | 🟡 中 | 用户自定义模块可能不是线程安全的，只能留在主线程 |
| TransformSystem → AnimationSystem 强依赖 | 🟡 中 | 骨骼输出写 Transform，必须等 TransformSystem 完成 |
| `ENTT_USE_ATOMIC` 未定义 | 🟢 低 | 多线程访问不同组件类型需要此宏，否则类型索引非原子 |

**建议**：Phase 2 从最简单的独立 System 开始（Particle3DSystem ∥ Audio3DSystem ∥ LightSystem，这三个一定没有组件重叠），逐步扩大并行范围。用 ThreadSanitizer 验证。

### 5.8 关于 TaskGraph 的深入分析

#### 5.8.1 TaskGraph 是什么

UE 的 TaskGraph 是一种**基于 DAG 的通用任务调度器**。它的核心逻辑可以简化为：

```cpp
// 声明任务 A，产生数据 X
auto a = graph.AddTask("Physics", []{ /* 计算物理 */ }, Produces<X>);

// 声明任务 B，消费 X，产生 Y（依赖 A）
auto b = graph.AddTask("AnimUpdate", []{ /* 更新动画 */ }, Consumes<X>, Produces<Y>);

// 声明任务 C，消费 Y（依赖 B）
auto c = graph.AddTask("RenderPrep", []{ /* 准备渲染 */ }, Consumes<Y>);

// 调度器自动分析依赖，无依赖的任务（如 D、E）自动并行执行
graph.Run(thread_pool);
```

UE 用 TaskGraph 调度**整帧所有任务**：动画更新（每个角色一个任务）、布料模拟（每个组件一个任务）、蒙皮更新（每个网格一个任务）、视锥裁剪（分区块）……每帧数百到上千个 TaskGraph 任务。

#### 5.8.2 DSE 的 JobSystem 已经具备等价能力

DSE 的 `JobSystem::SubmitWithDependency` 本质上就是 TaskGraph：

```cpp
// 手动声明依赖链（与 TaskGraph 等价）
auto a = job_sys->Submit(physics_task, JobPriority::High);
auto b = job_sys->SubmitWithDependency(anim_task, {a}, JobPriority::High);
auto c = job_sys->SubmitWithDependency(render_task, {b}, JobPriority::High);

// 无依赖的任务自动并行
auto d = job_sys->Submit(independent_task, JobPriority::Normal);
auto e = job_sys->Submit(another_independent, JobPriority::Normal);
```

**区别只在 API 层面：**

| 维度 | UE TaskGraph | DSE JobSystem |
|------|:-----------:|:-------------:|
| 依赖声明 | 自动推断（Produces/Consumes 类型系统） | 手动传 `JobHandle` 数组 |
| 调度粒度 | 单任务级别 | 单任务级别（等价） |
| 工作窃取 | ✅ | ✅ |
| 依赖链 | ✅ | ✅ `SubmitWithDependency` |
| 优先级 | 隐式（按依赖拓扑） | 显式（High/Normal/Low） |

**DSE 在调度能力上并不缺失。缺失的不是调度器本身，而是粒度。**

#### 5.8.3 真正的问题：DSE 的 ECS System 粒度太粗

`UpdateGameplay3D` 内部实际包含了 6 个 System，但被包在一个函数里：

```cpp
// gameplay_3d_module.cpp — 当前
void Gameplay3DModule::Update(World& world, float dt) {
    TransformSystem::Update(world, dt);      // 写 TransformComponent
    MeshRenderSystem::Update(world, dt);     // 读 TransformComponent
    AnimationSystem::Update(world, dt);      // 写 TransformComponent（骨骼）
    Particle3DSystem::Update(world, dt);     // 写 ParticleSystem3DComponent
    Audio3DSystem::Update(world, dt);        // 读 TransformComponent
    LightSystem::Update(world, dt);          // 写 LightComponent
}
```

要让 TaskGraph 调度这些 System，必须先做拆解：

```
改造前: UpdateGameplay3D() { A(); B(); C(); }   // 巨石函数，无法拆分
改造后: TaskA("TransformSystem", writes<Transform>);
         TaskB("AnimationSystem", writes<Transform>, after=A);
         TaskC("ParticleSystem", writes<Particle>, after=A);
         // TaskGraph 自动调度 A→B∥C
```

这不是加一个调度器就能解决的——**需要先改模块接口，把巨石函数拆成细粒度的可调度任务。** 这是 Phase 2 中"方式二"（模块内部使用 EnTT scheduler）的前提条件。

#### 5.8.4 真实的收益计算

假设拆成 6 个 System 并用 TaskGraph 调度：

```
TransformSystem     (2ms, writes<Transform>)
  ↓
AnimationSystem    (1ms, writes<Transform>)  ∥  ParticleSystem (0.5ms, writes<Particle>)
MeshRenderSystem   (0.5ms, reads<Transform>)     AudioSystem (0.5ms, reads<Transform>)
LightSystem        (0.3ms, reads<Transform>)     ↓
  ↓
Done
```

| 度量 | 值 |
|------|:---:|
| 串行时间 | 2 + 1 + 0.5 + 0.5 + 0.3 + 0.5 = **4.8ms** |
| 并行时间（4 核） | 2ms + max(1, 0.5, 0.5) + max(0.5, 0.3) ≈ **3.5ms** |
| 加速比 | 4.8 / 3.5 ≈ **1.37x** |

**核心原因：** TransformSystem（2ms）必须第一个跑，AnimationSystem（1ms）必须等它——这是计算本身的序列依赖，调度器解决不了。

#### 5.8.5 在 Phase 1 基础上叠加的收益

| 阶段 | 帧时间 | FPS | 累计加速 |
|------|:-----:|:---:|:-------:|
| 当前 | 14ms | 71 | 1x |
| Phase 1 | 6ms | 166 | **2.3x** |
| Phase 1 + Phase 2（TaskGraph） | 4.4ms | 227 | **3.2x** |
| Phase 1 + 2 + 3 | 3.5ms | 285 | **4x** |

**Phase 1 已经拿走了最大的收益（2.3x）。Phase 2（TaskGraph）在 Phase 1 基础上只叠加 ~1.3x。** 这不是 TaskGraph 本身的问题——而是 Update 阶段的 ECS System 之间天然存在序列依赖。

#### 5.8.6 为什么 UE 的 TaskGraph 看起来更强大

因为 UE 用 TaskGraph 并行化的**任务数量大得多**，而且**粒度细得多**：

```
UE 一帧的 TaskGraph 任务示例（部分）：
├── 动画更新（每个角色一个任务，可全部并行）
│     ├── TickAnimation(Character_1)
│     ├── TickAnimation(Character_2)  ← 全部并行
│     └── ...
├── 布料模拟（每个布料组件一个任务）
├── 粒子模拟（每个 emitter 一个任务）
├── 蒙皮更新（每个 skinned mesh 一个任务）
├── 视锥裁剪（分区块，每个区块一个任务）
└── 渲染线程内部也有数百个 TaskGraph 任务
```

**UE 做到了 entity 级别的任务并行。** DSE 当前的粒度是 System 级别（TransformSystem、AnimationSystem 等），不是每个 entity 级别。

要让 DSE 做到 entity 级别的并行，需把 `view<T>().each([&](auto e, auto& c) { ... })` 这个循环拆成多个子任务，每个处理 entity 的一个子集：

```cpp
// 分区迭代（手动拆分为 N 个子任务）
auto all_entities = registry.view<TransformComponent>();
size_t total = all_entities.size();
size_t chunk = (total + num_threads - 1) / num_threads;

for (int i = 0; i < num_threads; ++i) {
    size_t start = i * chunk;
    size_t end = std::min(start + chunk, total);
    jobs.push_back(job_sys->Submit([&, start, end]() {
        for (size_t j = start; j < end; ++j) {
            auto entity = all_entities[j];
            auto& transform = all_entities.get<TransformComponent>(entity);
            // 处理 transform...
        }
    }));
}
```

这是可行的，但当前 EnTT 的 view 不支持原生分区迭代（分区迭代器），需要手动按范围分块——额外的工作量，但收益也在 1.3~1.5x 的量级。

#### 5.8.7 EnTT 的多线程安全性

EnTT 官方文档明确支持并发访问不同组件类型：

> "As long as a thread iterates the entities that have the component X [...] another thread can safely do the same with components Y and Z and everything work like just fine."

关键技术前提：需要定义编译宏 `ENTT_USE_ATOMIC`（DSE 当前没有定义），确保多线程下类型索引生成是原子操作。这只是一行 CMake 配置：

```cmake
# CMakeLists.txt
target_compile_definitions(dse_engine PRIVATE ENTT_USE_ATOMIC)
```

Stage 1 的 `const World&` / `const entt::registry&` 路径天然线程安全（`const registry` 保证所有 storage 对象已初始化）。

#### 5.8.8 结论

| 问题 | 回答 |
|------|------|
| **DSE 能做 TaskGraph 吗？** | **能。** `SubmitWithDependency` 已经是 TaskGraph。EnTT 支持不同组件的并发访问。不存在技术障碍 |
| **收益高吗？** | Phase 1 之后的收益中等（~1.3x），Phase 1 之前收益可忽略。串行的 TransformSystem 是真正瓶颈 |
| **UE 的 TaskGraph 为什么更强？** | UE 的任务数量多（百级 vs DSE 的十级），粒度细（逐 entity 级 vs DSE 的逐 System 级），且从第一天就设计为任务化 |
| **值得做吗？** | 如果 Update 是性能瓶颈（复杂 AI、大量脚本的场景），**值得**。但如果 Phase 1 后帧率已经达标（如 166 FPS），没必要 |

### 5.9 Phase 2b：Entity 级分块并行（可选扩展，约 1 周）

#### 5.9.1 问题：System 级粒度用不满 16 核

Phase 2 的 System 级并行最多利用 3~4 核（~6 个 System，DAG 约束下可并行的只有 3~4 个）。现代桌面 CPU 普遍 16+ 核（8P+8E 或 16C），大部分核仍在空闲。

主流引擎的做法：

| 引擎 | 粒度 | 每帧 task 数 | 可用核数 |
|------|------|:-----------:|:------:|
| UE5 | 逐 entity（动画/蒙皮/粒子各一个 task） | 500~2000 | 16~32 |
| Unity DOTS | 逐 chunk（64 entity/chunk） | entity_count / 64 | 16+ |
| Godot 4 | System 级 | ~10 | 2~3 |
| **DSEngine Phase 2** | **System 级** | **~6** | **3~4** |
| **DSEngine Phase 2b** | **逐 chunk（64 entity/chunk）** | **entity_count / 64** | **8~16** |

#### 5.9.2 改造方案：通用分块迭代器

写一个通用工具函数（约 30 行），所有 System 复用：

```cpp
// engine/core/parallel_for.h（新建，约 30 行）
template<typename View, typename Func>
void ParallelForEach(JobSystem& js, View& view, Func&& func, size_t chunk_size = 64) {
    size_t total = view.size_hint();
    if (total <= chunk_size) {
        // 实体少，不值得并行，直接串行
        for (auto entity : view) { func(entity); }
        return;
    }

    std::vector<JobHandle> jobs;
    auto it = view.begin();
    while (it != view.end()) {
        auto chunk_end = it;
        for (size_t i = 0; i < chunk_size && chunk_end != view.end(); ++i, ++chunk_end) {}
        jobs.push_back(js.Submit([&view, it, chunk_end, &func]() {
            for (auto curr = it; curr != chunk_end; ++curr) {
                func(*curr);
            }
        }, JobPriority::Normal));
        it = chunk_end;
    }
    for (auto& j : jobs) js.Wait(j);
}
```

每个 System 的改造量约 3~5 行：

```cpp
// 改造前：
void LightSystem::Update(World& world, float dt) {
    auto view = world.registry().view<TransformComponent, LightComponent>();
    for (auto entity : view) { /* ... */ }
}

// 改造后：
void LightSystem::Update(World& world, float dt, JobSystem& js) {
    auto view = world.registry().view<TransformComponent, LightComponent>();
    ParallelForEach(js, view, [&](auto entity) { /* ... 逻辑不变 ... */ });
}
```

#### 5.9.3 改动量

| 项目 | 行数 |
|------|:----:|
| `engine/core/parallel_for.h` 通用工具 | ~30 行（一次性） |
| CMake 添加 `ENTT_USE_ATOMIC` | 1 行 |
| 6~8 个 System 各改 3~5 行 | ~30 行 |
| **合计** | **~60 行** |

#### 5.9.4 核利用率提升

| 阶段 | 可用核数 | 帧时间 |
|------|:------:|:-----:|
| Phase 2（System 级） | 3~4 核 | ~2.5ms Update |
| Phase 2b（Entity chunk 级） | **8~16 核** | ~1.5ms Update |

#### 5.9.5 风险

| 风险 | 严重度 | 说明 | 缓解 |
|------|:------:|------|------|
| 同一 entity 被两个并行 System 写同一组件 | 🔴 高 | 如 TransformSystem 和 AnimationSystem 都写 `TransformComponent` | DAG 依赖已保证不并行；Entity chunk 并行是**同一 System 内部**拆分，不跨 System |
| EnTT view 迭代器并发安全 | 🟡 中 | 多个线程同时迭代同一 view | EnTT 官方确认：同一组件类型的不同 entity 可并发修改；`ENTT_USE_ATOMIC` 保证类型索引原子 |
| 父子 Transform 依赖 | 🔴 高 | `TransformSystem` 中子实体依赖父实体的变换结果，按 chunk 随机拆分可能导致子在父之前计算 | 方案：TransformSystem 保持串行（层级遍历），仅对无层级依赖的 System（Light/Particle/Audio）做 chunk 并行 |
| chunk_size 选择不当 | 🟢 低 | chunk 太小 → 调度开销大于计算；chunk 太大 → 并行度不足 | 默认 64（与 Unity DOTS 一致），可运行时调参 |
| false sharing（缓存行竞争） | 🟡 中 | 两个相邻 entity 的组件数据在同一缓存行，不同核写入时互相失效 | EnTT 的 storage 是 SoA 布局，同类组件连续存储，chunk 边界通常对齐缓存行 |

**关键约束**：`TransformSystem` 因为父子层级依赖**不适合 chunk 并行**，必须保持串行层级遍历。可受益的 System：

| System | 是否可 chunk 并行 | 原因 |
|--------|:---------------:|------|
| TransformSystem | ❌ | 父子依赖，必须层级遍历 |
| AnimationSystem | ✅ | 每个骨骼动画独立 |
| Particle3DSystem | ✅ | 每个粒子系统独立 |
| Audio3DSystem | ✅ | 每个音源独立 |
| LightSystem | ✅ | 每个灯光独立 |
| MeshRenderSystem | ✅ | 只读 Transform |

### 5.10 Amdahl 极限分析（16 核场景）

#### 5.10.1 不可并行的串行部分

```
FixedUpdate 入口          2.0ms   ← PhysX/Jolt 内部已多线程，但调用入口串行
Lua 脚本执行              ~1.0ms  ← lua_State 不可跨线程
GPU Submit / Present     ~0.1ms  ← vkQueueSubmit 必须单线程
ECS 结构性变更            ~0.1ms  ← 创建/销毁实体必须单线程
──────────────────────────────
总串行部分 S ≈ 3.2ms
```

#### 5.10.2 理论极限

```
总帧时间 T = 14ms
串行部分 S = 3.2ms
可并行部分 P = T - S = 10.8ms

Amdahl 极限 = T / S = 14 / 3.2 ≈ 4.4x → ~310 FPS（∞ 核）
```

#### 5.10.3 各阶段逼近极限的程度

| 阶段 | 帧时间 | FPS | 逼近极限 | 用了几核 |
|------|:-----:|:---:|:------:|:------:|
| 当前（串行） | 14ms | 71 | 基准 | 1 |
| Phase 1 | 8ms | 125 | 45% | 2 |
| + Phase 3 | 5ms | 200 | 72% | 4~5 |
| + Phase 2 + 2b（Entity 并行） | ~3.5ms | 285 | **91%** | 12~16 |
| **理论极限**（∞ 核） | **3.2ms** | **310** | **100%** | ∞ |

#### 5.10.4 与主流引擎的对比

| | DSEngine（全量） | UE5 | Unity DOTS |
|---|---|---|---|
| 串行瓶颈 | Lua (~1ms) + FixedUpdate (2ms) | Blueprint Tick (~0.5ms) | Structural changes |
| 理论极限 | ~4.4x | ~6~8x | ~8~10x |
| 16 核利用率 | 12~16 核 | 16~32 核 | 16+ 核 |
| 极限差距原因 | **Lua 单线程** | Blueprint 可 nativize | Burst compiler |

#### 5.10.5 突破 Lua 瓶颈的路径

Lua 是 DSEngine 的 Amdahl 硬上限。两条突破路径：

| 方案 | 改动量 | 效果 |
|------|:-----:|------|
| **Lua → LuaJIT** | 中（替换 VM，验证兼容性） | 执行速度 10~50x，1ms → 0.02~0.1ms |
| **热路径原生化** | 低（关键 System 用 C++ 重写） | 可并行 + 无解释开销 |

采用 LuaJIT 后，串行部分从 3.2ms 降到 ~2.2ms，理论极限从 4.4x 提升到 **6.4x（~450 FPS）**。

---

## 六、Phase 3：Pass 级并行录制（约 3~4 周）

### 6.1 背景

RenderGraph 的编译输出是一个按拓扑排序的 Pass 列表，其中无依赖的 Pass 可以并行录制命令。

通过 [`RenderGraph::Compile()`](https://github.com/user/repo/engine/render/render_graph.cpp) 可以获取 DAG 的层次结构，标记出同一层级的 Pass 集合。

### 6.2 可并行录制的 Pass 分析

以下是 DSE 当前渲染图的典型结构：

```
Level 0（可并行）:
  ├── PreZPass          [写: prez_depth]
  ├── ShadowPass         [写: shadow_map[0..2]]
  ├── SpotShadowPass     [写: spot_shadow_map[0..3]]
  ├── PointShadowPass    [写: point_shadow_map[0..3]]
  └── GBufferPass（可选） [写: gbuffer]
       ↓ 所有 Level 0 的 RT 都由 Level 1 读取

Level 1（可并行）:
  ├── ForwardScenePass   [读: prez_depth, shadow_map, gbuffer  写: scene_color]
  ├── SSAOPass           [读: prez_depth                        写: ssao]
  ├── SSRPass            [读: gbuffer, scene_color              写: ssr]（可选）
  └── RSMPass            [读: gbuffer                           写: rsm]（可选）
       ↓

Level 2（可并行）:
  ├── DeferredLightingPass（可选）
  ├── DDGIRenderPass
  └── OutlinePass
       ↓

Level 3（可并行）:
  ├── BloomPass
  ├── TAA Pass
  └── DOF Pass
       ↓

Level 4:
  ├── PostProcessPass（Bloom/TAA/DOF/SSR 合成）
  └── PresentPass
```

**Level 0 的 5 个 Pass 彼此无依赖，可以完全并行。Level 1 的 3~4 个 Pass 同样可以并行。** 在 8 核机器上，Pass 级并行理论上可以将 RenderGraph 的执行时间压缩到单个 Pass 的最长时间。

### 6.3 Vulkan 的多线程录制

Vulkan 原生支持多线程录制 CommandBuffer：

```cpp
// 每个线程有自己的 VkCommandBuffer
struct ThreadContext {
    VkCommandPool pool;
    VkCommandBuffer cmd;
};

// 渲染线程拆分为每个 Pass 一个子任务
void RenderGraph::ExecuteParallel(JobSystem& js) {
    auto levels = ComputeParallelLevels();
    thread_local ThreadContext tl_ctx;

    for (auto& level : levels) {
        std::vector<JobHandle> jobs;
        for (auto& pass : level) {
            jobs.push_back(js.Submit([&]() {
                // 在 worker 线程中录制
                auto cmd_buf = rhi_device_->CreateCommandBuffer();
                pass.execute(*cmd_buf);
                // 录制完成后追加到全局提交列表
                pending_cmd_bufs_.push_back(cmd_buf);
            }));
        }
        for (auto& j : jobs) js.Wait(j);
        // Level 间 barrier（VkPipelineBarrier + VkSemaphore）
        InsertLevelBarrier();
    }

    // 所有录制完成后，主线程提交
    rhi_device_->Submit(pending_cmd_bufs_);
}
```

### 6.4 每个 Pass 需要独立的 CommandBuffer

这是核心前提。当前 `ExecuteRenderGraph` 共用同一个 `CommandBuffer`：

```cpp
auto cmd_buffer = CreateRuntimeRenderCommandBuffer(*this);
ExecuteRenderGraph(*cmd_buffer);
Submit(*cmd_buffer);
```

改造后，每个线程需要自己的 CommandBuffer：

```cpp
// 方式：Pass 录制阶段用独立 CommandBuffer
// 然后通过 RHI 的 multi-CommandBuffer Submit 合并
```

Vulkan 后端原生支持：`vkQueueSubmit` 接受 `VkSubmitInfo` 数组。
D3D11 后端支持有限：可以用 Deferred Context，但 Deferred Context 的驱动支持参差不齐。
OpenGL 后端不支持：GL 没有多线程录制命令的概念。

### 6.5 资源壁垒管理

并行录制后需要在 Level 间插入正确 Barrier：

| Level | 前一个 Level 写入 | 当前 Level 读取 | Barrier 类型（Vulkan） |
|-------|-----------------|----------------|----------------------|
| 0→1 | prez_depth | ForwardScenePass | `DEPTH_STENCIL_READ` |
| 0→1 | shadow_map | ForwardScenePass | `SHADER_READ` |
| 1→2 | scene_color | BloomPass | `SHADER_READ` |
| 1→2 | scene_color | TAA Pass | `SHADER_READ` |
| 2→3 | bloom_mips | PostProcessPass | `SHADER_READ` |

每个 Level 结束时，需要 `VkPipelineBarrier` 确保前一个 Level 的所有写入对下一个 Level 可见。这要求 RenderGraph 在 Compile 时就能生成完整的 Barrier 列表。

### 6.6 后端差异

| 后端 | 多 CommandBuffer 录制 | 可行方案 |
|------|:-------------------:|---------|
| Vulkan | ✅ `VkCommandBuffer` 可在任意线程创建和录制 | 每个 Level 的 Pass 提交到 JobSystem，使用独立的 `VkCommandPool` + `VkCommandBuffer` |
| D3D11 | ⚠️ `ID3D11Device::CreateDeferredContext` | 每个 Pass 一个 Deferred Context，录制完成后用 `FinishCommandList` + `ExecuteCommandList` 提交 |
| OpenGL | ❌ | 走串行路径（不参与 Pass 级并行） |

### 6.7 收益预测

| 配置 | 串行录制 | 并行录制（假设 Level 0: 5 Pass 并行） | 加速比 |
|------|:-------:|:-----------------------------------:|:-----:|
| 当前所有 Pass | 8ms | ~3ms（取决于最慢的单个 Pass） | 2.7x |

但这是**理论值**——实际上 Level 0 中最慢的可能是 ForwardScenePass（6ms），其他 Pass 在 0.5~2ms 之间。最终收益约 **1.2~1.4x**。

### 6.8 改动清单

| 文件 | 改动 |
|------|------|
| `engine/render/render_graph.h` | 添加 `ComputeParallelLevels()` 和 `ExecuteParallel()` |
| `engine/render/render_graph.cpp` | 实现 DAG 层级分析、Level Barrier 生成 |
| 三端 RHI 后端 | 实现多 CommandBuffer 录制和提交的线程安全包装 |
| `engine/runtime/frame_pipeline.cpp` | `RunRenderInternal` 中可选的并行执行路径 |
| `engine/render/rhi/rhi_device.h` | 可能需要添加 `Submit(std::vector<CommandBuffer>)` |

---

## 七、Phase 4：全局优化——三级缓冲（约 1~2 周）

### 7.1 背景

> **✅ GPU Driven VK/DX11 补全已完成。** GL/VK/DX11 三端的 `IRhiGpuDriven` 接口（CreateMegaVAO、UpdateMegaVBO/IBO、BindMegaVAO、MultiDrawIndexedIndirect、CreateStaticMeshVAO、Hi-Z、ReadSSBO 等）已全部实现并对齐。此 Phase 原计划的 4~6 周工作量已在前期完成，Phase 4 缩减为仅三级缓冲优化。

### 7.2 三级缓冲

当前 Phase 1 使用**双缓冲快照**。在重度 GPU 负载下，如果 GPU 执行时间超过帧时间，双缓冲可能导致主线程等 GPU。

升级到**三级缓冲（triple buffering）**：

```
帧 N:     主线程 Update(N) → Flush → [写 Ring Buffer Slot 0]
帧 N:     渲染线程 Render(N-1) → Submit GPU → [Slot 1 被 GPU 读取]
帧 N:     [GPU 在执行帧 N-2 的命令，Slot 2 被 GPU 占用]

帧 N+1:   主线程 Update(N+1) → Flush → [Slot 0 已空闲，重用]
帧 N+1:   渲染线程 Render(N) → Submit GPU → [Slot 1]
帧 N+1:   GPU 完成帧 N-2，释放 Slot 2
```

三级缓冲确保主线程永远不会因为等待 GPU 而阻塞，代价是多一份快照的内存（~3KB）。

### 7.3 收益预测

| 优化 | 收益场景 | 加速比 |
|------|---------|:-----:|
| 三级缓冲 | GPU 瓶颈场景 | 消除主线程空闲等待 |

---

## 八、完整路线图

```
Phase 0: GPU Driven 默认化                          ✅ 基本完成（三端 GPU Driven 已对齐）
  时间：剩余 ~1 天（端到端验证）
  依赖：无
  收益：≈0%（为 Phase 1 清除障碍）
  └───→

Phase 1: 薄快照 + 延迟一帧流水线并行                 ✅ 已完成 (2026-05-21)
  实际改动：10 files, +1219/-616 lines
  架构：专用渲染线程 + 双缓冲快照 + condition_variable 同步
  激活：DSE_RENDER_THREAD=1 环境变量（编辑器自动禁用）
  收益：1.5~1.75x 帧率（取决于 Update/Render 哪个是瓶颈）
  ├───→ 当前可以停在这里。下一步按需选择 Phase 3 或 Phase 2

Phase 3: Pass 级并行录制                             ⭐ 次高优先级（典型场景 Render 是瓶颈）
  时间：3~4 周
  依赖：Phase 1
  收益：1.2~1.4x（叠加）
  ├───→ 如果 Render 是瓶颈且 VK 后端，继续此阶段

Phase 2: ECS System 级并行                           仅在 Update 是瓶颈时有收益
  时间：2~3 周
  依赖：Phase 1
  收益：1.2~1.5x（叠加，但 Render 瓶颈时帧率无提升）
  ├───→ 如果 Update 是瓶颈（大量脚本/AI），继续此阶段

Phase 2b: Entity chunk 级并行                         用满 16 核（~60 行改动）
  时间：~1 周
  依赖：Phase 2
  收益：Update 从 ~2.5ms → ~1.5ms，16 核利用率从 3~4 核 → 8~16 核
  ├───→ TransformSystem 保持串行（父子依赖），其余 System 自动分块

Phase 4: 三级缓冲                                    ✅ GPU Driven 补全已完成，仅剩三级缓冲
  时间：1~2 周
  依赖：Phase 1
  收益：消除 GPU 瓶颈场景下主线程空闲等待
  └───→ 如果 GPU 负载重导致主线程等待，考虑此阶段
```

### 并行维度叠加示意图

```
Phase 0 + 1:                      Phase 0 + 1 + 2 + 3 + 4:
                                
┌──────┐ ┌──────┐                 ┌──────┐ ┌──────┐   
│Update│ │Update│  主线程          │UpdA  │ │UpdB  │  主线程 + JobSystem × N
│System│ │System│                 │Sys1  │ │Sys2  │  
└──┬───┘ └──┬───┘                 └──┬───┘ └──┬───┘  
   │         │                        │         │     
┌──▼─────────▼──┐                  ┌──▼─────────▼──┐ 
│   Render      │  渲染线程         │  Render       │ 
│ (Read Snapshot)│                 │ (Pass 1│Pass 2)│ 渲染线程 + JobSystem × N
└───────────────┘                  └─────────┬─────┘ 
                                             │     
                                          ┌──▼──┐  
                                          │ GPU  │  
                                          └─────┘  
```

---

## 九、与主流引擎的对比

| 维度 | Unreal Engine 5 | Unity (非 DOTS) | Godot 4 | DSEngine (最终) |
|------|:--------------:|:---------------:|:-------:|:--------------:|
| **流水线并行** | ✅ Game/Render 分离 | ✅ 主/渲染线程 | ✅ 场景/渲染线程 | ✅ **Phase 1** |
| **数据传递** | `ENQUEUE_RENDER_COMMAND` | GfxDevice 命令队列 | 渲染数据快照 | **RenderThinSnapshot** |
| **锁** | 无锁（只读快照） | 无锁（命令队列） | 无锁（快照） | **无锁** |
| **数据延迟** | ~1 帧 | 0~1 帧 | 1 帧 | **1 帧** |
| **ECS System 并行** | ✅ TaskGraph | ❌（主线程） | ❌ | ✅ **Phase 2** |
| **System 调度** | 显式依赖声明 | N/A | N/A | **JobSystem DAG** |
| **Pass 级并行** | ✅ 支持 | ✅ 支持 | ❌ | ✅ **Phase 3** |
| **GPU Driven** | ✅ Nanite | ❌ | ❌ | ✅ **已实现（三端对齐）** |
| **异步资源加载** | ✅ | ✅ | ✅ | ✅ **已有** |

---

## 十、理论极限（Amdahl's Law）

### Phase 1 之后

```
并行帧时间 = max(FixedUpdate + Update, Render) = max(6, 8) = 8ms
加速比:     14ms / 8ms = 1.75x（125 FPS）

注意：只有 Render < Update 时才能达到 2x+。典型场景 Render 是瓶颈。
```

### Phase 2 之后（ECS System 并行，假设 2D/3D 完全独立）

```
串行部分（FixedUpdate）:    S = 2ms
可并行部分（Update）：      P = 4ms（原本，并行后 ≈ 2.5ms）
可并行部分（Render）：      P = 8ms
加速比上限:                  1 / (2 / 14) ≈ 7x（理论上限）
现实可达:                  14ms / max(2, 2.5, 8) ≈ 1.75x（相对串行）
                              相对 Phase 1: 6ms→max(2,2.5, 8)=8ms...更差了？
```

这里有一个反直觉的情况：**Phase 2 在 Update 较快的 Scene 中可能使帧时间变长而不是变短**，因为 Update 变快后渲染线程变成瓶颈，而 Phase 1 中 Update 和 Render 的平衡被打破了。

**Phase 2 的真正价值**是在 Update 是瓶颈的场景（如大量脚本、复杂 AI 的 RPG 游戏）。如果 Update 本身就不重，Phase 2 不提供收益。

### Phase 3 之后

```
串行部分（FixedUpdate）:           S = 2ms
可并行部分（Update）：             P_1 = ms（取决于 System 并行度）
可并行部分（Render，Pass 并行后）： P_2 = ~4ms（假设 Pass 并行 2x）
加速比上限:                         14ms / max(2, 2, 4) = 3.5x（相对串行）
```

### 最终极限

```
不可并行的串行部分（精确分解）：
  FixedUpdate 入口          2.0ms   ← PhysX/Jolt 内部已多线程，但调用入口串行
  Lua 脚本执行              ~1.0ms  ← lua_State 不可跨线程
  GPU Submit / Present     ~0.1ms  ← vkQueueSubmit 必须单线程
  ECS 结构性变更            ~0.1ms  ← 创建/销毁实体必须单线程
  ──────────────────────────────
  总串行部分 S ≈ 3.2ms

可并行部分: P = 14 - 3.2 = 10.8ms
理想加速比: T / S = 14 / 3.2 ≈ 4.4x → ~310 FPS
现实极限:   ~4x（考虑调度开销、负载不均）
```

各阶段逼近极限的程度：

| 阶段 | 帧时间 | FPS | 逼近极限 | 用了几核 |
|------|:-----:|:---:|:------:|:------:|
| 当前（串行） | 14ms | 71 | 基准 | 1 |
| Phase 1 | 8ms | 125 | 45% | 2 |
| + Phase 3 | 5ms | 200 | 72% | 4~5 |
| + Phase 2 + 2b（Entity 并行） | ~3.5ms | 285 | **91%** | 12~16 |
| **理论极限**（∞ 核） | **3.2ms** | **310** | **100%** | ∞ |

**Lua 是 Amdahl 硬上限。** 采用 LuaJIT 后，串行部分从 3.2ms 降到 ~2.2ms，理论极限从 4.4x 提升到 6.4x（~450 FPS）。

---

## 十一、综合风险评估

| 风险 | 概率 | 影响 | 涉及 Phase | 缓解方案 |
|------|:----:|:----:|:---------:|---------|
| `render_meshes` 回调仍读 ECS | 低 | 高 | Phase 1 | 三端 GPU Driven 已对齐，`gpu_driven_supported` 为 true 时此回调不执行。需验证无回退场景 |
| `ENTT_USE_ATOMIC` 未定义 | 低 | 中 | Phase 2 | 添加编译宏，确保多线程下类型索引原子操作 |
| Lua 脚本访问 ECS 的线程安全 | 高 | 高 | Phase 2 | 确保 Lua 绑定只在主线程执行（不在 Update 并行区） |
| Vulkan CommandPool 线程安全 | 中 | 中 | Phase 3 | 每个 worker 线程创建独立的 VkCommandPool |
| DX11 Deferred Context 驱动质量 | 中 | 中 | Phase 3 | 准备 fallback：Deferred Context 失败时回退到即时模式 |
| JobSystem 调度延迟 | 低 | 中 | Phase 1,3 | Render 任务设 `High` 优先级；Pass 任务用 `Normal` |
| 物理引擎结果延迟一帧 | 低 | 低 | Phase 1 | 物理仍在 FixedUpdate 执行，结果写回 ECS，下一帧 Capture 读到——无额外延迟 |
| 用户自定义 IModule 线程安全 | 高 | 中 | Phase 2 | 文档明确声明 Phase 2 下模块 Update 的线程约束；提供 `ThreadSafe` 标记接口 |
| TransformSystem chunk 并行导致父子乱序 | 高 | 高 | Phase 2b | TransformSystem 保持串行层级遍历，仅无层级依赖的 System 做 chunk 并行 |
| false sharing（缓存行竞争） | 中 | 低 | Phase 2b | EnTT SoA 布局天然减少 false sharing；chunk_size=64 通常对齐缓存行 |
| Lua 单线程是 Amdahl 硬上限 | 确定 | 中 | 全局 | 串行部分 ≈3.2ms，理论极限 ~4.4x。突破需 LuaJIT 或热路径原生化 |

---

## 十二、分阶段验收标准

| Phase | 验收标准 |
|-------|---------|
| **Phase 0** | ✅ 已完成。三端 GPU Driven 已对齐 |
| **Phase 1** | ✅ 已完成 (2026-05-21)。RenderThinSnapshot 快照覆盖全部 Pass ECS 需求；渲染线程分离编译通过零错误；GL 后端数据安全；DSE_RENDER_THREAD=1 启用；已知限制：GPU Driven 在线程模式暂跳过（Phase 3 处理） |
| **Phase 2** | JobSystem DAG 正确调度所有 ECS System；Lua 脚本和 IModule 仅在主线程执行；无 data race（用 ThreadSanitizer 验证）；每个 System 的组件访问依赖声明准确 |
| **Phase 2b** | Entity chunk 并行后 8~16 核利用率；TransformSystem 仍串行无回归；chunk_size=64 调度开销 < 0.1ms |
| **Phase 3** | RenderGraph 的 Level 分析正确；每个 Level 内的 Pass 录制无冲突；Barrier 插入正确（验证层无报错）；帧率提升符合预期 |
| **Phase 4** | 三级缓冲在不同负载下无主线程阻塞 |

---

## 十三、总结

```
串行（当前）：     [FixedUpdate(2ms)][Update(4ms)][───Render(8ms)───] = 14ms → 71 FPS

Phase 0+1 ✅：    [FixedUpdate(2ms)][Update(4ms)]                   主线程 6ms
                  ←─────── 渲染线程 Render(上一帧数据, 8ms) ──────→  帧时间 = max(6, 8) = 8ms → 125 FPS

Phase 0+1+2：     [FixedUpdate(2ms)][Upd(并行, ~3ms)]               主线程 5ms
                  ←─────── 渲染线程 Render(上一帧数据, 8ms) ──────→  帧时间 = max(5, 8) = 8ms → 125 FPS（Render 瓶颈）

Phase 0+1+2+3：   [FixedUpdate(2ms)][Upd(并行, ~3ms)]               主线程 5ms
                  ←─── 渲染线程(Pass 并行, ~4ms) ─────────────────→  帧时间 = max(5, 4) = 5ms → 200 FPS

Phase 全量：      [FixedUpdate(2ms)][Upd(并行, ~3ms)]               主线程 5ms
                  ←─── 渲染线程(GPU Driven(✅已有) + Pass 并行, ~3ms) →  帧时间 = max(5, 3) = 5ms → 200 FPS
```

> ⚠️ **关键洞察**：Phase 2 在 Render 是瓶颈的典型场景中**不提供帧率收益**（帧时间仍由 Render 决定）。
> 只有 Phase 3（Pass 并行录制）能压缩 Render 耗时，从而突破 125 FPS 的 Phase 1 天花板。
> **因此 Phase 3 的优先级实际上高于 Phase 2**，除非你的场景 Update > Render。

### 核心原则

1. **无锁优先**——所有并行方案优先选择无锁设计（只读快照、流水线并行、延迟一帧），避免引入 mutex/atomic 竞争
2. **按需叠加**——不是每个场景都需要所有 Phase。Update 轻的就只做 Phase 1；物体多的才做 Phase 4
3. **可回退**——每个 Phase 都有对应的回退方案（如 GL 串行路径、DX11 即时模式）
4. **零增加序列依赖**——不因为并行化而在帧路径中引入新的序列依赖点（如等待 barrier、等待线程池）

---

## 十四、突破瓶颈——逼近 UE5 水平的路线

### 14.1 当前差距

Phase 全量（Phase 0~4 + 2b）完成后，DSEngine 的理论极限是 **4.4x / ~310 FPS**，UE5 是 **6~8x**。差距来源于 4 个架构瓶颈。

### 14.2 瓶颈分解

#### 瓶颈 A：Lua 串行（影响最大）

| | UE5 | DSEngine |
|---|---|---|
| 游戏逻辑语言 | C++（可并行） | Lua（单线程 VM） |
| 逻辑占帧时间 | ~0.3ms（只做事件/状态机胶水） | ~1.0ms（包含移动/AI/碰撞响应等实际逻辑） |
| Amdahl 占比 | 0.3/16.6 = 1.8% | **1.0/14 = 7.1%** |

**UE 的设计哲学**：蓝图虽然比 C++ 慢 8~10x，但只负责轻量逻辑（状态机、事件响应、UI 交互），热路径（移动、物理、动画、AI 寻路）全部 C++。因此蓝图的 0.3ms 即使慢 10x，对总帧时间影响 < 2%。

> 注：UE5 蓝图**不是原生 C++ 性能**。UE4 的 Blueprint Nativization（蓝图编译为 C++）已在 UE5 中废弃，当前蓝图走字节码 VM 解释执行。

**解法（三选一，按 ROI 排序）**：

| 方案 | 改动量 | 效果 | 风险 |
|------|:-----:|------|------|
| **热路径原生化**：移动/AI/碰撞响应等从 Lua 移到 C++ System | 低~中（逐步迁移） | Lua 降到 0.1~0.3ms | 🟢 低——C++ System 已有框架 |
| **Lua → LuaJIT** | 中（替换 VM + 验证兼容性） | 执行速度 20~50x，Lua 降到 0.02~0.05ms | 🟡 中——API 兼容性需逐项验证 |
| **多 lua_State 分片并行** | 高（重构脚本绑定 + 共享状态隔离） | 多个 VM 并行执行 | 🔴 高——共享状态极难正确处理 |

**推荐**：热路径原生化（优先）+ LuaJIT（中期）。这也是 UE 的做法——重活 C++ 干，脚本只做轻量事件。

#### 瓶颈 B：渲染管线只有 2 级（UE5 有 3 级）

```
UE5 三级流水线（延迟 2 帧，3 核全忙）：
  GameThread ──→ RenderThread ──→ RHI Thread
  (逻辑+快照)     (场景遍历+录制)    (API 调用+提交 GPU)

DSEngine Phase 1 后（延迟 1 帧，2 核）：
  主线程 ──→ 渲染线程
  (逻辑)     (遍历 + 录制 + 提交 全在一起)
```

UE5 的 RHI Thread 把"录制命令"和"提交 GPU"拆成两个线程，RenderThread 录完一帧可以立刻开始下一帧。

**解法**：Phase 3（Pass 并行录制）+ Phase 4（三级缓冲）组合 ≈ UE5 的三级管线。

**改动量**：已在文档 Phase 3/4 中估计（3~4 周 + 1~2 周）。

#### 瓶颈 C：System 无组件访问声明

UE5 的 TaskGraph 能自动分析依赖：每个 System 显式声明读/写哪些组件。DSEngine 当前没有声明，DAG 只能手动硬编码。

```cpp
// UE5 风格（自动推导并行度）：
class UMovementSystem : public USystem {
    DECLARE_READ(TransformComponent)
    DECLARE_WRITE(VelocityComponent)
};

// DSEngine 当前（手动维护依赖）：
void TransformSystem::Update(World& world, float dt) {
    auto view = world.registry().view<TransformComponent>();
    // 调度器不知道读/写了什么
}
```

**解法**（约 100 行）：

```cpp
// 给每个 System 加编译期 trait：
struct TransformSystemTraits {
    using Reads  = ComponentList<ParentComponent, LocalTransformComponent>;
    using Writes = ComponentList<TransformComponent>;
};

// 调度器在编译期推导出哪些 System 可以并行
// ~80 行模板 + 每个 System 加 2~3 行声明
```

#### 瓶颈 D：OpenGL 后端天然不可并行

| 后端 | 多线程命令录制 | 多线程提交 |
|---|---|---|
| Vulkan | ✅ per-thread CommandPool | ✅ vkQueueSubmit |
| DX11 | ⚠️ Deferred Context（驱动质量参差） | ❌ |
| **OpenGL** | **❌ 完全不支持** | **❌** |

OpenGL 的 GL Context 绑定到单一线程，Phase 1/3 在 GL 后端**完全无性能收益**。

**UE5 的做法**：直接不支持 OpenGL（最低 DX11/Vulkan/Metal）。

**解法**：逐步以 Vulkan 为主后端，GL 作为兼容后端保留但不参与并行优化。

### 14.3 修复后的极限对比

| | 当前方案极限 | 突破 4 个瓶颈后 | UE5 参考 |
|---|---|---|---|
| 串行部分 | 3.2ms（Lua 1ms + FixUpd 2ms + misc 0.2ms） | **~2.2ms**（Lua 残余 0.1ms + FixUpd 2ms + misc 0.1ms） | ~1.5~2ms |
| 理论极限 | 4.4x / 310 FPS | **6.4x / 450 FPS** | 6~8x |
| 16 核利用率 | 6~8 核 → 12~16 核（+Phase 2b） | **12~16 核** | 16~32 核 |
| 差距原因 | Lua 串行 + 2 级管线 + 无自动 DAG + GL 限制 | 剩余差距 = UE 20 年的细粒度 task 积累（工程量，非架构） | — |

### 14.4 实施优先级

```
瓶颈 A: Lua 热路径原生化              ⭐ 最高优先级（ROI 最高）
  时间：渐进式，每个 System ~1 天
  效果：串行部分 3.2ms → 2.2ms，极限从 4.4x → 6.4x
  └───→

瓶颈 C: System 组件访问声明            ⭐ 高优先级（改动小收益大）
  时间：~3 天（100 行模板 + 每 System 2~3 行）
  效果：自动 DAG 推导，无需手动维护依赖
  └───→

瓶颈 B: 三级管线                       中优先级（已在 Phase 3+4 规划中）
  时间：Phase 3(3~4 周) + Phase 4(1~2 周)
  效果：渲染录制和提交解耦，多用 1 核
  └───→

瓶颈 D: GL 后端淡出                    低优先级（策略决策，无代码改动）
  效果：解除并行天花板
```
