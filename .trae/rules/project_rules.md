# DSEngine 项目开发规则

> 供开发者与 AI 代理参考的详细项目规则文档。Roo Code 等代理建议优先遵循仓库根目录的 [`AGENTS.md`](AGENTS.md)，本文件作为详细补充说明。

---

## 一、语言与沟通

- 默认使用中文与用户交流。
- 代码注释应优先与所在文件现有风格保持一致；中文项目说明可使用中文，接口语义、跨平台约束、第三方库交互说明可保留英文。
- 代码注释不要过度，只在关键架构决策、复杂算法、兼容性修复或易误用逻辑处添加必要注释。
- 不要在每行代码上都写注释，保持代码自文档化。
- 输出风格保持直接、专业，避免无意义客套。

---

## 二、代码规范与风格

### 2.1 C++ 编码规范

- 根 [`CMakeLists.txt`](CMakeLists.txt) 声明 C++20（`CMAKE_CXX_STANDARD 20` + `REQUIRED ON`），但新增代码优先保持与现有工程一致的保守风格，除非确有必要，不强依赖新的 C++20 语法特性。
- **命名规范**：
  - 类型：`PascalCase`（如 `FramePipeline`、`EngineInstance`）
  - 函数/方法：多数为 `PascalCase`
  - 普通变量：多为 `snake_case`
  - 成员变量：多为 `trailing_underscore`
  - 常量/枚举值：遵循现有文件风格，常见为 `PascalCase` 或 `kCamelCase`
- **头文件风格**：
  - 使用 `#ifndef` / `#define` / `#endif` 头文件保护宏
  - 不在头文件中写 `using namespace`
  - 头文件优先轻量，能前向声明就不要额外包含重头文件
  - 注释风格与现有文件保持一致；只在公共接口、复杂约束、生命周期语义处补必要说明，不要求机械补全完整 Doxygen

### 2.2 文件组织

- 优先保持现有目录职责清晰：`apps/`、`modules/`、`engine/`、`depends/` 分层明确。
- 头文件尽量轻量，能用 forward declaration 就别 `#include`。
- `.cpp` 文件按需 `#include`，不要依赖无边界的聚合头。
- 新增文件前先确认同类功能是否已有既定放置目录。

### 2.3 命名空间

- 新代码优先遵循现有 `dse::...` 命名空间体系。
- 个别历史遗留的全局类型可以保持现状，但不要把“无命名空间”当成新增代码的默认选择。
- 避免为了形式而引入过深命名空间，优先与周边代码保持一致。

---

## 三、架构原则

### 3.1 依赖方向

```text
apps/  ->  modules/  ->  engine/  ->  depends/
```

- `apps/` 可以依赖所有下层。
- `modules/` 只能依赖 `engine/` 与 `depends/`。
- `engine/` 不能依赖 `modules/` 或 `apps/`。
- 若发现现有代码存在历史性耦合，新增改动不得继续扩大该耦合面。

### 3.2 服务定位器与运行时生命周期

- 所有核心服务通过 [`ServiceLocator`](engine/core/service_locator.h) 注册和获取。
- 服务生命周期由 [`EngineInstance`](engine/runtime/engine_app.h) 管理，重点关注 [`RegisterRuntimeServices()`](engine/runtime/engine_app.cpp:183) 与 [`ResetRuntimeServices()`](engine/runtime/engine_app.cpp:206)。
- 禁止新增不受控的全局单例；兼容入口允许保留，但新逻辑应优先走运行时注入路径。
- 新增服务优先通过 `ServiceLocator` 接入，而不是把生命周期散落到调用方。

### 3.3 渲染架构

- 新渲染功能优先基于 [`RenderGraph`](engine/render/render_graph.h) + [`IRenderPass`](engine/render/passes/render_pass_interface.h) 扩展。
- 不要把新的具体渲染逻辑直接硬编码回 [`FramePipeline`](engine/runtime/frame_pipeline.h) 的流程控制代码中。
- 修改渲染相关功能时，必须同步检查 OpenGL / Vulkan / D3D11 三后端。
- 新着色器或新后处理参数通常需要同步检查：
  - CPU 侧参数传递
  - [`gl_draw_executor.cpp`](engine/render/rhi/gl_draw_executor.cpp) 中的 inline shader / uniform 绑定
  - [`vulkan_shader_sources.h`](engine/render/rhi/vulkan/vulkan_shader_sources.h)
  - [`dx11_shader_sources.h`](engine/render/rhi/dx11/dx11_shader_sources.h)
  - 三后端执行器中的纹理绑定与参数布局
