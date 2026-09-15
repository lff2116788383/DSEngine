# 真HD-2D 实现方案（3D 场景 + 2D 精灵一体化）

> 目标：把 DSEngine 从「2D 精灵管线 + 烘焙假光影」升级为真正的 HD-2D 
> **3D 地形/建筑（PBR/CSM/雾）＋ 2D 像素精灵（camera-facing billboard，参与深度/光照/阴影）＋ 移轴景深/泛光/体积光后处理**，
> 对标 逸剑风云决 / 歧路旅人 的观感。
> 本文同时是「引擎缺陷清单 + 分阶段施工图 + 验收标准」，配套当前可跑的 2D 版模板 `templates/hd2d_wuxia`。

---

## 0. 为什么必须改引擎（结论）

现有 2D 精灵路径**设计上就不可用于 HD-2D**：

| 现状 | 代码位置 | 后果 |
|---|---|---|
| 精灵顶点在 CPU 按 XY 平面变换，shader 只做 `vp * pos`，无 camera-facing 展开 | `engine/render/shaders/src/sprite2d.vert` | 换 3D 相机会变成"躺着的卡片"，没有 billboard |
| sprite pipeline `depth_test=false / depth_write=false / cull=false`，且在 3D opaque 之后绘制 | `engine/runtime/frame_pipeline.cpp` L556-563；`engine/render/passes/builtin_passes.cpp` L632-640 | 精灵永远盖在 3D 上，没有遮挡关系 |
| 只有 `order_in_layer` 可从 Lua 设置；批处理排序 `texture` 优先于 `order_in_layer` | `modules/gameplay_2d/rendering/sprite_render_system.cpp`（ExtractFrameRenderData 排序） | 跨纹理层次只能靠加载顺序；没有按世界 Y 的深度排序 |
| 无精灵深度输出 | 同上 | DoF/SSAO/雾/体积光对精灵无效 |

> 因此「开 `DSE_ENABLE_3D=ON` 就够了」是误解：**必须新增 Sprite3D/Billboard 渲染路径**并把精灵接入深度、光照与后处理。

---

## 1. 目标形态与验收标准

**画面构成**：45 正交（或弱透视）相机；3D 地形/台阶/建筑/桥；2D 像素角色/敌人/NPC/道具（billboard）；
画面同时具备：建筑 CSM 阴影、灯笼照亮角色、角色接地阴影、屋檐/树冠遮挡角色、Bloom 灯火、移轴景深、暗角/色调、雾。

**M1M6 合并验收（一张夜景截图必须同时满足）**
1. 角色走到屋檐下被**遮挡**，走到灯下时角色与地面同时被照**亮**；
2. 角色与 3D 物体各自有**正确投影/接地阴影**；
3. 画面有**泛光与移轴景深**（远景虚、角色所在焦平面清晰）；
4. 以上在三后端（OpenGL / Vulkan / D3D11）**观感一致**，GLES3/WebGL2 按能力降级；
5. 旧 `platformer_2d`、`topdown_3d` 行为不回退。

---

## 2. 引擎缺陷清单（19 项，含严重度与处置）

### 2.1 架构级（阻断 HD-2D，M1M4 解决）
| # | 缺陷 | 位置 | 处置 |
|---|---|---|---|
| 2.1.1 | 无 billboard/Sprite3D 路径 | `sprite2d.vert` | 新增 Sprite3D billboard 顶点展开（`view` 矩阵列向量）+ 屏幕对齐模式 |
| 2.1.2 | 精灵不参与深度 | `frame_pipeline.cpp` L556 | 新 pass：depth_test/write on + alpha test；透明边缘不写深度 |
| 2.1.3 | 无 `sorting_layer`/Y-sort | `sprite_render_system.cpp` 排序 | 排序键改为 `(depth_bucket, texture, blend)` + `sorting_bias`；补 Lua setter |
| 2.1.4 | 无精灵深度输出 |  | 精灵写深度  DoF/雾/SSAO 可用；另加 2D 友好的 tilt-shift 兜底 |
| 2.1.5 | 无 billboard 阴影投射 |  | 先做"接收 CSM + 接地椭圆投影阴影"，M3+ 再做 billboard shadow caster |

