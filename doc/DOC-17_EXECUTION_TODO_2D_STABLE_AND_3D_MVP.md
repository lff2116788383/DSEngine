# DOC-17 执行 TODO：2D Stable 与 3D MVP

本文档是 [`doc/DOC-16_2D_STABLE_AND_3D_MVP_4_TO_6_WEEK_PLAN.md`](doc/DOC-16_2D_STABLE_AND_3D_MVP_4_TO_6_WEEK_PLAN.md) 的执行拆分版。

原则：

- 只列当前阶段真要做的事
- 不把“以后可能有用”的东西混进来
- 每项尽量可直接验证

---

## A. 当前已完成（本轮已落地）

- [x] 修复 Windows 下 Lua single tests / Catch runner 假性挂起问题
- [x] 将 Lua runtime 配置接口改为 `const&` 形式，避免临时对象相关问题
- [x] 将易触发问题的 Lua 单测断言改为“先求值再断言”模式
- [x] 文档补充 Windows + Catch + Lua 单测挂起规避说明
- [x] 验证以下门禁通过：
  - [x] `engine.unit`
  - [x] `engine.lua_runtime`
  - [x] `engine.lua_runtime.smoke`
  - [x] `engine.resource_injection`
  - [x] `engine.cpp_runtime`
  - [x] `engine.2d.ui`
  - [x] `engine.2d.particle`
  - [x] `engine.2d.physics2d`
  - [x] `engine.2d.localization`
  - [x] `engine.spine`
  - [x] `engine.spine.smoke`

---

## B. 本周优先 TODO（第 1 周）

### B1. 测试与门禁

- [x] 在文档中固定“当前常用 2D 主线门禁清单”（已写入 `doc/TESTING_CTEST_GUIDE.md`）
- [x] 检查 `tests/engine/runtime_smoke_snapshot_test.cpp` 是否也存在同类断言形态隐患（已确认当前文件已采用 snapshot/先求值后断言模式，无需额外修正）
- [x] 检查是否还有其他 single-test target 使用不一致的 runner 结构（当前 `tests/engine/` 下仅 `main.cpp` 与 `lua_test_main.cpp` 两套 runner；Lua single-test targets 已统一使用 `lua_test_main.cpp`）
- [x] 形成一条可直接复制执行的本地常用回归命令集合（已整理到本文档附录）

### B2. 2D 编辑器高频闭环排查

- [x] 检查 Scene save/load 主链是否存在明显脏状态问题（已确认当前至少存在 `tests/engine/scene/scene_flow_test.cpp` 与 `tests/engine/scene/editor_scene_io_bridge_test.cpp` 两条基础回归链；当前问题不是“完全无覆盖”，而是后续需要进一步提炼为 2D Stable 高频闭环检查项）
- [x] 检查 Play/Edit 切换是否污染运行态（代码审计结论：当前 `apps/editor_cpp/src/editor_toolbar.cpp` 的 Play/Stop 主要依赖 `entt::registry` 级别 backup/restore；尚未看到对 runtime 子系统、脚本状态、音频/定时器/其他单例状态的完整隔离或回滚，因此当前结构存在运行态污染风险，后续应作为 2D Stable 的高频闭环问题继续收口）
- [x] 检查 Inspector 对高频 2D 组件的修改反馈是否一致（代码审计结论：当前 2D Inspector 整体采用“直接改组件字段”的即时写入模式，但存在明显不一致：部分组件会显式标记 `dirty`（如 `TransformComponent` / `UILabelComponent`），部分高频组件则缺少统一 dirty/刷新语义（如 `SpriteRendererComponent` / `RigidBody2DComponent` / `ParticleEmitterComponent`）；另外 `Sprite Renderer` 区域的 UI 表述与底层字段 `shader_variant` 的语义存在混用，后续应作为高频编辑反馈一致性问题继续收口）
- [ ] 只挑选 2~3 个最影响闭环的问题进入修复

### B2.1 当前选中的高优先修复问题池

基于本轮对 2D 编辑器高频闭环的代码审计，当前建议优先进入修复的问题如下：

1. **Play/Edit 运行态隔离不完整（P0）**
   - 当前 Play/Stop 主要依赖 `entt::registry` 级别 backup/restore
   - 尚未看到对 Lua/C++ runtime、音频、时间/定时器、其他全局/单例子系统状态的完整隔离/回滚
   - 应优先定义最小 play-mode state isolation 边界
   - 当前第一版状态面清单已明确：
     - 仅靠 `registry` backup 可恢复：entity/component 数据本身，以及 Inspector 直接修改到 registry 上的编辑态字段
     - 不能仅靠 `registry` backup 覆盖：Lua runtime 全局状态、C++ business runtime/bridge 状态、`AudioSystem` 单例状态、`Time` 全局状态、Profiler 状态与历史、editor toolbar/selection/gizmo/language preview 等 editor-side globals
   - 当前已开始落地第一刀：`apps/editor_cpp/src/editor_toolbar.cpp` 已将按钮内联逻辑抽成 `EnterPlayMode(...)` / `ExitPlayMode(...)`，并接入第一版最小 reset 占位（Lua runtime clear、audio stop、`Time::Init()`）；编辑器目标 `dse_editor_cpp` 已成功编译通过

2. **Inspector 高频 2D 组件缺少统一 dirty / refresh 语义（P1）**
   - 当前部分组件显式打 `dirty`，部分组件直接写字段但无统一刷新语义
   - 建议优先统一 `Transform / SpriteRenderer / UILabel / RigidBody2D / ParticleEmitter` 这组高频组件的变更后行为

