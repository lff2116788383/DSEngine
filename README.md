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
- `3d_static_model`：资源化 `.dmesh/.dmat` 静态模型范例，对应 `samples/lua/3d/3d_static_model.lua`
- `3d_material_showcase`：材质参数 showcase，对应 `samples/lua/3d/3d_material_showcase.lua`
- `3d_lighting_showcase`：方向光/点光/聚光 showcase，对应 `samples/lua/3d/3d_lighting_showcase.lua`
- `3d_camera_showcase`：多相机切换 showcase，对应 `samples/lua/3d/3d_camera_showcase.lua`
- `3d_textured_cube`：贴图立方体范例，对应 `samples/lua/3d/3d_textured_cube.lua`
- `3d_scene_showcase`：P1 小型综合 3D 场景，对应 `samples/lua/3d/3d_scene_showcase.lua`
- `3d_skybox_environment`：SkyLight 环境色 showcase，对应 `samples/lua/3d/3d_skybox_environment.lua`
- `3d_postprocess_showcase`：Bloom 后处理 showcase，对应 `samples/lua/3d/3d_postprocess_showcase.lua`
- `3d_particles_showcase`：3D 粒子喷泉 showcase，对应 `samples/lua/3d/3d_particles_showcase.lua`
- `3d_physics_stack`：3D 刚体堆叠 showcase，对应 `samples/lua/3d/3d_physics_stack.lua`

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

### Lua 3D 自动验证脚本

推荐使用 `tools/verify_lua_3d_demos.py` 批量验证 Lua 3D demo。脚本会自动：

- 同步 `samples/` 到 `bin/samples/`，避免运行时仍读取旧的 `bin/samples/lua/main.lua` 或旧 config。
- 逐个修改 `bin/samples/lua/config.lua` 的 `Config.game_entry`。
- 设置截图和 readback 诊断环境变量。
- 运行 `bin/DSEngine_lua_debug.exe`。
- 输出每个 demo 的日志和截图。
- 结束后恢复原始 `bin/samples/lua/config.lua`。

最快完整验证流程：

```cmd
build_fast_lua.bat && python tools\verify_lua_3d_demos.py --entries all
```

只验证新 P0 Lua 3D demo：

```cmd
python tools\verify_lua_3d_demos.py --entries p0
```

只验证新 P1 Lua 3D demo：

```cmd
python tools\verify_lua_3d_demos.py --entries p1
```

只验证基础 3D demo：

```cmd
python tools\verify_lua_3d_demos.py --entries basic
```

只验证某一个入口，适合调试单个 demo：

```cmd
python tools\verify_lua_3d_demos.py --entries 3d_textured_cube --frames 90
```

默认输出目录：

```text
tmp/lua_3d_verify/
```

其中每个 demo 会生成：

```text
tmp/lua_3d_verify/lua_<entry>.log
tmp/lua_3d_verify/lua_<entry>.png
```

可用参数：

- `--entries basic|p0|p1|all|<entry...>`：选择验证范围，默认 `all`。
- `--frames <N>`：每个 demo 自动运行帧数，默认 `90`。
- `--timeout <SECONDS>`：单个 demo 超时时间，默认 `90`。
- `--out-dir <PATH>`：输出日志和截图目录，默认 `tmp/lua_3d_verify`。
- `--no-sync`：不重新同步 `samples/` 到 `bin/samples/`，仅在明确知道 `bin/samples/` 已最新时使用。

### 最快且稳定的 Lua demo 调试方式

经验上最稳定的调试方式是固定使用 `bin/samples` 作为运行时样例目录，并让脚本或命令在运行前同步源码样例。不要只改 `samples/lua/config.lua` 后直接运行宿主，因为宿主在本地常会解析到 `bin/samples/lua/main.lua`。

推荐流程：

1. 改 Lua demo 或资源文件。
2. 如果改了 C++ 引擎代码，先运行：

```cmd
build_fast_lua.bat
```

3. 如果只改 Lua 脚本/资源，可直接运行单 demo 验证：

```cmd
python tools\verify_lua_3d_demos.py --entries 3d_textured_cube --frames 90
```

4. 查看：

```text
tmp/lua_3d_verify/lua_3d_textured_cube.log
tmp/lua_3d_verify/lua_3d_textured_cube.png
```

5. 搜索日志中的关键错误：

```cmd
findstr /S /N /C:"[ERROR]" /C:"[WARN]" tmp\lua_3d_verify\*.log
```

稳定性注意事项：

- 单 demo 调试优先用 `python tools\verify_lua_3d_demos.py --entries <entry>`，比手动编辑 config 更不容易跑错入口。
- 如果需要手动运行宿主，先同步样例目录：

```cmd
if exist bin\samples rmdir /S /Q bin\samples && xcopy samples bin\samples /E /I /Y >nul
```

- 手动截图时建议使用 `DSE_MAX_FRAMES=90`、`DSE_SCREENSHOT_TARGET=main`、`DSE_RENDER_READBACK_DIAG=1`。
- 看到截图内容不符合预期时，先检查日志中的 `Lua bootstrap: startup script resolved to ...`，确认实际加载的是哪个 `main.lua`。

### 样例目录说明

`samples/lua/` 保留多个 Lua 示例，当前默认推荐入口为 2D 物理 showcase：
- `main.lua`：Lua 宿主入口，会根据 `config.lua` 的 `game_entry` 选择示例
- `config.lua`：运行配置，默认指向 `phase1_2d_physics_showcase`
- `phase1_2d_physics_showcase.lua`：2D 物理推荐范例
- `3d/triangle.lua`：3D 三角形基础范例
- `3d/square.lua`：3D 正方形基础范例
- `3d/cube.lua`：3D 立方体基础范例
- `3d/3d_static_model.lua`：资源化 Mesh/Material 静态模型范例
- `3d/3d_material_showcase.lua`：材质参数 showcase
- `3d/3d_lighting_showcase.lua`：3D 光照 showcase
- `3d/3d_camera_showcase.lua`：3D 相机切换 showcase
- `3d/3d_textured_cube.lua`：贴图立方体验证范例
- `3d/3d_scene_showcase.lua`：P1 小型综合 3D 场景
- `3d/3d_skybox_environment.lua`：SkyLight 环境色 showcase
- `3d/3d_postprocess_showcase.lua`：Bloom 后处理 showcase
- `3d/3d_particles_showcase.lua`：3D 粒子喷泉 showcase
- `3d/3d_physics_stack.lua`：3D 刚体堆叠 showcase
