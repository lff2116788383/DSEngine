# DSEngine SDK Consumer Example

本目录提供两个 SDK 消费者示例（同一个 CMake 工程的两个 target）：

| Target | 源文件 | 用途 |
| --- | --- | --- |
| `consumer_example` | `main.cpp` | **最小验证**：`find_package(DSEngine)` + 头文件边界 + 链接 DLL。 |
| `consumer_game` | `game_main.cpp` + `demo_scene.lua` | **完整 Demo**：仅用公共头经 `RunEngine(BusinessMode::Lua)` 启动引擎，加载 Lua 搭建「3D 场景 + Jolt 物理」。 |

## 使用方法

```powershell
# 1. 先打包 SDK（在引擎根目录）
.\scripts\package_sdk.ps1 -Config Release

# 2. 解压 SDK
Expand-Archive DSEngine-SDK-*.zip -DestinationPath sdk_output

# 3. 编译此示例
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_PREFIX_PATH="$(Resolve-Path sdk_output)"
cmake --build build --config Release

# 4. 运行最小验证
.\build\Release\consumer_example.exe

# 5. 运行完整 Demo（3D + 物理）。需要 exe 旁有 data/（着色器等运行时资源），
#    可从引擎 bin\data 拷贝，或用环境变量指定：
$env:DSE_DATA_ROOT = "<engine>\bin\data"
.\build\Release\consumer_game.exe                 # 开窗运行
.\build\Release\consumer_game.exe --frames=300    # 跑 300 帧后自动退出（无人值守冒烟）
```

> 无独显机器可先执行 `scripts\setup_swgl.ps1`，并设 `GALLIUM_DRIVER=llvmpipe` 用软件 OpenGL 运行。

## 验证内容

`consumer_example`（最小）：
- `find_package(DSEngine)` 成功
- 聚合头文件 `engine/dse.h` 可用
- 版本宏 `DSE_VERSION_MAJOR/MINOR/PATCH` 可用
- `ServiceLocator`, `World`, `Input` 等核心类型可见
- glm、entt 头文件通过 SDK 正确传递

`consumer_game`（完整 Demo）：
- 仅凭安装后的公共头即可包含 `engine/runtime/engine_app.h` 并调用 `dse::runtime::RunEngine`
- `BusinessMode::Lua` 宿主加载 `demo_scene.lua`，构建相机 / 平行光 / 静态地面 + 13 块动态立方体塔
- Jolt 物理在重力下驱动塔体塌落（运行时打印 `physics OK` + 顶层方块高度下降）
