# 快速上手：30 分钟做出并导出你的第一个 DSEngine 小游戏

> 面向**完全没碰过 DSEngine** 的人。跟着走完，你会得到一个能用键盘控制、可双击运行、可打包分发的 2D 小游戏——全程只用命令行 `dse`，不需要打开编辑器、不需要写 C++、不需要 GPU。
>
> 平台：Windows 10/11 x64。预计耗时 30 分钟（若直接下载预编译工具包，可省去首次编译，只需几分钟）。

---

## 你将得到什么

- 一个青色方块，用 **WASD / 方向键**上下左右移动
- 一个可双击启动的独立 exe + 资源包（`build/dist/`）
- 全部游戏逻辑写在一个 **Lua** 脚本里（`scripts/main.lua`），改完重新 `dse build` 即可

整条「黄金路径」就三步：

```
dse new 2d MyGame      →   dse build MyGame      →   运行 build/dist/MyGame.exe
（生成项目）                （拷贝运行时 + 打包）        （双击即玩）
```

---

## 第 0 步：拿到 `dse` 命令行（首次必做）

你需要 `dse.exe` 和运行时 `DSEngine_Game.exe`（`dse build` 会用到它）。二选一：

### 方式 A（推荐，最快）：下载预编译工具包

到本仓库的 **Releases** 页面下载 `DSEngine-tools-vX.Y.Z-win-x64.zip`，解压即可——里面有 `dse.exe` + `DSEngine_Game.exe`，开箱即用，**不需要装编译器、不需要从源码构建**。

- 下载直链：仓库 **Releases** 页（`<repo-url>/releases/latest`）的资产 `DSEngine-tools-*-win-x64.zip`

```powershell
# 解压到任意目录后，把该目录加进 PATH 或直接用全路径
.\dse.exe help
```

### 方式 B：从源码构建一次引擎

前置工具：Git、CMake 3.24+、Visual Studio 2022（C++ 桌面工作负载）。仓库自带脚本会自动装好工具链并构建：

```powershell
git clone --recursive <repo-url>
cd DSEngine
powershell -ExecutionPolicy Bypass -File scripts\bootstrap_windows.ps1
```

构建完成后确认这两个文件存在：

```powershell
dir bin\dse.exe
dir bin\DSEngine_Game_*.exe
```

> 维护者发布工具包：`powershell -ExecutionPolicy Bypass -File scripts\package_dse_tools.ps1`
> 会编译 `dse_cli`/`dse_standalone` 并打出上面那个 `DSEngine-tools-*.zip`，作为 Release 资产上传即可。

> 为了后面命令简短，可以把工具目录加进 PATH，或直接用全路径 `dse.exe`。下文统一写 `dse`。

---

## 第 1 步：创建项目（10 秒）

```powershell
dse new 2d MyGame
```

这会生成一个 `MyGame\` 目录，关键文件：

```
MyGame/
├── scripts/
│   └── main.lua        ← 你的全部游戏逻辑
├── assets/             ← 贴图/音频等资源（现在是空的）
└── project.dsproj      ← 项目描述
```

`dse new` 还支持这些模板：

| 模板 | 说明 |
|------|------|
| `empty` | 仅目录骨架，无脚本 |
| `2d`    | **本教程用的** 2D 玩法 + Lua 入口脚本（相机 + 可移动精灵） |
| `3d`    | 3D 演示场景（相机 + 平行光 + Lua 入口脚本） |
| `lua`   | 纯 Lua 入口脚本 |
| `cpp`   | C++ 宿主工程（`src/main.cpp` + `CMakeLists.txt`，需自己跑 CMake 编译） |

打开 `MyGame\scripts\main.lua` 看一眼，它已经是一个**能跑的完整小游戏**：

```lua
local app = dse.app
local KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP = 263, 262, 264, 265
local KEY_A, KEY_D, KEY_S, KEY_W = 65, 68, 83, 87

local player
local pos = { x = 0.0, y = 0.0 }
local speed = 5.0

