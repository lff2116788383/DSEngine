# DSEngine 渲染管线优化方案

> 基于代码分析生成，不依赖项目文档。分析对象为 `engine/render/` 全部源码。
> 更新日期: 2026-05-12

---

## 一、当前渲染管线现状

### 1.1 管线架构

DSEngine 当前只有**一条固定的 Forward Rendering 管线**，由 RenderGraph DAG 驱动，执行顺序如下：

```
PreZ → CSM Shadow (×3 cascade) → Spot Shadow (×4) → Point Shadow (×4)
  → Forward Scene → Bloom → UI → Composite → Present
```

**代码位置：**
- 管线定义: `engine/render/passes/builtin_passes.h` — 9 个 Pass 类
- 管线组装: `engine/runtime/frame_pipeline.h` — FramePipeline 持有 Pass 实例并构建 RenderGraph
- Pass 共享上下文: `engine/render/passes/render_pass_context.h` — RenderTargets / PipelineStates / 模块回调
- 渲染图: `engine/render/render_graph.h` — DAG 拓扑排序 + 自动剔除 + 波次并行

### 1.2 着色器

统一 PBR 着色器 `engine/render/shaders/src/pbr.frag`（385 行）支持以下模式：
- **PBR Cook-Torrance**: GGX NDF + Smith G + Schlick Fresnel（默认）
- **Half-Lambert**: KF 风格漫反射（`light_params.w == 2.0`）
- **Half-Lambert Static**: KF 默认像素着色器（`light_params.w == 3.0`）
- **Unlit**: 无光照直接输出

Shader 构建管线（最新）：GLSL 450 源码 → `tools/shader_compiler/` 编译 → SPIRV-Cross 交叉编译为 HLSL/GLSL 330/SPIR-V。

### 1.3 光照处理方式

```glsl
// pbr.frag main() — Clustered Forward+ 遍历（已实现）
Lo += directional_light_contribution;        // 1 盏方向光
int cl_idx = ComputeClusterId(gl_FragCoord, linearDepth);  // 定位 cluster
for (ci = 0; ci < cluster_point_count; ci++)  // 当前 cluster 的点光源
    Lo += point_light_contribution[light_indices[offset + ci]];
for (si = 0; si < cluster_spot_count; si++)   // 当前 cluster 的聚光灯
    Lo += spot_light_contribution[light_indices[offset + point_count + si]];
```

**光源上限**：`kMaxClusteredPointLights = 256`, `kMaxClusteredSpotLights = 256`（SSBO 存储）。
通过 16×16 tile × 24 Z-slice 的 3D 网格剔除，每个片元只遍历所属 cluster 中的光源。

### 1.4 已有的现代化基础设施

| 组件 | 状态 | 说明 |
|------|------|------|
| RenderGraph DAG | ✅ 可用 | 依赖声明、拓扑排序、自动剔除、瞬态资源、波次并行 |
| Compute Shader | ⚠️ Vulkan + DX11 | Bloom downsample/upsample 使用 CS，OpenGL 用 fragment fallback |
| 三后端 RHI | ✅ 已稳定 | OpenGL/Vulkan/DX11 视觉对比 RMSE 21-22，亮度一致（2026-05-12 验证） |
| 统一 Shader 编译 | ✅ 已稳定 | GLSL 450 → SPIRV-Cross → SPIR-V / GLSL 430 / HLSL SM5 |
| CommandBuffer | ✅ 可用 | 延迟提交、排序、合并绘制调用 |
| SSBO (三后端) | ✅ 已稳定 | OpenGL `GL_SHADER_STORAGE_BUFFER` / Vulkan `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT` / DX11 `ByteAddressBuffer` |
| Clustered Forward+ | ✅ 已完成 | `light_buffer.h/cpp` + `cluster_grid.h/cpp`，CPU 端分簇 + SSBO 上传 |
| SSAO + FXAA | ✅ 已完成 | `SSAOPass` + `FXAAPass`，三后端 shader 实现 |
| CSM 级联过渡 | ✅ 已完成 | `pbr.frag` smoothstep 混合相邻级联 |
| Light Probe SH 管线 | ⚠️ 管线就绪 | 三后端 UBO 上传 + `EvaluateSH()` shader，但 Bake 系统未实现，运行时 `enabled=false` |

