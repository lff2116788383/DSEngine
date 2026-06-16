# DSEngine

<p align="center">
  <img src="data/icon/dse_icon.png" alt="DSEngine Logo" width="128">
</p>

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.24%2B-064F8C.svg)](https://cmake.org)

[简体中文](README.md) | **English**

A lightweight **C++20 game engine** with an integrated editor, Lua scripting, and a full 2D/3D rendering pipeline.

Multi-backend RHI (OpenGL 4.5 / Vulkan / D3D11) · RenderGraph · DSSL shading language · ECS (EnTT) · Jolt Physics

## Features

### Rendering
- **Multi-backend RHI** — OpenGL 4.5, Vulkan 1.3, D3D11 (auto-fallback on failure)
- **RenderGraph** — DAG-based frame graph with dependency-driven pass scheduling & dead-pass culling
- **PBR pipeline** — metallic-roughness workflow, image-based lighting, GPU-Driven instancing (SSBO)
- **Cascaded shadow maps** (directional + spot + point lights)
- **DSSL** (DS Shading Language) — surface shader abstraction (auto-injects lighting/shadows/GI)
- **Bloom** post-processing with multi-pass downscale/upscale
- **Skybox / SkyLight** environment mapping
- **Sprite batching** for 2D, **3D mesh rendering** with instancing
- **Particle system** — GPU-friendly emitters with curve editor
- **Terrain** — heightmap sculpt + splat painting
- **Clustered Forward+** light culling

### Editor
- **Inspector** — 20+ component types with undo/redo
- **Hierarchy** — search, drag-reorder, parent/child
- **Viewport** — ImGuizmo translate/rotate/scale, box-select, Color-ID picking
- **Tilemap** / **Terrain** editors
- **Animation Timeline** / **Material PBR** panel
- **Prefab** save/load, multi-scene tabs
- **Console** with source-code jump, **Lua REPL**
- **File → Build Game...** — pack assets into `.dpak` and export a standalone executable

### Scripting
- **Lua 5.4** embedded with 22+ engine bindings
- Hot-reload in Edit mode (`PumpLuaScriptHotReloads`)
- Interactive Lua console in the editor

### Runtime
- **ECS** built on EnTT
- **Physics** — Box2D (2D), Jolt Physics (3D)
- **Audio** — positional audio with range visualization
- **Job system** — multi-threaded task graph
- **Asset pipeline** — `.dmesh` / `.dmat` / `.danim` / `.dskel` / `.dpak`
- **Memory management subsystem** — unified allocation facade, per-tag tracking + leak reporting, linear/frame/scratch/pool allocators, budgets, STL adapters, `Handle/HandleTable`, optional mimalloc backend (see [`docs/architecture/MEMORY_MANAGEMENT_DESIGN.md`](docs/architecture/MEMORY_MANAGEMENT_DESIGN.md))
- **Networking layer (experimental, off by default)** — a reliable/unreliable + multi-lane encrypted transport base on GameNetworkingSockets, a flat C ABI, an async HTTP client, and a minimal viable server-authoritative replication layer MVP (entity spawn/despawn, full-state Transform snapshots, input RPC with ownership checks). See [`docs/architecture/NETWORK_LAYER_DESIGN.md`](docs/architecture/NETWORK_LAYER_DESIGN.md)

---

## Project Structure

```
DSEngine/
├── engine/            Core engine (static library by default; DSEngine.dll when DSE_BUILD_SHARED=ON)
│   ├── assets/        Asset manager, pak reader/writer, scanner
│   ├── audio/         Audio playback
│   ├── core/          Job system, event bus, service locator, modules, memory mgmt (core/memory)
│   ├── ecs/           ECS components & systems
│   ├── http/          Async HTTP(S) client (experimental, DSE_ENABLE_HTTP)
│   ├── input/         Keyboard / mouse / gamepad
│   ├── net/           Net transport abstraction + GNS backend + C ABI + replication (experimental, DSE_ENABLE_NET)
│   ├── physics/       Box2D wrapper, PhysX optional
│   ├── render/        RHI abstraction, passes, materials, shaders
│   ├── runtime/       Engine app shell, frame pipeline
│   ├── scene/         Scene serialization (JSON)
│   └── scripting/     Lua VM, bindings, hot-reload
├── apps/
│   ├── editor_cpp/    ImGui-based editor (dsengine-editor.exe)
│   ├── standalone/    Standalone game runtime (DSEngine_Game.exe)
│   ├── runtime/       Lua host & C++ host examples
│   └── tools/         AssetBuilder CLI
├── modules/           Optional engine modules (terrain, animation, etc.)
├── plugins/           Optional runtime plugins
├── samples/           Runtime-loaded demos (cpp / lua / plugins)
├── examples/          Standalone example projects (KF_Framework, sdk_consumer, stress_test)
├── script/            Runtime Lua libraries (loaded via Lua package.path; shipped with the engine)
├── scripts/           Build / CI / packaging scripts (scripts/win/ holds the Windows .bat)
├── tools/             Codegen, shader compiler, asset cooking, etc.
├── data/              Shaders, textures, models, fonts
├── tests/             GoogleTest cases (unit/integration/smoke)
└── docs/              Architecture & roadmap docs
```

> **`script/` vs `scripts/`** — `script/` (singular) holds **runtime** Lua libraries that the
> engine loads at run time (hard-wired into the Lua `package.path`) and are installed with the
> engine; `scripts/` (plural) holds **build-time** developer/CI scripts.
>
> **`samples/` vs `examples/`** — `samples/` are small demos loaded by the engine runtime
> (`samples/lua`, `samples/cpp`, `samples/plugins`); `examples/` are self-contained example
> projects that consume the engine/SDK (`KF_Framework`, `sdk_consumer`, `stress_test`).

---

## Executable Targets

All executables are emitted to **`bin/`** at the repo root. The `dse_engine`, `dse_standalone`,
`dse_example_lua` and `DSEngine_example_cpp` targets append a per-config suffix
(`_debug` / `_release` / `_relwithdebinfo` / `_minsizerel`); the editor, AssetBuilder and the
various tool/test targets have **no config suffix**.

> Note: `dse_engine` itself is a **library** (static by default; compiled as `DSEngine.dll` when
> `DSE_BUILD_SHARED=ON`), not an executable. It is listed below to show dependency relationships.

### Targets built by a default desktop build

| Target | Output (`bin/`) | Purpose | Key dependencies |
|--------|-----------------|---------|------------------|
| `dse_engine` | `DSEngine[_config].lib` (or `.dll`) | Core engine library, linked by every app/tool below | EnTT, Box2D, Jolt, assimp, Lua, freetype, … |
| `dse_standalone` | `DSEngine_Game[_config].exe` | **Standalone game runtime** (no editor UI); this is what `File → Build Game...` exports | `dse_engine` |
| `DSEngine_example_cpp` | `DSEngine_c++[_config].exe` | **C++ host sample**: drive the engine directly from C++ | `dse_engine` |
| `dse_example_lua` | `DSEngine_lua[_config].exe` | **Lua scripting host sample** (requires `DSE_ENABLE_LUA`, ON by default) | `dse_engine` + Lua |
| `dse_cli` | `dse.exe` | **Headless project CLI**: scaffold templates / pack & encrypt asset bundles / one-shot build (`new` / `pack` / `build`) | `dse_engine` |
| `AssetBuilder` | `AssetBuilder.exe` | **Asset import/cook CLI**: glTF/FBX/textures → `.dmesh` / `.dmat`, etc. | tinygltf, assimp (optional), glm |
| `dse_dssl_compiler` | `dse_dssl_compiler.exe` | **DSSL shading-language compiler** | — |
| `dse_shader_compiler` | `dse_shader_compiler.exe` | **Shader compiler** (built only when no prebuilt one is found; or point at one via `-DDSE_HOST_SHADER_COMPILER=<path>`) | — |
| `dse_gtest_unit_tests` | `bin/` | GoogleTest **unit tests** (`DSE_BUILD_GTESTS`, ON by default) | googletest, `dse_engine` |
| `dse_gtest_integration_tests` | `bin/` | GoogleTest **integration tests** | googletest, `dse_engine` |
| `dse_gtest_smoke_tests` | `bin/` | GoogleTest **smoke tests** | googletest, `dse_engine` |
| `dse_serialize_smoke` | `bin/` | Serialization smoke test (requires `DSE_ENABLE_LUA`) | `dse_engine` |

### Opt-in targets (require enabling a switch)

| Target | Output (`bin/`) | Purpose | Switch (default) |
|--------|-----------------|---------|------------------|
| `dse_editor_cpp` | `dsengine-editor.exe` | **Visual editor** (Win32 GUI, ImGui) | `DSE_BUILD_EDITOR=ON` (default OFF) |
| launcher | — | Launcher (built when `apps/launcher` is present) | `DSE_BUILD_LAUNCHER=ON` (default OFF) |
| `dse_http_smoke` / `dse_http_lua_smoke` | `bin/` | HTTP client / Lua-binding smoke tests | `DSE_ENABLE_HTTP=ON` (default OFF) |
| `dse_net_smoke` | `bin/` | Networking **transport** loopback smoke (reliable/unreliable + lanes) | `DSE_ENABLE_NET=ON` (default OFF) |
| `dse_net_capi_smoke` | `bin/` | Networking **C ABI** smoke | `DSE_ENABLE_NET=ON` |
| `dse_net_lua_smoke` | `bin/` | Networking **Lua binding** smoke | `DSE_ENABLE_NET=ON` |
| `dse_repl_smoke` | `bin/` | **Replication loopback smoke**: spawn → full-snapshot match → ownership negative case → owned input RPC server-authoritative move → despawn | `DSE_ENABLE_NET=ON` |

> Others: `examples/sdk_consumer`'s `consumer_example` is a minimal SDK-consumer sample, built with the
> SDK/examples projects rather than the engine's default build; the Android platform additionally produces
> `dse_android_host` (a `.so` shared library, not a standalone executable).

In short: **a default desktop build ≈ 3 application hosts (standalone / cpp / lua) + 4 tools (dse / AssetBuilder / dssl / shader) + 4 test programs**;
the editor, HTTP and networking (including `dse_repl_smoke`) all require an explicit switch. To run the replication smoke, remember `-DDSE_ENABLE_NET=ON`.

---

## Prerequisites

| Tool | Version |
|------|---------|
| **CMake** | 3.24+ |
| **Visual Studio** | 2022 (v143 toolset) |
| **Windows SDK** | 10.0.22000+ |
| **Git** | with submodule support |

All third-party dependencies are vendored under `depends/` — no package manager needed.

---

## Build

> **Dependencies are in-tree git submodules** — after cloning you must initialize them, otherwise
> CMake configuration aborts with an explicit error.
>
> ```powershell
> git clone <repo-url>
> cd DSEngine
> git submodule update --init --recursive   # important: fetch everything under depends/
> ```

### Recommended: CMakePresets (VS 2022 "Open Folder" and build)

The repo ships `CMakePresets.json` (Ninja generator, organized by build type). Visual Studio 2022
auto-detects presets when you **Open this folder**: pick **Local Machine** as the target system and the
configuration dropdown shows **`x64 Debug` / `x64 RelWithDebInfo` / `x64 Release`**; pick **WSL Ubuntu**
to switch to **`WSL Debug` / `WSL RelWithDebInfo` / `WSL Release`**. Then **Build → Build All**.

Command-line equivalent (Windows; run from a VS developer environment / a terminal with `vcvars64`):

```powershell
cmake --preset windows-x64-debug          # configure Debug (GL+Vulkan+D3D11 + editor + GTest)
cmake --build --preset windows-x64-debug  # build (also windows-x64-relwithdebinfo / -release)
ctest  --preset windows-x64-debug         # run gtest cases
```

WSL/Linux (run inside WSL; matches CI `build-linux`: GL + Jolt, static libs, D3D11/Vulkan/GTest off):

```bash
cmake --preset wsl-debug                  # also wsl-relwithdebinfo / wsl-release
cmake --build --preset wsl-debug
```

| Preset group | Target system | Backends | Notes |
|--------------|---------------|----------|-------|
| `windows-x64-{debug,relwithdebinfo,release}` | Local Machine | GL + Vulkan + D3D11 | editor + GTest, Ninja + MSVC |
| `wsl-{debug,relwithdebinfo,release}` | WSL Ubuntu | GL (+ Jolt) | static libs, D3D11/Vulkan off, Ninja + gcc |

All presets pin `CMAKE_POLICY_VERSION_MINIMUM=3.5` (CMake 4 compatibility with older deps); no manual flags needed.

#### Building the whole project

After opening the folder, use **Build ▸ Build All** or **Build → DSEngine** to compile everything;
in the **Startup Item** dropdown pick `dsengine-editor.exe` (editor) or `DSEngine_Game_*.exe` (runtime) to F5-debug.

> If VS shows a default target like `glview` — a **third-party sample** under `depends/` (e.g. tinygltf's `glview`) —
> that's just a stale `.vs/` cache. Such samples are **not part of the DSEngine build** (tinygltf is header-only and
> never `add_subdirectory`-ed). Close VS, delete `.vs/` and `out/` at the repo root, re-"Open Folder", then **Build ▸ Build All**.

### Manual configuration (command line)

```powershell
# Clone with submodules
git clone --recursive <repo-url>
cd DSEngine

# Generate
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5

# Build targets
cmake --build build_vs2022 --config Release --target dse_engine        # Engine library
cmake --build build_vs2022 --config Release --target dse_editor_cpp    # Editor (needs -DDSE_BUILD_EDITOR=ON)
cmake --build build_vs2022 --config Release --target dse_standalone    # Standalone runtime
cmake --build build_vs2022 --config Release --target dse_cli           # Headless project CLI (outputs dse.exe)
cmake --build build_vs2022 --config Release --target dse_example_lua   # Lua demo host
```

Or use the convenience scripts:

```powershell
scripts\win\build_fast_editor.bat   # Editor only
scripts\win\build_fast_lua.bat      # Lua host only
scripts\win\build_all.bat           # Everything
```

Output binaries go to `bin/`.

### Common CMake switches

| Switch | Default | Description |
|--------|---------|-------------|
| `DSE_BUILD_SHARED` | OFF | Build `dse_engine` as a DLL (requires `DSE_EXPORT` on public APIs) |
| `DSE_BUILD_EDITOR` | OFF | Build the ImGui editor `dse_editor_cpp` |
| `DSE_BUILD_LAUNCHER` | OFF | Build the launcher target |
| `DSE_BUILD_GTESTS` | ON | Build GoogleTest targets |
| `DSE_ENABLE_LUA` | ON | Enable the Lua scripting runtime |
| `DSE_ENABLE_NAVMESH` | ON | Enable NavMesh / pathfinding (Recast/Detour) |
| `DSE_ENABLE_NET` | OFF | Enable the networking layer (GameNetworkingSockets backend + replication) |
| `DSE_ENABLE_HTTP` | OFF | Enable the async HTTP(S) client (IXWebSocket + OpenSSL) |
| `DSE_MEM_BACKEND` | system | Memory backend: `system` (zero-dep) or `mimalloc` (needs the `depends/mimalloc` submodule) |

---

## Getting Started — Indie Developer Guide

> The full flow from scratch to exporting a game.

### Step 1: Build

```powershell
# Generate the project (first time only)
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=ON

# Build the editor (also builds AssetBuilder + the standalone runtime)
cmake --build build_vs2022 --config Release --target dse_editor_cpp
```

> After a successful build, `bin/` should contain (Release build shown):
> - `dsengine-editor.exe` — the editor (no config suffix)
> - `DSEngine_Game_release.exe` — the standalone game runtime (Debug build is `DSEngine_Game_debug.exe`)
> - `AssetBuilder.exe` — the asset conversion tool (no config suffix)

### Step 2: Create a project

1. Double-click `bin\dsengine-editor.exe`
2. In the **Project Hub** click **New Project** and pick an empty directory
3. The editor creates a `data/` asset directory and opens it

### Step 3: Import assets

| Asset type | Action |
|-----------|--------|
| 3D model (.glb / .gltf / .fbx) | **Assets → Import Asset...** → pick a file → invokes AssetBuilder to generate `.dmesh` / `.dmat` |
| Texture (.png / .jpg / .hdr) | Drag the file onto the Project panel, or **Assets → Import Asset...** |
| Audio (.wav / .ogg / .mp3) | Same as above |

You can also drag files directly onto the editor window to trigger the import dialog.

### Step 4: Build a scene

1. Right-click in the **Hierarchy** panel → **Create Entity**
2. Select the entity → **Inspector** → **Add Component**:
   - `MeshRendererComponent` — render a 3D mesh
   - `RigidBody3DComponent` + `BoxCollider3DComponent` — physics simulation
   - `LuaScriptComponent` — attach a Lua script
3. Drag a `.dmesh` from the Project panel into the Viewport to auto-create a mesh entity
4. In the Viewport use **W/E/R** to switch translate/rotate/scale gizmos
5. **Ctrl+S** to save the scene as `.dscene`

### Step 5: Write a Lua script

Create a `.lua` file under `data/scripts/`:

```lua
local MyScript = {}

function MyScript:on_start()
    -- init
end

function MyScript:on_update(dt)
    -- per-frame logic
end

return MyScript
```

Drag the script into the Viewport or onto the entity's `LuaScriptComponent` `script_path` field.

### Step 6: Run in the editor

- Click ▶ **Play** in the toolbar — start Lua scripts and physics
- ⏸ **Pause** — pause (single-step supported)
- ⏹ **Stop** — stop and restore the initial scene state

### Step 7: Build a standalone game

1. **File → Build Game...**
2. Fill in the output directory and game name
3. Click **Build** — the editor will:
   - Copy and rename `DSEngine_Game.exe`
   - Pack all assets into `game.dpak`
   - Copy the required DLLs
4. After building, click **Open Folder** or **Run** to test

---

## Quick Start

> New here? The fastest path is the CLI: scaffold, build, and play a game in minutes — see the [English Quick Start](docs/getting-started/QUICKSTART.en.md) and the [FAQ](docs/getting-started/FAQ.md). The sections below cover the editor and Lua demos.

### Run the Editor

```powershell
bin\dsengine-editor.exe
```

- **File → Open Scene** to load a `.json` scene
- **File → Build Game...** to export a standalone game package

### Run a Lua Demo

```powershell
scripts\win\build_fast_lua.bat
bin\DSEngine_lua_debug.exe
```

Default demo: `samples/lua/phase1_2d_physics_showcase.lua` (2D physics).

Switch demo by editing `samples/lua/config.lua`:

```lua
Config.game_entry = "3d_scene_showcase"
```

Available entries: `3d_triangle`, `3d_cube`, `3d_textured_cube`, `3d_lighting_showcase`, `3d_material_showcase`, `3d_camera_showcase`, `3d_scene_showcase`, `3d_skybox_environment`, `3d_postprocess_showcase`, `3d_particles_showcase`, `3d_physics_stack`, and more.

### Batch Verify All Demos

```powershell
python tools\verify_lua_3d_demos.py --entries all
```

Screenshots and logs go to `tmp/lua_3d_verify/`.

---

## Building a Standalone Game

Three ways, all sharing the same pack / encrypt / mount implementation so they behave identically end-to-end.

### Option A: `dse` headless CLI (recommended — pure command line, Cocos-style)

```bash
# Show help
dse help            # or dse -h / --help

# 1) Scaffold a project (empty | 2d | 3d | lua | cpp)
dse new lua MyGame

# 2) One-shot build: locate the DSEngine_Game runtime, copy exe+DLLs, pack & encrypt, emit launch.bat
#    Omit --key for a plaintext bundle; pass a >=16-byte key for AES-128-CTR encryption
dse build MyGame --out dist --key 0123456789abcdef

# 3) Run (launch.bat already wires --bundle/--key/--script)
dist/launch.bat

# You can also just pack a directory into a (optionally encrypted) bundle
dse pack MyGame dist/game.bun --key 0123456789abcdef
```

> `dse` and `DSEngine_Game` must live in the same directory (or its `bin/` subfolder) so `build` can
> locate the runtime. A default build emits both into the repo's `bin/`.
>
> Templates: `empty/2d/3d/lua` are **Lua/data** projects that `dse build` packs and runs directly;
> `cpp` scaffolds a **C++ host project** (`src/main.cpp` + `CMakeLists.txt`, linking `dse_engine`),
> which follows the "edit source + compile" route — build it separately with
> `cmake -B build -DCMAKE_PREFIX_PATH=<dsengine_install_dir> && cmake --build build`.

### Option B: Editor

**File → Build Game...** → tick **Encrypt (AES-128 → game.bun)** and enter a `>=16`-byte key for end-to-end
encryption; leave it unticked to export a plaintext `game.dpak`. Encrypted builds auto-generate `launch.bat`,
and `Run` / `Build & Run` launch with `--key` as well.

### Option C: Manual

1. Build the `dse_standalone` target
2. Pack assets: `dse pack` / `pak_writer` / the editor's Build dialog
3. Place `game.bun` (or `.dpak`) next to `DSEngine_Game.exe` — it auto-detects on launch; an encrypted bundle needs `--key`

### Runtime CLI flags

`--scene=`, `--pak=`, `--bundle=`, `--key=`, `--script=`, `--width=`, `--height=`, `--title=`

### How end-to-end encryption works

Pack side `PackDirectoryToBundle` (`engine/assets/bundle_packer.{h,cpp}`, shared by CLI / editor) → zip-compress
then AES-128-CTR encrypt into `game.bun`; at runtime `EngineInstance::Init` `MountBundle`s it with the same key,
filling the VFS → `AssetManager::LoadFileToMemory` (VFS → `.dpak` → disk) and the Lua VFS `require` searcher read
straight from the bundle, so **no plaintext is left on disk** and encrypted Lua can still be `require`d.

---

## Environment Variables

| Variable | Description |
|----------|-------------|
| `DSE_STARTUP_LUA` | Override the Lua startup script path |
| `DSE_MAX_FRAMES` | Auto-exit after N frames (for CI / screenshots) |
| `DSE_SCREENSHOT_PATH` | Save a PNG screenshot on exit |
| `DSE_SCREENSHOT_TARGET` | Readback source: `main` or `scene` |
| `DSE_RENDER_READBACK_DIAG` | Set `1` for render-target diagnostics |

---

## Tests

```powershell
# One-shot: configure (gtests on) + build the three suites + run via ctest
scripts\win\build_fast_tests.bat

# Or manually:
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_GTESTS=ON
cmake --build build_vs2022 --config Debug --target dse_gtest_unit_tests dse_gtest_integration_tests dse_gtest_smoke_tests --parallel
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L gtest
```

The default `build_fast_tests.bat` config (3D off) runs ~2,269 GoogleTest cases (1787 unit / 440 integration / 42 smoke)
covering ECS, physics, serialization, asset pipeline, rendering, and more. A full build with `-DDSE_ENABLE_3D=ON`
compiles additional 3D-gated suites (~2,600 total).

> Networking/HTTP smoke tests (`dse_net_smoke`, `dse_net_capi_smoke`, `dse_net_lua_smoke`, `dse_repl_smoke`, `dse_http_smoke`, …)
> are not built by default; enable `-DDSE_ENABLE_NET=ON` / `-DDSE_ENABLE_HTTP=ON` respectively. They are registered with
> ctest, so you can run e.g. `ctest -R dse_repl_smoke -V`.

---

## Contributing

Contributions are welcome! Please use the [issue templates](.github/ISSUE_TEMPLATE/) when reporting bugs or requesting features.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Commit your changes
4. Open a Pull Request

Please make sure your changes compile on all three RHI backends (OpenGL / Vulkan / D3D11) before submitting.

---

## License

This project is licensed under the **Apache License 2.0** — see the [LICENSE](LICENSE) file for details.

---

> For architecture details see [`docs/architecture/ARCHITECTURE.md`](docs/architecture/ARCHITECTURE.md).
> For the shader system see [`docs/architecture/SHADER_SYSTEM.md`](docs/architecture/SHADER_SYSTEM.md).
> For memory management see [`docs/architecture/MEMORY_MANAGEMENT_DESIGN.md`](docs/architecture/MEMORY_MANAGEMENT_DESIGN.md).
> For the networking layer see [`docs/architecture/NETWORK_LAYER_DESIGN.md`](docs/architecture/NETWORK_LAYER_DESIGN.md).
> For the development roadmap see [`docs/roadmap/PROGRESS_REPORT.md`](docs/roadmap/PROGRESS_REPORT.md).
