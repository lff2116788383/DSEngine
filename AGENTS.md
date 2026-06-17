# AGENTS.md

本文件是 **所有 AI 代理**（Cursor / Claude Code / CodeBuddy / Roo Code / Trae / Aider 等）在本仓库工作时的**首要项目指南**。它面向工具中立、与代码现状对齐。

**优先级**：会话中的 system / developer / 用户指令 > 本文件 > 其它规则文档（`.trae/`、`.codebuddy/`）> 历史文档与注释。
**真理源**：当任何文档与实际代码/构建脚本/CMake 冲突时，**以代码现状为准**，并顺手修正文档。

> 详细规则见 [`.trae/rules/project_rules.md`](.trae/rules/project_rules.md) 与 [`.codebuddy/rules/`](.codebuddy/rules/)（开发 / 测试 / 文档三份）。上手与命令见 [`README.md`](README.md)。本文件是这些内容的权威摘要 + 索引，三者口径必须一致。

---

## 1. 项目是什么

**DSEngine** —— 轻量级 **C++20 游戏引擎**，自带可视化编辑器、Lua 脚本与 2D/3D 渲染管线。

- **渲染**：多后端 RHI（OpenGL 4.5 / Vulkan 1.3 / D3D11，失败自动回退）· RenderGraph（DAG 帧图）· PBR + IBL · 级联阴影 · Bloom 等后处理 · Clustered Forward+ · DSSL 着色语言。
- **运行时**：ECS（EnTT）· 物理 Box2D(2D) / Jolt(3D) · 音频 · Job 系统 · 资产管线（`.dmesh/.dmat/.dpak/.bun`）· 内存子系统（`engine/core/memory`）· 实验性网络层（GNS，默认关）。
- **脚本**：内嵌 Lua 5.4（sol2 绑定）+ 编辑器内热重载/REPL。
- **工具**：ImGui 编辑器、`dse` headless CLI（建项目/打包/build）、AssetBuilder、着色器/DSSL 编译器。
- **平台**：Windows（主）/ Linux(WSL) / Android(NDK) / Web(Emscripten, WebGL2)。
- **版本**：`0.1.0-alpha`（见 [`CMakeLists.txt`](CMakeLists.txt) `project(... VERSION 0.1.0)` + `DSEngine_VERSION_PRERELEASE "alpha"`，约第 8、15 行）。SemVer 预发布标签；正式发布时把 `PRERELEASE` 置空字符串。

---

## 2. 目录结构与依赖方向

```text
apps/  ->  modules/  ->  engine/  ->  depends/
```

- `apps/` 可依赖所有下层；`modules/` 只能依赖 `engine/` 与 `depends/`；**`engine/` 不得依赖 `modules/` 或 `apps/`**。
- 发现历史耦合时，新增改动**不得扩大**耦合面。

| 目录 | 职责 |
|------|------|
| `engine/` | 引擎核心（默认静态库；`DSE_BUILD_SHARED=ON` 时为 `DSEngine.dll`）。子目录：`assets/ audio/ core/ ecs/ http/ input/ net/ physics/ render/ runtime/ scene/ scripting/ ...` |
| `modules/` | 可选引擎模块（`gameplay_2d`、`gameplay_3d` 等）。**`gameplay_3d` 在启用 3D 时静态编入 `dse_engine`，不要按旧 DLL 模块思路设计** |
| `apps/` | `editor_cpp/`（编辑器）、`standalone/`（独立运行时）、`runtime/`（Lua/C++ 宿主示例）、`tools/`（AssetBuilder） |
| `samples/` | 引擎**运行时**加载的小演示（`lua/ cpp/ plugins/`） |
| `examples/` | 消费引擎/SDK 的**自包含**示例工程（`sdk_consumer`、`stress_test`、`KF_Framework`） |
| `script/`（单数） | 引擎**运行时** Lua 库（硬编码进 Lua `package.path`，随引擎发布） |
| `scripts/`（复数） | **构建期** 开发者/CI 脚本；Windows `.bat` 在 [`scripts/win/`](scripts/win/) |
| `tools/` | 代码生成、着色器/DSSL 编译器、资产烘焙、验证脚本（py） |
| `data/` | 着色器源、贴图、模型、字体 |
| `tests/` | GoogleTest 用例（`unit / integration / smoke`） |
| `depends/` | **in-tree git 子模块**，所有第三方依赖（无需包管理器） |
| `docs/` | 架构与路线图文档 |

---

## 3. 构建

> **依赖是 in-tree 子模块，克隆后必须先初始化**，否则 CMake 配置会以明确错误中止：
> ```bash
> git submodule update --init --recursive
> ```

