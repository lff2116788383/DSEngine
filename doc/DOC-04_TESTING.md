# DOC-04 测试与回归

本文档描述当前仓库已经落地的测试组织方式、构建入口与推荐回归策略。

## 1. 测试开关

顶层 CMake 使用：

- `DSE_BUILD_ENGINE_TESTS=ON`

默认值为 `OFF`。

如果不显式开启，则不会生成测试目标。

## 2. 当前测试目标

当前 `tests/` 目录下已组织以下目标：

- `dse_engine_unit_tests`
- `dse_lua_runtime_tests`
- `dse_spine_tests`

其中：

- `dse_engine_unit_tests`：引擎主体单测与多模块测试集合
- `dse_lua_runtime_tests`：Lua 运行时专项回归
- `dse_spine_tests`：Spine 专项测试

## 3. 当前 CTest 入口

当前已注册的核心 CTest 用例包括：

- `engine.unit`
- `engine.lua_runtime`
- `engine.cpp_runtime`
- `engine.resource_injection`
- `engine.spine`
- `engine.2d.ui`
- `engine.2d.tilemap`
- `engine.2d.particle`
- `engine.2d.physics2d`
- `engine.2d.localization`
- `engine.2d.animation`
- `engine.2d.camera`
- `engine.3d.unit`
- `engine.3d.smoke`

说明：

- `engine.unit` 是主体总入口
- 2D 模块已拆出若干 smoke 风格标签，便于快速回归
- 3D 已存在部分测试入口，但不代表 3D 已成为默认稳定主线

## 4. 覆盖范围概览

### 4.1 基础层

- time
- tween
- input
- event_bus
- job_system
- ecs world

### 4.2 场景与运行时

- transform_system
- prefab
- octree
- runtime_access
- cpp_business_runtime
- lua_runtime

### 4.3 2D 模块

- camera
- animation
- localization
- font_manager
- particle
- tilemap
- ui
- physics2d

### 4.4 Profiler

- cpu_profiler
- memory_profiler
- render_profiler

### 4.5 3D 模块

已有部分测试文件与入口，但当前 3D 更应理解为“已接入、待收口”的能力方向，测试更多体现为接入验证与基础行为校验。

## 5. 当前最有价值的回归点

### 5.1 Lua Runtime

Lua 运行时测试覆盖了当前最关键的稳定性问题，例如：

- 启动脚本缺失
- 启动脚本异常
- 无全局 `Update`
- 多实体脚本隔离
- 关闭后 Lua 内存归零
- 资源系统去全局态后的 Lua API context 显式注入边界

### 5.2 UI

UI 测试已覆盖：

- 点击回调
- 首帧布局命中
- 遮罩拦截
- 文本同步
- 本地化联动

### 5.3 Physics2D

Physics2D 测试已覆盖：

- 动态刚体重力更新
- 静态刚体稳定性
- 删除实体后的刚体清理
- collision / trigger 回调
- raycast 命中

## 6. 本地使用方式

### 6.1 构建测试

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_ENGINE_TESTS=ON
cmake --build build_vs2022 --config Debug --target dse_engine_unit_tests dse_lua_runtime_tests dse_spine_tests
```

### 6.2 跑全部 engine 标签

```bash
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L engine
```

### 6.3 跑 Lua Runtime 专项

```bash
ctest --test-dir build_vs2022 -C Debug -R engine.lua_runtime -V
```

### 6.4 跑资源注入专项门禁

```bash
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.cpp_runtime|engine.resource_injection|engine.spine"
```

### 6.5 跑 Spine 专项

```bash
ctest --test-dir build_vs2022 -C Debug -R engine.spine -V
```

### 6.6 跑关键 2D smoke

```bash
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.2d.(ui|tilemap|particle|physics2d|localization|animation|camera)"
```

## 7. 与 `build_all.bat` 的关系

可以通过：

```bat
build_all.bat --with-tests
```

快速接入测试构建。

但需要明确：

- `build_all.bat` 是本地脚本入口
- CTest 才是测试组织层的标准入口

## 8. 当前不足

当前测试体系已经成型，但还存在以下不足：

- 还未看到稳定的正式 CI 主线配置进入仓库核心流程
- 缺少持续化的 Debug / Release 双配置门禁
- 缺少性能基线自动比对
- 3D 测试存在，但与默认稳定主线关系仍较弱

## 9. 当前建议门禁

建议至少把以下内容作为日常回归基线：

- `engine.unit`
- `engine.lua_runtime`
- `engine.cpp_runtime`
- `engine.resource_injection`
- `engine.spine`
- `engine.2d.ui`
- `engine.2d.physics2d`
- `engine.2d.particle`
- `engine.2d.localization`

在 3D MVP 阶段，再逐步增加：

- `engine.3d.unit`
- `engine.3d.smoke`
- 最小 3D 场景回归

## 10. 结论

当前 DSEngine 的测试体系已经超出“只有少量 demo 测试”的阶段，具备了：

- CTest 统一入口
- 单元测试 + 专项测试 + 2D smoke 标签
- Lua Runtime / UI / Physics2D 的问题回归能力
- 一部分 3D 接入验证能力

后续重点不是继续堆文档，而是把当前测试入口进一步做成稳定门禁。