### 2.2 光照/阴影（M3 解决）
| # | 缺陷 | 位置 | 处置 |
|---|---|---|---|
| 2.2.1 | `Light2D` 是空实现（收集即丢弃） | `modules/gameplay_2d/lighting/light_2d_system.cpp` | 实现 2D 光照累积 pass（RT + 加法混合 + 法线贴图）或统一走 Sprite3D LIT |
| 2.2.2 | 无精灵接收 3D 光照/CSM |  | `SPRITE3D_LIT` 变体复用 clustered lights SSBO + shadow atlas |
| 2.2.3 | 无 2D 阴影投射/软阴影落地 |  | 2D 场景用光照累积 RT + 遮挡体；3D 场景用 CSM |

### 2.3 资源/API 缺口（低成本，缺陷批次先修）
| # | 缺陷 | 位置 | 处置 |
|---|---|---|---|
| 2.3.1 | `SpriteRendererComponent.uv` 无 Lua setter | `lua_binding_free_ecs_camera.gen.cpp` 仅 uv_offset/scroll | 补 `ecs.set_sprite_uv_rect` |
| 2.3.2 | `load_sprite_sheet` 拿到 UV 无处可用 | 同上 | 同 2.3.1（图集/精灵表落地） |
| 2.3.3 | 纹理固定 `{Linear, Repeat}` | `assets.load_texture(path)` | 补 `assets.load_texture_ex(path, filter, wrap)` |
| 2.3.4 | 无 blend/shader variant/sorting_layer setter | `SpriteRendererComponent` 有字段 | 补 4 个 setter（blend/variant/sorting_layer/color flash） |
| 2.3.5 | `add_tilemap` 不设 `tileset_cols/rows`，`tiles` 初值 -1 | `dse_api_physics2d.cpp` | 载入纹理尺寸自动推导 cols/rows，tiles 初值 0；补 `set_tilemap_collider` |
| 2.3.6 | `dse.tilemap` 需 `World*` lightuserdata，Lua 拿不到 | `lua_binding_tilemap.cpp` L_Create | 补 `dse.tilemap.attach(entity)`（内部用全局 World） |

### 2.4 API 一致性（低成本，缺陷批次先修）
| # | 缺陷 | 处置 |
|---|---|---|
| 2.4.1 | `ui.set_visible` 只收 number | Lua 侧兼容 bool/number |
| 2.4.2 | `audio.play_sfx/play_bgm` 的 loop 只收 int | 兼容 bool/number |
| 2.4.3 | PostProcess setter 命名/参数与文档不符（`fxaa`/`film_grain`/`vignette`） | 兼容别名 + 可变参数 |
| 2.4.4 | 三后端 shader/绑定需同步（改任何渲染功能） | 按 AGENTS 5 检查表执行 |

### 2.5 字体/稳定性/工具链（低成本，缺陷批次先修）
| # | 缺陷 | 位置 | 处置 |
|---|---|---|---|
| 2.5.1 | `load_cjk` 打包 `U+4E00` 起连续 800 码位（非高频字） | `dse_api_services.cpp` | 换成 ~1000 高频字 + 标点；补"自定义码位表"API |
| 2.5.2 | Lua `Awake` 抛错  引擎 segfault(exit 139) | `engine_app.cpp` / `lua_runtime.cpp` | 失败即干净返回错误码，不再进入渲染循环 |
| 2.5.3 | 无 headless/离屏 RHI，截图必须真实窗口 |  | 新增 null/offscreen 后端或 D3D11 WARP + GL surfaceless 路径，CI 无需 Xvfb |

---

## 3. 架构设计

### 3.1 组件与数据
```
Sprite3DComponent {
  texture_handle, uv_rect(vec4),
  size(worldW, worldH), anchor(0=脚底,0.5=中心),
  billboard_mode(None/Yaw/YawPitch/Screen),
  lit(bool), receive_shadow(bool), cast_shadow(bool),
  emissive(vec3), opacity, blend_mode,
  sorting_bias(float), z_offset(float), color_tint(vec4)
}
SpriteAtlasAsset { texture, clips: { name -> { frames[uv_rect, pivot, dur], loop, fps } } }
```

### 3.2 渲染流程（RenderGraph）
```
[ShadowPass(CSM)]  [OpaquePass(3D)]  [Sprite3DPass]  [ForegroundSpritePass]  [Transparent/Water]  [Bloom]  [DOF/TiltShift]  [UI]  [Composite]
```
- `Sprite3DPass`：depth test on / depth write on（alpha test 通过者）/ cull off；按 `(depth_bucket, texture, blend)` 排序后合批；
- 前景层（屋檐/树冠）：`sorting_bias < 0` 或独立 pass，保证盖住角色；
- 阴影：精灵在 ShadowPass 可选作为"投影卡片"（面片朝光），先只做接地阴影贴片。

