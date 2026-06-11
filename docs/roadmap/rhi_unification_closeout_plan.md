# RHI 统一收尾 + Vulkan 后端转正计划

> 状态：执行中（feature/engine-lib）。本计划基于对三后端代码的逐行核对，不依赖历史文档。

## 背景与现状核实

`engine/render/RHI_UNIFICATION_TASKS.md` 中 7 个问题已完成 6 个，仅 #3（CameraSystem 投影未修正）待排查。
本次核实结论（见下文「任务 #3 排查结论」）：**#3 无需代码修改，可关闭**。

更大的"统一收尾"债务在 DrawExecutor 层：

- `draw_executor_common.h` 已抽出 `DrawExecutorGlobalState` 与 5 个 `Prepare*UBO` 共享辅助函数，
  但 **`PreparePerSceneUBO` / `PreparePerMaterialUBO` / `PreparePointLightsUBO` / `PrepareSpotLightsUBO` /
  `PrepareLightProbeUBO` 目前没有任何后端调用**（仅 `PrepareSpotLightDataUBO`、`UpdateSortBatchStats` 被使用）。
- 三后端各自手写了等价的 UBO 填充逻辑：
  - GL: `gl_draw_executor_mesh.cpp` DrawMeshBatch（PerScene/LightProbe/PointLights/SpotLights/PerMaterial）
  - DX11: `dx11_draw_executor.cpp` DrawMeshBatch（DX11PerSceneCB 等）
  - Vulkan: `vulkan_draw_executor.cpp` UpdatePerSceneUBO/UpdatePerMaterialUBO/UpdatePointSpotLightUBOs
- 共享辅助函数与实际后端行为存在**两处语义漂移**（这是必须先修的正确性问题）：
  1. 三后端均将 `cascade_splits.w = wboit_mode`，而 `PreparePerSceneUBO` 写死 `0.0f`；
  2. 三后端 PerMaterial 在 `shading_mode == 5`（watercolor）时把水彩参数写入
     `toon_shadow_color/toon_params`，而 `PreparePerMaterialUBO` 没有该分支。
- 各后端本地 UBO 结构（`VulkanPerSceneUBO`、`DX11PerSceneCB` 等）与 `ubo_types.h`
  共享结构逐字段同布局（std140），属于重复定义。

## 阶段 A：UBO 填充逻辑统一（本次实施）

1. **修正共享辅助函数**（`draw_executor_common.h`）
   - `PreparePerSceneUBO`：`cascade_splits.w = static_cast<float>(item.wboit_mode)`；
   - `PreparePerMaterialUBO`：补 watercolor（shading_mode==5）分支。
2. **GL 后端改用共享函数**：DrawMeshBatch 中 PerScene/LightProbe/PointLights/SpotLights
   填充替换为 `Prepare*` 调用（上传仍走 `UBOManager`）。
3. **DX11 后端改用共享函数**：本地 CB 结构布局与共享结构一致处直接以共享结构填充后
   `UpdateConstantBuffer` 上传；删除重复填充代码。
4. **Vulkan 后端改用共享函数**：`UpdatePerSceneUBO/UpdatePerMaterialUBO/UpdatePointSpotLightUBOs`
   内部改为 `Prepare*` + `WriteToBuffer`；`VulkanPerSceneUBO` 等本地结构以
   `using ... = PerSceneUBO;` 别名到 `ubo_types.h` 共享类型（布局已逐字段核对一致）。
5. **关闭 RHI_UNIFICATION_TASKS.md #3**（排查结论见下）。

验收标准：三后端 UBO 填充语义唯一来源为 `draw_executor_common.h`；CI 三矩阵
（Debug/Release/RelWithDebInfo）构建 + 单元/集成/冒烟测试全绿；GL 行为零退化。

## 阶段 B：Vulkan 后端转正（本次实施）

现状：Vulkan Headers/Loader/glslang/spirv-cross 均已源码内置（depends/），
`DSE_ENABLE_VULKAN=ON` 无需系统 Vulkan SDK 即可构建；Vulkan 已有独立冒烟测试
（`vulkan_rhi_smoke_test.cpp`），但 CMake 默认 OFF、CI 仅 RelWithDebInfo 矩阵覆盖。

1. **CMake 默认值转正**：桌面平台（非 Android）`DSE_ENABLE_VULKAN` 默认 ON；
   Android 维持显式 OFF（待真机验证后另行开启）。
2. **CI 全矩阵覆盖**：Debug/Release 矩阵同样开启 Vulkan（标签同步更新），
   使 Vulkan 编译与冒烟测试获得与 GL/DX11 同级的回归保障。
3. CI 的 Vulkan SDK 安装步骤维持按矩阵条件执行（运行时验证层需要）。

## 任务 #3 排查结论（CameraSystem 投影，无需修改）

`CameraComponent.projection`（gameplay_2d/camera_system.cpp 写入）的全部消费点：

| 消费点 | 是否已应用 clip_correction |
|---|---|
| `frame_pipeline.cpp` → `snap.camera_2d.projection` → `builtin_passes.cpp` UIPass | 是（`clip_correction_2d * snap.camera_2d.projection`） |
| 编辑器 `BuildActiveCameraMatrices` → ForwardScenePass 编辑器分支 | 是（任务 #4 已修复，`clip_correction * ctx_.editor_projection`） |
| `dse_render_world_to_screen`（Lua world_to_screen） | 不适用：自行构造 GL 约定投影并以 GL 约定映射屏幕坐标，结果跨后端一致 |

即所有消费路径要么在使用处补乘修正矩阵，要么与后端约定无关，组件内缓存值无需修正。

## 阶段 C：后续路线（不在本次范围）

1. 高层绘制流程抽取：`DrawSkybox` / `DrawPostProcess` / `DrawSpriteBatch` 的
   "排序-分批-状态切换"骨架抽到公共层，后端仅保留资源绑定与提交。
2. 以 `forwarding_command_buffer.h` 为基础统一三后端 CommandBuffer 接口为纯虚 `IDrawExecutor`，
   消除 RhiDevice 中按宏分发的样板。
3. `builtin_passes.cpp`（3.3k 行）按 Pass 拆分独立文件，兑现 RenderGraph 模块化。
4. Android Vulkan 转正（依赖真机验证闭环）。
