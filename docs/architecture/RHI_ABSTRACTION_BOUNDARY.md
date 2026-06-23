# RHI 抽象边界重画 —— 进度与现状（A1 → B0 → B1 → B2a → B2b）

> 本文结合**提交历史**与**当前代码现状**，描述 RHI 抽象边界从「每效果一个虚函数」收敛为「后端无关通用原语 + 高层渲染器」的重画过程、已落地范围、以及剩余边界。
> 配套文档：契约设计见 [`RHI_PRIMITIVE_CONTRACT.md`](./RHI_PRIMITIVE_CONTRACT.md)（终态接口 + Metal 映射）；mesh 迁移摸底见 [`../plans/B2b_mesh_migration_scoping.md`](../plans/B2b_mesh_migration_scoping.md)。
> 分支：`feature/engine-lib`。基线：单元 2281 / 集成 544 / smoke **181**（三后端 llvmpipe/WARP/lavapipe 像素 smoke 全绿；B2a 时为 68，P0+B2b-2..5 累加至 92，2c-1..5 + Final-Feat-1..7 续加至 181）。
> HEAD：`feature/engine-lib`；**阶段 2b（后处理）+ 阶段 3（毛发）迁移均已完成**——`DrawPostProcess` ABI 全面删除，bloom mip 链经 `BloomRenderer` 走 CommandBuffer 级 `DispatchComputePass` 通用原语（Option A，见 §5B）；`DrawHairStrands` ABI 全面删除，毛发经 `HairRenderer` + `BuiltinProgram::HairStrand` 走 LINE_STRIP 拓扑 + vertexless SSBO 通用 `Draw`（见 §5C）。Mesh 线（B2b/2c/Final-Feat）代码已在本分支，时间线 HEAD 曾为 `db8c320b`。

---

## 1. 重画目标（一句话）

把 `CommandBuffer` 上**逐效果的虚函数**（`DrawSkybox/DrawSpriteBatch/DrawMeshBatch/DrawPostProcess/DrawParticles3D/DrawHairStrands`）替换为**一组有限的、后端无关的通用绘制原语**（pipeline / bindings / draw），效果逻辑上移到跑在原语上的**高层渲染器**。

成本模型：**O(效果数 × 后端数) → O(效果数 + 后端数)**。

