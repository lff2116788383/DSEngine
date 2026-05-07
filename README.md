# DSEngine

A lightweight C++20 game engine with an integrated editor, Lua scripting, and a full 2D/3D rendering pipeline.

## Features

### Rendering
- **OpenGL 4.5** core profile (Vulkan RHI backend in progress)
- **PBR pipeline** — metallic-roughness workflow, image-based lighting
- **Cascaded shadow maps** (directional + spot + point lights)
- **Bloom** post-processing with multi-pass downscale/upscale
- **Skybox / SkyLight** environment mapping
- **Sprite batching** for 2D, **3D mesh rendering** with instancing
- **Particle system** — GPU-friendly emitters with curve editor
- **Terrain** — heightmap sculpt + splat painting

### Editor
- **Inspector** — 20+ component types with undo/redo
- **Hierarchy** — search, drag-reorder, parent/child
- **Viewport** — ImGuizmo translate/rotate/scale, box-select, Color-ID picking
- **Tilemap** / **Terrain** editors
- **Animation Timeline** / **Material PBR** panel
- **Prefab** save/load, multi-scene tabs
- **Console** with source-code jump, **Lua REPL**
- **File → Build Game...** — pack assets into `.dpak` and export standalone exe

### Scripting
- **Lua 5.4** embedded with 22+ engine bindings
- Hot-reload in Edit mode (`PumpLuaScriptHotReloads`)
- Interactive Lua console in editor

### Runtime
- **ECS** built on EnTT
- **Physics** — Box2D (2D), PhysX optional (3D)
- **Audio** — positional audio with range visualization
- **Job system** — multi-threaded task graph
- **Asset pipeline** — `.dmesh` / `.dmat` / `.danim` / `.dskel` / `.dpak`

---

## Project Structure

```
DSEngine/
├── engine/            Core engine (shared library: DSEngine.dll)
│   ├── assets/        Asset manager, pak reader/writer, scanner
│   ├── audio/         Audio playback
│   ├── core/          Job system, event bus, service locator, modules
│   ├── ecs/           ECS components & systems
│   ├── input/         Keyboard / mouse / gamepad
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
├── samples/lua/       Lua demo collection (15+ demos)
├── data/              Shaders, textures, models, fonts
├── tests/             720+ unit tests
└── docs/              Architecture & roadmap docs
```

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

```powershell
# Clone with submodules
git clone --recursive <repo-url>
cd DSEngine

# Generate
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64

# Build targets
cmake --build build_vs2022 --config Release --target dse_engine        # Engine DLL
cmake --build build_vs2022 --config Release --target dse_editor_cpp    # Editor
cmake --build build_vs2022 --config Release --target dse_standalone    # Standalone runtime
cmake --build build_vs2022 --config Release --target dse_example_lua   # Lua demo host
```

Or use the convenience scripts:

```powershell
build_fast_editor.bat   # Editor only
build_fast_lua.bat      # Lua host only
build_all.bat           # Everything
```

Output binaries go to `bin/`.

---

## Quick Start

### Run the Editor

```powershell
bin\dsengine-editor.exe
```

- **File → Open Scene** to load a `.json` scene
- **File → Build Game...** to export a standalone game package

### Run a Lua Demo

```powershell
build_fast_lua.bat
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

From the editor: **File → Build Game...**

Or manually:

1. Build `dse_standalone` target
2. Pack assets: use `pak_writer` or the editor's Build dialog
3. Place `.dpak` next to `DSEngine_Game.exe` — it auto-detects on launch

CLI flags: `--scene=`, `--pak=`, `--script=`, `--width=`, `--height=`, `--title=`

---

## Environment Variables

| Variable | Description |
|----------|-------------|
| `DSE_STARTUP_LUA` | Override Lua startup script path |
| `DSE_MAX_FRAMES` | Auto-exit after N frames (for CI / screenshots) |
| `DSE_SCREENSHOT_PATH` | Save PNG screenshot on exit |
| `DSE_SCREENSHOT_TARGET` | Readback source: `main` or `scene` |
| `DSE_RENDER_READBACK_DIAG` | Set `1` for render target diagnostics |

---

## Tests

```powershell
cmake --build build_vs2022 --config Debug --target dse_tests
bin\dse_tests_debug.exe
```

720+ unit tests covering ECS, physics, serialization, asset pipeline, and more.

---

## License

Proprietary. All rights reserved.

---

> For architecture details see [`docs/Architecture-Refactor-Plan.md`](docs/Architecture-Refactor-Plan.md).
> For the development roadmap see [`docs/NEXT_DIRECTION.md`](docs/NEXT_DIRECTION.md).
