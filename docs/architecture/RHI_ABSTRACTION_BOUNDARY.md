# RHI 抽象边界重画 —— 进度与现状（A1 → B0 → B1 → B2a → B2b）

> 本文结合**提交历史**与**当前代码现状**，描述 RHI 抽象边界从「每效果一个虚函数」收敛为「后端无关通用原语 + 高层渲染器」的重画过程、已落地范围、以及剩余边界。
> 配套文档：契约设计见 [`RHI_PRIMITIVE_CONTRACT.md`](./RHI_PRIMITIVE_CONTRACT.md)（终态接口 + Metal 映射）；mesh 迁移摸底见 [`../plans/B2b_mesh_migration_scoping.md`](../plans/B2b_mesh_migration_scoping.md)。
> 分支：`feature/engine-lib`。基线：单元 2281 / 集成 544 / smoke **181**（三后端 llvmpipe/WARP/lavapipe 像素 smoke 全绿；B2a 时为 68，P0+B2b-2..5 累加至 92，2c-1..5 + Final-Feat-1..7 续加至 181）。
> HEAD：`feature/engine-lib`；**阶段 2b 后处理迁移已完成**——`DrawPostProcess` ABI 已全面删除，bloom mip 链经 `BloomRenderer` 走 CommandBuffer 级 `DispatchComputePass` 通用原语（Option A），见 §5B。Mesh 线（B2b/2c/Final-Feat）代码已在本分支，时间线 HEAD 曾为 `db8c320b`。

---

## 1. 重画目标（一句话）

把 `CommandBuffer` 上**逐效果的虚函数**（`DrawSkybox/DrawSpriteBatch/DrawMeshBatch/DrawPostProcess/DrawParticles3D/DrawHairStrands`）替换为**一组有限的、后端无关的通用绘制原语**（pipeline / bindings / draw），效果逻辑上移到跑在原语上的**高层渲染器**。

成本模型：**O(效果数 × 后端数) → O(效果数 + 后端数)**。

```
重画前（每效果一个 ABI，各后端各实现一遍）        重画后（边界下沉到通用原语）
┌─────────────── 高层 ───────────────┐         ┌──────────── 高层渲染器 ────────────┐
│ Scene / UI / Particle / ...        │         │ SkyboxRenderer SpriteBatchRenderer PostProcessRenderer│
└───────────────┬────────────────────┘         │ MeshRenderer（B2b+2c+Feat，ABI 并存）│
   cmd.DrawSkybox/DrawSpriteBatch/...           └──────────────┬─────────────────────┘
┌───────────────┴─ CommandBuffer ABI ┐            BindShaderProgram/BindVertexBuffer/
│ DrawSkybox  DrawSpriteBatch  DrawMesh│  ===>     BindTexture/BindUniformBuffer/
│ DrawPostProcess  DrawHairStrands     │           PushConstants/DrawIndexed/...
└──┬───────────┬───────────┬───────────┘        ┌──────────────┴─ CommandBuffer 原语 ─┐
  GL          DX11       Vulkan                  │  GL    DX11    Vulkan（各实现一组原语）│
 (各效果 × 各后端 = N×M 份实现)                   └─────────────────────────────────────┘
```

---

## 2. 提交时间线（重画的推进轨迹）

