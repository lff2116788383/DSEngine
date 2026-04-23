# Phase 02 Plan: 引擎稳定性与 3D 崩溃清零

**Date:** 2026-04-23  
**Status:** Planned  
**Source:** `/gsd-discuss-phase` discussion + Phase 2 source scan  
**Goal:** 先以真实源码与 CTest 入口为依据，跑通 runtime / Lua / 3D / asset 最小矩阵，定位并修复阻塞 Lua 驱动 3D 原型推进的 bug、崩溃、超时、目标缺失与假阳性。

## 1. Discussion Decisions

- **Phase:** Phase 2 — 引擎稳定性与 3D 崩溃清零
- **Preferred entry:** 先做源码事实扫描，确认 runtime / Lua / 3D / asset 真实入口
- **Output target:** 生成可执行 `PLAN.md`
- **Environment assumption:** 当前本机具备可运行图形上下文，因此 `engine.lua_runtime.smoke` 与 3D runtime smoke 应纳入本 Phase 验证闭环

## 2. Requirements Covered

| Requirement | Phase 2 Meaning |
|-------------|-----------------|
| `3D-01` | 最小 3D MVP 场景必须能被 scene gate 与 runtime smoke 稳定加载 |
| `3D-02` | 3D 改动必须受 `engine.3d.unit`、`engine.3d.scene_mvp`、`engine.3d.runtime_mvp_smoke` 保护 |
| `3D-04` | 当前目标是 Lua 3D 原型闭环真实性，不是继续空泛扩面 |
| `QA-01` | 所有修复必须关联真实验证命令，不允许未测即交付 |

## 3. Verified Source Entry Points

### 3.1 Runtime / 双宿主入口

- `engine/runtime/engine_app.cpp`
  - `EngineInstance::Init()` 负责 GLFW / OpenGL 上下文、`FramePipeline` 装配、启动场景回归检查。
  - `EngineInstance::Tick()` 驱动 fixed update、update、render 与 input 更新。
- `engine/runtime/frame_pipeline.h`
  - `FramePipeline` 仍是逐帧调度、runtime context、2D/3D 系统与渲染资源的核心中枢。
- `tests/engine/cpp_runtime_startup_scene_test.cpp`
  - 覆盖 `DSE_STARTUP_SCENE`、`3d_mvp_minimal.scene.json` 与 reference demo scene 的 C++ runtime 启动行为。

### 3.2 Lua / 绑定入口

- `engine/scripting/lua/lua_runtime.cpp`
- `engine/scripting/lua/bindings/lua_binding_ecs.cpp`
  - 已存在 3D 相关 ECS 绑定，如 transform、Camera3D、灯光、Terrain、Steering、ParticleSystem3D 等。
- `tests/engine/lua_runtime_core_single_test.cpp`
- `tests/engine/lua_resource_injection_test.cpp`
- `tests/engine/lua_runtime_smoke_single_test.cpp`
  - 已覆盖 `demo15_8` / `demo15_9` reference scene Lua smoke 标签：`[vse_demo_15_8_15_9]`。

### 3.3 3D runtime / scene 入口

- `modules/gameplay_3d/`
  - 包含 rendering、animation、ai、particles 等当前 3D MVP 子系统。
- `modules/gameplay_3d/gameplay_3d_module.cpp`
  - 存在动态模块导出入口 `CreateModule()` / `DestroyModule()`。
- `assets/scenes/3d_mvp_minimal.scene.json`
- `assets/scenes/reference_demo_15_8.scene.json`
- `assets/scenes/reference_demo_15_9.scene.json`
- `tests/engine/scene/scene_flow_test.cpp`
  - 覆盖 `3d_mvp_minimal.scene.json` 的 scene 反序列化与最小 3D 组件存在性。

### 3.4 Asset chain 入口

- `engine/assets/compiler/importer.cpp`
- `engine/assets/compiler/importer.h`
- `tests/engine/assets/importer_cooker_test.cpp`
  - 覆盖 glTF importer、cookers、PBR / skeleton / animation 字段与当前明确未接入能力边界。

### 3.5 CTest 注册入口

- `tests/engine/CMakeLists.txt`
  - 已注册：
    - `engine.unit`
    - `engine.cpp_runtime`
    - `engine.lua_runtime`
    - `engine.resource_injection`
    - `engine.lua_runtime.smoke`
    - `engine.3d.unit`
    - `engine.3d.scene_mvp`
    - `engine.3d.runtime_mvp_smoke`
    - `engine.asset_compiler`

## 4. Execution Plan

### Step 1 — Build the Phase 2 target set

**Purpose:** 先确认测试目标真实可构建，避免把 CTest 失败误判为功能失败。

```bat
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF -DDSE_BUILD_ENGINE_TESTS=ON
cmake --build build_vs2022 --config Debug --target dse_engine_unit_tests dse_lua_runtime_tests dse_lua_runtime_core_single_test dse_lua_runtime_smoke_single_test_v2 dse_lua_resource_injection_single_test DSEngine_example_cpp -- /m:1 /nologo
```

