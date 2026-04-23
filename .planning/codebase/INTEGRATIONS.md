# DSEngine Codebase Integrations

## 概览

DSEngine 当前不是典型的“云服务驱动应用”，外部集成主要集中在以下几类：

- 图形 / 窗口 / 输入运行库
- 脚本与资产导入链
- 本地桌面工具链
- 测试与截图验证链

仓库内**没有发现**必须依赖远端 SaaS、在线数据库、OAuth、Webhook 或后端 API 才能运行的主线功能。

## 运行时图形与平台集成

### OpenGL + GLFW

Runtime 与 Editor 都直接集成：

- `GLFW`：窗口、上下文与输入回调
- `OpenGL`：主要渲染后端
- `glad`：OpenGL 符号加载

关键位置：

- `engine/runtime/engine_app.cpp`
- `engine/runtime/frame_pipeline.cpp`
- `apps/editor_cpp/src/main.cpp`
- `apps/editor_cpp/CMakeLists.txt`

这属于最核心的本地平台集成。

## Lua 脚本集成

仓库原生支持 Lua 业务侧运行：

- `apps/runtime/lua_host/main.cpp` 指定 startup script
- `engine/scripting/lua/lua_runtime.cpp` 管理 Lua runtime 生命周期
- `engine/scripting/lua/bindings/` 暴露引擎能力到 Lua
- `script/` 与 `samples/lua/` 提供脚本样例

这类集成的边界非常重要：Lua 可调用的 API 必须来自真实绑定层，而不能凭空扩展。

## C++ 业务宿主集成

除了 Lua，仓库也支持通过 C++ 回调直接接入业务层：

- `ConfigureCppBusinessHooks(...)`
- `Bootstrap / Tick / Shutdown` 三段式生命周期

关键文件：

- `apps/runtime/cpp_host/main.cpp`
- `engine/scripting/cpp/cpp_business_runtime.cpp`
- `samples/cpp/phase1_demo_logic.cpp`

这说明业务逻辑可以绕过 Lua，直接以内嵌 C++ 方式运行。

## 资产导入与烹饪集成

### glTF / GLB / FBX 导入

`apps/tools/asset_builder/main.cpp` 集成了离线导入链：

- `GltfImporter`
- `FbxImporter`
- `MeshCooker`

输入格式：

- `.gltf`
- `.glb`
- `.fbx`

输出格式：

- `.dmesh`
- `.dmat`
- `.danim`
- `.dskel`

说明：

- FBX 走离线编译链，不是 runtime 直接读取
- 运行时依赖的是 DSE 自有资产格式，不是外部原始格式

关键文件：

- `apps/tools/asset_builder/main.cpp`
- `engine/assets/compiler/importer.h`
- `tests/engine/assets/importer_cooker_test.cpp`
- `tests/engine/assets/asset_builder_static_test.cpp`

## Spine 集成

仓库包含 `depends/spine-runtimes`，并在 2D 模块中集成 Spine：

- `modules/gameplay_2d/spine/spine_system.cpp`
- `modules/gameplay_2d/spine/spine_system.h`
- `tests/spine/`
- `tests/modules/gameplay_2d/spine/`

Spine 资源消费需要注入 `AssetManager`，属于已经工程化接入的功能模块，而不是 demo 级别代码。

## 物理集成

### Box2D

2D 主线默认集成 Box2D：

- `depends/box2d-2.4.1`
- `engine/physics/physics2d/`
- `tests/modules/gameplay_2d/physics/`

### PhysX（条件启用）

3D 物理通过 `DSE_ENABLE_PHYSX` 控制，说明它属于可选能力，不是默认稳定主线。

相关位置：

- `CMakeLists.txt`
- `modules/gameplay_3d/`
- `tests/engine/physics/physics3d_system_test.cpp`

## Assimp 集成

顶层 `CMakeLists.txt` 中存在 `depends/assimp` 检测与条件接入逻辑，主要用于离线资产导入链，而不是 runtime 常驻依赖。

说明：

- 该能力是否启用取决于 `depends/assimp/CMakeLists.txt` 是否存在
- `DSE_HAS_ASSIMP` 是关键开关变量

## 音频相关集成

顶层 `CMakeLists.txt` 含有：

- `add_definitions(-D USE_FMOD_STUDIO)`

这说明仓库存在 FMOD Studio 方向的音频接入意图或部分实现；不过从本次快速扫描结果看，音频系统更像是引擎内部能力的一部分，而不是清晰的外部服务型集成。后续若做音频专项规划，应优先审阅：

- `engine/audio/`
- `tests/engine/audio/audio_system_test.cpp`

## Launcher 桌面集成

`apps/launcher_tauri/` 是最明确的桌面应用外部栈集成：

- Tauri CLI / API
- React 19
- TypeScript
- Webpack
- Rust / Cargo toolchain

而 `build_all.bat` 还会自动检测甚至安装 Rust toolchain，说明 Launcher 构建与桌面打包已被纳入仓库工作流。

关键文件：

- `apps/launcher_tauri/package.json`
- `build_all.bat`

## 测试与截图集成

运行时支持通过环境变量输出截图，用于测试与回归验证：

- `DSE_SCREENSHOT_PATH`
- `DSE_SCREENSHOT_TARGET`

关键实现：

- `engine/runtime/engine_app.cpp`
- `tests/engine/runtime/reference_demo_screenshot_test.cpp`

这属于本地文件系统级集成，而不是外部网络服务。

## 本仓库未发现的常见集成

本轮扫描中，**没有发现**以下作为当前主线依赖存在：

- 远端数据库（如 MySQL / PostgreSQL / SQLite 持久化业务层）
- 用户认证服务
- 第三方支付
- 云存储 / 对象存储
- Webhook / 消息队列
- 在线配置中心
- REST / GraphQL 业务 API 主线

如果后续要引入这类能力，应视为新增架构决策，而不是延续现状。

## 结论

DSEngine 当前的 integrations 以 **本地引擎能力集成** 为主，而不是互联网业务集成：

- 图形平台：OpenGL / GLFW / glad
- 脚本：Lua / sol2
- 资产导入：glTF / GLB / FBX / Assimp
- 动画：Spine
- 物理：Box2D / 条件式 PhysX
- 桌面工具：Tauri / React / Rust
- 测试辅助：截图输出与环境变量驱动 smoke

这意味着后续规划重点通常是“系统边界、稳定性、工具链闭环”，而不是“后端服务编排”。
