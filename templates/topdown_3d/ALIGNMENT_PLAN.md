# topdown_3d 模板对齐 Unity 逆向源码 — 方案文档

> 状态：待审阅
> 范围：`templates/topdown_3d/scripts/`（14 个 Lua 模块）对齐
> `Desktop/desktop/逆向/asset_output/Scripts/Assembly-CSharp/`（约 250 个 C# 文件）
> 版本：v1（初稿）

---

## 一、目标与边界

**目标**：把当前 Lua 模板与 Unity 逆向源码的差异收敛到可控范围，最终做到"玩法可跑通、数值对齐、无死代码"。

**边界与原则**：
- 只对齐**游戏逻辑与数据**；Unity 专用技术（Prefab/物理触发器/动画事件/协程/图集）按 DSEngine 能力等价替换，不 1:1 照搬。
- 渲染/相机参数（距离、FOV、震屏幅度）因引擎世界尺度不同已按比例放大，属**有意适配**，不回调。
- 付费/渠道（CmBillingAndroid/ChannelMgr/HttpMsgManager 等 Android 原生依赖）桌面端无价值，标记为"不做"。
- 每一步改动独立提交、独立验证，不做超大合并提交。

---

## 二、差异总览（依据 4 份逐文件对比）

### A. 结构性缺口（影响最大）

| # | 问题 | 严重度 | 证据 |
|---|------|:------:|------|
| A1 | `ui_system.lua` 的 `update` 未接入 main.lua 主循环；暂停/复活/结算/天使通知全部不可达 | 高 | main.lua 仅调 `UISystem.build/clear/show_chance` + 回调注册 |
| A2 | `ef_system.lua` 仅 require、零调用；25 个特效全部"代码在、运行不生效" | 高 | grep `EfSystem.` 零命中 |
| A3 | `DB_Stage` 18-89 关数据简化：Lua 公式生成 vs C# 手写真实数据；reward 全错 | 高 | database.lua `for i=30,89` 生成 |
| A4 | 玩家高级动作缺失：冲刺/双击闪避/抓取冲锋/7 种抓取/蓄力/追击QTE/升龙/超级模式/宠物变身/骑乘/剧情移动/控制锁 | 高 | Cha_Control.cs 对应状态无 Lua 实现 |

### B. 数据表差异

| 表 | C# 规模 | Lua 现状 | 结论 |
|----|:-------:|----------|------|
| DB_Skill | 20×5 | 100 条全对齐 | 完全一致 |
| DB_Monster | 16 | 16 条全对齐 | 完全一致 |
| DB_Boss | 12 | 12 条全对齐 | 完全一致 |
| DB_Stage | 90 关手写 | 18-89 公式化 | 需重写 |
| DB_Weapon | 26 把武器数值表 | 6 种武器类型表（重新设计） | 需决定：补全数值 or 保留自定义 |
| DB_General | 30 将数值表 | 8 将简化表 | 同上 |
| DB_angel / DB_PetSkill | 8 / 2×10 | 完整（在 pet_system.lua） | 完全一致 |
| DB_Costume / DB_EquipItem / DB_Scenario / DB_Archive | 17 / 6×11 / 91×14 / 76 | 无 | 未移植（关联功能未移植，暂缓） |

### C. 参数不一致（具体可修）

见附录一完整清单。重点 5 项：
1. `general_meteo` 落点：C# `up*4.2 - forward*5.5`，Lua `x = Player.x + 4.2 - fx*5.5`（4.2 误加到 x 轴，疑似笔误）
2. Boss 召唤怪等级 `stage_index + 5` vs C# `cur_stage_index`
3. 初始怪数量：BuildStage 先放 `min(mainmon,5)` 又刷 3 只，C# 首批仅 3 只
4. 闪避 SP：C# `weaponweight*2=4`，Lua 10
5. 弓/法杖/法器升龙标志 `attack_rising` 漏设

### D. 整体未移植功能域（按优先级）

| 域 | 内容 | 优先级 |
|----|------|:------:|
| 存档系统 | Crypto/DataSave/ConvertSaveData/PlayerPrefsX/TimeControl | 高 |
| 游戏外 UI | UI_intro/UI_map/UI_skill/UI_forge/UI_status/UI_general/UI_archive/UI_result/UI_cashshop | 高 |
| 特殊关卡 | Map_Compose/Cart/Tank/Tower/DunGate/Dun_Snake/Chosun/Bamboo/Flower_cannon | 高 |
| 语言数据 | Language/Language_Name/Language_Costume/Language_Scenario | 高(机械搬移) |
| 剧情系统 | DB_Scenario/Scenario/Story_trans/UI_Ingame_story/Ending | 中 |
| 特殊演出体 | DeathHand/Dragonhead/Mirage/Mon_Destroy/Itemdrop/Souldrop/Hp_bar_fixed/Footstep | 中 |
| 网络排行 | RankingManager/HttpMsgManager | 中(可选) |
| 付费/渠道 | CmBillingAndroid/UI_cashshop 原生部分 | 不做 |
| 音效 | SoundEf_UI/SoundEf_slash | 低 |
| 工具/JSON | CommonUtils/JSONObject/MiniJSON/LitJson | 低(用内置替代) |

