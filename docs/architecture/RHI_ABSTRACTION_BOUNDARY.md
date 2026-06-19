# RHI 抽象边界重画 —— 进度与现状（A1 → B0 → B1 → B2a → B2b）

> 本文结合**提交历史**与**当前代码现状**，描述 RHI 抽象边界从「每效果一个虚函数」收敛为「后端无关通用原语 + 高层渲染器」的重画过程、已落地范围、以及剩余边界。
> 配套文档：契约设计见 [`RHI_PRIMITIVE_CONTRACT.md`](./RHI_PRIMITIVE_CONTRACT.md)（终态接口 + Metal 映射）；mesh 迁移摸底见 [`../plans/B2b_mesh_migration_scoping.md`](../plans/B2b_mesh_migration_scoping.md)。
> 分支：`feature/engine-lib`。基线：单元 2281 / 集成 544 / smoke **92**（三后端 llvmpipe/WARP/lavapipe 像素 smoke 全绿；B2a 时为 68，P0+B2b-2..5 累加至 92）。
> HEAD：`25fb30a6`（B2b-5 间接绘制）。

---

## 1. 重画目标（一句话）

把 `CommandBuffer` 上**逐效果的虚函数**（`DrawSkybox/DrawSpriteBatch/DrawMeshBatch/DrawPostProcess/DrawParticles3D/DrawHairStrands`）替换为**一组有限的、后端无关的通用绘制原语**（pipeline / bindings / draw），效果逻辑上移到跑在原语上的**高层渲染器**。

成本模型：**O(效果数 × 后端数) → O(效果数 + 后端数)**。

