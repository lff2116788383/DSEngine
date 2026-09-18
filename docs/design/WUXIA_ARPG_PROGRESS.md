# HD-2D 武侠刷子 ARPG 进度（WUXIA_ARPG_PROGRESS）

## 0. Goal 状态

```yaml
goal:
  objective: "用 DSEngine 制作 HD-2D 武侠刷子 ARPG（暗黑 2 式，3 张地图）"
  status: in_progress
  max_goal_rounds: 12
  current_round: R6
  attempt: 1
  completed_rounds: [R1, R2, R3, R4, R5]
  next_round: R6
  branch: feature/hd2d-wuxia-arpg
  baseline: feature/engine-lib @ 0a8fc8b0
  contract: docs/design/WUXIA_ARPG_PLAN.md
```

> 说明：本轮开始时用户要求读取 `docs/design/WUXIA_ARPG_PLAN.md`，但该文件与 PROGRESS 文件当时都不存在；R1 已按用户 objective 补建 PLAN，并建立本 PROGRESS。后续所有轮次以补建后的 PLAN 为轮次契约。

## 1. 轮次总表

| 轮次 | 阶段 | 状态 | 过门证据 |
|---|---|---|---|
| R1 | 能力审计 | 已完成（审计通过） | 本文 2 |
| R2 | 素材清单 + 完整设计 | 已完成（门禁通过） | 本文 3 |
| R3 | 垂直切片（1 张地图可玩 + HD-2D 受光生效） | 已完成（新游戏门禁通过） | 本文 5 |
| R4 | 战斗与成长 | 已完成（门禁通过） | 本文 7 |
| R5 | 另两张地图 + 天气与光影打磨 | 已完成（门禁通过） | 本文 8 |
| R6 | 收尾验证与交付 | 未开始 |  |

## 2. R1 能力审计

### 2.1 开工检查

- 初始工作区分支：`feature/engine-lib`，`git status --short --branch` 无输出。
- 已创建并切换到目标分支：`feature/hd2d-wuxia-arpg`。
- 初始提交：`0a8fc8b0`（`feature/engine-lib`）。
- 环境：Windows x64；CMake 4.3.2；MSVC 19.44.35226；VS2022 BuildTools；Python 3.12.6；Pillow 12.3.0；GPU 本机 RTX 3070（历史记录口径）。
- 文档检查：`Test-Path docs/design/WUXIA_ARPG_PLAN.md` = `False`；`Test-Path docs/design/WUXIA_ARPG_PROGRESS.md` = `False`。两文件已由 R1 补建。

### 2.2 基线构建

第一次在未初始化 MSVC 环境的 PowerShell 中直接执行：

```powershell
cmake --build --preset windows-x64-debug
```

结果：`已失败`，exit=1，耗时 14.7s。错误为 `cl.exe` 找不到标准头 `string`（`C1083`），根因是 `INCLUDE` 等 MSVC 环境变量未设置，不是仓库源码问题。

正式基线在 VS2022 BuildTools 开发环境中执行：

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul && cmake --build --preset windows-x64-debug'
```

结果：`已通过`，exit=0，耗时 26.6s。日志保存于 `tmp/r1_baseline_build_vs.log`（tmp 被 `.gitignore` 忽略）。

### 2.3 gtest 基线

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul && ctest --preset windows-x64-debug'
```

结果：`已通过`，exit=0，总 real time 53.05s。5/5 通过，0 失败：

| 测试 | 结果 | 耗时 |
|---|---:|---:|
| `gtest.engine.unit` | Passed | 21.51s |
| `gtest.engine.integration` | Passed | 27.69s |
| `gtest.release_matrix` | Passed | 0.10s |
| `gtest.engine.smoke.headless` | Passed | 0.17s |
| `gtest.engine.smoke.gpu` | Passed | 3.54s |

### 2.4 补充能力证据

#### 2.4.1 现有 HD-2D M6 验收场景三后端运行

```powershell
python templates\hd2d_wuxia\tools\run_hd2d_m6_acceptance.py --backends opengl,vulkan,d3d11 --out-dir tmp\r1_m6
```

结果：`已通过`，exit=0，耗时 16.1s；3 张 1280x720 截图生成。像素统计如下：

| 后端 | mean_luma | bright_ratio | warm_emissive_ratio |
|---|---:|---:|---:|
| OpenGL | 65.75 | 0.19797 | 0.00000 |
| Vulkan | 69.18 | 0.22169 | 0.00000 |
| D3D11 | 65.64 | 0.19382 | 0.00000 |

