# B2b 摸底 / 范围评估：迁移 `DrawMeshBatch` → `MeshRenderer`

## 1. 现状（已摸实现）

`DrawMeshBatch` 不是单一效果，而是**引擎整个 3D 渲染核心**。三后端实现规模：
- GL：`gl_draw_executor_mesh.cpp` 约 1115 行（`DrawMeshBatch` 116→末尾）
- DX11：`dx11_draw_executor.cpp` `DrawMeshBatch` 约 509→1240（~700 行）
- Vulkan：`vulkan_draw_executor.cpp` `DrawMeshBatch` 约 1960→2690（~730 行）

覆盖特性（远超 sprite）：
- **多 pass 变体**：forward PBR / deferred gbuffer / depth-only（shadow map）
- **着色模型**：PBR / HalfLambert-Skin / HalfLambert-Static / Toon / Watercolor / FaceSDF（`shading_mode` 0/2/3/4/5/6）
- **材质**：albedo/normal/MR/emissive/occlusion 5 纹理槽 + SSS/clearcoat/anisotropy/POM/alpha-test/double-sided
- **骨骼蒙皮**：bone matrices **SSBO**（含 palette 去重 + per-instance bone offset）
- **GPU 实例化**：GPUInstanceData **SSBO**（binding 5）、material **SSBO**（binding 9）、DrawCommands **SSBO**（binding 6，compute 间接绘制）
- **全局光照**：DDGI irradiance atlas、LightProbe SH、点/聚光灯（SSBO 或 UBO fallback）
- **地形 splatmap**（weight map + 4 layer）、**积雪**、**WBOIT**、Hi-Z occlusion culling

## 2. 调用点（迁移面）

- `mesh_render_system.cpp:1559`（transparent_items_）+ 不透明走 GPU-driven 间接路径
- `terrain_system.cpp:379, 800`
- `tree_system.cpp:443`
- `grass_system.cpp:920`
- `spine_system.cpp:373`（2D 蒙皮）

## 3. 关键阻塞：通用原语契约缺 SSBO / 间接绘制 / 实例化

B0 契约只有：`BindShaderProgram/BindVertexBuffer/BindTexture(2D)/BindTextureCube/BindIndexBuffer/BindUniformBuffer/PushConstantsMat4/Draw/DrawIndexed/SetPipelineState`。

`DrawMeshBatch` **根本性依赖**当前契约里没有的能力：
1. **SSBO 绑定原语**（骨骼/实例/材质）—— 任务路线图把 SSBO 排在 **B4（hair）**，B2b 之后。
2. **实例化 DrawIndexed**（`instance_count`/`first_instance`）—— 任务已注明「若用实例化需扩 DrawIndexed（三后端+文档）」。
3. **间接绘制**（GPU-driven `DrawIndexedIndirect` + compute 生成 DrawCommands）。
4. 多种内建 program 访问器（gbuffer / shadow / 各 shading_mode 变体）。

→ 忠实迁移 B2b 必须**先**扩展原语契约（至少 SSBO + 实例化 DrawIndexed），而这与任务把 SSBO 放在 B4 的排序冲突。

## 4. 建议的子步拆解（每步独立闭环，参考 B2a）

**前置 P0（契约扩展，三后端 + 文档 + 像素/单测）**
- P0a：`DrawIndexed` 加 `instance_count`/`first_instance`（三后端 + 文档 + smoke）
- P0b：新增 SSBO 绑定原语 `BindStorageBuffer(slot, handle, offset, size)`（三后端：GL SSBO / DX11 StructuredBuffer SRV / Vulkan storage descriptor）+ 文档 + smoke
- （间接绘制 `DrawIndexedIndirect` 视是否迁 GPU-driven 路径再定，可推迟）

**主迁移（按 shading 复杂度递增，每步配跨后端像素 smoke）**
- B2b-1：默认 forward PBR 静态网格（单 mesh、5 纹理槽、PerFrame/PerScene/PerMaterial UBO）→ 抽 `MeshRenderer` 骨架 + 像素 smoke
- B2b-2：骨骼蒙皮（bone SSBO + palette）
- B2b-3：GPU 实例化（instance SSBO + 实例化 DrawIndexed）
- B2b-4：depth-only / shadow + gbuffer 变体
- B2b-5：高级 shading（toon/watercolor/SSS/FaceSDF）+ 地形 splat / 积雪 / WBOIT / DDGI / 光探针
- B2b-6：迁所有调用点 → 删 ABI → 全绿提交