| 阶段 | 提交 | 内容 | 边界变化 |
|---|---|---|---|
| **A1**（spike+转正） | `3c8b4043` → `2db42b8b` → `d885d0eb` | DrawSkybox 改通用原语 → 三后端跑通 → **删 DrawSkybox ABI**，probe capture 迁 `SkyboxRenderer` | 首个效果下沉；首条 ABI 删除 |
| **B0**（契约 v1） | `2d059e49` | 落地原语契约 v1（`BindIndexBuffer/BindTexture(2D)/BindUniformBuffer/DrawIndexed` + `GetSprite2DShaderProgram`），新增非生产 `SpriteRenderer` 脚手架 + 跨后端像素 smoke | 原语集扩展（只增不删） |
| **B1**（像素闸门） | `560fc7d4` | 抽 `rhi_pixel_harness`（三后端离屏 GPU 上下文样板），skybox 跨后端像素 smoke | 建立后续每次迁移的回归闸门 |
| **B2a**（迁 Sprite） | `11d61181`（默认）→ `dc356ea5`（SDF）→ `f011ba3a`（VFX）→ `fc21e1b5`（迁 3 调用点）→ `43240e8e`（**删 DrawSpriteBatch ABI**） | 抽生产级 `SpriteBatchRenderer`（默认/SDF/VFX 全特性，SDF/VFX 参数走 push-block UBO），迁 SpriteRenderSystem/UIRenderSystem/ParticleSystem，删旧 ABI + 失效测试/mock/文档 | 第二个效果下沉；第二条 ABI 删除 |
| **P0**（mesh 前置原语） | `f38d0b13`→`e0f061f7`（P0a）/ `10f3ff2d`+`68c81b84`（P0b） | P0a `DrawIndexedInstanced` 三后端；P0b 图形阶段 `BindStorageBuffer(slot,handle,offset,size)` 三后端 + 组合像素 smoke | 原语集扩展（实例化 + SSBO，只增不删） |
| **B2b**（抽 MeshRenderer，ABI 并存） | `ee97d1ab`→`88301df8`→`34bb4328`→`eaf61c2d`→`25fb30a6` | 后端无关 `MeshRenderer`：B2b-1 静态 PBR / B2b-2 蒙皮(bone SSBO) / B2b-3 实例化(instance SSBO) / B2b-4 depth-only / B2b-5 间接(`DrawIndexedIndirect`)；各配跨后端像素 smoke | 第三效果**部分**下沉；`DrawMeshBatch` ABI **暂保留**（高级 shading 未迁） |
| **2c + Final-Feat**（MeshRenderer 高级 shading + 几何来源） | `debcaead`→`7295d247`→`b930db38`→`5a46be01`→`1048b815`→`5e460e2e`→`fb986188`→`1c909e5a`→`c9b516e3`→`f4645f46`→`44c1f487`→`db8c320b` | MeshRenderer 补：shading_mode 0/2-6 + SSS/clearcoat/POM、clustered 点光、地形 splat/积雪、WBOIT、DDGI/LightProbe、CSM 阴影、蒙皮/实例化/聚光/morph 高级 shading、外部常驻 VAO/EBO（tiled terrain）、共享网格模板去重（tree）；各配跨后端像素 smoke（92→181） | MeshRenderer 能力基本补齐；`DrawMeshBatch` ABI 仍并存（调用点未迁） |
| **B3**（迁粒子） | `c8f84edf`→`5e8fbe30`→`171aff31` | 抽 `ParticleRenderer`（`BuiltinProgram::Particle3D`，SSBO 实例化），迁调用点 + **删 `DrawParticles3D` ABI** + 三后端死代码清理（DX11 particle_quad / Vulkan particle_vbo） | 第四条 ABI 删除 |
| **阶段 2b**（迁后处理，✅ 已完成） | `ac4bcd2d`（闸门）→`edc3a3d5`/`012570c3`→…→`b7e7e900`→`a191e6ef`→`f959d65c`→收尾提交 | 抽 `PostProcessRenderer`（全屏 quad/UBO/纹理/PSO），逐效果迁移配方（.frag→std140 UBO + device 访问器 + 调用点 + D3D11 像素闸门），3D LUT 基础设施（`TextureDim::Tex3D`）+ `source==0` 支持；迁全部 **29** 效果（含 bloom_composite/atmosphere_sky/ui_overlay 手写 HLSL 簇）；compute mip 链选 **Option A**——新增 CommandBuffer 级 `DispatchComputePass` 原语 + `BloomRenderer`（compute/quad 分支）；**已删 `DrawPostProcess` ABI** | ✅ 三后端编译零错误 + ctest 三套件全绿 + D3D11 atmosphere_sky/bloom_composite 像素闸门绿 |

**B2a 关键设计**：三个 2D 系统各持独立 `SpriteBatchRenderer` 实例（每帧各画一次 → 各自动态 VBO 互不别名，满足 Vulkan 帧提交生命周期）；view/proj 经新增的 `CommandBuffer::GetView/GetProjectionMatrix()`（读 `SetCamera` 缓存）取用。

---

## 3. 当前边界现状（逐行核对 `rhi_device.h`）

### 3.1 通用绘制原语（边界**之下**，三后端各实现一组）

