# Quick Start: Build & Export Your First DSEngine Game in 30 Minutes

> For people who have **never touched DSEngine**. Follow it end to end and you will have a keyboard-controlled 2D mini-game that runs on double-click and can be packaged for distribution — using only the `dse` command line. No editor, no C++, no GPU required.
>
> Platform: Windows 10/11 x64. Estimated time: 30 minutes (a few minutes if you download the prebuilt toolkit and skip the first compile).
>
> 中文版见 [QUICKSTART.md](QUICKSTART.md)。

---

## What you'll get

- A cyan square you move up/down/left/right with **WASD / arrow keys**
- A standalone, double-click `.exe` + resource bundle (`build/dist/`)
- All game logic in a single **Lua** script (`scripts/main.lua`) — edit, re-run `dse build`, done

The whole "golden path" is three steps:

```
dse new 2d MyGame      →   dse build MyGame      →   run build/dist/MyGame.exe
(scaffold project)         (copy runtime + pack)      (double-click to play)
```

---

## Step 0: Get the `dse` command line (first time only)

You need `dse.exe` and the runtime `DSEngine_Game.exe` (used by `dse build`). Pick one:

### Option A (recommended, fastest): download the prebuilt toolkit

Download `DSEngine-tools-vX.Y.Z-win-x64.zip` from this repo's **Releases** page and unzip it — it contains `dse.exe` + `DSEngine_Game.exe`, ready to use, **no compiler and no source build needed**.

- Direct link: the `DSEngine-tools-*-win-x64.zip` asset on the repo **Releases** page (`<repo-url>/releases/latest`)

```powershell
# After unzipping anywhere, add the folder to PATH or use the full path
.\dse.exe help
```

### Option B: build the engine from source once

Prerequisites: Git, CMake 3.24+, Visual Studio 2022 (C++ desktop workload). The bundled script installs the toolchain and builds for you:

```powershell
git clone --recursive <repo-url>
cd DSEngine
powershell -ExecutionPolicy Bypass -File scripts\bootstrap_windows.ps1
```

When it finishes, confirm these two files exist:

```powershell
dir bin\dse.exe
dir bin\DSEngine_Game_*.exe
```

> Maintainers publishing the toolkit: `powershell -ExecutionPolicy Bypass -File scripts\package_dse_tools.ps1`
> compiles `dse_cli`/`dse_standalone` and produces the `DSEngine-tools-*.zip` above — upload it as a Release asset.

> To keep commands short, add the tools folder to PATH, or use the full path `dse.exe`. Below we just write `dse`.

---

## Step 1: Create a project (10 seconds)

```powershell
dse new 2d MyGame
```

This generates a `MyGame\` directory; the key files:

```
MyGame/
├── scripts/
│   └── main.lua        ← all of your game logic
├── assets/             ← textures/audio/etc. (empty for now)
└── project.dsproj      ← project descriptor
```

`dse new` also supports these templates:

| Template | Description |
|----------|-------------|
| `empty`        | Directory skeleton only, no script |
| `2d`           | **Used by this tutorial** — 2D gameplay + Lua entry script (camera + movable sprite) |
| `3d`           | 3D demo scene (camera + directional light + Lua entry script) |
| `lua`          | Pure Lua entry script |
| `cpp`          | C++ host project (`src/main.cpp` + `CMakeLists.txt`, build it yourself with CMake) |
| `platformer`   | Genre template: 2D platformer (gravity / jump / platform AABB collision / camera follow) |
| `topdown`      | Genre template: top-down RPG (8-way movement / obstacle collision / collectible coins / camera follow) |
| `thirdperson`  | Genre template: 3D third-person (ground + character cube / fixed-offset follow camera) |

You can also pass the template as a named option: `dse new MyGame --template=2d`.

> The genre templates generate a `main.lua` that is already a runnable mini-game with complete mechanics — just `dse build` and play, then modify on top of it.

Open `MyGame\scripts\main.lua` — it's already a **complete, runnable game**:

```lua
local app = dse.app
local KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP = 263, 262, 264, 265
local KEY_A, KEY_D, KEY_S, KEY_W = 65, 68, 83, 87

local player
local pos = { x = 0.0, y = 0.0 }
local speed = 5.0