GL vs D3D11 `PSNR=11.28dB SSIM=0.4559`；GL vs Vulkan `PSNR=11.99dB SSIM=0.5577`；Vulkan vs D3D11 `PSNR=11.75dB SSIM=0.5288`。这些差异存在，后续轮次需在统一相机/帧数下复测；R1 只确认三后端均能启动、出图、非空。

#### 2.4.2 模板 B+ 村庄场景烟测

```powershell
$env:DSE_HD2D_BPLUS='1'
$env:DSE_HD2D_AUTOSTART='1'
$env:DSE_MAX_FRAMES='30'
$env:DSE_SCREENSHOT_FRAME='20'
$env:DSE_SCREENSHOT_PATH="$PWD\tmp\r1_template_village.png"
bin\dsengine_lua_debug.exe --script=templates\hd2d_wuxia\scripts\main.lua
```

结果：`已通过`，exit=0，耗时 11s，`tmp/r1_template_village.png` 存在。关键日志：

```text
[assets] loaded 419 textures (419 ordered), audio ready
[bplus] map=village mesh=templates/hd2d_wuxia/assets/maps/village_3d.dmesh lights=13
[hd2d] autostart=1 demo=false
[hd2d] 青溪问剑 ready. maps=2
DSE_SCREENSHOT_WRITTEN ... size=1280x720
```

注意：退出时出现 `RHI resource ledger detected live GL objects: textures=2, framebuffers=1, buffers=27` 警告。该轮不判失败，但列入 R3 风险观察项。

#### 2.4.3 素材生成工具语法检查

```powershell
python -m py_compile templates\hd2d_wuxia\tools\*.py
```

结果：`已通过`，9/9 个 Python 工具通过语法编译，无失败。

### 2.5 能力矩阵

| 要求 | 当前能力 | 证据（实际代码/命令） | 缺口 | 目标轮次 |
|---|---|---|---|---|
| DSEngine 构建 | `windows-x64-debug` 可用 | `cmake --build` exit=0；`out/build/windows-x64-debug` | 需每次在 MSVC dev env 下跑 | R1 已建立命令 |
| gtest 基线 | 5 个 gtest 入口全绿 | `ctest --preset windows-x64-debug` 5/5 | 新游戏逻辑尚无测试；后续补 | R3/R4/R6 |
| HD-2D Sprite3D | 组件、pass、lit 路径已存在 | `engine/ecs/components_3d_render.h:105`；`engine/render/passes/sprite3d_pass.h:47` | 完整游戏表现层需端到端验证 | R3/R5 |
| 三后端渲染 | GL/VK/D3D11 shader 引入与 M6 runner 均通过 | `gl_shader_manager.cpp:66`、`vulkan_shader_manager.cpp:33`、`dx11_shader_manager.cpp:16`；M6 runner exit=0 | 当前模板默认只跑 GL；需每张地图三后端复测 | R3/R5/R6 |
| Lua 绑定 | codegen + 手写兼容层 | `tools/codegen/binding_defs.json`；`engine/scripting/lua/bindings/` 共 107 个 `.cpp`（92 个 `*.gen.cpp` + 15 个手写）；`ecs.add_sprite3d`、`set_sprite3d_lit`、`assets.load_sprite_atlas` 见 `lua_binding_compat.cpp:503,506,512` | 新 API 必须先 grep；不得臆造 | 持续 |
| 着色器生成链 | 源文件编译为 gen.h/spv，generated 被忽略 | `CMakeLists.txt:1187`；`.gitignore:206`；`engine/render/shaders/src/sprite3d*.{vert,frag}` | 任何渲染改动需重新生成并三后端同步 | 持续 |
| 地图数量 | 现有 2 张 | `templates/hd2d_wuxia/tools/gen_maps.py:25` `MAP_SPECS = [`；`mapdata.lua:4` village、`:245` stronghold | 缺第 3 张；R2 设计、R5 实现 | R2/R5 |
| 战斗/成长 | 三段连招、闪避、2 技能、等级/经验、金币、基础掉落 | `data.lua:6`、`:23`、`:33`；`player.lua:103`、`:121` | 装备只有两个 bool；无随机词缀/稀有度/刷子循环深度 | R2/R4 |
| 天气/光影 | 只有落叶/萤火，B+ 路径有暖点光/CSM/Bloom/接地阴影 | `fx.lua:102`、`:106`；`bplus.lua:106` lit Sprite3D、`:296` `MESH_LIT` | 无雨/雾/雪/过渡/天气影响光照；另两图未打磨 | R5 |
| 存档 | 已有基础存档 | `save.lua:7`、`:27` | 新装备/地图/天气状态需扩展并回归 | R3/R4 |
| 素材许可 | 默认程序化生成 | `templates/hd2d_wuxia/README.md:4` 明确全部由脚本程序化生成、不依赖第三方素材 | 无逐条 ledger；需在 R2 补齐 | R2 |
| 完整 B+ 游戏 | 模板已有可选 B+ presenter | 模板 B+ 村庄烟测 exit=0；M3M6 报告记录完整迁移未完成 | 需真实地图端到端、无 live-object 警告 | R3/R5/R6 |
| 硬件/远程机 | 本机 RTX 3070 可用 | M6 runner 三后端 exit=0 | GT 1030 远程台式机不可达/未执行；不得冒充已修复 | R6 或环境暂缓 |

