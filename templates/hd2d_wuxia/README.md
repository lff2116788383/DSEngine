# 青溪问剑  HD-2D 武侠模板

DSEngine 的 2D 品类模板：**分层大地图 + 八向角色 + 实时连招/技能 + 敌人 AI + 任务/对话/背包/存档**，
美术与音频**全部由脚本程序化生成**（`tools/` 下一键重建），不依赖任何第三方素材。

- 玩法：武侠 ARPG，两关（青溪村  黑风寨）+ Boss「寨主血刀」，主线任务链，8 种敌人行为。
- 画面：分层大地图（地面/前景遮挡）+ 烘焙光照与接地阴影 + Bloom/暗角/颗粒/FXAA + 相机平滑跟随/前瞻/
  震屏/受击顿帧 + 落叶/萤火天气 + 灯火呼吸 + 圆形接地阴影。
- 系统：三段连招（含暴击/击退/顿帧）、闪避无敌帧、两个技能（AoE / 治疗+增益）、体力、内力、
  经验等级成长、掉落/拾取、装备、NPC 打字机对话、任务追踪、背包、暂停、存档/读档、死亡/通关。

## 操作

| 按键 | 功能 |
|---|---|
| WASD / 方向键 | 八向移动 |
| J | 三段连招（第三段伤害最高） |
| K | 闪避（无敌帧，消耗体力） |
| U | 分花拂柳（360 剑气，18 内力，4s CD） |
| I | 紫霞真气（治疗 35% + 攻击 +35%，22 内力，10s CD） |
| 1 / 2 | 金创药 / 回气散 |
| E | 与 NPC 交谈 |
| Tab | 背包 |
| Esc | 暂停（继续/存档/读档/退出） |
| R | 死亡或通关后重开 |

## 运行

```bat
:: 仓库内直接跑（开发用）
bin\dsengine_lua_relwithdebinfo.exe --script=templates\hd2d_wuxia\scripts\main.lua

:: 或生成独立工程
dse new hd2d MyWuxia
```

自动化截图（无头验证，环境变量见 AGENTS.md 10）：

```bat
set DSE_MAX_FRAMES=420
set DSE_SCREENSHOT_FRAME=380
set DSE_SCREENSHOT_PATH=C:\temp\hd2d.png
set DSE_HD2D_AUTOSTART=1
set DSE_HD2D_DEMO=1
bin\dsengine_lua_relwithdebinfo.exe --script=templates\hd2d_wuxia\scripts\main.lua
```

- `DSE_HD2D_AUTOSTART=1`：跳过标题直接开局；`DSE_HD2D_DEMO=1`：内置演示输入（走位/挥剑/闪避），便于截图。

## 目录

```
assets/   生成产物：maps/(分层地图) char/ enemy/ npc/ fx/ ui/(含中文位图字体图集) audio/
scripts/  core 工具  assets 资源  data 数据  world 关卡/碰撞/相机  player  enemy  fx  ui  save  main
tools/    generate_assets.py 一键重建；gen_lib/gen_maps/gen_font/gen_audio 分模块
```

重生成素材（需要 Pillow；本仓库用 WSL 的 python3）：

```bash
python3 tools/generate_assets.py     # 角色/特效/UI/字体图集/地图/音频 全量重建
python3 tools/_gen_maps_only.py      # 只重建地图与 mapdata.lua（调地图布局时用）
```

`gen_maps.py` 里 `MAP_SPECS` 用「特征列表」描述关卡（水面/崖壁/竹林/房屋/道路/灯笼/刷怪点/NPC/掉落），
**同一份数据同时用于绘制美术与导出 `scripts/mapdata.lua` 的碰撞格子**，保证"看到的就是能撞到的"。

## B+ 3D 表现层（可选）

设置 `DSE_HD2D_BPLUS=1` 后，模板保留原有 2D 逻辑、碰撞、AI、任务与存档，仅把表现层切换为 HD-2D B+：

- `gen_maps.py` 生成 `assets/maps/*_3d.dmesh` 程序化 3D 地形/道具；运行时由 `bplus.lua` 加载为 `MESH_LIT` 网格。
- `gen_atlases.py` 为全部角色/敌人/NPC/FX 生成 `.dsprite.json` + `_atlas.png`；B+ presenter 用 Sprite3D 播片。
- 2D 精灵会被隐藏，由 Sprite3D 镜面实体接管位置、朝向、翻面、接触阴影与动画；3D 相机跟随玩家，地图点光转成 3D 点光。
- 2D 精灵加载顺序仍保留，未开启该环境变量时行为与旧 2D 模板完全一致。

