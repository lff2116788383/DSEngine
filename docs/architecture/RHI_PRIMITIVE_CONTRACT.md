# RHI 通用绘制原语契约 v1（B0）

> 目标：把 `CommandBuffer` 从「每效果一个虚函数」收敛为一组**后端无关的通用绘制原语**（pipeline / bindings / draw / dispatch），效果逻辑上移到高层渲染器。
> 本文是契约设计 + Metal 纸面映射校验 + 三后端实现要点。**本步（B0）只新增原语、不删旧 ABI、不迁移任何效果。**

---

## 1. 背景与动机

当前 RHI 成本是 **O(效果数 × 后端数)**：`DrawMeshBatch / DrawPostProcess / DrawParticles3D / DrawHairStrands`（+ 已删的 `DrawSkybox / DrawSpriteBatch`）每个都在 GL/Vulkan/DX11 各实现一遍。

- **加后端**（确定要加 Metal）= 把每个效果重写一遍。
- **行为漂移**：同一效果在不同后端的实现会跑偏（如 `RHI_UNIFICATION_TASKS.md:107` 记录的 skybox view 平移处理 GL 与其它后端不一致）。

收敛后成本变 **O(效果数 + 后端数)**：效果只写一次（高层渲染器跑在原语上），后端只实现一组**有限**原语。skybox spike 已实证：3 份后端实现 → 1 个 `SkyboxRenderer`，并消除了该类漂移。

---

## 2. 设计原则

1. **粒度 = pipeline + 资源绑定 + draw/dispatch**，不是「GL 式逐个 glBind」。现代 API（Vulkan/DX12/Metal）要的是更少、更批量的状态变更。
2. **slot-based 绑定 + 延迟组装（v1）**：高层渲染器按 slot 绑 VB/IB/纹理/UBO/SSBO/push-constant，后端把这些累积到执行器成员，在 `Draw*` 时一次性组装 pipeline + descriptor 并提交。这沿用 skybox spike 已验证的模式，能映射到全部后端。
3. **绑定组 / argument buffer（未来）**：把多个资源打包成一次绑定（Vulkan descriptor set、DX12 descriptor table、Metal argument buffer）是后续性能优化方向，v1 不做，但契约的 slot 编号需为之预留。
4. **不重造已有能力**：compute dispatch / SSBO compute / 间接绘制已是设备级抽象（`IRhiCompute` / `IRhiStorageBuffer` / `IRhiGpuDriven`），契约**引用**它们，graphics 原语只覆盖图形绘制录制路径。
5. **可被「最难消费者」表达**：契约以 `MeshDrawItem`（索引 + 实例化 + 材质 UBO + 多贴图 + 骨骼 SSBO）和 `HairDrawItem`（SSBO + multi-draw）为设计基准，而非 skybox。

---

## 3. 原语契约 v1（CommandBuffer 新增/扩展接口）

> 约定：`unsigned int` 句柄沿用现有资源句柄体系；新增枚举入 `rhi_types.h`。
> 标注：**[既有]** 已存在，**[扩展]** 改签名，**[新增]** 全新。

