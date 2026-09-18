# 青溪问剑  新游戏完整设计（WUXIA_ARPG_NEW_GAME_DESIGN）

> 版本：v1.0（R3，2026-09-18）
> 工程：`games/wuxia_arpg/`
> 明确约束：不把 `templates/hd2d_wuxia` 当游戏底座，不复制其代码/素材；素材全部由新生成器产出。

## 1. 目标

R3 已交付一个**独立新游戏**的垂直切片：
- 1 张地图 `qingxi_village` 可玩。
- HD-2D：程序化 3D 地形 + lit Sprite3D 角色/敌人 + 灯笼点光 + 方向光 + Bloom/后处理。
- 核心循环：移动 -> 攻击 -> 敌人死亡 -> 掉落 -> 经验/金币 -> 升级 -> 自动存档。
- OpenGL / Vulkan / D3D11 三后端验收通过。
- 素材全部由 `games/wuxia_arpg/tools/gen_assets.py` 新生成。

R4 起继续在同一工程深化，不回退到模板。

## 2. 目录与代码职责

| 路径 | 职责 |
|---|---|
| `games/wuxia_arpg/scripts/main.lua` | 生命周期、相机、demo、autosave、攻击命中结算 |
| `core.lua` | 数学/输入/路径/验收日志 |
| `assets.lua` | 新资产加载、actor atlas 切换、音频 |
| `data.lua` | 地图网格、出生点、敌人配置 |
| `terrain.lua` | 合并 3D lit 地形网格、道具 billboard、灯笼点光 |
| `player.lua` | 移动/碰撞/攻击/闪避/成长/装备原型 |
| `enemy.lua` | 山贼 AI/攻击/死亡/掉落 |
| `loot.lua` / `rng.lua` | 确定性随机、装备原型 |
| `ui.lua` / `font.lua` | 新字体图集 HUD |
| `save.lua` | `dse.serialize` 存档/读档 |
| `tools/gen_assets.py` | 新素材生成 |
| `tools/run_r3_acceptance.py` | R3 三后端验收 |
| `tools/pixel_stats.py` | 截图像素统计 |

## 3. 地图 `qingxi_village`

- 尺寸：24 x 18，tile 1.0 世界单位。
- 出生点：`(12.0, 15.0)`。
- 地形字符：
  - `.` 草地
  - `,` 土路
  - `#` 石壁（碰撞）
  - `T` 树（碰撞 + billboard）
  - `H` 房屋（碰撞 + billboard）
  - `R` 岩石（碰撞 + billboard）
  - `L` 灯笼（billboard + 点光 + emissive）
- 敌人：5 个 `bandit`，分散在地图内。
- 表现：每个格子生成 lit 地形四边形，按 grass/dirt/stone/water 分组；`#` 额外生成石壁盒子；树/竹/房屋/灯笼/岩石为 lit Sprite3D billboard。
- 光照：冷色方向光 + 每个 `L` 一盏暖色点光；`DSE_WUXIA_LIGHTS=0` 用于受光对照。
- 相机：3D 透视相机，跟随玩家，`(P.x, 6.2, P.z + 6.2)`，pitch -48，FOV 38。

## 4. 玩家与敌人

### 玩家
- 属性：hp 120、mp 60、atk 10、def 3、level/exp/gold、items。
- 移动：WASD/方向键，世界坐标 `x/z`，格碰撞。
- 攻击 J：单段但可重复；命中范围 1.45，方向扇形判定；伤害 `atk + 5`。
- 闪避 K：短冲刺 + 无敌帧。
- 动画：4 方向，action = idle/walk/attack/dodge/hurt/die。
- 成长：升级 + hp/mp/atk/def；掉落装备原型进 `P.items`。

### 敌人 `bandit`
- hp 42、atk 8、speed 1.9、sight 7.0、range 1.15、cd 1.2、exp 14、gold 8。
- AI：idle -> sight 内 chase -> range 内攻击；死亡后 1.2s 销毁。
- 死亡：经验/金币入账，按 35% 概率掉随机装备原型。
- sprite atlas：新生成的 4 方向 18 帧图集。

