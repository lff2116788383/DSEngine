# HD-2D 武侠刷子 ARPG 轮次契约 (WUXIA_ARPG_PLAN)

> 文档版本：v1.0
> 建立日期：2026-09-18（R1 审计时补建；见 PROGRESS 的缺失说明）
> 目标分支：`feature/hd2d-wuxia-arpg`
> 基线分支/提交：`feature/engine-lib` @ `0a8fc8b0`
> 总轮次预算：`max_goal_rounds = 12`（6 个阶段；至少 6 轮，剩余预算用于未过门重试）
> 进度与 Goal 状态源：`docs/design/WUXIA_ARPG_PROGRESS.md`

## 0. Goal

用 DSEngine 制作 HD-2D 武侠刷子 ARPG（暗黑 2 式，3 张地图）。严格按本文件的「轮次契约」逐轮推进，每轮只做一个阶段，把证据写入 `docs/design/WUXIA_ARPG_PROGRESS.md`。

硬性目标：
- 3 张可玩地图，HD-2D 受光生效。
- 暗黑 2 式核心循环：刷怪 -> 掉落随机装备/词缀 -> 成长/换装 -> 挑战更高强度内容 -> 再刷。
- 战斗与成长：连招、闪避、技能、等级、金币、背包、装备、Boss、存档。
- OpenGL / Vulkan / D3D11 三后端均有可复现证据。
- 素材许可清晰、逐条可追溯，禁止商业游戏解包素材。

## 1. 每轮开工与收尾契约

每轮开始必须完成：
1. 若 goal 工具可用，先 `get_goal`；当前工作环境未暴露该工具，则读取 PROGRESS 顶部的 Goal 状态块，功能等价，不得省略。
2. 读 `docs/design/WUXIA_ARPG_PLAN.md` 与 `docs/design/WUXIA_ARPG_PROGRESS.md`。
3. `git status --short --branch`；分支必须是 `feature/hd2d-wuxia-arpg`，工作区不得带与本轮无关的改动。
4. 跑一次基线构建：`cmake --build --preset windows-x64-debug`（Windows/MSVC 下必须先进入 VS2022 BuildTools `VsDevCmd.bat -arch=x64`）。
5. 跑一次 gtest：`ctest --preset windows-x64-debug`。
6. 只推进 `current_round`，完成前不得开始下一阶段；已完成阶段不得重复开工。

每轮结束必须完成：
1. 对本轮门禁逐项给出四档表述：`已通过 / 已失败 / 未执行 / 环境暂缓`；未生成目标、总数 0、未运行都不得写成已通过。
2. 把可复现命令、退出码、耗时、日志/截图/统计值写进 PROGRESS。
3. 更新 PROGRESS 的 Goal 状态、轮次状态、证据、风险、下一轮入口。
4. 使用 Conventional Commits 提交；只 push `feature/hd2d-wuxia-arpg`，绝不 push `master` / `main`。
5. 若门禁失败，`current_round` 不变，下一轮继续同一阶段；若 12 轮预算耗尽仍失败，把 Goal 标记为 `blocked`，如实说明，不伪完成。

## 2. 硬约束

- 只用许可清晰的素材；每条素材必须记录来源/作者/许可/生成命令/是否含第三方。默认只使用本仓库 `templates/hd2d_wuxia/tools/` 程序化生成的美术与音频。
- 禁止使用或提交任何商业游戏（含《逸剑风云决》）的解包素材、截图、音频、字体、数据表或逆向导出物。
- 禁止臆造 Lua 绑定或引擎 API；改脚本前先 grep 真实绑定定义与实现，优先使用 `engine/scripting/lua/bindings/` 中已有 API。
- 渲染特性改动必须同步 OpenGL / Vulkan / D3D11 后端；不得手改 `engine/render/shaders/generated/` 下的 `*.gen.h` / `*.spv`；必须通过 shader 编译链重新生成。
- 不新增未授权第三方库；不新增 submodule / vcpkg / npm 依赖。
- 不为了让测试变绿而放宽断言、删测试、跳测试；新增核心逻辑必须有回归测试或真实运行验证。
- 所有路径相对于仓库根目录；构建产物与缓存不得提交。
- 不改 `.gitignore` 绕过生成物忽略规则；`engine/render/shaders/generated/` 继续由 `.gitignore` 管理。

