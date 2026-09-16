# HD-2D M1+M2 实现报告

日期：2026-09-16
基线 HEAD：`7c2f623db2add897542e39ce536e821dbdecadb0`（`feature/engine-lib`）  
实现范围：`docs/design/HD2D_M1M2_TASK.md` 的 M1 + M2，不包含 M3~M6。

> 历史说明：第 1 节及之后部分保留首轮 WSL/llvmpipe 降级记录。
> 2026-09-16 已在台式机 Windows/MSVC 上完成 OpenGL/Vulkan 真机复验，
> 并修复 procedural inline mesh 二次变换导致的前脸不写深度问题；最新结论见第 0 节。
> D3D11 真机运行受设备初始化失败阻塞，不能作为已验证项。

---

## 0. 2026-09-16 台式机真机复验（最新，覆盖旧 WSL 数据）

### 0.1 根因

`MeshRenderSystem::BuildRenderQueues` 对 procedural inline mesh（`mesh_path.empty()`、不可实例化）生成的是 world-space 顶点，但 `MeshRenderer::DrawShaded` 会再次乘 `item.model`。房子因此被二次平移/缩放，前脸没有进入正确深度区间；Sprite3D 在房子矩形区域的 depth test 仍然通过并覆盖房子。

修复：`modules/gameplay_3d/rendering/mesh_render_system.cpp` 在所有 world-space 顶点路径上把 `item.model` 置为 identity；后续 `RenderScene::ApplyCameraOffset` 只调 translation，`DrawShaded` 不再二次应用 `mesh_model`。

### 0.2 OpenGL 台式机真机像素证据

台式机：NVIDIA GeForce GT 1030，Windows MSVC RelWithDebInfo，OpenGL 4.5。
真实房子 mask：同相机只画地面+房子、不创建 Sprite3D临时场景（house-only vs ground-only）差分；1028x720，mask=251381 px，bbox=(42,413)-(985,686)。
角色红色阈值：`r > 110 && r > g+35 && r > b+35 && g > 20`。

| 截图 | 角色红像素总数 | mask 内 | mask 外 |
|---|---:|---:|---:|
| `m1_behind_fixed.png` | 8121 | 0 | 8121 |
| `m1_front_fixed.png` | 78150 | 54376 | 23774 |

结论：behind inside == 0，behind outside > 0；front inside > 0。GL 真机硬验收通过。

截图路径（仓库内）：`docs/design/hd2d_m1m2_shots/desktop/`
- `m1_behind_fixed.png`
- `m1_front_fixed.png`
- `m1_house_only_fixed.png`
- `m1_ground_only_fixed.png`
- `m2_perf_fixed.png`

台式机原始路径：`C:\ProgramData\m1_behind_fixed2.png`、`C:\ProgramData\m1_front_fixed2.png`。

### 0.3 Vulkan 台式机真机 smoke/pixel

- `DSE_RHI_BACKEND=vulkan`，M1 behind：run_exit=0；用同一 OpenGL house mask 统计：red total=7584，inside=0，outside=7584。
- front 对照：red total=74146，inside=52939，outside=21207。
- 结论：Vulkan 真机遮挡同样成立。

### 0.4 D3D11

- 台式机 D3D11 设备创建失败：`D3D11CreateDeviceAndSwapChain failed (incl. WARP fallback)`，随后 `FramePipeline init failed` / segfault。失败发生在 RHI 初始化阶段，与 Sprite3D pass 无关。
- 三后端公共代码已同步并编译通过；D3D11 运行时应作为环境阻塞项，不能声称已验证。

### 0.5 M2 性能与排序

- `_sprite3d_perf_test.lua` GL 真机：1000 个 Sprite3D + 10 个盒子，`dse.metrics.get_draw_calls()=6 < 100`。
- `dse.metrics.get_sprite_count()=0`：当前 metrics 只统计 2D sprite 路径，未统计 Sprite3D；报告中的精灵数来自 Lua 脚本自身计数。
- `Sprite3DPass` 排序键为 `(depth_bucket, texture, blend)`，bucket 已改为 camera-relative/view-space depth（渲染线程用 `frame.view - camera_offset` 计算），不再使用绝对 world Z。
- `sorting_bias` 仍只影响排序 key，不是真实深度偏移。物理上更远的前景即使 `sorting_bias < 0`，只要 depth test/write 开启，仍会被更近的角色 depth-reject；屋檐/树冠盖住角色需要独立前景 pass 或真实深度偏移（shader depth bias / 分层 pass）。这是 M2 未完成项，不应伪装为已支持。

