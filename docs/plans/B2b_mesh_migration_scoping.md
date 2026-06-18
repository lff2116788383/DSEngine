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

GPU-driven 间接路径（compute 生成 DrawCommands SSBO + 间接绘制 + Hi-Z culling）规模最大、风险最高，可能需要独立阶段。

## 5. 决策点（需用户定夺）

B2b 体量 ≈ B2a 的数倍，且**被 SSBO 原语前置阻塞**（与任务把 SSBO 排在 B4 的顺序冲突），并触及引擎最核心、回归风险最高的 3D 路径。可选方向：
- **A**：按上面拆解全量做（先做 P0 契约扩展，含 SSBO 提前到 B2b 之前），逐子步闭环。
- **B**：仅迁可不依赖 SSBO 的子集（默认 forward PBR 静态网格，B2b-1），把蒙皮/实例化/GPU-driven 留到 SSBO 原语（B4）落地后再迁——B2b 分两段。
- **C**：先做 B4（SSBO/compute 原语）再回头做完整 B2b（重排路线图）。