## 3. 轮次契约

| 轮次 | 阶段 | 只允许做的事 | 过门标准 | 产物 |
|---|---|---|---|---|
| R1 | 能力审计 | 读文档；基线构建/gtest；审计引擎、绑定、渲染、模板、资产许可；补建缺失的 PLAN/PROGRESS；不扩玩法 | 构建与 gtest 有真实结果；审计表覆盖引擎/渲染/Lua/资产/三后端/测试；缺口与风险有责任轮次；工作区干净 | `WUXIA_ARPG_PLAN.md`、`WUXIA_ARPG_PROGRESS.md` R1 节 |
| R2 | 素材清单 + 完整设计 | 只做设计与许可清单；可为可追溯性增加文档；不进入大规模实现 | 3 张地图、随机词缀装备、战斗成长、Boss、任务、UI、存档、测试矩阵全部设计到；每条素材有来源/作者/许可；所有 API 都经 grep 证实 | `WUXIA_ARPG_DESIGN.md`、`WUXIA_ARPG_ASSET_LEDGER.md`、PROGRESS R2 |
| R3 | 垂直切片：1 张地图可玩 + HD-2D 受光生效 | 只实现 1 张地图的端到端切片；允许扩展模板 Lua 与必要资源；不加入第 2/3 张地图 | 从标题进入该地图可移动/战斗/掉落/升级/拾取/存档；HD-2D lit Sprite3D + lit 3D 地形可见；GL/VK/D3D11 均 exit=0 且出图；lit on/off 有可复现数值差异；ctest 无回归 | 代码/脚本/资源/测试/截图证据，PROGRESS R3 |
| R4 | 战斗与成长 | 只深化战斗与成长：连招、技能、词缀装备、稀有度、背包、属性、等级、精英/Boss；不新增完整地图 | 刷 20 只以上敌人可形成掉落-换装-变强闭环；词缀/稀有度/装备/技能有自动化验证；失败断言不得放宽；三后端运行无回归 | 代码/脚本/测试/数据表，PROGRESS R4 |
| R5 | 另两张地图 + 天气与光影打磨 | 在 R3 的 1 张可玩地图基础上，把另 2 张地图做到同等可玩；实现天气/光影循环与过渡；不新增第 4 张地图 | 全游戏 3 张地图可从标题进入并互相连通；每张地图有独立敌人/掉落/光照/天气；三后端截图或运行证据；资源许可清单同步 | 3 张地图、天气/灯光配置/代码、截图/统计，PROGRESS R5 |
| R6 | 收尾验证与交付 | 只做验证、修复、文档、交付整理；可修 R3-R5 的回归，但不重开已完成阶段做新功能 | 全量构建/gtest/M6 风格验收；3 张地图GL/VK/D3D11；存档/读档；无商业素材；命令与真实结果完整；feature 分支已 push | 最终报告、验收脚本、截图/统计、提交与 push 记录，PROGRESS R6 |

## 4. 设计基线（R2 必须写清）

### 4.1 世界与地图
- 3 张地图必须命名、给出尺寸、碰撞、出生点、出口、敌人、NPC、掉落点、灯光、天气、BGM。
- 当前模板已有 2 张：`village`（暮色青溪村）、`stronghold`（夜雨黑风寨）。R3 只冻结 1 张作为切片；R5 将现有第 2 张纳入验收并新增第 3 张，合计 3 张。
- 地图数据以 `templates/hd2d_wuxia/tools/gen_maps.py` 的 `MAP_SPECS` 为唯一生成源，重新生成 `mapdata.lua` 与碰撞；禁止手改生成文件来糊验收。

### 4.2 刷子 ARPG 循环
- 装备：基底 + 稀有度（普通/魔法/稀有/传奇或等价四档）+ 随机词缀 + 等级需求 + 随机 roll。
- 掉落：按敌人类型/地图等级/幸运或等价参数生成，Boss 必掉保底。
- 成长：等级、经验、主属性/衍生属性、技能点或等价成长、金币消耗。
- 战斗：三段连招、闪避、无敌帧、击退、暴击、技能、Boss 多阶段。
- 背包/装备/存档：可持久化，读档后随机装备与地图状态可恢复。
- 所有随机系统必须可 seed 复现，便于测试与截图。