### 3.3 光照模型（精灵）
`SPRITE3D_LIT` 顶点/片元：复用现有 PerFrame（vp/view/camera_pos）+ clustered lights SSBO + CSM shadow atlas + 可选法线贴图（从图集第二张或 `.dsprite.json` 指定）；
输出：`albedo * (ambient + Σ point/spot/dir) + emissive`，`emissive` 直接进 HDR 供 Bloom。

### 3.4 相机
- `set_camera_ortho_3d(size, pitch, yaw)`：正交 3D（保留 2D 操作手感，最接近 歧路旅人）；
- 弱透视可选（长焦小 FOV）用于建筑透视；
- 相机跟随/边界/抖动复用现有 2D 控制器逻辑（改成 3D 版 `CameraController3D`）。

### 3.5 逻辑与物理
3D 表现 + 2D 逻辑：角色仍用 AABB/圆形碰撞与手动推挤（当前模板已验证手感），地面高度用**高度场采样**（Terrain/TerrainTile 或自建 heightmap）；
需要真 3D 交互（攀爬、跳台、投射物）时再切 Jolt（`DSE_ENABLE_JOLT=ON`）。

### 3.6 资产管线
- 地形/建筑：`.dmesh/.dmat`（AssetBuilder 或 `tools/` 程序化生成），可复用现有 Terrain/Tree/Foliage 组件；
- 精灵：PNG + `.dsprite.json`（clips/uv/pivot/法线/发光），AssetBuilder 打包进 `.dpak`；
- 模板侧：`gen_maps.py` 增加"导出 3D 地形 + 道具 dmesh"的通道，美术风格不变。

---

## 4. 新增 API（Lua 可见，含向后兼容）

```lua
-- 精灵进 3D
ecs.add_sprite3d(e, tex, w, h, {anchor=0, billboard="yaw", lit=true})
ecs.set_sprite3d_uv_rect(e, u0,v0,u1,v1)
ecs.set_sprite3d_atlas(e, atlas_handle, "walk_down")
ecs.set_sprite3d_anim(e, "walk", {fps=10, loop=true})
ecs.set_sprite3d_billboard(e, "screen")        -- none|yaw|yaw_pitch|screen
ecs.set_sprite3d_lit(e, true)
ecs.set_sprite3d_emissive(e, r,g,b)
ecs.set_sprite3d_shadow(e, receive=true, cast=false)
ecs.set_sorting_bias(e, -0.5)                  -- 越小越靠前
ecs.set_sprite_blend(e, "additive")            -- alpha|add
ecs.set_sprite_shader_variant(e, "SPRITE3D_LIT")

-- 修复既有缺口
ecs.set_sprite_uv_rect(e, u0,v0,u1,v1)
ecs.set_sprite_sorting_layer(e, n)
assets.load_texture_ex(path, "nearest", "clamp")
assets.load_sprite_atlas(path)                 -- .dsprite.json
tilemap:attach(entity)                         -- 不再需要 World*
font.load_codepoints(id, ttf, {cp1, cp2, ...}) -- 自定义字形集

-- 相机/后处理
ecs.add_camera_3d(e, fov, priority, near, far)
ecs.set_camera_ortho_3d(e, size, pitch, yaw)
ecs.set_post_process_tilt_shift(e, enabled, focus, range, blur)
```

---

## 5. 分阶段施工图（单人估 46 周）

| 阶段 | 任务 | 关键文件 | 验收 | 估时 |
|---|---|---|---|---|
| **M1 精灵进 3D** | `Sprite3DComponent`+billboard shader+`Sprite3DPass`(depth test/write+alpha test)+Lua 绑定+最小 demo | `engine/ecs/components_3d_render.h`、`engine/render/shaders/src/sprite3d.*`、`engine/render/passes/`、`engine/scripting/lua/bindings/`、`tools/codegen/binding_defs.json` | 角色被 3D 房子遮挡，可绕到建筑后 | 12 周 |
| **M2 排序/批处理** | depth bucket 排序、`sorting_bias`、前景层、纹理合批、三后端管线状态 | `sprite_render_system.cpp`/`sprite_batch_renderer.*`、三后端 executor | 1000 精灵遮挡正确、draw call < 100 | 35 天 |
| **M3 光照/阴影** | `SPRITE3D_LIT`、法线贴图、CSM 接收、点/聚光、emissiveBloom、接地阴影 | `engine/render/shaders/src/sprite3d_lit.*`、`clustered` 光路、`shadow` pass | 灯笼照亮角色与地面、树影落在角色上 | 12 周 |
| **M4 后处理/相机** | 精灵写深度DoF、Tilt-shift、体积光适配、正交/弱透视 3D 相机 | `builtin_passes_postfx.cpp`、`camera` 组件/控制器 | 移轴景深生效且 UI 不受影响 | 1 周 |
| **M5 资产/工具** | `.dsprite.json`、AssetBuilder、程序化 3D 地形/道具导出、编辑器预览 | `engine/assets/`、`apps/tools/asset_builder/`、`templates/hd2d_wuxia/tools/` | `dse new hd2d` 直接产出 3D 地形+精灵工程 | 1 周 |
| **M6 模板升级+验证** | `hd2d_wuxia` 切 B+ 架构（保留现有系统）、golden 截图回归、CI 无头渲染 | `templates/hd2d_wuxia/`、`tests/`、`scripts/` | 第 1 节 5 条验收全绿 | 1 周 |

