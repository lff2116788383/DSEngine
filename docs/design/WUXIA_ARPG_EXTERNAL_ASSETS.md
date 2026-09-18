# 外部 CC0 素材集成说明（Ninja Adventure Asset Pack）

## 来源与许可

- 素材包：Ninja Adventure Asset Pack
- 作者：Pixel-Boy / AAA
- 发布页：https://pixel-boy.itch.io/ninja-adventure-asset-pack
- 镜像页：https://opengameart.org/content/ninja-adventure-asset-pack
- 下载镜像：https://github.com/pixel-boy/NinjaAdventure
- 许可：CC0 1.0（素材包作者公开发布为 CC0；GitHub 镜像无单独 LICENSE 文件）

## 已导入内容

运行时优先路径：`games/wuxia_arpg/assets/external/ninja_adventure/`

- 角色图集：
  - `hero`  `samurai_blue/sprite.png`
  - `bandit`  `samurai_green/samurai_green.png`
  - `boss`  `ninja_blue/sprite.png`
- 图集规格：
  - 原图 64112，按 1616 切 4 列  7 行
  - 列顺序：down / up / left / right
  - 行：idle / walk1-3 / attack / jump / die-hurt
  - 重新打包为 11 帧、4 方向的 `.dsprite.json`，clip：`idle / walk / attack / dodge / hurt / die`
- 音乐：
  - `theme_plain.ogg`：青溪村
  - `theme_swamp.ogg`：黑风寨
  - `theme_lost_village.ogg`：幽篁秘谷
  - `theme_dream.ogg`：备用

## 重新导入

```powershell
# 1) 解压 Ninja Adventure 到任意目录
# 2) 指定源目录
python games\wuxia_arpg\tools\import_ninja_adventure.py --source <解压目录>

# 3) 重新生成资产台账
python games\wuxia_arpg\tools\gen_ledger.py
```

`assets.lua` 会优先加载 `assets/external/ninja_adventure/actor/*.dsprite.json`；缺失时回退到程序化生成图集。
`main.lua` 按地图播放对应 OGG BGM。

## 合规

- 不使用商业游戏（含《逸剑风云决》）解包素材。
- 不引入第三方代码库；仅导入 CC0 美术/音频。
- 新资产台账：`docs/design/WUXIA_ARPG_NEW_ASSET_LEDGER.md` / `.csv`，当前 98 条。