| 原语 | 引入 | 现签名（`CommandBuffer`） | 状态 |
|---|---|---|---|
| `BindShaderProgram(program)` | A1 | `(unsigned int)` | ✅ 三后端 |
| `BindVertexBuffer(buffer, stride, attrs)` | A1 | 单 slot、无 rate | ✅ 三后端（**未含** slot/PerInstance） |
| `BindTextureCube(slot, h)` | A1 | — | ✅（B0 起由 `BindTexture(…,TexCube)` 归并） |
| `PushConstantsMat4(m)` | A1 | 仅 mat4 | ✅ 三后端（**未**泛化为字节块 `PushConstants`） |
| `Draw(vertex_count, first_vertex)` | A1 | **无** instance 参数 | ✅ 三后端 |
| `BindIndexBuffer(buffer, IndexType)` | B0 | — | ✅ 三后端 |
| `BindTexture(slot, h, TextureDim)` | B0 | 2D/Cube/2DArray | ✅ 三后端 |
| `BindUniformBuffer(slot, h, offset, size)` | B0 | — | ✅ 三后端 |
| `DrawIndexed(index_count, first_index, base_vertex)` | B0 | **无** instance 参数 | ✅ 三后端 |
| `GetView/GetProjectionMatrix()` | B2a | 读 `SetCamera` 缓存 | ✅（高层渲染器取相机矩阵） |
| `DrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance)` | P0a | 实例化重载（不改 `DrawIndexed`） | ✅ 三后端（mesh 实例化/蒙皮消费） |
| `BindStorageBuffer(slot, handle, offset, size)` | P0b | 图形阶段读 SSBO | ✅ 三后端（GL SSBO / DX11 StructuredBuffer SRV 仅绑 VS / Vulkan storage descriptor） |
| `DrawIndexedIndirect(indirect_buffer, byte_offset)` | B2b-5 | CommandBuffer 级间接 | ✅ 三后端（GL glMultiDrawElementsIndirect / Vulkan vkCmdDrawIndexedIndirect / DX11 DrawIndexedInstancedIndirect） |
| `DispatchComputePass(ComputeDispatch)` | 阶段 2b | 通用 compute 调度（源 SRV→当前 RT 作 UAV/storage） | ✅ compute 后端（DX11 FL11+ / Vulkan）；GL 经 `GetBloomComputeShader()==0` 由 `BloomRenderer` 回退全屏 quad，不调该原语 |

内建资源访问器（`RhiDevice`，供高层渲染器复用引擎着色器/几何）：`GetBuiltinProgram(BuiltinProgram)`（按枚举取 Skybox/Sprite2D/SpriteFxSdf/SpriteFxVfx + **ForwardPbr/ForwardPbrSkinned/ForwardPbrInstanced/ForwardPbrDepth**（B2b-1..5）+ **ForwardShaded/ForwardSkinnedShaded/ForwardInstancedShaded/ForwardMorphShaded**（2c-1 + Final-Feat-2/3/5 高级 shading）程序，取代每效果一个访问器——见 §8.2 D4）+ `GetSkyboxCubeVertexBuffer`；间接缓管理 `CreateIndirectBuffer/UpdateIndirectBuffer/DeleteIndirectBuffer`（B2b-5）。

> **说明（2c + Final-Feat）**：高级 shading 与几何来源能力均落在 `MeshRenderer` 层（`DrawShaded` 系列 + Final-Feat-6 `DrawShadedExternal`/`BuildShadedWorldVertexBuffer` + Final-Feat-7 `DrawSharedTemplateInstanced`/`BuildShadedLocalVertexBuffer`），**未新增任何 CommandBuffer 原语**——全部复用既有 `DrawIndexed/DrawIndexedInstanced/BindStorageBuffer/BindUniformBuffer/BindVertexBuffer/BindIndexBuffer`，故 §3.1 原语表不变。

### 3.2 仍存的逐效果 ABI（边界**之上**待下沉）

