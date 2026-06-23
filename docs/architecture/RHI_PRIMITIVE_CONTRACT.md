# RHI 通用绘制原语契约 v1（B0）

> 目标：把 `CommandBuffer` 从「每效果一个虚函数」收敛为一组**后端无关的通用绘制原语**（pipeline / bindings / draw / dispatch），效果逻辑上移到高层渲染器。
> 本文是契约设计 + Metal 纸面映射校验 + 三后端实现要点。**本步（B0）只新增原语、不删旧 ABI、不迁移任何效果。**

---

## 1. 背景与动机

当前 RHI 成本是 **O(效果数 × 后端数)**：`DrawMeshBatch / DrawPostProcess / DrawHairStrands`（+ 已删的 `DrawSkybox / DrawSpriteBatch / DrawParticles3D`）每个都在 GL/Vulkan/DX11 各实现一遍。

- **加后端**（确定要加 Metal）= 把每个效果重写一遍。
- **行为漂移**：同一效果在不同后端的实现会跑偏（如 `RHI_UNIFICATION_TASKS.md:107` 记录的 skybox view 平移处理 GL 与其它后端不一致）。

收敛后成本变 **O(效果数 + 后端数)**：效果只写一次（高层渲染器跑在原语上），后端只实现一组**有限**原语。skybox spike 已实证：3 份后端实现 → 1 个 `SkyboxRenderer`，并消除了该类漂移。

---

## 2. 设计原则

1. **粒度 = pipeline + 资源绑定 + draw/dispatch**，不是「GL 式逐个 glBind」。现代 API（Vulkan/DX12/Metal）要的是更少、更批量的状态变更。
2. **slot-based 绑定 + 延迟组装（v1）**：高层渲染器按 slot 绑 VB/IB/纹理/UBO/SSBO/push-constant，后端把这些累积到执行器成员，在 `Draw*` 时一次性组装 pipeline + descriptor 并提交。这沿用 skybox spike 已验证的模式，能映射到全部后端。
3. **绑定组 / argument buffer（已落地，有界）**：把多个 UBO/纹理/SSBO 打包成一次原子绑定。已实现 `CommandBuffer::BindGroup(const BindGroupDesc&)`（`BindGroupDesc` 见 `rhi_types.h`，含 `uniform_buffers`/`textures`/`storage_buffers` 三组 entry，slot 语义与逐 slot `Bind*` 完全一致）。基类提供**功能性默认实现**：逐 entry 转发到 `BindUniformBuffer`/`BindTexture`/`BindStorageBuffer`（真实绑定，非 no-op），各后端的延迟组装阶段天然把这批绑定汇成一次提交（Vulkan→单个 descriptor set / DX11→连续 b/t/s 寄存器 / GL→同 VAO 内批量 `glBindBufferBase` + 纹理单元）。活体消费者：`SpriteRenderer` 用它绑 {PerFrame UBO + `u_texture`}（像素闸门 `sprite_primitive_smoke_test`）；另有专项多 UBO 成组闸门 `bind_group_pixel_smoke_test`（DX11 验证 b0/b1 正确分发）。**更深一层**的 descriptor set / argument buffer **对象级**批量（预创建并缓存 descriptor set / DX12 descriptor table / Metal argument buffer）仍是后续性能优化方向；当前默认实现保证语义正确且 slot 编号已为之预留，后端可按需覆写 `BindGroup` 做对象级批量。
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
// B5-3b：原 SetPipelineState(pso)+BindShaderProgram(prog) 已聚合为单一图形管线对象。
// 经 RhiDevice::GetGraphicsPipeline(pso, program) 惰性去重缓存为句柄，program==0 = 仅 PSO 状态。
void BindPipeline(unsigned int graphics_pipeline_handle);        // [B5-3b]  pso 子状态 + program 聚合

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

