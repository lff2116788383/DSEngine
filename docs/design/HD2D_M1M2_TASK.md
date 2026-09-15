# HD-2D M1+M2 任务书（Sprite3D / Billboard + 深度遮挡 + 排序批处理）

> 上游背景：`docs/design/HD2D_IMPLEMENTATION_PLAN.md`（19 项缺陷 + M1M6 + API 设计）。
> **本任务 = 该文档的 M1 + M2**，目标是先验证 HD-2D 的核心假设："2D 精灵能真正进入 3D 场景并被 3D 物体正确遮挡"。
> 环境铁律见 `docs/DESKTOP_BUILD.md` 顶部：**所有编译/运行/截图统一在台式机执行，笔记本只做编辑/素材/git。**

## 0. 基线（开工前确认）

| 项 | 值 |
|---|---|
| 分支 | `feature/engine-lib` |
| 基线提交 | `3578686a`（P1P5 已修并验证；P1 兼容层 / P2 精灵 5 接口 / P3 tilemap / P4 中文字表 / P5 启动失败不再 segfault） |
| 回归自证 | `templates/hd2d_wuxia/scripts/_compat_test.lua`（台式机跑一次应打印 `[compat] OK ...`） |
| 现有可跑模板 | `templates/hd2d_wuxia`（2D 近似版，玩法系统完整，可作对照） |

## 1. 现状约束（必须遵守，均已在代码中核实）

1. **精灵是纯 2D 管线**：`engine/render/shaders/src/sprite2d.vert` 只做 `vp * pos`（顶点已在 CPU 世界变换，无 billboard）；
   sprite pipeline 为 `depth_test=false / depth_write=false / cull=false`，且在 3D opaque **之后**绘制
   （`engine/render/passes/builtin_passes.cpp` L632-640；`engine/runtime/frame_pipeline.cpp` L556-563）。
2. **批处理排序**：`sorting_layer  shader_variant  material  texture  blend  order_in_layer`
   （`modules/gameplay_2d/rendering/sprite_render_system.cpp`），Lua 只能设 `order_in_layer`（旧）与 `sorting_layer`（P2 新增）。
3. **改渲染功能必须三后端同步**：OpenGL / Vulkan / D3D11，shader 源在 `engine/render/shaders/src/`，
   由 `dse_shader_compiler` 编译并生成 `*.gen.h`  **绝不手改 `*.gen.h` / `*.gen.cpp`**。
4. **不要手改 codegen 产物**：新 Lua API 优先加在手写文件
   `engine/scripting/lua/bindings/lua_binding_compat.cpp`（在生成绑定之后覆盖/追加，是 P1/P2/P3 已验证的模式）；
   若要正式入 codegen，改 `tools/codegen/binding_defs.json` 后重新生成。
5. **`rhi_types.h` 里的 `TextureFilter / TextureWrap / TextureSamplerDesc` 是全局作用域**（不在 `dse::render`）。
6. **native API 侧访问世界**：`engine/scripting/native_api/dse_api_internal.h` 提供 `GW()`(World*) / `TE(uint32)` / `GAM()`(AssetManager*)。

## 2. M1：精灵进 3D（核心，必成）

### 2.1 组件
新增 `Sprite3DComponent`（建议 `engine/ecs/components_3d_sprite.h`，或并入 `components_3d_render.h`）：
```cpp
struct Sprite3DComponent {
    dse::render::TextureRef texture_handle;
    glm::vec4 uv_rect{0,0,1,1};
    float size_w = 1.0f, size_h = 1.0f;
    float anchor_y = 0.0f;          // 0=脚底对齐 transform，0.5=中心
    int   billboard = 1;            // 0=None(平面) 1=Yaw 2=YawPitch 3=Screen
    bool  lit = false;              // M1 可先留 false，M3 接入光照
    bool  receive_shadow = false;    // M3
    glm::vec3 emissive{0,0,0};
    float opacity = 1.0f;
    float sorting_bias = 0.0f;      // 越小越靠前（M2 用）
    float z_offset = 0.0f;
    glm::vec4 color_tint{1,1,1,1};
};
```
同步：组件反射（`component_reflection.gen.cpp` 为生成物，按 codegen 流程加）+ 场景 JSON codec + 编辑器面板（可后置）。

### 2.2 Shader（三后端同步）
新增 `sprite3d.vert` / `sprite3d.frag`（GLSL 450，先只在 GL 打通，再同步 Vulkan/D3D11 源与执行器绑定）：
- billboard 展开（Yaw）：`right = normalize(vec3(view[0][0], view[1][0], view[2][0]));`
  顶点 = `world_pos + right * (pos.x * size_w) + up * (pos.y * size_h)`（up 用世界 Y 或 view 上方向，按模式切换）；
- `Screen` 模式：在裁剪空间做像素偏移（用于血条/伤害数字锁定屏幕朝向）；
- `frag`：采样 `albedo * color_tint`，`alpha < 0.1  discard`（保证能写深度）。