```cpp
// ---- 新增枚举（rhi_types.h） ----
enum class VertexInputRate : uint8_t { PerVertex = 0, PerInstance = 1 };
enum class IndexType       : uint8_t { UInt16 = 0, UInt32 = 1 };
enum class TextureDim      : uint8_t { Tex2D = 0, TexCube = 1, Tex2DArray = 2 };
enum class ShaderStage     : uint32_t { Vertex = 1, Fragment = 2, Compute = 4,
                                        AllGraphics = Vertex | Fragment };
// VertexAttr 增加 format（v1 仅 Float 路径，预留整型）
enum class VertexAttrFormat : uint8_t { Float = 0, /* 预留: Int, UInt, ... */ };

// ---- 状态 ----
void SetPipelineState(unsigned int pso_handle);                  // [既有]  blend/depth/cull/raster
void BindShaderProgram(unsigned int program_handle);             // [既有]

// ---- 顶点 / 索引输入 ----
void BindVertexBuffer(uint32_t slot, unsigned int buffer_handle, // [扩展] +slot +rate
                      uint32_t stride,
                      const std::vector<VertexAttr>& attrs,
                      VertexInputRate rate = VertexInputRate::PerVertex);
void BindIndexBuffer(unsigned int buffer_handle, IndexType type);// [新增]

// ---- 资源绑定（slot-based） ----
void BindTexture(uint32_t slot, unsigned int texture_handle,     // [新增] 归并 BindTextureCube
                 TextureDim dim);
void BindUniformBuffer(uint32_t slot, unsigned int buffer_handle,// [新增] 常量/uniform buffer
                       uint32_t offset = 0, uint32_t size = 0);
void BindStorageBuffer(uint32_t slot, unsigned int buffer_handle);// [新增] 图形阶段读 SSBO（hair）
void PushConstants(ShaderStage stage, uint32_t offset,           // [新增] 取代 PushConstantsMat4
                   const void* data, uint32_t size);

// ---- 绘制 ----
void Draw(uint32_t vertex_count, uint32_t instance_count = 1,    // [扩展] +instance_count +first_instance
          uint32_t first_vertex = 0, uint32_t first_instance = 0);
void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1,// [新增]
                 uint32_t first_index = 0, int32_t base_vertex = 0,
                 uint32_t first_instance = 0);

// ---- 间接绘制 / compute（不在本契约重定义，复用既有） ----
// 间接绘制:  IRhiGpuDriven::*Indirect*
// compute:   IRhiCompute::DispatchCompute / BeginComputePass / ComputeMemoryBarrier
```

### 旧原语的归并关系（过渡期保留，迁移完再删）
| 旧（skybox spike） | 新 | 处理 |
|---|---|---|
| `BindVertexBuffer(buffer, stride, attrs)` | `BindVertexBuffer(slot=0, …, PerVertex)` | 旧签名转调新签名 |
| `BindTextureCube(slot, h)` | `BindTexture(slot, h, TexCube)` | 旧转调新 |
| `PushConstantsMat4(m)` | `PushConstants(Vertex, 0, &m, 64)` | 旧转调新 |
| `Draw(vc, first)` | `Draw(vc, 1, first, 0)` | 旧转调新 |

> `SkyboxRenderer` 暂不动（仍用旧原语，由旧转调新保证行为不变）；待 B1 给它补像素测试后再切到新原语。

---

## 4. 效果消费者 → 所需原语映射

| 效果 | pipeline | VB | IB | 纹理 | UBO | SSBO | push const | draw | 备注 |
|---|---|---|---|---|---|---|---|---|---|
| **Skybox**（已迁） | ✓ | 1×vec3 | — | 1×Cube | — | — | mat4(VP) | Draw(36) | 已用旧原语跑通三后端 |
| **Sprite2D** | ✓ | batch VB | 可选 | 1×2D | PerFrame/Scene/Material | — | — | Draw/DrawIndexed | 最适合首迁（验证 UBO + 2D 纹理） |
| **Mesh PBR** | ✓ | batch VB(+instance VB) | ✓ | 5+×2D(albedo/normal/MR/emissive/AO)+splat+shadow | PerFrame/Scene/Material | 骨骼 palette | model/bone_offset | DrawIndexed(+Instanced) | 最难；验证 index+instancing+多 UBO/纹理 |
| **Particles3D** | ✓ | quad VB + instance VB | — | 1×2D | PerFrame | — | — | Draw(Instanced) | 验证 PerInstance 顶点流 |
| **Hair** | ✓ | — | — | — | 参数 | position/tangent SSBO | world+材质 | multi-draw(per-strand) | 验证图形阶段 SSBO + 多段绘制 |

