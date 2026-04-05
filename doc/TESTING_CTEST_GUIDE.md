# TESTING_CTEST_GUIDE

本文档用于固化当前 Windows 本地最小测试门禁、资源系统去全局态回归入口，以及 [`CTest`](tests/CMakeLists.txt) / 脚本 / 文档三者之间的统一口径。

## 1. 目标

当前测试体系不再只停留在“可以手动跑一些测试”，而是明确区分：

- 完整回归入口
- 最小日常门禁入口
- 资源系统去全局态专项回归入口

本轮重点不是补正式 CI，而是先把本地可执行门禁固化下来。

## 2. 前置开关

顶层 [`CMakeLists.txt`](CMakeLists.txt) 中测试仍由 [`DSE_BUILD_ENGINE_TESTS`](CMakeLists.txt:22) 控制。

典型配置命令：

```bat
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF -DDSE_BUILD_ENGINE_TESTS=ON
```

## 3. 当前最小门禁集合

建议将以下用例作为当前日常最小门禁：

- `engine.unit`
- `engine.lua_runtime`
- `engine.cpp_runtime`
- `engine.resource_injection`
- `engine.spine`
- `engine.2d.ui`
- `engine.2d.physics2d`
- `engine.2d.particle`
- `engine.2d.localization`

其中：

- `engine.unit` 负责主体单元回归
- `engine.lua_runtime` 负责 Lua 运行时专项
- `engine.cpp_runtime` 负责 C++ business runtime 生命周期与注入链路
- `engine.resource_injection` 负责资源系统去全局态后的 Lua 注入回归
- `engine.spine` 覆盖 2D 资源消费链中的 Spine 入口

上述入口已在 [`tests/engine/CMakeLists.txt`](tests/engine/CMakeLists.txt) 与 [`tests/spine/CMakeLists.txt`](tests/spine/CMakeLists.txt) 中注册。

## 4. 资源系统去全局态专项验证

当前已确认工程内测试代码不再命中以下旧模式：

- [`AssetManager::Instance()`](engine/runtime/engine_app.cpp:53)
- `ConfigureLuaApiContext(... nullptr)`
- `Initialize(nullptr)`

资源系统去全局态后的关键验证入口为：

- [`engine.lua_runtime`](tests/engine/CMakeLists.txt:173)
- [`engine.resource_injection`](tests/engine/CMakeLists.txt:185)
- [`engine.cpp_runtime`](tests/engine/CMakeLists.txt:197)
- [`engine.spine`](tests/spine/CMakeLists.txt:25)

它们分别覆盖：

- Lua API context 必须显式注入 [`AssetManager`](engine/assets/asset_manager.h)
- C++ business runtime bootstrap 必须显式接收 [`AssetManager&`](engine/scripting/cpp/cpp_business_runtime.h:20)
- Spine 资源访问必须通过注入式 [`SetAssetManager(...)`](modules/gameplay_2d/spine/spine_system.h)

## 5. 推荐执行方式

### 5.1 快速构建并跑最小门禁

直接运行：

```bat
build_fast_tests.bat
```

该脚本现在会：

1. 配置测试构建
2. 只构建最关键测试目标
3. 运行最小门禁集合
4. 在终端输出失败信息

脚本文件见 [`build_fast_tests.bat`](build_fast_tests.bat)。

### 5.2 手动运行最小门禁

```bat
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.unit|engine.lua_runtime|engine.cpp_runtime|engine.resource_injection|engine.spine|engine.2d.ui|engine.2d.physics2d|engine.2d.particle|engine.2d.localization"
```

### 5.3 手动运行完整 engine 标签集合

```bat
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L engine
```

## 6. 与其他文档的关系

- [`doc/DOC-02_BUILD_AND_RUN.md`](doc/DOC-02_BUILD_AND_RUN.md) 负责总体构建与运行说明
- [`doc/DOC-04_TESTING.md`](doc/DOC-04_TESTING.md) 负责测试体系全貌与回归建议
- 本文档只负责把“当前可执行的最小门禁”写清楚

## 7. 当前边界

当前这套门禁补齐的是：

- 本地最小 CTest 基线
- 资源系统去全局态后的关键回归入口
- 脚本 / CTest / 文档口径统一

当前仍未补齐的是：

- 正式 CI 工作流
- Debug / Release 双配置持续化门禁
- 性能基线自动比对
- 3D 默认主线门禁

因此，本轮结果应理解为“测试门禁开始收紧并具备稳定入口”，而不是“完整 CI 体系已经建成”。