---

## 三、分阶段实施计划

每阶段：目标 → 改动 → 涉及文件 → 验证方式。每阶段独立提交。

### Phase 0：接线（让已有代码生效）

**目标**：激活 ui_system.lua 与 ef_system.lua。

- 在 main.lua `Update` 中接入 `UISystem.update(dt)` 与 `EfSystem.update(dt)`（含各自 clear）。
- 接入 `UISystem.handle_esc`（ESC 键 → 暂停菜单）与死亡复活流程联动。
- 检查 UISystem 内对 `DB.DB_Weapon/DB.DB_General` 的取值是否越界（现为 6/8 条自定义表）。

**验证**：运行游戏 → 血条/连击/蓄力条实时刷新；暂停菜单可开可关；击杀怪出现分裂/血溅特效；结算界面可弹出。

**风险**：接线后可能暴露 UI 代码自身 bug（构建/布局坐标），需要滚动修。

### Phase 1：数据对齐

**目标**：DB_Stage 与 C# 完全一致；修正疑似笔误。

1. **DB_Stage 18-89 重写**：从 `DB_Stage.cs` 逐关搬移 `basemon1/basemon2/mainmon/bosscount/boss1-3/reward` 真实数据（C# 已解析，直接翻译为 Lua 表，不写生成公式）。
2. **数值笔误修正**（附录一表 A 项）：
   - `general_meteo` 落点 x 轴去掉 `+4.2`
   - Boss 召唤怪等级去掉 `+5`
   - 初始怪数量改回首批 3 只
   - 弓/法杖/法器补 `attack_rising`
   - 闪避 SP 改 `weaponweight*2`、抓取补回血、GetItem 回血改 `(100+level-1)`
   - Sk_machinegun/Sk_flybug 首射时机、magicmissile 收缩、spear 初始位移、spear_Dash 收束
3. **DB_Weapon/DB_General 决策**（需要与用户确认，见第四节决策项）：
   - 方案 X：从 C# 补全 26/30 条完整数值表，替换自定义 6/8 条 → 还原度最高，但需同步改所有引用处
   - 方案 Y：保留自定义表，仅补齐缺失字段 → 改动小

**验证**：随机抽 20 关对照 `DB_Stage.cs`；运行第 20/35/50/80 关确认波次与 Boss 配置符合预期。

### Phase 2：玩家核心动作补齐（玩法手感）

**目标**：补齐 Cha_Control 中缺失的高频动作。

按依赖顺序：
1. 冲刺态（跑 3s → chamovestat=3 + 残影）+ 冲刺攻击（攻击 -1 路径）
2. 双击闪避（dubbleclick 计时）替换单键 L
3. 抓取系统：7 种抓取动画按怪物 kind/sizekind 选择 + 抓取回 HP + 吸附跟随
4. 蓄力攻击 Exstart/Eximpact + 引导圈
5. 无敌联动 hitrate=200、受击后 rigidbody.mass 语义（用击退距离替代）
6. 追击 QTE（attackex1）、升龙（riseattack）—— 若时间有限可后移

**验证**：操作手感逐项与 C# 逻辑对照；连击/受击/抓取回血数值验证。

### Phase 3：单机游戏循环（存档 + 游戏外 UI）

**目标**：金币/技能/武器/关卡进度可持久化，有主菜单 → 地图 → 战斗 → 结算闭环。

1. 存档层：`Crypto` 等价（DSEngine 若已有存档 API 则接入，否则轻量 AES + 磁盘文件）+ `Save/Load` 键值表（金币/玉石/技能等级/武器库存/关卡进度）
2. 菜单 UI：UI_intro（主菜单）→ UI_map（90 关选择）→ 复用现有战斗 HUD
3. 结算 UI：UI_result 完整化（星级/奖励/开箱）——衔接 Phase 0 已接线的 Txt_result/Txt_star
4. UI_skill 技能商店（购买/升级/装备技能槽 20 个）

**验证**：杀怪攒金币 → 存档 → 重启读档；通关写入关卡进度 → 地图解锁下一关。

### Phase 4：特殊关卡玩法 + 剧情（可选扩展）

