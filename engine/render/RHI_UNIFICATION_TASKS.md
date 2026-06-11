# RHI 三后端统一审查任务指令

## 背景
DSEngine 支持 OpenGL / Vulkan / DX11 三个渲染后端。已完成 `GetProjectionCorrection()` 修正投影矩阵（commit 49c221f），但仍有多个位置存在后端差异未统一。

## 已完成
- [x] 投影矩阵 NDC 修正 (`GetProjectionCorrection()`)
- [x] 纹理加载 Y-flip (`NeedsTextureYFlip()`)
- [x] Readback Y-flip (`NeedsReadbackYFlip()`)
- [x] Vulkan/DX11 阴影着色器 z 坐标不再 `*0.5+0.5`
- [x] #1 Vulkan 正面绕序修正 (CW)
- [x] #2 2D/UI 正交投影 clip_correction
- [x] #3 CameraSystem 2D fallback clip_correction
- [x] #3 CameraSystem 内部投影已排查：所有消费点均在使用处补乘修正或与后端无关，无需修改（详见 docs/roadmap/rhi_unification_closeout_plan.md）
- [x] #4 编辑器相机 clip_correction
- [x] #5 RT-UV 方向已验证无需修改
- [x] #6 Cubemap 面序已验证无需修改；天空盒 VP 平移 bug 已修复
- [x] #7 HLSL 矩阵布局已验证无需修改

---

## 问题 1: Vulkan 正面绕序 (Front Face Winding) ✅ 已修复

### 现状
- 投影修正矩阵包含 Y-flip → 三角形在屏幕空间的绕序从 CCW 变为 CW
- 但 Vulkan 管线创建时固定设为 `VK_FRONT_FACE_COUNTER_CLOCKWISE`
- 结果: 所有三角形会被 back-face cull 掉

### 修复
- `ToVkFrontFace()` 返回 `VK_FRONT_FACE_CLOCKWISE`
- L188 改为调用 `ToVkFrontFace()` 而非硬编码

---

## 问题 2: 2D/UI 精灵正交投影缺少修正 ✅ 已修复

### 修复方案: 通过回调链传递 clip_correction
- `render_pass_context.h` → `render_2d_ui` 回调增加 `const glm::mat4&` 参数
- `builtin_passes.cpp` UIPass::Execute → 传递 `GetProjectionCorrection()`
- `module.h` → `OnRenderUI` 增加默认参数 `clip_correction = identity`
- `frame_pipeline.cpp` → lambda 传递 clip_correction
- `gameplay_2d_module.h/cpp` → 传递到 UIRenderSystem
- `sprite_render_system.h/cpp` → `ortho = clip_correction * glm::ortho(...)`

---

## 问题 3: CameraSystem 内部投影未修正 ✅ 已排查无需修改

### 排查结论
`CameraComponent.projection` 的全部消费点：
- `frame_pipeline.cpp` 快照 → `builtin_passes.cpp` UIPass：使用处已乘 `clip_correction_2d`；
- 编辑器 `BuildActiveCameraMatrices` → ForwardScenePass 编辑器分支：任务 #4 已补乘修正；
- `dse_render_world_to_screen`（Lua world_to_screen）：自行构造 GL 约定投影并以 GL 约定映射屏幕坐标，跨后端一致。

组件内缓存值无需修正。以下为原始排查记录：

### 现状
`modules/gameplay_2d/camera/camera_system.cpp`:
- 第 57 行: `camera.projection = glm::perspective(...)`
- 第 96 行: `camera.projection = glm::ortho(...)`
- 第 114 行: `camera.projection = glm::perspective(...)`

这些存储到 `CameraComponent.projection` 中, 如果任何系统直接使用 `camera.projection` (而非重新计算), 就会缺失修正。

### 排查
- `grep_search` 搜索 `camera.projection` 的使用处
- 如果仅由 2D 系统/Lua 绑定使用 (如 `world_to_screen`), 则可能需要修正
- 如果仅作为缓存用于 builtin_passes 中已修正的路径, 则无需重复处理