| 旧 ABI（`CommandBuffer` 纯虚） | 迁移阶段 | 状态 |
|---|---|---|
| ~~`DrawSkybox`~~ | A1 | ✅ 已删（`d885d0eb`） |
| ~~`DrawSpriteBatch`~~ | B2a | ✅ 已删（`43240e8e`） |
| `DrawMeshBatch(items)` | **B2b** | 🔶 `MeshRenderer` 已**并存**承接 forward-PBR + 高级 shading 全模式（B2b-1..5 + 2c-1..5 + Final-Feat-1..7）；ABI 删除**仍推迟**——能力已基本齐备，剩迁 6 调用点 + spine 2D 蒙皮缺口（见 §5） |
| ~~`DrawParticles3D(items, view, proj)`~~ | B3 | ✅ 已删（`c8f84edf` + `5e8fbe30`，迁至 `ParticleRenderer` + `BuiltinProgram::Particle3D`） |
| `DrawHairStrands(items, view, proj)` | B4 | ⏳ 待迁 |
| ~~`DrawPostProcess(request)`~~ | **阶段 2b** | ✅ 已删（阶段 2b 收尾提交）；全部 29 效果迁至 `PostProcessRenderer`，bloom mip 链经 `BloomRenderer` 走 `DispatchComputePass` 原语（见 §5B） |

### 3.3 高层渲染器（边界**之上**，跑在原语上）

| 渲染器 | 文件 | 性质 |
|---|---|---|
| `SkyboxRenderer` | `engine/render/skybox_renderer.{h,cpp}` | 生产（A1，DrawSkybox 已删） |
| `SpriteBatchRenderer` | `engine/render/sprite_batch_renderer.{h,cpp}` | 生产（B2a，DrawSpriteBatch 已删） |
| `SpriteRenderer` | `engine/render/sprite_renderer.{h,cpp}` | B0 契约验证脚手架（非生产） |
| `MeshRenderer` | `engine/render/mesh_renderer.{h,cpp}` | 后端无关 forward 能力：B2b-1..5（静态/蒙皮/实例化/depth-only/间接）+ 2c-1..5 与 Final-Feat-1..7（高级 shading 全模式/CSM/聚光/morph、地形 splat+积雪、WBOIT、DDGI/LightProbe、外部常驻 VAO/EBO、共享网格模板去重）；与 `DrawMeshBatch` ABI **并存**，**尚未取代**（调用点未迁、spine 2D 蒙皮未覆盖） |
| `ParticleRenderer` | `engine/render/particle_renderer.{h,cpp}` | 生产（B3，DrawParticles3D 已删，SSBO 实例化） |
| `PostProcessRenderer` | `engine/render/post_process_renderer.{h,cpp}` | 生产（阶段 2b，✅ `DrawPostProcess` 已删）：全屏 quad + std140 UBO（set=2,b0）+ 纹理（2D/3D）+ PSO；承接全部 29 个全屏后处理效果 |
| `BloomRenderer` | `engine/render/bloom_renderer.{h,cpp}` | 生产（阶段 2b）：bloom mip 链高层封装，按 `device.GetBloomComputeShader()` 分支——compute 后端（DX11/Vulkan）经 `cmd.DispatchComputePass()`，GL 回退 `PostProcessRenderer` 全屏 quad（down/upsample.frag） |

---

## 4. 契约终态 vs 已实现（差距分析）

[`RHI_PRIMITIVE_CONTRACT.md`](./RHI_PRIMITIVE_CONTRACT.md) §3 描述的是**完整终态**；按「每个原语都要有活体消费者 + 像素覆盖、不留死代码」的原则（§7），尚未落地的原语**与其消费者同批实现**：

| 终态原语（契约 §3） | 现状 | 阻塞/排期 |
|---|---|---|
| `BindStorageBuffer(slot, h, offset, size)`（图形阶段读 SSBO） | ✅ **已实现**（P0b `10f3ff2d`，三后端） | 提前于 B4 落地（mesh 蒙皮/实例已消费，B2b-2/3）；hair（B4）待消费 |
| 泛化 `PushConstants(stage, offset, data, size)`（取代 `PushConstantsMat4`） | 仅 `PushConstantsMat4` | 待 mesh 等需要非 mat4 push 数据时落地 |
| 实例化 `DrawIndexedInstanced`（`instance_count`/`first_instance`） | ✅ **已实现**（P0a `f38d0b13`，新增重载不改现签名） | mesh 实例化已消费（B2b-3）；particles（B3）待消费 |
| 间接绘制 `DrawIndexedIndirect`（CommandBuffer 级） | ✅ **已实现**（B2b-5 `25fb30a6`，三后端） | mesh 间接已消费（`MeshRenderer::DrawIndirect`） |
| slot 化 `BindVertexBuffer(slot, …, rate)`（PerVertex/PerInstance） | 单 slot、无 rate | 同上，随实例化落地 |