### 1.5 已预留但未实现的功能

| 功能 | 代码位置 | 状态 |
|------|----------|------|
| SSAO | `SSAOPass` + 三后端 shader | ✅ 已实现，通过 `PostProcessComponent::ssao_enabled` 控制 |
| FXAA | `FXAAPass` + 三后端 shader | ✅ 已实现，通过 `PostProcessComponent::fxaa_enabled` 控制 |
| Light Probe (SH L2) | `LightProbeComponent` + `EvaluateSH()` + 三后端 UBO | ⚠️ 管线已通，Bake 系统未实现 |
| Reflection Probe | `ReflectionProbeComponent` (cubemap) | 组件定义完成，未接管线 |
| Morph Targets | `MorphComponent` | 组件定义完成，GPU buffer 未实现 |

---

## 二、与主流引擎对比

### 2.1 管线架构对比

| 能力维度 | DSEngine | Unity URP | Unity HDRP | Unreal 5 | Godot 4 |
|----------|----------|-----------|------------|----------|---------|
| 管线类型 | Forward (固定) | Forward+ (可编程) | Deferred + Forward (可编程) | Deferred + Forward+ 混合 | Clustered Forward |
| 管线可选/切换 | ❌ | ✅ 3 套可选 | ✅ | 自动混合 | 2 套可选 |
| Light Culling | ✅ Clustered Forward+ | Clustered | Tiled + Clustered | Tiled/Clustered | Clustered |
| 光源上限 | 256+256+1 | 数百 | 数千 | 无实际限制 | 数百 |
| GBuffer | ❌ | ❌ (Forward+无需) | ✅ | ✅ | ✅ (Deferred 可选) |
| GPU Driven | ❌ | ⚠️ 部分 | ✅ SRP Batcher | ✅ Nanite | ❌ |
| Compute 管线 | ⚠️ Vulkan + DX11 Bloom | ✅ | ✅ | ✅ 深度依赖 | ✅ |

### 2.2 后处理对比

| 效果 | DSEngine | Unity URP | Unreal 5 | Godot 4 |
|------|----------|-----------|----------|---------|
| Bloom | ✅ | ✅ | ✅ | ✅ |
| Tonemapping | ✅ PBR 内 Reinhard + Bloom ACES Filmic | ✅ ACES/多算法 | ✅ | ✅ |
| SSAO | ✅ 三后端实现 | ✅ | ✅ GTAO | ✅ |
| SSR | ❌ | ❌ (HDRP有) | ✅ | ✅ |
| TAA/FXAA | ⚠️ FXAA ✅ / TAA ❌ | ✅ | ✅ TSR | ✅ |
| DOF | ❌ | ✅ | ✅ | ✅ |
| Motion Blur | ❌ | ✅ | ✅ | ✅ |
| Auto Exposure | ✅ | ✅ | ✅ | ✅ |
| Color Grading | ⚠️ 仅 exposure+gamma | ✅ LUT | ✅ LUT | ✅ |

### 2.3 阴影对比

| 能力 | DSEngine | 主流引擎 |
|------|----------|----------|
| 方向光 | CSM 3 级 + PCSS 软阴影 ✅ | CSM 4-8 级 + PCSS/VSM + Contact Shadow |
| 点光源 | Cubemap Shadow Map | 同上 + 软阴影 |
| 聚光灯 | 单张 Shadow Map | 同上 |
| Virtual Shadow Maps | ❌ | Unreal 5 有 |
| 级联过渡 | ✅ smoothstep 混合 | ✅ 渐变混合 |

### 2.4 核心差距总结