**结论**：上表的并集 = 第 3 节契约。新增项里 `DrawIndexed`、实例化（PerInstance 顶点流 + `instance_count`）、`BindUniformBuffer`、`BindStorageBuffer`、通用 `PushConstants`、类型化 `BindTexture` 都是被真实消费者要求的，没有为未来臆想的原语。

> **multi-draw（hair per-strand）**：v1 不引入独立 multi-draw 原语；hair 迁移时先用「循环 `DrawIndexed`/`Draw`」表达（N 段 = N 次 draw call），后续若成为热点再引入 `MultiDraw`/indirect。先正确、再优化。

---

## 5. Metal 纸面映射校验

验证契约能干净映射到 Metal（未来后端），且粒度对得上 `MTLRenderCommandEncoder`。

| 本契约原语 | Metal（MTLRenderCommandEncoder / MTLComputeCommandEncoder） | 映射质量 |
|---|---|---|
| `SetPipelineState(pso)` | `setRenderPipelineState:`（含 vertex/fragment function + color/blend + vertex descriptor）+ `setDepthStencilState:` + `setCullMode:` + `setFrontFacingWinding:` + `setTriangleFillMode:` | 干净（PSO 句柄聚合二者） |
| `BindShaderProgram(prog)` | 着色器函数在 Metal 中被烘进 `MTLRenderPipelineState` | ⚠ Metal 下 shader 属于 PSO，无独立绑定——见 §8 备注（program+PSO 应聚合） |
| `BindVertexBuffer(slot, …, rate)` | `setVertexBuffer:offset:atIndex:`；`rate` → pipeline 的 `MTLVertexBufferLayoutDescriptor.stepFunction`(perVertex/perInstance) | 干净 |
| `BindIndexBuffer(buf, type)` | index buffer + `MTLIndexType` 作为 `drawIndexedPrimitives` 入参 | 干净（Metal 把 IB 当 draw 入参，RHI 缓存到 draw 时传入） |
| `BindTexture(slot, tex, dim)` | `setFragmentTexture:atIndex:` / `setVertexTexture:atIndex:`（dim 由 MTLTexture 自带） | 干净 |
| `BindUniformBuffer(slot, …)` | `setVertexBuffer:offset:atIndex:` / `setFragmentBuffer:…`（Metal 无独立 UBO 概念，常量缓冲就是 buffer） | 干净 |
| `BindStorageBuffer(slot, buf)` | 同上 `setVertex/FragmentBuffer:`（SSBO 在 Metal 就是 device 指针 buffer） | 干净 |
| `PushConstants(stage, off, data, size)` | `setVertexBytes:length:atIndex:` / `setFragmentBytes:…`（≤4KB 内联，正是 push constant 语义） | **完美对应** |
| `Draw(vc, ic, fv, fi)` | `drawPrimitives:vertexStart:vertexCount:instanceCount:baseInstance:` | 干净 |
| `DrawIndexed(ic, inst, fi, bv, finst)` | `drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:instanceCount:baseVertex:baseInstance:` | 干净 |
| compute `Dispatch`（既有 IRhiCompute） | `MTLComputeCommandEncoder` `setComputePipelineState:` + `dispatchThreadgroups:threadsPerThreadgroup:` | 干净 |
| 绑定组（未来） | `setVertexBuffer:` 指向 **argument buffer** | 干净（未来优化方向天然支持） |

**校验结论**：契约整体干净映射到 Metal，无需为 Metal 反推新概念。唯一需注意的是 §8 的 `BindShaderProgram` 与 PSO 的聚合问题——这对 DX12/Vulkan 同样存在，应在契约层面收敛。

---

## 6. 三后端实现要点与限制