> push-block UBO 决策（契约 §8.2）已在 **B2a 为 sprite 实现**：SDF/VFX 参数走单个 `SpriteFx` UBO（binding 0，三后端同布局映射），无需 per-backend push constant 分叉。mesh 迁移可复用该模式。

---

## 5. B2b 现状：原语 + 高级 shading 能力均已补齐，剩调用点迁移 + 删 ABI

`DrawMeshBatch` 是**引擎整个 3D 渲染核心**（三后端各 ~700–1115 行），覆盖 forward PBR / deferred gbuffer / shadow depth-only / 6 种 shading 模型 / 骨骼蒙皮 / GPU 实例化 / GPU-driven 间接绘制 / DDGI / 光探针 / 地形 splat / 积雪 / WBOIT，调用点 6 处（mesh/terrain×2/tree/grass/spine）。

**原阻塞已解**：B2b 原本被三项未实现原语阻塞，现均已落地（见 §3.1）：
1. **SSBO 绑定**（骨骼/实例）—— P0b `BindStorageBuffer` 提前到 B2b 之前落地（`10f3ff2d`）；
2. **实例化 DrawIndexed** —— P0a `DrawIndexedInstanced` 新增重载（`f38d0b13`）；
3. **间接绘制** —— B2b-5 `DrawIndexedIndirect` CommandBuffer 级原语（`25fb30a6`）。

据此抽出后端无关 `MeshRenderer`，逐子步闭环：B2b-1 静态 PBR / B2b-2 蒙皮 / B2b-3 实例化 / B2b-4 depth-only / B2b-5 间接，各配跨后端像素 smoke（smoke 76→92）。

**高级 shading 阻塞已解**：原「剩余阻塞」是 `MeshRenderer` 缺高级特性、调用点无法迁。2c-1..5 + Final-Feat-1..7 已补齐（smoke 92→181，各配跨后端像素 smoke）：
- 2c-1/2c-2：shading_mode 0/2-6（HalfLambert/Toon/Watercolor/FaceSDF）+ SSS/clearcoat/anisotropy/POM + ≤64 clustered 点光；
- 2c-3/2c-4/2c-5：地形 splatmap + 积雪、WBOIT、DDGI/LightProbe；
- Final-Feat-1..5：CSM 方向光阴影、蒙皮/实例化/聚光/morph 的高级 shading 变体；
- Final-Feat-6/7：外部常驻 VAO/EBO + index_count_override（tiled terrain）、共享网格模板去重（tree foliage）。

**剩余工作（ABI 删除，B2b-6）**：能力已基本齐备，剩的不再是「能力缺口」而是工程收尾——① 补 `spine_system` 2D 蒙皮（唯一未覆盖能力）；② 迁 6 个调用点（mesh_render_system / terrain×2 / tree / grass / spine）到 `MeshRenderer`；③ 全仓 `grep` 确认无残留调用后删 `DrawMeshBatch` ABI + 失效测试/mock/文档。

**用户决策（选项 B）**：当前**保留 `DrawMeshBatch` ABI 与 MeshRenderer 并存**，ABI 删除待上述收尾。拆解见 [`../plans/B2b_mesh_migration_scoping.md`](../plans/B2b_mesh_migration_scoping.md)。

---

## 5B. 阶段 2b：DrawPostProcess → PostProcessRenderer（✅ 已完成，ABI 已删）

`DrawPostProcess` 是约 30 个全屏后处理效果的总入口，调用点散落在 `builtin_passes.cpp` + `atmosphere_sky_pass.cpp` + editor。三后端「参数上传」实现发散（GL 按 SPIRV-Cross 名 `glUniform`、Vulkan push_constant、DX11 cbuffer，且每效果有特判 binder），是典型的 O(效果 × 后端) 成本。

**迁移配方**（每效果原子闭环）：① `.frag` 参数块 `push_constant → std140 set=2,binding=0 UBO`（vec2/vec3/mat4 一律标量化，避免 std140 填充与调用点紧排 float 数组错位）；② device 访问器 `GetGenPPShaderProgram(effect)→handle`（DX11/Vulkan 显式映射，GL 走泛型名查表）；③ 迁调用点到 `PostProcessRenderer::Draw`（混合效果 `blend=true`，额外纹理经 `.Tex(slot)` / `.Tex3D(slot)`）；④ 加 D3D11 像素闸门。

