# HD-2D 武侠刷子 ARPG 完整设计（WUXIA_ARPG_DESIGN）

> 版本：v1.0（R2，2026-09-18）
> 基线：`feature/engine-lib` @ `0a8fc8b0`
> 目标分支：`feature/hd2d-wuxia-arpg`
> 轮次契约：`docs/design/WUXIA_ARPG_PLAN.md`
> 资产台账：`docs/design/WUXIA_ARPG_ASSET_LEDGER.md`

## 1. 设计目标

做一个可玩、可刷、可成长的 HD-2D 武侠 ARPG：
- 暗黑 2 式核心循环：刷怪 -> 掉落随机装备/词缀 -> 换装成长 -> 打更高强度敌人 -> 再刷。
- 3 张地图：青溪村、黑风寨、幽篁秘谷。
- 战斗手感：三段连招、闪避无敌帧、技能、暴击、击退、Boss 多阶段。
- 成长：等级、经验、金币、背包、装备、随机词缀、稀有度、技能强化。
- 表现：3D 程序化地形 + lit Sprite3D 角色/敌人/FX + 暖点光/方向光 + 接触阴影 + Bloom/后处理 + 天气。
- 三后端：OpenGL / Vulkan / D3D11 都有可复现证据。
- 资产：只用仓库内程序化生成素材；字体图集使用 OFL 的 Noto Sans SC；禁止商业游戏解包素材。

## 2. 世界设计

### 2.1 三张地图总表

| id | 名称 | 尺寸 | tile | 等级带 | 天气 | BGM | 出口 | 主要敌人 | Boss |
|---|---|---:|---:|---:|---|---|---|---|---|
| `village` | 暮色青溪村 | 44x30 | 32 | 1-3 | 落叶 / 小雨 | field | 北 -> stronghold | bandit, wolf | 无 |
| `stronghold` | 夜雨黑风寨 | 44x30 | 32 | 4-8 | 夜雨 / 雷暴 | boss | 南 -> village；北 -> youhuang（击败寨主后） | bandit, wolf, ghost | 寨主血刀 |
| `youhuang` | 幽篁秘谷 | 48x34 | 32 | 9-14 | 迷雾 / 萤火 / 雨 | field, boss | 南 -> stronghold | ghost, wolf, bandit, elite | 幽篁剑主谢无咎 |

### 2.2 `village`：暮色青溪村

- 定位：起始村、任务 hub、低强度刷怪区、装备/药剂教学。
- 布局：北侧山口、南侧溪流/桥、三座房屋（家/铁匠铺/祠堂）、竹丛、树、篱笆、灯笼、井、宝箱。
- 出生点：`(20.5, 25.5)`。
- 出口：北侧 `(20, 0.4, w=4, h=1.6)` -> `stronghold` / `entry=south`。
- NPC：`elder`、`smith`、`villager`；任务对话沿用现有文本并新增幽篁秘谷线索。
- 敌群：bandit x2、wolf x2、ghost x1（普通）；R4 后按难度注入精英。
- 光照：13 个暖色灯笼点光；白天/黄昏 ambient；B+ 下使用 `ecs.add_point_light_3d`。
- 天气：默认 `leaf`，有 20% 概率切换 `light_rain`。
- 掉落：低 ilvl 1-3；装备基底以铁剑/皮甲/玉佩为主。

### 2.3 `stronghold`：夜雨黑风寨

- 定位：中段主线、刷装备、第一个 Boss。
- 布局：南侧入口、北侧寨主庭院、三座建筑（大厅/兵营/库房）、竹丛、树、篱笆、灯笼、井、宝箱。
- 出生点：`(21.5, 26.5)`。
- 出口：南 `(20, 28.2, w=4, h=1.6)` -> `village`；北 `(22, 0.4, w=4, h=1.6)` -> `youhuang`，解锁条件：Boss `blood_blade` 已击杀且拿到 `boss_token`。
- 敌群：bandit x4、wolf x1、ghost x1；R4 后提高精英/冠军出现率。
- Boss：`blood_blade`，位置 `(21.5, 6.5)`，三阶段，必掉 `boss_token` + 稀有以上装备。
- 光照：13 个暖/血红点光；夜雨 ambient 偏冷；CSM 方向光变弱。
- 天气：默认 `rain`，Boss 激活时切 `storm`，含闪电脉冲。
- 掉落：ilvl 4-8；开始掉落带 1-2 条词缀的魔法装备。