1. ~~**光源扩展性**~~：✅ 已解决 — Clustered Forward+ 支持 256+256 光源
2. **屏幕空间效果部分缺失**：✅ SSAO 已实现；❌ SSR/SSGI 未实现
3. **后处理栈部分补齐**：✅ Bloom + SSAO + FXAA + Auto Exposure + Color Grading LUT + Vignette + Film Grain；❌ TAA/DOF/Motion Blur
4. **阴影质量已提升**：✅ PCSS 软阴影 + CSM 级联过渡 + Contact Shadow；后续可继续补更高阶阴影细节
5. **无间接光照**：Light Probe / Reflection Probe 组件已定义但未接入管线（⚠️ SH 管线已通，Bake 未实现）

---

## 三、前置依赖

> **✅ 前置依赖已满足（2026-05-11）。**
>
> 三后端统一 Shader 改造已完成并合入 master（commit d142240）。
> `engine/render/shaders/src/` 下的单份 GLSL 450 源文件通过 `tools/shader_compiler` 离线编译为
> SPIR-V / GLSL 330 / HLSL SM5，三后端 ShaderManager 均加载生成的头文件。
>
> **本方案的所有 shader 修改（pbr.frag 光源遍历、新增 SSAO/FXAA shader 等）直接在
> 统一源码上进行。可立即开始实施。**

---

## 四、分阶段优化方案

### Phase 1: Clustered Forward+ (解决光源扩展性 — 最高优先级) — ✅ 已完成

> **实现日期**: 2026-05-11
>
> 全部 4 个步骤已完成：SSBO 光源缓冲 + ClusterGrid CPU 分簇 + pbr.frag cluster 遍历 + RenderGraph 集成。
> 光源上限从 4+4 提升到 256+256，三后端均通过视觉回归验证。
>
> **关键文件**: `light_buffer.h/cpp`, `cluster_grid.h/cpp`, `pbr.frag`, `frame_pipeline.cpp`, `builtin_passes.cpp`

**目标**：突破 4+4 光源硬上限，支持数百动态光源，零 GBuffer 开销。

**为什么选 Clustered Forward+ 而不是 Deferred**：
- 当前引擎全部基于 Forward 管线，改动最小
- 不需要 GBuffer（节省带宽，对移动端友好）
- Unity URP 和 Godot 4 均选择了这条路径
- 透明物体天然支持，无需额外 Pass

**技术方案**：

```
┌──────────────────────────────────────────────────────┐
│                 Clustered Forward+ 架构                │
├──────────────────────────────────────────────────────┤
│                                                        │
│  1. Cluster 划分 (CPU 或 Compute)                      │
│     ┌─────────────────────────┐                        │
│     │ 屏幕 X×Y tile + Z 指数深度 │                     │
│     │ 典型: 16×16 tile, 24 Z切片  │                    │
│     │ → 总 cluster 数 ≈ 4096      │                    │
│     └─────────────────────────┘                        │
│                                                        │
│  2. Light Assignment (Compute Shader)                  │
│     ┌─────────────────────────┐                        │
│     │ 输入: 光源列表 (SSBO)       │                    │
│     │ 输出: cluster→light 映射    │                    │
│     │ 每 cluster 存影响它的光源ID  │                    │
│     └─────────────────────────┘                        │
│                                                        │
│  3. PBR Fragment Shader 修改                           │
│     ┌─────────────────────────┐                        │
│     │ 用 gl_FragCoord + depth   │                      │
│     │ 定位所属 cluster           │                     │
│     │ 只遍历该 cluster 的光源     │                    │
│     └─────────────────────────┘                        │
│                                                        │
└──────────────────────────────────────────────────────┘
```

**实现步骤**：

1. **新增 SSBO 光源缓冲**
   - 替换 `PerScene` UBO 中的 `MAX_POINT_LIGHTS=4` / `MAX_SPOT_LIGHTS=4`
   - 改为 SSBO（OpenGL 4.3+ / Vulkan / DX11 StructuredBuffer）
   - 上限提升到 256+ 光源

2. **新增 ClusterBuildPass (Compute)**
   - 在 PreZ 之后、Forward Scene 之前插入
   - 输入：深度缓冲（PreZ 已生成）、光源 SSBO、投影矩阵
   - 输出：`cluster_light_indices` SSBO + `cluster_light_offsets` SSBO
   - RenderGraph: `Read(prez_depth)`, `Write(cluster_data)`