**基础设施**：2b-① 跨后端 PP 像素闸门 `postprocess_pixel_smoke_test`（passthrough/tonemapping 解析真值基线）；2b-② `RhiDevice::GetGenPPShaderProgram` + `PostProcessRenderer`（全屏 quad/UBO/纹理/PSO，仿 `SpriteBatchRenderer`）；3D LUT 基础设施（`TextureDim::Tex3D` 三后端 + 渲染器 `is_3d` + GL `GL_TEXTURE_3D`）；渲染器支持 `source_texture==0`（无源纹理程序化效果，如 atmosphere_transmittance_lut）。

**已迁全部 29 效果**（device 访问器登记）：passthrough、copy、ui_overlay、fxaa、bloom_extract、lum_compute、ssao_blur、ssao、contact_shadow、edge_detect、lum_adapt、dof、motion_blur、ssr、taa_resolve、motion_vector、volumetric_fog、volumetric_cloud、water、decal、wboit_composite、weather_particle、sss_blur、tonemapping、ssao_apply、light_shaft、atmosphere_transmittance_lut、**bloom_composite**、**atmosphere_sky**。上一轮剩余的三个手写 HLSL 簇（bloom_composite / atmosphere_sky / ui_overlay@CompositePass）本轮全部收敛为单一 gen-PP UBO 着色器：bloom_composite 收敛为 16 浮点 `BloomCompositeAeParams`（按纹理在否派生使能标志，纹理经 `.Tex/.Tex3D` 挂载），atmosphere_sky 核实其 cbuffer/SRV 寄存器与渲染器约定一致后直接走 gen-PP DXBC。

**compute mip 链架构岔口——选 Option A（已落地）**：bloom_downsample/upsample 后端实现发散（DX11/Vulkan = compute 写 UAV，GL = 全屏 quad ping-pong）。已将设备级 `DispatchCompute` 提升为 **CommandBuffer 级通用原语 `DispatchComputePass(ComputeDispatch)`**（语义：源 SRV binding0 → 当前 RT 颜色附件作 UAV/storage image，8×8 tile 推导线程组，blend_weight 供 upsample 累加），三后端 override；DX11/Vulkan 消费，GL 默认空实现。新增高层 **`BloomRenderer`**：按 `device.GetBloomComputeShader(upsample)` 返回值分支——非 0（DX11/Vulkan）走 `cmd.DispatchComputePass()`，返回 0（GL）回退 `PostProcessRenderer.Draw()` 全屏 quad（bloom_downsample/upsample.frag 参数块 push_constant→std140 set=2 binding=0）。无需新写不可验证的 GL compute 着色器。

**删 `DrawPostProcess` ABI（阶段 2b-④，已完成）**：全仓 `grep` 确认无生产调用点后，删：CommandBuffer 纯虚 `DrawPostProcess`（`rhi_device.h`）+ 三后端 command_buffer forwarder + executor 实现（`dx11/gl/vulkan_draw_executor` 含 GL `kEffectTable`/各 `Bind*` binder）+ device `RealSubmitDrawPostProcess` 转发。`GetGenPPShaderProgram` 保留（渲染器仍用）。同步修改失效测试/mock：三后端无设备 safety 单测改调 `DispatchComputePass`；集成 `BloomPassTest` 改断言 `DispatchComputePass`（shader=99 降/88 升），`CompositePassTest` 改断言 stub 设备记录的 gen-PP 效果名（bloom 启用→bloom_composite、禁用→tonemapping，ui_overlay 恒取）；smoke 去 `RenderPP`（旧 ABI）路径，统一走 `PostProcessRenderer`。

**验证**：三后端编译零错误；ctest 三套件（unit/integration/smoke）全绿；新增 D3D11 像素闸门——`bloom_composite`（中性参数→ACES+gamma 灰阶，16 浮点 UBO 顺序校验）、`atmosphere_sky`（depth 早退→passthrough，双纹理绑定 + 36 浮点 UBO）均绿。**验证缺口**：本机仅 D3D11(WARP) 可跑像素；GL/Vulkan 无驱动一律 skip（既有状况）。GL/Vulkan 着色器/绑定改动（含 BloomRenderer GL quad 分支 + compute 原语）只能靠编译 + 镜像保证，运行像素验证待多后端 CI。

---

## 6. 不变量与闸门（每次迁移必守）