- 后处理效果通过 [`PostProcessRequest`](engine/render/rhi/postprocess_common.h) 传递参数：
  - `params` **只放纯 uniform 数据**（float 数值），不得混入纹理句柄。
  - 纹理句柄统一通过 `.Tex(slot, handle)` / `.Tex3D(slot, handle)` 写入 `textures[]` 数组，三端执行器用 `request.FindTex(slot)` 读取。
  - slot 编号对应 GLSL `layout(binding = N)`（spirv-cross 映射为 DX11 `tN` / Vulkan `binding = N`）。
- 新增 [`RhiDevice`](engine/render/rhi/rhi_device.h) 接口或行为时，所有后端与测试桩都要同步。
- **着色器编译链**：GLSL 源在 [`engine/render/shaders/src/`](engine/render/shaders/src/)（`*.frag/.vert/.comp`），经 `dse_shader_compiler` 编译为 SPIR-V 再嵌入为 `*.gen.h` 烧进可执行文件；`generated/` 是 gitignore 产物，**只提交着色器源、绝不手改 `*.gen.h`**，改源后需重生成并重建 `dse_engine`。在**变迭代次数循环**里采样要用 `textureLod(s, uv, 0.0)` 而非隐式梯度 `texture()`，否则 FXC 报 `X3570`/`X3511`。

### 3.4 模块与运行时更新路径

- 2D / 3D 玩法模块优先沿现有运行时更新路径接入，而不是再设计旧式 DLL 模块边界。
- [`Gameplay3DModule`](modules/gameplay_3d/gameplay_3d_module.cpp:144) 当前已静态编入 `dse_engine`，不要按旧的独立 DLL 模式设计新依赖。
- 与 `FramePipeline` 的协作优先沿现有 `IModule`、运行时注入、RenderGraph 注册机制扩展，避免新增硬编码耦合。

### 3.5 脚本系统

- Lua 绑定使用 sol2。
- 新增组件绑定时，在 [`engine/scripting/lua/bindings/`](engine/scripting/lua/bindings/) 对应文件中添加，遵循现有模式。
- 对外 Lua API 发生变化时，应同步检查 [`docs/LUA_API.md`](docs/LUA_API.md) 是否需要更新。
- Lua 绑定文件按功能域拆分，避免一个超大绑定文件承载全部逻辑。

---

## 四、构建系统

### 4.1 CMake 配置

- 根 [`CMakeLists.txt`](CMakeLists.txt) 是唯一构建入口。
- 构建目录有两条并存路径，按需选用：
  - **CMakePresets（推荐）**：见 [`CMakePresets.json`](CMakePresets.json)，Ninja 生成器，输出到 `out/build/<preset>`（`windows-x64-{debug,relwithdebinfo,release}` / `wsl-*` / `web-*`）。已固定 `CMAKE_POLICY_VERSION_MINIMUM=3.5`。
  - **`build_vs2022`（脚本 / CI）**：VS 2022 生成器；[`scripts/win/`](scripts/win/) 下的 `.bat` 与 CI（`BUILD_DIR=build_vs2022`）走这条。
- 当前核心引擎目标是 `dse_engine`；相关宿主程序位于 [`apps/runtime`](apps/runtime/)、[`apps/standalone`](apps/standalone/) 与 [`apps/editor_cpp`](apps/editor_cpp/)。
- 常见条件编译开关以根 [`CMakeLists.txt`](CMakeLists.txt) 为准，例如：
  - `DSE_ENABLE_2D`
  - `DSE_ENABLE_3D`
  - `DSE_ENABLE_PHYSX`
  - `DSE_ENABLE_VULKAN`
  - `DSE_ENABLE_D3D11`
  - `DSE_ENABLE_SPINE`
  - `DSE_BUILD_GTESTS`
  - `DSE_BUILD_EDITOR`
  - `DSE_BUILD_LAUNCHER`
- 开关默认值以当前 CMake 配置为准，不要依赖过期文档记忆。

### 4.2 新增源文件