3. **修改 pbr.frag**
   ```glsl
   // 替换暴力 for 循环
   ivec3 cluster_id = ComputeClusterId(gl_FragCoord, linearDepth);
   uint offset = cluster_light_offsets[cluster_id];
   uint count  = cluster_light_counts[cluster_id];
   for (uint i = 0; i < count; ++i) {
       uint light_idx = cluster_light_indices[offset + i];
       // ... PBR 计算
   }
   ```

4. **RenderGraph 集成**
   ```
   PreZ → Shadow → ClusterBuild → Forward Scene (读 cluster data) → Bloom → ...
   ```

**涉及文件**：
- `engine/render/passes/` — 新增 `cluster_build_pass.h/cpp`
- `engine/render/shaders/src/pbr.frag` — 改光源遍历逻辑
- `engine/render/rhi/rhi_types.h` — 新增 SSBO 资源类型（如尚未支持）
- `engine/render/passes/render_pass_context.h` — 新增 cluster 相关 RT/Buffer
- `engine/ecs/components_3d.h` — 移除 `MAX_POINT_LIGHTS` / `MAX_SPOT_LIGHTS` 硬编码

**风险与兼容性**：
- OpenGL 需要 4.3+（Compute + SSBO），当前引擎已要求 4.x
- DX11 需 CS 5.0（已有 `StructuredBuffer` 概念）
- 兼容降级：光源数 ≤8 时可回退旧路径

---

### Phase 2: 后处理栈扩充

**目标**：补齐工业级后处理效果链。按投入产出比排序。

#### 2.1 SSAO — Screen Space Ambient Occlusion — ✅ 已完成

> **实现日期**: 2026-05-11
>
> 三后端 SSAO shader + SSAOPass 已实现。通过 `PostProcessComponent::ssao_enabled` 控制开关。
> OpenGL / Vulkan / DX11 均验证通过。

**优先级**：最高（字段已预留，视觉效果提升最明显）

**方案**: GTAO (Ground Truth Ambient Occlusion) — Unreal 用的算法，质量/性能比最优。

**新增 Pass**: `SSAOPass`
- 输入：PreZ 深度纹理 + 法线（从 depth 重建或单独输出）
- 输出：AO 纹理（单通道 R8）
- 在 Forward Scene 之后、Composite 之前应用

**实现要点**：
- 需要从 PreZ 深度重建 View-Space 法线（无需额外 Normal Pass）
- 半分辨率计算 + 双边模糊上采样 → 性能友好
- 接入 `PostProcessComponent::ssao_enabled` / `ssao_radius` / `ssao_bias`

**新增文件**：
- `engine/render/passes/ssao_pass.h/cpp`
- `engine/render/shaders/src/ssao.frag`
- `engine/render/shaders/src/ssao_blur.frag`

#### 2.2 FXAA — 抗锯齿 — ✅ 已完成

> **实现日期**: 2026-05-11
>
> 三后端 FXAA shader + FXAAPass 已实现。在 Composite 之后、Present 之前执行。

**优先级**：高（实现简单，单 Pass fragment shader）

**方案**: FXAA 3.11 (NVIDIA) — 单次全屏 Pass，无需历史帧。

**新增 Pass**: `FXAAPass`（在 Composite 之后、Present 之前）

#### 2.3 TAA — Temporal Anti-Aliasing

**优先级**：中（需要 Motion Vector，依赖更多基础设施）

**前置条件**：
- 需新增 Motion Vector 输出（在 Forward Pass 中额外写一个 RT）
- 需要 Jitter（逐帧抖动投影矩阵）
- 需要历史帧缓冲

**建议在 FXAA 稳定后再替换为 TAA**。

#### 2.4 其他后处理（按优先级递减）

##### 2.4a Auto Exposure — ✅ 已完成

> **实现日期**: 2026-05-12

##### 2.4b Color Grading LUT — ✅ 已完成

> **实现日期**: 2026-05-13

##### 2.4c Vignette / Film Grain — ✅ 已完成

