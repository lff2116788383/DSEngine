# DSEngine 项目开发规则

> 自动生效于 DSEngine 项目上下文

---

## 一、语言与沟通

- **始终使用中文与用户交流**，代码注释也使用中文
- 代码注释不要过度，只在关键的架构决策、复杂的算法、或者修复 Bug 时加注释
- 不要在每行代码上都写注释，保持代码自文档化
- 用户习惯非正式但专业的技术交流风格，避免过度客套

---

## 二、代码规范与风格

### 2.1 C++ 编码规范

- **CMake 声明 C++20**（`CMAKE_CXX_STANDARD 20` + `REQUIRED ON`），但实际代码以 C++17 风格为主，C++20 特性仅在必要时才使用（如 `if constexpr (requires {...})`）
- **命名规范**：
  - 类名：`PascalCase`（如 `FramePipeline`、`Physics2DSystem`）
  - 函数/方法：`PascalCase`（如 `CreateEntity`、`FixedUpdate`）
  - 变量：`snake_case`（如 `fixed_delta_time`、`entity_count_`）
  - 成员变量：`trailing_underscore`（如 `entity_count_`、`physics_world_`）
  - 常量/枚举值：`PascalCase` 或 `kCamelCase`（如 `kMaxAccumulator`、`Joint2DType::Revolute`）
- **头文件风格**：
  - 使用 `#ifndef` / `#define` / `#endif` 头文件保护宏，前缀 `DSE_`（如 `DSE_PHYSICS2D_SYSTEM_H`）
  - 不在头文件中写 `using namespace`（`.cpp` 中酌情使用）
  - 在头文件写完整 Doxygen 风格文档注释（`/** @brief ... */`），包含 `@param`、`@return`、`@example`
  - .cpp 实现文件中只在文件顶部的 brief 注释中保留描述，函数体内部不写冗余注释，除非逻辑特别复杂

### 2.2 文件组织

- 每个类一对文件：`xxx.h` + `xxx.cpp`
- 头文件尽量轻量，能用 forward declaration 就别 `#include`
- .cpp 文件按需 `#include`，不要依赖聚合头文件

### 2.3 命名空间

```cpp
namespace dse {          // 引擎顶级命名空间
namespace core {          // 核心基础设施
namespace runtime {       // 运行时
namespace physics3d {     // 3D 物理
namespace gameplay3d {    // 3D 玩法模块
namespace gameplay2d {    // 2D 玩法模块
// render 相关命名空间在 engine/render/ 内各自定义
} }
```

- 全局工具类（如 `Physics2DSystem`）可以不放在命名空间内，直接用类名
- 不要嵌套过深的命名空间（最多 3 层）

---

## 三、架构原则

### 3.1 依赖方向

```
apps/  →  modules/  →  engine/  (单向依赖，不允许反向)
                     →  depends/ (第三方库)
```

- `apps/`（编辑器、运行时宿主）可以依赖所有下层
- `modules/`（gameplay_2d、gameplay_3d）只能依赖 `engine/` 和 `depends/`
- `engine/` 不能依赖 `modules/` 或 `apps/`
- 所有模块通过 `IModule` 接口与 `FramePipeline` 交互，不硬编码依赖

### 3.2 服务定位器

- 所有核心服务通过 `dse::core::ServiceLocator` 注册和获取
- **禁止新增全局单例**。现有单例兼容接口已标记 `[[deprecated]]`
- 服务生命周期由 `EngineInstance` 统一管理（`RegisterRuntimeServices()` / `ResetRuntimeServices()`）
- 新增服务必须走 `ServiceLocator` 路径

### 3.3 渲染架构

- 所有新的渲染功能必须基于 `RenderGraph` DAG 架构，添加新的 `IRenderPass` 子类
- 不允许在 `FramePipeline` 或 `rhi_device` 中硬编码新的渲染 Pass
- 三后端（OpenGL / Vulkan / D3D11）必须同步更新，新着色器需要在 `gl_draw_executor.cpp`（inline）、`vulkan_shader_sources.h`、`dx11_shader_sources.h` 三处同步
- 新增 `RhiDevice` 虚方法时，所有后端（含 mock）必须同步实现

