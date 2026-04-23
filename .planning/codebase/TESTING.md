# DSEngine Codebase Testing

## 总体测试策略

DSEngine 当前测试体系已经不是“零散单测集合”，而是一个围绕 **CTest + 分层门禁 + Windows 本地最小回归** 构建的验证系统。

测试主线重点覆盖：

- engine 核心单测
- Lua / C++ runtime 生命周期
- 2D 子系统 smoke / snapshot
- 3D MVP 最小门禁
- 资产导入与烹饪链
- 编辑器与场景 IO 的关键桥接路径

## 测试入口

### CTest 是统一入口

文档与 CMake 配置都明确了：测试统一通过 `ctest` 运行，而不是只在 IDE 中手点执行。

关键文件：

- `tests/CMakeLists.txt`
- `tests/engine/CMakeLists.txt`
- `tests/spine/CMakeLists.txt`
- `doc-archive/TESTING_CTEST_GUIDE.md`

### 顶层开关

顶层 `CMakeLists.txt` 中测试由：

- `DSE_BUILD_ENGINE_TESTS`

控制。

典型构建方式见：

- `README.md`
- `doc-archive/TESTING_CTEST_GUIDE.md`
- `build_fast_tests.bat`

## 测试目录结构

### `tests/engine/`

覆盖范围很广，包括：

- `base/`
- `core/`
- `ecs/`
- `scene/`
- `editor/`
- `runtime/`
- `assets/`
- `audio/`
- `render/`
- `profiler/`
- `platform/`
- `scripting/`

### `tests/modules/`

聚焦 gameplay 模块：

- `gameplay_2d/*`
- `gameplay_3d/*`

### `tests/spine/`

Spine 单独成组，说明它在当前仓库中是高价值专项能力，而不是普通边角功能。

## 组织方式

### 单个测试二进制，多组 CTest gate

`tests/engine/CMakeLists.txt` 里，`dse_engine_unit_tests` 被注册成多个测试入口：

- `engine.unit`
- `engine.2d.ui`
- `engine.2d.tilemap`
- `engine.2d.particle`
- `engine.2d.physics2d`
- `engine.2d.spine`
- `engine.2d.localization`
- `engine.2d.animation`
- `engine.2d.camera`
- `engine.3d.unit`
- `engine.3d.scene_mvp`
- `engine.3d.smoke`
- `engine.3d.runtime_mvp_smoke`

这说明测试组织不是“一个测试文件对应一个可执行”，而是：

- 用统一测试 target 聚合实现
- 再用标签与运行参数切出多个 gate

### 多测试目标并存

除主单测目标外，还存在：

- `dse_lua_runtime_tests`
- `dse_lua_runtime_core_single_test`
- `dse_lua_runtime_smoke_single_test_v2`
- 资源注入与 spine 等专项相关入口

这说明 Lua runtime 相关测试被单独拉出来做更细的专项门禁。

## 当前高价值门禁

根据 `doc-archive/TESTING_CTEST_GUIDE.md`，当前最常用的 2D 主线门禁包括：

- `engine.unit`
- `engine.lua_runtime`
- `engine.lua_runtime.smoke`
- `engine.resource_injection`
- `engine.cpp_runtime`
- `engine.2d.ui`
- `engine.2d.physics2d`
- `engine.2d.particle`
- `engine.2d.localization`
- `engine.spine`
- `engine.spine.smoke`

这些门禁覆盖：

- 双宿主 runtime
- 资源注入
- 高频 2D 子系统
- Spine 资源消费链

## 3D 相关门禁

当前 3D 不是默认稳定主线，但已经具备明确最小 gate：

- `engine.3d.unit`
- `engine.3d.scene_mvp`
- `engine.3d.runtime_mvp_smoke`
- `engine.asset_compiler`

其中：

- `engine.3d.scene_mvp`：守住最小 3D 场景可加载性
- `engine.3d.runtime_mvp_smoke`：守住 runtime 能实际切入 3D MVP 场景
- `engine.asset_compiler`：守住 glTF/GLB 到运行时资产的离线链路

## 测试命名风格

从大量 `TEST_CASE(...)` 可以看出，当前偏向：

- Given / When / Then
- 明确场景、动作、预期结果

示例风格：

- `Given_EmptySpinePaths_When_Update_Then_ComponentStateRemainsUnchanged`
- `Given_DefaultTerrainComponent_When_Created_Then_IsDirtyIsTrue`

这使测试日志具有较好的语义可读性。

## Smoke / Snapshot / Regression 倾向

仓库测试不只验证纯逻辑，还包含：

- **smoke**：最小可执行链路
- **snapshot**：结果快照/表现验证
- **static regression**：静态回归保护
- **runtime smoke**：真实宿主启动路径验证
- **scene MVP gate**：场景级门禁

说明测试目标是“防回归 + 守主链”，不是单纯追求覆盖率数字。

## Windows 特定经验

`doc-archive/TESTING_CTEST_GUIDE.md` 中记录了非常具体的 Windows + Lua 测试经验：

- 某些断言表达式写法会导致 `session.run()` 不返回的假性超时
- 推荐先算到局部变量，再做 `REQUIRE(...)`

这是一条非常实际的项目级测试约定，新增 Lua runtime 单测时应优先继承。

## 推荐命令

### 最小常用门禁

- `build_fast_tests.bat`
- `ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.unit|engine.lua_runtime|engine.cpp_runtime|engine.resource_injection|engine.spine|engine.2d.ui|engine.2d.physics2d|engine.2d.particle|engine.2d.localization"`

### engine 标签全集

- `ctest --test-dir build_vs2022 -C Debug --output-on-failure -L engine`

### 3D MVP 回归矩阵

- `ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.3d.unit|engine.3d.scene_mvp|engine.3d.runtime_mvp_smoke"`

### 资产导入门禁

- `ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.asset_compiler"`

## 当前测试哲学

从文档与目录结构综合看，当前测试哲学是：

- 优先守住 2D 稳定主线
- 用最小但真实可执行的 gate 防回归
- 3D 以 MVP 收口为主，不假装“已经全面稳定”
- 把 runtime、场景、资源链路纳入门禁，而不只是测纯算法

## 结论

DSEngine 的 testing 体系已经具备较成熟的基础：

- 有统一入口
- 有层级标签
- 有最小门禁矩阵
- 有运行时与资源链路验证
- 有工程经验文档沉淀

后续新增能力时，最佳做法通常是：

1. 先判断属于哪个 gate 层级
2. 补充对应测试文件
3. 更新 `CTest` 注册或标签
4. 必要时同步更新 `doc-archive/TESTING_CTEST_GUIDE.md`

也就是说，测试在这个仓库里是**主线工程约束**，不是上线前临时补的附属物。