### 2.4 `youhuang`：幽篁秘谷（第 3 张，R5 新增）

- 定位：后期刷子地图、挑战 Boss、刷稀有/传奇装备。
- 布局：四周崖壁包围，内部竹林、古木、废弃石屋/祠堂、石径、灯笼/鬼火、宝箱；中部有开阔 Boss 场。
- 出生点：`(24.5, 30.5)`。
- 出口：南侧 `(24, 33.2, w=4, h=1.6)` -> `stronghold` / `entry=north`。
- 敌群：ghost x4、wolf x2、bandit x2；精英与冠军为 R5 主要刷子目标。
- Boss：`youhuang_lord`，位置 `(24.5, 5.5)`，三阶段，技能含剑气波、招魂、雾隐突进；必掉传奇保底。
- 光照：18 个冷色鬼火点光 + 微弱月光方向光；雾中远处灯光衰减。
- 天气：`fog_firefly` 为主，随机 `rain`；雾浓时降低 Bloom/exposure 对比。
- 掉落：ilvl 9-14，稀有/传奇权重最高；地图本身是 R5/R6 的主要刷子验证场。

### 2.5 地图连接与解锁

```
village --北门--> stronghold --北门(击杀寨主后)--> youhuang
   ^                   |                               |
   +-------南门--------+-----------南门--------------+
```

- 地图切换沿用现有 `main.lua` 的 `G.pending_map` / `G.pending_entry` / `ENTRY` 流程。
- 新地图必须加入 `MAP_SPECS` 后由 `gen_maps.py` 生成，禁止手改 `mapdata.lua`。
- 所有出口/出生点/敌人点位在 `MAP_SPECS` 中数据化。

## 3. 刷子装备系统

### 3.1 物品数据模型

新增 `scripts/loot.lua` / `scripts/items.lua`（R4 实现），纯 Lua 表：

```lua
item = {
  uid = 10001,
  base_id = "iron_sword",
  name = "铁剑",
  slot = "weapon",          -- weapon / armor / accessory
  rarity = "magic",         -- common / magic / rare / legendary
  ilvl = 5,
  req_level = 3,
  implicit = { atk = 3 },
  affixes = {
    { id = "prefix_atk", tier = 2, stat = "atk", value = 7 },
    { id = "suffix_crit", tier = 1, stat = "crit_chance", value = 0.03 },
  },
  computed = { atk = 10, crit_chance = 0.03 },
  icon_key = "icon_sword",
  created_by = { map = "stronghold", enemy = "bandit", rank = "elite" },
}
```

约束：
- `uid` 在一次存档内唯一；读档后保留。
- `affixes` 是数组，避免重复词缀；每个词缀带 tier 与 roll 值。
- `computed` 为缓存，可从 base + implicit + affixes 重算。
- 药水/铜钱继续使用现有 `P.inv` 计数模型；装备进入新 `P.items` 列表与 `P.equip`。

### 3.2 基底

| base_id | 名称 | slot | req_level | 基础属性 | 备注 |
|---|---|---|---:|---|---|
| `iron_sword` | 铁剑 | weapon | 1 | atk+6 | 起始 |
| `qingfeng_sword` | 青锋剑 | weapon | 5 | atk+12, crit+0.02 | 中期 |
| `xuantie_sword` | 玄铁重剑 | weapon | 10 | atk+22, crit_damage+0.15 | 后期 |
| `cloth_robe` | 布衣 | armor | 1 | def+4, hp+10 | 起始 |
| `leather_armor` | 皮甲 | armor | 4 | def+8, hp+20 | 中期 |
| `chain_armor` | 锁子甲 | armor | 9 | def+14, hp+35 | 后期 |
| `jade_pendant` | 玉佩 | accessory | 3 | mp+20, magic_find+0.05 | 中期 |
| `tiger_talisman` | 虎符 | accessory | 7 | atk+8, crit+0.03 | 后期 |
| `spirit_ring` | 灵犀戒 | accessory | 11 | mp+30, life_steal+0.03 | 后期 |

### 3.3 稀有度与词缀