**Pass condition:** 所有目标成功产出；若 target 缺失，先修 CMake / target 注册，不进入功能修复。

### Step 2 — Run runtime / Lua baseline matrix

**Purpose:** 先确认 Phase 1 留下的 runtime 与 Lua smoke 最终闭环，尤其补跑此前因图形上下文暂缓的 smoke。

```bat
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.cpp_runtime|engine.lua_runtime|engine.resource_injection|engine.unit|engine.lua_runtime.smoke"
```

**Pass condition:**

- `engine.cpp_runtime` 通过
- `engine.lua_runtime` 通过
- `engine.resource_injection` 通过
- `engine.unit` 通过
- `engine.lua_runtime.smoke` 在本机图形环境下通过

**Failure handling:**

- 若 `engine.lua_runtime.smoke` 失败，优先检查 `demo15_8.lua` / `demo15_9.lua`、Lua ECS 绑定、reference scene 资源路径与 `AssetManager` 行为。
- 若 `engine.cpp_runtime` 失败，优先检查 `DSE_STARTUP_SCENE`、`EngineInstance::Init()`、启动场景回归检查与窗口上下文生命周期。

### Step 3 — Run 3D MVP matrix

**Purpose:** 确认 3D unit、scene gate 与 runtime smoke 三层信号一致。

```bat
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.3d.unit|engine.3d.scene_mvp|engine.3d.runtime_mvp_smoke"
```

**Pass condition:**

- `engine.3d.unit` 通过，说明 3D 子系统逻辑门禁仍可用
- `engine.3d.scene_mvp` 通过，说明 `3d_mvp_minimal.scene.json` 的 scene 层结构仍可加载
- `engine.3d.runtime_mvp_smoke` 通过，说明 C++ runtime 能在真实窗口 / OpenGL 上下文中加载最小 3D MVP 场景

**Failure handling:**

- 若 `engine.3d.scene_mvp` 通过但 `engine.3d.runtime_mvp_smoke` 失败，优先定位 runtime 装配、资源路径、OpenGL 上下文或渲染路径问题。
- 若 `engine.3d.unit` 失败，先按具体 Catch2 标签分离 rendering / animation / ai / particles 子系统。

### Step 4 — Run asset compiler gate

**Purpose:** 确认当前资产链主路径与已声明边界没有退化。

```bat
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.asset_compiler"
```

**Pass condition:**

- importer/cooker 单测通过
- 当前 glTF 支持字段与未支持字段的静态边界仍与源码一致

**Failure handling:**

- 若 importer/cooker 失败，优先修复数据解析、输出文件、临时路径或当前支持矩阵漂移。
- 不在本 Phase 扩展新 asset 能力，除非它直接阻塞 3D MVP smoke。

### Step 5 — Triage and fix failures by blocker class

按以下顺序处理失败，避免先修低价值边角：

1. **目标缺失 / 构建失败**：CMake target、源文件注册、链接依赖
2. **崩溃 / 进程异常退出**：生命周期、空指针、GLFW/OpenGL 上下文、资源管理
3. **超时 / 窗口阻塞**：`DSE_MAX_FRAMES`、`DSE_NO_TEST_PAUSE`、测试环境变量
4. **断言失败**：scene 内容、Lua 绑定、组件默认值、asset 路径
5. **假阳性 / 口径漂移**：测试标签、PASS_REGULAR_EXPRESSION、文档与注册名不一致

每个修复必须满足：

- 修改前定位真实源码与测试失败输出
- 优先最小修改，不做重构扩面
- 修改后重跑对应失败 gate
- 如改变测试入口或能力边界，同步更新 `doc-archive/TESTING_CTEST_GUIDE.md` 或相关专题文档

## 5. Expected Deliverables

- Phase 2 最小测试矩阵执行结果记录
- 至少一轮失败清单分类：通过 / 失败 / 环境问题 / 未执行
- 对所有阻塞 Lua / runtime / 3D 原型推进的问题完成最小修复，或明确记录为人工阻塞
- 若修复涉及代码，补充或更新对应回归测试
- 更新 Phase 2 `SUMMARY.md`，并在完成后同步 `.planning/STATE.md`

## 6. Out of Scope

- 不新增大规模 3D gameplay 能力
- 不把 Lua 3D API 从“已绑定部分能力”直接宣称为“完整支持”
- 不重写 runtime / asset / scripting 主链
- 不前置编辑器体验打磨
- 不引入新的第三方依赖

## 7. Verification Checklist

- [ ] 构建 Phase 2 目标集合
- [ ] 执行 runtime / Lua baseline matrix
- [ ] 执行 3D MVP matrix
- [ ] 执行 asset compiler gate
- [ ] 分类记录所有失败信号
- [ ] 修复阻塞项并重跑对应 gate
- [ ] 记录真实能力边界
- [ ] 更新 `SUMMARY.md` 与必要文档

## 8. Next Command

```text
/gsd-execute-phase
```

---

*Phase: 02-stability-3d-crash-zero*  
*Plan written: 2026-04-23 after source-fact scan and Phase 2 discussion*
