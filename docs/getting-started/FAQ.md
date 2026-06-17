# DSEngine FAQ / 常见问题

Common questions about getting `dse`, building, running, packaging, and distributing games. For a full walkthrough see [QUICKSTART](QUICKSTART.md) ([English](QUICKSTART.en.md)).

针对拿到 `dse`、构建、运行、打包、分发 的高频问题。完整上手见 [快速上手](QUICKSTART.md)。

---

## Getting the tools / 拿到工具

### Q: I don't have `dse.exe`. Where do I get it? / 我没有 `dse.exe`，去哪拿？

**EN:** Two options. (A) Download the prebuilt toolkit `DSEngine-tools-vX.Y.Z-win-x64.zip` from the repo **Releases** page and unzip — it ships `dse.exe` + `dsengine_game.exe`, no compiler needed. (B) Build from source once with `scripts\bootstrap_windows.ps1`; the binaries land in `bin\`.

**中文：** 两条路。(A) 到仓库 **Releases** 页下载预编译工具包 `DSEngine-tools-vX.Y.Z-win-x64.zip`，解压即用——内含 `dse.exe` + `dsengine_game.exe`，不需要编译器。(B) 用 `scripts\bootstrap_windows.ps1` 从源码构建一次，产物在 `bin\`。

### Q: `dse build` says it can't find the runtime. / `dse build` 找不到运行时怎么办？

**EN:** `dse build` does not compile the engine — it copies a prebuilt `dsengine_game*.exe`. It must sit next to `dse.exe` or in a sibling `bin\`. If you used the prebuilt toolkit, run `dse` from inside the unzipped folder. If you built from source, run the `dse.exe` in `bin\`.

**中文：** `dse build` 不编译引擎，而是拷贝预构建的 `dsengine_game*.exe`。它必须和 `dse.exe` 同目录，或在相邻的 `bin\` 里。用预编译工具包时，在解压目录内运行 `dse`；从源码构建时，用 `bin\` 里的 `dse.exe`。

---

## Running the game / 运行游戏

### Q: Double-clicking the exe opens an empty window or closes immediately. / 双击 exe 是空窗口或秒退。

**EN:** Make sure `game.dsmanifest` and `game.bun` are in the same folder as the exe (they are produced together by `dse build`). The manifest records the entry script (`scripts/main.lua`); without it the runtime has nothing to run. If you moved only the exe, copy the whole `dist\` folder.

**中文：** 确认 `game.dsmanifest` 和 `game.bun` 与 exe 在同一目录（它们由 `dse build` 一起产出）。manifest 记录入口脚本（`scripts/main.lua`），缺了它运行时没东西可跑。如果只移动了 exe，请整份 `dist\` 目录一起拷。

### Q: `Failed to create GLFW window`. / 启动报 `Failed to create GLFW window`。

**EN:** The machine has no usable hardware OpenGL (common on VMs, CI runners, RDP sessions). Deploy the Mesa3D llvmpipe software renderer into the dist directory and force it on:

**中文：** 机器没有可用的硬件 OpenGL（虚拟机、CI、远程桌面常见）。把 Mesa3D llvmpipe 软件渲染部署到 dist 目录并强制启用：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup_swgl.ps1 -BinDir "MyGame\build\dist"
$env:GALLIUM_DRIVER = "llvmpipe"
MyGame\build\dist\MyGame.exe
```

**EN:** To bake this into the build (set `GALLIUM_DRIVER` in the generated launch scripts and remind you to ship the Mesa DLLs), build with `dse build MyGame --with-swgl`.

**中文：** 想把它固化进出包（在生成的 launch 脚本里设好 `GALLIUM_DRIVER`，并提示随包带 Mesa DLL），用 `dse build MyGame --with-swgl`。

### Q: Performance is very low with software rendering. / 软件渲染下帧率很低。

**EN:** Expected — llvmpipe is a CPU rasterizer meant for GPU-less environments and testing, not shipping. Distribute to players with a real GPU; only use llvmpipe where no hardware OpenGL exists.

**中文：** 正常现象——llvmpipe 是 CPU 光栅器，用于无显卡环境和测试，不用于正式发行。面向有显卡的玩家分发；只在没有硬件 OpenGL 的地方用 llvmpipe。

---

## Packaging & encryption / 打包与加密

### Q: When do I need `launch.bat` instead of double-clicking the exe? / 什么时候要用 `launch.bat` 而不是双击 exe？

