# topdown_3d 对齐复刻 — 新会话交接指令

> 用途：给新会话（无历史上下文）继续完成 Unity 逆向游戏移植任务。
> 启动后先读本文件 + `templates/topdown_3d/ALIGNMENT_PLAN.md`，即可无缝接手。
> 分支：`feature/engine-lib`（已推送，工作区干净）

---

## 一、项目背景速览

- **目标**：把 Unity 逆向游戏「亡灵杀手」完整移植到 DSEngine Lua 模板 `templates/topdown_3d/`。
- **逆向 C# 源**：`Desktop\desktop\逆向\asset_output\Scripts\Assembly-CSharp\`（约 250 个 C# 文件，勿提交，在仓库外）。
- **Lua 模板**：`c:\Users\wenbilin\Desktop\Engine\DSEngine\templates\topdown_3d\scripts\`（14 个模块）。
- **模块 ↔ C# 映射**见方案文档附录二。
- **运行验证方式**（最后统一调试用）：
  ```
  $env:DSE_STARTUP_LUA = "c:\...\templates\topdown_3d\scripts\main.lua"
  $env:DSE_DATA_ROOT   = "c:\...\templates\topdown_3d"
  $env:DSE_MAX_FRAMES  = "90"
  $env:DSE_SCREENSHOT_PATH = "c:\...\templates\topdown_3d\verify.png"
  $env:DSE_SCREENSHOT_TARGET = "main"
  & c:\...\DSEngine\bin\dsengine_game_debug.exe
  ```
  Lua 运行错误会打 `Lua Update failed`；Lua 的 print 输出到 stdout。

---

## 二、已完成（提交记录，勿重复做）

| 提交 | 内容 |
|------|------|
| cd2c5218 | docs: 对齐方案文档 `ALIGNMENT_PLAN.md` |
| b6269a62 | Phase 0-1：UISystem/EfSystem 接入 main.lua 主循环 + ESC 暂停 toggle + 删除旧纯文本 HUD；DB_Stage 90 关真实数据；11 处数值笔误修正 |
| d4c02f77 | Phase 1c：`DB_WeaponShop`(26 把) + `DB_GeneralPool`(30 将) C# 完整数据 |
| 0a9278c5 | Phase 2：冲刺态(3s+残影)、双击闪避、抓取回血、无敌联动 hitrate=200 |

数据对齐已脚本校验：DB_Stage 90/90、DB_WeaponShop 26/26、DB_GeneralPool 30/30。

**注意**：运行 `dsengine_game_debug.exe` 需先确保构建的是最新代码；本仓库的 Lua 脚本为运行时加载（热重载），改 Lua 不需要重编译引擎，直接重跑 exe 即可。

---

## 三、剩余任务（按优先级）

### Task A：Phase 3 存档系统（高）
- C# 参考：`Crypto.cs`（AES 加密 Save/Load_int_key）、`DataSave.cs`、`PlayerPrefsX.cs`、`ConvertSaveData.cs`、`TimeControl.cs`。
- 目标：金币/玉石/技能等级/武器库存/关卡进度可持久化。DSEngine 是否有存档 API 需先查（`dse.assets` / `dse.app` 绑定）；若无，用 `io.open` 写本地文件 + 轻量异或/Base64 混淆（不必 1:1 复刻 AES）。
- 新建 `scripts/save.lua`，导出 `save_all() / load_all()`，在 main.lua 的 `RestartGame`/`AdvanceStage` 和 `Awake` 里接入。
- 存哪些键：`G.coin / G.jade / G.soul / Player.level / Player.exp / Player.skill_grades / Player.weapon_kind / G.stage_index`。

### Task B：Phase 3 游戏外 UI（高，工作量最大）
- C# 参考：`UI_intro.cs`（主菜单）、`UI_map.cs`（90 关地图）、`UI_skill.cs`（技能商店）、`UI_status.cs`（角色状态）、`UI_result.cs`（结算，衔接已接线的 Txt_result/Txt_star）、`UI_general.cs`（武将管理）、`UI_archive.cs`（图鉴）、`UI_forge.cs`（锻造）。
- 复用 ui_system.lua 的 `make_quad/make_text/make_button` 工具（模块内 local，需导出或复制）。
- 建议顺序：主菜单（开始/继续）→ 世界地图（90 关，解锁=已通关关+1）→ 战斗 → 结算（星级/奖励/回地图）→ 技能商店。
- 战斗 HUD 已由 ui_system.lua 承担（已接线），勿重复。

### Task C：Phase 4 特殊关卡玩法（中高）
- C# 参考：`Map_Compose.cs`（关卡组装）、`Cart.cs`（运粮车）、`Tank.cs`/`Tank_Destroy.cs`（攻城）、`Tower.cs`/`Tower_base.cs`（守城）、`DunGate.cs`（Boss 门）、`Dun_Snake.cs`/`Snake_base.cs`、`Chosun.cs`、`Bamboo.cs`、`Flower_cannon.cs`、`Poisonball.cs`、`Wind_axe.cs`。
- 对应 main.lua 的 `play_kind` 分支：目前只有普通战斗关（Spawn.cs 的 play_kind 6/7 守城/攻城玩法缺失）。

### Task D：Phase 4 剧情 + 语言（中）
- 剧情：`DB_Scenario.cs`（91×14，129KB 数据）+ `scenario.cs`（演出控制）+ `Story_trans.cs` + `Language_Scenario.cs`（812 条文本）。数据量大，直接搬表 + 简化演出。
- 语言：`Language.cs`(486)/`Language_Name.cs`(106)/`Language_Costume.cs`(121)/`Language_Archive.cs`(157)，当前 Lua 硬编码中文，若保持单语可降级。

### Task E：Phase 2 后移项（低）
- 蓄力攻击 Exstart/Eximpact（ui_system 已有蓄力条 + main.lua `on_power_release` 已接，只缺触发输入：长按/双击 J）。
- 追击 QTE（attackex1，0.5-0.7s 窗口）、上升攻击 riseattack。
- 7 种抓取动画按怪物 kind/sizekind 选择（当前单流程已可用 + 回血已补）。

### Task F：最后统一调试（收尾）
- 完整跑 main.lua，逐个修复：Lua 运行错误 → 画面/HUD → 操作手感 → 数据正确性。
- 清理临时验证脚本/截图。
- 全部完成后更新 `ALIGNMENT_PLAN.md` 状态，删除/标记本交接文档。

---

## 四、关键约定与坑（务必遵守）

1. **提交**：每阶段独立 commit（Conventional Commits），推 `feature/engine-lib`；`git add` 指定文件，勿 `add -A`。
2. **不提交**：逆向 C# 源（仓库外）、临时验证脚本 `_*.lua` / `verify*.png` / `_check*.py`、`logs/`。
3. **C# 数据表搬移**：读 C# 后逐行转 Lua 表，**禁止写生成公式**（DB_Stage 教训）；搬完用 Python 正则校验（参考 `_check_stage.py` 思路，用完删）。
4. **Lua 静态检查**：每改完一个文件用 GetDiagnostics 确认无诊断错误；语法问题可 `luac -p file.lua`（depends/lua 有 lua.exe 或直接用引擎跑）。
5. **已踩坑**：
   - 引擎键码是 GLFW 码（`KEY_ESCAPE=256`），main.lua 顶部 `local KEY_*` 表，新增键码先查表。
   - `dse.ui.set_visible` 只收 number（1/0），用 `ui_set_visible` 包装。
   - `dse.ui` 层不进 `DSE_SCREENSHOT_TARGET=main` 截图，UI 验证靠运行日志 + 手动确认，或临时 print 诊断（用完删）。
   - 引擎渲染/着色器改动需重编译（且 ninja 不追踪 .gen.h 依赖，需删 obj 全量重编）；本任务仅改 Lua，无需重编译。
6. **范围克制**：付费/渠道（CmBillingAndroid 等 Android 原生）、网络排行（HttpMsgManager）标记"不做"，不要浪费时间。

---

## 五、验收标准（用户视角）
- 单机闭环：主菜单 → 地图选关 → 战斗（完整 HUD/特效/技能/宠物）→ 结算 → 存档 → 重启读档。
- 数值对齐：DB_Stage/Weapon/General 与 C# 一致（已达成，勿回归）。
- 无死代码：ui_system/ef_system 已接线并生效。
