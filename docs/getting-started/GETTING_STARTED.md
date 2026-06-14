# Getting Started with DSEngine

> **只想最快做出一个能玩、能导出的小游戏？** 看 [快速上手（QUICKSTART.md）](QUICKSTART.md) —— 只用 `dse` 命令行，30 分钟从零到一个可分发的 2D 游戏，不需要打开编辑器或写 C++。
>
> 本文是面向**贡献者 / 想从源码构建引擎**的人的完整指南（构建引擎、使用编辑器、运行 Demo、测试）。

This guide walks you through building, running, and creating your first project with DSEngine.

---

## 1. Build the Engine

### Prerequisites

- **Windows 10/11** (x64)
- **Visual Studio 2022** with C++ desktop workload
- **CMake 3.24+** on PATH
- **Git** (submodules required)

### Clone & Generate

```powershell
git clone --recursive <repo-url>
cd DSEngine
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64
```

### Build All Targets

```powershell
# Option A: batch script (builds everything)
build_all.bat

# Option B: individual targets
cmake --build build_vs2022 --config Release --target dse_engine
cmake --build build_vs2022 --config Release --target dse_editor_cpp
cmake --build build_vs2022 --config Release --target dse_standalone
cmake --build build_vs2022 --config Release --target dse_example_lua
```

Binaries are output to `bin/`.

---

## 2. Run the Editor

```powershell
bin\dsengine-editor.exe
```

### Editor Layout

| Panel | Purpose |
|-------|---------|
| **Viewport** | Scene view with gizmo manipulation |
| **Hierarchy** | Entity tree — drag to reorder, right-click to add |
| **Inspector** | Component properties for selected entity |
| **Console** | Log output + Lua REPL |
| **Asset panels** | Material, Animation, Terrain, Tilemap, Audio, Particles |

### Basic Workflow

1. **File → New Scene** — creates an empty scene
2. Right-click **Hierarchy** → **Add Entity** to create entities
3. Select an entity → **Inspector** → **Add Component** to attach components
4. Use the **Viewport** gizmo to position objects (W/E/R for translate/rotate/scale)
5. **File → Save** (Ctrl+S) to save as `.json`
6. Click **Play** to enter Play mode; **Stop** to return to Edit mode

### Lua Scripting in the Editor

- Attach a **ScriptComponent** to any entity and set `script_path` to a `.lua` file
- Open the **Lua Console** panel to run Lua interactively
- In Edit mode, Lua scripts hot-reload automatically when saved

---

## 3. Run a Lua Demo (No Editor)

```powershell
build_fast_lua.bat
bin\DSEngine_lua_debug.exe
```

The Lua host loads `samples/lua/main.lua`, which reads `samples/lua/config.lua` to pick the demo.

### Switch Demos

Edit `samples/lua/config.lua`:

```lua
Config.game_entry = "3d_scene_showcase"
```

### Available Demos

| Entry | Description |
|-------|-------------|
| `phase1_2d_physics_showcase` | 2D rigid body, triggers, raycasts |
| `3d_triangle` | Minimal 3D triangle |
| `3d_cube` | 3D cube with rotation |
| `3d_textured_cube` | Textured cube |
| `3d_static_model` | `.dmesh/.dmat` model loading |
| `3d_material_showcase` | PBR material parameters |
| `3d_lighting_showcase` | Directional / point / spot lights |
| `3d_camera_showcase` | Multi-camera switching |
| `3d_scene_showcase` | Combined 3D scene |
| `3d_skybox_environment` | SkyLight + cubemap |
| `3d_postprocess_showcase` | Bloom post-processing |
| `3d_particles_showcase` | 3D particle fountain |
| `3d_physics_stack` | 3D rigid body stacking |

---

## 4. Build a Standalone Game

### From the Editor

1. Open your scene in the editor
2. **File → Build Game...**
3. Set **Game Title** and **Output Directory**
4. Choose whether to pack all data or scene-referenced assets only
5. Click **Build**