### 0.6 回归

- `_compat_test.lua`：`[compat] OK: P1 bool/number + P2 精灵 API + P3 tilemap_ex + P4 中文字表`，exit=0。
- `platformer_2d`（在模板目录运行以解析模板 assets）：`[platformer] ready -- 3 levels, 8 pickups total`，截图写出，exit=0。
- `topdown_3d`：`[topdown_3d] Game initialized  full port from C# source`，exit=0。

## 1. 改动清单

### M1：精灵进 3D

- `engine/ecs/components_3d_render.h`
  - 新增 `Sprite3DComponent`，字段与任务书 2.1 对齐。
- `tools/codegen/binding_defs.json`
  - 将 `Sprite3DComponent` 加入 `reflect_only_components`，驱动反射和场景 JSON codec。
- `engine/reflect/component_reflection.gen.cpp`（codegen 产物）
  - 新增 `RegisterSprite3D()`。
- `engine/scene/scene_json_codec.gen.h`（codegen 产物）
  - 新增 `Serialize/Deserialize_Sprite3DComponent` 与 codec 表项。
- `engine/scene/scene_json_codec_custom.h`
  - 序列化 `TextureRef` 的原始 RHI 句柄（反射无法表达 `TextureRef`）。
- `engine/scene/scene_component_serialization.cpp`
  - 旧 `Scene::Serialize/Deserialize` 扩展钩子也接入 Sprite3D。
- `engine/render/shaders/src/sprite3d.vert`
  - Yaw/YawPitch/Screen/None 四种 billboard；Yaw 按任务书从 view 列向量取 `right`；
  - Screen 模式在裁剪空间做像素偏移；`size_w/size_h/anchor_y/z_offset/tint` 从顶点属性读取。
- `engine/render/shaders/src/sprite3d.frag`
  - `albedo * color_tint`；`alpha < 0.1` 时 `discard`，保证透明像素不写深度。
- `engine/render/sprite_batch_renderer.{h,cpp}`
  - 复用/扩展现有 `SpriteDrawItem`，新增 `DrawSprite3D()`；
  - 新增 Sprite3D 顶点格式、PSO（depth test/write on、cull off、alpha SrcAlpha/1-SrcAlpha）；
  - 游程合批按相邻 `(texture, blend)` 合并。
- `engine/render/passes/sprite3d_pass.{h,cpp}`
  - 新增 `Sprite3DPass`：主线程提取并排序，渲染线程消费；
  - 排序键为 `(depth_bucket, texture, blend)`，`depth_bucket = floor((world_z + sorting_bias) * 10.0)`。
- `engine/render/passes/builtin_passes.cpp`
  - `ForwardScenePass` 写入 `frame.camera_offset`，让 Sprite3D 与 CPU/GPU 场景共享 camera-relative 偏移。
- `modules/gameplay_2d/gameplay_2d_module.{h,cpp}`
  - Gameplay2D 持有 `Sprite3DPass`，并在原 2D sprite pass 之前调用；
  - 由于 `ForwardScenePass` 先绘制所有 3D opaque，再进入 2D 回调，因此顺序满足3D opaque 之后、原 sprite pass 之前。
- `engine/render/rhi/rhi_types.h`
  - `BuiltinProgram::Sprite3D = 18`；`SpriteDrawItem` 增加 Sprite3D 字段。
- `engine/render/frame_context.h`
  - `FrameContext` 增加 `camera_offset`。
- OpenGL / Vulkan / D3D11 shader manager 与 device：
  - 加载/绑定 Sprite3D 程序；DX11 额外生成 input layout；Vulkan 使用生成的 SPIR-V。
- `engine/scripting/lua/bindings/lua_binding_compat.cpp`
  - 新增手写兼容层 API：
    - `ecs.add_sprite3d(e, tex, w, h, {billboard, anchor, lit, receive_shadow, z_offset, sorting_bias})`
    - `ecs.set_sprite3d_uv_rect(e, u0, v0, u1, v1)`
    - `ecs.set_sprite3d_billboard(e, "yaw"|"yaw_pitch"|"screen"|"none")`
    - `ecs.set_sprite3d_sorting_bias(e, v)`
    - `ecs.set_sprite3d_emissive(e, r, g, b)`
    - 另增 `set_sprite3d_size/color_tint/opacity` 便于 demo。