3. **Sprite Renderer Inspector 的 UI 表述与底层字段语义不一致（P1）**
   - 当前 UI 操作看起来像在编辑 texture/sprite
   - 但底层拖拽实际写入的是 `shader_variant`
   - 应明确修正为“字段语义对齐”或“UI 文案对齐”中的一种

#### P0 当前最小手工验证计划（Play/Edit 第一刀）

建议在继续补第二刀前，先做以下 5 项最小手工验证：

1. **基本进入/退出 Play 不崩**
   - 启动 `bin/dsengine-editor.exe`
   - 打开当前可编辑的 2D scene
   - 点击 Play，等待 2~3 秒，再点击 Stop
   - 期望：不崩溃、不挂死、可顺利回到 Edit

2. **registry 恢复是否正常**
   - Edit 下记录一个实体的关键字段（如 Transform / UILabel / Sprite）
   - Enter Play → 等待 → Stop
   - 期望：回到 Edit 后恢复为进入 Play 前的值

3. **Lua runtime 残留是否明显减少**
   - 对会走 Lua runtime 的 scene 连续执行 2~3 次 Play / Stop
   - 期望：不出现明显的脚本残留/重复初始化/越来越异常的状态

4. **音频是否残留到 Edit**
   - 用可触发 BGM/SFX 的 scene 进入 Play
   - 播放声音后点击 Stop
   - 期望：回到 Edit 后运行态音频停止

5. **选择态与 Inspector 是否回到合理状态**
   - Edit 下选中实体 → Play → Stop
   - 期望：`selected_entity` 清空行为稳定，Inspector 不出现明显错误状态

### B3. 文档与口径

- [ ] 在版本/路线文档中明确：当前稳定主线是 2D，3D 仍为 MVP 收口中
- [ ] 整理一版“当前 2D 已达标能力清单”
- [ ] 整理一版“当前 3D 仅承诺 MVP 的能力边界”

---

## C. 第 2 周 TODO（2D 对外可展示）

- [ ] 选定或整理一个最小 2D showcase 场景
- [ ] 明确该场景覆盖的能力项（UI / localization / particle / physics2d / spine / animation / tilemap）
- [ ] 用该场景反向检查编辑器高频路径问题
- [ ] 为该 showcase 场景补一条最小运行/回归说明

---

## D. 第 3~4 周 TODO（3D MVP 收口）

- [ ] 固定最小 3D MVP 场景定义
- [ ] 明确 3D MVP 完成定义
- [ ] 整理当前 3D 能力为三类：必须保留 / 暂不纳入 / 暂缓
- [ ] 核实 `engine.3d.scene_mvp` 与 `engine.3d.runtime_mvp_smoke` 的当前有效性
- [ ] 检查最小 3D 场景 save/load 主链
- [ ] 检查关键 3D 组件 Inspector 主链（MeshRenderer / Camera3D / DirectionalLight / Skybox）

---

## E. 第 5~6 周 TODO（性能与对外表述）

- [ ] 建立最小 2D baseline（startup / scene load / draw calls / frame update）
- [ ] 建立最小 3D MVP baseline（draw calls / shadow cost / scene load / frame timing）
- [ ] 输出一版正式能力口径：稳定主线 / MVP / 不承诺项
- [ ] 在后续阶段三选一：2D 稳定版准备 / 3D MVP 深化 / 编辑器可用性强化

---

## F. 当前明确不做

- [ ] 不启动大而全资源数据库重构
- [ ] 不启动大而全 Prefab 系统
- [ ] 不扩展完整 Shader Graph / 可视化材质编辑器
- [ ] 不扩展高级后处理矩阵
- [ ] 不扩展大型 3D 地形 / 粒子生态
- [ ] 不以 AAA 渲染能力作为当前阶段目标

说明：

这些条目保留在此处，不是为了执行，而是为了防止后续排期再次失焦。

---

## G. 附录：本地常用回归命令集合

以下命令默认在仓库根目录执行，Windows / PowerShell 环境下可直接复制。

### G1. 构建 Lua / runtime 相关目标并跑最小回归

```powershell
cmake --build build_vs2022 --config Debug --target dse_lua_runtime_core_single_test dse_lua_runtime_smoke_single_test dse_lua_resource_injection_single_test dse_lua_runtime_tests
$env:DSE_NO_TEST_PAUSE='1'
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.lua_runtime|engine.lua_runtime.smoke|engine.resource_injection|engine.cpp_runtime"
```

### G2. 跑当前 2D 主线常用门禁

```powershell
$env:DSE_NO_TEST_PAUSE='1'
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.unit|engine.lua_runtime|engine.lua_runtime.smoke|engine.resource_injection|engine.cpp_runtime|engine.2d.ui|engine.2d.physics2d|engine.2d.particle|engine.2d.localization|engine.spine|engine.spine.smoke"
```

### G3. 单独跑外围 2D / spine 门禁

```powershell
$env:DSE_NO_TEST_PAUSE='1'
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.2d.ui|engine.2d.physics2d|engine.2d.particle|engine.2d.localization|engine.spine|engine.spine.smoke"
```

### G4. 单独跑 3D MVP 相关门禁

```powershell
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.3d.scene_mvp|engine.3d.runtime_mvp_smoke|engine.3d.smoke|engine.3d.unit"
```

### G5. 查看当前所有已注册测试

```powershell
ctest --test-dir build_vs2022 -N
```

### G6. 快速看本轮修改与提交状态

```powershell
git status --short
git log --oneline -n 10
```