The dialog will:
- Copy `DSEngine_Game.exe` + DLLs to the output folder
- Pack assets into `game.dpak`
- Copy `data/` as fallback

### From the Command Line

```powershell
# 1. Build the standalone target
cmake --build build_vs2022 --config Release --target dse_standalone

# 2. The exe is at bin\DSEngine_Game_release.exe
# 3. Place your .dpak next to it, or keep data/ alongside

# 4. Run
bin\DSEngine_Game_release.exe --title="My Game" --width=1280 --height=720
```

CLI arguments:

| Flag | Description |
|------|-------------|
| `--scene=<path>` | Scene file to load (default: `main.dscene`) |
| `--pak=<path>` | `.dpak` archive path (auto-detects if next to exe) |
| `--script=<path>` | Lua startup script (default: `main.lua`) |
| `--width=<N>` | Window width (default: 1280) |
| `--height=<N>` | Window height (default: 720) |
| `--title=<str>` | Window title (default: "DSEngine Game") |

---

## 5. Asset Pipeline

### Supported Formats

| Asset Type | Extension | Tool |
|------------|-----------|------|
| Mesh | `.dmesh` | `AssetBuilder` (from glTF/OBJ) |
| Material | `.dmat` | Editor Material panel or hand-authored |
| Animation | `.danim` | `AssetBuilder` (from glTF) |
| Skeleton | `.dskel` | `AssetBuilder` (from glTF) |
| Scene | `.json` | Editor save/load |
| Pak archive | `.dpak` | Editor Build Game or `pak_writer` API |

### Asset Packing (.dpak)

The `.dpak` format bundles multiple assets into a single binary file:

```
Header (32 bytes)  →  magic, version, entry count, offsets
TOC entries        →  path (64 chars) + offset + size per entry
Data blocks        →  raw file contents, sequentially packed
```

At runtime, `AssetManager::MountPak()` maps the archive; file reads fall back to mounted paks when loose files are not found.

---

## 6. Testing

```powershell
# One-shot: configure (gtests on) + build the three suites + run via ctest
build_fast_tests.bat

# Or manually:
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_GTESTS=ON
cmake --build build_vs2022 --config Debug --target dse_gtest_unit_tests dse_gtest_integration_tests dse_gtest_smoke_tests --parallel
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L gtest

# Batch-verify Lua demos (screenshots + logs)
python tools\verify_lua_3d_demos.py --entries all
```

The default config (3D off) totals 2,269 GoogleTest cases (1787 unit / 440 integration / 42 smoke); a full `-DDSE_ENABLE_3D=ON` build adds 3D-gated suites (~2,600 total). Binaries are `bin\dse_gtest_{unit,integration,smoke}_tests.exe`.

---

## 7. Project Layout Quick Reference

```
engine/assets/          AssetManager, pak_reader, pak_writer, asset_scanner
engine/ecs/             Components & systems (Transform, Sprite, Mesh, Physics, ...)
engine/render/          RHI device, shader pipeline, render passes
engine/scripting/       Lua VM, C++ bindings, hot-reload watcher
engine/runtime/         EngineInstance, FramePipeline, RunEngine()
apps/editor_cpp/src/    All editor panels (ImGui)
apps/standalone/        Standalone game exe (no editor UI)
samples/lua/            Lua demo scripts
data/shaders/           GLSL vertex/fragment shaders
data/textures/          Built-in textures (PBR defaults, skyboxes)
data/models/            Sample 3D models (.dmesh)
tests/                  Google Test suites
```

---

## Troubleshooting

- **MSVC encoding errors on engine headers** — Ensure `/utf-8` is passed. The engine sets this for `dse_engine` privately; standalone/example targets also set it.
- **Missing DLLs at runtime** — The build copies DLLs to `bin/` via post-build steps. If launching from a different directory, copy the DLLs alongside the exe.
- **Submodule errors** — Run `git submodule update --init --recursive`.
- **PhysX not found** — PhysX is optional. The engine builds without it; 3D physics demos that need PhysX will be skipped.