### 2.6 缺口与风险

1. **第 3 张地图缺失**：`MAP_SPECS` 只有 `village`、`stronghold`。R2 定第 3 张地图设计，R5 实现并连通。
2. **刷子深度不足**：掉落是固定概率表，装备仅 `iron_sword` / `leather_armor` 两个 bool，无随机词缀/稀有度/ilvl/数值 roll。R2 设计、R4 实现。
3. **天气过窄**：模板只有 `leaf` / `firefly`；无雨/雾/雪、无天气过渡与光照联动。R5 打磨。
4. **完整 B+ 游戏层未验收**：现有证据主要是 M6 验收场景与村庄烟测；实际战斗、掉落、Boss 在三后端的完整链路要在 R3/R5 补证据。
5. **live GL objects 警告**：退出时 ledger 报 textures/framebuffers/buffers 未释放完。先作为风险，R3 起每次运行比较是否恶化；若影响测试或稳定性，按渲染后端同步规则定位。
6. **Pillow 依赖**：资产生成工具依赖 Pillow 12.3.0；这是现有生成工具链依赖，不新增库，但必须在 R2 的生成命令/环境说明中记录。若环境无 Pillow，优先使用已提交的生成产物，不临时引入新库。
7. **陈旧的 M3M6 结论**：M3M6 报告记录完整 B+ 表现层迁移未完成；实际脚本已有 B+ 迁移路径，需在 R3 用真实运行重新得出结论并更新文档口径。
8. **性能/内存**：当前模板加载 419 张纹理。三张地图全量资源和天气后需用 headless stats 测内存/帧时间，避免只追求功能。
9. **API 幻觉风险**：新增玩法必须先 grep `binding_defs.json` 和 `lua_binding_compat.cpp` / `*_free_*.gen.cpp`；不命中的能力改为现有 API 或 C++ 实现。
10. **许可台账缺失**：R1 未发现商业解包素材引用，但缺少逐条 source/author/license 记录；R2 必须先补 ledger 再扩大素材。

### 2.7 R1 门禁结论

| 门禁 | 结论 | 说明 |
|---|---|---|
| 轮次契约存在 | 已通过 | R1 已补建 `WUXIA_ARPG_PLAN.md`，并建立 PROGRESS |
| 基线构建 | 已通过 | VS2022 BuildTools dev env 下 exit=0；未初始化环境时曾 exit=1，已定位为环境问题 |
| gtest | 已通过 | 5/5，0 失败 |
| 三后端 HD-2D 启动/出图 | 已通过 | M6 runner GL/VK/D3D11 exit=0，均出 1280x720 图 |
| 模板 B+ 村庄运行 | 已通过 | exit=0，日志 `maps=2`、`lights=13`，截图写出；带 live-object 警告 |
| 素材许可审计 | 未执行/部分通过 | 已确认 README 声明程序化生成；逐条 ledger 未做，R2 必须完成 |
| 商业游戏素材检查 | 已通过（静态） | 在 `templates/hd2d_wuxia` 文本文件中未发现《逸剑风云决》/解包/商业引用；R2 继续逐条核验 |
| GT 1030 台式机真机 | 环境暂缓 | 远程机不可达；不得把本机 RTX 3070 结果写成 GT 1030 结果 |
| 完整游戏三地图 | 未执行 | 属于 R3/R5/R6 目标 |

R1 审计结论：`已通过`。下一轮进入 R2：先产出完整设计和逐条素材许可清单，不进入大规模玩法实现。

## 3. R2 素材清单 + 完整设计

### 3.1 R2 开工基线

