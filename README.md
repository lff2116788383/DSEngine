# DSEngine

<p align="center">
  <img src="data/icon/dse_icon.png" alt="DSEngine Logo" width="128">
</p>

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.24%2B-064F8C.svg)](https://cmake.org)

**简体中文** | [English](README.en.md)

一个轻量级 **C++20 游戏引擎**，内置可视化编辑器、Lua 脚本与完整的 2D/3D 渲染管线。

多后端 RHI（OpenGL 4.5 / Vulkan / D3D11）· RenderGraph · DSSL 着色语言 · ECS（EnTT）· Jolt 物理

## 功能特性

### 渲染
- **多后端 RHI** —— OpenGL 4.5、Vulkan 1.3、D3D11（失败自动回退）
- **RenderGraph** —— 基于 DAG 的帧图，依赖驱动的 Pass 调度与死 Pass 剔除
- **PBR 管线** —— 金属-粗糙度工作流、基于图像的光照（IBL）、GPU 驱动实例化（SSBO）
- **级联阴影贴图**（方向光 + 聚光 + 点光）
- **DSSL**（DS 着色语言）—— 表面着色器抽象（自动注入光照/阴影/GI）
- **Bloom** 后处理，多级降/升采样
- **Skybox / SkyLight** 环境贴图
- 2D **精灵批处理**、3D **网格渲染** 带实例化
- **粒子系统** —— GPU 友好的发射器，带曲线编辑器
- **地形** —— 高度图雕刻 + splat 纹理绘制
- **Clustered Forward+** 光源裁剪

### 编辑器
- **Inspector** —— 20+ 组件类型，支持撤销/重做
- **Hierarchy** —— 搜索、拖拽排序、父子关系
- **Viewport** —— ImGuizmo 平移/旋转/缩放、框选、Color-ID 拾取
- **Tilemap** / **Terrain** 编辑器
- **动画时间线** / **材质 PBR** 面板
- **Prefab** 保存/加载、多场景标签页
- **Console** 支持源码跳转、**Lua REPL**
- **File → Build Game...** —— 将资产打包为 `.dpak` 并导出独立可执行程序

### 脚本
- 内嵌 **Lua 5.4**，22+ 引擎绑定
- 编辑模式下热重载（`PumpLuaScriptHotReloads`）
- 编辑器内交互式 Lua 控制台

### 运行时
- **ECS** 基于 EnTT
- **物理** —— Box2D（2D）、Jolt Physics（3D）
- **音频** —— 带范围可视化的位置音频
- **Job 系统** —— 多线程任务图
- **资产管线** —— `.dmesh` / `.dmat` / `.danim` / `.dskel` / `.dpak`
- **内存管理子系统** —— 统一分配门面、按标签追踪 + 泄漏报告、线性/帧/scratch/池分配器、预算、STL 适配器、`Handle/HandleTable`、可选 mimalloc 后端（详见 [`docs/architecture/MEMORY_MANAGEMENT_DESIGN.md`](docs/architecture/MEMORY_MANAGEMENT_DESIGN.md)）
- **网络层（实验性，默认关闭）** —— 基于 GameNetworkingSockets 的可靠/不可靠 + 多通道加密传输底座、C ABI、异步 HTTP 客户端，以及最小可用的服务器权威复制层 MVP（实体 spawn/despawn、Transform 全量快照、带所有权校验的输入 RPC）。详见 [`docs/architecture/NETWORK_LAYER_DESIGN.md`](docs/architecture/NETWORK_LAYER_DESIGN.md)

---

## 目录结构

```
DSEngine/
├── engine/            引擎核心（默认静态库；DSE_BUILD_SHARED=ON 时为 DSEngine.dll）
│   ├── assets/        资产管理器、pak 读写、扫描器
│   ├── audio/         音频播放
│   ├── core/          Job 系统、事件总线、服务定位器、模块、内存管理（core/memory）
│   ├── ecs/           ECS 组件与系统
│   ├── http/          异步 HTTP(S) 客户端（实验性，DSE_ENABLE_HTTP）
│   ├── input/         键盘 / 鼠标 / 手柄
│   ├── net/           网络传输抽象 + GNS 后端 + C ABI + 复制层（实验性，DSE_ENABLE_NET）
│   ├── physics/       Box2D 封装，PhysX 可选
│   ├── render/        RHI 抽象、Pass、材质、着色器
│   ├── runtime/       引擎应用外壳、帧管线
│   ├── scene/         场景序列化（JSON）
│   └── scripting/     Lua 虚拟机、绑定、热重载
├── apps/
│   ├── editor_cpp/    基于 ImGui 的编辑器（dsengine-editor.exe）
│   ├── standalone/    独立游戏运行时（DSEngine_Game.exe）
│   ├── runtime/       Lua 宿主与 C++ 宿主示例
│   └── tools/         AssetBuilder 命令行工具
├── modules/           可选引擎模块（地形、动画等）
├── plugins/           可选运行时插件
├── samples/           运行时加载的演示（cpp / lua / plugins）
├── examples/          独立示例工程（KF_Framework、sdk_consumer、stress_test）
├── script/            运行时 Lua 库（通过 Lua package.path 加载，随引擎一同发布）
├── scripts/           构建 / CI / 打包脚本（scripts/win/ 存放 Windows .bat）
├── tools/             代码生成、着色器编译器、资产烘焙等
├── data/              着色器、贴图、模型、字体
├── tests/             GoogleTest 用例（unit/integration/smoke）
└── docs/              架构与路线图文档
```

> **`script/` 与 `scripts/`** —— `script/`（单数）存放引擎**运行时**加载的 Lua 库（硬编码进 Lua
> `package.path`，随引擎安装）；`scripts/`（复数）存放**构建期**的开发者/CI 脚本。
>
> **`samples/` 与 `examples/`** —— `samples/` 是引擎运行时加载的小演示
> （`samples/lua`、`samples/cpp`、`samples/plugins`）；`examples/` 是消费引擎/SDK 的自包含示例
> 工程（`KF_Framework`、`sdk_consumer`、`stress_test`）。

---

## 可执行程序目标

所有可执行程序统一输出到仓库根目录的 **`bin/`**。其中 `dse_engine`、`dse_standalone`、`dse_example_lua`、
`DSEngine_example_cpp` 会按构建类型追加后缀（`_debug` / `_release` / `_relwithdebinfo` / `_minsizerel`）；
编辑器、AssetBuilder 与各工具/测试目标**无配置后缀**。

> 注意：`dse_engine` 本身是**库**（默认静态库；`DSE_BUILD_SHARED=ON` 时编译为 `DSEngine.dll`），并非可执行程序，下表一并列出以说明依赖关系。

### 默认桌面构建会产出的目标

| 目标 | 输出（`bin/`） | 用途 | 主要依赖 |
|------|----------------|------|----------|
| `dse_engine` | `DSEngine[_配置].lib`（或 `.dll`） | 引擎核心库，被下方所有应用/工具链接 | EnTT、Box2D、Jolt、assimp、Lua、freetype 等 |
| `dse_standalone` | `DSEngine_Game[_配置].exe` | **独立游戏运行时**（无编辑器 UI）；编辑器 `File → Build Game...` 导出的就是它 | `dse_engine` |
| `DSEngine_example_cpp` | `DSEngine_c++[_配置].exe` | **C++ 宿主示例**：用 C++ 直接驱动引擎的入门样例 | `dse_engine` |
| `dse_example_lua` | `DSEngine_lua[_配置].exe` | **Lua 脚本宿主示例**：用 Lua 跑游戏逻辑（需 `DSE_ENABLE_LUA`，默认 ON） | `dse_engine` + Lua |
| `dse_cli` | `dse.exe` | **Headless 工程 CLI**：脱离编辑器建项目模板 / 打包加密资源包 / 一键 build（`new` / `pack` / `build`） | `dse_engine` |
| `AssetBuilder` | `AssetBuilder.exe` | **资产导入/烘焙 CLI**：glTF/FBX/贴图 → `.dmesh` / `.dmat` 等引擎格式 | tinygltf、assimp（可选）、glm |
| `dse_dssl_compiler` | `dse_dssl_compiler.exe` | **DSSL 着色语言编译器** | —— |
| `dse_shader_compiler` | `dse_shader_compiler.exe` | **着色器编译器**（仅当未找到预构建编译器时才构建；也可用 `-DDSE_HOST_SHADER_COMPILER=<path>` 指定现成的） | —— |
| `dse_gtest_unit_tests` | `bin/` | GoogleTest **单元测试**（`DSE_BUILD_GTESTS`，默认 ON） | googletest、`dse_engine` |
| `dse_gtest_integration_tests` | `bin/` | GoogleTest **集成测试** | googletest、`dse_engine` |
| `dse_gtest_smoke_tests` | `bin/` | GoogleTest **冒烟测试** | googletest、`dse_engine` |
| `dse_serialize_smoke` | `bin/` | 序列化冒烟（需 `DSE_ENABLE_LUA`） | `dse_engine` |

### 需显式开启开关才构建的目标（opt-in）

| 目标 | 输出（`bin/`） | 用途 | 开启开关（默认值） |
|------|----------------|------|--------------------|
| `dse_editor_cpp` | `dsengine-editor.exe` | **可视化编辑器**（Win32 GUI，ImGui） | `DSE_BUILD_EDITOR=ON`（默认 OFF） |
| launcher | —— | 启动器（仓库提供 `apps/launcher` 时构建） | `DSE_BUILD_LAUNCHER=ON`（默认 OFF） |
| `dse_http_smoke` / `dse_http_lua_smoke` | `bin/` | HTTP 客户端冒烟 / Lua 绑定冒烟 | `DSE_ENABLE_HTTP=ON`（默认 OFF） |
| `dse_net_smoke` | `bin/` | 网络**传输层**回环冒烟（可靠/不可靠 + 多通道） | `DSE_ENABLE_NET=ON`（默认 OFF） |
| `dse_net_capi_smoke` | `bin/` | 网络 **C ABI** 冒烟 | `DSE_ENABLE_NET=ON` |
| `dse_net_lua_smoke` | `bin/` | 网络 **Lua 绑定** 冒烟 | `DSE_ENABLE_NET=ON` |
| `dse_repl_smoke` | `bin/` | **复制层回环冒烟**：spawn → 全量快照一致 → 所有权负例 → 属主输入 RPC 服务器权威移动 → despawn | `DSE_ENABLE_NET=ON` |

> 其它：`examples/sdk_consumer` 的 `consumer_example` 是 SDK 最小消费示例，随 SDK/示例工程构建，不在引擎主默认构建里；
> Android 平台另会产出 `dse_android_host`（`.so` 共享库，并非独立可执行程序）。

一句话：**默认桌面构建 ≈ 3 个应用宿主（standalone / cpp / lua）+ 4 个工具（dse / AssetBuilder / dssl / shader）+ 4 类测试程序**；
编辑器、HTTP、网络（含 `dse_repl_smoke`）均需显式开开关。要跑复制层冒烟，记得 `-DDSE_ENABLE_NET=ON`。

---

## 环境要求

| 工具 | 版本 |
|------|------|
| **CMake** | 3.24+ |
| **Visual Studio** | 2022（v143 工具集） |
| **Windows SDK** | 10.0.22000+ |
| **Git** | 需支持子模块 |

所有第三方依赖均以 in-tree 形式内置在 `depends/` 下，无需任何包管理器。

---

## 构建

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

#### 编译整个工程

打开文件夹后，用 **生成 ▸ 全部生成（Build All）** 或菜单底部 **生成 → DSEngine** 编译整个工程；
顶部 **启动项（Startup Item）** 下拉里选 `dsengine-editor.exe`（编辑器）或 `DSEngine_Game_*.exe`（运行时）即可 F5 调试。

> 若 VS 把默认目标显示成 `glview` 等 `depends/` 下的**第三方示例**（如 tinygltf 的 `glview`），那只是旧的
> `.vs/` 缓存残留——这些示例**并不在 DSEngine 的构建里**（tinygltf 仅作头文件 include，从不 `add_subdirectory`）。
> 关闭 VS、删除项目根目录的 **`.vs/` 与 `out/`** 后重新"打开文件夹"即可，再用 **生成 ▸ 全部生成** 编译。

### 手动配置（命令行）

```powershell
# 带子模块克隆
git clone --recursive <repo-url>
cd DSEngine

# 生成工程
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5

# 构建各目标
cmake --build build_vs2022 --config Release --target dse_engine        # 引擎库
cmake --build build_vs2022 --config Release --target dse_editor_cpp    # 编辑器（需 -DDSE_BUILD_EDITOR=ON）
cmake --build build_vs2022 --config Release --target dse_standalone    # 独立运行时
cmake --build build_vs2022 --config Release --target dse_cli           # headless 工程 CLI（输出 dse.exe）
cmake --build build_vs2022 --config Release --target dse_example_lua   # Lua 示例宿主
```

或使用便捷脚本：

```powershell
scripts\win\build_fast_editor.bat   # 仅编辑器
scripts\win\build_fast_lua.bat      # 仅 Lua 宿主
scripts\win\build_all.bat           # 全部
```

可执行文件输出到 `bin/`。

### 常用 CMake 开关

| 开关 | 默认 | 说明 |
|------|------|------|
| `DSE_BUILD_SHARED` | OFF | 将 `dse_engine` 构建为 DLL（需公共 API 上有 `DSE_EXPORT`） |
| `DSE_BUILD_EDITOR` | OFF | 构建 ImGui 编辑器 `dse_editor_cpp` |
| `DSE_BUILD_LAUNCHER` | OFF | 构建启动器目标 |
| `DSE_BUILD_GTESTS` | ON | 构建 GoogleTest 测试目标 |
| `DSE_ENABLE_LUA` | ON | 启用 Lua 脚本运行时 |
| `DSE_ENABLE_NAVMESH` | ON | 启用 NavMesh / 寻路（Recast/Detour） |
| `DSE_ENABLE_NET` | OFF | 启用网络层（GameNetworkingSockets 后端 + 复制层） |
| `DSE_ENABLE_HTTP` | OFF | 启用异步 HTTP(S) 客户端（IXWebSocket + OpenSSL） |
| `DSE_MEM_BACKEND` | system | 内存后端：`system`（零依赖）或 `mimalloc`（需 `depends/mimalloc` 子模块） |

---

## 新手指南 —— 独立开发者上手

> 从零开始到导出游戏的完整流程。

### 步骤 1：构建

```powershell
# 生成工程（仅第一次）
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=ON

# 构建编辑器（会自动同时构建 AssetBuilder + 独立运行时）
cmake --build build_vs2022 --config Release --target dse_editor_cpp
```

> 构建成功后，`bin/` 目录里应包含（以下为 Release 构建）：
> - `dsengine-editor.exe` —— 编辑器（无配置后缀）
> - `DSEngine_Game_release.exe` —— 独立游戏运行时（Debug 构建为 `DSEngine_Game_debug.exe`）
> - `AssetBuilder.exe` —— 资产转换工具（无配置后缀）

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
   - `MeshRendererComponent` —— 渲染 3D 网格
   - `RigidBody3DComponent` + `BoxCollider3DComponent` —— 物理模拟
   - `LuaScriptComponent` —— 绑定 Lua 脚本
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

- 点击工具栏 ▶ **Play** —— 启动 Lua 脚本和物理
- ⏸ **Pause** —— 暂停（可单帧步进）
- ⏹ **Stop** —— 停止并恢复场景初始状态

### 步骤 7：构建独立游戏

1. **File → Build Game...**
2. 填写输出目录和游戏名称
3. 点击 **Build** —— 编辑器会：
   - 将 `DSEngine_Game.exe` 复制并重命名
   - 将所有资产打包为 `game.dpak`
   - 复制必要的 DLL
4. 构建完成后点 **Open Folder** 或 **Run** 直接测试

---

## 快速开始

### 运行编辑器

```powershell
bin\dsengine-editor.exe
```

- **File → Open Scene** 加载 `.json` 场景
- **File → Build Game...** 导出独立游戏包

### 运行 Lua 演示

```powershell
build_fast_lua.bat
bin\DSEngine_lua_debug.exe
```

默认演示：`samples/lua/phase1_2d_physics_showcase.lua`（2D 物理）。

通过编辑 `samples/lua/config.lua` 切换演示：

```lua
Config.game_entry = "3d_scene_showcase"
```

可用入口：`3d_triangle`、`3d_cube`、`3d_textured_cube`、`3d_lighting_showcase`、`3d_material_showcase`、`3d_camera_showcase`、`3d_scene_showcase`、`3d_skybox_environment`、`3d_postprocess_showcase`、`3d_particles_showcase`、`3d_physics_stack` 等。

### 批量验证所有演示

```powershell
python tools\verify_lua_3d_demos.py --entries all
```

截图与日志输出到 `tmp/lua_3d_verify/`。

---

## 构建独立游戏

有三种方式，底层共用同一套打包/加密/挂载实现，端到端一致：

### 方式 A：`dse` Headless CLI（推荐，像 Cocos 那样纯命令行）

```bash
# 1) 建项目模板（empty | 2d | 3d | lua）
dse new lua MyGame

# 2) 一键 build：定位 DSEngine_Game 运行时、拷贝 exe+DLL、打包加密、生成 launch.bat
#    --key 省略 = 明文打包；给定 >=16 字节的 key = AES-128-CTR 加密
dse build MyGame --out dist --key 0123456789abcdef

# 3) 运行（launch.bat 已写好 --bundle/--key/--script）
dist/launch.bat

# 也可只打包某个目录为（可加密）资源包
dse pack MyGame dist/game.bun --key 0123456789abcdef
```

> `dse` 与 `DSEngine_Game` 需在同一目录（或其 `bin/` 子目录）下，`build` 才能定位到运行时。
> 默认构建会把两者都产出到仓库 `bin/`。

### 方式 B：编辑器

**File → Build Game...** → 勾选 **Encrypt (AES-128 → game.bun)** 并填入 `>=16` 字节密钥即可端到端加密；
不勾选则导出明文 `game.dpak`。加密构建会自动生成 `launch.bat`，`Run` / `Build & Run` 也会带上 `--key` 启动。

### 方式 C：手动

1. 构建 `dse_standalone` 目标
2. 打包资产：`dse pack` / `pak_writer` / 编辑器 Build 对话框
3. 把 `game.bun`（或 `.dpak`）放在 `DSEngine_Game.exe` 旁边 —— 启动时自动检测；加密包需用 `--key` 传密钥

### 运行时命令行参数

`--scene=`、`--pak=`、`--bundle=`、`--key=`、`--script=`、`--width=`、`--height=`、`--title=`

### 端到端加密如何跑通

打包侧 `PackDirectoryToBundle`（`engine/assets/bundle_packer.{h,cpp}`，CLI / 编辑器共用）→ zip 压缩后 AES-128-CTR 加密为 `game.bun`；
运行时 `EngineInstance::Init` 用同一 key `MountBundle` 解密并填充 VFS → `AssetManager::LoadFileToMemory`
（VFS → `.dpak` → 磁盘）与 Lua 的 VFS `require` searcher 直接从包内读取，**磁盘不留明文**，加密后的 Lua 也能 `require`。

---

## 环境变量

| 变量 | 说明 |
|------|------|
| `DSE_STARTUP_LUA` | 覆盖 Lua 启动脚本路径 |
| `DSE_MAX_FRAMES` | N 帧后自动退出（用于 CI / 截图） |
| `DSE_SCREENSHOT_PATH` | 退出时保存 PNG 截图 |
| `DSE_SCREENSHOT_TARGET` | 回读源：`main` 或 `scene` |
| `DSE_RENDER_READBACK_DIAG` | 设为 `1` 开启渲染目标诊断 |

---

## 测试

```powershell
# 一键：配置（开启 gtest）+ 构建三套测试 + 用 ctest 运行
build_fast_tests.bat

# 或手动：
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_GTESTS=ON
cmake --build build_vs2022 --config Debug --target dse_gtest_unit_tests dse_gtest_integration_tests dse_gtest_smoke_tests --parallel
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L gtest
```

默认 `build_fast_tests.bat` 配置（关闭 3D）约运行 2,269 个 GoogleTest 用例（1787 单元 / 440 集成 / 42 冒烟），
覆盖 ECS、物理、序列化、资产管线、渲染等。开启 `-DDSE_ENABLE_3D=ON` 的完整构建会编译额外的 3D 门控测试套件（约 2,600 个）。

> 网络/HTTP 相关冒烟（`dse_net_smoke`、`dse_net_capi_smoke`、`dse_net_lua_smoke`、`dse_repl_smoke`、`dse_http_smoke` 等）
> 默认不构建，需分别开启 `-DDSE_ENABLE_NET=ON` / `-DDSE_ENABLE_HTTP=ON`，并已注册到 ctest，可用
> `ctest -R dse_repl_smoke -V` 单独运行。

---

## 贡献

欢迎贡献！报告 bug 或提需求时请使用 [issue 模板](.github/ISSUE_TEMPLATE/)。

1. Fork 本仓库
2. 创建特性分支（`git checkout -b feature/my-feature`）
3. 提交你的修改
4. 发起 Pull Request

提交前请确保你的修改能在三套 RHI 后端（OpenGL / Vulkan / D3D11）上编译通过。

---

## 许可证

本项目基于 **MIT License** 授权 —— 详见 [LICENSE](LICENSE) 文件。

---

> 架构细节见 [`docs/architecture/ARCHITECTURE.md`](docs/architecture/ARCHITECTURE.md)。
> 着色器系统见 [`docs/architecture/SHADER_SYSTEM.md`](docs/architecture/SHADER_SYSTEM.md)。
> 内存管理设计见 [`docs/architecture/MEMORY_MANAGEMENT_DESIGN.md`](docs/architecture/MEMORY_MANAGEMENT_DESIGN.md)。
> 网络层设计见 [`docs/architecture/NETWORK_LAYER_DESIGN.md`](docs/architecture/NETWORK_LAYER_DESIGN.md)。
> 开发路线图见 [`docs/roadmap/PROGRESS_REPORT.md`](docs/roadmap/PROGRESS_REPORT.md)。