### 3.4 物理系统

- 2D 物理使用 Box2D，3D 物理使用 PhysX 4.1
- 物理系统只负责模拟，不负责渲染——物理体位置通过 ECS `TransformComponent` 同步到渲染
- 物理引擎的 runtime 指针（如 `b2Body*`、`PxRigidActor*`）存储在对应的 ECS 组件中（如 `RigidBody2DComponent::runtime_body`）

### 3.5 脚本系统

- Lua 绑定使用 sol2 库
- 新增组件绑定：在 `engine/scripting/lua/bindings/` 对应文件中添加，遵循现有模式（`sol::usertype<T>`）
- 新增 Lua API 需要在 `docs/LUA_API.md` 同步更新
- Lua 绑定文件按功能域拆分（`lua_binding_ecs_rendering.cpp`、`lua_binding_ecs_phys3d.cpp` 等）

---

## 四、构建系统

### 4.1 CMake 配置

- 根 `CMakeLists.txt` 是唯一构建入口，`engine/` 下没有独立 CMakeLists.txt
- 引擎编译为单个 `DSEngine.dll` 动态库
- 条件编译开关（均在根 CMakeLists.txt 中定义）：
  - `DSE_ENABLE_3D` — 3D 运行时（默认 ON）
  - `DSE_ENABLE_PHYSX` — PhysX 物理（默认 ON）
  - `DSE_ENABLE_VULKAN` — Vulkan 后端（默认 OFF）
  - `DSE_ENABLE_D3D11` — D3D11 后端（仅 Windows，默认 ON）
  - `DSE_ENABLE_SPINE` — Spine 动画（默认 ON）
  - `DSE_BUILD_GTESTS` — GoogleTest 测试（默认 ON）
  - `DSE_BUILD_EDITOR` — 构建编辑器（默认 OFF）

### 4.2 新增源文件

- 在 `engine/` 或 `modules/` 下新增 .cpp 文件时，CMake 使用 `GLOB_RECURSE` 自动收集，**一般情况下无需修改 CMakeLists.txt**
- 但如果在 `engine/render/rhi/` 下新增后端子目录，需要在根 CMakeLists.txt 的条件编译块中添加路径

---

## 五、测试要求

### 5.1 测试规范

- 测试框架使用 GoogleTest
- 测试文件放在 `tests/gtest/unit/`（单元测试）或 `tests/gtest/integration/`（集成测试）
- 新增功能必须写对应测试才能合入
- 测试命名：`xxx_test.cpp`，测试用例名 `TEST(TestSuite, TestCase)`

### 5.2 覆盖率期望

| 模块 | 最低覆盖率 |
|------|:---------:|
| Core 基础设施（EventBus/JobSystem/ServiceLocator） | 90%+ |
| ECS（World/组件操作） | 80%+ |
| 渲染（RHI/RenderGraph） | 70%+ |
| 物理 | 70%+ |
| 场景管理 | 70%+ |
| Asset 管理 | 70%+ |

### 5.3 运行测试

```bash
# 编译并运行所有 gtest（构建目录 build_vs2022，输出到 bin/）
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L gtest

# 快速构建+测试
build_fast_tests.bat

# 完整验证（GTest + Lua 运行时 + 3D Demo）
verify_all.bat
```

---

## 六、新增功能的 Checklist

新增任何功能模块前，确认以下事项：

- [ ] 是否遵循了架构依赖方向（`apps → modules → engine`）？
- [ ] 是否需要新增 CMake 条件编译开关？
- [ ] 是否需要新增 ECS 组件（在 `engine/ecs/` 下）？
- [ ] 是否需要在三后端（GL/Vulkan/D3D11）同步实现？
- [ ] 是否需要暴露 Lua API（在 `engine/scripting/lua/bindings/` 下）？
- [ ] 是否需要注册到 `ServiceLocator`？
- [ ] 是否新增了 IRenderPass（走 RenderGraph 而非硬编码）？
- [ ] 是否编写了对应的 GTest？
- [ ] 是否更新了 `dse.h` 聚合头文件（如果新增了公开头文件）？