> **实现日期**: 2026-05-13
>
> 已复用现有 [`bloom_composite`](engine/render/passes/builtin_passes.cpp:615) / final composite 路径，
> 在 [`PostProcessComponent`](engine/ecs/components_3d.h:76) 中补充最小运行时参数，
> 并同步更新 OpenGL / Vulkan / D3D11 三后端 composite shader 与执行器参数绑定。


| 效果 | 复杂度 | 前置依赖 |
|------|--------|----------|
| Auto Exposure | 低 | Compute average luminance |
| DOF (景深) | 中 | 深度纹理（已有） |
| Motion Blur | 中 | Motion Vector（TAA 前置） |
| SSR | 高 | 需要 GBuffer 或 Hi-Z |
| Color Grading LUT | 低 | 无 |
| Vignette | 低 | 无 |
| Chromatic Aberration | 低 | 无 |
| Film Grain | 低 | 无 |

**建议的后处理管线顺序**：
```
Forward Scene → SSAO → SSR(未来) → Bloom → DOF(未来) → Motion Blur(未来)
  → Composite (合并 Scene + UI + AO) → Color Grading → FXAA/TAA → Present
```

---

### Phase 3: 间接光照 (Light Probe + Reflection Probe 接入)

**目标**：利用已定义的 `LightProbeComponent` 和 `ReflectionProbeComponent` 提供间接光照。

#### 3.1 Light Probe — 间接漫反射 (SH L2) — ⚠️ 管线已通，Bake 未实现

**当前状态**：
- ✅ `LightProbeComponent` 已有 `sh_coefficients[9]`（9 个 vec3，L2 球谐）
- ✅ `pbr.frag` 已实现 `EvaluateSH(N)` 函数 + `u_sh_enabled` 开关
- ✅ 三后端 UBO 管线已通（OpenGL `UBOManager::UploadLightProbeData`，Vulkan `light_probe_ubo_[]` 双缓冲，DX11 `light_probe_data_cb_` b9 槽位）
- ✅ `frame_pipeline.cpp` 每帧调用 `SetGlobalLightProbeSH()`，当前硬编码 `enabled=false`

**仍需实现**：
1. **Bake 系统**：在编辑器中，对每个 probe 位置渲染 6 面 cubemap → 积分为 SH L2
2. **运行时查询**：从 ECS 收集 `LightProbeComponent`，根据物体位置查找最近 probe，将 SH 系数传入 `SetGlobalLightProbeSH(sh, true)`
3. **Probe 混合**：根据物体位置找最近的 N 个 probe，按距离加权混合

**涉及修改**：
- `pbr.frag` — 替换 ambient 计算
- 新增 `probe_bake_system.cpp` — 编辑器 bake 流程
- 新增 probe 数据上传到 UBO/SSBO

#### 3.2 Reflection Probe — 间接高光

**当前状态**：`ReflectionProbeComponent` 已有 `cubemap_handle`、`box_size`、`use_box_projection`。

**需要实现**：
1. **Bake 系统**：渲染 cubemap + 生成预滤波 mipmap（Split-Sum 近似）
2. **运行时采样**：
   ```glsl
   vec3 R = reflect(-V, N);
   float lod = roughness * max_mip_level;
   vec3 prefiltered = textureLod(u_reflection_cubemap, R, lod).rgb;
   vec2 brdf = texture(u_brdf_lut, vec2(NdotV, roughness)).rg;
   vec3 specular_ibl = prefiltered * (F * brdf.x + brdf.y);
   ```
3. **Box Projection 修正**（已有字段）：
   ```glsl
   if (use_box_projection) R = BoxProjectedDirection(R, worldPos, probePos, boxMin, boxMax);
   ```

**新增资源**：
- BRDF LUT 纹理（2D，预计算，512×512 RG16F）— 构建时一次生成
- 预滤波 cubemap（Probe Bake 时生成）

---

### Phase 4: 阴影质量提升

#### 4.1 CSM 级联过渡 — ✅ 已完成

> **实现日期**: 2026-05-11
>
> `pbr.frag` 已实现 smoothstep 级联边界混合：在当前级联范围末尾 20% 区域
> 使用 `smoothstep(blendStart, splitEnd, viewDepth)` 混合到下一级联。

