# 青溪问剑 HD-2D 武侠刷子 ARPG 最终交付报告（R6）

> 分支：`feature/hd2d-wuxia-arpg`
> 工程：`games/wuxia_arpg/`
> 状态：代码/资产/验收已完成；push 为环境暂缓（GitHub 网络不可达）
> 日期：2026-09-18

## 1. 交付内容

- 独立新游戏工程 `games/wuxia_arpg/`，不使用 `templates/hd2d_wuxia` 作为底座。
- 3 张 HD-2D 地图：
  - `qingxi_village` 暮色青溪村
  - `blackwind_stronghold` 夜雨黑风寨
  - `youhuang_valley` 幽篁秘谷
- 地图连通：
  - 青溪村北 -> 黑风寨
  - 黑风寨南 -> 青溪村
  - 黑风寨北 -> 幽篁秘谷
  - 幽篁秘谷南 -> 黑风寨
- 战斗与成长：
  - 三段连招（第三段高伤/暴击）
  - `U` 分花拂柳 AoE
  - `I` 紫霞真气治疗 + 攻击增益
  - 等级、经验、金币、装备、随机词缀、稀有度、战力比较与自动装备
  - 背包 `Tab` / `W/S` 选择 / `J` 装备
  - normal / elite / champion / boss；Boss 三阶段
- 天气与光影：
  - 晴 / 落叶 / 雨 / 雷暴 / 雾 / 雪
  - 天气粒子、方向光、灯笼点光、曝光/Bloom、雷暴闪电脉冲
- HD-2D：
  - 程序化 3D lit 地形
  - lit Sprite3D 角色/敌人
  - 接触阴影、暖点光、Bloom、tilt-shift、FXAA、暗角
- 存档：
  - `dse.serialize` 保存玩家、装备、地图 id、天气元数据
  - 读档后按 `uid` 重新链接装备槽

## 2. R6 验收命令与结果

### 2.1 基线

```powershell
cmake --build --preset windows-x64-debug
ctest --preset windows-x64-debug
```

结果：
- 构建：`已通过`，exit=0，13.8s。
- gtest：`已通过`，exit=0，51.9s，5/5，0 失败。

### 2.2 最终矩阵

```powershell
python games\wuxia_arpg\tools\run_r6_acceptance.py --backends opengl,vulkan,d3d11 --out-dir tmp\r6_final
```

结果：`已通过`，exit=0，127.2s，最终输出 `[r6] acceptance PASS`。

覆盖：
- `_r6_save_test.lua`：`[r6-save] PASS map=blackwind_stronghold weather=storm items=1`
- `_r4_logic_test.lua`：`[r4-logic] PASS`
- `_r4_combat_test.lua`：`[r4-combat] PASS`
- `_r5_maps_test.lua`：`[r5-maps] PASS maps=3 weather=落叶/雷暴/雾`
- 资产审计：`[r6] asset audit PASS`
- 三后端  4 个地图/天气用例全部 PASS，且三后端自动巡图 PASS。

### 2.3 三后端像素统计示例

| 后端/地图/天气 | mean_luma | bright_ratio |
|---|---:|---:|
| OpenGL 青溪村/落叶 | 60.50 | 0.00000 |
| OpenGL 黑风寨/雷暴 | 60.43 | 0.00000 |
| OpenGL 幽篁谷/雾 | 57.34 | 0.00000 |
| OpenGL 幽篁谷/雪 | 60.71 | 0.00400 |
| Vulkan 青溪村/落叶 | 60.39 | 0.00000 |
| Vulkan 黑风寨/雷暴 | 59.23 | 0.00000 |
| Vulkan 幽篁谷/雾 | 57.07 | 0.00000 |
| Vulkan 幽篁谷/雪 | 60.31 | 0.00263 |
| D3D11 青溪村/落叶 | 60.41 | 0.00000 |
| D3D11 黑风寨/雷暴 | 60.36 | 0.00000 |
| D3D11 幽篁谷/雾 | 57.30 | 0.00000 |
| D3D11 幽篁谷/雪 | 60.67 | 0.00400 |

截图路径：`tmp/r6_final/r5_{backend}_{map}_{weather}.png`（tmp 被忽略，统计写入本报告与 PROGRESS）。
可浏览 Gallery：`docs/design/wuxia_arpg_gallery/gallery.html`（含 contact_sheet.png、实机截图与替换前后对比图）。
截图分析：`docs/design/WUXIA_ARPG_SCREENSHOT_ANALYSIS.md`。

## 3. 资产与合规

- 逐条资产台账：`docs/design/WUXIA_ARPG_NEW_ASSET_LEDGER.md` 与 `.csv`，当前 98 条。
- 程序化地形/天气/UI 图集由 `games/wuxia_arpg/tools/gen_assets.py` 生成。\n- 角色与 BGM 使用 CC0「Ninja Adventure Asset Pack」（Pixel-Boy / AAA）导入；见 `WUXIA_ARPG_EXTERNAL_ASSETS.md`。
- 字体图集输入：`apps/editor_cpp/fonts/NotoSansSC-Regular.ttf`，SIL OFL 1.1。
- 静态审计：未发现 `SimHei`、`逸剑风云决`、商业游戏解包引用。
- 未使用 `templates/hd2d_wuxia` 的代码或素材。
- 未新增第三方库。

## 4. 运行方式

```powershell
bin\dsengine_lua_debug.exe --script=games\wuxia_arpg\scripts\main.lua
```

调试环境变量：
- `DSE_WUXIA_MAP=blackwind_stronghold`
- `DSE_WUXIA_WEATHER=storm`
- `DSE_WUXIA_TOUR=1`
- `DSE_WUXIA_DEMO=1`
- `DSE_WUXIA_AUTOSAVE=1`
- `DSE_WUXIA_LIGHTS=0`

## 5. 已知限制

- 远程 GT 1030 台式机仍不可达，未取得 GT 1030 真机证据；不得把本机 RTX 3070 结果冒充 GT 1030。
- GitHub 网络环境不可达，R5/R6 提交暂未 push；`master` 未 push。
- 发布级打包和签名未在本轮范围内。

## 5.1 CC0 外部素材升级

用户选择方案 B 后，已导入 CC0「Ninja Adventure Asset Pack」：

- 作者：Pixel-Boy / AAA
- 许可：CC0 1.0
- 来源：https://pixel-boy.itch.io/ninja-adventure-asset-pack
- 导入内容：
  - `hero`  samurai_blue
  - `bandit`  samurai_green
  - `boss`  ninja_blue
  - 4 向 1616 角色帧重切为 11 帧 `.dsprite.json`
  - `theme_plain.ogg` / `theme_swamp.ogg` / `theme_lost_village.ogg` / `theme_dream.ogg`
- 新游戏 `assets.lua` 优先加载外部图集与 OGG BGM，缺失时回退到程序化素材。
- 导入后重新执行 R6 最终验收：

```powershell
python games\wuxia_arpg\tools\run_r6_acceptance.py --backends opengl,vulkan,d3d11 --out-dir tmp\r7_ninja_accept
```

结果：`已通过`，exit=0，191s，`[r6] acceptance PASS`；资产审计 `rows=98`。

对比截图：`docs/design/wuxia_arpg_gallery/09_ninja_character_compare.png`。
## 6. 结论

R6 最终验证：三图、三后端、存档/读档、战斗成长、天气光影、资产审计均已通过。
唯一未完成项为 push，原因是环境网络不可达，已按 `环境暂缓` 记录；恢复网络后应补推 `feature/hd2d-wuxia-arpg`。