### 2.3 RenderGraph pass
新增 `Sprite3DPass`，位置在所有 3D opaque 之后、原 sprite pass 之前：
- 管线状态：`depth_test=on, depth_write=on, cull=off, blend=SrcAlpha/1-SrcAlpha`；
- 先做 **alpha test 版本**（discard 透明像素  深度正确、无排序穿透）；
- 与现有 `SpriteDrawItem`/`sprite_batch_renderer` 复用还是新建 batch，二选一但要在报告里说明理由。

### 2.4 Lua API（加到 `lua_binding_compat.cpp`）
```lua
ecs.add_sprite3d(e, tex, w, h, {billboard="yaw", anchor=0.0, lit=false})
ecs.set_sprite3d_uv_rect(e, u0,v0,u1,v1)
ecs.set_sprite3d_billboard(e, "yaw"|"yaw_pitch"|"screen"|"none")
ecs.set_sprite3d_sorting_bias(e, v)
ecs.set_sprite3d_emissive(e, r,g,b)
```

### 2.5 Demo + 验收（M1 的硬指标）
新建 `templates/hd2d_wuxia/scripts/_sprite3d_test.lua`：
- 用 `ecs.add_mesh_renderer`(内联顶点) 造 1 块地面 + 1 栋"房子"盒子，`set_mesh_shader_variant("MESH_LIT")`；
- 用 `ecs.add_camera_3d` 建 3D 相机（若顺手实现 `set_camera_ortho_3d(size,pitch,yaw)` 更好）；
- 放 1 个 billboard 角色，用演示输入让它**走到房子后面**；
- 按 `DSE_MAX_FRAMES` + `DSE_SCREENSHOT_FRAME` 出图。

**验收标准（必须给出像素证据）**：截图里房子矩形区域内角色像素数  0、房子外 > 0（用颜色计数/掩码统计，参照此前做法）；
并附一张"角色在房子前面"的对照图。

## 3. M2：排序与批处理

1. 排序键改为 `(depth_bucket  texture  blend)`，`depth_bucket = floor((world_z + sorting_bias) * K)`；
2. `sorting_bias` 支持前景层（屋檐/树冠盖住角色）；
3. 三后端管线状态同步；复用/扩展现有 `SpriteDrawItem`（`rhi_types.h`）；
4. **验收**：1000 个 billboard 精灵 + 若干 3D 盒子，遮挡正确且 draw call 合理（打印 `dse.metrics.get_draw_calls()` 与精灵数）。

## 4. 交付物

- 代码（组件/shader/pass/绑定/demo）+ `docs/design/HD2D_M1M2_REPORT.md`（改动清单、三后端同步矩阵、截图路径、draw call 与像素统计、阻塞点）；
- 更新 `docs/design/HD2D_IMPLEMENTATION_PLAN.md` 5 的 M1/M2 勾选状态；
- commit（Conventional Commits）+ push `origin/feature/engine-lib` + 台式机同步（bundlescpfetch/merge --ff-only）；
- 回归：`_compat_test.lua` 与 `platformer_2d` 不回退。

## 5. 台式机命令速查（照抄）

```powershell
# 同步（笔记本）
git bundle create _sync.bundle feature/engine-lib --not <台式机HEAD>
scp -o BatchMode=yes _sync.bundle Administrator@169.254.139.190:C:/ProgramData/dse_sync.bundle
```
```bat
:: 编译 + 运行（台式机）
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d C:\Users\70195005\Desktop\Engine\DSEngine
git fetch C:/ProgramData/dse_sync.bundle feature/engine-lib && git merge --ff-only FETCH_HEAD
cmake --build out\build\windows-x64-relwithdebinfo --target dse_example_lua --parallel
set DSE_MAX_FRAMES=300 & set DSE_SCREENSHOT_FRAME=260 & set DSE_SCREENSHOT_PATH=C:\ProgramData\m1.png
bin\dsengine_lua_relwithdebinfo.exe --script=templates\hd2d_wuxia\scripts\_sprite3d_test.lua
```
```powershell
# 取回截图（笔记本）
scp Administrator@169.254.139.190:C:/ProgramData/m1.png C:/ProgramData/m1.png
```

## 6. 风险与回退

| 风险 | 规避 |
|---|---|
| 三后端不同步 | 先 GL 打通并出图，再同步 Vulkan/D3D11；每步都跑 `_compat_test.lua` + `platformer_2d` |
| 透视/正交下 billboard 朝向错 | 用 view 矩阵列做展开，正交/透视通用；先只做 Yaw + Screen |
| alpha 混合与深度冲突 | M1 全部走 alpha test（discard），半透明留给后续 Transparent 层 |
| 反射/序列化漏改 | 新组件必须进反射 + scene codec，否则存档/编辑器读不到 |
| 两天内遮挡做不出来 | **如实记录阻塞点**，回退 B'（3D 背景 + 2D 精灵顶层）并标注"HD-2D 需引擎级投入" |