| 稀有度 | 词缀数 | 基础权重 | 精英权重 | 冠军权重 | Boss |
|---|---:|---:|---:|---:|---:|
| common | 0 | 60% | 35% | 20% | 0% |
| magic | 1-2 | 30% | 45% | 40% | 20% |
| rare | 3-4 | 9% | 18% | 30% | 65% |
| legendary | 4-5 + 固定威能 | 1% | 2% | 10% | 15% |

前缀池（示例）：
- `prefix_atk`：攻击 +N
- `prefix_def`：防御 +N
- `prefix_hp`：最大生命 +N
- `prefix_mp`：最大内力 +N
- `prefix_crit`：暴击率 +N%
- `prefix_crit_dmg`：暴击伤害 +N%
- `prefix_haste`：攻击速度 +N%
- `prefix_skill`：技能伤害 +N%

后缀池（示例）：
- `suffix_stamina`：体力上限/回复
- `suffix_life_steal`：吸血 +N%
- `suffix_mana_regen`：内力回复 +N/秒
- `suffix_move`：移动速度 +N%
- `suffix_magic_find`：掉落幸运 +N%
- `suffix_gold_find`：金钱 +N%
- `suffix_thorns`：反伤 +N
- `suffix_resist`：受到伤害降低 +N%

词缀值按 `ilvl` 和 tier 计算，tier 越高 roll 越高；R4 必须写成纯函数并有固定 seed 测试。

### 3.4 掉落与随机

- 纯 Lua `loot.lua` 使用自定义 xorshift32 `rng`，seed 来自角色 run seed + 地图 id + 击杀计数，保证重放/测试可复现。
- 每个敌人有 `rank` 与掉落等级区间：
  - normal：基础权重；
  - elite：额外 magic/rare 权重，掉 1 件装备概率高；
  - champion：额外 rare/legendary 权重，必掉至少 magic；
  - boss：必掉保底，Boss 专属暗金/传奇可加入。
- 金币继续 `P.gain_gold`；装备通过 `P.add_item` 的扩展版本加入 `P.items`。
- 旧固定掉落（药水/任务物）保留，以免现有任务链断裂。

### 3.5 背包与装备

- `P.inv`：药水、任务物、铜钱等堆叠物。
- `P.items`：装备实例数组。
- `P.equip = { weapon = <uid>, armor = <uid>, accessory = <uid> }`。
- `P.stats()` 汇总基础属性 + 等级 + 已装备 `computed`。
- 背包 UI 用 Tab 打开，显示格子、稀有度颜色、词缀行；按 J 装备/替换，按 K 丢弃，左右切换物品。
- 装备变化后立即重算 `P.stats()`，无需重新读档。

## 4. 战斗与成长

### 4.1 玩家属性

```text
hp / hp_max
mp / mp_max
stam / stam_max
atk / def
level / exp / exp_next
crit_chance / crit_damage
move_speed
life_steal
magic_find / gold_find
```

等级：1-30。每级：
- `hp_max += 16`，`mp_max += 8`，`atk += 3`，`def += 2`（沿用现有曲线）。
- 技能按等级里程碑强化，不单独做属性点分配。

### 4.2 技能

保留现有：
- 三段连招 `combo1/2/3`：J 键，第三段高伤、击退、顿帧。
- `dodge`：K 键，消耗体力，短无敌帧。
- `fenhua` 分花拂柳：U 键，AoE。
- `zixia` 紫霞真气：I 键，治疗 + 攻击增益。

R4 新增：
- `tayun` 踏云步：L 键，短冲刺，低体力消耗，可穿过小怪不穿墙。
- `wanjian` 万剑归宗：O 键，等级 10 解锁，高内力高伤害，多段 AoE。
- `jianyi` 剑意：等级 15 解锁被动，暴击率 +10%、暴击伤害 +25%。

技能伤害/治疗/冷却全部通过 `data.lua` 表驱动；每级里程碑自动升级，不引入新的属性点 UI。

### 4.3 敌人 rank 与词缀

| rank | 表现 | 强化 |
|---|---|---|
| normal | 原色 | 无 |
| elite | 蓝色描边/更亮 emissive | 血量 x1.8，伤害 x1.25，额外掉落 |
| champion | 黄色描边/更大接触阴影 | 血量 x3.0，伤害 x1.6，必掉魔法+ |
| boss | 独立 atlas/体型 | 三阶段，专属技能 |