> ⚠ **实际执行与本拆解有出入，详见 §6 实施记录**：实际下达的 B2b-5 被收窄为「GPU-driven 间接绘制」（本拆解里本在 P0 括号备注的可选项）；本拆解原定义的 B2b-5「高级 shading + 地形/雪/WBOIT/DDGI」与 B2b-6「删 ABI」**均未执行**（用户决策推迟）。

GPU-driven 间接路径（compute 生成 DrawCommands SSBO + 间接绘制 + Hi-Z culling）规模最大、风险最高，可能需要独立阶段。

## 5. 决策点（需用户定夺）

B2b 体量 ≈ B2a 的数倍，且**被 SSBO 原语前置阻塞**（与任务把 SSBO 排在 B4 的顺序冲突），并触及引擎最核心、回归风险最高的 3D 路径。可选方向：
- **A**：按上面拆解全量做（先做 P0 契约扩展，含 SSBO 提前到 B2b 之前），逐子步闭环。
- **B**：仅迁可不依赖 SSBO 的子集（默认 forward PBR 静态网格，B2b-1），把蒙皮/实例化/GPU-driven 留到 SSBO 原语（B4）落地后再迁——B2b 分两段。
- **C**：先做 B4（SSBO/compute 原语）再回头做完整 B2b（重排路线图）。

---

## 6. 实施记录（截至 2026-06，HEAD `db8c320b`）

**实际路线 = A 的变体**：先做 P0 前置原语（含 SSBO 提前），再逐子步抽 `MeshRenderer`，但**不删 `DrawMeshBatch` ABI**（与 MeshRenderer 并存）。每子步：三后端零错编译 + gtest 不回归 + 跨后端像素 smoke 三后端绿 + 直推。

| 子步 | 提交 | 内容 | smoke 基线 |
|---|---|---|---|
| P0a | `f38d0b13`→`e0f061f7` | `DrawIndexedInstanced` 三后端原语 | — |
| P0b | `10f3ff2d`＋smoke `68c81b84` | `BindStorageBuffer(slot,handle,offset,size)` 三后端（图形阶段） | 76 |
| B2b-1 | `ee97d1ab` | forward PBR 静态网格（`ForwardPbr`，Cook-Torrance + 5 纹理槽） | 76 |
| B2b-2 | `88301df8` | 骨骼蒙皮（`ForwardPbrSkinned`，bone 矩阵 SSBO@slot0；离线编译器 `@SSBO_LOW_REGISTERS` 让 DX11 SSBO 落 t0） | 80 |
| B2b-3 | `34bb4328` | GPU 实例化（`ForwardPbrInstanced`，instance model SSBO，gl_InstanceIndex 取数；DX11 恒 first_instance=0 + 0 基 SSBO 索引） | 84 |
| B2b-4 | `eaf61c2d` | depth-only / shadow（`ForwardPbrDepth`，三后端深度回读 `ReadRenderTargetDepthFloatWithSize`）；修 depth-only pass 须 `clear_color_enabled=true` 才触发深度清屏 | 88 |
| B2b-5 | `25fb30a6` | GPU-driven 间接（CommandBuffer 级 `DrawIndexedIndirect` 三后端 + `MeshRenderer::DrawIndirect`）；修 DX11 `DRAWINDIRECT_ARGS` 不能 `USAGE_DYNAMIC`（WARP 不支持 Map），改 `USAGE_DEFAULT`+`UpdateSubresource` | 92 |
| 2c-1 | `debcaead` | 高级 shading 迁入 `MeshRenderer::DrawShaded`（`ForwardShaded`，shading_mode 0/2-6 + SSS/clearcoat/anisotropy/POM/alpha-test/double-sided）；DX11 culling_enabled 修复 | 102 |
| 2c-2 | `7295d247` | clustered 点光（≤ 64，UBO fallback）+ Cook-Torrance BRDF | 104 |
| 2c-3 | `b930db38` | 地形 splatmap（权重图 + 4 layer 混合）、积雪迁入 `DrawShaded` | 108 |
| 2c-4 | `5a46be01` | 透明 WBOIT（加权合成 OIT）迁入 `DrawShaded` | 112 |
| 2c-5 | `1048b815` | 全局光（LightProbe SH + DDGI irradiance atlas） | 116 |
| Final-Feat-1 | `5e460e2e` | `DrawShaded` 补 CSM 方向光阴影（PerScene UBO 48→304B，3 级 cascade + 3×3 PCF） | 121 |
| Final-Feat-2 | `fb986188` | 蒙皮 + 高级 shading（`DrawSkinnedShaded` / `ForwardSkinnedShaded`，bone 矩阵 SSBO@set7.b0） | 129 |
| Final-Feat-3 | `1c909e5a` | 硬件实例化 + 高级 shading（`DrawInstancedShaded` / `ForwardInstancedShaded`，每实例 model SSBO@set7.b0，gl_InstanceIndex 取数） | 137 |
| Final-Feat-4 | `c9b516e3` | 聚光灯 SpotLight（`FwdSpotLightUBO@set7.b1` + 内/外锥 smoothstep） | 145 |
| Final-Feat-5 | `f4645f46` | Morph target 形变（`DrawMorphShaded` / `ForwardMorphShaded`，morph 增量 SSBO@set7.b0） | 153 |
| Final-Feat-6 | `44c1f487` | 外部常驻 VAO/EBO + index_count_override（tiled terrain；`DrawShadedExternal` + `BuildShadedWorldVertexBuffer`，按 `first_index` 子段绘制） | 169 |
| Final-Feat-7 | `db8c320b` | 共享网格模板去重（shared_vertex_ptr，tree；`DrawSharedTemplateInstanced` + `BuildShadedLocalVertexBuffer`，一份局部模板 + 每实例 model SSBO） | **181** |