---

## 6. 缺陷批次（不等 M1M6，先修 2.3/2.4/2.5）

| 批次 | 内容 | 风险 |
|---|---|---|
| **P1** | 2.4.12.4.3 API 兼容层（`set_visible`/`play_sfx`/PostProcess 别名） | 低（纯 Lua 绑定兼容，不改渲染） |
| **P2** | 2.3.12.3.4 精灵 UV/blend/variant/sorting_layer setter + `load_texture_ex` | 低（新增 API，不动旧路径） |
| **P3** | 2.3.52.3.6 tilemap 尺寸推导 + `tilemap.attach` | 中（touch physics2d C API） |
| **P4** | 2.5.1 高频中文字表 + `font.load_codepoints` | 低 |
| **P5** | 2.5.2 Awake 失败不再 segfault | 中（改启动/关闭流程） |
| **P6** | 2.2.1 `Light2D` 真实实现（2D 光照累积 + 法线） | 中高（新 pass + 三后端） |
| **P7** | 2.5.3 headless/offscreen RHI（CI 截图） | 中高 |

> P1P5 是"缺陷修复"，可在模板继续迭代的同时并行完成；P6/P7 与 M1M4 有重叠，按 M3/M4 排期。

---

## 7. 测试与验证

1. **单测**：新组件的序列化/反射往返；`Sprite3DBatch` 排序键单测（构造乱序输入断言输出顺序）；`light accumulation` 的 CPU 侧数据单测。
2. **渲染验证**：沿用 `DSE_MAX_FRAMES/DSE_SCREENSHOT_FRAME/DSE_SCREENSHOT_PATH`，新增 golden 比对脚本
   （对关键区域做像素相关性/直方图门限，参照本次 2D 版验证方法：正常 vs 翻转相关性 0.587 vs -0.056）。
3. **多后端一致性**：同场景在 GL/Vulkan/D3D11 各出一张，做 SSIM 门限（现工程已有 RHI 后端矩阵）。
4. **能力降级**：GLES3/WebGL2 关闭 compute/SSBO 相关分支，保持"精灵能遮挡/能受光（简化）"。
5. **回归**：`platformer_2d`、`topdown_3d`、`tests/gtest` 全绿；RHI resource ledger 无泄漏。

---

## 8. 风险与规避

| 风险 | 规避 |
|---|---|
| 三后端 shader/管线不同步 | 严格走 AGENTS 5 检查表；先 GL 打通再同步 Vulkan/D3D11 |
| alpha test 与半透明排序冲突 | 不透明像素写深度 + alpha test；纯半透明（法术/光晕）走独立 Transparent 层 |
| 正交相机深度精度 | 收窄 near/far（如 0.1200），按相机高度动态调整 |
| billboard 投影阴影效果差 | 先只做接地椭圆阴影 + 接收 CSM；投影列为 M3+ |
| 组件新增破坏存档/编辑器 | 同步 `component_reflection.gen.cpp` + scene JSON codec + 编辑器面板 |
| 工期膨胀 | 每阶段独立可交付：M1/M2 完成即可判定 HD-2D 路线成立与否 |

---

## 9. 现有 2D 模板的定位

`templates/hd2d_wuxia`（青溪问剑）当前是"**2D 管线 + 烘焙光影 + 后处理**"的近似版，价值在于：
系统层（战斗/技能/AI/任务/存档/中文 UI）已完整，M1M6 只需替换**表现层**（地图/角色渲染 + 光照）与资产导出，
玩法逻辑与数据可原样迁移。也就是：**先用 2D 版把"游戏做完"，再用 B+ 把"画面做真"**。