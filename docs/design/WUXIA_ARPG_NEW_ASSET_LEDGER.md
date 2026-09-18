# 青溪问剑（新游戏）资产许可台账

> R3 新游戏工程 `games/wuxia_arpg/` 的逐条资产/源码记录。
> 本工程**不使用** `templates/hd2d_wuxia` 的代码或素材。
> 权威逐条表：本 Markdown；含 SHA-256 的 CSV：`docs/design/WUXIA_ARPG_NEW_ASSET_LEDGER.csv`。

## 外部输入

| 输入 | 路径 | 作者/来源 | 许可 | 说明 |
|---|---|---|---|---|
| Noto Sans SC Regular | `apps/editor_cpp/fonts/NotoSansSC-Regular.ttf` | Google Noto CJK contributors | SIL OFL 1.1 | 仅用于生成新字体图集 |
| Pillow | 生成期环境（不随仓库提交） | Python Pillow contributors | HPND License | 仅素材生成/像素统计，不进入运行时 |

## 生成命令

- `G1`：`python games/wuxia_arpg/tools/gen_assets.py`
- `HAND`：仓库手写源码/文档

## 逐条清单

| # | 相对路径 | 类型 | 来源 | 作者 | 许可 | 命令 |
|---:|---|---|---|---|---|---|
| 1 | games/wuxia_arpg/.gitignore | 文档/仓库配置 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 2 | games/wuxia_arpg/assets/actor/bandit_d_atlas.dsprite.json | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 3 | games/wuxia_arpg/assets/actor/bandit_d_atlas.png | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 4 | games/wuxia_arpg/assets/actor/bandit_l_atlas.dsprite.json | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 5 | games/wuxia_arpg/assets/actor/bandit_l_atlas.png | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 6 | games/wuxia_arpg/assets/actor/bandit_r_atlas.dsprite.json | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 7 | games/wuxia_arpg/assets/actor/bandit_r_atlas.png | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 8 | games/wuxia_arpg/assets/actor/bandit_u_atlas.dsprite.json | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 9 | games/wuxia_arpg/assets/actor/bandit_u_atlas.png | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 10 | games/wuxia_arpg/assets/actor/hero_d_atlas.dsprite.json | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 11 | games/wuxia_arpg/assets/actor/hero_d_atlas.png | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 12 | games/wuxia_arpg/assets/actor/hero_l_atlas.dsprite.json | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 13 | games/wuxia_arpg/assets/actor/hero_l_atlas.png | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 14 | games/wuxia_arpg/assets/actor/hero_r_atlas.dsprite.json | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 15 | games/wuxia_arpg/assets/actor/hero_r_atlas.png | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 16 | games/wuxia_arpg/assets/actor/hero_u_atlas.dsprite.json | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 17 | games/wuxia_arpg/assets/actor/hero_u_atlas.png | 新生成角色图集 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 18 | games/wuxia_arpg/assets/audio/bgm_loop.wav | 新生成音频 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 19 | games/wuxia_arpg/assets/audio/sfx_click.wav | 新生成音频 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 20 | games/wuxia_arpg/assets/audio/sfx_hit.wav | 新生成音频 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 21 | games/wuxia_arpg/assets/audio/sfx_levelup.wav | 新生成音频 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 22 | games/wuxia_arpg/assets/audio/sfx_pickup.wav | 新生成音频 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 23 | games/wuxia_arpg/assets/audio/sfx_slash.wav | 新生成音频 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 24 | games/wuxia_arpg/assets/props/bamboo.png | 新生成 3D 道具贴图 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 25 | games/wuxia_arpg/assets/props/house.png | 新生成 3D 道具贴图 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 26 | games/wuxia_arpg/assets/props/lantern.png | 新生成 3D 道具贴图 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 27 | games/wuxia_arpg/assets/props/rock.png | 新生成 3D 道具贴图 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 28 | games/wuxia_arpg/assets/props/tree.png | 新生成 3D 道具贴图 | gen_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 29 | games/wuxia_arpg/assets/ui/font.png | 新生成字体图集/度量 | gen_assets.py + NotoSansSC-Regular.ttf | DSEngine Authors / Noto CJK contributors | Apache-2.0 (生成输出) + SIL OFL 1.1 (字形来源) | G1 |
| 30 | games/wuxia_arpg/README.md | 文档/仓库配置 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 31 | games/wuxia_arpg/scripts/_logic_test.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 32 | games/wuxia_arpg/scripts/assets.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 33 | games/wuxia_arpg/scripts/core.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 34 | games/wuxia_arpg/scripts/data.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 35 | games/wuxia_arpg/scripts/enemy.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 36 | games/wuxia_arpg/scripts/font.lua | 新生成字体图集/度量 | gen_assets.py + NotoSansSC-Regular.ttf | DSEngine Authors / Noto CJK contributors | Apache-2.0 (生成输出) + SIL OFL 1.1 (字形来源) | G1 |
| 37 | games/wuxia_arpg/scripts/fx.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 38 | games/wuxia_arpg/scripts/loot.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 39 | games/wuxia_arpg/scripts/main.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 40 | games/wuxia_arpg/scripts/player.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 41 | games/wuxia_arpg/scripts/rng.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 42 | games/wuxia_arpg/scripts/save.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 43 | games/wuxia_arpg/scripts/terrain.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 44 | games/wuxia_arpg/scripts/ui.lua | 新游戏 Lua 源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 45 | games/wuxia_arpg/tools/gen_assets.py | 生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 46 | games/wuxia_arpg/tools/pixel_stats.py | 生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |
| 47 | games/wuxia_arpg/tools/run_r3_acceptance.py | 生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | HAND |

## 禁止项

- 禁止任何商业游戏（含《逸剑风云决》）解包素材进入本工程。
- 禁止把 `templates/hd2d_wuxia` 的代码/素材复制进本工程。