敌人词缀（R4）：
- `swift` 迅捷：移速 +35%
- `brutal` 狂暴：攻击 +40%
- `iron` 铁壁：防御 +60%
- `leech` 汲魂：命中回复生命
- `thunder` 雷罚：攻击附带短 AoE

### 4.4 Boss 设计

- `blood_blade` 寨主血刀：
  - P1 100-66%：扇形斩、冲刺斩。
  - P2 66-33%：召唤 2 个 bandit，狂暴攻速。
  - P3 33-0%：大范围血刀旋风，预留闪避窗口。
- `youhuang_lord` 幽篁剑主：
  - P1：三道剑气波，需侧向闪避。
  - P2：雾隐突进 + 幽灵召唤。
  - P3：全屏雾 + 落剑，必须依靠灯笼/受光找安全区。
- Boss 战必须可被 headless 脚本触发；R5/R6 用状态机断言各阶段。

### 4.5 难度与重复刷

- 每张地图有 `monster_level` 基准；每次进入地图根据角色等级轻微缩放，避免低等级图完全无意义。
- 精英/冠军随机刷新，提供反复刷的动力。
- 地图出口旁有简短提示当前地图等级/推荐等级/天气。
- 不新增联网、不新增多人。

## 5. 任务、NPC 与存档

### 5.1 主线

1. `q_start`：在 village 与 elder 对话。
2. `q_rescue`：在 stronghold 击杀 5 个敌人。
3. `q_boss`：击杀 `blood_blade`，获得 `boss_token`。
4. `q_valley`：与 elder/villager 对话后解锁 stronghold 北门。
5. `q_youhuang`：击杀 `youhuang_lord`，完成主线。
6. 可选：R5 增加悬赏：清剿幽篁秘谷精英 x10，可重复完成。

### 5.2 存档

扩展现有 `save.lua`，继续使用真实绑定 `dse.serialize.encode/decode`（`lua_binding_serialize.cpp`）：

```lua
{
  ver = 2,
  run_seed = 123456,
  map_id = "stronghold",
  px = 21.5, py = 26.5,
  level = 8, exp = 120, gold = 830,
  items = { ... item instances ... },
  equip = { weapon = 10001, armor = 10002, accessory = nil },
  inv = { hp_potion = 5, mp_potion = 3, boss_token = 1 },
  skills = { wanjian = 2, tayun = 1 },
  quests = { q_start = true, q_rescue = true, q_boss = true, q_valley = true },
  weather = "storm",
  defeated_bosses = { blood_blade = true },
}
```

读档后：
- 重新计算 `P.stats()`。
- 重新生成天气粒子与地图灯光。
- 校验物品 `uid` 唯一与装备槽合法；损坏则拒绝该存档字段，不崩溃。

## 6. HD-2D 渲染、光影与天气

### 6.1 表现层

- B+ 表现层：`templates/hd2d_wuxia/scripts/bplus.lua`。
- 地形：`gen_maps.py` 生成 `*_3d.dmesh`，运行时用 `ecs.mesh_renderer_add` + `ecs.set_mesh_shader_variant(terrain, "MESH_LIT")` 加载。
- 角色/敌人/NPC：`ecs.add_sprite3d` + `ecs.set_sprite3d_lit(true)` + `ecs.set_sprite3d_receive_shadow(true)` + `ecs.set_sprite3d_contact_shadow(...)`。
- 动画：`.dsprite.json` -> `dse.assets.load_sprite_atlas` -> `ecs.set_sprite3d_atlas` + `ecs.set_sprite3d_anim`。
- 法线/自发光：`ecs.set_sprite3d_normal`、`ecs.set_sprite3d_emissive`；用于灯笼、鬼火、剑气、Boss 眼睛等 Bloom tap。
- 灯光：`ecs.add_point_light_3d`；方向月光/日光 `ecs.add_directional_light_3d`。
- 相机：`ecs.add_camera_3d`、`ecs.set_camera_ortho_3d`、`ecs.camera_set_bounds`、`camera_shake`、`camera_set_zoom`。
- 后处理：Bloom、tilt-shift、vignette、film grain、exposure、color grading、FXAA，沿用 `bplus.lua` 现有路径。

