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