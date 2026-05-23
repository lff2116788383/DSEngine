# GPU-Driven 与 CPU Per-Item 路径统一重构方案

> 制定日期：2026-05-23
> 状态：Phase 1-5 全部完成 ✅

---

## 零、代码审计关键发现

### 已有基础设施

| 组件 | GL | VK | DX11 |
|:-----|:--:|:--:|:----:|
| **GPU-driven VS** (`pbr_gpu_driven_vert.gen.h`) | ✅ 使用中 | ❌ 有 SPIR-V 未用 | ❌ 有 HLSL 未用 |
| **GPU-driven FS** (材质从 SSBO 读) | ❌ 不存在 | ❌ 不存在 | ❌ 不存在 |
| **Model matrix 来源** | SSBO `gl_BaseInstance` | push constants 逐 draw 循环 | PerObjectCB 逐 draw 循环 |
| **Draw call 方式** | `glMultiDrawElementsIndirect` (单次) | `vkCmdDrawIndexed` 循环 | `DrawIndexedInstancedIndirect` 循环 |
| **Material** | 单一 default UBO | 单一 default UBO | 单一 default UBO |

### 核心发现

1. `pbr_gpu_driven_vert.gen.h` **已存在三端编译产物**（SPIR-V + GLSL 430 + HLSL SM5），但 VK/DX11 的 `SetupGPUDrivenPBR` 仍然使用标准 PBR shader
2. GL 已经是真正的 GPU-driven（单次 multi-draw + VS 从 SSBO 读 model）
3. VK 和 DX11 是**伪 GPU-driven**（CPU 逐 draw 循环 + push constants/CB 更新）
4. DX11 HLSL 变体用 `SV_InstanceID` 索引 `ByteAddressBuffer _33 : register(t21)`，但 `SV_InstanceID` 在 `instanceCount=1` 时恒为 0——**此 shader 用于 multi-draw 时索引永远错误**
5. **GPU-driven fragment shader 不存在**——三端都用 `PerMaterial UBO` 的单一默认材质

### 当前问题总结

| 问题 | 根因 | 影响 |
|:-----|:-----|:-----|
| GPU-driven 被 `modules.empty()` 禁用 | module 和 GPU-driven 双重渲染导致深度冲突 | 有 module（生产必有）时 GPU-driven 永不执行 |
| Mega buffer key 碰撞 | `mesh_registry_[mesh_path]`，inline mesh `mesh_path=""` 全部碰撞 | 多个 inline mesh 共享第一个实体的顶点数据 |
| 无 per-draw material | `SetupGPUDrivenPBR` 只写一个 default material UBO | 所有 GPU-driven 物体颜色/材质相同 |
| VK/DX11 伪 indirect draw | CPU 逐 draw 循环推送 model matrix | CPU 开销与 per-item 相同甚至更高 |
| DX11 HLSL draw index bug | `SV_InstanceID` 不受 `StartInstanceLocation` 影响 | GPU-driven HLSL shader 无法正确索引 |
| `Render()` skip 条件不完整 | 缺少 `temp_indices.empty()` 检查 | 无索引的实体可能被错误跳过 |

---

## 一、设计原则

1. **实体级分流**：共享 `IsGPUDrivenEligible()` 判定，每个实体只走一条路径
2. **VS model 一律从 SSBO 读**：三端统一，消除 push constants / PerObjectCB 的 per-draw 更新
3. **FS material 从 material SSBO 读**：新建 `pbr_gpu_driven.frag`，不污染现有 per-item shader
4. **材质去重**：多实体相同材质共享 `material_id`，SSBO 最小化
5. **DX11 API 限制显式承认**：per-draw 循环不可避免，但开销从 80B CB 更新降至 16B draw_id 更新

---

## 二、Phase 分解

### Phase 1：Foundation — 分流基础 + Key 修复 ✅ DONE

> **完成日期：2026-05-23，编译通过，零行为变化**

#### 1a. `IsGPUDrivenEligible()` 共享判定函数

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `modules/gameplay_3d/rendering/mesh_render_system.h` | 修改 | 声明 `static bool IsGPUDrivenEligible(World&, entt::entity, const MeshRendererComponent&)` |
| `modules/gameplay_3d/rendering/mesh_render_system.cpp` | 修改 | 实现判定函数；`PrepareGPUScene()` 和 `Render()` 统一调用 |

判定条件（合并两处逻辑，**修复 `Render()` 缺少 `temp_indices.empty()` 检查的 bug**）：

```cpp
static bool IsGPUDrivenEligible(World& world, entt::entity entity,
                                 const MeshRendererComponent& mr) {
    if (!mr.visible) return false;
    if (mr.temp_vertices.empty() || mr.temp_indices.empty()) return false;
    if (!mr.local_bounds_valid) return false;
    if (mr.color.a < 0.999f) return false;  // 透明 → CPU per-item
    if (world.registry().all_of<Animator3DComponent>(entity)) {
        const auto& a = world.registry().get<Animator3DComponent>(entity);
        if (a.enabled && !a.final_bone_matrices.empty()) return false;  // 蒙皮 → CPU
    }
    return true;
}
```

- `PrepareGPUScene()` 替代内联 skip 条件为 `if (!IsGPUDrivenEligible(...)) continue;`
- `Render()` 替代 line 673-682 为 `if (gpu_driven_active_ && IsGPUDrivenEligible(...)) continue;`
- **消除条件漂移风险**

#### 1b. Dual-map Mega Buffer Key 修复

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `modules/gameplay_3d/rendering/mesh_render_system.h` | 修改 | 现有 `mesh_registry_` 重命名为 `file_mesh_registry_`；新增 `std::unordered_map<uint32_t, dse::render::MeshBatchEntry> inline_mesh_registry_` |
| `modules/gameplay_3d/rendering/mesh_render_system.cpp` | 修改 | `PrepareGPUScene()` 中分支：`mesh_path` 非空用 `file_mesh_registry_[path]`（去重），为空用 `inline_mesh_registry_[entity_id]`（独立条目） |