- `CMakeLists.txt`
  - 显式追加 `sprite3d.vert/.frag`，避免旧构建目录因文件 GLOB 未重配而漏编。

### 2026-09-16 台式机真机深度修复

- `modules/gameplay_3d/rendering/mesh_render_system.cpp`
  - world-space procedural inline mesh 使用 identity `item.model`，避免 `DrawShaded` 二次应用 `mesh_model`，修复房子前脸不写正确深度。
- `engine/render/passes/sprite3d_pass.{h,cpp}`
  - M2 bucket 从绝对 world Z 改为 camera-relative/view-space depth；排序从 Extract 阶段移到 `Render()`，以便使用 `frame.view` 与 `camera_offset`。

### M2：排序与批处理

- `Sprite3DPass` 内部按 `(depth_bucket, texture, blend)` 排序：
  - `depth_bucket` 受 `sorting_bias` 影响，越小越靠前；
  - `sorting_bias < 0` 可作为前景层/屋檐/树冠遮挡角色的实现基础。
- 复用 `SpriteDrawItem`，不另起一套数据或渲染路径；
- `DrawSprite3D` 在排序结果上按相邻 `(texture, blend)` 合批；
- 三后端 PSO 状态使用同一 `PipelineStateDesc`：
  - `depth_test = true`
  - `depth_write = true`
  - `depth_func = Less`
  - `culling = false`
  - alpha 路径 `SrcAlpha / OneMinusSrcAlpha`
- 验收脚本：
  - `templates/hd2d_wuxia/scripts/_sprite3d_perf_test.lua`
  - 1000 个共享纹理 billboard + 10 个 3D 盒子；打印 `dse.metrics.get_draw_calls()` 和精灵数。

### 文档/证据

- 报告：本文件。
- M1 demo：`templates/hd2d_wuxia/scripts/_sprite3d_test.lua`
- M2 性能 demo：`templates/hd2d_wuxia/scripts/_sprite3d_perf_test.lua`
- 截图目录：`docs/design/hd2d_m1m2_shots/`
- 更新 `docs/design/HD2D_IMPLEMENTATION_PLAN.md`：M1/M2 标为 `[~]`（代码完成，GL 实测；VK/D3D11 待桌面机验证）。

---

## 2. 三后端同步矩阵

| 后端 | 组件/反射/序列化 | shader 源产物 | 程序加载 | PSO 状态 | 运行验证 |
|---|---|---|---|---|---|
| OpenGL | 已同步 | `sprite3d.vert/.frag`  GLSL/SPIR-V/HLSL 全目标编译通过 | `GLShaderManager::InitSprite3DShader()` | 统一 PSO |  WSL/llvmpipe 截图通过 |
| Vulkan | 已同步 | 同一源  SPIR-V 编译通过 | `VulkanShaderManager::InitSprite3DShader()` | 统一 PSO |  WSL/lavapipe 尝试运行，Vulkan 设备/表面初始化失败；无桌面真机验证 |
| D3D11 | 已同步 | 同一源  DXBC 编译通过 | `DX11ShaderManager::InitSprite3DShader()` + input layout | 统一 PSO |  代码同步，无 Windows/真机运行时验证 |
| WebGPU | 未改（默认关闭） | - | 返回 0 优雅跳过 | - | 不在本次范围 |

说明：三后端未发现需要分叉的 Sprite3D 特殊状态；差异仅在 shader 程序获取方式（GLSL/DXBC/SPIR-V）。Vulkan 的 WSL/lavapipe 运行尝试停在 `vkCreateShaderModule: Invalid device`（Vulkan 设备/表面初始化未成功），不能作为 Sprite3D Vulkan 运行时证据；D3D11 仍完全缺少 Windows/真机验证。两者都需要桌面机跑 smoke/pixel 用例确认 descriptor/input-layout/寄存器绑定。

---

## 3. 截图路径