### 3.1 推荐：CMakePresets（Ninja，输出到 `out/build/<preset>`）
见 [`CMakePresets.json`](CMakePresets.json)。已固定 `CMAKE_POLICY_VERSION_MINIMUM=3.5`（兼容 CMake 4 + 旧依赖），无需手传。

```bash
cmake --preset windows-x64-debug          # GL+Vulkan+D3D11 + 编辑器 + GTest（需 VS 开发者环境/vcvars64）
cmake --build --preset windows-x64-debug
ctest  --preset windows-x64-debug         # 跑 gtest 标签用例
```

| 预设组 | 目标系统 | 后端 | 备注 |
|--------|----------|------|------|
| `windows-x64-{debug,relwithdebinfo,release}` | 本地 | GL + Vulkan + D3D11 | 编辑器 + GTest，Ninja + MSVC |
| `wsl-{debug,relwithdebinfo,release}` | WSL/Linux | GL (+Jolt) | 静态库，关 D3D11/Vulkan/GTest（与 CI `build-linux` 一致） |
| `web-{debug,release}[-3d]` | Emscripten | WebGL2(=GLES3.0) | 需 `$EMSDK` 生效；目标 `dse_web_host`；`-3d` 保留前向 3D |

### 3.2 手动 / 脚本（VS 2022 生成器，构建目录 `build_vs2022`）
脚本与 CI 走这条路（`BUILD_DIR=build_vs2022`）：

```powershell
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build_vs2022 --config Release --target dse_engine

# 便捷脚本（均在 scripts\win\）
scripts\win\build_all.bat          # 全部
scripts\win\build_fast_editor.bat  # 仅编辑器
scripts\win\build_fast_lua.bat     # 仅 Lua 宿主
scripts\win\build_fast_tests.bat   # 配置+构建三套测试+ctest
scripts\win\verify_all.bat         # 全链路验证
```

> 若工作区根目录没有 `CMakeCache.txt`，**不要**直接对 `.` 执行 `cmake --build`；先用已有构建目录（`build_vs2022` 或 `out/build/<preset>`）或先配置。

### 3.3 关键 CMake 开关（默认值以 [`CMakeLists.txt`](CMakeLists.txt) 为准，勿凭记忆）

| 开关 | 默认 | 说明 |
|------|:----:|------|
| `DSE_ENABLE_2D` / `DSE_ENABLE_3D` | ON / ON | 2D / 3D 运行时路径 |
| `DSE_ENABLE_JOLT` / `DSE_ENABLE_PHYSX` | ON / OFF | 3D 物理后端，**互斥**（同时开报 FATAL_ERROR） |
| `DSE_ENABLE_VULKAN` | ON（Android OFF） | Vulkan 后端 |
| `DSE_ENABLE_D3D11` | ON（仅 Windows） | D3D11 后端 |
| `DSE_ENABLE_LUA` / `DSE_ENABLE_NAVMESH` | ON / ON | Lua 脚本 / Recast 寻路 |
| `DSE_ENABLE_SPINE` / `DSE_ENABLE_ASSIMP` | OFF / OFF | 2D 骨骼 / FBX-OBJ 导入（缩小包体，默认关；glTF 不受 Assimp 影响） |
| `DSE_ENABLE_NET` / `DSE_ENABLE_HTTP` | OFF / OFF | 网络层(GNS) / 异步 HTTP |
| `DSE_BUILD_EDITOR` / `DSE_BUILD_GTESTS` | OFF / ON | 编辑器（preset 里置 ON） / 测试目标 |
| `DSE_BUILD_SHARED` | OFF | 把 `dse_engine` 编成 DLL |
| `DSE_MEM_BACKEND` | system | `system`（零依赖）或 `mimalloc` |

可执行产物统一输出到 `bin/`。应用宿主按配置加后缀（`_debug/_release/...`），编辑器/工具/测试**无后缀**。主要目标：`dse_engine`(库)、`dse_standalone`(`dsengine_game[_cfg].exe`)、`DSEngine_example_cpp`、`dse_example_lua`、`dse_cli`(`dse.exe`)、`AssetBuilder`、`dse_dssl_compiler`、`dse_shader_compiler`、`dse_editor_cpp`(`dsengine-editor.exe`，opt-in)。

---

## 4. 测试

- **框架是 GoogleTest**；`ctest` 只是**发现/执行入口**，不要把它说成框架。
- 用例在 [`tests/gtest/`](tests/gtest/)，分 `unit / integration / smoke` 三套（对应目标 `dse_gtest_unit_tests` / `dse_gtest_integration_tests` / `dse_gtest_smoke_tests`）。
- 运行：
  ```bat
  ctest --preset windows-x64-debug                                   # 推荐
  ctest --test-dir build_vs2022 -C Debug --output-on-failure -L gtest
  scripts\win\build_fast_tests.bat
  ```
