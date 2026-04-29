### DSEngine

### 推荐入口

当前仓库默认推荐的 Lua 范例是 `samples/lua/phase1_2d_physics_showcase.lua`。

它覆盖以下链路：
- **2D 刚体下落**：动态箱体落到静态地面
- **触发器回调**：箱体穿过触发区域时输出 enter / exit 日志
- **射线检测**：水平射线持续探测并显示命中实体
- **命中高亮**：射线命中的实体会被高亮显示
- **射线可视化**：橙色点列显示当前射线路径

### Lua 范例运行方式

- **快速构建 Lua 宿主**
  - 运行 `build_fast_lua.bat`
- **直接构建目标**
  - `cmake --build build_vs2022 --config Debug --target dse_example_lua`
- **启动默认示例**
  - 从仓库根目录运行 `bin/DSEngine_lua_debug.exe`
  - 或运行 `bin/DSEngine_lua.exe`
- **从 `bin/` 目录启动**
  - 也可以在 `bin/` 目录中直接运行 `DSEngine_lua_debug.exe`，启动回归文件会写入当前 `bin/` 目录。

### 切换 Lua 范例

Lua 宿主默认加载 `samples/lua/main.lua`，该入口会读取 `samples/lua/config.lua` 中的 `Config.game_entry`。

常用入口值：
- `phase1_2d_physics_showcase`：默认 2D 物理 showcase
- `3d_triangle`：3D 三角形范例，对应 `samples/lua/3d/triangle.lua`
- `3d_square`：3D 正方形范例，对应 `samples/lua/3d/square.lua`
- `3d_cube`：3D 立方体范例，对应 `samples/lua/3d/cube.lua`

示例：将 `samples/lua/config.lua` 中的配置改为：

```lua
Config.game_entry="3d_cube"
```

如果运行时使用的是 `bin/samples/lua/main.lua`，需要先同步样例目录：

```powershell
if (Test-Path bin\samples) { Remove-Item bin\samples -Recurse -Force }
Copy-Item samples bin\samples -Recurse -Force
```

也可以通过环境变量直接指定启动脚本：

```powershell
$env:DSE_STARTUP_LUA='samples\lua\main.lua'
bin\DSEngine_lua_debug.exe
```

### Lua 范例截图方式

运行时支持通过环境变量自动截图：

- `DSE_SCREENSHOT_PATH`：截图输出路径；设置后会在退出前保存 PNG。
- `DSE_SCREENSHOT_TARGET`：截图来源，推荐使用 `main`，也可使用 `scene`。
- `DSE_MAX_FRAMES`：自动运行帧数，适合截图/回归验证时自动退出。
- `DSE_RENDER_READBACK_DIAG`：设置为 `1` 时输出 render target/readback 诊断日志。

PowerShell 示例：

```powershell
$env:DSE_STARTUP_LUA='samples\lua\main.lua'
$env:DSE_MAX_FRAMES='90'
$env:DSE_SCREENSHOT_TARGET='main'
$env:DSE_SCREENSHOT_PATH='tmp\lua_sample.png'
$env:DSE_RENDER_READBACK_DIAG='1'
bin\DSEngine_lua_debug.exe
```

验证 `samples/lua/3d` 三个范例时，可分别把 `Config.game_entry` 设置为 `3d_triangle`、`3d_square`、`3d_cube`，并把 `DSE_SCREENSHOT_PATH` 改成不同文件名，例如：

```powershell
$env:DSE_SCREENSHOT_PATH='tmp\lua_3d_cube.png'
```

### 样例目录说明

`samples/lua/` 保留多个 Lua 示例，当前默认推荐入口为 2D 物理 showcase：
- `main.lua`：Lua 宿主入口，会根据 `config.lua` 的 `game_entry` 选择示例
- `config.lua`：运行配置，默认指向 `phase1_2d_physics_showcase`
- `phase1_2d_physics_showcase.lua`：2D 物理推荐范例
- `3d/triangle.lua`：3D 三角形基础范例
- `3d/square.lua`：3D 正方形基础范例
- `3d/cube.lua`：3D 立方体基础范例
