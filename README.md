# DSEngine

<p align="center">
  <img src="data/icon/dse_icon.png" alt="DSEngine Logo" width="128">
</p>

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.24%2B-064F8C.svg)](https://cmake.org)

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
- **File → Build Game...** — pack assets into `.dpak` and export standalone exe

### Scripting
- **Lua 5.4** embedded with 22+ engine bindings
- Hot-reload in Edit mode (`PumpLuaScriptHotReloads`)
- Interactive Lua console in editor

### Runtime
- **ECS** built on EnTT
- **Physics** — Box2D (2D), Jolt Physics (3D)
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
├── plugins/           Optional runtime plugins
├── samples/           Runtime-loaded demos (cpp / lua / plugins)
├── examples/          Standalone example projects (KF_Framework, sdk_consumer, stress_test)
├── script/            Runtime Lua libraries (loaded via Lua package.path; shipped with the engine)
├── scripts/           Build / CI / packaging scripts (scripts/win/ holds the Windows .bat)
├── tools/             Codegen, shader compiler, asset cooking, etc.
├── data/              Shaders, textures, models, fonts
├── tests/             2269 GoogleTest cases (unit/integration/smoke)
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

> **依赖是 in-tree git submodule**：克隆后必须先初始化，否则 CMake 配置会以明确错误中止。
>
> ```powershell
> git clone <repo-url>
> cd DSEngine
> git submodule update --init --recursive   # 关键：拉取 depends/ 下全部依赖
> ```

### 推荐：CMakePresets（VS 2022 打开文件夹即编译）

仓库自带 `CMakePresets.json`（Ninja 生成器，按构建类型组织）。Visual Studio 2022
**打开此文件夹**后自动识别预设：在目标系统选 **本地计算机**，配置下拉里即出现
**`x64 Debug` / `x64 RelWithDebInfo` / `x64 Release`**；选 **WSL Ubuntu** 时则切换为
**`WSL Debug` / `WSL RelWithDebInfo` / `WSL Release`**。选定后 **生成 → 全部生成** 即可。

命令行等价流程（Windows，需在 VS 开发者环境 / 已 `vcvars64` 的终端中运行）：

```powershell
cmake --preset windows-x64-debug          # 配置 Debug（GL+Vulkan+D3D11 + 编辑器 + GTest）
cmake --build --preset windows-x64-debug  # 构建（另有 windows-x64-relwithdebinfo / -release）
ctest  --preset windows-x64-debug         # 跑 gtest 用例
```

WSL/Linux（在 WSL 内运行；与 CI `build-linux` 一致：GL + Jolt，静态库，关闭 D3D11/Vulkan/GTest）：

```bash
cmake --preset wsl-debug                  # 另有 wsl-relwithdebinfo / wsl-release
cmake --build --preset wsl-debug
```

| 预设组 | 目标系统 | 后端 | 备注 |
|--------|----------|------|------|
| `windows-x64-{debug,relwithdebinfo,release}` | 本地计算机 | GL + Vulkan + D3D11 | 编辑器 + GTest，Ninja + MSVC |
| `wsl-{debug,relwithdebinfo,release}` | WSL Ubuntu | GL (+ Jolt) | 静态库，关闭 D3D11/Vulkan，Ninja + gcc |

所有预设已固定 `CMAKE_POLICY_VERSION_MINIMUM=3.5`（CMake 4 兼容旧依赖），无需手动传参。

### 手动配置（命令行）

```powershell
# Clone with submodules
git clone --recursive <repo-url>
cd DSEngine

# Generate
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5

# Build targets
cmake --build build_vs2022 --config Release --target dse_engine        # Engine DLL
cmake --build build_vs2022 --config Release --target dse_editor_cpp    # Editor
cmake --build build_vs2022 --config Release --target dse_standalone    # Standalone runtime
cmake --build build_vs2022 --config Release --target dse_example_lua   # Lua demo host
```

Or use the convenience scripts:

```powershell
scripts\win\build_fast_editor.bat   # Editor only
scripts\win\build_fast_lua.bat      # Lua host only
scripts\win\build_all.bat           # Everything
```

Output binaries go to `bin/`.

---

## Getting Started — Indie Developer Guide

> 从零开始到导出游戏的完整流程。

### 步骤 1：构建

```powershell
# 生成工程（仅第一次）
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64

# 构建编辑器（会自动同时构建 AssetBuilder + 独立运行时）
cmake --build build_vs2022 --config Release --target dse_editor_cpp
```

> 构建成功后，`bin/` 目录里应包含（上面是 Release 构建）：
> - `dsengine-editor.exe` — 编辑器（无配置后缀）
> - `DSEngine_Game_release.exe` — 独立游戏运行时（Debug 构建为 `DSEngine_Game_debug.exe`）
> - `AssetBuilder.exe` — 资产转换工具（无配置后缀）

### 步骤 2：创建项目

1. 双击启动 `bin\dsengine-editor.exe`
2. 在 **Project Hub** 点击 **New Project**，选择一个空目录
3. 编辑器会自动创建 `data/` 资产目录并打开

### 步骤 3：导入资产

| 资产类型 | 操作 |
|---------|------|
| 3D 模型 (.glb / .gltf / .fbx) | **Assets → Import Asset...** → 选文件 → 会调用 AssetBuilder 生成 `.dmesh` / `.dmat` |
| 贴图 (.png / .jpg / .hdr) | 直接拖拽文件到 Project 面板，或 **Assets → Import Asset...** |
| 音频 (.wav / .ogg / .mp3) | 同上 |

也可以将文件直接拖拽到编辑器窗口触发导入弹窗。

### 步骤 4：搭建场景

1. **Hierarchy** 面板右键 → **Create Entity** 创建实体
2. 选中实体 → **Inspector** 面板点 **Add Component** 添加组件：
   - `MeshRendererComponent` — 渲染 3D 网格
   - `RigidBody3DComponent` + `BoxCollider3DComponent` — 物理模拟
   - `LuaScriptComponent` — 绑定 Lua 脚本
3. 将 Project 面板里的 `.dmesh` 文件拖拽到 Viewport，自动创建带 Mesh 的实体
4. 在 Viewport 里用 **W/E/R** 切换移动/旋转/缩放 Gizmo
5. **Ctrl+S** 保存场景为 `.dscene`

### 步骤 5：编写 Lua 脚本

在 `data/scripts/` 下新建 `.lua` 文件，模板：

```lua
local MyScript = {}

function MyScript:on_start()
    -- 初始化
end

function MyScript:on_update(dt)
    -- 每帧逻辑
end

return MyScript
```

将脚本文件拖入 Viewport 或拖到实体的 `LuaScriptComponent` 的 script_path 字段。

### 步骤 6：在编辑器中运行

- 点击工具栏 ▶ **Play** — 启动 Lua 脚本和物理
- ⏸ **Pause** — 暂停（可单帧步进）
- ⏹ **Stop** — 停止并恢复场景初始状态

### 步骤 7：构建独立游戏

1. **File → Build Game...**
2. 填写输出目录和游戏名称
3. 点击 **Build** — 编辑器会：
   - 将 `DSEngine_Game.exe` 复制并重命名
   - 将所有资产打包为 `game.dpak`
   - 复制必要的 DLL
4. 构建完成后点 **Open Folder** 或 **Run** 直接测试

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
# One-shot: configure (gtests on) + build the three suites + run via ctest
build_fast_tests.bat

# Or manually:
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_GTESTS=ON
cmake --build build_vs2022 --config Debug --target dse_gtest_unit_tests dse_gtest_integration_tests dse_gtest_smoke_tests --parallel
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L gtest
```

The default `build_fast_tests.bat` config (3D off) runs 2,269 GoogleTest cases (1787 unit / 440 integration / 42 smoke) covering ECS, physics, serialization, asset pipeline, rendering, and more. A full build with `-DDSE_ENABLE_3D=ON` compiles additional 3D-gated suites (~2,600 total).

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

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

> For architecture details see [`docs/architecture/ARCHITECTURE.md`](docs/architecture/ARCHITECTURE.md).
> For the shader system see [`docs/architecture/SHADER_SYSTEM.md`](docs/architecture/SHADER_SYSTEM.md).
> For the development roadmap see [`docs/roadmap/PROGRESS_REPORT.md`](docs/roadmap/PROGRESS_REPORT.md).
