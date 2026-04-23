# DSEngine Codebase Testing

## 总体测试现状

当前仓库的测试体系已经从旧的 Catch/多目录测试布局收敛到 **GoogleTest + CTest** 的新入口。实际存在的测试目录是：

- `tests/gtest/`
- `tests/gtest/unit/`

当前明确可构建的测试目标是：

- `dse_gtest_unit_tests`

当前明确注册的 CTest 条目是：

- `gtest.engine.unit`
- 标签：`engine;unit;gtest`

这意味着后续测试规划必须基于当前真实入口推进，不能继续假设 `tests/engine/`、`tests/modules/`、`tests/spine/` 等旧目录仍然存在。

## 当前测试入口

### CMake 开关

顶层 `CMakeLists.txt` 通过以下选项控制 GTest：

- `DSE_BUILD_GTESTS`

当该开关开启时，CMake 会使用本地 GoogleTest 源码：

- `depends/googletest-1.17.0`

若本地源码缺失，配置阶段会失败并提示解压位置。

### 测试目标

`tests/gtest/unit/CMakeLists.txt` 定义：

- target：`dse_gtest_unit_tests`
- link：`GTest::gtest_main`、`dse_engine`
- CTest name：`gtest.engine.unit`
- CTest labels：`engine;unit;gtest`

### 脚本入口

`build_fast_tests.bat` 是当前最直接的测试脚本：

1. 配置 `DSE_BUILD_GTESTS=ON`
2. 构建 `dse_gtest_unit_tests`
3. 执行 `ctest --test-dir build_vs2022 -C Debug --output-on-failure -L gtest`

`build_all.bat` 也暴露 `--with-tests` / `--no-tests` 选项，并通过 CMake 参数控制是否启用 GTest。

## 建议测试分层

用户提出的策略整体合理，但需要按仓库当前依赖状态分阶段落地。

### 1. 单元测试：GoogleTest

适合优先覆盖：

- 核心数学工具
- 基础数据结构
- `engine/base/`
- `engine/core/` 中无外部设备依赖的逻辑
- ECS 组件默认值与轻量行为
- 资产路径、配置解析、纯函数工具

建议命名与标签：

- `gtest.engine.unit`
- `engine;unit;gtest`

当前最优先的工作不是追求大量覆盖率，而是建立稳定、快速、可重复的单元测试骨架。

### 2. 集成测试：GoogleTest + GMock

`depends/googletest-1.17.0` 已包含 GoogleMock，因此 GMock 是可用但尚未接入当前测试目标的能力。

适合后续覆盖：

- `AssetManager` 与 RHI / 文件加载边界
- runtime service 注入
- `FramePipeline` 初始化依赖
- 模块加载与 `IModule` 生命周期
- Lua/C++ runtime 桥接中的可替换边界

建议新增独立目标或标签，避免和纯 unit 混在一起：

- `gtest.engine.integration`
- `engine;integration;gtest;gmock`

GMock 的价值在于隔离系统交互，不建议用它替代简单纯逻辑单测。

### 3. 性能测试：Google Benchmark

这个方向合理，但**当前仓库没有发现项目主线可用的 Google Benchmark 依赖**。`depends/` 中有第三方库自己的 benchmark 目录或文档，但不等于本项目已经 vendored `google/benchmark`。

因此建议把 Google Benchmark 放到第三阶段：

1. 先建立 GTest 单元测试基线
2. 再接入 GMock 集成测试
3. 最后在用户确认新增第三方依赖后，引入 Google Benchmark

适合性能测试的关键路径：

- `FramePipeline` update/render 关键段的可测子路径
- ECS 大量实体遍历
- AssetManager 缓存命中/路径解析
- Lua 绑定高频调用边界
- Mesh / animation / particle 关键循环
- 资源导入与烹饪中的热点路径

建议标签：

- `engine;benchmark;performance`

Benchmark 不建议纳入默认 `build_fast_tests.bat`，应作为手动或专项 gate，避免拖慢默认门禁。

## 推荐落地顺序

### Phase A：GTest 全引擎单元测试基线

目标：先让整个引擎库有可持续扩展的单元测试目录和 target 结构。

建议目录：

- `tests/gtest/unit/core/`
- `tests/gtest/unit/base/`
- `tests/gtest/unit/ecs/`
- `tests/gtest/unit/assets/`
- `tests/gtest/unit/runtime/`
- `tests/gtest/unit/modules/`

建议输出：

- 一个或多个 `dse_gtest_*_unit_tests` target
- 每个 target 都有明确 CTest name 与标签
- 默认 `build_fast_tests.bat` 至少跑通基础 unit 集合

### Phase B：GMock 集成测试层

目标：把系统间交互从真实环境中剥离出来做可重复验证。

建议目录：

- `tests/gtest/integration/`

建议关注：

- runtime service 注入
- 资产系统与渲染设备边界
- 动态模块接口
- Lua runtime 上下文边界

### Phase C：Google Benchmark 性能专项

目标：只覆盖关键路径，不把 benchmark 当普通单测运行。

建议目录：

- `tests/benchmark/`

建议前置条件：

- 用户明确授权引入 Google Benchmark
- CMake 开关独立，例如 `DSE_BUILD_BENCHMARKS`
- 默认关闭，不进入普通构建和快速测试

## 补充建议

- **补测试文档**：当前 README 与部分规划文档仍引用旧测试结构，应在测试基线 phase 中同步修正。
- **补标签规范**：建议统一 `engine;unit;gtest`、`engine;integration;gtest;gmock`、`engine;benchmark;performance`。
- **补目录约束**：新增测试优先放在 `tests/gtest/` 下，不再恢复旧目录，除非明确重建多框架测试体系。
- **先测构建链**：每新增 target 都必须验证 `cmake --build ... --target <target>` 与对应 `ctest` 标签。
- **避免 benchmark 过早引入**：Google Benchmark 是合理目标，但需要额外第三方依赖授权，不应在当前阶段默认假设已存在。

## 结论

新的测试策略方向是合理的，但应修正为分阶段执行：

1. **立即优先**：GoogleTest 覆盖引擎核心单元测试基线
2. **随后推进**：GoogleTest + GMock 做系统交互集成测试
3. **专项引入**：Google Benchmark 做关键路径性能测试，需先确认依赖引入策略

当前最应该进入后续阶段开发的，是 **GTest 全引擎单元测试基线建设**。