### 6.2 默认开关

- R3 起，WUXIA 模板默认启用 HD-2D 表现层；设置 `DSE_HD2D_BPLUS=0` 可回退 2D 逻辑用于兼容测试。
- 所有证据场景必须显式写明 BPLUS on/off，避免把 2D 回退误当 HD-2D 通过。

### 6.3 天气系统

天气使用 Lua 表驱动 + 现有 FX/Sprite3D 路径实现，不新增引擎绑定：

| 天气 | 粒子 | 环境 | 光照 | 后处理 |
|---|---|---|---|---|
| `clear` | 无 | 暖色 | 正常 | 正常 |
| `leaf` | 落叶 | 青橙 | 暖点光 | 轻微暗角 |
| `light_rain` | 雨线 | 灰蓝 | 方向光减弱 | exposure -0.05 |
| `rain` | 雨线 + 溅射 | 冷蓝 | 灯笼对比增强 | 对比 +，Bloom 略降 |
| `storm` | 大雨 + 闪电 | 深蓝紫 | 闪电瞬间点光/曝光 | 闪电帧 exposure 脉冲 |
| `fog_firefly` | 雾片 + 萤火 | 青绿 | 冷鬼火 + 月光 | 远景降对比、Bloom 略升 |
| `snow` | 雪花 | 冷白 | 环境提亮 | 白平衡偏冷 |

- 天气切换 2-4 秒内插值；进入地图时先设 base weather，再按随机表选当前天气。
- 雨/雾/雪粒子由 `gen_lib.py` / `generate_assets.py` 生成序列帧，再经 `gen_atlases.py` 打包；R5 同步更新资产台账。
- 闪电只允许短时曝光/点光脉冲，不能永久改变场景。

## 7. UI / UX

- HUD：生命、内力、体力、等级、经验条、金币、当前地图/天气、技能栏。
- 技能栏：J/K/U/I/L/O 六槽；冷却数字；不可用置灰。
- 背包/装备：Tab；格子 + 物品详情 + 词缀列表 + 已装备对比。
- 掉落提示：稀有度颜色 + 名称 + 词缀摘要；Boss 暗金/传奇使用特殊音效。
- 任务追踪：右侧当前主线/悬赏、进度 `x/y`。
- 伤害数字/暴击：沿用现有 `fx.lua` 飘字，按稀有度/暴击分色。
- 地图/天气提示：进入地图时显示标题 + 推荐等级 + 天气。

## 8. API 可追溯表（禁止臆造）

| 设计能力 | 真实 API | 证据 |
|---|---|---|
| 3D 精灵 | `ecs.add_sprite3d` | `lua_binding_compat.cpp:503` |
| 受光精灵 | `ecs.set_sprite3d_lit` | `lua_binding_compat.cpp:506` |
| 接地阴影 | `ecs.set_sprite3d_contact_shadow` | `lua_binding_compat.cpp:509` |
| 法线 | `ecs.set_sprite3d_normal` | `lua_binding_compat.cpp:508` |
| 自发光 | `ecs.set_sprite3d_emissive` | `lua_binding_compat.cpp:516` |
| 图集 | `ecs.set_sprite3d_atlas` / `assets.load_sprite_atlas` | `lua_binding_compat.cpp:510,512` |
| 精灵动画 | `ecs.set_sprite3d_anim` | `lua_binding_compat.cpp:511` |
| 正交相机 | `ecs.set_camera_ortho_3d` | `lua_binding_compat.cpp:513` |
| 3D 相机 | `ecs.add_camera_3d` | `lua_binding_free_ecs_camera.gen.cpp:135` |
| 相机边界/震屏/缩放 | `camera_set_bounds` / `camera_shake` / `camera_set_zoom` | `lua_binding_free_ecs_2d.gen.cpp:333-335` |
| 点光/方向光 | `ecs.add_point_light_3d` / `ecs.add_directional_light_3d` | `lua_binding_free_ecs_light.gen.cpp:228,232` |
| 地形网格 | `ecs.mesh_renderer_add` | `lua_binding_free_ecs_gap.gen.cpp:649` |
| MESH_LIT | `ecs.set_mesh_shader_variant` | `lua_binding_ecs_mesh_renderer.gen.cpp:140` |
| 地形材质 | `ecs.set_mesh_material` | `lua_binding_free_ecs_mesh.gen.cpp:157` |
| 景深/移轴 | `ecs.set_post_process_tilt_shift` | `lua_binding_compat.cpp:514` |
| Bloom | `ecs.set_post_process_bloom_*` | `lua_binding_ecs_post_process.gen.cpp:762` 起 |
| 曝光 | `ecs.set_post_process_exposure` | `lua_binding_ecs_post_process.gen.cpp:774` |
| 天气组件（可选） | `ecs.add_weather` / `ecs.set_weather` | `lua_binding_free_ecs_gameplay3d.gen.cpp:636-637`；主方案仍用 Lua FX 以保持 2D 兼容 |
| 存档序列化 | `dse.serialize.encode/decode` | `lua_binding_serialize.cpp:13-14` |
| 音频 | `dse.audio.play_sfx` / `play_sfx_random` | `lua_binding_free_audio_full.gen.cpp:318,320` |
| 输入/时间 | `dse.app.get_key` / `get_key_down` / `time_since_startup` | `lua_binding_core.cpp:294,295,303` |