-- Awake() 在启动时调用一次
function Awake()
    local camera = dse.ecs.create_entity()
    dse.ecs.add_transform(camera, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(camera, 5.0)

    player = dse.ecs.create_entity()
    dse.ecs.add_transform(player, pos.x, pos.y, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_sprite(player, 0.30, 0.80, 1.0, 1.0, 0)

    print("[MyGame] ready -- move the square with WASD or arrow keys")
end

-- Update(dt) 每帧调用一次，dt = 距上一帧的秒数
function Update(dt)
    dt = dt or 0.0
    local dx, dy = 0.0, 0.0
    if app.get_key(KEY_A) or app.get_key(KEY_LEFT)  then dx = dx - 1.0 end
    if app.get_key(KEY_D) or app.get_key(KEY_RIGHT) then dx = dx + 1.0 end
    if app.get_key(KEY_W) or app.get_key(KEY_UP)    then dy = dy + 1.0 end
    if app.get_key(KEY_S) or app.get_key(KEY_DOWN)  then dy = dy - 1.0 end
    pos.x = pos.x + dx * speed * dt
    pos.y = pos.y + dy * speed * dt
    dse.ecs.set_transform_position(player, pos.x, pos.y, 0.0)
end
```

**两个一定要记住的生命周期函数**（运行时按全局函数名调用，名字必须一字不差）：

- `Awake()` —— 启动时调用一次，用来建实体、相机、初始化
- `Update(dt)` —— 每帧调用一次，`dt` 是距上一帧的秒数，用来做移动 / 逻辑

> 输入 API 实际挂在 `dse.app` 下，所以脚本顶部用 `local app = dse.app` 起个别名后写 `app.get_key(...)` 更顺手。

---

## 第 2 步：构建并打包（30 秒）

```powershell
dse build MyGame
```

完成后产物在 `MyGame\build\dist\`：

```
dist/
├── MyGame.exe          ← 运行时可执行文件
├── game.bun            ← 打包后的脚本 + 资源
├── game.dsmanifest     ← 窗口标题 / 尺寸 / 启动画面 / 入口脚本配置
└── launch.bat          ← 启动脚本（等价于双击 exe，加密资源时需要它带 --key）
```

> 注意：`dse build` **不会重新编译引擎**——它把 `bin\` 下预构建的运行时拷过来、打包你的资源、生成启动脚本。所以构建只要几秒。

---

## 第 3 步：运行（玩！）

直接双击 `MyGame\build\dist\MyGame.exe` 即可（或在命令行里跑）：

```powershell
MyGame\build\dist\MyGame.exe
```

窗口出现后，按 **WASD** 或**方向键**移动青色方块。

> exe 会自动读取同目录的 `game.dsmanifest`（里面记录了入口脚本 `scripts/main.lua`）和 `game.bun` 资源包，所以双击即玩。
>
> 什么时候还需要 `launch.bat`？当你用 `--key` **加密**了资源包时——解密密钥要通过 `launch.bat` 里的 `--key=...` 参数传入，这种情况下用 `launch.bat` 启动。

### 没有独立显卡 / 在远程桌面或虚拟机里？

如果启动报 `Failed to create GLFW window`，说明环境没有可用的硬件 OpenGL。用仓库自带脚本部署软件渲染（Mesa3D llvmpipe）到 dist 目录即可：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup_swgl.ps1 -BinDir "MyGame\build\dist"
```

然后设置环境变量再启动：

```powershell
$env:GALLIUM_DRIVER = "llvmpipe"
MyGame\build\dist\MyGame.exe
```

---

## 第 4 步：改一行，立刻见效

打开 `MyGame\scripts\main.lua`，把移动速度调快一倍：

```lua
local speed = 10.0     -- 原来是 5.0
```

或者把方块换个颜色（RGBA，0~1）：

```lua
dse.ecs.add_sprite(player, 1.0, 0.4, 0.2, 1.0, 0)   -- 橙色
```

保存后重新构建并运行：

```powershell
dse build MyGame
MyGame\build\dist\MyGame.exe
```

想做更完整的玩法（加金币、计分、胜利条件）？接着看 [第一篇 2D 教程](TUTORIAL_2D_FIRST_GAME.md)。

---

## 第 5 步（可选）：导出到 Web

DSEngine 支持用 Emscripten 把游戏编译成可在浏览器里跑的 WebAssembly。这一步需要先装好 [EMSDK](https://emscripten.org/docs/getting_started/downloads.html)。

```powershell
# 编译 Web 产物（默认 web-release 预设）
dse build --target web

# 把产物（index.html/.js/.wasm/.data）收集成可上传的 Web 包
dse dist --target web --out dist\web
```

`dist\web` 整个目录压缩后即可上传到 itch.io 等平台。3D 项目加 `--3d`，调试构建加 `--debug`。

---

## 常用命令速查

```powershell
dse new <template> <dir>          # 创建项目（模板：empty/2d/3d/lua/cpp）
dse build <project>               # 构建 + 打包到 <project>\build\dist
dse build <project> --out=DIR     # 指定输出目录
dse build <project> --key=KEY     # 用 AES-128-CTR 加密资源包（KEY ≥ 16 字节）
dse pack <dir> <out.bun>          # 单独把一个目录打成资源包
dse build <project> --with-swgl   # 随包发行软件 OpenGL（无独显机器可跑）
dse build --target web [--3d]     # 用 Emscripten 编译 Web 产物
dse dist  --target web --out=DIR  # 收集 Web 产物为可上传包
dse dist  --target win|linux      # 把已构建目录打成 .zip / .tar.gz（Export Template）
dse help                          # 查看完整帮助
```

---

## 接下来

- [第一篇 2D 教程：金币收集小游戏](TUTORIAL_2D_FIRST_GAME.md) —— 在本模板基础上加金币、计分、胜利判定
- [Lua API 参考](../api/LUA_API.md) —— 全部可用的 `dse.*` 接口（对照源码核实）
- [从源码构建引擎 / 用编辑器 / 测试](GETTING_STARTED.md) —— 面向贡献者的完整构建指南

---

## 遇到问题？

| 现象 | 原因 / 解决 |
|------|------------|
| 双击 exe 是空窗口 | 确认同目录有 `game.dsmanifest`（记录入口脚本）和 `game.bun`；用 `--key` 加密过资源时改用 `launch.bat` 启动 |
| `Failed to create GLFW window` | 没有硬件 OpenGL，按第 3 步部署 llvmpipe 软件渲染 |
| 找不到 `dse.exe` | 还没拿到工具——按第 0 步方式 A 下载预编译工具包，或方式 B 跑 `bootstrap_windows.ps1` |
| 脚本改了没生效 | 改完要重新 `dse build`，再运行 exe |
| `Lua ... failed: attempt to index a nil value (global 'app')` | 脚本里直接用了 `app` 却没写 `local app = dse.app` 别名 |
