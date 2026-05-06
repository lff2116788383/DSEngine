# DSEngine SDK Consumer Example

这是一个最小的 SDK 消费者示例，用于验证 `find_package(DSEngine)` 能正常工作。

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

# 4. 运行
.\build\Release\consumer_example.exe
```

## 验证内容

- `find_package(DSEngine)` 成功
- 聚合头文件 `engine/dse.h` 可用
- 版本宏 `DSE_VERSION_MAJOR/MINOR/PATCH` 可用
- `ServiceLocator`, `World`, `Input` 等核心类型可见
- glm、entt 头文件通过 SDK 正确传递
