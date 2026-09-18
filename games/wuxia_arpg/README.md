# 青溪问剑  HD-2D 武侠刷子 ARPG（新游戏 R3 垂直切片）

这是从零建立的新游戏工程，**不使用 `templates/hd2d_wuxia` 的代码或素材**。
R3 阶段交付：1 张村庄地图可玩、HD-2D 受光生效、刷怪/掉落/升级/存档最小闭环。

## 运行

```powershell
bin\dsengine_lua_debug.exe --script=games\wuxia_arpg\scripts\main.lua
```

演示/验收模式：

```powershell
$env:DSE_WUXIA_DEMO='1'
$env:DSE_WUXIA_AUTOSAVE='1'
$env:DSE_WUXIA_ACCEPT='1'
$env:DSE_MAX_FRAMES='240'
$env:DSE_SCREENSHOT_FRAME='210'
$env:DSE_SCREENSHOT_PATH="$PWD\tmp\wuxia_game.png"
bin\dsengine_lua_debug.exe --script=games\wuxia_arpg\scripts\main.lua
```

关闭 HD-2D 灯光（用于受光对照）：

```powershell
$env:DSE_WUXIA_LIGHTS='0'
```

## 操作

| 按键 | 功能 |
|---|---|
| WASD / 方向键 | 移动 |
| J | 攻击 |
| K | 闪避 |
| Esc | 预留暂停（R4） |

## 目录

- `assets/actor/`：主角与山贼的 4 向精灵图集（新生成）
- `assets/props/`：树、竹、房屋、灯笼、岩石（新生成）
- `assets/ui/font.png`：由 OFL Noto Sans SC 生成的新字体图集
- `assets/audio/`：脚本生成的新 WAV
- `scripts/`：新游戏逻辑
- `tools/gen_assets.py`：一键生成全部新素材
- `tools/run_r3_acceptance.py`：R3 三后端验收脚本

## 重新生成素材

```powershell
python games\wuxia_arpg\tools\gen_assets.py
```

## R4 战斗与成长

- J：三段连招（第三段高伤/暴击）。
- K：闪避。
- U：分花拂柳（AoE，消耗内力）。
- I：紫霞真气（治疗 + 攻击增益）。
- Tab：背包/装备面板；W/S 选择，J 装备选中物品。
- 敌人 rank：normal / elite / champion / boss。
- Boss：寨主血刀，三阶段，半血/残血会强化。
- 装备：新生成基底 + 稀有度 + 随机词缀，装备后实时改变攻击/防御/生命/内力/暴击等。
- R4 验收：

```powershell
python games\wuxia_arpg\tools\run_r4_acceptance.py --backends opengl --out-dir tmp\r4_final --max-frames 600 --shot-frame 560
python games\wuxia_arpg\tools\run_r4_acceptance.py --backends vulkan --out-dir tmp\r4_final --max-frames 600 --shot-frame 560
python games\wuxia_arpg\tools\run_r4_acceptance.py --backends d3d11 --out-dir tmp\r4_final --max-frames 600 --shot-frame 560
```
## R5 三张地图与天气

| 地图 id | 名称 | 默认天气 | 出口 |
|---|---|---|---|
| `qingxi_village` | 暮色青溪村 | 落叶 | 北 -> 黑风寨 |
| `blackwind_stronghold` | 夜雨黑风寨 | 雷暴 | 南 -> 青溪村；北 -> 幽篁秘谷 |
| `youhuang_valley` | 幽篁秘谷 | 雾 | 南 -> 黑风寨 |

- 天气：晴 / 落叶 / 雨 / 雷暴 / 雾 / 雪，含粒子、方向光、灯笼色温、曝光/Bloom 调整。
- 雷暴有闪电曝光脉冲；雾/雨/雪会改变环境色调与点光强度。
- 环境变量调试：
  - `DSE_WUXIA_MAP=blackwind_stronghold`
  - `DSE_WUXIA_WEATHER=storm`
  - `DSE_WUXIA_TOUR=1`：自动依次走完三张地图。
- R5 验收：

```powershell
python games\wuxia_arpg\tools\run_r5_acceptance.py --backends opengl,vulkan,d3d11 --out-dir tmp\r5_final
```
## 外部 CC0 素材升级

角色与 BGM 已优先使用 CC0「Ninja Adventure Asset Pack」：

- 作者：Pixel-Boy / AAA
- 许可：CC0 1.0
- 来源：https://pixel-boy.itch.io/ninja-adventure-asset-pack
- 导入：`python games\wuxia_arpg\tools\import_ninja_adventure.py --source <解压目录>`
- 运行时优先路径：`games/wuxia_arpg/assets/external/ninja_adventure/`
- 缺失时回退到程序化素材。
## 许可

新生成素材与脚本为 DSEngine Authors 原创，随仓库顶层 `LICENSE` 以 Apache-2.0 发布。
字体图集输入为 `apps/editor_cpp/fonts/NotoSansSC-Regular.ttf`（Google Noto CJK，SIL OFL 1.1）。
禁止使用任何商业游戏解包素材。