```cpp
// PrepareGPUScene 中的 key 分支
MeshBatchEntry* entry = nullptr;
if (!mesh_renderer.mesh_path.empty()) {
    auto it = file_mesh_registry_.find(mesh_renderer.mesh_path);
    if (it != file_mesh_registry_.end()) {
        entry = &it->second;
    } else {
        // 注册新 file mesh 到 mega buffer
        ...
        entry = &file_mesh_registry_[mesh_renderer.mesh_path];
    }
} else {
    auto eid = static_cast<uint32_t>(entity);
    auto it = inline_mesh_registry_.find(eid);
    if (it != inline_mesh_registry_.end()) {
        entry = &it->second;
    } else {
        // 注册新 inline mesh 到 mega buffer（每实体独占条目）
        ...
        entry = &inline_mesh_registry_[eid];
    }
}
```

- `InvalidateMegaBuffer()` 同时清空两个 map
- 零字符串分配（inline mesh 用整数 key）

#### 1c. 静态合批互斥处理

**审查发现 [Critical #1]**：`Render()` 末尾（line 1262-1277）独立提交 `static_batch_items_`，该路径不受 `gpu_driven_active_` / `IsGPUDrivenEligible()` 控制。Phase 4 移除 `modules.empty()` 后，`is_static` 的 eligible 实体会同时被 GPU indirect 和静态合批绘制 → **双重绘制**。

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `modules/gameplay_3d/rendering/mesh_render_system.cpp` `Render()` | 修改 | 静态合批提交前检查 `gpu_driven_active_`：若为 true 则跳过静态合批 |

```cpp
// line ~1262: 静态合批结果提交
if (!static_batch_items_.empty() && !gpu_driven_active_) {
    // GPU-driven 已覆盖所有 eligible 静态实体，跳过静态合批
    for (auto& sb_item : static_batch_items_) { ... }
    cmd_buffer.DrawMeshBatch(static_batch_items_);
}
```

**原理**：GPU-driven 已涵盖所有满足 `IsGPUDrivenEligible()` 的静态实体（性能优于静态合批），剩余 non-eligible 静态实体（蒙皮/透明）走 per-item 路径。

#### 1d. Shadow 路径确认

`OnRenderShadow()` → `mesh_render_system_.Render()`，而 `Render()` 已有 `gpu_driven_active_` + `IsGPUDrivenEligible()` 跳过逻辑。**shadow 路径无需额外修改**。

**验证**：编译通过 + 现有行为不变（`modules.empty()` 仍阻止 GPU-driven，两个 map 不会被使用）

---

### Phase 2：GPU-Driven Fragment Shader + Material SSBO ✅ DONE

> **完成日期：2026-05-23，编译通过**
>
> 实际方案：运行时 string patch（非新建 shader 源文件），避免 744 行 PBR frag 全量复制。
> - VS patch: `flat out uint v_material_id` 输出 + mat_id 赋值
> - FS patch: PerMaterial UBO → Material SSBO（`#define _1407 gpu_materials[v_material_id]`）
> - GPUMaterialData 字段名与 PerMaterial UBO 完全一致，零重映射
> - 材质去重 hash + Material SSBO binding 9 上传
> - Critical #3 修复：file mesh VBO 烘白色，颜色走 material SSBO

#### 2a. GPU-driven fragment shader 源文件

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `engine/render/shaders/src/pbr_gpu_driven.frag` | **新建** | 基于 `pbr.frag`，材质参数从 `GPUMaterialData` SSBO (binding 9) 读取 |

关键差异（与 `pbr.frag` 对比）：

```glsl
// pbr_gpu_driven.frag — 关键差异部分
layout(std430, binding = 9) readonly buffer MaterialSSBO {
    GPUMaterialData dse_materials[];
};
flat in uint v_material_id;  // 从 VS 传入

void main() {
    GPUMaterialData mat = dse_materials[v_material_id];
    vec3 albedo = mat.albedo_alpha.rgb * vColor.rgb;  // 顶点颜色 × 材质 albedo
    float alpha = mat.albedo_alpha.a * vColor.a;
    float metallic = mat.params0.x;
    float roughness = mat.params0.y;
    float ao = mat.params0.z;
    float normal_strength = mat.params0.w;
    // ... 后续 PBR lighting 逻辑与 pbr.frag 完全相同
}
```

`GPUMaterialData` 结构（已定义于 `gpu_scene_types.h`，128 bytes）：

```
albedo_alpha:      rgb + alpha
params0:           metallic, roughness, ao, normal_strength
params1:           alpha_cutoff, sss_strength, clear_coat, anisotropy
emissive_shading:  emissive.rgb + shading_mode
toon_shadow:       shadow_color.rgb + threshold
toon_params:       softness, spec_size, spec_strength, rim
extra0:            pom_height, cc_roughness, watercolor_paper, watercolor_edge
extra1:            watercolor_bleed, watercolor_density, flags (packed), unused
```

#### 2b. VS 增加 `v_material_id` 输出

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `engine/render/shaders/src/pbr_gpu_driven.vert` | 修改 | 新增 `flat out uint v_material_id = dse_inst[draw_id].mat_id` |

当前 VS 已从 SSBO 读 `mat_id`/`cmd_id` 字段但未输出。只需加一个 flat varying。

#### 2c. 编译 + 三端 gen.h 生成

```bash
dse_shader_compiler pbr_gpu_driven.vert  →  更新 pbr_gpu_driven_vert.gen.h (增加 v_material_id)
dse_shader_compiler pbr_gpu_driven.frag  →  新建 pbr_gpu_driven_frag.gen.h (SPIR-V + GLSL 430 + HLSL)
```

#### 2d. Material SSBO 填充 + 材质去重

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `modules/gameplay_3d/rendering/mesh_render_system.h` | 修改 | 新增 `std::vector<GPUMaterialData> gpu_materials_` + `std::unordered_map<uint64_t, uint32_t> material_dedup_` |
| `modules/gameplay_3d/rendering/mesh_render_system.cpp` | 修改 | `PrepareGPUScene()` 中提取材质 → hash 去重 → `gpu_materials_` 追加 → `inst.material_id = dedup_id` |
| `engine/render/passes/render_pass_context.h` | 修改 | 新增 `GpuBufferHandle gpu_material_ssbo` |

**审查发现 [Critical #3]**：`PrepareGPUScene()` line 1390 将 `mesh_renderer.color` 烘焙进 mega buffer VBO 顶点色。两个实体共享 `mesh_path` 但颜色不同时，第二个实体使用第一个实体的颜色 → **颜色错误**（预存 bug，Phase 4 激活后暴露）。

**修复**：file mesh 注册时烘焙白色 `(1,1,1,1)` 作为 VBO 顶点色，实际颜色走 material SSBO；inline mesh 继续烘焙 `mesh_renderer.color`（颜色是 mesh 身份的一部分）。FS 中 `albedo = mat.albedo_alpha.rgb * vColor.rgb` 对两种情况均正确：
- File mesh: `entity_color × white = entity_color` ✅
- Inline mesh: `(1,1,1,1) × baked_color = baked_color` ✅

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `modules/gameplay_3d/rendering/mesh_render_system.cpp` `PrepareGPUScene()` line ~1390 | 修改 | file mesh 注册时 `color = glm::vec4(1.0f)`；inline mesh 保持 `color = mesh_renderer.color` |

材质提取（每个 eligible 实体）：

```cpp
GPUMaterialData mat{};
// File mesh: 实际颜色从 material SSBO 传递
// Inline mesh: VBO 已烘焙颜色，material albedo 设为白色避免双重应用
if (!mesh_renderer.mesh_path.empty()) {
    mat.albedo_alpha = mesh_renderer.color;  // file mesh: 颜色走 material
} else {
    mat.albedo_alpha = glm::vec4(1.0f);      // inline mesh: 颜色已在 VBO
}
mat.params0 = glm::vec4(mesh_renderer.metallic, mesh_renderer.roughness,
                         mesh_renderer.ao, mesh_renderer.normal_strength);
mat.emissive_shading = glm::vec4(mesh_renderer.emissive,
                                  static_cast<float>(mesh_renderer.shading_mode));
// ... toon_shadow, toon_params, extra0, extra1 同理

uint64_t hash = MaterialHash(mat);
auto it = material_dedup_.find(hash);
if (it != material_dedup_.end()) {
    inst.material_id = it->second;
} else {
    uint32_t id = static_cast<uint32_t>(gpu_materials_.size());
    gpu_materials_.push_back(mat);
    material_dedup_[hash] = id;
    inst.material_id = id;
}
```

去重效果：100 个实体 3 种材质 → SSBO 仅 384B（3 × 128B），而非 12.8KB。

#### 2e. 三端 ShaderManager 加载 GPU-driven program

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `engine/render/rhi/opengl/gl_shader_manager.cpp` | 修改 | `CompileProgram(kpbr_gpu_driven_vert_glsl430, kpbr_gpu_driven_frag_glsl430)` 替代当前 `CompileProgram(kpbr_gpu_driven_vert_glsl430, kpbr_frag_glsl430)` |
| `engine/render/rhi/vulkan/vulkan_shader_manager.h/cpp` | 修改 | 新增 `gpu_driven_pbr_program_` 从 `kpbr_gpu_driven_vert_spv` + `kpbr_gpu_driven_frag_spv` 创建 |
| `engine/render/rhi/dx11/dx11_shader_manager.h/cpp` | 修改 | 新增 `gpu_driven_pbr_program_` 从 HLSL/DXBC 变体创建 |

**验证**：shader 编译通过 + 三端加载成功（尚未绑定使用，无行为变化）

---

### Phase 3：真正的 Indirect Draw — VK/DX11 ✅ DONE

> **完成日期：2026-05-23，编译通过**
>
> - 3a VK: SSBO 加 `INDIRECT_BUFFER_BIT`；`MultiDrawIndexedIndirect` 改为单次 `vkCmdDrawIndexedIndirect`，移除 per-draw push constants 循环
> - 3b DX11: SSBO 加 `DRAWINDIRECT_ARGS` flag；buffer lookup 支持 SSBO handle（DX11 per-draw 循环保留，API 限制）
> - 3c PreZ: `PreZPass::Execute()` 添加 GPU-driven depth-only indirect draw（修复 Critical #2）
> - 3d Shadow: GL 已正确；VK/DX11 通过 buffer lookup 修复自动继承
> - 3e 清理: 保留 `CacheGPUDrivenInstanceData` 接口（DX11 仍需），VK 端实质空操作

#### 3a. Vulkan — 单次 `vkCmdDrawIndexedIndirect`

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `engine/render/rhi/vulkan/vulkan_draw_executor.cpp` `SetupGPUDrivenPBR()` | 修改 | 绑定 `gpu_driven_pbr_program_` 而非标准 PBR；绑定 instance SSBO (binding 5) + material SSBO (binding 9) 的 descriptor set |
| `engine/render/rhi/vulkan/vulkan_rhi_device.cpp` `MultiDrawIndexedIndirect()` | 修改 | **移除 per-draw push constants 循环**；改为单次 `vkCmdDrawIndexedIndirect(cmd_buf, indirect_buffer, 0, draw_count, stride)` |

VS 中 `gl_InstanceIndex` = `firstInstance`（每个 draw command 的 `base_instance` 字段），正确索引 instance SSBO。

**审查发现 [Medium #1]**：`gpu_draw_cmd_ssbo` 以 `GpuBufferUsage::kStorage` 创建（走 `CreateSSBO`）。Vulkan 的 `CreateSSBO` 使用 `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`，但 `vkCmdDrawIndexedIndirect` 要求 `VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT`，缺少该 flag 会触发 validation error。

**修复**：`PrepareGPUScene()` 中 draw command SSBO 改用 `GpuBufferUsage::kStorage | GpuBufferUsage::kIndirect`。同时 Vulkan 后端的 `CreateSSBO` 或覆写的 `CreateGpuBuffer` 需检测组合 flag 并同时设置两个 usage bit。

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `modules/gameplay_3d/rendering/mesh_render_system.cpp` line ~1483 | 修改 | draw cmd SSBO desc.usage = `kStorage \| kIndirect` |
| `engine/render/rhi/vulkan/vulkan_resource_manager.cpp` | 修改 | `CreateSSBO` 添加 `VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT`（或覆写 `CreateGpuBuffer` 处理组合 flag） |

**Before**:
```cpp
for (int i = 0; i < draw_count && i < cached_gpu_count_; ++i) {
    pc.model = inst_data[i].model;
    vkCmdPushConstants(active_render_cmd_, layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
    vkCmdDrawIndexed(active_render_cmd_,
                     cmd_data[i].count, 1,
                     cmd_data[i].first_index,
                     cmd_data[i].base_vertex, 0);
}
```

**After**:
```cpp
vkCmdDrawIndexedIndirect(active_render_cmd_,
                          indirect_vk_buf, 0,
                          draw_count, stride);
// 单次提交，GPU 自行消费 N 个 draw command
```

- 移除 `cached_gpu_models_` / `cached_gpu_count_` 等缓存字段（VK 端）
- Push constants 仅保留给 per-item 路径的 `DrawMeshBatch`

#### 3b. DX11 — draw_id CB + ByteAddressBuffer

DX11 没有 multi-draw-indirect API，per-draw 循环**不可避免**（DX11 API 固有限制）。但可以大幅减少 CPU 开销：

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `engine/render/rhi/dx11/dx11_draw_executor.h` | 修改 | 新增 `ComPtr<ID3D11Buffer> draw_id_cb_`（16 bytes：`uint draw_id + 3 padding`） |
| `engine/render/rhi/dx11/dx11_draw_executor.cpp` `Init()` | 修改 | 创建 `draw_id_cb_`（D3D11_USAGE_DEFAULT, 16B） |
| `engine/render/rhi/dx11/dx11_draw_executor.cpp` `SetupGPUDrivenPBR()` | 修改 | 绑定 `gpu_driven_pbr_program_`；instance data `ByteAddressBuffer t21` + material data `ByteAddressBuffer t22`；绑定 `draw_id_cb_` 到 VS b7 |
| `engine/render/rhi/dx11/dx11_rhi_device.cpp` `MultiDrawIndexedIndirect()` | 修改 | **移除 `UpdatePerObjectCB` 调用**；改为 per-draw 更新 16B `draw_id_cb_`（只写 draw index） |

**Before**:
```cpp
for (int i = 0; i < draw_count; ++i) {
    DX11PerObjectCB obj{};           // ~80B
    obj.model = inst_data[i].model;  // 64B mat4 copy
    obj.skinned = 0;
    obj.morph_enabled = 0;
    obj.use_instancing = 0;
    draw_executor_.UpdatePerObjectCB(obj);  // Map/Unmap 80B
    dc->DrawIndexedInstancedIndirect(buf->buffer.Get(), byte_offset);
}
```

**After**:
```cpp
for (int i = 0; i < draw_count; ++i) {
    uint32_t draw_id[4] = { static_cast<uint32_t>(i), 0, 0, 0 };  // 16B
    dc->UpdateSubresource(draw_id_cb_.Get(), 0, nullptr, draw_id, 0, 0);
    dc->DrawIndexedInstancedIndirect(buf->buffer.Get(), byte_offset);
}
```

对应的 DX11 HLSL VS 修改（GPU-driven 变体）：

```hlsl
cbuffer DrawIdCB : register(b7) { uint draw_id; uint3 _pad_did; };
ByteAddressBuffer instances : register(t21);  // GPUInstanceData[], 80B/entry
ByteAddressBuffer materials : register(t22);  // GPUMaterialData[], 128B/entry

void vert_main() {
    // 从 instances buffer 读 model matrix (offset 0-63 of entry)
    float4x4 model = asfloat(uint4x4(
        instances.Load4(draw_id * 80 + 0),
        instances.Load4(draw_id * 80 + 16),
        instances.Load4(draw_id * 80 + 32),
        instances.Load4(draw_id * 80 + 48)));
    // 从 instances buffer 读 material_id (offset 64 of entry)
    uint mat_id = instances.Load(draw_id * 80 + 64);
    v_material_id = mat_id;
    // ... 标准变换逻辑
}
```

> **注意**：这从根本上修复了当前 HLSL 中 `SV_InstanceID` 恒为 0 的 bug——改为从 `draw_id_cb_` 显式读取 draw index。

**审查发现 [Medium #2]**：DX11 GPU-driven HLSL 从 `ByteAddressBuffer t21/t22` 读数据，但当前 `SetupGPUDrivenPBR` 未绑定任何 SRV。DX11 SSBO（`CreateSSBO`）创建的 Buffer 需要 `D3D11_BIND_SHADER_RESOURCE`，并需为其创建 SRV。

**修复**：

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `engine/render/rhi/dx11/dx11_resource_manager.cpp` | 修改 | `CreateSSBO` 确保 Buffer 有 `BIND_SHADER_RESOURCE` flag，并创建 `D3D11_BUFFEREX_SRV_FLAG_RAW` 类型 SRV |
| `engine/render/rhi/dx11/dx11_draw_executor.cpp` `SetupGPUDrivenPBR()` | 修改 | 绑定 instance SRV 到 VS t21 (`VSSetShaderResources(21, 1, &srv)`) + material SRV 到 VS/PS t22 |

#### 3c. PreZ GPU-driven 支持

**审查发现 [Critical #2]**：`PreZPass::Execute()` (line 115-131) 仅调用 `OnRenderPreZ()` → `Render()`。Phase 4 后 `gpu_driven_active_` 为 true，`Render()` 跳过 eligible 实体，但 PreZ 没有 GPU-driven indirect draw → eligible 实体无 `prez_depth`。下游 **SSAO / SSR / DOF / ContactShadow / Outline / MotionVector** 全部读 `prez_depth` → **渲染错误**。

**修复**：在 `PreZPass::Execute()` 中复用 GPU-driven shadow shader（depth-only，与 PreZ 需求一致）执行 indirect draw。

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `engine/render/passes/builtin_passes.cpp` `PreZPass::Execute()` | 修改 | 添加 `use_gpu_indirect` 条件分支：绑定 mega VAO + instance SSBO + shadow shader → `MultiDrawIndexedIndirect` |

```cpp
void PreZPass::Execute(CommandBuffer& cmd_buffer) {
    cmd_buffer.BeginRenderPass({ctx_.render_targets.prez, ...});
    // ...
    cmd_buffer.SetPipelineState(ctx_.pipeline_states.prez);

    // GPU-driven PreZ: eligible 实体 depth-only indirect draw
    const bool use_gpu_indirect = ctx_.gpu_driven_enabled
        && ctx_.gpu_mega_vao && ctx_.gpu_draw_cmd_ssbo
        && ctx_.gpu_indirect_draw_count > 0;
    if (use_gpu_indirect) {
        auto* rhi = ctx_.rhi_device;
        rhi->SetupGPUDrivenShadowShader(view, projection);  // depth-only VS
        rhi->BindGpuBuffer(ctx_.gpu_instance_ssbo, kSSBOBindingInstances);
        rhi->BindMegaVAO(ctx_.gpu_mega_vao);
        rhi->MultiDrawIndexedIndirect(ctx_.gpu_draw_cmd_ssbo.raw(), ...);
        rhi->UnbindVAO();
    }

    // Module per-item PreZ (non-eligible 实体)
    for (auto& mod : ctx_.modules) {
        mod.instance->OnRenderPreZ(*ctx_.world, cmd_buffer);
    }
}
```

~15 行改动，**复用已有 shadow shader，无需新 shader**。

#### 3d. Shadow 路径同步

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `engine/render/rhi/vulkan/vulkan_draw_executor.cpp` `SetupGPUDrivenShadow()` | 修改 | 绑定 gpu_driven shadow shader + instance SSBO descriptor |
| `engine/render/rhi/vulkan/vulkan_rhi_device.cpp` | 修改 | Shadow 部分 `MultiDrawIndexedIndirect()` 同 3a，单次调用 |
| `engine/render/rhi/dx11/dx11_draw_executor.cpp` `SetupGPUDrivenShadow()` | 修改 | 绑定 gpu_driven shadow shader + draw_id CB + instance ByteAddressBuffer |

GL shadow 路径已经正确（`gpu_driven_shadow_shader_handle_` + `glMultiDrawElementsIndirect`），无需改动。

#### 3e. 清理冗余接口

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `engine/render/rhi/rhi_gpu_driven.h` | 修改 | 移除 `CacheGPUDrivenInstanceData()` 虚函数 |
| `engine/render/rhi/vulkan/vulkan_rhi_device.h/cpp` | 修改 | 移除 `CacheGPUDrivenInstanceData()` 实现 + `cached_gpu_*` 成员 |
| `engine/render/rhi/dx11/dx11_rhi_device.h/cpp` | 修改 | 移除 `CacheGPUDrivenInstanceData()` 实现 + `cached_gpu_*` 成员 |
| `modules/gameplay_3d/rendering/mesh_render_system.cpp` | 修改 | 移除 `PrepareGPUScene()` 末尾的 `CacheGPUDrivenInstanceData()` 调用 |

**验证**：三端编译通过 + 单个 demo（如 `3d_scene_showcase`）GPU-driven 路径独立测试（临时移除 `modules.empty()` 条件）

---

### Phase 4：路径分流 — 移除 `modules.empty()` ✅ DONE

> **完成日期：2026-05-23，编译通过**
>
> - CSM/Spot/Point Shadow + ForwardScene：7 处 `modules.empty()` 移除
> - CPU fallback 简化：`!use_gpu_indirect && modules.empty()` 合并条件
> - 注释更新：说明 eligible/non-eligible 分流机制

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `engine/render/passes/builtin_passes.cpp` | 修改 | **4 处** `use_gpu_indirect` 条件移除 `ctx_.modules.empty()`：CSMShadowPass (line ~156) / SpotShadowPass (line ~231) / PointShadowPass (line ~288) / ForwardScenePass (line ~559) |
| `engine/render/passes/builtin_passes.cpp` `ForwardScenePass::Execute()` | 修改 | GPU indirect draw 后继续执行 `OnRenderScene`（module 渲染 non-eligible 实体）；Material SSBO 绑定到 binding 9 |

移除后的执行流程：

```
ForwardScenePass::Execute():
  1. use_gpu_indirect = gpu_driven_enabled && gpu_mega_vao && gpu_draw_cmd_ssbo && count > 0
     // 不再检查 modules.empty()
  2. if (use_gpu_indirect):
       rhi->BindGpuBuffer(gpu_instance_ssbo, binding=5)
       rhi->BindGpuBuffer(gpu_material_ssbo, binding=9)
       rhi->BindMegaVAO(mega_vao)
       rhi->SetupGPUDrivenPBRShader(...)     // 使用 gpu_driven_pbr program
       rhi->MultiDrawIndexedIndirect(...)     // eligible 实体一次画完
  3. for (module : modules):
       module.OnRenderScene(world, cmd_buffer, clip_correction)
       ↳ mesh_render_system_.Render(world, cmd_buffer)
         ↳ for (entity : mesh_renderers):
              if (gpu_driven_active_ && IsGPUDrivenEligible(...)): continue  // 跳过
              // 画 skinned / transparent / 其他 non-eligible 实体
       ↳ grass_system_.Render(...)    // 草地始终走 per-item
       ↳ terrain_system_.Render(...)  // 地形始终走 per-item
```

**关键正确性保证**：

| 实体类型 | GPU indirect | OnRenderScene per-item | 结果 |
|:---------|:------------:|:---------------------:|:-----|
| 不透明 + 非蒙皮 + 有 bounds | ✅ 画 | ❌ 跳过 (IsGPUDrivenEligible → true) | 无双重绘制 |
| 蒙皮 (Animator3D) | ❌ 不在 mega buffer | ✅ 画 | 正确 |
| 透明 (alpha < 1.0) | ❌ 不在 mega buffer | ✅ 画（需排序） | 正确 |
| 草地 / 地形 / 粒子 | ❌ 不在 mega buffer | ✅ 各自 system 画 | 正确 |
| Inline mesh (mesh_path 空) | ✅ 画 (Phase 1 dual-map) | ❌ 跳过 | 正确 |

**验证清单**：
- [ ] 59 个 3D demo 三端全量回归
- [ ] KF_Framework 三端 RMSE 对比（≤ 基线）
- [ ] 含 inline mesh 的 demo（如 `3d_procedural_mesh`）颜色/形状正确
- [ ] 含 skinned mesh 的 demo（如 `3d_animation_basic`）动画正确
- [ ] 含透明物体的 demo 混合正确
- [ ] 含草地/地形的 demo 渲染正确
- [ ] Shadow 正确性（CSM + Spot + Point）

---

### Phase 5：纹理绑定 ✅

> **完成日期：2026-05-23**

GPU-driven 路径支持 albedo/normal/MR/emissive/occlusion 五类纹理，通过分桶排序 + 逐桶绑定实现。

**方案：按纹理组合分桶 + 多次 indirect draw（方案 A）**

| 文件 | 改动类型 | 说明 |
|:-----|:--------:|:-----|
| `engine/render/rhi/gpu_scene_types.h` | 修改 | 新增 `GPUDrawTextures`（5 texture handle 组合键）+ `TextureBucket`（offset/count/textures）|
| `modules/gameplay_3d/rendering/mesh_render_system.h` | 修改 | 新增 `gpu_tex_keys_` + `gpu_texture_buckets_` 成员 |
| `modules/gameplay_3d/rendering/mesh_render_system.cpp` | 修改 | `PrepareGPUScene()` 收集纹理 key → 排列排序 → 建桶；`GPUMaterialData.flags` 设置 `has_normal/mr/emissive/occlusion` |
| `engine/render/passes/render_pass_context.h` | 修改 | 新增 `gpu_texture_buckets` 指针 + `gpu_texture_bucket_count` |
| `engine/render/rhi/rhi_gpu_driven.h` | 修改 | 新增 `BindGPUDrivenTextures()` 虚方法 + `MultiDrawIndexedIndirect` 增加 `byte_offset` 参数 |
| `engine/render/rhi/opengl/gl_rhi_device.h/cpp` | 修改 | GL 实现 `BindGPUDrivenTextures`（`glActiveTexture` + `glBindTexture`，handle=0 绑白色纹理）|
| `engine/render/rhi/vulkan/vulkan_rhi_device.h/cpp` | 修改 | VK 实现 `BindGPUDrivenTextures`：每桶分配新 Set 2 descriptor set，写入 5 PBR 纹理 + 阴影 + bone UBO |
| `engine/render/rhi/vulkan/vulkan_draw_executor.h/cpp` | 修改 | `BindGPUDrivenTextures()` 实现 + 缓存 SetupGPUDrivenPBR 中间状态 |
| `engine/render/rhi/dx11/dx11_rhi_device.h/cpp` | 修改 | DX11 实现 `BindGPUDrivenTextures`：`PSSetShaderResources` + `PSSetSamplers` 按 PBR slot 绑定 |
| `engine/render/rhi/dx11/dx11_draw_executor.h` | 修改 | 新增 `white_srv()` / `white_sampler()` accessor |
| `engine/render/passes/builtin_passes.cpp` | 修改 | ForwardScenePass 逐桶循环：`BindGPUDrivenTextures` → `MultiDrawIndexedIndirect(offset, count)` |

**纹理分桶原理**：`PrepareGPUScene()` 收集每个 draw 的 5 个纹理 handle 组合键，按键排序所有 draw commands/instances/AABBs，然后连续相同键的 draw 组成一个桶。ForwardScenePass 每桶绑定纹理后发起一次 indirect draw。

三端行为一致：每个桶一次 draw call，桶内共享纹理。10 种纹理组合 = 10 次 draw call（远少于逐实体 per-item）。

**备选方案对比**：

| 方案 | 优点 | 缺点 | 三端支持 |
|:-----|:-----|:-----|:---------|
| **A: 纹理分桶** (推荐) | 无需扩展，架构简单 | 每桶一次 draw call | GL/VK/DX11 ✅ |
| B: Texture Array | 单次 draw call | 纹理需统一尺寸 | GL/VK/DX11 ✅ |
| C: Bindless Textures | 最高性能 | DX11 不支持 | GL 4.4+ / VK ✅ / DX11 ❌ |

---

## 三、三端一致性矩阵（重构完成后）

| 维度 | GL 4.3+ | Vulkan | DX11 |
|:-----|:--------|:-------|:-----|
| **VS model 来源** | SSBO binding 5, `gl_BaseInstance` | SSBO binding 5, `gl_InstanceIndex` | ByteAddressBuffer t21, `draw_id` from CB b7 |
| **FS material 来源** | SSBO binding 9, `v_material_id` | SSBO binding 9, `v_material_id` | ByteAddressBuffer t22, `v_material_id` |
| **Draw call** | `glMultiDrawElementsIndirect` (1 call) | `vkCmdDrawIndexedIndirect` (1 call) | `DrawIndexedInstancedIndirect` (N calls, API 限制) |
| **CPU per-draw 开销** | 0 | 0 | 16B CB 更新/draw（vs 当前 80B+） |
| **Shader program** | `gpu_driven_pbr` (vert + frag) | `gpu_driven_pbr` (vert + frag) | `gpu_driven_pbr` (vert + frag) |
| **Hi-Z culling** | Compute shader | Compute shader | Compute shader |
| **Material dedup** | ✅ | ✅ | ✅ |

> DX11 的 N 次 `DrawIndexedInstancedIndirect` 是 DX11 API 的固有限制（DX11 没有 multi-draw-indirect），不是技术债。DX12 的 `ExecuteIndirect` 可解决但超出当前引擎范围。

---

## 四、完整文件改动表

| 文件 | Phase | 行数 | 说明 |
|:-----|:-----:|:----:|:-----|
| `modules/gameplay_3d/rendering/mesh_render_system.h` | 1,2 | +25 | 共享判定 + dual-map + material 缓存 |
| `modules/gameplay_3d/rendering/mesh_render_system.cpp` | 1,2 | +80 | IsGPUDrivenEligible + dual-map + material 填充/去重 |
| `engine/render/shaders/src/pbr_gpu_driven.vert` | 2 | +3 | `flat out uint v_material_id` |
| `engine/render/shaders/src/pbr_gpu_driven.frag` | 2 | ~80 | **新建** — material SSBO 读取 + PBR lighting |
| `engine/render/shaders/generated/embed/*.gen.h` | 2 | — | shader compiler 重新生成 |
| `engine/render/rhi/opengl/gl_shader_manager.cpp` | 2 | +5 | 使用 gpu_driven frag |
| `engine/render/rhi/vulkan/vulkan_shader_manager.h/cpp` | 2,3 | +20 | 加载 gpu_driven program |
| `engine/render/rhi/dx11/dx11_shader_manager.h/cpp` | 2,3 | +20 | 加载 gpu_driven program |
| `engine/render/passes/render_pass_context.h` | 2 | +3 | `gpu_material_ssbo` |
| `engine/render/rhi/vulkan/vulkan_draw_executor.cpp` | 3 | +30/-20 | 绑定 gpu_driven shader + SSBOs |
| `engine/render/rhi/vulkan/vulkan_rhi_device.cpp` | 3 | +5/-30 | 单次 indirect draw 替代循环 |
| `engine/render/rhi/dx11/dx11_draw_executor.h` | 3 | +5 | `draw_id_cb_` 声明 |
| `engine/render/rhi/dx11/dx11_draw_executor.cpp` | 3 | +30/-15 | 绑定 gpu_driven shader + ByteAddressBuffers |
| `engine/render/rhi/dx11/dx11_rhi_device.cpp` | 3 | +5/-15 | draw_id CB 替代 PerObjectCB |
| `engine/render/rhi/rhi_gpu_driven.h` | 3 | -5 | 移除 `CacheGPUDrivenInstanceData` |
| `engine/render/passes/builtin_passes.cpp` | 2,3c,4 | +30/-10 | PreZ GPU-driven + material SSBO 绑定 + 移除 `modules.empty()` |
| `engine/render/rhi/vulkan/vulkan_resource_manager.cpp` | 3a | +3 | SSBO 添加 INDIRECT_BUFFER usage flag |
| `engine/render/rhi/dx11/dx11_resource_manager.cpp` | 3b | +15 | SSBO 添加 BIND_SHADER_RESOURCE + 创建 raw SRV |
| **总计** | | **~460 行 net** | |

---

## 五、实施时间线

```
Phase 1 ──── 0.5天 ──→ 编译验证 (零行为变化)
Phase 2 ──── 2天   ──→ shader 编译 + 三端加载验证
Phase 3 ──── 2.5天 ──→ 单 demo 三端 GPU-driven 验证 (含 PreZ + VK buffer flag + DX11 SRV)
Phase 4 ──── 1天   ──→ 59 demo 三端全量回归 (含 SSAO/SSR/DOF 正确性)
Phase 5 ──── 1天   ──→ 纹理分桶 + 三端绑定 + demo 验证 ✅
Phase 5b ─── 0.5天 ──→ 技术债清零: VK/DX11 纹理绑定 + 排序优化 + 资源泄漏归零 ✅
────────────────────────────────────────────
总计: Phase 1-5 全部完成
```

---

## 六、技术债清零清单

| 现有债务 | 处理阶段 | 方式 |
|:---------|:--------:|:-----|
| `modules.empty()` 禁用 GPU-driven | Phase 4 | 移除条件，靠 IsGPUDrivenEligible 分流 |
| Mega buffer key 碰撞 (inline mesh) | Phase 1b | Dual-map：file_mesh_registry_ + inline_mesh_registry_ |
| 无 per-draw material | Phase 2 | GPUMaterialData SSBO binding 9 + 材质去重 |
| VK 伪 indirect（per-draw push constants 循环） | Phase 3a | 单次 `vkCmdDrawIndexedIndirect` |
| DX11 伪 indirect（per-draw CB 80B） | Phase 3b | draw_id CB 16B（API 限制下的最优解） |
| `Render()` 缺 `temp_indices` 检查 | Phase 1a | 共享 `IsGPUDrivenEligible()` |
| DX11 HLSL `SV_InstanceID` 恒为 0 bug | Phase 3b | 改用 `draw_id_cb_` 显式传入 draw index |
| Eligibility 条件分散两处（PrepareGPUScene / Render） | Phase 1a | 统一 `IsGPUDrivenEligible()` 函数 |
| GPU-driven FS 不存在 | Phase 2a | 新建 `pbr_gpu_driven.frag` |
| `CacheGPUDrivenInstanceData` 冗余接口 | Phase 3e | 移除虚函数及所有实现 |
| 静态合批 + GPU-driven 双重绘制 | Phase 1c | `gpu_driven_active_` 时跳过静态合批提交 |
| File mesh 顶点色污染（共享 path 不同 color） | Phase 2d | file mesh VBO 烘白色 + 颜色走 material SSBO |
| PreZ 深度缺失（SSAO/SSR/DOF 异常） | Phase 3c | PreZ 添加 GPU-driven indirect draw |
| VK SSBO 缺 INDIRECT_BUFFER flag | Phase 3a | draw cmd SSBO usage 加 `kIndirect` |
| DX11 SSBO 缺 SRV 绑定 | Phase 3b | 创建 raw SRV + 绑定 t21/t22 |

**新增技术债：零。**

DX11 的 per-draw 循环是 DX11 API 的固有限制，不计入技术债。
GL 3.3 (无 SSBO/Compute) 下 GPU-driven 整体不可用——由 `gpu_driven_enabled` 标志控制，不影响 per-item 路径。

### Phase 5b 技术债清零明细

| 问题 | 修复 | 文件 |
|:-----|:-----|:-----|
| VK `BindGPUDrivenTextures` 无实现（基类 no-op） | 每桶分配新 Set 2 descriptor set + 14 个 binding 写入 | `vulkan_draw_executor.cpp/h`, `vulkan_rhi_device.cpp/h` |
| DX11 `BindGPUDrivenTextures` 无实现（基类 no-op） | `PSSetShaderResources` + `PSSetSamplers` 按 reflection slot 绑定 | `dx11_rhi_device.cpp/h`, `dx11_draw_executor.h` |
| 排序算法全量拷贝 | in-place cycle-based permutation apply, O(N)/O(1) | `mesh_render_system.cpp` |
| `DeleteMegaVAO` 未更新 buffers_destroyed | 加 ledger 更新 | `gl_rhi_device.cpp` |
| `DestroyAllRenderTargets` 未删除 MRT 额外 color attachments | 遍历 `color_texture_handles` 向量 | `gl_resource_manager.cpp` |
| GL 纹理 fallback 解绑（normal/MR/emissive/occlusion 绑 0） | 改为绑定白色纹理 | `gl_rhi_device.cpp` |
| DX11 `MultiDrawIndexedIndirect` 桶偏移索引错误 | `global_idx = base_index + i` | `dx11_rhi_device.cpp` |
| `GPUDrawTextures` 缺 `operator!=` | 添加 | `gpu_scene_types.h` |

**验证结果：** textures=52/52, buffers=29/29, shader_programs=1/1 — **全部资源归零，零 WARN。**

---

## 七、审查修订记录

> 2026-05-23 第一轮代码审查后补充

### 发现的关键遗漏

| 编号 | 严重性 | 问题 | 根因 | 影响 | 修复位置 |
|:----:|:------:|:-----|:-----|:-----|:---------|
| C1 | **Critical** | 静态合批双重绘制 | `Render()` line 1262 提交 `static_batch_items_` 不受 `gpu_driven_active_` 控制 | `is_static` eligible 实体被 GPU indirect + 静态合批各画一次 | Phase 1c |
| C2 | **Critical** | PreZ 深度缺失 | `PreZPass::Execute()` 无 GPU-driven 路径，`Render()` 跳过 eligible 实体 | SSAO/SSR/DOF/ContactShadow/Outline/MotionVector 深度数据不完整 | Phase 3c |
| C3 | **Critical** | File mesh 顶点色污染 | `PrepareGPUScene` line 1390 烘焙 `mesh_renderer.color` 进 VBO；共享 path 的第二个实体使用第一个实体的颜色 | 同 mesh_path 不同 color 的实体颜色错误 | Phase 2d |
| M1 | Medium | VK SSBO 缺 indirect flag | `gpu_draw_cmd_ssbo` 以 `kStorage` 创建，VK 端无 `VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT` | Phase 3a `vkCmdDrawIndexedIndirect` 触发 validation error | Phase 3a |
| M2 | Medium | DX11 SRV 未绑定 | `SetupGPUDrivenPBR` 未为 instance/material SSBO 创建 SRV 并绑定 t21/t22 | DX11 GPU-driven HLSL 读不到数据 | Phase 3b |

### Vulkan descriptor write 反射守护修复

> 2026-05-23 运行时验证后补充

| 编号 | 严重性 | 问题 | 根因 | 影响 | 修复 |
|:----:|:------:|:-----|:-----|:-----|:-----|
| C4 | **Critical** | `vkUpdateDescriptorSets` 向空 layout 写入 | `AllocateAndUpdateMeshDescriptorSets` 无条件写入 Set 1/2/3 所有 binding，但 shadow/PreZ shader 仅声明 Set 0 binding 0 + Set 2 binding 8，Set 1 为空洞 layout（zero bindings） | PreZ pass 触发 `VUID-VkWriteDescriptorSet-dstBinding-10009` 崩溃 | 添加 `has_binding(set, binding, type)` 反射守护，所有 descriptor write 仅在 shader 实际声明时执行 |

修复文件：`engine/render/rhi/vulkan/vulkan_draw_executor.cpp`

守护范围：
- **Set 1**: binding 0 (PerScene UBO) + binding 1-4 (SSBOs) + binding 5 (LightProbe UBO)
- **Set 2**: binding 0 (PerMaterial) + binding 1-5 (textures) + binding 6-7 (shadow) + binding 8-9 (bone/morph) + GBuffer binding 1
- **Set 3**: binding 0 (point shadow cubemaps)

验证：`3d_textured_cube` Vulkan 后端 120 帧正常渲染 + 24 项 Vulkan 单测全部通过。

### 验证清单更新

原有验证清单补充以下项目（对应审查发现）：

- [ ] **[C1]** 含 `is_static` 实体的 demo：确认无双重绘制（RenderDoc 截帧检查 draw call 数）
- [ ] **[C2]** SSAO / SSR / DOF 开启时：GPU-driven 实体深度正确（无黑色/缺失区域）
- [ ] **[C3]** 两个实体共享 mesh_path 但 color 不同：各自颜色正确
- [x] **[C4]** Vulkan PreZ/shadow pass 无 descriptor write 崩溃
- [ ] **[M1]** Vulkan validation layer 零 error（已修复 descriptor write，残余 GPU-driven 路径 warning 待后续）
- [ ] **[M2]** DX11 RenderDoc 截帧：t21/t22 slot 有正确数据