每个效果迁移是**独立闭环**：迁移 → 跨后端像素 smoke → 删该 ABI（含全部调用点 + 失效测试/mock/文档）→ 三后端绿 → 直接推 `feature/engine-lib`（无 PR）。

1. **三后端编译零错误**。
2. **gtest 不回归**（删 safety 单测导致的下降需记录新基线；B2a 已记：2287→2281，删 6 个针对 DrawSpriteBatch 的无设备 safety 单测）。
3. **该效果跨后端像素 smoke 三后端绿**（`rhi_pixel_harness`；教训：gtest 全绿 ≠ 像素正确）。
4. **删 ABI 前 `grep -rn` 全仓确认所有调用点已迁**（A1 踩坑：light_probe/reflection_probe 是隐藏 skybox 调用点）。
5. **Vulkan 资源生命周期**：被命令缓冲引用的资源不能在帧提交（fence）前删。

---

## 7. 剩余路线图（便于全局对照）

- **B2b**：抽 `MeshRenderer`——forward-PBR（B2b-1..5）+ 高级 shading 全模式（2c-1..5 + Final-Feat-1..7：toon/watercolor/SSS/FaceSDF + 地形 splat/积雪 + WBOIT + clustered 点光 + DDGI + CSM + 蒙皮/实例化/聚光/morph + 外部 VAO/EBO + 共享模板）均已迁入，与 `DrawMeshBatch` 并存。**剩：**补 spine 2D 蒙皮 → 迁 6 调用点 → 删 `DrawMeshBatch` ABI。
- **B3**：✅ 已删 `DrawParticles3D`——迁至 `ParticleRenderer` + `BuiltinProgram::Particle3D`（SSBO 实例化）。
- **阶段 2b**：✅ 已完成。`PostProcessRenderer` 承接全部 **29** 效果（含末三手写 HLSL 簇）；compute mip 链选 Option A——新增 CommandBuffer 级 `DispatchComputePass` 原语 + `BloomRenderer`（compute/quad 分支）；**已删 `DrawPostProcess` ABI**（三后端 executor + forwarder + 纯虚）。详见 §5B。
- **B4**：迁 `DrawHairStrands`（SSBO + 多段绘制）。
- **B5**：全局绑定收敛（shadow map / global uniforms / `BindShaderProgram`+PSO 聚合，偿还契约 §8.1 债务）。

---

## 8. 方案评估、技术债与优化空间（坦率版）

### 8.1 方案是否合理 / 是否最佳
整体方向**合理且符合业界做法**（slot-based 绑定 + 延迟组装 + 高层渲染器，类似 bgfx/sokol 的中间层定位）。三个工程决策尤其稳健：
- **每效果独立闭环 + 即删 ABI**：始终可发布、不留混合态、回归面小。
- **像素闸门**（`rhi_pixel_harness`）：直击「gtest 全绿 ≠ 像素正确」这一真实历史 bug 类别。
- **契约以最难消费者（mesh/hair）倒推**：避免为 skybox 这种简单效果定出不够用的接口。

但**不是唯一/绝对最优**。一个更激进的替代是直接上 **bind group / argument buffer**（WebGPU/Metal 风格：把多资源打包成一次绑定 + program 烘进 PSO），可一次性消除下面 8.2 的多条债务，但**改动面与风险远大**。当前 v1 选择「slot-based + 文档预留 bind group 升级路径」是务实的中间路线——**以可控的已知债务换取低风险增量推进**。结论：合理，非最优上限，但债务都已显式登记、有偿还阶段。