// ---- 间接绘制 / compute ----
// compute:   IRhiCompute::DispatchCompute / BeginComputePass / ComputeMemoryBarrier（设备级，复用既有）
void DrawIndexedIndirect(unsigned int indirect_buffer,          // [新增 B2b-5] CommandBuffer 级间接索引绘制
                         uint32_t byte_offset = 0);
// 从 indirect_buffer 的 byte_offset 处读一条 DrawElementsIndirectCommand
// {count, instance_count, first_index, base_vertex, base_instance} 发起索引实例化绘制；
// 三后端实现见 §6.4，args 布局三端一致（5×uint32）。indirect_buffer 经 CreateIndirectBuffer 创建。
```

> **实现进度（P0 / B2b / 2c / Final-Feat，2026-06）**：契约的实例化与 SSBO 能力已落地——`DrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance)`（P0a 新增重载，**不改** `DrawIndexed` 签名）、`BindStorageBuffer(slot, handle, offset, size)`（P0b，图形阶段读 SSBO）、`DrawIndexedIndirect(indirect_buffer, byte_offset)`（B2b-5，CommandBuffer 级间接绘制）。活体消费者为 `MeshRenderer`（B2b-1..5：静态 / 蒙皮 / 实例化 / depth-only / 间接 forward PBR；2c-1..5：高级 shading 全模式 + 地形 splat/积雪 + WBOIT + DDGI/LightProbe；Final-Feat-1..7：CSM/蒙皮/实例化/聚光/morph 高级 shading + 外部常驻 VAO/EBO + 共享网格模板），各配跨后端像素 smoke（smoke 76→92→**181**）。注：2c / Final-Feat **未新增任何 CommandBuffer 原语**，均复用上述原语，系 `MeshRenderer` 方法而非新原语。进度详见 [`RHI_ABSTRACTION_BOUNDARY.md`](./RHI_ABSTRACTION_BOUNDARY.md) §2–§3。

> **实现进度（A / B 终态收尾，2026-06）**：
> - **A — `BindVertexBuffer` slot 化 + `VertexInputRate`**：兑现 §3 终态签名 `BindVertexBuffer(uint32_t slot, handle, stride, attrs, VertexInputRate rate=PerVertex)`，**就地改签名**（非新增重载）并一次性迁移全部 22 个调用点为 `slot=0`/`PerVertex`（语义与旧单 slot 一致）。三后端：GL `glVertexAttribDivisor(loc, rate==PerInstance?1:0)`；DX11 多 slot `IASetVertexBuffers` + 据 attr/rate 显式自建多 slot input layout（反射布局恒落 slot0/per-vertex，无法表达 per-instance，故 slot>0 或 PerInstance 时经 `GetOrCreatePrimInputLayout` 全量自建）；Vulkan 多 slot binding（`binding=slot`，`inputRate` 按 rate）烘进 PSO。活体消费者 + 像素闸门 `instanced_vertex_rate_pixel_smoke_test`（slot0 per-vertex quad + slot1 per-instance 实例流画 3 个互不重叠 quad，DX11 验证）。自此实例化实例顶点流不再仅靠 SSBO 绕过。
> - **B — `BindGroup` 绑定组**：见 §2.3。新增 `BindGroup(const BindGroupDesc&)`，默认转发到逐 slot `Bind*`，`SpriteRenderer` 改用它绑 {PerFrame UBO + 纹理}，专项闸门 `bind_group_pixel_smoke_test`。

### 旧原语的归并关系（过渡期保留，迁移完再删）
| 旧（skybox spike） | 新 | 处理 |
|---|---|---|
| `BindVertexBuffer(buffer, stride, attrs)` | `BindVertexBuffer(slot=0, …, PerVertex)` | **A 已就地改签名**：全部 22 调用点迁移为 `slot=0`/`PerVertex`（无转调封装，slot=0/PerVertex 默认参数保证语义不变） |
| `BindTextureCube(slot, h)` | `BindTexture(slot, h, TexCube)` | 旧转调新 |
| `PushConstantsMat4(m)` | `PushConstants(Vertex, 0, &m, 64)` | **B5-3a 已泛化删旧**（无转调；skybox/PP/compute 统一经字节块 ABI） |
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
| `BindPipeline(handle)`（B5-3b 聚合 pso+program） | `setRenderPipelineState:`（含 vertex/fragment function + color/blend + vertex descriptor）+ `setDepthStencilState:` + `setCullMode:` + `setFrontFacingWinding:` + `setTriangleFillMode:` | **完美对应**：单一图形管线对象正是 Metal `MTLRenderPipelineState` 的语义（shader 烘进 PSO） |
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

**校验结论**：契约整体干净映射到 Metal，无需为 Metal 反推新概念。原 §8 的 `BindShaderProgram` 与 PSO 分离问题已于 **B5-3b 聚合为单一图形管线对象 `BindPipeline`**（见上表），Metal `MTLRenderPipelineState` 自此完美对应。

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

### 间接绘制 `DrawIndexedIndirect`（B2b-5，三后端实现要点）

`CommandBuffer::DrawIndexedIndirect(indirect_buffer, byte_offset)` 从 indirect buffer 读一条 `DrawElementsIndirectCommand` 发起索引实例化绘制。**args 布局三后端严格一致**：5×uint32 `{count, instance_count, first_index, base_vertex, base_instance}`（与 GL `DrawElementsIndirectCommand` / Vulkan `VkDrawIndexedIndirectCommand` 同序同宽）。

- **OpenGL**：`glMultiDrawElementsIndirect(GL_TRIANGLES, index_type, (void*)byte_offset, draw_count=1, stride=sizeof(cmd))`，args 缓绑到 `GL_DRAW_INDIRECT_BUFFER`（需 GL4.3+）。
- **Vulkan**：`vkCmdDrawIndexedIndirect(cmd, buffer, byte_offset, drawCount=1, stride)`；args 缓须含 `VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT`；被命令缓冲引用的资源在 fence 前不可删（教训 1）。
- **DX11**：`DrawIndexedInstancedIndirect(args_buffer, byte_offset)`；args 缓须 `MISC_DRAWINDIRECT_ARGS`。**限制**：该缓**不能 `USAGE_DYNAMIC`**（多数驱动 / WARP 不支持 `Map`），须 `USAGE_DEFAULT` + `UpdateSubresource`（局部更新走 `D3D11_BOX`），否则 `instance_count` 不生效、仅渲首实例；`MISC_DRAWINDIRECT_ARGS` 不允许任何 `BindFlags`。

**契约同实例化（见上 DX11 限制）**：DX11 `SV_InstanceID` 仍从 0 起，`base_instance` 偏移须经 0 基 SSBO 索引表达，不能靠 `base_instance` 取数；`MeshRenderer::DrawIndirect` 因此恒置 `base_instance=0`。

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
| 实例化（`DrawInstanced`/`DrawIndexedInstanced`） | B3（particles） | ✅ 已落地；`Draw` 现有签名**不改**（现有 `Draw(36u,0u)` 调用会把 `0` 误读为 instance_count），改为新增 `Draw*Instanced` 重载 |
| `BindVertexBuffer` +slot/+PerInstance（实例顶点流） | A（终态收尾） | ✅ 已落地：**就地改签名**（slot=0/PerVertex 默认参数保证旧语义），全部 22 调用点一次性迁移；像素闸门 `instanced_vertex_rate_pixel_smoke_test`。详见 §3 进度note |
| `BindGroup`（绑定组 / argument buffer） | B（终态收尾） | ✅ 已落地：默认转发 + `SpriteRenderer` 消费 + `bind_group_pixel_smoke_test`。详见 §2.3 |
| `BindStorageBuffer`（图形阶段读 SSBO） | B4（hair） | 仅 hair 消费 |
| 多 UBO 槽（PerScene/PerMaterial） | B2（mesh/sprite 生产路径） | sprite2d 仅需 PerFrame |

> 说明：契约（§3）描述的是**完整终态**；§7 是**实现节奏**。终态接口可在 CommandBuffer 上以默认 no-op 虚函数先声明（文档化 surface），但其**后端实现**与消费者同批落地，保证落地即被像素测试覆盖。
>
> **签名变更约束**：`Draw(vertex_count, first_vertex=0)` 现有签名保持不变，实例化通过**新增重载** `Draw*Instanced` 表达（避免 `Draw(36u,0u)` 把 `0` 误读为 instance_count）。`BindVertexBuffer` 不同：A 阶段**就地改签名**为 `BindVertexBuffer(slot, …, rate)`，因新增参数（前置 `slot`、后置 `rate`）均有默认值（`rate=PerVertex`）且全部 22 调用点显式补 `slot=0`，旧 `BindVertexBuffer(buffer, …)` 形态不再存在但语义零漂移——无 `Draw` 那种「位置实参被误读」风险，故选就地迁移而非加重载。

---

## 8. 待决问题（实现前需拍板）

1. ~~**`BindShaderProgram` 与 PSO 的关系**~~ → **已定并实现（B5-3b）**：聚合为单一「图形管线对象」`GraphicsPipelineDesc{pso,program}` + `BindPipeline(handle)`（设备级 `GetGraphicsPipeline(pso,program)` 惰性去重缓存，`program==0`=仅 PSO 状态），删 `BindShaderProgram`+`SetPipelineState` 两分离原语。GL 友好性以「program==0 仅设状态」保留（Pass 层 GPU-driven 自绑/渲染器覆盖语义不变）。为 Metal/DX12 铺路。
2. ~~**push constant 在 GL/DX11 的承载**~~ → **已定并实现（B5-3a）**：统一走「push block UBO」——编译器把 GL/ESSL 的 `layout(push_constant)` 降级为按阶段区分的 std140 块 `DsePush{VS,FS,CS}`，DX11 用 b0 push cbuffer，Vulkan 用真 push constant。`PushConstants(stage,off,data,size)` 字节块 ABI 三后端统一路由。
3. ~~验证策略 A vs B~~ → **已定：B**（见 §7）。

---

## 附：后续阶段（B1–B5，便于全局对照）
- [x] **A1**：DrawSkybox 改通用原语 + 删 ABI（`d885d0eb`）。
- [x] **B0**：原语契约 v1 实现 + SpriteRenderer 脚手架 + 跨后端像素 smoke（`2d059e49`）。
- [x] **B1**：跨后端离屏像素 smoke gtest（先补 skybox），作为后续每次迁移的回归闸门（`560fc7d4`）。
- [~] **B2**：迁 Sprite/Mesh → 抽高层渲染器 + 删其旧 ABI + 像素测试。
  - [x] **B2a** 迁 Sprite（`SpriteBatchRenderer`，默认/SDF/VFX）+ 删 DrawSpriteBatch（`11d61181`..`43240e8e`）。
  - [x] **B2b** 抽 `MeshRenderer`（后端无关 forward-PBR 能力）→ **`DrawMeshBatch` ABI 已删**（阶段4 M1–M4）：
    - [x] **P0a** `DrawIndexedInstanced` 三后端原语（`f38d0b13`/`e0f061f7`）。
    - [x] **P0b** `BindStorageBuffer` 图形阶段三后端原语（`10f3ff2d`）+ 组合像素 smoke（`68c81b84`）。
    - [x] **B2b-1** forward PBR 静态网格（`ee97d1ab`）。
    - [x] **B2b-2** 骨骼蒙皮（bone SSBO，`88301df8`）。
    - [x] **B2b-3** GPU 实例化（instance SSBO，`34bb4328`）。
    - [x] **B2b-4** depth-only / shadow（三后端深度回读，`eaf61c2d`）。
    - [x] **B2b-5** GPU-driven 间接绘制（`DrawIndexedIndirect` 三后端，`25fb30a6`）。
    - [x] **2c-1..5 + Final-Feat-1..7** 高级 shading 全模式（`debcaead`…`db8c320b`）：shading_mode/SSS/clearcoat/POM、地形 splat/积雪、WBOIT、DDGI/LightProbe、CSM、蒙皮/实例化/聚光/morph、外部常驻 VAO/EBO（tiled terrain）、共享网格模板去重（tree）——均复用现有原语，各配跨后端像素 smoke。
    - [x] **删 `DrawMeshBatch` ABI**：✅ **已删**（阶段4 M1–M4）——先补齐 MeshRenderer 三处覆盖缺口（M1 蒙皮×硬件实例化 / M2 编辑器视图模式 wireframe·overdraw·force_unlit / M3 GBuffer-RSM MRT），M4 加 `MeshRenderer::DrawBatch` 分发层、迁 6 调用点（render_scene DrawOpaqueCpu/DrawTransparent → 常驻 `FramePipeline::cpu_mesh_renderer_`），删纯虚 + 三后端 forwarder + 三执行器实现 + `gl_draw_executor_mesh.cpp` 整文件。全仓已无 `DrawMeshBatch(` 声明/调用（仅残留若干说明性注释）。详见 [`RHI_ABSTRACTION_BOUNDARY.md`](./RHI_ABSTRACTION_BOUNDARY.md) §8.2 D10。
    - 基线：smoke 76 → 92（B2b-2..5）→ **181**（2c-1..5 + Final-Feat-1..7 各增跨后端像素 smoke），详见 [`../plans/B2b_mesh_migration_scoping.md`](../plans/B2b_mesh_migration_scoping.md)。
- [x] **B3**：✅ 迁 Particles（`ParticleRenderer` + SSBO 实例化），**删 `DrawParticles3D` ABI**。
- [x] **阶段 2b（后处理）**：✅ 迁 `DrawPostProcess` → `PostProcessRenderer`（全屏 quad/std140 UBO 契约）。**全部 29 效果已迁**（含 bloom_composite/atmosphere_sky/ui_overlay 手写 HLSL 簇）；compute mip 链选 **Option A**——新增 CommandBuffer 级 `DispatchComputePass` 原语 + `BloomRenderer`（compute/quad 分支），**已删 `DrawPostProcess` ABI**。详见 [`RHI_ABSTRACTION_BOUNDARY.md`](./RHI_ABSTRACTION_BOUNDARY.md) §2 阶段2b。
- [x] **B4（毛发）**：✅ 迁 Hair（`HairRenderer`，HairUniforms UBO + position/tangent SSBO + 逐 strand `Draw`，`PrimitiveTopology` 贯穿 PSO），**删 `DrawHairStrands` ABI**。
- [x] **B5（全局绑定收敛）**：✅ B5-1 删 `BindTextureCube`（归并 `BindTexture(…,TexCube)`）；B5-3a `PushConstantsMat4` 泛化为字节块 `PushConstants(stage,off,data,size)`；B5-3b `BindShaderProgram`+`SetPipelineState` 聚合为单一 `BindPipeline(handle)`（为 Metal/DX12 铺路）。
- [x] **D6（相机解耦）**：✅ 引入 `engine/render/frame_context.h`，从 `CommandBuffer` 移除 `SetCamera/GetView/GetProjectionMatrix`，相机经 `FrameContext` 由各 Pass 显式下传。
- [x] **A（终态收尾）**：✅ `BindVertexBuffer` slot 化 + `VertexInputRate`（见 §3 进度 note）。
- [x] **B（终态收尾）**：✅ `BindGroup` 绑定组（见 §2.3）。

> **尚未兑现的「完整终态」**（非债务，属下一阶段，需真实多后端环境）：① **Metal/DX12 后端**——整个边界重画的终极验收（「加后端=O(1)」需新后端实证，当前为「设计成立、未经新后端验证」）；② **GL/Vulkan 真机逐像素验证（§8.2 D7）**——需真实 GPU 或多后端 CI，VM 软渲（WARP/llvmpipe）只验解析真值。