### 进展更新（2c-1..5 + Final-Feat-1..7）

原 §4 定义的「高级 shading」能力已在 `MeshRenderer::DrawShaded` 及其蒙皮/实例化/morph 变体中全部补齐，跨后端像素 smoke 由 92 升至 **181**（逐特性详见上表，三后端零错编译 + gtest 不回归 + 跨后端 RMSE<12）。下表为原「未实现特性」的现状：

| 调用点依赖的能力 | 现状 |
|---|---|
| shading_mode 2/3/4/5/6（HalfLambert-Skin/Static、Toon、Watercolor、FaceSDF）、SSS/clearcoat/anisotropy/POM、≤ 64 clustered 点光 | ✓ 2c-1 / 2c-2 |
| 地形 splatmap（权重图 + 4 layer）、积雪 | ✓ 2c-3 |
| 透明 WBOIT | ✓ 2c-4 |
| DDGI / LightProbe | ✓ 2c-5 |
| CSM 方向光阴影 | ✓ Final-Feat-1 |
| 骨骼蒙皮 + 高级 shading | ✓ Final-Feat-2 |
| GPU 实例化 + 高级 shading | ✓ Final-Feat-3 |
| 聚光灯 SpotLight | ✓ Final-Feat-4 |
| Morph target 形变 | ✓ Final-Feat-5 |
| 外部常驻 VAO/EBO + index_count_override（tiled terrain） | ✓ Final-Feat-6 |
| 共享网格模板去重（foliage / tree） | ✓ Final-Feat-7 |
| 2D 蒙皮（`spine_system`） | 待办（剩余能力缺口） |

### 仍未做（B2b-6：迁调用点 + 删 ABI）

MeshRenderer 侧渲染能力已基本齐备，但**实际迁移 6 个调用点**（`mesh_render_system` / `terrain_system` ×2 / `tree_system` / `grass_system` / `spine_system`）并**删除 `DrawMeshBatch` ABI** 仍未做。当前仍按用户决策（选项 B）保留 ABI 与 MeshRenderer 并存。下一步可逐调用点迁移到 `MeshRenderer`、回归像素后再删 ABI；`spine_system` 的 2D 蒙皮为迁移前需补齐的最后一处能力缺口。