- 分支：`feature/hd2d-wuxia-arpg`，开始时工作区 clean。
- 构建：
  - `cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul && cmake --build --preset windows-x64-debug'`
  - 结果：`已通过`，exit=0，12.9s。
- gtest：
  - `cmd /c 'call "...\VsDevCmd.bat" -arch=x64 >nul && ctest --preset windows-x64-debug'`
  - 结果：`已通过`，exit=0，49.90s，5/5 通过，0 失败。

### 3.2 R2 产物

| 产物 | 路径 | 规模 |
|---|---|---:|
| 完整设计 | `docs/design/WUXIA_ARPG_DESIGN.md` | 19,451 bytes / 216 行 |
| 逐条资产台账 Markdown | `docs/design/WUXIA_ARPG_ASSET_LEDGER.md` | 120,452 bytes / 746 行 |
| 逐条资产台账 CSV（含 SHA-256） | `docs/design/WUXIA_ARPG_ASSET_LEDGER.csv` | 149,294 bytes / 713 行（含表头） |

资产台账逐条统计：712 条受版本控制的模板文件，其中：
- 角色/敌人/NPC/FX 序列帧 PNG：408
- 图集描述/图集 PNG：210
- UI 素材 PNG：27
- Lua 游戏源码：23
- 音频素材 WAV：17
- 素材生成/验收工具源码：9
- 地图分层/程序化 PNG：8
- 手写/测试 `.dsprite.json`：2
- 字体度量 Lua（生成）：2
- 3D 地图网格：2
- 中文位图字体图集 PNG：2
- 文档：1
- 地图数据 Lua（生成）：1

### 3.3 R2 许可合规修复：字体图集改用 OFL 字体

R2 审计发现 `templates/hd2d_wuxia/tools/gen_font.py` 原先从 `templates/topdown_3d/assets/font/SimHei.ttf` 取字形，来源与再分发许可不清晰。R2 已修复：

- `gen_font.py` 的 `FONT_CANDIDATES` 现在只引用：
  - `templates/hd2d_wuxia/assets/font/NotoSansSC-Regular.ttf`（可选本地覆盖）
  - `apps/editor_cpp/fonts/NotoSansSC-Regular.ttf`（仓库内 OFL 字体）
- 不再引用 `SimHei.ttf`；找不到 OFL 字体时脚本直接报错，避免静默回退到不明字体。
- 已用 Noto Sans SC Regular 重新生成：
  - `templates/hd2d_wuxia/assets/ui/font_small.png`
  - `templates/hd2d_wuxia/assets/ui/font_big.png`
  - `templates/hd2d_wuxia/scripts/font_small.lua`
  - `templates/hd2d_wuxia/scripts/font_big.lua`
- 重新生成输出：small=642 glyphs，big=642 glyphs。
- 字体后烟测：`bin\dsengine_lua_debug.exe` 以 `DSE_HD2D_BPLUS=1` 跑 village 12 帧，exit=0，截图写出。
- `python -m py_compile templates\hd2d_wuxia\tools\gen_font.py` exit=0。

### 3.4 设计覆盖检查

| R2 要求 | 设计覆盖 | 结论 |
|---|---|---|
| 3 张地图 | village / stronghold / youhuang，含尺寸、出口、敌人、Boss、天气、光照、掉落档次 | 已覆盖 |
| 随机词缀装备 | 基底、稀有度权重、前后缀池、ilvl/tier、确定性 RNG、装备槽与 UI | 已覆盖 |
| 战斗与成长 | 三段连招、闪避、6 技能、精英/冠军/Boss 三阶段、等级成长 | 已覆盖 |
| 刷子循环 | 刷怪 -> 掉落 -> 换装 -> 精英/冠军/Boss -> 再刷；悬赏可重复 | 已覆盖 |
| 任务/NPC/存档 | 5 段主线 + 可选悬赏；扩展 `dse.serialize` 存档结构 | 已覆盖 |
| HD-2D 渲染 | lit Sprite3D、MESH_LIT 地形、灯光、接触阴影、Bloom/后处理、相机 | 已覆盖 |
| 天气 | clear/leaf/light_rain/rain/storm/fog_firefly/snow，含过渡与光照联动 | 已覆盖 |
| API 真实性 | 20+ 真实 API 证据表，逐条列 `文件:行` | 已覆盖 |
| 测试矩阵 | T-BUILD/T-GTEST/T-R3-GAME/T-R3-LIT/T-R4-LOOT/T-R4-COMBAT/T-R5-MAPS/T-R5-WEATHER/T-R6-M6/T-R6-FULL | 已覆盖 |
| 资产许可 | 712 条逐条记录 source/author/license/command，CSV 带 SHA-256 | 已覆盖 |
| 禁止商业素材 | 静态检查命令与结果 `NO_HITS` | 已通过 |

