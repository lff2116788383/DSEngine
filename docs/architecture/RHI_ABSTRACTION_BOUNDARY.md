# RHI 抽象边界重画 —— 进度与现状（A1 → B0 → B1 → B2a）

> 本文结合**提交历史**与**当前代码现状**，描述 RHI 抽象边界从「每效果一个虚函数」收敛为「后端无关通用原语 + 高层渲染器」的重画过程、已落地范围、以及剩余边界。
> 配套文档：契约设计见 [`RHI_PRIMITIVE_CONTRACT.md`](./RHI_PRIMITIVE_CONTRACT.md)（终态接口 + Metal 映射）；mesh 迁移摸底见 [`../plans/B2b_mesh_migration_scoping.md`](../plans/B2b_mesh_migration_scoping.md)。
> 分支：`feature/engine-lib`。基线：单元 2281 / 集成 544 / smoke 68（三后端 llvmpipe/WARP/lavapipe 像素 smoke 全绿）。

---

## 1. 重画目标（一句话）

把 `CommandBuffer` 上**逐效果的虚函数**（`DrawSkybox/DrawSpriteBatch/DrawMeshBatch/DrawPostProcess/DrawParticles3D/DrawHairStrands`）替换为**一组有限的、后端无关的通用绘制原语**（pipeline / bindings / draw），效果逻辑上移到跑在原语上的**高层渲染器**。

成本模型：**O(效果数 × 后端数) → O(效果数 + 后端数)**。