- `docs/design/hd2d_m1m2_shots/m1_front.png`  角色在房子前面（对照）
- `docs/design/hd2d_m1m2_shots/m1_behind.png`  角色在房子后面（遮挡验收）
- `docs/design/hd2d_m1m2_shots/m2_perf.png`  1000 billboard + 盒子性能场景
- `docs/design/hd2d_m1m2_shots/platformer_2d_reg.png`  platformer_2d WSL/GL 回归
- `docs/design/hd2d_m1m2_shots/topdown_3d_reg.png`  topdown_3d WSL/GL 回归

原始降级机路径（WSL）：
- `/tmp/m1_front2.png`
- `/tmp/m1_behind2.png`
- `/tmp/m2_perf.png`

Windows 暂存路径：
- `C:\ProgramData\dse_m1\m1_front2.png`
- `C:\ProgramData\dse_m1\m1_behind2.png`
- `C:\ProgramData\dse_m1\m2_perf.png`

截图说明：`DSE_RENDER_SCALE=0.25`，因此 scene 截图分辨率是 `320x180`；这是 llvmpipe 下降级验证的速度控制，不影响遮挡像素统计。窗口/最终尺寸由桌面机验证时使用默认 scale 复测。

---

## 4. 像素统计证据（M1 硬指标）

统计方法：
- 在 behind 截图中提取房子材质的蓝色像素，做孔洞填充得到房子屏幕多边形 mask；
- 房子 polygon bbox = `(82, 92) - (237, 102)`，共 `1336` 像素；
- 角色像素按主角红色披风颜色阈值检测（`r>110 && r>g+35 && r>b+35 && g>20`）；
- 将同一房子 mask 同时应用到 front/behind 两张截图。

结果：

| 截图 | 角色红色像素总数 | 房子 polygon 内 | 房子 polygon 外 |
|---|---:|---:|---:|
| `m1_behind.png`（角色在房子后） | 889 | **0** | **889** |
| `m1_front.png`（角色在房子前，对照） | 4896 | 418 | 4478 |

结论：
- **房子 polygon 内角色像素数 == 0**；
- **房子 polygon 外角色像素数 = 889 > 0**；
- 对照图 `m1_front.png` 房子 polygon 内角色像素数 = 418 > 0，说明同一角色、同一房子、同一相机下遮挡关系是可区分的，不是角色根本没画出来。

---

## 5. draw call / 精灵数统计（M2）

运行命令（WSL 降级验证）：

```bash
cd /mnt/c/Users/Administrator/Desktop/Engine/DSEngine
xvfb-run -a stdbuf -oL -eL env \
  DSE_MAX_FRAMES=62 DSE_SCREENSHOT_FRAME=60 DSE_SCREENSHOT_PATH=/tmp/m2_perf.png \
  DSE_RENDER_PIPELINE_PROFILE=forward_2d DSE_RENDER_SCALE=0.25 \
  ./bin/dsengine_lua_relwithdebinfo --script=templates/hd2d_wuxia/scripts/_sprite3d_perf_test.lua
```

实测输出：

```text
[sprite3d-perf] Awake sprites=1000 boxes=10 draw_calls=0 metric_sprites=0
[sprite3d-perf] frame=60 sprites=1000 draw_calls=24 metric_sprites=0
```

说明：
- `sprites=1000` 是脚本自身统计；`dse.metrics.get_sprite_count()` 仍只统计 2D sprite 路径，因此显示 `0`。这是引擎 metrics 口径问题，不是精灵没画。后续应把 Sprite3D 数量接入 metrics。
- `draw_calls=24 < 100`，目标满足。24 中包含 10 个盒子、地面、后处理和 UI 等固定开销；1000 个共享纹理 billboard 本身被合批为极少数 draw call。
- 如果 1000 个精灵使用大量不同纹理，排序键第二项会让 draw call 上升；本验收场景按任务书使用同图集/同纹理条件。

---

## 6. 回归

WSL/OpenGL（llvmpipe，DSE_ENABLE_3D=ON）降级回归：

- `templates/hd2d_wuxia/scripts/_compat_test.lua`：
  - 3D 构建运行通过，打印：
    ```text
    [compat] tilemap get_tile=3 font_width=279.7
    [compat] OK: P1 bool/number + P2 精灵 API + P3 tilemap_ex + P4 中文字表
    ```
- `templates/platformer_2d/scripts/main.lua`：
  - `DSE_MAX_FRAMES=5` 正常退出，RC=0；
  - 截图 `docs/design/hd2d_m1m2_shots/platformer_2d_reg.png`；
  - 运行结束时 `entities=43, draw_calls=5, render_passes=5`。