- **先验证后交付**：任何代码改动至少配套一类真实验证（编译/测试/运行）。测试失败即阻断，不得宣称完成。
- **结果只用四档表述**：已通过 / 已失败 / 未执行 / 环境暂缓。总数为 0、目标未生成、未运行 = **不得**说“通过”。
- 新增/改核心逻辑优先补回归测试；单测避免真实窗口/图形上下文/外部 IO；测试资源须来自仓库已签入内容；测试名与 `TEST`/`TEST_F` 场景说明用**中文**写清意图/前置/预期。

---

## 5. 渲染与 RHI（改动前必读）

- 新渲染功能优先基于 [`RenderGraph`](engine/render/render_graph.h) + [`IRenderPass`](engine/render/passes/render_pass_interface.h) 扩展，**不要**把具体渲染逻辑硬编码回 [`FramePipeline`](engine/runtime/frame_pipeline.h) 流程控制。
- **改任何渲染功能/着色器参数，必须同步三后端**：
  - CPU 侧参数传递
  - OpenGL inline shader / uniform 绑定（[`gl_draw_executor.cpp`](engine/render/rhi/gl_draw_executor.cpp)）
  - Vulkan 源（[`vulkan_shader_sources.h`](engine/render/rhi/vulkan/vulkan_shader_sources.h)）
  - D3D11 源（[`dx11_shader_sources.h`](engine/render/rhi/dx11/dx11_shader_sources.h)）
  - 三后端执行器中的纹理绑定与参数布局
- 后处理参数通过 [`PostProcessRequest`](engine/render/rhi/postprocess_common.h)：`params` **只放纯 uniform（float 数值），不得混入纹理句柄**；纹理统一用 `.Tex(slot, handle)` / `.Tex3D(...)` 写入，执行器用 `request.FindTex(slot)` 读取；slot 对应 GLSL `layout(binding = N)`（spirv-cross 映射为 DX11 `tN` / Vulkan `binding=N`）。
- 新增/改 [`RhiDevice`](engine/render/rhi/rhi_device.h) 接口时，所有后端与测试桩同步。

### 5.1 着色器编译链（重要）
GLSL 源（`engine/render/shaders/src/*.{frag,vert,comp}`）**经 `dse_shader_compiler`** 编译为 SPIR-V（`.spv`）再嵌入为 C++ 头（`*.gen.h`），**烧进可执行文件**——运行时不读 `.frag`。

- `generated/` 是 **gitignore 的构建产物**；只提交 `.frag/.vert/.comp` 源。**绝不手改生成的 `.gen.h`**。
- 改着色器源后必须**重新生成 + 重建 `dse_engine`**（CMake 已配依赖会自动跑 `dse_shader_compiler`）。
- **FXC 陷阱**：在**变迭代次数循环**里（如 `for(i<sampleCount)`，运行时变量）用隐式梯度的 `texture()` 采样会触发 FXC `X3570`（梯度在变迭代循环）/`X3511`（无法展开）。全屏 RT/LUT 无 mip，改用 `textureLod(s, uv, 0.0)` 输出等价且消除报错。
- 高级特性（Compute/SSBO 等）按 RHI **能力标志在运行时门控**，核心渲染代码**不出现平台宏**。

---

## 6. 架构要点

```text
EngineInstance（生命周期）
├── ServiceLocator（依赖注入）   ├── World（ECS, EnTT）   ├── EventBus   ├── JobSystem
└── FramePipeline（帧流水线）
    ├── Gameplay2D/3D 运行时系统
    ├── RenderGraph（DAG）→ IRenderPass（PreZ/Shadow/Scene/Bloom/UI/Composite…）
    └── RhiDevice（OpenGL / Vulkan / D3D11）
```

- 核心服务统一通过 [`ServiceLocator`](engine/core/service_locator.h) 注册/获取；生命周期由 [`EngineInstance`](engine/runtime/engine_app.h) 管理（`RegisterRuntimeServices()` / `ResetRuntimeServices()`）。**禁止新增不受控全局单例**；兼容入口可留，但新逻辑走运行时注入。
- Lua 绑定用 sol2，按功能域拆分放 [`engine/scripting/lua/bindings/`](engine/scripting/lua/bindings/)；对外 Lua API 变更同步检查 [`docs/LUA_API.md`](docs/LUA_API.md)。
- 关键文件索引见 [`.trae/rules/project_rules.md`](.trae/rules/project_rules.md) 第 7.2 节。

---

## 7. 代码规范