```
重画前（每效果一个 ABI，各后端各实现一遍）        重画后（边界下沉到通用原语）
┌─────────────── 高层 ───────────────┐         ┌──────────── 高层渲染器 ────────────┐
│ Scene / UI / Particle / ...        │         │ SkyboxRenderer  SpriteBatchRenderer │
└───────────────┬────────────────────┘         │ (MeshRenderer…待迁)                 │
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

内建资源访问器（`RhiDevice`，供高层渲染器复用引擎着色器/几何）：
`GetSkyboxShaderProgram` / `GetSkyboxCubeVertexBuffer` / `GetSprite2DShaderProgram` / `GetSpriteFxSdfShaderProgram` / `GetSpriteFxVfxShaderProgram`。

### 3.2 仍存的逐效果 ABI（边界**之上**待下沉）

| 旧 ABI（`CommandBuffer` 纯虚） | 迁移阶段 | 状态 |
|---|---|---|
| ~~`DrawSkybox`~~ | A1 | ✅ 已删（`d885d0eb`） |
| ~~`DrawSpriteBatch`~~ | B2a | ✅ 已删（`43240e8e`） |
| `DrawMeshBatch(items)` | **B2b** | ⏳ 待迁（最重，见 §5） |
| `DrawParticles3D(items, view, proj)` | B3 | ⏳ 待迁 |
| `DrawHairStrands(items, view, proj)` | B4 | ⏳ 待迁 |
| `DrawPostProcess(request)` | 未排期（B3 后） | ⏳ 待迁 |

### 3.3 高层渲染器（边界**之上**，跑在原语上）

| 渲染器 | 文件 | 性质 |
|---|---|---|
| `SkyboxRenderer` | `engine/render/skybox_renderer.{h,cpp}` | 生产（A1，DrawSkybox 已删） |
| `SpriteBatchRenderer` | `engine/render/sprite_batch_renderer.{h,cpp}` | 生产（B2a，DrawSpriteBatch 已删） |
| `SpriteRenderer` | `engine/render/sprite_renderer.{h,cpp}` | B0 契约验证脚手架（非生产） |

---

## 4. 契约终态 vs 已实现（差距分析）

[`RHI_PRIMITIVE_CONTRACT.md`](./RHI_PRIMITIVE_CONTRACT.md) §3 描述的是**完整终态**；按「每个原语都要有活体消费者 + 像素覆盖、不留死代码」的原则（§7），尚未落地的原语**与其消费者同批实现**：

| 终态原语（契约 §3） | 现状 | 阻塞/排期 |
|---|---|---|
| `BindStorageBuffer(slot, h)`（图形阶段读 SSBO） | **未实现**（`CommandBuffer` 无此虚函数） | 消费者：mesh 骨骼/实例（B2b）、hair（B4）。契约把它排在 B4 |
| 泛化 `PushConstants(stage, offset, data, size)`（取代 `PushConstantsMat4`） | 仅 `PushConstantsMat4` | 待 mesh 等需要非 mat4 push 数据时落地 |
| 实例化 `Draw/DrawIndexed`（`instance_count`/`first_instance`） | 现签名**无** instance 参数 | mesh 实例化（B2b）、particles（B3）。**新增重载**表达，不改现签名（防静默语义漂移） |
| slot 化 `BindVertexBuffer(slot, …, rate)`（PerVertex/PerInstance） | 单 slot、无 rate | 同上，随实例化落地 |

> push-block UBO 决策（契约 §8.2）已在 **B2a 为 sprite 实现**：SDF/VFX 参数走单个 `SpriteFx` UBO（binding 0，三后端同布局映射），无需 per-backend push constant 分叉。mesh 迁移可复用该模式。

---

## 5. 剩余边界的关键发现（B2b 阻塞点）

`DrawMeshBatch` 是**引擎整个 3D 渲染核心**（三后端各 ~700–1115 行），覆盖 forward PBR / deferred gbuffer / shadow depth-only / 6 种 shading 模型 / 骨骼蒙皮 / GPU 实例化 / GPU-driven 间接绘制 / DDGI / 光探针 / 地形 splat / 积雪 / WBOIT，调用点 6 处（mesh/terrain×2/tree/grass/spine）。

**关键阻塞**：它根本性依赖**当前原语契约尚未实现**的能力——
1. **SSBO 绑定原语**（骨骼/实例/材质 SSBO）—— 契约把 `BindStorageBuffer` 排在 **B4**，在 B2b 之后；
2. **实例化 DrawIndexed**（`instance_count`/`first_instance`）；
3. **间接绘制**（GPU-driven `DrawIndexedIndirect` + compute 生成 DrawCommands）。

→ 忠实迁移 B2b 必须**先扩展原语契约**（至少 SSBO + 实例化 DrawIndexed），这与契约把 SSBO 排在 B4 的顺序冲突。可选方向（A 全量先扩契约 / B 先迁 forward PBR 静态网格子集 / C 重排路线图先做 B4）详见 [`../plans/B2b_mesh_migration_scoping.md`](../plans/B2b_mesh_migration_scoping.md) §5。**方向待定，本文不预设。**

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

- **B2b**：迁 `DrawMeshBatch → MeshRenderer`（需先扩 SSBO + 实例化 DrawIndexed，方向待定）。
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
| D4 | **`RhiDevice` 内建资源访问器随效果线性增长**（`GetSprite2D/SpriteFxSdf/SpriteFxVfx/Skybox…`） | mesh 会引入 gbuffer/shadow/toon/watercolor… 多 program，访问器爆炸，等于把 per-effect 表面搬到 device | B2b 前宜引入**按名/句柄的 shader 注册表**取代逐 program 访问器 |
| D5 | **每系统各持一个 `SpriteBatchRenderer`**（3 套动态 VBO/IBO/UBO） | 为满足 Vulkan 生命周期牺牲了跨系统合批与显存复用 | 可用**共享 frame-ring 分配器**优化（见 8.3） |
| D6 | **`CommandBuffer::GetView/GetProjectionMatrix()`** 把相机状态缓存在命令缓冲上 | 概念上相机属 frame/scene context，轻微耦合泄漏 | 引入 FrameContext 时收敛 |
| D7 | **像素闸门为 VM 软渲（llvmpipe/WARP/lavapipe）** | RMSE 不复现真机；只验解析真值，真机专属 bug 可能漏 | 已知并接受；条件允许时补一次真机基线 |
| D8 | **契约把 SSBO 排 B4，但 B2b(mesh) 先需 SSBO** | 路线图顺序自相矛盾，B2b 被阻塞 | §5 三方向之一，待定 |

### 8.3 优化空间（非阻塞，按收益排序）
1. **共享 frame-ring buffer 分配器**：所有高层渲染器（sprite 现 3 实例 + 未来 mesh/particle）共用一个按帧环形复用的动态顶点/UBO 池，解决 D5 的显存与合批问题，同时天然满足 Vulkan「提交前不复用」约束。
2. **shader 注册表**（解 D4）：device 暴露 `GetBuiltinProgram(name/enum)`，避免每效果加一个虚函数。
3. **bind group / argument buffer**（契约 §2.3）：把 PerFrame/PerScene/PerMaterial + 多纹理打成一次绑定，减少状态切换；为 Metal/DX12 铺路。
4. **实例化/间接绘制原语**随 B2b/B3 落地（新增重载，不改现签名）。

### 8.4 结论
- **合理**：是；增量、可回归、债务显式。
- **有优化空间**：有（8.3，主要是 frame-ring 分配器与 shader 注册表，建议在 B2b 之前先落 D4/D5 以免 mesh 阶段放大）。
- **最佳？**：是「低风险务实最优」，非「理论最优」（后者是 bind group + program/PSO 聚合的一次性大改）。
- **技术债**：未隐藏，8.2 全部登记并各有偿还阶段；唯一需即时决策的是 D8（B2b 的 SSBO 顺序）。
