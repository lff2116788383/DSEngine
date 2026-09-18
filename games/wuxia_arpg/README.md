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

## 许可

新生成素材与脚本为 DSEngine Authors 原创，随仓库顶层 `LICENSE` 以 Apache-2.0 发布。
字体图集输入为 `apps/editor_cpp/fonts/NotoSansSC-Regular.ttf`（Google Noto CJK，SIL OFL 1.1）。
禁止使用任何商业游戏解包素材。