- 当前 [`CMakeLists.txt`](CMakeLists.txt:189) 通过 `GLOB_RECURSE` 自动收集大量 `engine/` 与 `modules/gameplay_2d/` 源文件。
- [`modules/gameplay_3d`](modules/gameplay_3d/) 在启用 3D 时会额外被加入 [`dse_engine`](CMakeLists.txt:228)。
- 一般新增 `.cpp` 不一定需要手改 CMake，但新增目录、条件编译分支、第三方依赖或新 target 时必须检查根 [`CMakeLists.txt`](CMakeLists.txt)。
- 修改构建规则时，优先保持现有批处理脚本可继续使用。

---

## 五、测试要求

### 5.1 测试规范

- 测试框架以 GoogleTest 为主。
- 测试主要位于 [`tests/gtest/`](tests/gtest/)。
- 新增功能、关键回归修复、或可独立验证的逻辑改动，应补对应测试。
- 对不易单测覆盖的渲染/平台路径，至少补最小验证路径或编译验证说明。

### 5.2 覆盖率与质量目标

- 下列覆盖率更适合作为质量目标，而不是每次改动都强制承诺的硬门槛：

| 模块 | 目标覆盖率 |
|------|:---------:|
| Core 基础设施（EventBus / JobSystem / ServiceLocator） | 90%+ |
| ECS（World / 组件操作） | 80%+ |
| 渲染（RHI / RenderGraph） | 70%+ |
| 物理 | 70%+ |
| 场景管理 | 70%+ |
| Asset 管理 | 70%+ |

### 5.3 运行测试

```bat
ctest --preset windows-x64-debug                                    :: 预设方式（Ninja, out/build）
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L gtest  :: build_vs2022 方式
scripts\win\build_fast_tests.bat
scripts\win\verify_all.bat
```

- 默认构建目录优先使用 `build_vs2022`。
- 如果工作区根目录没有 `CMakeCache.txt`，不要直接对 `.` 执行 `cmake --build`，应先使用已有构建目录或先配置构建目录。

---

## 六、新增功能 Checklist

新增或大改功能前，优先确认：

- [ ] 是否遵循依赖方向（`apps -> modules -> engine -> depends`）？
- [ ] 是否应沿现有运行时注入 / `ServiceLocator` / `RenderGraph` 路径扩展？
- [ ] 是否需要同步修改 OpenGL / Vulkan / D3D11 三后端？
- [ ] 是否涉及 CPU 参数、shader 参数、执行器绑定三者的一致性？
- [ ] 是否需要暴露或调整 Lua API？
- [ ] 是否需要补测试或最小验证路径？
- [ ] 是否需要更新公开文档或开发说明？

---

## 七、项目知识速查

### 7.1 核心架构简图

```text
EngineInstance (生命周期管理)
├── ServiceLocator (依赖注入容器)
├── World (ECS 实体世界，基于 EnTT)
├── FramePipeline (帧流水线)
│   ├── Gameplay2D / Gameplay3D 相关运行时系统
│   ├── RenderGraph (DAG 渲染图)
│   │   └── IRenderPass (PreZ / Shadow / Scene / Bloom / UI / Composite ...)
│   └── RhiDevice (RHI 抽象)
│       ├── OpenGL
│       ├── Vulkan
│       └── D3D11
├── EventBus
└── JobSystem
```

### 7.2 关键文件索引

| 文件 | 说明 |
|------|------|
| [`engine/runtime/engine_app.h`](engine/runtime/engine_app.h) / [`engine/runtime/engine_app.cpp`](engine/runtime/engine_app.cpp) | 引擎入口，`EngineInstance` |
| [`engine/runtime/frame_pipeline.h`](engine/runtime/frame_pipeline.h) / [`engine/runtime/frame_pipeline.cpp`](engine/runtime/frame_pipeline.cpp) | 帧流水线，主循环与渲染调度 |
| [`engine/runtime/runtime_update_graph.cpp`](engine/runtime/runtime_update_graph.cpp) | 运行时 update/fixed update 调度 |
| [`engine/ecs/world.h`](engine/ecs/world.h) / [`engine/ecs/world.cpp`](engine/ecs/world.cpp) | ECS 世界 |
| [`engine/core/service_locator.h`](engine/core/service_locator.h) | 服务定位器 |
| [`engine/render/render_graph.h`](engine/render/render_graph.h) | DAG 渲染图 |
| [`engine/render/passes/render_pass_interface.h`](engine/render/passes/render_pass_interface.h) | `IRenderPass` 接口 |
| [`engine/render/passes/builtin_passes.h`](engine/render/passes/builtin_passes.h) / [`engine/render/passes/builtin_passes.cpp`](engine/render/passes/builtin_passes.cpp) | 内置渲染 Pass |
| [`engine/render/rhi/rhi_device.h`](engine/render/rhi/rhi_device.h) | RHI 抽象基类 + `CommandBuffer` + `OpenGLCommandBuffer` |
| [`engine/render/rhi/gl_rhi_device.h`](engine/render/rhi/gl_rhi_device.h) | `OpenGLRhiDevice` 实现（从 rhi_device.h 拆出） |
| [`engine/runtime/frame_pipeline_modules.h`](engine/runtime/frame_pipeline_modules.h) | FramePipeline Pimpl 内部头（隔离 modules/ 依赖） |
| [`engine/render/rhi/gl_draw_executor.cpp`](engine/render/rhi/gl_draw_executor.cpp) | OpenGL 绘制执行（含 inline shader） |
| [`engine/render/rhi/vulkan/vulkan_shader_sources.h`](engine/render/rhi/vulkan/vulkan_shader_sources.h) | Vulkan shader 源 |
| [`engine/render/rhi/dx11/dx11_shader_sources.h`](engine/render/rhi/dx11/dx11_shader_sources.h) | D3D11 shader 源 |
| [`modules/gameplay_3d/gameplay_3d_module.cpp`](modules/gameplay_3d/gameplay_3d_module.cpp) | Gameplay3D 静态编入说明 |