### 8.2 已知技术债（均已登记，非隐藏）
| # | 债务 | 影响 | 偿还 |
|---|---|---|---|
| D1 | **通用原语是 default no-op 虚函数**（未实现的后端静默空转，非编译失败） | 与历史黑屏同类的隐患：后端漏实现 → 静默不绘制 | 像素闸门兜底；终态可考虑改纯虚 + 显式 Mock |
| D2 | **`BindShaderProgram` 与 PSO 分离**（契约 §8.1） | Metal/DX12/Vulkan 把 shader 烘进 PSO，未来落 Metal 要返工 | B5 聚合为图形管线对象 |
| D3 | **过渡期重复原语**：`BindTextureCube` vs `BindTexture(…,TexCube)`、`PushConstantsMat4` 未泛化为 `PushConstants(stage,off,data,size)` | 两套写法并存，需纪律 | 迁移收尾 / B5 清理 |
| D4 | ~~`RhiDevice` 内建资源访问器随效果线性增长~~ → **已还**：归并为单个 `GetBuiltinProgram(BuiltinProgram)`，新增内建程序只加枚举值，不再加虚函数 | — | ✅ 已还（B2b 前置） |
| D5 | ~~**每系统各持一个 `SpriteBatchRenderer`**（3 套动态 VBO/IBO/UBO）→ 共享 frame-ring 分配器~~ | **复评后降级**：3 系统用不同相机/不同 pass（sprite=world、UI=ortho、particle），本无跨系统合批可言；显存节省也微小。原「优化」收益≈0 | 不单独做；真正问题见 D9 |
| D9 | **sprite 动态缓冲单缓冲，但引擎 2 帧在飞**（`MAX_FRAMES_IN_FLIGHT=2`）：`SpriteBatchRenderer` 每帧 `UpdateGpuBuffer` 覆写同一 `vbo_/ubo_/fx_ubos_`，而 `AcquireNextImage` 的 fence 只保证 N-2 帧完成 → 帧 N+1 可能在帧 N 仍被 GPU 读取时覆写。mesh 执行器已用 `MAX_FRAMES` 双缓冲规避，sprite 没有 | 真机 2 帧在飞下的潜在竞争（软渲掩盖，呼应 D7）；测试走离屏+fence 等待故不暴露 | **建议**：把 mesh 执行器既有的「每在飞帧缓冲」抽成可复用 helper，B2b 的 MeshRenderer 落地时一并做，再回填 sprite。非阻塞 |
| D6 | **`CommandBuffer::GetView/GetProjectionMatrix()`** 把相机状态缓存在命令缓冲上 | 概念上相机属 frame/scene context，轻微耦合泄漏 | 引入 FrameContext 时收敛 |
| D7 | **像素闸门为 VM 软渲（llvmpipe/WARP/lavapipe）** | RMSE 不复现真机；只验解析真值，真机专属 bug 可能漏 | 已知并接受；条件允许时补一次真机基线 |
| D8 | ~~**契约把 SSBO 排 B4，但 B2b(mesh) 先需 SSBO**~~ → **已解**：P0b 把 `BindStorageBuffer` 提前到 B2b 之前落地（`10f3ff2d`），mesh 蒙皮/实例已消费 | — | ✅ 已解 |
| D10 | **`MeshRenderer` 与 `DrawMeshBatch` ABI 并存**（现 MeshRenderer 已覆盖高级 shading 全模式 + 外部 VAO + 共享模板，仅差 spine 2D 蒙皮） | 两条 mesh 路径并存，需纪律；6 调用点仍走旧 ABI | 补 spine 2D 蒙皮 → 迁 6 调用点 → 删 ABI（用户决策：本阶段保留） |

### 8.3 优化空间（非阻塞，按收益排序）
1. **可复用「每在飞帧缓冲」helper**（解 D9，取代原 D5 设想）：把 mesh 执行器既有的 `MAX_FRAMES` 双缓冲模式抽成小工具，按 `current_frame_index_` 轮转动态顶点/UBO，满足 2 帧在飞的「提交前不覆写」约束。B2b 的 MeshRenderer 必然需要它，落地时一并做，再回填 sprite。
2. ~~**shader 注册表**（解 D4）~~ → **已落地**：`RhiDevice::GetBuiltinProgram(BuiltinProgram)` 取代逐 program 访问器。
3. **bind group / argument buffer**（契约 §2.3）：把 PerFrame/PerScene/PerMaterial + 多纹理打成一次绑定，减少状态切换；为 Metal/DX12 铺路。
4. **实例化/间接绘制原语**随 B2b/B3 落地（新增重载，不改现签名）。

### 8.4 结论
- **合理**：是；增量、可回归、债务显式。
- **有优化空间**：有（8.3）。D4（shader 注册表）已在 B2b 前还清；原 D5（共享分配器）经复评收益≈0 已降级，真正问题是 D9（sprite 单缓冲 vs 2 帧在飞），建议随 B2b 抽「每在飞帧缓冲」helper 一并解决。
- **最佳？**：是「低风险务实最优」，非「理论最优」（后者是 bind group + program/PSO 聚合的一次性大改）。
- **技术债**：未隐藏，8.2 全部登记并各有偿还阶段；唯一需即时决策的是 D8（B2b 的 SSBO 顺序）。