**原问题**：`pbr.frag` 中级联切换是硬切（`if abs(z) < split → cascadeIndex`），边界有明显接缝。

**方案**：在级联边界区域混合两级 shadow 值：
```glsl
float blend = smoothstep(split[i] - margin, split[i] + margin, abs(z));
shadow = mix(shadow_cascade[i], shadow_cascade[i+1], blend);
```

改动量：~20 行 shader 代码。

#### 4.2 PCSS 软阴影 — ✅ 已完成

> **实现日期**: 2026-05-12
>
> `pbr.frag` 新增 PCSS 实现：16 点 Poisson disk 采样、`FindBlockerDepth()` 通过
> `sampler2DShadow` 比较结果 + 3 步二分法近似遮挡体深度、`PCSS_Shadow()` 根据半影宽度
> 做可变核 Poisson PCF。`ShadowForCascade()` 从 `SampleShadowPCF()` 切换到 `PCSS_Shadow()`。
> CSM 级联 smoothstep 混合和 `u_shadow_strength` 保持不变。
> 三后端验证通过：VK 22.2 / GL 22.3 / DX11 22.1 (vs KF RMSE)，无回归。

已实现 PCSS (Percentage Closer Soft Shadows)：
- Blocker search (二分法近似) → 估算半影宽度 → 可变核 Poisson PCF
- 阴影远处柔和、近处锐利，更真实

#### 4.3 Contact Shadow — ✅ 已完成

> **实现日期**: 2026-05-13
>
> 已在 [`ContactShadowPass::Execute()`](engine/render/passes/builtin_passes.cpp:790) 中完成 screen-space ray march 接入，
> 并在 [`CompositePass::Execute()`](engine/render/passes/builtin_passes.cpp:542) 中与 final composite 合流。

在 Forward Scene 之后做一次 Screen-Space Ray March：
- 从像素沿光源方向步进深度缓冲
- 补充近距离微小遮挡细节（CSM 分辨率不足的区域）

---

### Phase 5: 可选 Deferred 路径（中长期）

**当 Phase 1-4 完成后**，如果需要支持数千光源或高级屏幕空间效果，可引入 Deferred 路径。

**GBuffer 布局建议**：

| RT | 格式 | 内容 |
|----|------|------|
| RT0 | RGBA8 | Albedo.rgb + MaterialID |
| RT1 | RGB10A2 | Normal.xyz (Octahedron 编码) + Metallic |
| RT2 | RGBA8 | Roughness + AO + Emissive flag + Spare |
| Depth | D24S8 | 深度 + Stencil (标记 Deferred/Forward 物体) |

**管线切换**：
```
if (render_path == Deferred)
    PreZ → GBuffer → Shadow → Deferred Lighting → Transparent Forward → PostProcess
else
    PreZ → Shadow → ClusterBuild → Forward+ Scene → PostProcess
```

通过 RenderGraph 动态组装，IModule 无需关心具体路径。

---

## 五、实施路线图

```
阶段        时间估算    核心交付物                      状态
─────────────────────────────────────────────────────────────
Phase 1     2-3 周     Clustered Forward+              ✅ 已完成 (2026-05-11)
Phase 2.1   1 周       SSAO                            ✅ 已完成 (2026-05-11)
Phase 2.2   2-3 天     FXAA                            ✅ 已完成 (2026-05-11)
Phase 4.1   2-3 天     CSM 级联过渡                    ✅ 已完成 (2026-05-11)
Phase 4.2   1 周       PCSS 软阴影                     ✅ 已完成 (2026-05-12)
Phase 2.4a  1-2 天     Auto Exposure                   ✅ 已完成 (2026-05-12)
Phase 2.4b  1 天       Color Grading LUT               ❌ 未开始
Phase 2.4c  0.5 天     Vignette / Film Grain           ❌ 未开始
Phase 4.3   3 天       Contact Shadow                  ❌ 未开始
Phase 3.1   1-2 周     Light Probe SH Bake + 运行时     ⚠️ 管线就绪，Bake 未实现
Phase 3.2   1-2 周     Reflection Probe + IBL          ❌ 未开始
Phase 2.3   2 周       TAA                             ❌ 未开始
Phase 5     4-6 周     可选 Deferred 路径               ❌ 未开始
```