### 3.5 静态合规检查

```powershell
$texts = Get-ChildItem templates\hd2d_wuxia -Recurse -File -Include *.py,*.lua,*.md,*.json
$texts | Select-String -Pattern '逸剑风云决|解包素材|商业游戏|unpack|commercial game' -CaseSensitive:$false
```

结果：`NO_HITS`。R2 未发现《逸剑风云决》或其他商业游戏解包素材引用。

### 3.6 R2 门禁结论

| 门禁 | 结论 | 说明 |
|---|---|---|
| 完整设计 | 已通过 | 3 图、刷子装备、战斗成长、Boss、天气、UI、存档、测试矩阵全部设计 |
| 逐条素材台账 | 已通过 | 712 条 source/author/license/command，CSV 带 SHA-256 |
| 字体许可修复 | 已通过 | SimHei 已替换为 Noto Sans SC（SIL OFL 1.1），字体重新生成并烟测 |
| API 幻觉检查 | 已通过 | 设计只使用 `lua_binding_*` 已证实 API，附 `文件:行` |
| 商业素材静态检查 | 已通过 | `NO_HITS` |
| 玩法实现 | 未执行 | 属 R3-R6，R3 只做 1 张地图垂直切片 |
| GT 1030 真机 | 环境暂缓 | 远程机不可达，不得冒充 |

### 3.7 R2 提交与推送状态（已恢复）

- 本地提交：
  - `941463b4 fix(tools): use OFL Noto Sans SC for wuxia font atlas`
  - `0a9c1fdd docs(wuxia): add R2 design and asset ledger`
  - `79d51a8b docs(wuxia): record R2 push environment status`
- 后续网络恢复后重试成功：`git push` 返回 0，`ba0a6505..79d51a8b feature/hd2d-wuxia-arpg`。
- 结果：`已通过`。远端 `origin/feature/hd2d-wuxia-arpg` 与本地一致；未 push `master`。
- 注：R2 的模板设计/台账仍是历史产物；用户 R3 修正后，后续实现以新游戏和 `WUXIA_ARPG_NEW_ASSET_LEDGER.*` 为准。

R2 结论：设计/台账门禁 `已通过`，push `已通过`。

## 5. R3 新游戏垂直切片（用户修正后）

### 5.1 修正说明

R3 开工时用户明确指示：**不要使用游戏模板**，`templates/hd2d_wuxia` 素材垃圾且有 bug；必须做新游戏，素材也要全新。
因此已：
- 丢弃/回滚 R3 对 `templates/hd2d_wuxia` 的所有未提交改动；
- 新建独立游戏工程 `games/wuxia_arpg/`；
- 新建素材生成器 `games/wuxia_arpg/tools/gen_assets.py`，全部素材重新生成；
- 不使用 `templates/hd2d_wuxia` 的任何 PNG/WAV/DSprite/Lua/Bplus/UI/Save 代码；
- 唯一外部输入为 OFL 字体 `apps/editor_cpp/fonts/NotoSansSC-Regular.ttf`（新字体图集为 R3 生成输出）。

### 5.2 R3 基线

- 构建：`cmake --build --preset windows-x64-debug`（VS2022 BuildTools dev env），exit=0，13.6s。
- gtest：`ctest --preset windows-x64-debug`，exit=0，52.9s，5/5 通过，0 失败。
- R3 改动只增加 Lua/PNG/WAV/Python 资产与脚本，无 C++/CMake 改动。

### 5.3 新素材生成

```powershell
python games\wuxia_arpg\tools\gen_assets.py
```

结果：`已通过`，exit=0。生成：
- 4 方向主角/山贼图集：8 个 `.dsprite.json` + 8 张 atlas PNG（每套 18 帧，含 idle/walk/attack/dodge/hurt/die）
- 5 张程序化 3D 道具贴图：tree/bamboo/house/lantern/rock
- 1 张新字体图集 `assets/ui/font.png` + `scripts/font.lua`（183 glyphs，Noto Sans SC OFL 输入）
- 6 个新 WAV：5 个 SFX + 1 个 BGM loop
- 不依赖 `templates/hd2d_wuxia` 的任何资产。

新游戏资产台账：`docs/design/WUXIA_ARPG_NEW_ASSET_LEDGER.md` / `.csv`（47 条，含 SHA-256）。