## 5. 随机装备原型

- RNG：`rng.lua` xorshift32，固定 seed 可复现。
- 基底：铁剑/布衣/玉佩。
- 稀有度：普通/魔法/稀有/传奇；魔法 1-2 词缀，稀有 3-4，传奇 4-5。
- 词缀：攻击/防御/生命/内力/暴击 + 身法/吸血/幸运/回气。
- 死亡掉落：`Loot.drop(ilvl, rank)`。
- 存档：完整 `items` + `item_seq` 经 `dse.serialize` 持久化。

## 6. R3 验收命令与结果

```powershell
python games\wuxia_arpg\tools\gen_assets.py
bin\dsengine_lua_debug.exe --script=games\wuxia_arpg\scripts\_logic_test.lua
python games\wuxia_arpg\tools\run_r3_acceptance.py --backends opengl --out-dir tmp\r3_new_game --max-frames 240 --shot-frame 210
python games\wuxia_arpg\tools\run_r3_acceptance.py --backends vulkan --out-dir tmp\r3_new_game --max-frames 240 --shot-frame 210
python games\wuxia_arpg\tools\run_r3_acceptance.py --backends d3d11 --out-dir tmp\r3_new_game --max-frames 240 --shot-frame 210
```

| 后端 | lit-on luma | lit-off luma | delta |
|---|---:|---:|---:|
| OpenGL | 68.24 | 8.11 | 60.13 |
| Vulkan | 68.10 | 7.63 | 60.47 |
| D3D11 | 68.16 | 8.15 | 60.01 |

逻辑测试：`[r3-logic] PASS rng=3336926330 items=30 save_items=30`。

## 7. 真实 API 约束

新游戏使用且仅使用已验证 API：

| 能力 | API | 证据 |
|---|---|---|
| lit Sprite3D | `ecs.add_sprite3d` / `ecs.set_sprite3d_lit` | `lua_binding_compat.cpp:503,506` |
| 图集动画 | `assets.load_sprite_atlas` / `ecs.set_sprite3d_atlas` / `ecs.set_sprite3d_anim` | `lua_binding_compat.cpp:510-512` |
| 接地阴影 | `ecs.set_sprite3d_contact_shadow` | `lua_binding_compat.cpp:509` |
| 3D 地形网格 | `ecs.add_mesh_renderer` | `lua_binding_free_ecs_mesh.gen.cpp:156` |
| MESH_LIT | `ecs.set_mesh_shader_variant` | `lua_binding_ecs_mesh_renderer.gen.cpp:140` |
| 点光 | `ecs.add_point_light_3d` | `lua_binding_free_ecs_light.gen.cpp:232` |
| 方向光 | `ecs.add_directional_light_3d` | `lua_binding_free_ecs_light.gen.cpp:228` |
| 3D 相机 | `ecs.add_camera_3d` | `lua_binding_free_ecs_camera.gen.cpp:135` |
| Bloom/后处理 | `ecs.set_post_process_*` | `lua_binding_ecs_post_process.gen.cpp` |
| UI 文本 | `ui.add_renderer` / `ui.set_uv` / `ui.set_position` | 现有 UI 绑定 |
| 存档 | `dse.serialize.encode/decode` | `lua_binding_serialize.cpp:13-14` |
| 音频 | `dse.audio.play_sfx` | `lua_binding_free_audio_full.gen.cpp:318` |

## 8. 后续轮次

- R4：三段连招、技能、完整装备/词缀/背包、精英/Boss 阶段、更多自动化测试。
- R5：再新增 2 张新地图（总计 3 张）、天气/光影打磨、地图连接与刷子难度。
- R6：全量验收、性能、文档、交付。

## 9. 非目标

- 不使用 `templates/hd2d_wuxia` 或 `templates/topdown_3d` 的游戏素材。
- 不新增第三方库。
- 不新增 RHI 特性；如改渲染，三后端同步并重新生成 shader，不手改 `*.gen.h`。
- 不使用任何商业游戏解包素材。