1. 特殊关卡：Map_Compose 组装、Cart 运粮车、Tank 攻城、Tower 守城、DunGate Boss 门
2. 剧情：DB_Scenario 数据搬移 + Scenario 演出控制 + Story_trans
3. 语言数据：Language_* 文本表搬移，替换 Lua 硬编码中文

---

## 四、需要用户决策的项

| # | 决策 | 选项 |
|---|------|------|
| D1 | DB_Weapon/DB_General 数据 | X=补全 26/30 完整表（还原度高）；Y=保留自定义 6/8（改动小） |
| D2 | 存档实现 | 用引擎现有存档 API / 轻量自实现（AES+文件） |
| D3 | 阶段范围 | 先做 Phase 0-2（接线+数据+核心动作），还是 0-3 全做 |
| D4 | 是否现在清理"未移植功能域" | 付费/网络/剧情 标记为"本期不做"还是"后续排期" |

---

## 五、验证与交付约定

- 每阶段完成后：`git add` 相关文件 → 独立 commit（Conventional Commits）→ 推送 `feature/engine-lib`。
- 每个功能点至少一次真实运行验证（截图或日志），不依赖静态推断。
- 渲染/数值改动遵循项目规则：三后端一致、不手改 `*.gen.h`。

---

## 附录一：参数不一致明细表

| # | 项 | C# | Lua 现状 | 位置 |
|---|----|----|----------|------|
| A | general_meteo 落点 | pos+up*4.2-forward*5.5 | x=Player.x+4.2-fx*5.5（+4.2 误加 x 轴） | skill_system.lua:1143 |
| B | SpiritSword_p3 弹道 | 未归一化向量×速度 | 归一化等速 | skill_system.lua:460 |
| C | Sk_machinegun 首射 | 0.8s | 0.6s | skill_system.lua |
| D | Sk_flybug 首射 | 0.5s 延迟 | 0s | skill_system.lua |
| E | magicmissile 收缩 | 全轴 1.5/s | 仅 x/z 0.75/s | bullet_system.lua:278 |
| F | Bullet_lightning | 无失效 | +0.5s 生命周期 | bullet_system.lua |
| G | 闪避 SP | weaponweight*2=4 | 10 | main.lua |
| H | 召唤武将 SP | 离场 -10 | -100 | main.lua |
| I | 武将 dash/runspeed | 存档传入 | 写死 80/0.3 | ai_system.lua:55-57 |
| J | 天使 speed/firerate | DB_angel | 写死 0.3/1.0 | ai_system.lua:429 |
| K | Boss 召唤怪等级 | cur_stage_index | stage_index+5 | main.lua:3724 |
| L | 初始怪数量 | 首批 3 只 | min(mainmon,5)+3 | main.lua:3615 |
| M | 弓/法杖/法器 attack_rising | 3/4/5 段置位 | 漏设 | main.lua |
| N | 抓取回血 | Heal(maxhp*0.1) | 无 | main.lua |
| O | GetItem 回血 | (100+level-1)*0.1 | (100+level)*0.1 | main.lua:818 |
| P | 无敌联动 | hitrate=200 | 不联动 | main.lua |
| Q | 敌人命中玩家 | 武器朝向击退 | 固定 180 击退 | main.lua |

## 附录二：关键文件对照表

| Lua 模块 | 对应 C#（已移植） |
|----------|-------------------|
| main.lua | Cha_Control / AI_Enemy01 / AI_Boss01 / Spawn / AI_General / AI_Asist |
| skill_system.lua | Cha_Skill / SwordDance / SpiritSword(1-3,p1-3) / Sk_×6 / Junwui |
| bullet_system.lua | 31 × Bullet_*.cs |
| ai_system.lua | AI_General / AI_Asist |
| ui_system.lua | UI_Ingame / UI_Ingame_GUI / Icon_Skill / Gauge_UV / MakeUI / Txt_result / Txt_star |
| cam_move.lua | Cam_Move |
| monster_efs.lua | Monster_efs / Hp_bar / WeaponDrop / DamageNum / Shadow |
| ef_system.lua | 25 × Ef_*.cs |
| pet_system.lua | Pet_eagle / Pet_horse / Cha_Control_ride_* / AI_Ride_* / Cam_Move_ride / UI_pet 等 19 个 |
| cutin01.lua | Cutin01 |
| weapon_damage.lua | WeaponDamage / AI_Boss01 武器部分 |
| database.lua | DB_Skill / DB_Monster / DB_Boss / DB_Stage |
| model_scale.lua | （工具，AssetBuilder 生成） |
| state.lua | （共享状态，非 C# 直接对应） |