### 修复
若需修正, CameraSystem 需要访问 RhiDevice:
- 通过 `World` 中存储的 RhiDevice 指针, 或
- 在 FramePipeline 初始化时注入

---

## 问题 4: 编辑器相机投影未修正 ✅ 已修复

### 修复
在 ForwardScenePass::Execute 编辑器分支中应用 clip_correction:
```cpp
const glm::mat4 clip_correction = ctx_.rhi_device->GetProjectionCorrection();
cmd_buffer.SetCamera(ctx_.editor_view, clip_correction * ctx_.editor_projection);
```

---

## 问题 5: Render-to-Texture UV 方向 ✅ 已验证无需修改

### 验证结果
各后端全屏四边形 UV 已独立处理，各自自洽：
- **OpenGL/Vulkan**: NDC(-1,-1)→UV(0,0)，与各自纹理原点约定匹配
- **DX11**: 已翻转 UV（NDC top → v=0），符合 DX11 纹理原点在左上的约定

---

## 问题 6: Cubemap 面序 (Face Order) ✅ 已验证 + 天空盒 VP bug 已修复

### 面数据验证结果
- `stbi_set_flip_vertically_on_load(false)` 用于 cubemap 加载（top-down）
- 三个后端 `CreateTextureCube` 均上传相同 top-down 面数据，面序一致
- 采样方向 `aPos`（顶点位置）跨后端一致，**面序无需修改**

### 发现的天空盒 VP bug
Vulkan/DX11 的 `DrawSkybox` 未去除 view 矩阵平移，而 OpenGL 正确使用了
`mat4(mat3(view))`。已在 CPU 端修复两个后端。

OpenGL cubemap 面序:
```
+X, -X, +Y, -Y, +Z, -Z
```
Vulkan/DX11 面序相同, 但 **Y 轴方向不同**:
- OpenGL: +Y 面朝上 (纹理原点在左下)
- Vulkan/DX11: +Y 面朝上 (纹理原点在左上)

### 排查
检查 `CreateTextureCube` 的实现, 确认 Vulkan/DX11 是否需要翻转 +Y/-Y 面的像素。

---

## 问题 7: HLSL 矩阵传递 (DX11 Row-Major vs Column-Major) ✅ 已验证无需修改

### 验证结果
- HLSL `cbuffer` 中 `float4x4` 无 `row_major` 标记 → 默认 **column-major**
- GLM column-major 数据通过 `memcpy` 直接拷入 → 布局一致
- `mul(matrix, vector)` = mat * vec → 正确

---

## 工作流程

1. 按问题编号从 1 开始逐个处理
2. 每个问题先做最小排查 (grep/read), 确认是否真的影响当前渲染
3. 修复后编译: `cmake --build build_vs2022 --target dse_standalone --config Release`
4. 运行验证 (OpenGL 不退化):
   ```
   .\bin\DSEngine_Game_release.exe --script=examples\KF_Framework\script\main.lua --rhi=opengl
   ```
5. 如果有 Vulkan 运行时, 同时验证:
   ```
   .\bin\DSEngine_Game_release.exe --script=examples\KF_Framework\script\main.lua --rhi=vulkan
   ```
6. 全部完成后提交:
   ```
   git add -A && git commit -m "RHI统一: [具体修复内容]" && git push origin master
   ```

## 优先级

| 优先级 | 问题 | 影响 |
|--------|------|------|
| P0 | #1 Vulkan 绕序 | Vulkan 完全黑屏 (所有三角形被 cull) |
| P0 | #2 2D/UI 正交投影 | Vulkan/DX11 UI 翻转 |
| P1 | #4 编辑器相机 | 编辑器模式下 Vulkan/DX11 渲染错误 |
| P1 | #5 RT-UV 方向 | Vulkan/DX11 后处理图像翻转 |
| P2 | #3 CameraSystem | 可能无实际影响 (需先排查) |
| P2 | #6 Cubemap | 天空盒可能 Y 反转 |
| P3 | #7 HLSL 矩阵 | 可能已正确 (需确认) |

## 约束
- 使用中文回复和提交信息
- OpenGL 后端行为绝对不能退化 (修正矩阵为 identity)
- 不删改现有注释
- 优先最小改动