-- Awake() is called once at startup
function Awake()
    local camera = dse.ecs.create_entity()
    dse.ecs.add_transform(camera, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(camera, 5.0)

    player = dse.ecs.create_entity()
    dse.ecs.add_transform(player, pos.x, pos.y, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_sprite(player, 0.30, 0.80, 1.0, 1.0, 0)

    print("[MyGame] ready -- move the square with WASD or arrow keys")
end

-- Update(dt) is called once per frame; dt = seconds since the last frame
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

**Two lifecycle functions you must remember** (the runtime calls them by exact global name — spelling must match):

- `Awake()` — called once at startup; create entities, cameras, do initialization
- `Update(dt)` — called once per frame; `dt` is seconds since the last frame; do movement / logic

> The input API lives under `dse.app`, so aliasing `local app = dse.app` at the top of the script makes `app.get_key(...)` shorter.

---

## Step 2: Build and package (30 seconds)

```powershell
dse build MyGame
```

Output lands in `MyGame\build\dist\`:

```
dist/
├── MyGame.exe          ← runtime executable
├── game.bun            ← packed scripts + assets
├── game.dsmanifest     ← window title / size / splash / entry-script config
└── launch.bat          ← launch script (equivalent to double-clicking the exe; needed to pass --key for encrypted assets)
```

> Note: `dse build` does **not** recompile the engine — it copies the prebuilt runtime from `bin\`, packs your assets, and generates the launch script. So a build takes only seconds.

---

## Step 3: Run (play!)

Just double-click `MyGame\build\dist\MyGame.exe` (or run it from the command line):

```powershell
MyGame\build\dist\MyGame.exe
```

When the window appears, move the cyan square with **WASD** or the **arrow keys**.

> The exe automatically reads the sibling `game.dsmanifest` (which records the entry script `scripts/main.lua`) and the `game.bun` bundle, so double-click works.
>
> When do you still need `launch.bat`? When you **encrypted** the bundle with `--key` — the decryption key is passed via the `--key=...` argument inside `launch.bat`, so launch through it in that case.

### No discrete GPU / running in remote desktop or a VM?

If startup reports `Failed to create GLFW window`, there's no usable hardware OpenGL. Deploy software rendering (Mesa3D llvmpipe) into the dist directory with the bundled script:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup_swgl.ps1 -BinDir "MyGame\build\dist"
```

Then set the environment variable and launch:

```powershell
$env:GALLIUM_DRIVER = "llvmpipe"
MyGame\build\dist\MyGame.exe
```

> Tip: `dse build MyGame --with-swgl` makes the build set `GALLIUM_DRIVER` in the generated launch scripts for you (assuming the Mesa DLLs were deployed alongside the runtime).

---

## Step 4: Change one line, see it instantly

Open `MyGame\scripts\main.lua` and double the movement speed:

```lua
local speed = 10.0     -- was 5.0
```

Or recolor the square (RGBA, 0–1):

```lua
dse.ecs.add_sprite(player, 1.0, 0.4, 0.2, 1.0, 0)   -- orange
```

Save, then rebuild and run:

```powershell
dse build MyGame
MyGame\build\dist\MyGame.exe
```

Want fuller gameplay (coins, scoring, a win condition)? Continue with the [first 2D tutorial](TUTORIAL_2D_FIRST_GAME.md).

---

## Step 5 (optional): Export to Web

DSEngine can compile your game to WebAssembly with Emscripten. This step needs [EMSDK](https://emscripten.org/docs/getting_started/downloads.html) installed first.

```powershell
# Compile the Web build (default web-release preset)
dse build --target web

# Collect the artifacts (index.html/.js/.wasm/.data) into an uploadable Web bundle
dse dist --target web --out dist\web
```

Zip the whole `dist\web` directory and upload it to platforms like itch.io. Add `--3d` for 3D projects, `--debug` for debug builds.

---

## Desktop Export Template

Turn an already-built game directory into a single distributable archive:

```powershell
dse dist --target win   --in MyGame\build\dist     # -> .zip (Windows Export Template)
dse dist --target linux --in MyGame/build/dist     # -> .tar.gz (Linux Export Template)
```

Add `--installer` (Windows, requires Inno Setup `iscc`) or `--appimage` (Linux, requires `appimagetool`) to additionally emit an installer.

---

## Command cheat sheet

```powershell
dse new <template> <dir>          # create project (templates: empty/2d/3d/lua/cpp/platformer/topdown/thirdperson)
dse new <dir> --template=<template>  # same, template given as a named option
dse build <project>               # build + package into <project>\build\dist
dse build <project> --out=DIR     # custom output directory
dse build <project> --key=KEY     # encrypt the bundle with AES-128-CTR (KEY >= 16 bytes)
dse pack <dir> <out.bun>          # pack a single directory into a bundle
dse build <project> --with-swgl   # ship software OpenGL (runs on machines with no discrete GPU)
dse build --target web [--3d]     # compile a Web build with Emscripten
dse dist  --target web --out=DIR  # collect Web artifacts into an uploadable bundle
dse dist  --target win|linux      # archive a built directory into .zip / .tar.gz (Export Template)
dse help                          # full help
```

---

## Next

- [First 2D tutorial: coin-collector game](TUTORIAL_2D_FIRST_GAME.md) — add coins, scoring, and a win condition on top of this template
- [Lua API reference](../api/LUA_API.md) — every available `dse.*` interface (verified against source)
- [Build from source / use the editor / run tests](GETTING_STARTED.md) — full build guide for contributors
- [FAQ](FAQ.md) — common packaging / running / encryption / software-rendering questions

---

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| Double-clicking the exe shows an empty window | Make sure `game.dsmanifest` (records the entry script) and `game.bun` are next to the exe; if you encrypted assets with `--key`, launch via `launch.bat` instead |
| `Failed to create GLFW window` | No hardware OpenGL — deploy the llvmpipe software renderer as in Step 3 |
| `dse.exe` not found | You don't have the tools yet — download the prebuilt toolkit (Step 0, Option A) or run `bootstrap_windows.ps1` (Option B) |
| Script edits have no effect | After editing you must re-run `dse build`, then run the exe again |

See the [full FAQ](FAQ.md) for more.