**EN:** For **plaintext** bundles, double-clicking the exe just works — it reads `game.dsmanifest` + `game.bun` automatically. You only need `launch.bat` when you packaged with `--key` (encrypted bundle): the decryption key is passed via the `--key=...` argument inside `launch.bat`, which the runtime needs to mount the encrypted `game.bun`. So **ship `launch.bat` (and tell players to use it) whenever the resources are encrypted.** A double-click of the bare exe on an encrypted bundle will fail to mount resources.

**中文：** **明文**资源包下，双击 exe 即可——它会自动读 `game.dsmanifest` + `game.bun`。只有当你用 `--key` 打包（加密资源包）时才需要 `launch.bat`：解密密钥通过 `launch.bat` 里的 `--key=...` 参数传入，运行时据此挂载加密的 `game.bun`。所以**资源加密时务必随包发 `launch.bat` 并让玩家用它启动**；对加密包直接双击裸 exe 会挂载资源失败。

### Q: How do I encrypt my resource bundle? / 怎么加密资源包？

**EN:** Pass `--key=KEY` (>= 16 bytes, AES-128-CTR) to `dse build`. The generated `launch.bat` embeds the key argument so the runtime can decrypt at startup.

**中文：** 给 `dse build` 加 `--key=KEY`（>= 16 字节，AES-128-CTR）。生成的 `launch.bat` 会带上密钥参数，运行时启动时即可解密。

```powershell
dse build MyGame --key=0123456789abcdef
```

---

## Distribution / 分发

### Q: How do I produce a single distributable file? / 怎么打成一个可分发的文件？

**EN:** Use the Export Template commands on an already-built directory:

**中文：** 对已构建目录用 Export Template 命令：

```powershell
dse dist --target win   --in MyGame\build\dist    # -> .zip  (Windows)
dse dist --target linux --in MyGame/build/dist    # -> .tar.gz (Linux)
```

**EN:** Add `--installer` (Windows, needs Inno Setup `iscc` on PATH) or `--appimage` (Linux, needs `appimagetool`) to also emit an installer. For the browser, use `dse build --target web` then `dse dist --target web`.

**中文：** 加 `--installer`（Windows，需 PATH 上有 Inno Setup `iscc`）或 `--appimage`（Linux，需 `appimagetool`）可附带生成安装器。Web 用 `dse build --target web` 再 `dse dist --target web`。

---

## Templates & scripting / 模板与脚本

### Q: What templates can `dse new` create? / `dse new` 有哪些模板？

**EN:** `empty`, `2d`, `3d`, `lua`, `cpp`, plus three genre templates that scaffold runnable gameplay: `platformer` (2D gravity + jump + AABB platforms), `topdown` (top-down RPG with 8-way movement, obstacles, collectible coins), `thirdperson` (3D third-person with a follow camera). Either `dse new <template> <dir>` or `dse new <dir> --template=<template>` works.

**中文：** `empty`、`2d`、`3d`、`lua`、`cpp`，外加三个带可运行玩法的品类模板：`platformer`（2D 重力+跳跃+平台 AABB 碰撞）、`topdown`（俯视 RPG，8 向移动/障碍/可拾取金币）、`thirdperson`（3D 第三人称跟随相机）。`dse new <模板> <目录>` 与 `dse new <目录> --template=<模板>` 两种写法都行。

### Q: I edited `main.lua` but nothing changed. / 我改了 `main.lua` 但没变化。

**EN:** Script + assets are baked into `game.bun` at build time. Re-run `dse build` after editing, then launch the exe again.

**中文：** 脚本和资源在构建时被打进了 `game.bun`。改完要重新 `dse build`，再启动 exe。

### Q: Which lifecycle functions does the runtime call? / 运行时会调用哪些生命周期函数？

**EN:** Global `Awake()` once at startup, and `Update(dt)` every frame (`dt` = seconds since last frame). Names must match exactly. See the [Lua API reference](../api/LUA_API.md) for all `dse.*` interfaces.

**中文：** 全局 `Awake()` 启动时调用一次，`Update(dt)` 每帧调用一次（`dt` = 距上一帧秒数）。函数名必须一字不差。全部 `dse.*` 接口见 [Lua API 参考](../api/LUA_API.md)。

---

## Still stuck? / 还卡住？

Open an issue on the repo with: OS version, the exact command you ran, and the full console output (it includes `[INFO]`/`[ERROR]` lines that pinpoint the failure).

带上以下信息到仓库提 issue：系统版本、你跑的确切命令、完整控制台输出（里面的 `[INFO]`/`[ERROR]` 行能定位失败点）。
