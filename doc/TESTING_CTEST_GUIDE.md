# DSEngine CTest 与引擎单测骨架

## 目标

本方案为仓库提供可长期扩展的测试结构，采用：

- CTest 作为统一测试编排入口
- Catch 单文件框架作为 C++ 断言实现
- `DSE_BUILD_ENGINE_TESTS` 作为测试构建开关（默认关闭）

## 当前落地内容

- 顶层 CMake 增加选项 `DSE_BUILD_ENGINE_TESTS`，默认 `OFF`
- 新增 `tests/` 测试子工程入口
- 新增 `tests/engine/` 引擎单测目标 `dse_engine_unit_tests`
- 新增 `tests/engine/` Lua 运行时专项目标 `dse_lua_runtime_tests`
- 新增 `tests/spine/` Spine 专项目标 `dse_spine_tests`
- 注册 CTest 用例 `engine.unit`，并标记标签 `engine;unit`
- 注册 CTest 用例 `engine.lua_runtime`，并标记标签 `engine;unit;lua_runtime`
- 注册 CTest 用例 `engine.spine`，并标记标签 `engine;unit;spine`
- 提供示例测试：`Tween::Evaluate` 与 `Tween::Lerp`
- 已补充 Lua 运行时回归：启动脚本缺失/异常、无全局 `Update`、脚本组件隔离更新、Shutdown 内存归零
- 已补充 2D UI 回归：修复并覆盖 `UISystem` 首帧/布局变更时，事件命中使用过期 `runtime_model` 的问题
- `build_all.bat` 接入测试开关：
  - `--with-tests`：开启并执行引擎单测
  - `--no-tests`：关闭引擎单测（默认）

## 目录结构

```text
tests/
  CMakeLists.txt
  engine/
    CMakeLists.txt
    main.cpp
    base/
      tween_test.cpp
```

## 本地使用

### 方式一：直接用 CMake + CTest

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_ENGINE_TESTS=ON
cmake --build build_vs2022 --config Debug --target dse_engine_unit_tests dse_lua_runtime_tests dse_spine_tests
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L engine

# 仅执行 Lua 运行时专项回归
ctest --test-dir build_vs2022 -C Debug -R engine.lua_runtime -V

# 仅执行 Spine 专项回归
ctest --test-dir build_vs2022 -C Debug -R engine.spine -V
```

### 方式二：通过 build_all.bat

```bat
build_all.bat --with-tests
```

默认行为仍为关闭测试构建：

```bat
build_all.bat
```

## 扩展约定

新增测试时遵循以下约定，保证长期可维护：

1. 按被测模块分目录：`tests/engine/<module>/xxx_test.cpp`
2. 测试名称格式：`模块_行为_预期`
3. 每个测试文件聚焦一个模块，不跨多系统
4. 标签分层：
   - `engine`：引擎层
   - `unit`：纯单元测试
   - 后续可扩展 `integration`、`smoke`
5. 所有新增测试必须可由 `ctest` 直接执行

## 新增单测流程

1. 在 `tests/engine/` 下新增 `*_test.cpp`
2. 在 `tests/engine/CMakeLists.txt` 注册源文件
3. 本地执行：
   - 构建：`cmake --build ... --target dse_engine_unit_tests dse_lua_runtime_tests dse_spine_tests`
   - 运行：`ctest --test-dir ... -L engine`
   - Lua 专项：`ctest --test-dir ... -R engine.lua_runtime -V`
   - Spine 专项：`ctest --test-dir ... -R engine.spine -V`
4. 通过后再提交代码

## 后续建议

- 在 CI 中增加 `ctest --output-on-failure -L engine` 与 `ctest -R engine.lua_runtime` 作为合并门禁
- 逐步覆盖纯函数模块、资源解析、ECS 关键行为
- 引入 `integration` 标签，将脚本绑定与运行时组合行为纳入自动回归
- 2D 闭环建议优先继续补齐：UI 遮罩/层级事件、Tilemap 大地图更新、Physics2D 触发器/碰撞回调、Spine 资源异常与动画切换