- **C++20**（`CMAKE_CXX_STANDARD 20`，`REQUIRED ON`），但新代码与现有工程保守风格一致，不为用而用新语法。
- 命名：类型/函数 `PascalCase`，普通变量 `snake_case`，成员 `trailing_underscore_`；命名空间用 `dse::...`。
- 头文件：`#ifndef/#define` 保护宏；**不在头文件写 `using namespace`**；能前向声明就不 `#include` 重头文件。
- **内存/错误（`engine/` 层）**：默认禁用异常（`-fno-exceptions`）；**不要用裸 `new`/`delete` 管理所有权**——优先内存池/对象池/智能指针；致命错误用断言，常规错误用错误码 / `std::optional`。
- **注释**：拒绝“执行 XX 操作”式机器水文；解释**“因”而非“果”**（设计妥协/复杂算法/兼容性）；中文优先；C++ 公共接口用 Doxygen（`@param/@return/@warning`），ECS 组件字段用 `///<` 行内释义；改代码同步改注释。
- 技术债标记统一可检索格式：`// TODO: [YYYY-MM-DD] 描述` / `// FIXME: 描述`。

---

## 8. 防幻觉与变更纪律

- **谋定而后动**：改前先用工具检索代码库，**禁止凭空猜 API**。Lua 脚本里严禁臆造未经 C++ 导出的绑定——先确认绑定真实存在。
- **最小变更**：优先改现有文件，避免无谓新建/重构。
- **不擅自引入第三方库**（vcpkg/submodule/npm 等）须经用户明确授权。新增目录/target/条件分支/依赖时必须同步 [`CMakeLists.txt`](CMakeLists.txt)。
- **编辑安全**：写文件**保留原有 BOM/编码**；严禁用低级正则脚本无脑批量注入代码/注释，所有修改基于语义理解、定点编辑。
- **不伪造完成**：受阻时 Todo 保持 `in_progress` 并诚实说明阻塞。
- 命令仅限项目工作区，不执行高危/越界命令。

---

## 9. Git 与 CI

- **分支模型**：在 feature 分支开发（如 `feature/engine-lib`），通过 **PR 合入 `master`**。**绝不直接 push `master`/`main`**，不强推主干。
- **提交信息**：Conventional Commits（`feat:`/`fix:`/`docs:`/`refactor:`/`chore:` …）。
- **CI**：[`.github/workflows/ci.yml`](.github/workflows/ci.yml) 在 **push / PR 到 `master`** 及手动触发时运行，作业含：`build-and-test`(Windows)、`editor-build`(Windows)、`sdk-verify`(Windows)、`build-linux`(Ubuntu)、`build-android`(Ubuntu)、`build-web`(Ubuntu)。CI 的 `BUILD_DIR=build_vs2022`。
- **发布**：SDK 打包 [`scripts/package_sdk.ps1`](scripts/package_sdk.ps1)（win-x64，解析 `CMakeLists.txt` 的 `PRERELEASE` 行命名发行包），验证 [`scripts/verify_sdk.ps1`](scripts/verify_sdk.ps1)。当前尚无 git tag，首个目标为 `v0.1.0-alpha`。

---

## 10. 运行 / 调试 / 演示

- 运行编辑器：`bin\dsengine-editor.exe`；运行 Lua 宿主：`bin\dsengine_lua_debug.exe`（演示入口在 [`samples/lua/config.lua`](samples/lua/config.lua) 的 `Config.game_entry`）。
- 批量验证 3D 演示：`python tools\verify_lua_3d_demos.py --entries all`。
- 独立游戏（headless CLI）：`dse new lua MyGame` → `dse build MyGame --out dist [--key <≥16B>]` → `dist\launch.bat`。
- **Headless/CI/截图环境变量**：`DSE_MAX_FRAMES`（N 帧后退出）、`DSE_SCREENSHOT_PATH`、`DSE_SCREENSHOT_TARGET`(`main`/`scene`)、`DSE_STARTUP_LUA`、`DSE_RENDER_READBACK_DIAG=1`。

---

## 11. 新增功能 Checklist

- [ ] 遵循依赖方向 `apps -> modules -> engine -> depends`？
- [ ] 是否应沿 `ServiceLocator` / 运行时注入 / `RenderGraph` 路径扩展（而非硬编码进 FramePipeline）？
- [ ] 渲染改动是否同步了 OpenGL / Vulkan / D3D11 三后端 + CPU 参数 + 执行器绑定？
- [ ] 改了着色器源 → 是否重生成并重建？是否避免手改 `*.gen.h`？
- [ ] 是否需要暴露/调整 Lua API（并更新 `docs/LUA_API.md`）？
- [ ] 是否补了测试或最小验证路径，并真实跑过？
- [ ] 是否需要同步更新文档/规则（保持本文件、`.trae/`、`.codebuddy/`、README 一致）？