```
重画前（每效果一个 ABI，各后端各实现一遍）        重画后（边界下沉到通用原语）
┌─────────────── 高层 ───────────────┐         ┌──────────── 高层渲染器 ────────────┐
│ Scene / UI / Particle / ...        │         │ SkyboxRenderer SpriteBatchRenderer PostProcessRenderer│
└───────────────┬────────────────────┘         │ MeshRenderer（B2b+2c+Feat+阶段4，ABI 已删）│
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
| **阶段 3（B4）**（迁毛发，✅ 已完成） | 阶段 3 收尾提交 | 新增 `PrimitiveTopology` 贯穿 PSO（LINE_STRIP）+ vertexless 通用 `Draw`（缺省 VBO + 补 UBO/SSBO 绑定）；三后端 shader manager 加 `InitHairStrandShader` + `BuiltinProgram::HairStrand`；抽 `HairRenderer`（组合 HairUniforms UBO\@set0.b0 + position/tangent SSBO\@set7.b0/b1，逐 strand `cmd.Draw`）；迁 `hair_system` 调用点；**已删 `DrawHairStrands` ABI** | ✅ 三后端编译零错误 + ctest 三套件全绿 + D3D11 `hair_pixel_smoke_test` 像素闸门绿 |
| **阶段 4（M1–M4）**（迁 mesh，✅ 已完成） | `c86d8cbd`（M1）→`b1300c07`（M2）→`e47f9db8`（M3）→ M4 收尾提交 | 先补 MeshRenderer 三处覆盖缺口：**M1** 蒙皮×硬件实例化（bone-palette 去重，`BuiltinProgram::ForwardSkinnedInstancedShaded` + 实例/骨骼双 SSBO）/**M2** 编辑器视图模式（wireframe line-fill PSO / overdraw 加性 / force_unlit）/**M3** GBuffer/RSM MRT（`BuiltinProgram::GBufferMesh` + DX11 MRT 多附件绑定修复）；**M4** 加 `MeshRenderer::DrawBatch`（逐 item 据 skinned/instanced/morph + `current_pass_depth_only`/`gbuffer_rendering_mode`/编辑器标志路由到 M1–M3 Draw\* 方法，保留 shadow-cull 实例预算 + PreZ 蒙皮实例跳过；depth/gbuffer 复用 forward program，零新增着色器），迁 6 调用点（`render_scene` DrawOpaqueCpu/DrawTransparent → 常驻 `FramePipeline::cpu_mesh_renderer_`；terrain/tree/grass/spine/mesh_render 早已迁），**删 `DrawMeshBatch` ABI**（纯虚 + 三后端 forwarder + 三执行器实现 + `gl_draw_executor_mesh.cpp` 整文件 + GL `RealSubmitDrawMeshBatch`）+ 失效测试/mock/文档 | ✅ 三后端编译零错误 + ctest 三套件全绿 + D3D11 GBuffer/SkinnedInstanced/EditorViewMode/DepthOnly 像素闸门绿（GL/Vulkan 无驱动 skip） |

**B2a 关键设计**：三个 2D 系统各持独立 `SpriteBatchRenderer` 实例（每帧各画一次 → 各自动态 VBO 互不别名，满足 Vulkan 帧提交生命周期）；view/proj 由各 Pass 构造后经 `FrameContext` 显式下传给高层渲染器（**D6 已收尾**：早期曾挂在 `CommandBuffer::GetView/GetProjectionMatrix()` 读 `SetCamera` 缓存，现已移除——相机属帧/场景概念，不再挂命令记录器；见 §8.2 D6 与 `engine/render/frame_context.h`）。

---

## 3. 当前边界现状（逐行核对 `rhi_device.h`）

### 3.1 通用绘制原语（边界**之下**，三后端各实现一组）

| 原语 | 引入 | 现签名（`CommandBuffer`） | 状态 |
|---|---|---|---|
| ~~`BindShaderProgram(program)`~~ + ~~`SetPipelineState(pso)`~~ → `BindPipeline(handle)` | A1 | `(unsigned int)` 图形管线句柄 | ✅ **B5-3b 已聚合**：分离的「绑 program」+「设 PSO」收敛为单一图形管线对象 `GraphicsPipelineDesc{pso,program}`（`RhiDevice::GetGraphicsPipeline(pso,program)` 惰性去重缓存）；`BindPipeline` 恒应用 PSO 状态+拓扑、`program!=0` 时再绑 program（Pass 层 program==0 仅设状态，保留 GPU-driven 自绑/渲染器覆盖语义）。删两旧分离原语。为 Metal/DX12「shader 烘进 pipeline」铺路。详见 §8.1 D2 |
| `BindVertexBuffer(slot, buffer, stride, attrs, rate)` | A1（slot/rate 收尾 A） | slot 化 + `VertexInputRate` | ✅ 三后端（**含** slot/PerInstance）：GL `glVertexAttribDivisor` / DX11 多 slot `IASetVertexBuffers` + attr 自建 input layout / Vulkan 多 binding+rate 烘进 PSO；像素闸门 `instanced_vertex_rate_pixel_smoke_test` |
| ~~`BindTextureCube(slot, h)`~~ | A1 | — | ✅ **B5-1 已删**：统一走 `BindTexture(…,TexCube)`（三后端 + skybox 像素 smoke 绿） |
| ~~`PushConstantsMat4(m)`~~ → `PushConstants(stage,offset,data,size)` | A1 | 字节块（`ShaderStage` 路由） | ✅ **B5-3a 已泛化**：三后端字节块（Vulkan 真 push / DX11 push cbuffer b0 / GL push-block UBO `DsePush{VS,FS,CS}`）；skybox/PP/compute 三消费点统一经此 ABI（删 `BindUniformBuffer(0)` 参数路径与 DX11 PerFrame-b0 hack）。详见 §8.2 D3 |
| `Draw(vertex_count, first_vertex)` | A1 | **无** instance 参数 | ✅ 三后端 |
| `BindIndexBuffer(buffer, IndexType)` | B0 | — | ✅ 三后端 |
| `BindTexture(slot, h, TextureDim)` | B0 | 2D/Cube/2DArray | ✅ 三后端 |
| `BindUniformBuffer(slot, h, offset, size)` | B0 | — | ✅ 三后端 |
| `DrawIndexed(index_count, first_index, base_vertex)` | B0 | **无** instance 参数 | ✅ 三后端 |
| `DrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance)` | P0a | 实例化重载（不改 `DrawIndexed`） | ✅ 三后端（mesh 实例化/蒙皮消费） |
| `BindStorageBuffer(slot, handle, offset, size)` | P0b | 图形阶段读 SSBO | ✅ 三后端（GL SSBO / DX11 StructuredBuffer SRV 仅绑 VS / Vulkan storage descriptor） |
| `DrawIndexedIndirect(indirect_buffer, byte_offset)` | B2b-5 | CommandBuffer 级间接 | ✅ 三后端（GL glMultiDrawElementsIndirect / Vulkan vkCmdDrawIndexedIndirect / DX11 DrawIndexedInstancedIndirect） |
| `DispatchComputePass(ComputeDispatch)` | 阶段 2b | 通用 compute 调度（源 SRV→当前 RT 作 UAV/storage） | ✅ compute 后端（DX11 FL11+ / Vulkan）；GL 经 `GetBloomComputeShader()==0` 由 `BloomRenderer` 回退全屏 quad，不调该原语 |

内建资源访问器（`RhiDevice`，供高层渲染器复用引擎着色器/几何）：`GetBuiltinProgram(BuiltinProgram)`（按枚举取 Skybox/Sprite2D/SpriteFxSdf/SpriteFxVfx + **ForwardPbr/ForwardPbrSkinned/ForwardPbrInstanced/ForwardPbrDepth**（B2b-1..5）+ **ForwardShaded/ForwardSkinnedShaded/ForwardInstancedShaded/ForwardMorphShaded**（2c-1 + Final-Feat-2/3/5 高级 shading）程序，取代每效果一个访问器——见 §8.2 D4）+ `GetSkyboxCubeVertexBuffer`；间接缓管理 `CreateIndirectBuffer/UpdateIndirectBuffer/DeleteIndirectBuffer`（B2b-5）。

> **说明（2c + Final-Feat）**：高级 shading 与几何来源能力均落在 `MeshRenderer` 层（`DrawShaded` 系列 + Final-Feat-6 `DrawShadedExternal`/`BuildShadedWorldVertexBuffer` + Final-Feat-7 `DrawSharedTemplateInstanced`/`BuildShadedLocalVertexBuffer`），**未新增任何 CommandBuffer 原语**——全部复用既有 `DrawIndexed/DrawIndexedInstanced/BindStorageBuffer/BindUniformBuffer/BindVertexBuffer/BindIndexBuffer`，故 §3.1 原语表不变。

**设备级即时原语（P1，编辑器架构 §5.A/§5.B）**：以下两个原语挂在 `RhiDevice` 上（**非** `CommandBuffer` 录制集），自包含、**同步执行**（返回后即可 `ReadRenderTargetColorRgba8WithSize` 回读），用于把编辑器视口拾取/多视口呈现从「直绑 OpenGL」中解耦：

| 原语（`RhiDevice`） | 引入 | 签名 | 状态 |
|---|---|---|---|
| `ImmediateDraw(ImmediateDrawDesc)` | P1（§5.A） | 自定义 shader program + 一段交错顶点 + 少量 uniform → 直绘到指定 RT（含 viewport/clear/blend/depth_test、`ImmediateTopology`、按名打包 uniform） | ✅ 三后端：GL 临时 VAO/VBO + `glVertexAttribPointer` + `glGetUniformLocation`；DX11 `GetOrCreatePrimInputLayout`（attr→`TEXCOORD<loc>`）+ cbuffer 反射按名打包；Vulkan 动态 `VkPipeline` 复合键缓存 + 一次性 cmd buffer + push-constant 成员偏移反射。像素闸门 `immediate_draw_pixel_smoke_test`（Fills/ViewportSubregion/ColorIdRoundTrip） |
| `BlitRenderTarget(src_rt, dst_rt)` | P1（§5.B） | 等尺寸把 src 颜色 0 号附件拷到 dst | ✅ 三后端：GL `glBlitFramebuffer`；DX11 `CopyResource`（MSAA 源先 `ResolveSubresource`）；Vulkan `vkCmdBlitImage` + 前后 layout barrier。像素闸门 `BlitRenderTargetPixelSmokeTest` |

> P1 是编辑器去 GL 化的**地基**；其上的 §5.1 视口拾取 / §5.2 多视口 / §5.3 ImGui 呈现层迁移仍待续做（§5.3 须三后端手动截图验证，GL/Vulkan 真机收尾需真实 GPU）。本机仅 D3D11(WARP) 跑真实像素，GL/Vulkan 无驱动优雅 skip。

### 3.2 仍存的逐效果 ABI（边界**之上**待下沉）

| 旧 ABI（`CommandBuffer` 纯虚） | 迁移阶段 | 状态 |
|---|---|---|
| ~~`DrawSkybox`~~ | A1 | ✅ 已删（`d885d0eb`） |
| ~~`DrawSpriteBatch`~~ | B2a | ✅ 已删（`43240e8e`） |
| ~~`DrawMeshBatch(items)`~~ | **B2b → 阶段 4（M1–M4）** | ✅ 已删（阶段 4 M4 收尾提交）；迁至 `MeshRenderer`（M1 蒙皮×实例化 / M2 编辑器视图模式 / M3 GBuffer-MRT 补齐覆盖缺口后，M4 加 `DrawBatch` 分发层路由全部 CPU mesh 队列，保留 shadow-cull 预算 + PreZ 跳过）；删纯虚 + 三后端 forwarder + 三执行器实现 + `gl_draw_executor_mesh.cpp` 整文件（见 §5） |
| ~~`DrawParticles3D(items, view, proj)`~~ | B3 | ✅ 已删（`c8f84edf` + `5e8fbe30`，迁至 `ParticleRenderer` + `BuiltinProgram::Particle3D`） |
| ~~`DrawHairStrands(items, view, proj)`~~ | **阶段 3（B4）** | ✅ 已删（阶段 3 收尾提交）；迁至 `HairRenderer` + `BuiltinProgram::HairStrand`，LINE_STRIP 拓扑经 PSO 贯穿 vertexless 非索引 `Draw`，position/tangent 走 SSBO\@slot0/1（见 §5C） |
| ~~`DrawPostProcess(request)`~~ | **阶段 2b** | ✅ 已删（阶段 2b 收尾提交）；全部 29 效果迁至 `PostProcessRenderer`，bloom mip 链经 `BloomRenderer` 走 `DispatchComputePass` 原语（见 §5B） |

### 3.3 高层渲染器（边界**之上**，跑在原语上）

| 渲染器 | 文件 | 性质 |
|---|---|---|
| `SkyboxRenderer` | `engine/render/skybox_renderer.{h,cpp}` | 生产（A1，DrawSkybox 已删） |
| `SpriteBatchRenderer` | `engine/render/sprite_batch_renderer.{h,cpp}` | 生产（B2a，DrawSpriteBatch 已删） |
| `SpriteRenderer` | `engine/render/sprite_renderer.{h,cpp}` | B0 契约验证脚手架（非生产） |
| `MeshRenderer` | `engine/render/mesh_renderer.{h,cpp}` | 生产（B2b + 阶段 4，✅ `DrawMeshBatch` 已删）：后端无关 forward 能力 B2b-1..5（静态/蒙皮/实例化/depth-only/间接）+ 2c-1..5 与 Final-Feat-1..7（高级 shading 全模式/CSM/聚光/morph、地形 splat+积雪、WBOIT、DDGI/LightProbe、外部常驻 VAO/EBO、共享网格模板去重）+ 阶段 4 M1 蒙皮×实例化 / M2 编辑器视图模式 / M3 GBuffer-MRT；M4 `DrawBatch(cmd, device, items, view, proj)` 据 item 字段 + 全局渲染状态路由全部 CPU mesh 队列，**已取代 `DrawMeshBatch` ABI** |
| `ParticleRenderer` | `engine/render/particle_renderer.{h,cpp}` | 生产（B3，DrawParticles3D 已删，SSBO 实例化） |
| `HairRenderer` | `engine/render/hair_renderer.{h,cpp}` | 生产（阶段 3/B4，`DrawHairStrands` 已删）：组合 HairUniforms UBO\@set0.b0（VS/FS 共享）+ position/tangent SSBO\@set7.b0/b1（vertexless，`gl_VertexIndex` 取数）；PSO 烘焙 LINE_STRIP 拓扑，逐 strand `cmd.Draw(count, first)` |
| `PostProcessRenderer` | `engine/render/post_process_renderer.{h,cpp}` | 生产（阶段 2b，✅ `DrawPostProcess` 已删）：全屏 quad + std140 UBO（set=2,b0）+ 纹理（2D/3D）+ PSO；承接全部 29 个全屏后处理效果 |
| `BloomRenderer` | `engine/render/bloom_renderer.{h,cpp}` | 生产（阶段 2b）：bloom mip 链高层封装，按 `device.GetBloomComputeShader()` 分支——compute 后端（DX11/Vulkan）经 `cmd.DispatchComputePass()`，GL 回退 `PostProcessRenderer` 全屏 quad（down/upsample.frag） |

---

## 4. 契约终态 vs 已实现（差距分析）

[`RHI_PRIMITIVE_CONTRACT.md`](./RHI_PRIMITIVE_CONTRACT.md) §3 描述的是**完整终态**；按「每个原语都要有活体消费者 + 像素覆盖、不留死代码」的原则（§7），尚未落地的原语**与其消费者同批实现**：

| 终态原语（契约 §3） | 现状 | 阻塞/排期 |
|---|---|---|
| `BindStorageBuffer(slot, h, offset, size)`（图形阶段读 SSBO） | ✅ **已实现**（P0b `10f3ff2d`，三后端） | mesh 蒙皮/实例（B2b-2/3）+ hair（阶段 3/B4，position/tangent SSBO\@slot0/1）已消费 |
| 泛化 `PushConstants(stage, offset, data, size)`（取代 `PushConstantsMat4`） | ✅ **B5-3a 已实现**（三后端字节块，删旧 `PushConstantsMat4`） | skybox/PP/compute 三消费点已统一消费 |
| 实例化 `DrawIndexedInstanced`（`instance_count`/`first_instance`） | ✅ **已实现**（P0a `f38d0b13`，新增重载不改现签名） | mesh 实例化已消费（B2b-3）；particles（B3）待消费 |
| 间接绘制 `DrawIndexedIndirect`（CommandBuffer 级） | ✅ **已实现**（B2b-5 `25fb30a6`，三后端） | mesh 间接已消费（`MeshRenderer::DrawIndirect`） |
| slot 化 `BindVertexBuffer(slot, …, rate)`（PerVertex/PerInstance） | ✅ **已实现**（收尾 A，就地改签名 + 全部 22 调用点迁移，三后端） | 像素闸门 `instanced_vertex_rate_pixel_smoke_test`（slot0 per-vertex + slot1 per-instance 实例流，DX11 验证）；particles 实例流可复用 |
| 绑定组 `BindGroup(BindGroupDesc)`（多 UBO/纹理/SSBO 成组） | ✅ **已实现**（收尾 B，默认转发 + 后端延迟组装天然批量） | `SpriteRenderer` 消费 {PerFrame UBO + 纹理}；专项闸门 `bind_group_pixel_smoke_test`（DX11 验证多 UBO 成组）；对象级 descriptor set/argument buffer 批量仍为后续优化 |

> push-block UBO 决策（契约 §8.2）已在 **B2a 为 sprite 实现**：SDF/VFX 参数走单个 `SpriteFx` UBO（binding 0，三后端同布局映射），无需 per-backend push constant 分叉。mesh 迁移可复用该模式。

---

## 5. B2b + 阶段 4：✅ 已完成（MeshRenderer 取代 `DrawMeshBatch`，ABI 已删）

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

**阶段 4 收尾（M1–M4，✅ 已完成）**：用户决策选项 B（性能对齐——先补 MeshRenderer 覆盖缺口再删 ABI）。
- **M1 蒙皮×硬件实例化**（`c86d8cbd`）：`BuiltinProgram::ForwardSkinnedInstancedShaded`（`forward_shaded_skinned_instanced.vert` + `forward_shaded.frag`）三后端 + `MeshRenderer::DrawSkinnedInstancedShaded`（bone-palette 去重密排，实例 SSBO\@slot0 `MeshSkinnedInst{mat4;int bone_offset}` 80B + 骨骼 SSBO\@slot1，不预乘 model）；D3D11 像素闸门 `skinned_instanced_shaded_pixel_smoke_test`。
- **M2 编辑器视图模式**（`b1300c07`）：`PipelineStateDesc.wireframe` 三后端 PSO（DX11 `D3D11_FILL_WIREFRAME` / GL `glPolygonMode` / Vulkan `VK_POLYGON_MODE_LINE`）；MeshRenderer forward 路径读 `GetGlobalRenderState()` 的 wireframe/overdraw/force_unlit；D3D11 像素闸门 `editor_view_mode_pixel_smoke_test`。
- **M3 GBuffer/RSM MRT**（`e47f9db8`）：`BuiltinProgram::GBufferMesh`（`forward_pbr.vert` + `gbuffer.frag`）+ `MeshRenderer::DrawGBuffer`（CPU 预变换世界顶点，MRT 输出 gAlbedo/gNormal/gPosition）；修复 DX11 `BeginRenderPass` 仅绑 attachment0 的缺口（MRT 绑全部 RTV + 逐附件清屏）；D3D11 像素闸门 `gbuffer_mesh_pixel_smoke_test` 逐附件校验。
- **M4 迁调用点 + 删 ABI**（M4 收尾提交）：
  - `MeshRenderer::DrawBatch(cmd, device, items, view, proj)` 分发翻译层——逐 item 据 `skinned`/`instance_transforms`/`morph` + `DrawExecutorGlobalState::current_pass_depth_only`（三后端在 `BeginRenderPass` 据有无彩色附件写入）/`gbuffer_rendering_mode`/编辑器三标志，路由到 M1–M3 的 `DrawShaded`/`DrawSkinnedShaded`/`DrawInstancedShaded`/`DrawSkinnedInstancedShaded`/`DrawGBuffer`；保留 shadow-cull 实例预算（ortho 检测 + per-item 预过滤 `instance_transforms`，渲染实例数与执行器逐位一致）+ PreZ（透视 depth-only）跳过蒙皮实例；depth-only/gbuffer **复用 forward program**（depth pass frag 颜色被丢弃、gbuffer 输出 MRT），**零新增着色器**。
  - 常驻持有者：`FramePipeline::cpu_mesh_renderer_`（跨帧复用内部 VBO/SSBO），经 `RenderPassContext::mesh_renderer` plumb 到各 pass；`render_scene.h` 的 `DrawOpaqueCpu/DrawTransparent` 改签名收 `(cmd, device, MeshRenderer&)`；`MeshRenderSystem` 经 `SetRenderContext(device, renderer)` 注入（`i_builtin_modules`→`BuiltinModulesImpl`→`RenderMeshes` 三层接口同步）。terrain/tree/grass/spine/mesh_render 调用点早已迁 MeshRenderer。
  - 删 `DrawMeshBatch` ABI：`rhi_device.h` 纯虚 + 三后端 `*_command_buffer.h` forwarder 声明 + 三执行器实现（`dx11_draw_executor.cpp` / `vulkan_draw_executor.cpp` / `gl_draw_executor_mesh.cpp` **整文件删除**）+ GL `RealSubmitDrawMeshBatch`；失效测试/mock（`{dx11,gl,vulkan}_rhi_test.cpp` 的 `*DrawMeshBatchSafety` 用例、`rendergraph_integration_test.cpp` MOCK_METHOD、`runtime_render_shell_unit_test.cpp` override）+ 文档（本文 + `CPP_API.md`）。
  - 注：阴影矩阵不再经旧 `cmd.SetGlobalMat4Array` 缓冲 + `DispatchPendingLightArrays`（DrawMeshBatch 入口）路径，改由 `ShadowPass` 直接 `device.SetGlobalLightSpaceMatrix/SetGlobalCascadeSplit` 写全局状态（`builtin_passes.cpp`），MeshRenderer 各路径从 `GetGlobalRenderState()` 读取，与 M1–M3 已迁移系统一致。
  - 验证：三后端编译零错误 + ctest 三套件全绿 + D3D11 GBuffer/SkinnedInstanced/EditorViewMode/DepthOnly 像素闸门绿（GL/Vulkan 无驱动 skip）。

**阶段 5-B5-2 收尾（✅ 已完成）—— 全局绑定收敛 + 清 M4 遗留死代码**：
- **删死缓冲路径**：M4 删 `DrawMeshBatch` ABI 后，上文的 `cmd.SetGlobalMat4Array`/`SetGlobalFloatArray` 缓冲 + `DispatchPendingLightArrays` 派发已**全仓无生产调用方**（仅单测引用已删 ABI）。本里程碑删除 `CommandBuffer::SetGlobalMat4Array`/`SetGlobalFloatArray` 纯虚 + `ForwardingCommandBuffer` 的 `pending_mat4_array_`/`pending_float_array_` 暂存与访问器 + `DispatchPendingLightArrays`；保留标量 `SetGlobalMat4`/`pending_mat4_`（独立路径）。同步删失效单测/mock 对应 ABI 引用（不改用例逻辑）。
- **全局绑定收敛**：`RhiDevice` 的阴影/光源全局状态接口统一为 `(index, value)` 签名并按类别分组（阴影贴图 dir/spot/point、光照空间矩阵 dir/spot、CSM 级联分割 / atlas 区域、探针 SH）；删除冗余的单参 `SetGlobalSpotShadowMap(handle)`/`SetGlobalSpotLightSpaceMatrix(mat)`（index=0 重载，全仓无调用方），语义不变、调用点零改动。
- 验证：三后端编译零错误 + ctest 三套件全绿 + D3D11 像素闸门绿（含 spot/point/CSM 阴影 smoke，覆盖收敛后的绑定路径；GL/Vulkan 无驱动 skip）。

拆解见 [`../plans/B2b_mesh_migration_scoping.md`](../plans/B2b_mesh_migration_scoping.md)。

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

## 5C. 阶段 3（B4）：DrawHairStrands → HairRenderer（✅ 已完成，ABI 已删）

`DrawHairStrands(items, view, proj)` 是 TressFX 毛发线带的总入口（三后端各自创建毛发着色器/管线 + 绑定 position/tangent SSBO + 逐 strand 多段绘制），又一处 O(效果 × 后端) 成本。其特殊性：① 线带拓扑（LINE_STRIP，通用 `Draw` 原先硬编码三角形）；② vertexless（无顶点属性，`gl_VertexIndex`/`SV_VertexID` 取 SSBO）；③ 逐 strand 多段绘制（每根头发是 line-strip 中一段连续顶点）。

**基础设施补齐**：
1. **`PrimitiveTopology` 贯穿 PSO**——新增 `PrimitiveTopology` 枚举 + `PipelineStateDesc.topology` 字段；三后端把枚举映射到原生 API（DX11 `D3D11_PRIMITIVE_TOPOLOGY_*`、GL `glDraw* mode`、Vulkan `VkPrimitiveTopology` 烘焙进管线）。`SetPipelineState` 把拓扑从 PSO 推给绘制执行器（DX11/GL `PrimSetTopology`；Vulkan 在管线创建期消费 `desc.topology`），通用 `Draw` 据此光栅化（不再硬编码三角形）。
2. **vertexless 通用绘制**——非索引 `PrimDraw` 支持缺省 VBO（`prim_vbo==0` 时不绑顶点缓冲；GL 惰性建非零 VAO），并补齐 UBO/纹理/SSBO 绑定（此前仅索引 `Draw` 路径绑定）。
3. **`BuiltinProgram::HairStrand`**——三后端 shader manager 各自 `InitHairStrandShader()`（DX11 DXBC / GL 编译 / Vulkan 预编译 SPIR-V，均源自 `hair.vert`+`hair.frag` 的 `.gen.h`），`GetBuiltinProgram(HairStrand)` 惰性创建并返回 handle。

**着色器契约**：`hair.vert`/`hair.frag` 共享组合 `HairUniforms` UBO\@set0.b0（320 字节 std140：model/view/projection/camera_pos/light_dir/light_color/root_color/tip_color/spec_color/params0/params1，全字段逐字一致以对齐偏移）+ position/tangent SSBO\@set7.b0/b1（经 `@SSBO_LOW_REGISTERS` 落低位寄存器）。片元走 Kajiya-Kay（切线键控高光 + root→tip 颜色按 `tangent.w` 厚度插值）。

**高层 `HairRenderer`**（仿 `ParticleRenderer`）：`EnsureResources` 惰性建半透明混合 PSO（测深度不写、不剔除、`topology=LineStrip`）+ per-item UBO + 取 `BuiltinProgram::HairStrand`；`Draw` 逐 item 上传 `HairUniforms` UBO、绑 position/tangent SSBO\@slot0/1，再逐 strand 调 `cmd.Draw(strand_counts[i], strand_firsts[i])`。`hair_system.cpp` 调用点由 `cmd.DrawHairStrands(...)` 迁为 `hair_renderer_.Draw(cmd, *rhi_, items, view, proj)`。

**删 `DrawHairStrands` ABI**：全仓 `grep` 确认无生产调用点后，删 CommandBuffer 纯虚 + 三后端 command_buffer forwarder + executor 实现 + device `RealSubmitDrawHairStrands` 转发。修失效测试/mock：`rendergraph_integration_test`（去 `DrawHairStrands` MOCK_METHOD）、`runtime_render_shell_unit_test`（去 override stub）。

**验证**：三后端编译零错误；ctest 三套件全绿；新增 D3D11 像素闸门 `hair_pixel_smoke_test`——经 `HairRenderer` + `BuiltinProgram::HairStrand` 画一条水平 LINE_STRIP（vertexless + position/tangent SSBO），断言中央横带有线、带外近黑、四角清屏黑（验证线带拓扑贯穿 + vertexless SSBO 绘制 + 组合 UBO 跨 VS/FS）。GL/Vulkan 无驱动 skip（既有状况）。

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

- **B2b + 阶段 4**：✅ 已完成。抽 `MeshRenderer`——forward-PBR（B2b-1..5）+ 高级 shading 全模式（2c-1..5 + Final-Feat-1..7）+ 阶段 4 M1 蒙皮×实例化 / M2 编辑器视图模式 / M3 GBuffer-MRT；M4 加 `DrawBatch` 分发层路由全部 CPU mesh 队列、迁 6 调用点（常驻 `FramePipeline::cpu_mesh_renderer_`）、**已删 `DrawMeshBatch` ABI**（三后端 executor + forwarder + 纯虚 + `gl_draw_executor_mesh.cpp` 整文件 + GL `RealSubmitDrawMeshBatch`）。详见 §5。
- **B3**：✅ 已删 `DrawParticles3D`——迁至 `ParticleRenderer` + `BuiltinProgram::Particle3D`（SSBO 实例化）。
- **阶段 2b**：✅ 已完成。`PostProcessRenderer` 承接全部 **29** 效果（含末三手写 HLSL 簇）；compute mip 链选 Option A——新增 CommandBuffer 级 `DispatchComputePass` 原语 + `BloomRenderer`（compute/quad 分支）；**已删 `DrawPostProcess` ABI**（三后端 executor + forwarder + 纯虚）。详见 §5B。
- **阶段 3（B4）**：✅ 已完成。迁 `DrawHairStrands`（SSBO + 多段绘制）至 `HairRenderer` + `BuiltinProgram::HairStrand`；新增 `PrimitiveTopology` 贯穿 PSO（LINE_STRIP）+ vertexless 通用 `Draw`；**已删 `DrawHairStrands` ABI**（三后端 executor + forwarder + 纯虚）。详见 §5C。
- **B5**：全局绑定收敛（shadow map / global uniforms / `BindShaderProgram`+PSO 聚合，偿还契约 §8.1 债务）。
  - **B5-1**：✅ 已完成。去重过渡期重复原语之 cube：删 `BindTextureCube`，统一 `BindTexture(…,TexCube)`（三后端 + skybox 像素 smoke 绿）。`PushConstantsMat4` 泛化为字节块 `PushConstants` 挪 B5-3（GL push_constant 降级为具名 uniform，泛化需配 GL→UBO 降级，随 program+PSO 聚合一并改）。详见 §5 / §8.2 D3。
  - **B5-2**：✅ 已完成。删 M4 遗留死缓冲路径（`SetGlobalMat4Array`/`SetGlobalFloatArray` + `DispatchPendingLightArrays`，全仓无生产调用方）；`RhiDevice` 阴影/光源全局状态接口收敛为统一 `(index, value)` 签名并按类别分组，删冗余单参重载（语义不变、调用点零改动）。详见 §5。
  - **B5-3a**：✅ 已完成。`PushConstantsMat4` 泛化为字节块 `PushConstants(ShaderStage, offset, data, size)`（三后端：Vulkan 真 push / DX11 push cbuffer b0 / GL push-block UBO）；编译器把 GL/ESSL 的 `layout(push_constant)` 降级为按阶段区分的 std140 块 `DsePush{VS,FS,CS}`（规避同名块跨阶段一致约束）；skybox/PP/compute 三消费点统一经 `PushConstants` ABI，删 `BindUniformBuffer(0)` 参数路径与 DX11 skybox PerFrame-b0 hack。验证：三后端零错 + ctest 全绿 + DX11 像素闸门（skybox + PP tonemapping/bloom 参数）。GL/Vulkan 无驱动靠编译+std140/反射镜像。详见 §8.2 D3。
  - **B5-3b**：✅ 已完成（高风险，§8 拍板项）。`BindShaderProgram`+`SetPipelineState` 聚合为单一图形管线对象：新增 `GraphicsPipelineDesc{pso_state, program}` + 设备级惰性去重缓存 `RhiDevice::GetGraphicsPipeline(pso,program)→handle`（后端无关，三端共用）+ `CommandBuffer::BindPipeline(handle)`；三后端按句柄解出 (pso,program) 应用（DX11/GL 应用 PSO 状态+拓扑后 `program!=0` 绑 program，Vulkan 设活动 PSO 并在绘制时把 program 惰性烘进 `VkPipeline`）。迁移全部生产调用点（渲染器 mesh/sprite/sprite_batch/hair/particle/skybox/PP 成对 `SetPipelineState+BindShaderProgram` → 单次 `BindPipeline`；Pass 层 builtin_passes/atmosphere/probe + `FramePipeline.pipeline_states.*` → `GetGraphicsPipeline(pso, 0)` PSO-only 管线，保留 GPU-driven 自绑/渲染器覆盖语义），**一次性删** `BindShaderProgram`+`SetPipelineState` 两分离原语（ABI + 三后端 + mock/测试）。验证：三后端零错 + ctest 全绿 + DX11 全量像素闸门（skybox/sprite_batch SDF·VFX/postprocess/instanced/depth-only/indirect 等 D3D11 变体全 OK）。GL/Vulkan 无驱动靠编译+镜像。详见 §8.1 D2。

---

## 8. 方案评估、技术债与优化空间（坦率版）

### 8.1 方案是否合理 / 是否最佳
整体方向**合理且符合业界做法**（slot-based 绑定 + 延迟组装 + 高层渲染器，类似 bgfx/sokol 的中间层定位）。三个工程决策尤其稳健：
- **每效果独立闭环 + 即删 ABI**：始终可发布、不留混合态、回归面小。
- **像素闸门**（`rhi_pixel_harness`）：直击「gtest 全绿 ≠ 像素正确」这一真实历史 bug 类别。
- **契约以最难消费者（mesh/hair）倒推**：避免为 skybox 这种简单效果定出不够用的接口。

但**不是唯一/绝对最优**。一个更激进的替代是直接上 **bind group / argument buffer**（WebGPU/Metal 风格：把多资源打包成一次绑定 + program 烘进 PSO），可一次性消除下面 8.2 的多条债务，但**改动面与风险远大**。v1 选择「slot-based + 文档预留 bind group 升级路径」是务实的中间路线；**收尾阶段已落地「有界 bind group」**（`BindGroup(BindGroupDesc)`，默认转发到逐 slot `Bind*` + 后端延迟组装天然批量；见契约 §2.3），对象级 descriptor set/argument buffer 预创建缓存仍列为后续优化。结论：合理，债务都已显式登记、有偿还阶段。

### 8.2 已知技术债（均已登记，非隐藏）
| # | 债务 | 影响 | 偿还 |
|---|---|---|---|
| D1 | ~~**通用原语是 default no-op 虚函数**（未实现的后端静默空转，非编译失败）~~ → **已还**：12 个通用绘制原语（`BindPipeline`/`BindVertexBuffer`/`PushConstants`/`Draw`/`BindIndexBuffer`/`BindTexture`/`BindUniformBuffer`/`BindStorageBuffer`/`DrawIndexed`/`DispatchComputePass`/`DrawIndexedInstanced`/`DrawIndexedIndirect`）`CommandBuffer` 上由 no-op `{}` 默认改纯虚 `= 0`，强制所有实现显式声明；GL 加显式 no-op `DispatchComputePass`（无 compute，消费者经 `GetBloomComputeShader()==0` 回退 quad），测试 Mock 补齐 `MOCK_METHOD`/override | — | ✅ **已还**：真机 RTX 3070 unit 2272 / integration 544 / smoke 288 全绿 |
| D2 | ~~**`BindShaderProgram` 与 PSO 分离**（契约 §8.1）~~ → **已还**：B5-3b 聚合为单一图形管线对象 `GraphicsPipelineDesc{pso,program}` + `BindPipeline(handle)`（设备级 `GetGraphicsPipeline(pso,program)` 惰性去重缓存，后端无关），删 `BindShaderProgram`+`SetPipelineState` 两分离原语；三后端各按句柄解出 (pso,program) 应用（Vulkan 惰性烘进 `VkPipeline`、DX11/GL 分别应用 PSO 状态+拓扑后按需绑 program）。Pass 层 program==0 仅设状态，保留 GPU-driven 自绑/渲染器覆盖语义 | — | ✅ **已还**（B5-3b）：为 Metal/DX12 铺路 |
| D3 | ~~**过渡期重复原语**：`BindTextureCube` vs `BindTexture(…,TexCube)`；`PushConstantsMat4` 未泛化~~ → **已还**：cube 重复 B5-1 删；`PushConstantsMat4` B5-3a 泛化为字节块 `PushConstants(stage,off,data,size)`（三后端 + GL push_constant→`DsePush{VS,FS,CS}` UBO 降级），skybox/PP/compute 统一路由 | — | ✅ **已还**（cube B5-1、PushConstants B5-3a） |
| D4 | ~~`RhiDevice` 内建资源访问器随效果线性增长~~ → **已还**：归并为单个 `GetBuiltinProgram(BuiltinProgram)`，新增内建程序只加枚举值，不再加虚函数 | — | ✅ 已还（B2b 前置） |
| D5 | ~~**每系统各持一个 `SpriteBatchRenderer`**（3 套动态 VBO/IBO/UBO）→ 共享 frame-ring 分配器~~ | **复评后降级**：3 系统用不同相机/不同 pass（sprite=world、UI=ortho、particle），本无跨系统合批可言；显存节省也微小。原「优化」收益≈0 | 不单独做；真正问题见 D9 |
| D9 | ~~**sprite 动态缓冲单缓冲，但引擎 2 帧在飞**（`MAX_FRAMES_IN_FLIGHT=2`）：`SpriteBatchRenderer` 每帧 `UpdateGpuBuffer` 覆写同一 `vbo_/ubo_/fx_ubos_`，帧 N+1 可能在帧 N 仍被 GPU 读取时覆写~~ → **已还**：抽后端无关 helper `PerInFlightBuffer`（`engine/render/rhi/per_in_flight_buffer.{h,cpp}`），按 `RhiDevice::FramesInFlight()` 个槽位 N 缓冲、每帧仅(重)建/写 `CurrentFrameSlot()` 当前槽位（其 fence 已在 `AcquireNextImage` 等待，安全），其余在飞槽位不触碰；新增 `RhiDevice::FramesInFlight()/CurrentFrameSlot()`（默认 1/0，Vulkan 覆写为 `MAX_FRAMES_IN_FLIGHT`/`context.current_frame()`）。`SpriteBatchRenderer` 的 `vbo_/ubo_/fx_ubos_` 改用之（`ibo_` 静态非每帧写故仍单缓冲）。GL/DX11（N=1）退化为单缓冲，行为不变 | — | ✅ **已还**：真机 RTX 3070 unit 2272 / integration 544 / smoke 288 全绿 |
| D6 | ~~**`CommandBuffer::GetView/GetProjectionMatrix()`** 把相机状态缓存在命令缓冲上~~ → **已还**：引入 `engine/render/frame_context.h`（`FrameContext{view, projection}`，均含投影修正），相机由各 Pass 构造后显式传给高层消费者（render_scene / 模块渲染系统 / 探针捕获），从 `CommandBuffer` 移除 `SetCamera/GetView/GetProjectionMatrix` | — | ✅ **已还**（D6，`92531b46`）：相机不再耦合命令记录器 |
| D7 | **像素闸门为 VM 软渲（llvmpipe/WARP/lavapipe）** | RMSE 不复现真机；只验解析真值，真机专属 bug 可能漏 | 已知并接受；条件允许时补一次真机基线 |
| D8 | ~~**契约把 SSBO 排 B4，但 B2b(mesh) 先需 SSBO**~~ → **已解**：P0b 把 `BindStorageBuffer` 提前到 B2b 之前落地（`10f3ff2d`），mesh 蒙皮/实例已消费 | — | ✅ 已解 |
| D10 | ~~**`MeshRenderer` 与 `DrawMeshBatch` ABI 并存**~~ → **已解**：阶段 4（M1–M4）补齐 MeshRenderer 覆盖缺口（蒙皮×实例化 / 编辑器视图模式 / GBuffer-MRT）后，M4 加 `DrawBatch` 分发层、迁 6 调用点、**删 `DrawMeshBatch` ABI**（含 `gl_draw_executor_mesh.cpp` 整文件） | — | ✅ 已解 |
| D11 | **`CommandBuffer` 缺多线程录制**：当前「立即转发录制」把 CPU 侧录制开销内联在调用线程（GL/DX11 即时下发、Vulkan 录进 `VkCommandBuffer` submit 时执行）。万级 DrawCall 时 CPU 录制可能成瓶颈 | **远期潜力，不阻塞发版**：当前无可测瓶颈，做了反而毁掉「全绿招牌态」。注意①真正障碍是 `draw_executor_` 为**设备单例**（录制态共享），上多线程须先把它**去单例化 / per-command-buffer 化**；②**GL 物理不可达**（单 context、无 secondary cmd buffer），多线程录制是 Vulkan/(未来)DX12 专属卖点，不强求三后端对齐；③万级 DrawCall 的更高杠杆解是已落地的 **GPU-driven（`DrawIndexedIndirect`）**，优先压它。原语接口已为此预留（签名不变） | 远期；出现实测瓶颈时先压 GPU-driven，再仅在 Vulkan/DX12 上做 |
| D12 | **Mesa/llvmpipe 等 CI 软渲回归**：CI/VM 上 GL→llvmpipe、Vulkan→lavapipe、DX11→WARP 的软件光栅器与真机驱动存在行为/精度差异（见 D7） | **不影响真机 GPU 硬件跑**：软渲像素「解析真值」正确，但 RMSE 不复现真机；软渲特有的实现差异可能让某些 case 在 CI 抖动而真机正常（反之亦然）。本机仅 D3D11(WARP) 可跑真实像素，GL/Vulkan 无驱动恒 skip | 已知并接受；条件允许时以真机基线为准绳校准，CI 软渲仅作「能跑通+解析真值」级回归 |

### 8.3 优化空间（非阻塞，按收益排序）
1. ~~**可复用「每在飞帧缓冲」helper**（解 D9，取代原 D5 设想）~~ → **已落地**：抽后端无关 `PerInFlightBuffer`（按 `RhiDevice::FramesInFlight()`/`CurrentFrameSlot()` 轮转槽位，仅(重)建/写当前在飞槽位），已回填 `SpriteBatchRenderer` 的 `vbo_/ubo_/fx_ubos_`。MeshRenderer 后续可复用同一 helper。
2. ~~**shader 注册表**（解 D4）~~ → **已落地**：`RhiDevice::GetBuiltinProgram(BuiltinProgram)` 取代逐 program 访问器。
3. ~~**bind group / argument buffer**（契约 §2.3）~~ → **已落地（有界）**：`BindGroup(BindGroupDesc)` 把多 UBO/纹理/SSBO 打成一次原子绑定（默认转发到逐 slot `Bind*`，后端延迟组装天然汇成一次提交），`SpriteRenderer` 已消费，闸门 `bind_group_pixel_smoke_test`；**对象级** descriptor set/DX12 descriptor table/Metal argument buffer 预创建缓存仍为后续优化点（为 Metal/DX12 铺路）。
4. **实例化/间接绘制原语**随 B2b/B3 落地（新增重载，不改现签名）。

### 8.4 结论
- **合理**：是；增量、可回归、债务显式。
- **有优化空间**：有（8.3）。D4（shader 注册表）已在 B2b 前还清；原 D5（共享分配器）经复评收益≈0 已降级；D9（sprite 单缓冲 vs 2 帧在飞）已抽后端无关 `PerInFlightBuffer` helper 还清并回填 sprite。
- **最佳？**：是「低风险务实最优」，非「理论最优」（后者是 bind group + program/PSO 聚合的一次性大改）。
- **技术债**：未隐藏，8.2 全部登记并各有偿还阶段；唯一需即时决策的是 D8（B2b 的 SSBO 顺序）。
