# HD-2D 武侠刷子 ARPG 资产许可台账

> 生成日期：2026-09-18（R2）
> 范围：`templates/hd2d_wuxia/` 下所有受 Git 版本控制的资产、生成脚本、游戏脚本与工具。
> 权威逐条表：本 Markdown；带 SHA-256 的同名 CSV：`docs/design/WUXIA_ARPG_ASSET_LEDGER.csv`。

## 1. 结论与硬约束

- 本模板所有美术/音频均为仓库内脚本程序化生成，**不依赖任何商业游戏解包素材**，不包含《逸剑风云决》或其他商业游戏的素材。
- 文字图集在 R2 已从来源不明的 `SimHei.ttf` 改用仓库内来源清晰的 `apps/editor_cpp/fonts/NotoSansSC-Regular.ttf`（Google Noto CJK，SIL Open Font License 1.1）重新生成。旧 `SimHei.ttf` 不再被 `templates/hd2d_wuxia/tools/gen_font.py` 引用。
- 程序化生成输出的作者为 `DSEngine Authors`，整体随仓库 `LICENSE` 以 `Apache-2.0` 发布；字体图集额外携带 SIL OFL 1.1 的字形来源义务。
- 禁止使用 NC/ND/未知许可素材；禁止商业游戏解包/截图/音频/字体/数据。

## 2. 外部输入与工具

| 输入/工具 | 版本/路径 | 作者/来源 | 许可 | 说明 |
|---|---|---|---|---|
| Noto Sans SC Regular | `apps/editor_cpp/fonts/NotoSansSC-Regular.ttf` | Google Noto CJK contributors | SIL OFL 1.1 | 来源 URL：https://github.com/notofonts/noto-cjk；字体图集的唯一字形输入；`apps/editor_cpp/fonts/README.md` 记录 OFL 来源 |
| Pillow | 12.3.0（Python 环境，不随仓库提交） | Python Pillow contributors | HPND License | 仅生成期工具，不链接进运行时，不新增/不提交第三方库 |
| Python | 3.12.6（生成期环境） | Python Software Foundation | PSF-2.0 | 运行素材生成与像素统计脚本 |

## 3. 生成命令

| 编号 | 命令 | 说明 |
|---|---|---|
| G1 | `python templates/hd2d_wuxia/tools/generate_assets.py` | 全量生成角色/敌人/NPC/FX/UI/字体/地图/图集/音频；内部调用 gen_font/gen_maps/gen_atlases/gen_audio |
| G2 | `python templates/hd2d_wuxia/tools/gen_maps.py templates/hd2d_wuxia` | 由 `MAP_SPECS` 重新生成 `assets/maps/*` 与 `scripts/mapdata.lua` |
| G3 | `python templates/hd2d_wuxia/tools/gen_atlases.py templates/hd2d_wuxia` | 由序列帧重新生成 `*_atlas.png` 与 `*_atlas.dsprite.json` |

> R2 字体合规修复实际执行的是 `generate_assets.generate_fonts(root, TEXT_SAMPLES+scan_lua_texts(root))`；全量入口 G1 使用同一函数、同一 `NotoSansSC-Regular.ttf`，可复现相同字体输出。

## 4. 逐条清单