### 5.4 R3 逻辑验收

```powershell
bin\dsengine_lua_debug.exe --script=games\wuxia_arpg\scripts\_logic_test.lua
```

结果：`已通过`，exit=0，输出：
```text
[r3-logic] PASS rng=3336926330 items=30 save_items=30
```
覆盖：确定性 xorshift RNG、随机装备原型、`dse.serialize` 存档/读档 roundtrip。

### 5.5 R3 三后端验收

命令（每个后端单独执行）：

```powershell
python games\wuxia_arpg\tools\run_r3_acceptance.py --backends opengl --out-dir tmp\r3_new_game --max-frames 240 --shot-frame 210
python games\wuxia_arpg\tools\run_r3_acceptance.py --backends vulkan --out-dir tmp\r3_new_game --max-frames 240 --shot-frame 210
python games\wuxia_arpg\tools\run_r3_acceptance.py --backends d3d11 --out-dir tmp\r3_new_game --max-frames 240 --shot-frame 210
```

结果：`已通过`，三个后端 exit=0，`[r3] new-game acceptance PASS`。

| 后端 | lit-on mean_luma | lit-off mean_luma | delta | 游戏截图 mean_luma | 结论 |
|---|---:|---:|---:|---:|---|
| OpenGL | 68.13 | 7.80 | 60.33 | 72.88 | 已通过 |
| Vulkan | 67.99 | 7.37 | 60.62 | 72.63 | 已通过 |
| D3D11 | 67.80 | 7.80 | 60.00 | 72.87 | 已通过 |

游戏演示自动完成并输出以下真实标记：
- `[wuxia] map=qingxi_village`
- `[wuxia] attack dir=... targets=...`
- `[wuxia] exp+...`
- `[wuxia] gold+...`
- `[wuxia] drop uid=... name=... rarity=...`
- `[wuxia] autosave ok=true`
- `DSE_SCREENSHOT_WRITTEN ... 1280x720`
- 日志：`tmp/r3_new_opengl.log`、`tmp/r3_new_vulkan.log`、`tmp/r3_new_d3d11.log`
- 截图：`tmp/r3_new_game/r3_game_{opengl,vulkan,d3d11}.png`（tmp 被忽略，数值已写入本文件）

### 5.6 侧身造型修复

- 用户反馈：人物侧身造型看起来像三个。
- 根因：`games/wuxia_arpg/tools/gen_assets.py` 早期侧身帧复用了正面躯干，剑刃从身体正中穿过、双腿分离，导致视觉上出现多个竖直人形轮廓。
- 修复：重写 `human()` 的侧身绘制，改为更窄的侧向躯干、单侧持剑/持械、连续衣摆和单腿/前后脚轮廓；重新生成全部 actor 图集。
- 复验：三个后端重新执行 `run_r3_acceptance.py`，均 `new-game acceptance PASS`；受光 luma delta 仍约 60。
- 新生成器与资产仍在 `games/wuxia_arpg/`，未使用模板素材。
### 5.7 R3 门禁结论

| 门禁 | 结论 | 说明 |
|---|---|---|
| 新游戏独立于模板 | 已通过 | 代码/素材全在 `games/wuxia_arpg`，未使用模板 |
| 1 张地图可玩 | 已通过 | `qingxi_village` 移动/攻击/碰撞/敌人/死亡/掉落/升级/存档 |
| HD-2D 受光生效 | 已通过 | 三后端 lit on/off luma delta 约 60 |
| 掉落/升级/存档闭环 | 已通过 | 逻辑测试 + 演示日志 `drop/exp/levelup/autosave` |
| 三后端 | 已通过 | OpenGL/Vulkan/D3D11 均 exit=0 且出图 |
| 新素材许可 | 已通过 | 新台账逐条记录；字体输入为 OFL Noto |
| 禁止商业解包 | 已通过 | 未引用任何商业游戏素材 |
| 第 2/3 张地图 | 未执行 | 属 R5 |
| Boss/完整战斗成长 | 未执行 | 属 R4 |

R3 结论：`已通过`。下一轮 R4：在新游戏 `games/wuxia_arpg` 上深化战斗与成长，不触碰模板。

## 7. R4 战斗与成长

### 7.1 R4 开工基线

- 构建：`cmake --build --preset windows-x64-debug`，exit=0，13.0s。
- gtest：`ctest --preset windows-x64-debug`，exit=0，52.3s，5/5 通过，0 失败。
- 只改 `games/wuxia_arpg/`，继续不使用模板。

### 7.2 R4 实现

