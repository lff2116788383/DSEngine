# DSEngine 测试说明

## 运行方式

### 命令行（推荐，CI 同款）

```bash
ctest -C Debug --test-dir build_vs2022
# 或直接跑单个可执行文件
./bin/dse_gtest_unit_tests.exe
./bin/dse_gtest_integration_tests.exe
./bin/dse_gtest_smoke_tests.exe
```

`ctest` 默认单进程顺序执行，是最稳定的方式。

### VS2022 测试资源管理器

VS 的 Google Test Adapter 默认可能**并行**执行用例，会引发两类“假失败”，见下文。
为避免这些噪声，请启用仓库根目录的 `DSEngine.runsettings`：

- 菜单 `Test > Configure Run Settings > Select Solution-wide runsettings File`，选择 `DSEngine.runsettings`；或
- 菜单 `Test > Test Settings` 勾选 `Auto Detect runsettings Files`。

## 关于在 VS 里看到的“失败”

### 一、被误报为“崩溃/失败”的 SKIPPED 用例（不是失败）

部分用例在当前环境缺少依赖时会主动 `GTEST_SKIP()`，这是**预期行为**。命令行/ctest
会正确显示为通过/跳过；VS 的 Google Test Adapter 在并行 + 输出交错时可能把它们误标为
“!! 此测试可能已崩溃 !!”，但每条输出都带 `[ SKIPPED ]`。启用 `DSEngine.runsettings`
（顺序执行）即可消除该误报。

这些用例及其跳过条件：

| 用例 | 跳过条件 | 若想让它真正运行 |
|------|----------|------------------|
| `GLRhiSmokeTest.*`、`OpenGLRhiDeviceTest.*` | 需要 GLEW + 完整 GL 上下文，纯测试环境无 | 通过运行时（带窗口/GLEW 初始化）运行，而非无头单测 |
| `Physics3DSmokeTest.PhysXNotEnabled_SkipAll` | 未定义 `DSE_ENABLE_PHYSX` | 编译时定义 `DSE_ENABLE_PHYSX` 并链接 PhysX |
| `StlAllocatorTest.VectorAllocations...`、`StringHeapAllocationIsTagged` | 内存追踪未启用（`Memory::TrackingEnabled()` 为 false） | 以启用内存追踪的配置构建 |

> 注：VS 输出面板里可能出现形如 `鏈惎鐢ㄨ拷韪...` 的乱码，那是 UTF-8 中文跳过原因
> （“未启用追踪，跳过标签计费断言”）按 GBK 显示的 mojibake，不影响测试结果。

结论：这些 SKIP 用例**无需修改**，它们正确反映了“当前环境不具备该能力”。

### 二、并行执行下基于文件系统的偶发失败（已修复）

`AssetLruTest`、`AssetAsyncExpandedTest`、`AssetHotReloadTest`、`AssetBundleIntegrationTest`
原先各自共用一个固定的临时目录/文件名（如 `%TEMP%\dse_asset_lru_test`、`test_output.bun`）。
并行执行时，一个用例的 `TearDown` 删除目录会破坏另一个正在运行的用例，导致
`Access is denied` / `被另一进程占用` / `LoadDmesh 返回 nullptr` 等失败。

已将这些 fixture 改为**每个用例使用唯一临时路径**（基于用例名 + 时间戳 + 计数器），
并把清理改为不抛异常的 `remove_all(path, error_code)`。现在无论是否并行都不再冲突。