示例：

```bat
set DSE_HD2D_BPLUS=1
set DSE_HD2D_AUTOSTART=1
set DSE_MAX_FRAMES=180
bin\dsengine_lua_debug.exe --script=templates\hd2d_wuxia\scripts\main.lua
```

重新生成 B+ 资产：

```bash
python3 tools/generate_assets.py       # 全量 PNG + dmesh + dsprite + 字体/音频
python3 tools/_gen_maps_only.py        # 只重建地图与 maps/*_3d.dmesh
python3 tools/gen_atlases.py .         # 只重建角色/敌人/NPC/FX 图集
```

## 美术图集切分流程（gen_atlases.py）

Sprite3D 播片不直接吃逐帧 PNG，而是吃**打包图集** `.dsprite.json`（`texture` + `frames[].uv_rect` +
`clips{}.frames/fps/loop`）。切分由 `tools/gen_atlases.py` 完成，是「美术素材 → 运行时可播动画」的唯一入口。

### 1) 输入：逐帧 PNG 的命名与落盘位置

| 类别 | 目录 | 命名规则 | 例 |
|---|---|---|---|
| 角色/敌人 | `assets/char/<actor>/`、`assets/enemy/<actor>/` | `<prefix>_<dir>_<action>_<n>.png`（`n` 从 0 起，按序号排序） | `hero_d_walk_0.png` … `hero_d_walk_5.png` |
| NPC | `assets/npc/<name>/` | `<name>_<dir>_idle_<n>.png` | `elder_d_idle_0.png` |
| FX 序列 | `assets/fx/` | `<kind>_<n>.png` | `slash_0.png`、`slash_1.png` |
| FX 单帧 | `assets/fx/` | `<kind>.png` | `glow_warm.png` |

`dir` 取 `d/u/l/r`，`action` 取 `idle/walk/attack/dodge/cast/hurt/die`（各 actor 支持的集合见
`ACTOR_SPECS` / `NPC_SPECS` / `FX_SEQUENCES`）。**素材由 `gen_lib.py` 程序化生成**，所以默认无需外部美术；
若换成真实美术，只要按上表命名落盘即可，脚本无需改。

### 2) 切分命令

```bash
python3 tools/gen_atlases.py .        # 参数是「输出根目录」，"." = 模板根（本目录）
```

脚本扫描上述目录 → 按 actor/方向/动作横向打包 → 写两个文件到素材同目录：

- `<prefix>_<dir>_<action>_atlas.png`：打包后的图集（帧横向排列）；
- `<prefix>_<dir>_<action>_atlas.dsprite.json`：`texture`（兄弟文件名，按图集目录解析）、
  `width/height`、`frames[]`（`name/index/pixel_rect/uv_rect/pivot`）、`clips{<action>:{frames,fps,loop}}`。

`fps/loop` 来自 `ACTION_FPS` / `ACTION_LOOP`（未列出则 10 帧/循环）；FX 序列按 20 帧非循环、`pivot_y=0.5`。

### 3) 附属贴图（法线 / 自发光）

若同目录存在 `<base>_normal.png` / `<base>_emissive.png`（`<base>` = 图集名去掉 `_atlas`），脚本会把
`normal` / `emissive` 字段写进 json；运行时 `load_sprite_atlas` 会一并加载，Sprite3D lit 路径据此做
法线扰动与 Bloom 自发光（验证用例：`_sprite3d_normal_emissive_test.lua`）。

### 4) 消费方式（三处，任选其一即可让实体用上）

```lua
-- Lua（B+ presenter 走的就是这条）
local atlas = dse.assets.load_sprite_atlas("assets/char/hero/hero_d_walk_atlas.dsprite.json")
dse.ecs.set_sprite3d_atlas(e, atlas, "walk")
dse.ecs.set_sprite3d_anim(e, "walk", { fps = 8.0, loop = true })
```

- **编辑器**：Inspector → Sprite3D → `Atlas` 填 `.dsprite.json` 路径、`Clip` 填片段名，缩略图与
  **独立预览视口**（Window 菜单 → Sprite3D Preview）可直接看播片与时间轴。
