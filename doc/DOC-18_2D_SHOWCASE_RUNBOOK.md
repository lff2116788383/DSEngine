# DOC-18 2D Showcase 最小运行说明

## 1. 目标

`phase1_2d_showcase` 用于提供一个最小但正式的 2D 对外展示入口，优先覆盖当前已经稳定的 phase1 运行时能力，而不是扩展新的系统面。

当前第一版覆盖项：

- camera
- sprite
- tilemap
- physics2d
- UI panel / button / label / numeric label
- particle burst
- audio loop
- animator

当前第一版暂未把“运行时主动切语言”列为必选演示项。原因是 Lua runtime 当前尚未暴露 localization bindings；该项保留为后续增强。

## 2. 入口文件

- [`samples/lua/main.lua`](samples/lua/main.lua)
- [`samples/lua/config.lua`](samples/lua/config.lua)
- [`samples/lua/phase1_2d_showcase.lua`](samples/lua/phase1_2d_showcase.lua)

默认入口已切到 `Config.game_entry = "phase1_2d_showcase"`。

## 3. 运行方式

### 3.1 Lua runtime

按现有项目方式启动 Lua runtime / launcher，对应会进入 [`samples/lua/main.lua`](samples/lua/main.lua) 并加载 `phase1_2d_showcase`。

### 3.2 运行后应看到的内容

启动后第一屏应包含以下元素：

1. 正交相机视图。
2. 一块 12x3 的 tilemap 地面。
3. 至少两个带 physics2d 的 sprite 物体。
4. 顶部 UI 面板、标题、副标题、Score / Combo 数值区。
5. 自动刷新的 numeric label。
6. 周期性 particle burst。
7. 自动播放的 BGM。
8. 一个 animator 驱动的展示物体。

## 4. 最小回归检查

建议每次修改 [`samples/lua/phase1_2d_showcase.lua`](samples/lua/phase1_2d_showcase.lua) 或相关 Lua bindings 后，至少检查以下 5 项：

1. **入口路由正确**
   - [`samples/lua/config.lua`](samples/lua/config.lua) 中 `game_entry` 指向 `phase1_2d_showcase`
   - [`samples/lua/main.lua`](samples/lua/main.lua) 能正确 `require("phase1_2d_showcase")`

2. **基础资源回退可工作**
   - `mirror_assets/...` 路径不可用时，`data/...` 前缀回退不会立即报错退出
   - 当前已确认 [`data/mirror_assets/Resources`](data/mirror_assets/Resources) 目录不存在，因此第一版脚本依赖现有 fallback 与运行时容错，不把“完整正式美术资源”作为第一版阻塞项

3. **动态 UI 更新正常**
   - Score / Combo 数值会自动变化
   - 提示文本会在两种状态之间切换

4. **动态表现正常**
   - particle emitter 会周期性 burst
   - audio pitch 会随循环轻微变化
   - 动态物体位置会持续变化

5. **不夸大能力边界**
   - 第一版只宣称已经接通的 2D runtime 能力
   - 不把 localization 运行时切换写成当前已完成项

## 5. 当前限制

- 当前工作区下未发现 [`data/mirror_assets/Resources`](data/mirror_assets/Resources) 目录，说明脚本里引用的资源路径并不等于“当前仓库内资源完备”。
- 因此第一版 showcase 更适合作为**能力入口与结构骨架**，而不是已经达到最终美术展示质量的正式内容包。
- 若后续要提升为更完整的正式演示，需要补齐稳定可追踪的 2D 资源目录，并把 UI / localization / audio 的资源依赖整理成可审计清单。