---

## 七、项目知识速查

### 7.1 核心架构简图

```
EngineInstance (生命周期管理)
├── ServiceLocator (依赖注入容器)
├── World (ECS 实体世界，基于 EnTT)
├── FramePipeline (帧流水线)
│   ├── IModule (gameplay_2d / gameplay_3d)
│   │   └── 各子系统（Sprite/UI/Physics/Mesh/Animation...）
│   ├── RenderGraph (DAG 渲染图)
│   │   └── IRenderPass (PreZ/Shadow/Scene/Bloom/UI/Composite...)
│   │       └── CommandBuffer (录制 → RhiDevice 执行)
│   └── RhiDevice (RHI 抽象)
│       ├── OpenGLRhiDevice
│       ├── VulkanRhiDevice
│       └── D3D11RhiDevice
├── EventBus (跨 DLL 安全的事件总线)
└── JobSystem (线程池 + 工作窃取 + 依赖链)
```

### 7.2 关键文件索引

| 文件 | 说明 |
|------|------|
| `engine/runtime/engine_app.h/cpp` | 引擎入口，EngineInstance |
| `engine/runtime/frame_pipeline.h/cpp` | 帧流水线，主循环调度 |
| `engine/ecs/world.h/cpp` | ECS 世界 |
| `engine/core/service_locator.h` | 服务定位器 |
| `engine/core/event_bus.h/cpp` | 事件总线 |
| `engine/core/event_id.h` | 事件 ID 定义（所有事件常量集中于此） |
| `engine/core/job_system.h/cpp` | 作业系统 |
| `engine/core/module.h` | IModule 接口 |
| `engine/core/memory_pool.h` | 内存池 |
| `engine/core/object_pool.h` | 对象池 |
| `engine/core/dynamic_library.h/cpp` | 动态库加载 |
| `engine/base/time.h/cpp` | 时间系统 |
| `engine/base/debug.h/cpp` | 调试日志 |
| `engine/assets/asset_manager.h/cpp` | 资产管理（异步加载/热重载） |
| `engine/assets/pak_writer.h/cpp` | PAK 打包 |
| `engine/assets/pak_reader.h/cpp` | PAK 读取 |
| `engine/render/render_graph.h/cpp` | DAG 渲染图 |
| `engine/render/passes/render_pass_interface.h` | IRenderPass 接口 |
| `engine/render/passes/builtin_passes.h/cpp` | 内置渲染 Pass |
| `engine/render/rhi/rhi_device.h` | RHI 抽象基类 + CommandBuffer |
| `engine/render/rhi/rhi_types.h` | RHI 类型定义 |
| `engine/render/rhi/gl_draw_executor.h/cpp` | OpenGL 绘制执行（含 inline shader） |
| `engine/render/rhi/ubo_manager.h/cpp` | UBO 管理（PerFrame/PerScene/PerMaterial） |
| `engine/render/rhi/rhi_factory.h/cpp` | RHI 设备工厂（环境变量 `DSE_RHI_BACKEND`） |
| `engine/render/rhi/vulkan/vulkan_rhi_device.h/cpp` | Vulkan 后端 |
| `engine/render/rhi/dx11/dx11_rhi_device.h/cpp` | D3D11 后端 |
| `engine/physics/physics2d/physics2d_system.h/cpp` | 2D 物理（Box2D） |
| `engine/physics/physics3d/physics3d_system.h/cpp` | 3D 物理（PhysX） |
| `engine/ecs/components_3d.h` | 3D 组件（Mesh/Camera/Light/Bbox/PostProcess/Animator） |
| `engine/ecs/components_3d_physics.h` | 3D 物理组件（RigidBody/Collider/Joint/CharacterController） |
| `engine/ecs/components_2d.h` | 2D 组件聚合头文件 |
| `engine/input/input.h/cpp` | 输入系统（键鼠/Gamepad/ActionMapping/录制回放） |
| `engine/profiler/cpu_profiler.h/cpp` | CPU Profiler（Chrome Trace 导出） |
| `engine/dse.h` | 引擎公开 API 聚合头文件 |

