# HD-2D 武侠刷子 ARPG 进度（WUXIA_ARPG_PROGRESS）

## 0. Goal 状态

```yaml
goal:
  objective: "用 DSEngine 制作 HD-2D 武侠刷子 ARPG（暗黑 2 式，3 张地图）"
  status: in_progress
  max_goal_rounds: 12
  current_round: R1
  attempt: 1
  completed_rounds: []
  next_round: R2
  branch: feature/hd2d-wuxia-arpg
  baseline: feature/engine-lib @ 0a8fc8b0
  contract: docs/design/WUXIA_ARPG_PLAN.md
```

> 说明：本轮开始时用户要求读取 `docs/design/WUXIA_ARPG_PLAN.md`，但该文件与 PROGRESS 文件当时都不存在；R1 已按用户 objective 补建 PLAN，并建立本 PROGRESS。后续所有轮次以补建后的 PLAN 为轮次契约。

## 1. 轮次总表

| 轮次 | 阶段 | 状态 | 过门证据 |
|---|---|---|---|
| R1 | 能力审计 | 进行中 | 本文 2 |
| R2 | 素材清单 + 完整设计 | 未开始 |  |
| R3 | 垂直切片（1 张地图可玩 + HD-2D 受光生效） | 未开始 |  |
| R4 | 战斗与成长 | 未开始 |  |
| R5 | 另两张地图 + 天气与光影打磨 | 未开始 |  |
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

## 3. R2 入口条件

- 分支保持 `feature/hd2d-wuxia-arpg`。
- R2 只做设计资产台账，不改 C++ 渲染核心、不新增第三方库。
- R2 设计必须引用已 grep 证实存在的 Lua/C++ API；未证实 API 不能写进设计作为实现依据。
- R2 资产台账必须逐条覆盖现有与新增素材，明确来源/作者/许可/生成命令。