- `templates/topdown_3d/scripts/main.lua`：
  - `DSE_MAX_FRAMES=5` 正常退出，RC=0；
  - 截图 `docs/design/hd2d_m1m2_shots/topdown_3d_reg.png`；
  - 运行结束时 `entities=18, draw_calls=5, render_passes=5`。
  - 注意：WSL 构建关闭了 Jolt，因此它不能替代桌面机完整物理/渲染回归；只能证明新增 Sprite3D 路径没有破坏脚本启动、资源加载和既有帧流程。

桌面机仍需执行任务书的 `platformer_2d` / `topdown_3d` 完整回归截图。

---

## 7. 阻塞点

1. **台式机不可达**：本会话从笔记本侧 `ssh 169.254.139.190:22` 失败，无法执行任务书要求的 Windows 构建、截图和三后端 smoke/pixel 用例。
2. **Vulkan/D3D11 真机未验证**：代码和 shader 产物已同步，但缺少桌面 GPU 运行结果；特别是 DX11 input layout、Vulkan descriptor set / texture binding、三后端 SSAO/后处理顺序下的 Sprite3D 深度一致性。
3. **WSL/lavapipe Vulkan 尝试失败**：安装了 Mesa lavapipe 与 Vulkan loader 后，Vulkan 构建/链接成功，但启动时 Vulkan context 未成功创建 device/surface，随后在 `vkCreateShaderModule` 处 `Invalid device`。该失败发生在 Sprite3D 之外的基础 RHI 初始化阶段，不能证明也没有证明 Sprite3D Vulkan 路径可用；需要桌面 NVIDIA 真机验证。
4. **WSL/llvmpipe camera-relative 观察**：在同步路径（render_thread=0）下，初始 demo 把相机放在 `z=9` 时，`MeshRenderer` CPU 盒子没有按预期随 camera-relative 偏移，而 Sprite3D 的 CPU 端已减 `camera_offset`，导致盒子看起来在原点填满画面。为了得到可靠降级验证，demo 最终改为 **相机在原点、物体在负 Z**（`camera_offset=0`），绕过该差异。这个差异可能是 WSL/Linux 主线程路径的现有 bug，需桌面机确认；如果桌面机也有，应单独修 `ApplyCameraOffset`/主线程渲染路径。
5. **`TextureRef` 序列化语义**：反射不支持 `TextureRef`，当前 scene custom codec 持久化原始 RHI 句柄。跨会话加载后句柄可能失效；M5 资产管线接入 `.dsprite.json` / 纹理路径后应替换。
6. **`metrics.get_sprite_count()` 口径**：不统计 Sprite3D，性能验收需要脚本自己计数。建议补一个 `Sprite3D` 数量到 frame stats（非 M1/M2 必需）。

---

## 8. 未完成项

- Windows/MSVC RelWithDebInfo 构建、任务书指定的三后端运行与截图；Vulkan WSL/lavapipe 仅尝试到设备初始化失败，D3D11 未尝试。
- Vulkan/D3D11 的 Sprite3D pixel smoke 用例（至少覆盖 depth occlusion + alpha discard）。
- `platformer_2d`、`topdown_3d` 的桌面机完整回归截图（WSL/GL 冒烟回归已通过，但不含 Jolt/桌面三后端）。
- M3：`lit/receive_shadow/emissive` 的 shader 消费（字段/API/序列化已就位，M1 保持 unlit）。
- M2 半透明精灵排序（任务书明确先做 alpha-test 版本，半透明留后续 Transparent 层）。
- M5：Sprite3D 编辑器面板、纹理路径序列化、图集/动画。
- `dse.metrics` 的 Sprite3D 计数接入。

---

## 9. 合并为一个 commit 的理由

M1 的 `Sprite3DPass` 提取阶段天然包含 M2 的排序键和 `sorting_bias` 处理，
`SpriteBatchRenderer::DrawSprite3D` 的顶点/PSO 与合批游程也在同一函数中；
如果把 M2 排序/合批单独回退，Sprite3DPass 会处于能提取但不能正确排序/合批的中间态，
反而违反一个可回退 commit的目标。因此本次将 M1+M2 合并为一个可独立回退的
`feat(render)` commit，并在 commit body 中列出两阶段内容。