## 9. 测试与验收矩阵

| 测试 ID | 轮次 | 命令/脚本 | 通过标准 |
|---|---|---|---|
| T-BUILD | 每轮 | `cmake --build --preset windows-x64-debug` | exit=0 |
| T-GTEST | 每轮 | `ctest --preset windows-x64-debug` | 0 failed |
| T-R3-GAME | R3 | 新增 `_wuxia_r3_acceptance.lua`，三后端 headless | exit=0；日志含移动/攻击/掉装/升级/存档标记；截图存在 |
| T-R3-LIT | R3 | `_sprite3d_lit_test.lua` + 实际地图 lit on/off | 三后端 exit=0；lit-on `mean_luma` 比 lit-off 高 2.0；截图非空 |
| T-R4-LOOT | R4 | 新增 `_wuxia_loot_test.lua` 固定 seed | 稀有度/词缀数/装备属性/存档 roundtrip 全部断言通过 |
| T-R4-COMBAT | R4 | 新增 `_wuxia_combat_test.lua` | 三段连招、闪避、技能、精英/Boss 阶段状态机通过 |
| T-R5-MAPS | R5 | 新增 `_wuxia_maps_test.lua` | 3 图均可进入，出口连通，天气/灯光/敌人存在 |
| T-R5-WEATHER | R5 | `_wuxia_weather_test.lua` + 像素统计 | 至少 3 种天气状态；粒子/光照/后处理数值有差异 |
| T-R6-M6 | R6 | `python templates/hd2d_wuxia/tools/run_hd2d_m6_acceptance.py --backends opengl,vulkan,d3d11` | 三后端 exit=0；M6 验收图/统计通过 |
| T-R6-FULL | R6 | 新增 `run_wuxia_acceptance.py` | 3 图  3 后端；读档/通关/无缺失资源；资产台账完整 |

像素统计使用现有 `templates/hd2d_wuxia/tools/hd2d_pixel_stats.py`。新增断言不得比现有 M3/M6 测试更弱。

## 10. 实施映射

| 轮次 | 实现内容 |
|---|---|
| R3 | 1 张 village 切片：默认 B+、随机装备原型、掉落/拾取/升级/存档、三后端验收脚本 |
| R4 | 完整 loot/词缀/稀有度/背包/装备/技能/Boss 阶段与自动化测试 |
| R5 | `youhuang` 与第二张现有图纳入验收、3 图连通、天气/光影打磨、资产台账追加 |
| R6 | 全量验收、性能/内存、文档交付、push |

## 11. 非目标

- 不做联网/多人。
- 不新增第三方库、submodule、vcpkg、npm 依赖。
- 不新增 RHI 级渲染特性；如必须改渲染，三后端同步并重新生成 shader，不手改 `*.gen.h`。
- 不使用任何商业游戏解包素材（含《逸剑风云决》）。
- 不做开放世界、不做无缝大地图、不做服务器存档。
- 不为了让测试变绿而放宽断言或跳过既有测试。