| # | 路径 | 类型 | 来源/工具 | 作者 | 许可 | 生成命令 |
|---:|---|---|---|---|---|---|
| 1 | templates/hd2d_wuxia/README.md | 文档 | 仓库手写 | DSEngine Authors | Apache-2.0 | （文档） |
| 2 | templates/hd2d_wuxia/assets/audio/bgm_battle.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 3 | templates/hd2d_wuxia/assets/audio/bgm_boss.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 4 | templates/hd2d_wuxia/assets/audio/bgm_field.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 5 | templates/hd2d_wuxia/assets/audio/bgm_title.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 6 | templates/hd2d_wuxia/assets/audio/sfx_coin_use.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 7 | templates/hd2d_wuxia/assets/audio/sfx_crit.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 8 | templates/hd2d_wuxia/assets/audio/sfx_die.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 9 | templates/hd2d_wuxia/assets/audio/sfx_dodge.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 10 | templates/hd2d_wuxia/assets/audio/sfx_gate.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 11 | templates/hd2d_wuxia/assets/audio/sfx_heal.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 12 | templates/hd2d_wuxia/assets/audio/sfx_hit.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 13 | templates/hd2d_wuxia/assets/audio/sfx_levelup.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 14 | templates/hd2d_wuxia/assets/audio/sfx_pickup.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 15 | templates/hd2d_wuxia/assets/audio/sfx_skill.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 16 | templates/hd2d_wuxia/assets/audio/sfx_slash.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 17 | templates/hd2d_wuxia/assets/audio/sfx_talk.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 18 | templates/hd2d_wuxia/assets/audio/sfx_ui.wav | 音频素材 WAV | gen_audio.py | DSEngine Authors | Apache-2.0 | G1 |
| 19 | templates/hd2d_wuxia/assets/char/hero/hero_d_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 20 | templates/hd2d_wuxia/assets/char/hero/hero_d_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 21 | templates/hd2d_wuxia/assets/char/hero/hero_d_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 22 | templates/hd2d_wuxia/assets/char/hero/hero_d_attack_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 23 | templates/hd2d_wuxia/assets/char/hero/hero_d_attack_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 24 | templates/hd2d_wuxia/assets/char/hero/hero_d_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 25 | templates/hd2d_wuxia/assets/char/hero/hero_d_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 26 | templates/hd2d_wuxia/assets/char/hero/hero_d_cast_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 27 | templates/hd2d_wuxia/assets/char/hero/hero_d_cast_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 28 | templates/hd2d_wuxia/assets/char/hero/hero_d_cast_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 29 | templates/hd2d_wuxia/assets/char/hero/hero_d_cast_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 30 | templates/hd2d_wuxia/assets/char/hero/hero_d_cast_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 31 | templates/hd2d_wuxia/assets/char/hero/hero_d_cast_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 32 | templates/hd2d_wuxia/assets/char/hero/hero_d_cast_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 33 | templates/hd2d_wuxia/assets/char/hero/hero_d_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 34 | templates/hd2d_wuxia/assets/char/hero/hero_d_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 35 | templates/hd2d_wuxia/assets/char/hero/hero_d_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 36 | templates/hd2d_wuxia/assets/char/hero/hero_d_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 37 | templates/hd2d_wuxia/assets/char/hero/hero_d_die_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 38 | templates/hd2d_wuxia/assets/char/hero/hero_d_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 39 | templates/hd2d_wuxia/assets/char/hero/hero_d_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 40 | templates/hd2d_wuxia/assets/char/hero/hero_d_dodge_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 41 | templates/hd2d_wuxia/assets/char/hero/hero_d_dodge_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 42 | templates/hd2d_wuxia/assets/char/hero/hero_d_dodge_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 43 | templates/hd2d_wuxia/assets/char/hero/hero_d_dodge_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 44 | templates/hd2d_wuxia/assets/char/hero/hero_d_dodge_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 45 | templates/hd2d_wuxia/assets/char/hero/hero_d_dodge_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 46 | templates/hd2d_wuxia/assets/char/hero/hero_d_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 47 | templates/hd2d_wuxia/assets/char/hero/hero_d_hurt_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 48 | templates/hd2d_wuxia/assets/char/hero/hero_d_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 49 | templates/hd2d_wuxia/assets/char/hero/hero_d_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 50 | templates/hd2d_wuxia/assets/char/hero/hero_d_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 51 | templates/hd2d_wuxia/assets/char/hero/hero_d_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 52 | templates/hd2d_wuxia/assets/char/hero/hero_d_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 53 | templates/hd2d_wuxia/assets/char/hero/hero_d_idle_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 54 | templates/hd2d_wuxia/assets/char/hero/hero_d_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 55 | templates/hd2d_wuxia/assets/char/hero/hero_d_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 56 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk.dsprite.json | 手写/测试 .dsprite.json | 仓库手写测试资产 | DSEngine Authors | Apache-2.0 | （手写/测试资产） |
| 57 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 58 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 59 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 60 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 61 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 62 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 63 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 64 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 65 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk_emissive.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 66 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk_m3.dsprite.json | 手写/测试 .dsprite.json | 仓库手写测试资产 | DSEngine Authors | Apache-2.0 | （手写/测试资产） |
| 67 | templates/hd2d_wuxia/assets/char/hero/hero_d_walk_normal.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 68 | templates/hd2d_wuxia/assets/char/hero/hero_l_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 69 | templates/hd2d_wuxia/assets/char/hero/hero_l_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 70 | templates/hd2d_wuxia/assets/char/hero/hero_l_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 71 | templates/hd2d_wuxia/assets/char/hero/hero_l_attack_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 72 | templates/hd2d_wuxia/assets/char/hero/hero_l_attack_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 73 | templates/hd2d_wuxia/assets/char/hero/hero_l_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 74 | templates/hd2d_wuxia/assets/char/hero/hero_l_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 75 | templates/hd2d_wuxia/assets/char/hero/hero_l_cast_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 76 | templates/hd2d_wuxia/assets/char/hero/hero_l_cast_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 77 | templates/hd2d_wuxia/assets/char/hero/hero_l_cast_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 78 | templates/hd2d_wuxia/assets/char/hero/hero_l_cast_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 79 | templates/hd2d_wuxia/assets/char/hero/hero_l_cast_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 80 | templates/hd2d_wuxia/assets/char/hero/hero_l_cast_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 81 | templates/hd2d_wuxia/assets/char/hero/hero_l_cast_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 82 | templates/hd2d_wuxia/assets/char/hero/hero_l_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 83 | templates/hd2d_wuxia/assets/char/hero/hero_l_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 84 | templates/hd2d_wuxia/assets/char/hero/hero_l_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 85 | templates/hd2d_wuxia/assets/char/hero/hero_l_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 86 | templates/hd2d_wuxia/assets/char/hero/hero_l_die_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 87 | templates/hd2d_wuxia/assets/char/hero/hero_l_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 88 | templates/hd2d_wuxia/assets/char/hero/hero_l_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 89 | templates/hd2d_wuxia/assets/char/hero/hero_l_dodge_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 90 | templates/hd2d_wuxia/assets/char/hero/hero_l_dodge_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 91 | templates/hd2d_wuxia/assets/char/hero/hero_l_dodge_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 92 | templates/hd2d_wuxia/assets/char/hero/hero_l_dodge_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 93 | templates/hd2d_wuxia/assets/char/hero/hero_l_dodge_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 94 | templates/hd2d_wuxia/assets/char/hero/hero_l_dodge_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 95 | templates/hd2d_wuxia/assets/char/hero/hero_l_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 96 | templates/hd2d_wuxia/assets/char/hero/hero_l_hurt_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 97 | templates/hd2d_wuxia/assets/char/hero/hero_l_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 98 | templates/hd2d_wuxia/assets/char/hero/hero_l_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 99 | templates/hd2d_wuxia/assets/char/hero/hero_l_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 100 | templates/hd2d_wuxia/assets/char/hero/hero_l_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 101 | templates/hd2d_wuxia/assets/char/hero/hero_l_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 102 | templates/hd2d_wuxia/assets/char/hero/hero_l_idle_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 103 | templates/hd2d_wuxia/assets/char/hero/hero_l_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 104 | templates/hd2d_wuxia/assets/char/hero/hero_l_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 105 | templates/hd2d_wuxia/assets/char/hero/hero_l_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 106 | templates/hd2d_wuxia/assets/char/hero/hero_l_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 107 | templates/hd2d_wuxia/assets/char/hero/hero_l_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 108 | templates/hd2d_wuxia/assets/char/hero/hero_l_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 109 | templates/hd2d_wuxia/assets/char/hero/hero_l_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 110 | templates/hd2d_wuxia/assets/char/hero/hero_l_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 111 | templates/hd2d_wuxia/assets/char/hero/hero_l_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 112 | templates/hd2d_wuxia/assets/char/hero/hero_l_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 113 | templates/hd2d_wuxia/assets/char/hero/hero_r_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 114 | templates/hd2d_wuxia/assets/char/hero/hero_r_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 115 | templates/hd2d_wuxia/assets/char/hero/hero_r_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 116 | templates/hd2d_wuxia/assets/char/hero/hero_r_attack_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 117 | templates/hd2d_wuxia/assets/char/hero/hero_r_attack_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 118 | templates/hd2d_wuxia/assets/char/hero/hero_r_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 119 | templates/hd2d_wuxia/assets/char/hero/hero_r_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 120 | templates/hd2d_wuxia/assets/char/hero/hero_r_cast_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 121 | templates/hd2d_wuxia/assets/char/hero/hero_r_cast_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 122 | templates/hd2d_wuxia/assets/char/hero/hero_r_cast_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 123 | templates/hd2d_wuxia/assets/char/hero/hero_r_cast_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 124 | templates/hd2d_wuxia/assets/char/hero/hero_r_cast_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 125 | templates/hd2d_wuxia/assets/char/hero/hero_r_cast_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 126 | templates/hd2d_wuxia/assets/char/hero/hero_r_cast_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 127 | templates/hd2d_wuxia/assets/char/hero/hero_r_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 128 | templates/hd2d_wuxia/assets/char/hero/hero_r_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 129 | templates/hd2d_wuxia/assets/char/hero/hero_r_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 130 | templates/hd2d_wuxia/assets/char/hero/hero_r_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 131 | templates/hd2d_wuxia/assets/char/hero/hero_r_die_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 132 | templates/hd2d_wuxia/assets/char/hero/hero_r_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 133 | templates/hd2d_wuxia/assets/char/hero/hero_r_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 134 | templates/hd2d_wuxia/assets/char/hero/hero_r_dodge_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 135 | templates/hd2d_wuxia/assets/char/hero/hero_r_dodge_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 136 | templates/hd2d_wuxia/assets/char/hero/hero_r_dodge_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 137 | templates/hd2d_wuxia/assets/char/hero/hero_r_dodge_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 138 | templates/hd2d_wuxia/assets/char/hero/hero_r_dodge_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 139 | templates/hd2d_wuxia/assets/char/hero/hero_r_dodge_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 140 | templates/hd2d_wuxia/assets/char/hero/hero_r_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 141 | templates/hd2d_wuxia/assets/char/hero/hero_r_hurt_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 142 | templates/hd2d_wuxia/assets/char/hero/hero_r_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 143 | templates/hd2d_wuxia/assets/char/hero/hero_r_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 144 | templates/hd2d_wuxia/assets/char/hero/hero_r_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 145 | templates/hd2d_wuxia/assets/char/hero/hero_r_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 146 | templates/hd2d_wuxia/assets/char/hero/hero_r_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 147 | templates/hd2d_wuxia/assets/char/hero/hero_r_idle_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 148 | templates/hd2d_wuxia/assets/char/hero/hero_r_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 149 | templates/hd2d_wuxia/assets/char/hero/hero_r_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 150 | templates/hd2d_wuxia/assets/char/hero/hero_r_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 151 | templates/hd2d_wuxia/assets/char/hero/hero_r_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 152 | templates/hd2d_wuxia/assets/char/hero/hero_r_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 153 | templates/hd2d_wuxia/assets/char/hero/hero_r_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 154 | templates/hd2d_wuxia/assets/char/hero/hero_r_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 155 | templates/hd2d_wuxia/assets/char/hero/hero_r_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 156 | templates/hd2d_wuxia/assets/char/hero/hero_r_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 157 | templates/hd2d_wuxia/assets/char/hero/hero_r_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 158 | templates/hd2d_wuxia/assets/char/hero/hero_u_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 159 | templates/hd2d_wuxia/assets/char/hero/hero_u_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 160 | templates/hd2d_wuxia/assets/char/hero/hero_u_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 161 | templates/hd2d_wuxia/assets/char/hero/hero_u_attack_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 162 | templates/hd2d_wuxia/assets/char/hero/hero_u_attack_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 163 | templates/hd2d_wuxia/assets/char/hero/hero_u_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 164 | templates/hd2d_wuxia/assets/char/hero/hero_u_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 165 | templates/hd2d_wuxia/assets/char/hero/hero_u_cast_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 166 | templates/hd2d_wuxia/assets/char/hero/hero_u_cast_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 167 | templates/hd2d_wuxia/assets/char/hero/hero_u_cast_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 168 | templates/hd2d_wuxia/assets/char/hero/hero_u_cast_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 169 | templates/hd2d_wuxia/assets/char/hero/hero_u_cast_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 170 | templates/hd2d_wuxia/assets/char/hero/hero_u_cast_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 171 | templates/hd2d_wuxia/assets/char/hero/hero_u_cast_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 172 | templates/hd2d_wuxia/assets/char/hero/hero_u_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 173 | templates/hd2d_wuxia/assets/char/hero/hero_u_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 174 | templates/hd2d_wuxia/assets/char/hero/hero_u_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 175 | templates/hd2d_wuxia/assets/char/hero/hero_u_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 176 | templates/hd2d_wuxia/assets/char/hero/hero_u_die_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 177 | templates/hd2d_wuxia/assets/char/hero/hero_u_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 178 | templates/hd2d_wuxia/assets/char/hero/hero_u_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 179 | templates/hd2d_wuxia/assets/char/hero/hero_u_dodge_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 180 | templates/hd2d_wuxia/assets/char/hero/hero_u_dodge_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 181 | templates/hd2d_wuxia/assets/char/hero/hero_u_dodge_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 182 | templates/hd2d_wuxia/assets/char/hero/hero_u_dodge_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 183 | templates/hd2d_wuxia/assets/char/hero/hero_u_dodge_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 184 | templates/hd2d_wuxia/assets/char/hero/hero_u_dodge_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 185 | templates/hd2d_wuxia/assets/char/hero/hero_u_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 186 | templates/hd2d_wuxia/assets/char/hero/hero_u_hurt_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 187 | templates/hd2d_wuxia/assets/char/hero/hero_u_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 188 | templates/hd2d_wuxia/assets/char/hero/hero_u_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 189 | templates/hd2d_wuxia/assets/char/hero/hero_u_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 190 | templates/hd2d_wuxia/assets/char/hero/hero_u_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 191 | templates/hd2d_wuxia/assets/char/hero/hero_u_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 192 | templates/hd2d_wuxia/assets/char/hero/hero_u_idle_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 193 | templates/hd2d_wuxia/assets/char/hero/hero_u_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 194 | templates/hd2d_wuxia/assets/char/hero/hero_u_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 195 | templates/hd2d_wuxia/assets/char/hero/hero_u_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 196 | templates/hd2d_wuxia/assets/char/hero/hero_u_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 197 | templates/hd2d_wuxia/assets/char/hero/hero_u_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 198 | templates/hd2d_wuxia/assets/char/hero/hero_u_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 199 | templates/hd2d_wuxia/assets/char/hero/hero_u_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 200 | templates/hd2d_wuxia/assets/char/hero/hero_u_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 201 | templates/hd2d_wuxia/assets/char/hero/hero_u_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 202 | templates/hd2d_wuxia/assets/char/hero/hero_u_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 203 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 204 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 205 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 206 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 207 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 208 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 209 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 210 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 211 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 212 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 213 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 214 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 215 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 216 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 217 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 218 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 219 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 220 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 221 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 222 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 223 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 224 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 225 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 226 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 227 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_d_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 228 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 229 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 230 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 231 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 232 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 233 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 234 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 235 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 236 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 237 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 238 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 239 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 240 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 241 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 242 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 243 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 244 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 245 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 246 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 247 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 248 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 249 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 250 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 251 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 252 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_l_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 253 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 254 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 255 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 256 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 257 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 258 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 259 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 260 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 261 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 262 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 263 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 264 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 265 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 266 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 267 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 268 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 269 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 270 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 271 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 272 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 273 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 274 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 275 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 276 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 277 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_r_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 278 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 279 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 280 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 281 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 282 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 283 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 284 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 285 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 286 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 287 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 288 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 289 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 290 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 291 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 292 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 293 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 294 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 295 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 296 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 297 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 298 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 299 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 300 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 301 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 302 | templates/hd2d_wuxia/assets/enemy/bandit/bandit_u_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 303 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 304 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 305 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 306 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_attack_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 307 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 308 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 309 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 310 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 311 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 312 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 313 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_die_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 314 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_die_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 315 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 316 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 317 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 318 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 319 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 320 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 321 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 322 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 323 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_idle_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 324 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 325 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 326 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 327 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 328 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 329 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 330 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 331 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 332 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 333 | templates/hd2d_wuxia/assets/enemy/boss/boss_d_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 334 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 335 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 336 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 337 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_attack_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 338 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 339 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 340 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 341 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 342 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 343 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 344 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_die_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 345 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_die_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 346 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 347 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 348 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 349 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 350 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 351 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 352 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 353 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 354 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_idle_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 355 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 356 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 357 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 358 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 359 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 360 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 361 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 362 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 363 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 364 | templates/hd2d_wuxia/assets/enemy/boss/boss_l_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 365 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 366 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 367 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 368 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_attack_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 369 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 370 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 371 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 372 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 373 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 374 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 375 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_die_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 376 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_die_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 377 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 378 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 379 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 380 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 381 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 382 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 383 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 384 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 385 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_idle_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 386 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 387 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 388 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 389 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 390 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 391 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 392 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 393 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 394 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 395 | templates/hd2d_wuxia/assets/enemy/boss/boss_r_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 396 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 397 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 398 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 399 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_attack_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 400 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 401 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 402 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 403 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 404 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 405 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 406 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_die_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 407 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_die_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 408 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 409 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 410 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 411 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 412 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 413 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 414 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 415 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 416 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_idle_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 417 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 418 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 419 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 420 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 421 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 422 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 423 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 424 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 425 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 426 | templates/hd2d_wuxia/assets/enemy/boss/boss_u_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 427 | templates/hd2d_wuxia/assets/enemy/ghost_l_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 428 | templates/hd2d_wuxia/assets/enemy/ghost_l_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 429 | templates/hd2d_wuxia/assets/enemy/ghost_l_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 430 | templates/hd2d_wuxia/assets/enemy/ghost_l_attack_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 431 | templates/hd2d_wuxia/assets/enemy/ghost_l_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 432 | templates/hd2d_wuxia/assets/enemy/ghost_l_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 433 | templates/hd2d_wuxia/assets/enemy/ghost_l_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 434 | templates/hd2d_wuxia/assets/enemy/ghost_l_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 435 | templates/hd2d_wuxia/assets/enemy/ghost_l_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 436 | templates/hd2d_wuxia/assets/enemy/ghost_l_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 437 | templates/hd2d_wuxia/assets/enemy/ghost_l_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 438 | templates/hd2d_wuxia/assets/enemy/ghost_l_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 439 | templates/hd2d_wuxia/assets/enemy/ghost_l_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 440 | templates/hd2d_wuxia/assets/enemy/ghost_l_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 441 | templates/hd2d_wuxia/assets/enemy/ghost_l_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 442 | templates/hd2d_wuxia/assets/enemy/ghost_l_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 443 | templates/hd2d_wuxia/assets/enemy/ghost_l_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 444 | templates/hd2d_wuxia/assets/enemy/ghost_l_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 445 | templates/hd2d_wuxia/assets/enemy/ghost_l_idle_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 446 | templates/hd2d_wuxia/assets/enemy/ghost_l_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 447 | templates/hd2d_wuxia/assets/enemy/ghost_l_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 448 | templates/hd2d_wuxia/assets/enemy/ghost_l_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 449 | templates/hd2d_wuxia/assets/enemy/ghost_l_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 450 | templates/hd2d_wuxia/assets/enemy/ghost_l_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 451 | templates/hd2d_wuxia/assets/enemy/ghost_l_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 452 | templates/hd2d_wuxia/assets/enemy/ghost_l_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 453 | templates/hd2d_wuxia/assets/enemy/ghost_l_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 454 | templates/hd2d_wuxia/assets/enemy/ghost_l_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 455 | templates/hd2d_wuxia/assets/enemy/ghost_l_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 456 | templates/hd2d_wuxia/assets/enemy/ghost_r_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 457 | templates/hd2d_wuxia/assets/enemy/ghost_r_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 458 | templates/hd2d_wuxia/assets/enemy/ghost_r_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 459 | templates/hd2d_wuxia/assets/enemy/ghost_r_attack_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 460 | templates/hd2d_wuxia/assets/enemy/ghost_r_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 461 | templates/hd2d_wuxia/assets/enemy/ghost_r_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 462 | templates/hd2d_wuxia/assets/enemy/ghost_r_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 463 | templates/hd2d_wuxia/assets/enemy/ghost_r_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 464 | templates/hd2d_wuxia/assets/enemy/ghost_r_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 465 | templates/hd2d_wuxia/assets/enemy/ghost_r_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 466 | templates/hd2d_wuxia/assets/enemy/ghost_r_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 467 | templates/hd2d_wuxia/assets/enemy/ghost_r_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 468 | templates/hd2d_wuxia/assets/enemy/ghost_r_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 469 | templates/hd2d_wuxia/assets/enemy/ghost_r_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 470 | templates/hd2d_wuxia/assets/enemy/ghost_r_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 471 | templates/hd2d_wuxia/assets/enemy/ghost_r_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 472 | templates/hd2d_wuxia/assets/enemy/ghost_r_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 473 | templates/hd2d_wuxia/assets/enemy/ghost_r_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 474 | templates/hd2d_wuxia/assets/enemy/ghost_r_idle_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 475 | templates/hd2d_wuxia/assets/enemy/ghost_r_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 476 | templates/hd2d_wuxia/assets/enemy/ghost_r_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 477 | templates/hd2d_wuxia/assets/enemy/ghost_r_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 478 | templates/hd2d_wuxia/assets/enemy/ghost_r_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 479 | templates/hd2d_wuxia/assets/enemy/ghost_r_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 480 | templates/hd2d_wuxia/assets/enemy/ghost_r_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 481 | templates/hd2d_wuxia/assets/enemy/ghost_r_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 482 | templates/hd2d_wuxia/assets/enemy/ghost_r_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 483 | templates/hd2d_wuxia/assets/enemy/ghost_r_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 484 | templates/hd2d_wuxia/assets/enemy/ghost_r_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 485 | templates/hd2d_wuxia/assets/enemy/wolf_l_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 486 | templates/hd2d_wuxia/assets/enemy/wolf_l_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 487 | templates/hd2d_wuxia/assets/enemy/wolf_l_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 488 | templates/hd2d_wuxia/assets/enemy/wolf_l_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 489 | templates/hd2d_wuxia/assets/enemy/wolf_l_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 490 | templates/hd2d_wuxia/assets/enemy/wolf_l_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 491 | templates/hd2d_wuxia/assets/enemy/wolf_l_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 492 | templates/hd2d_wuxia/assets/enemy/wolf_l_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 493 | templates/hd2d_wuxia/assets/enemy/wolf_l_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 494 | templates/hd2d_wuxia/assets/enemy/wolf_l_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 495 | templates/hd2d_wuxia/assets/enemy/wolf_l_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 496 | templates/hd2d_wuxia/assets/enemy/wolf_l_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 497 | templates/hd2d_wuxia/assets/enemy/wolf_l_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 498 | templates/hd2d_wuxia/assets/enemy/wolf_l_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 499 | templates/hd2d_wuxia/assets/enemy/wolf_l_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 500 | templates/hd2d_wuxia/assets/enemy/wolf_l_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 501 | templates/hd2d_wuxia/assets/enemy/wolf_l_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 502 | templates/hd2d_wuxia/assets/enemy/wolf_l_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 503 | templates/hd2d_wuxia/assets/enemy/wolf_l_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 504 | templates/hd2d_wuxia/assets/enemy/wolf_l_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 505 | templates/hd2d_wuxia/assets/enemy/wolf_l_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 506 | templates/hd2d_wuxia/assets/enemy/wolf_l_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 507 | templates/hd2d_wuxia/assets/enemy/wolf_l_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 508 | templates/hd2d_wuxia/assets/enemy/wolf_l_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 509 | templates/hd2d_wuxia/assets/enemy/wolf_l_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 510 | templates/hd2d_wuxia/assets/enemy/wolf_l_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 511 | templates/hd2d_wuxia/assets/enemy/wolf_l_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 512 | templates/hd2d_wuxia/assets/enemy/wolf_r_attack_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 513 | templates/hd2d_wuxia/assets/enemy/wolf_r_attack_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 514 | templates/hd2d_wuxia/assets/enemy/wolf_r_attack_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 515 | templates/hd2d_wuxia/assets/enemy/wolf_r_attack_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 516 | templates/hd2d_wuxia/assets/enemy/wolf_r_attack_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 517 | templates/hd2d_wuxia/assets/enemy/wolf_r_die_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 518 | templates/hd2d_wuxia/assets/enemy/wolf_r_die_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 519 | templates/hd2d_wuxia/assets/enemy/wolf_r_die_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 520 | templates/hd2d_wuxia/assets/enemy/wolf_r_die_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 521 | templates/hd2d_wuxia/assets/enemy/wolf_r_die_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 522 | templates/hd2d_wuxia/assets/enemy/wolf_r_die_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 523 | templates/hd2d_wuxia/assets/enemy/wolf_r_hurt_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 524 | templates/hd2d_wuxia/assets/enemy/wolf_r_hurt_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 525 | templates/hd2d_wuxia/assets/enemy/wolf_r_hurt_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 526 | templates/hd2d_wuxia/assets/enemy/wolf_r_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 527 | templates/hd2d_wuxia/assets/enemy/wolf_r_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 528 | templates/hd2d_wuxia/assets/enemy/wolf_r_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 529 | templates/hd2d_wuxia/assets/enemy/wolf_r_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 530 | templates/hd2d_wuxia/assets/enemy/wolf_r_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 531 | templates/hd2d_wuxia/assets/enemy/wolf_r_walk_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 532 | templates/hd2d_wuxia/assets/enemy/wolf_r_walk_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 533 | templates/hd2d_wuxia/assets/enemy/wolf_r_walk_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 534 | templates/hd2d_wuxia/assets/enemy/wolf_r_walk_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 535 | templates/hd2d_wuxia/assets/enemy/wolf_r_walk_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 536 | templates/hd2d_wuxia/assets/enemy/wolf_r_walk_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 537 | templates/hd2d_wuxia/assets/enemy/wolf_r_walk_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 538 | templates/hd2d_wuxia/assets/enemy/wolf_r_walk_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 539 | templates/hd2d_wuxia/assets/fx/dust_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 540 | templates/hd2d_wuxia/assets/fx/dust_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 541 | templates/hd2d_wuxia/assets/fx/dust_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 542 | templates/hd2d_wuxia/assets/fx/dust_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 543 | templates/hd2d_wuxia/assets/fx/dust_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 544 | templates/hd2d_wuxia/assets/fx/dust_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 545 | templates/hd2d_wuxia/assets/fx/firefly.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 546 | templates/hd2d_wuxia/assets/fx/firefly_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 547 | templates/hd2d_wuxia/assets/fx/firefly_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 548 | templates/hd2d_wuxia/assets/fx/glow_cool.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 549 | templates/hd2d_wuxia/assets/fx/glow_torch.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 550 | templates/hd2d_wuxia/assets/fx/glow_warm.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 551 | templates/hd2d_wuxia/assets/fx/heal_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 552 | templates/hd2d_wuxia/assets/fx/heal_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 553 | templates/hd2d_wuxia/assets/fx/heal_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 554 | templates/hd2d_wuxia/assets/fx/heal_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 555 | templates/hd2d_wuxia/assets/fx/heal_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 556 | templates/hd2d_wuxia/assets/fx/heal_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 557 | templates/hd2d_wuxia/assets/fx/heal_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 558 | templates/hd2d_wuxia/assets/fx/hit_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 559 | templates/hd2d_wuxia/assets/fx/hit_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 560 | templates/hd2d_wuxia/assets/fx/hit_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 561 | templates/hd2d_wuxia/assets/fx/hit_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 562 | templates/hd2d_wuxia/assets/fx/hit_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 563 | templates/hd2d_wuxia/assets/fx/hit_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 564 | templates/hd2d_wuxia/assets/fx/leaf_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 565 | templates/hd2d_wuxia/assets/fx/leaf_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 566 | templates/hd2d_wuxia/assets/fx/leaf_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 567 | templates/hd2d_wuxia/assets/fx/leaf_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 568 | templates/hd2d_wuxia/assets/fx/leaf_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 569 | templates/hd2d_wuxia/assets/fx/levelup_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 570 | templates/hd2d_wuxia/assets/fx/levelup_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 571 | templates/hd2d_wuxia/assets/fx/levelup_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 572 | templates/hd2d_wuxia/assets/fx/levelup_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 573 | templates/hd2d_wuxia/assets/fx/levelup_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 574 | templates/hd2d_wuxia/assets/fx/levelup_5.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 575 | templates/hd2d_wuxia/assets/fx/levelup_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 576 | templates/hd2d_wuxia/assets/fx/levelup_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 577 | templates/hd2d_wuxia/assets/fx/qi_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 578 | templates/hd2d_wuxia/assets/fx/qi_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 579 | templates/hd2d_wuxia/assets/fx/qi_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 580 | templates/hd2d_wuxia/assets/fx/qi_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 581 | templates/hd2d_wuxia/assets/fx/qi_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 582 | templates/hd2d_wuxia/assets/fx/qi_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 583 | templates/hd2d_wuxia/assets/fx/qi_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 584 | templates/hd2d_wuxia/assets/fx/shadow_big.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 585 | templates/hd2d_wuxia/assets/fx/shadow_mid.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 586 | templates/hd2d_wuxia/assets/fx/shadow_small.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 587 | templates/hd2d_wuxia/assets/fx/slash_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 588 | templates/hd2d_wuxia/assets/fx/slash_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 589 | templates/hd2d_wuxia/assets/fx/slash_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 590 | templates/hd2d_wuxia/assets/fx/slash_3.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 591 | templates/hd2d_wuxia/assets/fx/slash_4.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 592 | templates/hd2d_wuxia/assets/fx/slash_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 593 | templates/hd2d_wuxia/assets/fx/slash_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 594 | templates/hd2d_wuxia/assets/maps/stronghold_3d.dmesh | 3D 地图网格 | gen_maps.py | DSEngine Authors | Apache-2.0 | G2 |
| 595 | templates/hd2d_wuxia/assets/maps/stronghold_far.png | 地图分层/程序化 PNG | gen_maps.py | DSEngine Authors | Apache-2.0 | G2 |
| 596 | templates/hd2d_wuxia/assets/maps/stronghold_fg.png | 地图分层/程序化 PNG | gen_maps.py | DSEngine Authors | Apache-2.0 | G2 |
| 597 | templates/hd2d_wuxia/assets/maps/stronghold_ground.png | 地图分层/程序化 PNG | gen_maps.py | DSEngine Authors | Apache-2.0 | G2 |
| 598 | templates/hd2d_wuxia/assets/maps/stronghold_sky.png | 地图分层/程序化 PNG | gen_maps.py | DSEngine Authors | Apache-2.0 | G2 |
| 599 | templates/hd2d_wuxia/assets/maps/village_3d.dmesh | 3D 地图网格 | gen_maps.py | DSEngine Authors | Apache-2.0 | G2 |
| 600 | templates/hd2d_wuxia/assets/maps/village_far.png | 地图分层/程序化 PNG | gen_maps.py | DSEngine Authors | Apache-2.0 | G2 |
| 601 | templates/hd2d_wuxia/assets/maps/village_fg.png | 地图分层/程序化 PNG | gen_maps.py | DSEngine Authors | Apache-2.0 | G2 |
| 602 | templates/hd2d_wuxia/assets/maps/village_ground.png | 地图分层/程序化 PNG | gen_maps.py | DSEngine Authors | Apache-2.0 | G2 |
| 603 | templates/hd2d_wuxia/assets/maps/village_sky.png | 地图分层/程序化 PNG | gen_maps.py | DSEngine Authors | Apache-2.0 | G2 |
| 604 | templates/hd2d_wuxia/assets/npc/elder/elder_d_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 605 | templates/hd2d_wuxia/assets/npc/elder/elder_d_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 606 | templates/hd2d_wuxia/assets/npc/elder/elder_d_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 607 | templates/hd2d_wuxia/assets/npc/elder/elder_d_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 608 | templates/hd2d_wuxia/assets/npc/elder/elder_d_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 609 | templates/hd2d_wuxia/assets/npc/elder/elder_r_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 610 | templates/hd2d_wuxia/assets/npc/elder/elder_r_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 611 | templates/hd2d_wuxia/assets/npc/elder/elder_r_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 612 | templates/hd2d_wuxia/assets/npc/elder/elder_r_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 613 | templates/hd2d_wuxia/assets/npc/elder/elder_r_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 614 | templates/hd2d_wuxia/assets/npc/elder/elder_u_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 615 | templates/hd2d_wuxia/assets/npc/elder/elder_u_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 616 | templates/hd2d_wuxia/assets/npc/elder/elder_u_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 617 | templates/hd2d_wuxia/assets/npc/elder/elder_u_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 618 | templates/hd2d_wuxia/assets/npc/elder/elder_u_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 619 | templates/hd2d_wuxia/assets/npc/smith/smith_d_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 620 | templates/hd2d_wuxia/assets/npc/smith/smith_d_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 621 | templates/hd2d_wuxia/assets/npc/smith/smith_d_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 622 | templates/hd2d_wuxia/assets/npc/smith/smith_d_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 623 | templates/hd2d_wuxia/assets/npc/smith/smith_d_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 624 | templates/hd2d_wuxia/assets/npc/smith/smith_r_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 625 | templates/hd2d_wuxia/assets/npc/smith/smith_r_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 626 | templates/hd2d_wuxia/assets/npc/smith/smith_r_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 627 | templates/hd2d_wuxia/assets/npc/smith/smith_r_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 628 | templates/hd2d_wuxia/assets/npc/smith/smith_r_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 629 | templates/hd2d_wuxia/assets/npc/smith/smith_u_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 630 | templates/hd2d_wuxia/assets/npc/smith/smith_u_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 631 | templates/hd2d_wuxia/assets/npc/smith/smith_u_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 632 | templates/hd2d_wuxia/assets/npc/smith/smith_u_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 633 | templates/hd2d_wuxia/assets/npc/smith/smith_u_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 634 | templates/hd2d_wuxia/assets/npc/villager/villager_d_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 635 | templates/hd2d_wuxia/assets/npc/villager/villager_d_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 636 | templates/hd2d_wuxia/assets/npc/villager/villager_d_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 637 | templates/hd2d_wuxia/assets/npc/villager/villager_d_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 638 | templates/hd2d_wuxia/assets/npc/villager/villager_d_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 639 | templates/hd2d_wuxia/assets/npc/villager/villager_r_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 640 | templates/hd2d_wuxia/assets/npc/villager/villager_r_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 641 | templates/hd2d_wuxia/assets/npc/villager/villager_r_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 642 | templates/hd2d_wuxia/assets/npc/villager/villager_r_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 643 | templates/hd2d_wuxia/assets/npc/villager/villager_r_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 644 | templates/hd2d_wuxia/assets/npc/villager/villager_u_idle_0.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 645 | templates/hd2d_wuxia/assets/npc/villager/villager_u_idle_1.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 646 | templates/hd2d_wuxia/assets/npc/villager/villager_u_idle_2.png | 角色/敌人/NPC/FX 序列帧 PNG | generate_assets.py + gen_lib.py | DSEngine Authors | Apache-2.0 | G1 |
| 647 | templates/hd2d_wuxia/assets/npc/villager/villager_u_idle_atlas.dsprite.json | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 648 | templates/hd2d_wuxia/assets/npc/villager/villager_u_idle_atlas.png | 图集描述/图集 PNG | gen_atlases.py | DSEngine Authors | Apache-2.0 | G3 |
| 649 | templates/hd2d_wuxia/assets/ui/bar_frame.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 650 | templates/hd2d_wuxia/assets/ui/bar_hp.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 651 | templates/hd2d_wuxia/assets/ui/bar_mp.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 652 | templates/hd2d_wuxia/assets/ui/bar_stam.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 653 | templates/hd2d_wuxia/assets/ui/button.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 654 | templates/hd2d_wuxia/assets/ui/cursor.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 655 | templates/hd2d_wuxia/assets/ui/font_big.png | 中文位图字体图集 PNG | gen_font.py + apps/editor_cpp/fonts/NotoSansSC-Regular.ttf | DSEngine Authors / Noto CJK contributors | Apache-2.0 (生成输出) + SIL OFL 1.1 (字形来源) | G1 |
| 656 | templates/hd2d_wuxia/assets/ui/font_small.png | 中文位图字体图集 PNG | gen_font.py + apps/editor_cpp/fonts/NotoSansSC-Regular.ttf | DSEngine Authors / Noto CJK contributors | Apache-2.0 (生成输出) + SIL OFL 1.1 (字形来源) | G1 |
| 657 | templates/hd2d_wuxia/assets/ui/grad_bottom.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 658 | templates/hd2d_wuxia/assets/ui/icon_armor.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 659 | templates/hd2d_wuxia/assets/ui/icon_coin.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 660 | templates/hd2d_wuxia/assets/ui/icon_hp_potion.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 661 | templates/hd2d_wuxia/assets/ui/icon_manual.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 662 | templates/hd2d_wuxia/assets/ui/icon_mp_potion.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 663 | templates/hd2d_wuxia/assets/ui/icon_sword.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 664 | templates/hd2d_wuxia/assets/ui/minimap_frame.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 665 | templates/hd2d_wuxia/assets/ui/name_plate.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 666 | templates/hd2d_wuxia/assets/ui/panel_dialog.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 667 | templates/hd2d_wuxia/assets/ui/panel_hud.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 668 | templates/hd2d_wuxia/assets/ui/panel_menu.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 669 | templates/hd2d_wuxia/assets/ui/portrait_hero.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 670 | templates/hd2d_wuxia/assets/ui/quest_marker.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 671 | templates/hd2d_wuxia/assets/ui/skill_icon_0.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 672 | templates/hd2d_wuxia/assets/ui/skill_icon_1.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 673 | templates/hd2d_wuxia/assets/ui/skill_icon_2.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 674 | templates/hd2d_wuxia/assets/ui/skill_icon_3.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 675 | templates/hd2d_wuxia/assets/ui/skill_slot.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 676 | templates/hd2d_wuxia/assets/ui/title_plate.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 677 | templates/hd2d_wuxia/assets/ui/vignette.png | UI 素材 PNG | generate_assets.py | DSEngine Authors | Apache-2.0 | G1 |
| 678 | templates/hd2d_wuxia/scripts/_compat_test.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 679 | templates/hd2d_wuxia/scripts/_hd2d_m4_test.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 680 | templates/hd2d_wuxia/scripts/_hd2d_m5_roundtrip_test.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 681 | templates/hd2d_wuxia/scripts/_hd2d_m5_test.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 682 | templates/hd2d_wuxia/scripts/_hd2d_m6_acceptance_test.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 683 | templates/hd2d_wuxia/scripts/_smoke.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 684 | templates/hd2d_wuxia/scripts/_sprite3d_lit_test.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 685 | templates/hd2d_wuxia/scripts/_sprite3d_many_lights_test.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 686 | templates/hd2d_wuxia/scripts/_sprite3d_normal_emissive_test.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 687 | templates/hd2d_wuxia/scripts/_sprite3d_perf_test.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 688 | templates/hd2d_wuxia/scripts/_sprite3d_test.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 689 | templates/hd2d_wuxia/scripts/assets.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 690 | templates/hd2d_wuxia/scripts/audio.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 691 | templates/hd2d_wuxia/scripts/bplus.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 692 | templates/hd2d_wuxia/scripts/core.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 693 | templates/hd2d_wuxia/scripts/data.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 694 | templates/hd2d_wuxia/scripts/enemy.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 695 | templates/hd2d_wuxia/scripts/font_big.lua | 字体度量 Lua（生成） | gen_font.py + apps/editor_cpp/fonts/NotoSansSC-Regular.ttf | DSEngine Authors / Noto CJK contributors | Apache-2.0 (生成输出) + SIL OFL 1.1 (字形来源) | G1 |
| 696 | templates/hd2d_wuxia/scripts/font_small.lua | 字体度量 Lua（生成） | gen_font.py + apps/editor_cpp/fonts/NotoSansSC-Regular.ttf | DSEngine Authors / Noto CJK contributors | Apache-2.0 (生成输出) + SIL OFL 1.1 (字形来源) | G1 |
| 697 | templates/hd2d_wuxia/scripts/fx.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 698 | templates/hd2d_wuxia/scripts/main.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 699 | templates/hd2d_wuxia/scripts/mapdata.lua | 地图数据 Lua（生成） | gen_maps.py | DSEngine Authors | Apache-2.0 | G2 |
| 700 | templates/hd2d_wuxia/scripts/player.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 701 | templates/hd2d_wuxia/scripts/save.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 702 | templates/hd2d_wuxia/scripts/ui.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 703 | templates/hd2d_wuxia/scripts/world.lua | Lua 游戏源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （手写源） |
| 704 | templates/hd2d_wuxia/tools/_gen_maps_only.py | 素材生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （工具源） |
| 705 | templates/hd2d_wuxia/tools/gen_atlases.py | 素材生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （工具源） |
| 706 | templates/hd2d_wuxia/tools/gen_audio.py | 素材生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （工具源） |
| 707 | templates/hd2d_wuxia/tools/gen_font.py | 素材生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （工具源） |
| 708 | templates/hd2d_wuxia/tools/gen_lib.py | 素材生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （工具源） |
| 709 | templates/hd2d_wuxia/tools/gen_maps.py | 素材生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （工具源） |
| 710 | templates/hd2d_wuxia/tools/generate_assets.py | 素材生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （工具源） |
| 711 | templates/hd2d_wuxia/tools/hd2d_pixel_stats.py | 素材生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （工具源） |
| 712 | templates/hd2d_wuxia/tools/run_hd2d_m6_acceptance.py | 素材生成/验收工具源码 | 仓库手写 | DSEngine Authors | Apache-2.0 | （工具源） |

## 5. 静态合规检查命令

```powershell
$texts = Get-ChildItem templates\hd2d_wuxia -Recurse -File -Include *.py,*.lua,*.md,*.json
$texts | Select-String -Pattern '逸剑风云决|商业游戏|解包素材|unpack|commercial game' -CaseSensitive:$false
```

## 6. 新增素材规则

- R3-R6 新增素材优先扩展 `templates/hd2d_wuxia/tools/gen_lib.py` / `gen_maps.py` / `generate_assets.py`，由脚本生成并追加到本台账与 CSV。
- 若必须引入外部素材，只能选 CC0 / CC-BY / CC-BY-SA / SIL OFL 等许可清晰素材，并在本台账先补来源、作者、许可、URL、SHA-256，再进入代码实现。
- 任何商业游戏解包素材一律拒绝，不得进入仓库、截图或构建产物。