### OpenGL（立即模式）
- VB：`glBindVertexArray` + 每 slot 一个 `glBindBuffer`/`glVertexAttribPointer`；`PerInstance` → `glVertexAttribDivisor(loc,1)`。
- IB：绑到当前 VAO 的 `GL_ELEMENT_ARRAY_BUFFER`。
- 纹理：`glActiveTexture(GL_TEXTURE0+slot)` + `glBindTexture(target_by_dim)`；sampler uniform 绑定见 skybox 修复（着色器初始化时设 binding）。
- UBO/SSBO：`glBindBufferBase(GL_UNIFORM_BUFFER/GL_SHADER_STORAGE_BUFFER, slot, h)`（SSBO 需 GL4.3+，`supports_ssbo_` 已探测）。
- push const：lower 为命名 uniform（现状 `u_vp`）→ 推广为按 `(offset,size)` 写入一个约定的 uniform block 或具名 uniform；v1 先支持 mat4/通用字节块映射到一个 std140 「push block」UBO。
- draw：`glDrawArraysInstancedBaseInstance` / `glDrawElementsInstancedBaseVertexBaseInstance`（桌面 GL4.2+；WebGL2 无 baseInstance，需回退）。

### Vulkan（延迟组装，最难）
- 绑定累积到执行器成员（已有 skybox 路径雏形 `AllocateAndUpdateSkyboxDescriptorSets`）。
- **难点**：通用 descriptor set 需要与着色器的 descriptor set layout 匹配。依赖 `VulkanShaderProgram` 在创建时由反射/声明得到的 `descriptor_set_layout` + `pipeline_layout`（B0 实现前需确认其结构）。`Draw*` 时按已绑定的 UBO/SSBO/texture slot 分配并更新 descriptor set。
- push const：`vkCmdPushConstants`（按 `pipeline_layout` 的 push constant range；`stage` → `VkShaderStageFlags`）。
- IB：`vkCmdBindIndexBuffer`；draw：`vkCmdDrawIndexed`。
- pipeline：按 PSO + 顶点布局 + render pass 缓存 `GetOrCreateVkPipeline`（已有）。

### DX11（延迟组装，较易，无 descriptor set）
- 纹理：`PSSetShaderResources(slot, SRV)` + `PSSetSamplers`。
- UBO：`VSSetConstantBuffers(slot)` / `PSSetConstantBuffers(slot)`。
- SSBO：`PSSetShaderResources`（StructuredBuffer SRV，只读）/ UAV（读写）。
- push const：DX11 无原生 push constant → 映射到一个小的 `D3D11_USAGE_DYNAMIC` 常量缓冲（`Map`/`Unmap` 写入），绑到约定 slot。
- IB：`IASetIndexBuffer`；draw：`DrawIndexedInstanced`/`DrawInstanced`。
- **限制**：DX11 `DrawInstanced` 的 `StartInstanceLocation` 影响顶点取数但 `SV_InstanceID` 仍从 0 起——`first_instance` 语义与 GL/Vulkan/Metal 不完全一致，实例数据偏移需用 instance VB 偏移表达（在文档标注，迁移时注意）。

---

## 7. 本步（B0）实现范围与验证策略（已定：策略 B）

**核心原则：每个落地的原语都必须有「活体消费者 + 像素覆盖」，不留死代码。** 这直接回应「纯加表面积易藏隐蔽 bug」的风险（skybox 黑屏回归正是 gtest 没覆盖像素路径导致的）。因此原语**按消费者就绪程度分批落地**，而非一次性把整套契约都实现成无人调用的桩。

### B0 落地的原语（每个都有 B0 内的像素验证）
| 原语 | B0 活体验证 |
|---|---|
| `BindIndexBuffer` + `DrawIndexed` | 新 `SpriteRenderer` 画索引化 quad |
| `BindTexture(slot, h, Tex2D)` | `SpriteRenderer` 采样 2D 纹理 |
| `BindUniformBuffer(slot, h)` | `SpriteRenderer` 绑 PerFrame UBO(vp) |
| `PushConstants(stage,off,data,size)`（取代 `PushConstantsMat4`） | `SkyboxRenderer` 改用它推 VP，现有 skybox 像素测试覆盖 |
| `BindTexture(slot, h, TexCube)`（归并 `BindTextureCube`） | `SkyboxRenderer` 改用它绑 cubemap，现有 skybox 像素测试覆盖 |