- **场景持久化**：`.dscene` 存 `atlas_path` + `clip_name`（不是裸 RHI 句柄），加载时经运行时 atlas
  注册表重新解析纹理与逐帧 UV（`editor_scene_io.cpp` 的 `ResolveSprite3DAtlas`，与运行时
  `scene_json_codec_custom.h` 同源）。

### 5) 自检

```bash
python3 tools/gen_atlases.py .                          # 期望输出 [atlas] actor/npc atlases=97 / fx atlases=8
dse AssetBuilder --sprite <x>.dsprite.json out.dsprite  # 期望 "N frames, M clips"
```

实测（本机 RTX 3070 / Windows，在 `dse new hd2d` 生成的工程副本上跑）：

- 生成器输出 `[atlas] actor/npc atlases=97` + `[atlas] fx atlases=8` → 共 **105 张图集**；
- 模板目录现有 **105 个 `*_atlas.dsprite.json` + 105 张 `*_atlas.png`**（另有 2 个手写
  `.dsprite.json` 供 M3/M5 像素用例使用，故 `.dsprite.json` 总数为 107）；
- `dse new hd2d` 产出的独立工程：**107 个 `.dsprite.json` + 105 张图集 PNG + 2 个 `*_3d.dmesh`**，
  且按本 README「运行」章节跑通（日志 `[bplus] map=village mesh=assets/maps/village_3d.dmesh lights=13`，
  无 `MISSING texture`）。

## 引擎约束（本模板已规避，改动前必读）

1. **纹理加载顺序 = 世界精灵绘制顺序**。精灵批处理排序为 `sorting_layer  shader_variant  material 
   texture  blend  order_in_layer`，而 Lua 的 `ecs.add_sprite` 的 order 只写 `order_in_layer`，
   没有设置 `sorting_layer` 的接口；因此跨纹理层次只能靠加载顺序（`assets.lua` 里
   背景  光晕/阴影  道具  角色  特效  前景遮挡，最后才是 UI 字体纹理）。
2. `ecs.add_sprite` 的基础四边形是 **1x1 世界单位、以 transform 为中心**，纹理按四边形拉伸；
   像素尺寸需自己换算（本模板 `A.size` 表 + 32px/格）。
3. `SpriteRendererComponent.uv` 支持子矩形，但 Lua 只暴露 `uv_offset/uv_scroll`；
   所以地图是**整图烘焙**（1 次 draw call/层），角色动画是**逐帧 PNG** + `add_animation_state`。
   精灵 UV 方向已用截图相关性验证（正常 vs 翻转：0.587 vs -0.056）。
4. `dse.ecs.add_light_2d` 系列目前是**空实现**（`Light2DSystem::ExtractFrameRenderData` 只收集不渲染），
   本模板的灯火效果改为「光晕精灵 + 地面烘焙暖色光照 + Bloom」。
5. `audio.play_sfx(path, vol, loop)` 的 loop 形参在引擎侧是 **int**（`luaL_checkinteger`），必须传 0/1。
6. `ui.set_visible(e, v)` 需要 **number**（0/1），传 boolean 会报 `number expected, got boolean`。
7. PostProcess 的 setter 名称以生成绑定为准：`set_post_process_fxaa_enabled` /
   `set_post_process_film_grain_enabled` / `set_post_process_vignette_enabled` 等；
   `set_post_process_film_grain(e, enabled, intensity)` 参数个数与文档不一致，建议用细粒度 setter。
8. 引擎自带的 `dse.font.load_cjk` 只打包 `U+4E00` 起连续 800 个码位（常用字大量缺失），
   中文 UI 改为**自绘位图字体图集**（`gen_font.py` 扫描 `.lua` 文本收集字形 + `core.Text` 逐字排版）。

## 已知限制 / 后续可做

- 地图是整图烘焙，不支持运行时改地形；需要动态地块时应改回 Tilemap（但注意第 3 点的 UV 限制）。
- 没有寻路（敌人为直线追击 + 视线判定）；需要绕墙可接 `dse.pathfinding` 的 2D 网格 A*。
- 无 Swimming/跳跃；如需 Z 轴可切 3D 模块（见 README「HD-2D 与 3D」讨论）。