### 7.3 构建与运行

```bash
# 构建目录固定为 build_vs2022，输出到 bin/
set BUILD_DIR=build_vs2022

# Debug 构建
cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF
cmake --build %BUILD_DIR% --config Debug

# Release 构建
cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF
cmake --build %BUILD_DIR% --config Release

# 启用 Vulkan 后端
cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 -DDSE_ENABLE_VULKAN=ON

# 构建编辑器
cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=ON

# 运行 Lua Runtime Host（输出在 bin/）
bin\DSEngine_lua_debug.exe --scene=samples\lua\phase2_3d_mvp.lua

# 运行编辑器
bin\dsengine-editor.exe

# 运行测试（仅 gtest 标签）
ctest --test-dir %BUILD_DIR% -C Debug --output-on-failure -L gtest

# 快速构建测试
build_fast_tests.bat

# 完整验证（构建 + 测试 + Lua demo）
verify_all.bat
```

### 7.4 常用构建脚本

| 脚本 | 用途 |
|------|------|
| `build_all.bat` | 完整构建（引擎 + 测试 + SDK 安装），支持 `--with-editor` 等参数 |
| `build_fast_cpp.bat` | 快速构建 C++ runtime 目标 |
| `build_fast_tests.bat` | 编译并运行所有 GTest |
| `build_fast_editor.bat` | 快速构建编辑器 |
| `build_fast_lua.bat` | 快速构建 Lua runtime |
| `build_fast_sdk.bat` | 打包 SDK |
| `build_fast_launcher.bat` | 构建 Tauri 启动器 |
| `verify_all.bat` | 全链路验证（GTest + Lua 构建 + 3D Demo），支持 `--skip-gtest` 等参数 |

### 7.5 开发工作流

```bash
# 改 C++ 代码 → 测试
build_fast_tests.bat

# 改渲染 → 验证三后端
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF
cmake --build build_vs2022 --config Debug --target DSEngine_lua
bin\DSEngine_lua_debug.exe --scene=samples\lua\phase2_3d_mvp.lua

# 改编辑器 → 构建+运行
build_fast_editor.bat
bin\dsengine-editor.exe

# 完整验证（提交前必做）
verify_all.bat
```

---

## 八、已知问题与注意事项

- **PhysX Extensions CRT 不匹配已解决**：commit `7917921` 从 PhysX 4.1.2 源码重新编译了 `PhysXExtensions_static_64.lib` / `PhysXPvdSDK_static_64.lib` / `PhysXCharacterKinematic_static_64.lib` 三个库为 `/MD`(Release) / `/MDd`(Debug) 版本，位于 `depends/physx/physx/bin/win.x86_64.vc142.mt/`。`dumpbin /directives` 已验证 CRT 正确。`DSE_HAS_PHYSX_EXTENSIONS` 已启用，Joint 功能（`PxFixedJointCreate`、`PxRevoluteJointCreate`、`PxD6JointCreate`）完全可用。
- **EnTT 跨 DLL 模板实例化**：所有 ECS 组件操作必须在同一 DLL（`DSEngine.dll`）中完成，不要在编辑器 exe 和引擎 dll 之间传递 `entt::registry` 内的迭代器
- **Lua 绑定性能**：sol2 在频繁调用的热点路径上有开销，性能敏感处用 C++ 实现后通过 Lua API 调用，而非在 Lua 中实现密集循环
- **Vulkan 后端默认关闭**：依赖 Vulkan SDK 或子模块，不要求所有开发者启用
- **编辑器默认关闭**：`DSE_BUILD_EDITOR=ON` 才构建编辑器