**下一步建议执行顺序**（编辑器相关暂缓）：
1. Phase 2.4a-c (低复杂度后处理: Auto Exposure → Color Grading LUT → Vignette)
2. Phase 4.3 (Contact Shadow)
3. Phase 3.1 (Light Probe Bake — 运行时部分，不含编辑器 UI)
4. Phase 3.2 (Reflection Probe + IBL)
5. Phase 2.3 (TAA)
6. Phase 5 (Deferred 路径)

---

## 六、每阶段的验证标准

### Phase 1 验证
- [x] KF_Framework demo 场景光源上限从 4+4 提升到 256+256
- [x] 三后端 SSBO 创建/上传/绑定正常
- [x] pbr.frag cluster 遍历逻辑正确，视觉回归通过
- [x] OpenGL/Vulkan/DX11 三后端均通过 (RMSE: VK 21.8, GL 22.3 vs KF)

### Phase 2.1 验证
- [x] SSAO 开关不影响已有渲染正确性
- [x] 三后端 SSAO shader 创建成功
- [x] Vulkan 日志确认 SSAO(540006)/SSAO_blur(540007) 创建

### Phase 3 验证
- [ ] Light Probe bake 后，关闭方向光时物体仍有环境光照
- [ ] Reflection Probe 金属球面可见环境反射
- [ ] Probe 之间切换无跳变（距离混合）

### Phase 4.1 验证
- [x] CSM 级联边界 smoothstep 混合，无硬切接缝
- [x] 性能影响可忽略

---

## 七、对现有代码的影响评估

| 文件/模块 | Phase 1 | Phase 2 | Phase 3 | Phase 4 | Phase 5 |
|-----------|---------|---------|---------|---------|---------|
| `pbr.frag` | **大改** (光源遍历) | 小改 (AO 采样) | 中改 (IBL) | 小改 (阴影) | 中改 (GBuffer 输出) |
| `builtin_passes.h/cpp` | +1 Pass | +2-3 Pass | 无 | 小改 | +3-4 Pass |
| `render_pass_context.h` | +SSBO handles | +AO RT | +Probe data | 无 | +GBuffer RTs |
| `render_graph.h` | 无 | 无 | 无 | 无 | 无 |
| `frame_pipeline.h/cpp` | 小改 | 小改 | 中改 | 无 | 中改 |
| `rhi_device.h` | +SSBO API | 无 | 无 | 无 | +MRT 支持 |
| `components_3d.h` | 移除硬编码上限 | 无 | 无 | 无 | 无 |
| 三后端 RHI 实现 | +SSBO 实现 | 无 | 无 | 无 | +MRT 实现 |

**核心不变量**：
- RenderGraph 架构无需改动（新 Pass 通过 `AddPass` + `Read/Write` 自然融入）
- IModule 接口无需改动（模块通过 `RegisterRenderPasses` 注入自定义 Pass）
- ECS 组件只增不改（新增 ClusterLightData，不修改已有组件）

---

## 八、技术决策记录

| 决策 | 选择 | 理由 |
|------|------|------|
| Forward+ vs Deferred | **Forward+ 优先** | 改动最小、透明物体友好、RenderGraph 已就绪 |
| SSAO 算法 | **GTAO** | 质量/性能最优平衡（Unreal 同款） |
| AA 方案 | **FXAA → TAA** | FXAA 零依赖先上线；TAA 等 Motion Vector 就绪后替换 |
| IBL 方案 | **Split-Sum + BRDF LUT** | 工业标准（Epic Games 论文），与 Cook-Torrance BRDF 完美匹配 |
| Cluster 划分 | **16×16 tile × 24 Z-slice (指数)** | 与 Doom 2016 / id Tech 方案对齐，Z 用指数划分匹配透视投影 |
| 软阴影 | **PCSS** | 比 VSM 更直观、无 light bleeding 问题 |