### 4.3 渲染与天气
- HD-2D：3D 程序化地形 + lit Sprite3D 角色/敌人/FX + 接触阴影 + 暖色点光 + 方向光/CSM + Bloom/色调/颗粒/FXAA 等现有后处理。
- 天气：至少晴/雨/雾/落雪或等价 3 种以上；天气切换有过渡，影响光照/粒子/环境色/音效。
- 若修改任何渲染特性：先 grep RHI/后端实现；三后端同步；只改 shader 源，重新生成，不手改 `*.gen.h`。

### 4.4 资产许可
- `WUXIA_ARPG_ASSET_LEDGER.md` 逐条记录：相对路径、类型、来源、作者、许可、生成/获取命令、是否含第三方、是否含商业游戏内容。
- 默认程序化生成素材：来源填 `templates/hd2d_wuxia/tools/`，作者填 `DSEngine Authors`，许可填 `Apache-2.0 (repository LICENSE)`，并给出生成命令。
- 任何外部素材仅允许明确 CC0/CC-BY/CC-BY-SA 或等价宽松许可；必须写作者、来源 URL、许可证文本位置；禁止 NC/ND/未知许可。
- 严禁《逸剑风云决》或其他商业游戏的任何解包/截图/音频/字体/数据。

### 4.5 Lua API 约束
- Lua 只能调用 `engine/scripting/lua/bindings/` 与 `docs/LUA_API.md` 已证实存在的 API。
- 当前人工绑定主要在 `lua_binding_compat.cpp`、`lua_binding_ecs.cpp`、`lua_binding_free_ecs_*.gen.cpp` 等；生成文件由 `tools/codegen/binding_defs.json` 经 codegen 生成。
- R2 设计中的每个新 API 必须先在绑定源码/defs 中 grep 命中；不命中的功能改用已有 API 或仅在 C++ 中实现，不得在 Lua 里臆造。

## 5. 每轮证据格式

每项验证统一写成：

```text
验证项：
结论：已通过 / 已失败 / 未执行 / 环境暂缓
环境：OS / 编译器 / GPU / 后端 / 提交
命令：
退出码/耗时：
原始结果：
证据路径：
备注：
```

禁止用应该可以看起来正常等口头结论代替命令、退出码、像素统计、测试计数或截图路径。

## 6. 阶段推进状态机

- `current_round` 只能按 R1R2...R6 前进。
- 过门后 `completed_rounds += current_round`，`current_round = next`，`attempt = 1`。
- 未过门时 `attempt += 1`，当前阶段不变，继续修复。
- 任何阶段不能跳过；R6 只能收尾，不能新增未设计的大功能。
- 任何后续轮次发现已完成阶段回归，在当前轮修复并把回归证据写入当前轮，不重开旧阶段。

## 7. R1 已确认的基线与缺口摘要

- 基线：`cmake --build --preset windows-x64-debug` 在 VS2022 BuildTools 环境下 exit=0；`ctest --preset windows-x64-debug` 5/5 通过。
- HD-2D 能力：`Sprite3DComponent`、`Sprite3DPass::RenderLit`、GL/VK/D3D11 的 `sprite3d_lit_vert.gen.h` 引入路径均已存在；M6 验收 runner 三后端 exit=0。
- Lua 绑定：`tools/codegen/binding_defs.json` + 107 个绑定源文件（92 个 `*.gen.cpp` + 15 个手写 `.cpp`）；`ecs.add_sprite3d`、`set_sprite3d_lit`、`assets.load_sprite_atlas` 等已在 `lua_binding_compat.cpp` 证实。
- 素材：模板 README 声称美术/音频全部脚本程序化生成、不依赖第三方素材；当前缺少逐条资产许可台账，R2 补。
- 主缺口：地图 2/3；装备与掉落深度不足；天气仅落叶/萤火；完整 B+ 游戏层仍需端到端验收；GT 1030 台式机证据环境暂缓。