验证载体：`SpriteRenderer` 复用既有 **sprite2d** 着色器（三后端已编译；顶点 pos\@0/color\@1/uv\@2、PerFrame UBO\@set0.b0、`u_texture` sampler2D\@set2.b1），画一个带纹理 quad；新增一条跨后端离屏像素 smoke gtest（sprite + skybox 各一张）。

### 延后到「真实消费者」阶段才实现的原语（避免死代码）
| 原语 | 落地阶段 | 原因 |
|---|---|---|
| 实例化（`BindVertexBuffer` +slot/+PerInstance、`DrawInstanced`/`DrawIndexedInstanced`） | B3（particles） | 无消费者前不实现；且 `Draw`/`BindVertexBuffer` 现有签名**不改**（现有 `Draw(36u,0u)` 调用会把 `0` 误读为 instance_count），改为新增 `Draw*Instanced` 重载 |
| `BindStorageBuffer`（图形阶段读 SSBO） | B4（hair） | 仅 hair 消费 |
| 多 UBO 槽（PerScene/PerMaterial） | B2（mesh/sprite 生产路径） | sprite2d 仅需 PerFrame |

> 说明：契约（§3）描述的是**完整终态**；§7 是**实现节奏**。终态接口可在 CommandBuffer 上以默认 no-op 虚函数先声明（文档化 surface），但其**后端实现**与消费者同批落地，保证落地即被像素测试覆盖。
>
> **不改签名的安全约束**：`Draw(vertex_count, first_vertex=0)` 与 `BindVertexBuffer(buffer, stride, attrs)` 现有签名保持不变；实例化/多槽通过**新增重载/新方法**表达，杜绝静默语义漂移。

---

## 8. 待决问题（实现前需拍板）

1. **`BindShaderProgram` 与 PSO 的关系**：Metal/Vulkan/DX12 把 shader 烘进 pipeline。是否在契约层把「program + PSO」聚合为一个「图形管线对象」？v1 可暂保持分离（GL 友好），但需记为已知债务，否则 Metal 落地时要返工。**建议**：v1 保持分离，文档标注；B5 全局收敛时再聚合。
2. **push constant 在 GL/DX11 的承载**：统一映射到一个约定 slot 的「push block」UBO（std140），还是具名 uniform？**建议**：统一走「push block UBO」，跨后端语义最一致（Vulkan 用真 push constant，GL/DX11 用小 dynamic UBO）。
3. ~~验证策略 A vs B~~ → **已定：B**（见 §7）。

---

## 附：后续阶段（B1–B5，便于全局对照）
- [x] **A1**：DrawSkybox 改通用原语 + 删 ABI（`d885d0eb`）。
- [x] **B0**：原语契约 v1 实现 + SpriteRenderer 脚手架 + 跨后端像素 smoke（`2d059e49`）。
- [x] **B1**：跨后端离屏像素 smoke gtest（先补 skybox），作为后续每次迁移的回归闸门（`560fc7d4`）。
- [~] **B2**：迁 Sprite/Mesh → 抽高层渲染器 + 删其旧 ABI + 像素测试。
  - [x] **B2a** 迁 Sprite（`SpriteBatchRenderer`，默认/SDF/VFX）+ 删 DrawSpriteBatch（`11d61181`..`43240e8e`）。
  - [ ] **B2b** 迁 Mesh（`MeshRenderer`）—— 阻塞于 SSBO + 实例化原语，方向待定，见 [`../plans/B2b_mesh_migration_scoping.md`](../plans/B2b_mesh_migration_scoping.md)。
- [ ] **B3**：迁 Particles（验证 Dispatch/实例化）；DrawPostProcess 一并考虑。
- [ ] **B4**：迁 Hair（SSBO + 多段绘制）。
- [ ] **B5**：全局绑定收敛（shadow map / global uniforms / program+PSO 聚合）。