- 三段连招：`combo=1/2/3`，第三段高伤、暴击与更大范围。
- 技能：`U` 分花拂柳 AoE；`I` 紫霞真气治疗 + 攻击增益。
- 敌人 rank：normal / elite / champion / boss。
- Boss `boss_blood_blade`：320 HP，66%/33% 触发 phase 2/3，必掉装备。
- 装备：6 种基底、4 档稀有度、9 类随机词缀，计算战力并自动装备。
- 背包：`Tab` 打开，`W/S` 选择，`J` 装备；`run_r4` 不要求人工操作。
- 存档：装备实例与装备槽 `uid` roundtrip 后重新链接。
- 新素材：Boss 4 方向图集（新生成），字体图集扩充到 210 glyphs；均更新到新资产台账。`WUXIA_ARPG_NEW_ASSET_LEDGER.*` 由 `gen_ledger.py` 自动生成，共 59 条。

### 7.3 R4 自动化验收

逻辑测试：

```powershell
bin\dsengine_lua_debug.exe --script=games\wuxia_arpg\scripts\_r4_logic_test.lua
```

结果：`已通过`，exit=0：
```text
[r4-logic] PASS power=35.6 items=1 equip=armor
```

战斗测试：

```powershell
bin\dsengine_lua_debug.exe --script=games\wuxia_arpg\scripts\_r4_combat_test.lua
```

结果：`已通过`，exit=0：
```text
[wuxia] skill=zixia hp=102 buff=1.35
[r4-combat] PASS combo=3 d1=18 d3=28 boss_phase=3 items=1
```

### 7.4 R4 三后端演示验收

命令（每个后端单独执行）：

```powershell
python games\wuxia_arpg\tools\run_r4_acceptance.py --backends opengl --out-dir tmp\r4_final --max-frames 600 --shot-frame 560
python games\wuxia_arpg\tools\run_r4_acceptance.py --backends vulkan --out-dir tmp\r4_final --max-frames 600 --shot-frame 560
python games\wuxia_arpg\tools\run_r4_acceptance.py --backends d3d11 --out-dir tmp\r4_final --max-frames 600 --shot-frame 560
```

全部 `已通过`，exit=0，`[r4] acceptance PASS`。

| 后端 | lit-on luma | lit-off luma | delta | 游戏截图 luma | 结论 |
|---|---:|---:|---:|---:|---|
| OpenGL | 68.67 | 8.50 | 60.17 | 72.19 | 已通过 |
| Vulkan | 68.60 | 8.16 | 60.44 | 72.00 | 已通过 |
| D3D11 | 68.60 | 8.50 | 60.10 | 72.19 | 已通过 |

演示日志真实出现：
- `[wuxia] combo=2` / `[wuxia] combo=3`
- `[wuxia] skill=fenhua`
- `[wuxia] attack combo=3`
- `[wuxia] boss_phase=2` / `[wuxia] boss_phase=3`
- `[wuxia] boss_dead kind=boss_blood_blade`
- `[wuxia] equip slot=weapon/armor`
- `[wuxia] autosave ok=true`

日志：`tmp/r4_runner_opengl.log`、`tmp/r4_runner_vulkan.log`、`tmp/r4_runner_d3d11.log`。
截图数值与路径：`tmp/r4_final/r4_game_{opengl,vulkan,d3d11}.png`（tmp 被忽略）。

### 7.5 R4 门禁结论

| 门禁 | 结论 | 说明 |
|---|---|---|
| 三段连招/技能 | 已通过 | logic + combat test + demo 日志 |
| 装备/词缀/成长 | 已通过 | 随机装备、战力、自动装备、存档 roundtrip |
| 精英/冠军/Boss | 已通过 | elite/champion 掉落，Boss 三阶段/击杀 |
| 三后端无回归 | 已通过 | OpenGL/Vulkan/D3D11 全部 PASS |
| 受光未回归 | 已通过 | lit delta 约 60 |
| 新素材许可 | 已通过 | Boss 图集/新字体图集进入 `WUXIA_ARPG_NEW_ASSET_LEDGER.*` |
| 第 3 张地图/天气 | 未执行 | 属 R5 |
| GT 1030 真机 | 环境暂缓 | 仍不可达，不得冒充 |

R4 结论：`已通过`。下一轮 R5：在 `games/wuxia_arpg` 上追加 2 张新地图（合计 3 张）与天气/光影打磨。

## 8. R5 三图与天气/光影打磨

### 8.1 R5 开工基线