```
重画前（每效果一个 ABI，各后端各实现一遍）        重画后（边界下沉到通用原语）
┌─────────────── 高层 ───────────────┐         ┌──────────── 高层渲染器 ────────────┐
│ Scene / UI / Particle / ...        │         │ SkyboxRenderer  SpriteBatchRenderer │
└───────────────┬────────────────────┘         │ MeshRenderer（B2b-1..5，ABI 并存） │
   cmd.DrawSkybox/DrawSpriteBatch/...           └──────────────┬─────────────────────┘
┌───────────────┴─ CommandBuffer ABI ┐            BindShaderProgram/BindVertexBuffer/
│ DrawSkybox  DrawSpriteBatch  DrawMesh│  ===>     BindTexture/BindUniformBuffer/
│ DrawPostProcess DrawParticles3D Hair │           PushConstants/DrawIndexed/...
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

内建资源访问器（`RhiDevice`，供高层渲染器复用引擎着色器/几何）：`GetBuiltinProgram(BuiltinProgram)`（按枚举取 Skybox/Sprite2D/SpriteFxSdf/SpriteFxVfx + **ForwardPbr/ForwardPbrSkinned/ForwardPbrInstanced/ForwardPbrDepth**（B2b-1..5）程序，取代每效果一个访问器——见 §8.2 D4）+ `GetSkyboxCubeVertexBuffer`；间接缓管理 `CreateIndirectBuffer/UpdateIndirectBuffer/DeleteIndirectBuffer`（B2b-5）。

### 3.2 仍存的逐效果 ABI（边界**之上**待下沉）

| 旧 ABI（`CommandBuffer` 纯虚） | 迁移阶段 | 状态 |
|---|---|---|
| ~~`DrawSkybox`~~ | A1 | ✅ 已删（`d885d0eb`） |
| ~~`DrawSpriteBatch`~~ | B2a | ✅ 已删（`43240e8e`） |
| `DrawMeshBatch(items)` | **B2b** | 🔶 `MeshRenderer` 已**并存**承接 forward-PBR 子集（B2b-1..5）；ABI 删除**推迟**（高级 shading 未迁，见 §5） |
| `DrawParticles3D(items, view, proj)` | B3 | ⏳ 待迁 |
| `DrawHairStrands(items, view, proj)` | B4 | ⏳ 待迁 |
| `DrawPostProcess(request)` | 未排期（B3 后） | ⏳ 待迁 |

### 3.3 高层渲染器（边界**之上**，跑在原语上）

| 渲染器 | 文件 | 性质 |
|---|---|---|
| `SkyboxRenderer` | `engine/render/skybox_renderer.{h,cpp}` | 生产（A1，DrawSkybox 已删） |
| `SpriteBatchRenderer` | `engine/render/sprite_batch_renderer.{h,cpp}` | 生产（B2a，DrawSpriteBatch 已删） |
| `SpriteRenderer` | `engine/render/sprite_renderer.{h,cpp}` | B0 契约验证脚手架（非生产） |
| `MeshRenderer` | `engine/render/mesh_renderer.{h,cpp}` | B2b-1..5 后端无关 forward-PBR 能力（静态/蒙皮/实例化/depth-only/间接）；与 `DrawMeshBatch` ABI **并存**，**未取代**（高级 shading 未覆盖） |

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

## 5. B2b 现状：原语已补齐，MeshRenderer 能力并存，ABI 删除被高级 shading 阻塞

`DrawMeshBatch` 是**引擎整个 3D 渲染核心**（三后端各 ~700–1115 行），覆盖 forward PBR / deferred gbuffer / shadow depth-only / 6 种 shading 模型 / 骨骼蒙皮 / GPU 实例化 / GPU-driven 间接绘制 / DDGI / 光探针 / 地形 splat / 积雪 / WBOIT，调用点 6 处（mesh/terrain×2/tree/grass/spine）。

**原阻塞已解**：B2b 原本被三项未实现原语阻塞，现均已落地（见 §3.1）：
1. **SSBO 绑定**（骨骼/实例）—— P0b `BindStorageBuffer` 提前到 B2b 之前落地（`10f3ff2d`）；
2. **实例化 DrawIndexed** —— P0a `DrawIndexedInstanced` 新增重载（`f38d0b13`）；
3. **间接绘制** —— B2b-5 `DrawIndexedIndirect` CommandBuffer 级原语（`25fb30a6`）。

据此抽出后端无关 `MeshRenderer`，逐子步闭环：B2b-1 静态 PBR / B2b-2 蒙皮 / B2b-3 实例化 / B2b-4 depth-only / B2b-5 间接，各配跨后端像素 smoke（smoke 76→92）。

**剩余阻塞（ABI 删除）**：`MeshRenderer` 仅实现 forward-PBR 静态/蒙皮/实例化/depth-only/间接，而 6 个调用点均依赖其**未实现**的高级特性：shading_mode 2/3/4/5/6（HalfLambert/Toon/Watercolor/FaceSDF）、SSS/clearcoat/anisotropy/POM、地形 splatmap + 积雪、WBOIT、clustered 点光（≤4～64）、DDGI/LightProbe。现阶段删 ABI 会破坏地形/植被/风格化角色/spine，且可能 gtest 仍绿但像素错（教训 3）。

**用户决策（选项 B）**：本阶段**保留 `DrawMeshBatch` ABI 与 MeshRenderer 并存**，ABI 删除推迟到高级 shading 迁移完成后。拆解见 [`../plans/B2b_mesh_migration_scoping.md`](../plans/B2b_mesh_migration_scoping.md)。

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

- **B2b**：抽 `MeshRenderer`（forward-PBR 静态/蒙皮/实例化/depth-only/间接）——已完成且与 `DrawMeshBatch` 并存。**剩：**高级 shading（toon/watercolor/SSS/FaceSDF + 地形 splat/积雪 + WBOIT + clustered 点光 + DDGI）迁入 MeshRenderer 后，迁 6 调用点 → 删 `DrawMeshBatch` ABI。
- **B3**：迁 `DrawParticles3D`（验证 Dispatch/实例化）；`DrawPostProcess` 一并考虑。
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
| D10 | **`MeshRenderer` 与 `DrawMeshBatch` ABI 并存**（forward-PBR 子集 vs 全特性） | 两条 mesh 路径并存，需纪律；高级 shading 调用点仍走旧 ABI | 高级 shading 迁入 MeshRenderer 后删 ABI（用户决策：本阶段保留） |

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