### 7.3 构建与运行

```bat
cmake --preset windows-x64-debug          :: 推荐：CMakePresets（Ninja → out/build/<preset>）
cmake --build --preset windows-x64-debug

cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64   :: 或：VS 生成器（脚本 / CI 用）
cmake --build build_vs2022 --config Debug
scripts\win\build_fast_tests.bat
scripts\win\build_all.bat
scripts\win\verify_all.bat
```

- 如果需要启用特定能力，如 Vulkan / Editor / Launcher，以根 [`CMakeLists.txt`](CMakeLists.txt) 当前选项为准追加开关。
- 常见可执行文件位于 `bin/`，例如 `DSEngine_lua_debug.exe`、`dsengine-editor.exe`，但具体产物名仍以当前构建结果为准。

### 7.4 常用构建脚本

| 脚本 | 用途 |
|------|------|
| [`scripts/win/build_all.bat`](scripts/win/build_all.bat) | 完整构建与验证入口 |
| [`scripts/win/build_fast_cpp.bat`](scripts/win/build_fast_cpp.bat) | 快速构建 C++ 目标 |
| [`scripts/win/build_fast_tests.bat`](scripts/win/build_fast_tests.bat) | 编译并运行 GTest |
| [`scripts/win/build_fast_editor.bat`](scripts/win/build_fast_editor.bat) | 快速构建编辑器 |
| [`scripts/win/build_fast_lua.bat`](scripts/win/build_fast_lua.bat) | 快速构建 Lua runtime |
| [`scripts/win/build_fast_sdk.bat`](scripts/win/build_fast_sdk.bat) | 打包 SDK |
| [`scripts/win/build_fast_launcher.bat`](scripts/win/build_fast_launcher.bat) | 构建启动器 |
| [`scripts/win/verify_all.bat`](scripts/win/verify_all.bat) | 全链路验证 |

---

## 八、当前仓库特别注意事项

- [`FramePipeline`](engine/runtime/frame_pipeline.h) 仍保留部分兼容入口，但新逻辑优先通过 [`EngineInstance`](engine/runtime/engine_app.h) 与运行时注入路径接入。
- Gameplay3D 相关代码已静态编入 `dse_engine`，不要按旧 DLL 模块思路设计新依赖。
- Vulkan 后端默认是可选能力，修改时要注意 `DSE_ENABLE_VULKAN` 条件编译。
- D3D11 后端仅在 Windows 下启用，修改时注意 `DSE_ENABLE_D3D11` 条件编译。
- 渲染改动后，至少做受影响目标编译验证；条件允许时补最小运行时验证。

---

## 九、已知问题与说明

- 某些历史文档、示意图、旧注释可能仍保留 DLL 模块化时期的表述；若与当前代码冲突，应以代码现状为准。
- 构建目录、二进制产物名、条件编译默认值等信息可能随脚本和 CMake 演进而变化，引用时优先核对实际文件。
- 若本文件与 [`AGENTS.md`](AGENTS.md) 不一致，应优先以 [`AGENTS.md`](AGENTS.md) 的摘要规则和当前代码现状为准。