- 构建：`cmake --build --preset windows-x64-debug`，exit=0，19.8s。
- gtest：`ctest --preset windows-x64-debug`，exit=0，52.6s，5/5 通过，0 失败。
- 继续只改 `games/wuxia_arpg/`。

### 8.2 R5 实现

- `data.lua` 改为多地图模型：
  - `qingxi_village`：暮色青溪村，默认落叶，北 -> 黑风寨。
  - `blackwind_stronghold`：夜雨黑风寨，默认雷暴，南 -> 青溪村，北 -> 幽篁秘谷。
  - `youhuang_valley`：幽篁秘谷，默认雾，南 -> 黑风寨。
- 三图共 3 条连通边；`DSE_WUXIA_TOUR=1` 可自动走完三图。
- `weather.lua` 新天气系统：
  - 天气：晴 / 落叶 / 雨 / 雷暴 / 雾 / 雪。
  - 新生成雨、雪、雾、落叶粒子贴图。
  - 天气影响方向光颜色/强度、灯笼点光强度/色温、曝光与 Bloom。
  - 雷暴含周期性闪电曝光脉冲。
- `terrain.lua` 支持 `clear/build`，真实销毁/重建三图地形和道具。
- `main.lua` 支持 `DSE_WUXIA_MAP` / `DSE_WUXIA_WEATHER` / `DSE_WUXIA_TOUR`，并保留 R4 战斗成长。
- 新素材：三图共用的天气粒子、字体图集扩充到 222 glyphs；新资产台账由 `gen_ledger.py` 自动更新，共 66 条。

### 8.3 R5 自动化验收

命令：

```powershell
python games\wuxia_arpg\tools\run_r5_acceptance.py --backends opengl,vulkan,d3d11 --out-dir tmp\r5_final
```

结果：`已通过`，exit=0，耗时 123.5s。

覆盖：
- R4 逻辑/战斗回归：`PASS`。
- `_r5_maps_test.lua`：`[r5-maps] PASS maps=3 weather=落叶/雷暴/雾`。
- 三后端  4 个地图/天气用例：
  - `qingxi_village/leaf`
  - `blackwind_stronghold/storm`
  - `youhuang_valley/fog`
  - `youhuang_valley/snow`
- 三后端自动巡图 `DSE_WUXIA_TOUR=1`：均依次经过 `qingxi_village -> blackwind_stronghold -> youhuang_valley`。

真实像素统计示例：

| 后端/地图/天气 | mean_luma | bright_ratio |
|---|---:|---:|
| OpenGL 青溪村/落叶 | 60.50 | 0.00000 |
| OpenGL 黑风寨/雷暴 | 60.43 | 0.00000 |
| OpenGL 幽篁谷/雾 | 57.34 | 0.00000 |
| OpenGL 幽篁谷/雪 | 60.71 | 0.00400 |
| Vulkan 幽篁谷/雪 | 60.31 | 0.00263 |
| D3D11 幽篁谷/雪 | 60.67 | 0.00400 |

日志：`tmp/r5_runner_all.log`；截图：`tmp/r5_final/r5_{backend}_{map}_{weather}.png`（tmp 被忽略，数值已记录）。

### 8.4 R5 门禁结论

| 门禁 | 结论 | 说明 |
|---|---|---|
| 3 张地图 | 已通过 | `qingxi_village` / `blackwind_stronghold` / `youhuang_valley` |
| 地图连通 | 已通过 | tour 日志真实走完三图 |
| 天气系统 | 已通过 | 晴/落叶/雨/雷暴/雾/雪；粒子 + 光影 |
| 三后端覆盖 | 已通过 | OpenGL/Vulkan/D3D11 三图用例全部 PASS |
| R4 回归 | 已通过 | R4 逻辑/战斗测试仍 PASS |
| 新素材/台账 | 已通过 | 天气粒子与字体图集已进入 `WUXIA_ARPG_NEW_ASSET_LEDGER.*` |
| GT 1030 真机 | 环境暂缓 | 仍不可达，不得冒充 |

R5 结论：`已通过`。下一轮 R6：收尾验证与交付，重点做全量三图三后端、存档/读档、发布整理和最终资产审计。

## 9. R6 入口条件

- 分支保持 `feature/hd2d-wuxia-arpg`。
- 只做验证、修复、文档、交付整理，不新增完整地图/大系统。
- R6 先跑 R5 验收作为回归基线，再执行三图  三后端全矩阵。
- 必须给出存档/读档、资产台账、三后端截图/统计、git/push 状态。
- 不 push `master`。