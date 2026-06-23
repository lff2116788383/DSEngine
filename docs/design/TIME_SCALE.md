# 全局 / 分层 Time-Scale 设计（慢动作 · 暂停 · hit-stop · 子弹时间）

> 状态：**已评审通过，实现中**。对应 `docs/roadmap/SCRIPTING_API_BACKLOG.md` 第 1 条。

## 1. 目标与语义

- **全局 time-scale**：一个全局缩放因子 `0 = 暂停`、`1 = 正常`、`0.5 = 半速`、`>1 = 快进`。
- **分层 / 逐实体 time-scale**：每个实体可挂 `TimeScaleComponent`，最终生效缩放 =
  `全局 time-scale × 实体 local_scale`（对标 Unreal 的 `WorldSettings.TimeDilation × AActor::CustomTimeDilation`）。
  典型用途：世界进入子弹时间（全局 0.3）时，让 Boss 保持正常（local ≈ 1/0.3）。
- **两条时间通道**：
  - **scaled_dt**（缩放）—— gameplay 逻辑、动画、粒子、Tween、脚本、**物理固定步长累加**。
  - **unscaled_dt**（真实）—— UI / 暂停菜单 / 输入 / 性能统计 / 渲染帧调度，保证 `scale=0` 时菜单仍可操作、FPS 显示正常。
- **音频** 默认不缩放（预留接口位，本期不实现 pitch 联动）。

## 2. 与主流引擎对照

| 能力 | Unity | Unreal | Godot | 本方案 |
|------|-------|--------|-------|--------|
| 全局缩放 | `Time.timeScale` | `WorldSettings.TimeDilation` | `Engine.time_scale` | `Time::set_time_scale` |
| scaled / unscaled dt | `deltaTime` / `unscaledDeltaTime` | dilated `DeltaSeconds` / real | `_process` / 实时 | scaled_dt / unscaled_dt（TimeContext） |
| 逐实体缩放 | ✗（需自己乘） | ✓ `CustomTimeDilation` | ✗ | ✓ `TimeScaleComponent` |
| `scale=0` 暂停物理 | ✓ | ✓ | ✓ | ✓（累加器不增长） |
| 物理逐刚体差速 | ✗ | ✗（近似） | ✗ | ✗（Option A，见 §6） |

结论：全局缩放与三家一致；逐实体缩放达到 Unreal 级别（Unity/Godot 内置都没有）。

## 3. 现状（代码核实）

dt 单一来源与分发链路（已逐处确认）：

```
EngineInstance::Tick()                          engine/runtime/engine_app.cpp:508
  Time::Update();  dt = Time::delta_time();      // 唯一帧计时来源（真实时间）
  if (dt > 0.1f) dt = 0.1f;                       // 首帧/卡顿钳制
  accumulator_ += dt;  clamp(kMaxAccumulator)     // 固定步长累加器（最多 10 步/帧）
  while (accumulator_ >= fixed_time_step_)
      pipeline_->FixedUpdate(fixed_time_step_);   // 物理等定步长
  pipeline_->Update(dt);                          // 变步长：gameplay/anim/particle/...
  pipeline_->Render();
  Input::Update();
```

- 变步长链路：`Update(dt)` → `RunFrameUpdate` → `RunRuntimeUpdateGraph(dt)` →
  `IBuiltinModules::UpdateGameplay2D/3D(world, dt)` 与外部模块 `IModule::OnUpdate(world, dt)`。
- 动画/粒子/Tween/IK/Animator **全部以形参 `delta_time` 接收**（`animation_system.cpp:10`、
  `particle_system.cpp:91`、`animator_system.cpp:734`、`particle3d_system.cpp:105` 等），
  **无一直接读 `Time::delta_time()`**。→ 在源头缩放即可全覆盖。
- 模块代码里**没有**绝对时钟（`TimeSinceStartup`/`chrono`）做 gameplay 计时 → 不存在"暂停后冷却仍走"的隐藏 bug。
- 物理**无**渲染插值 alpha → `scale=0` 不会产生插值抖动。
- 直接读 `Time::delta_time()` 的点（渲染上下文 `frame_pipeline.cpp:1302/2749`、stats `:1561`、
  `metrics.get_fps/get_frame_time_ms`、编辑器 idle/metrics）属**真实时间**语义 → 保持读真实 dt 即正确，无需改。

## 4. 架构（零债：TimeContext 显式线程化）

不改变 `Time::delta_time()` 的既有语义（仍返回**真实** dt），从而渲染/统计零改动、零 footgun。
缩放通过**显式传参**贯穿 update graph（类似 Unreal 把 `DeltaSeconds` 显式传入 Tick），而非依赖全局隐式读取。

### 4.1 `Time`（engine/base/time.{h,cpp}）
```cpp
static float delta_time();          // 不变：真实帧 dt
static float scaled_delta_time();   // 新增：delta_time() * time_scale()
static void  set_time_scale(float); // 新增：clamp 到 [0, kMaxTimeScale]
static float time_scale();          // 新增
```

### 4.2 `TimeContext`（engine/base/time_context.h，纯数据）
```cpp
struct TimeContext {
    float scaled_dt   = 0.0f;  // 全局缩放后 dt（gameplay/anim/particle/tween）
    float unscaled_dt = 0.0f;  // 真实 dt（UI/暂停菜单/统计）
    float time_scale  = 1.0f;  // 全局缩放
};
```

### 4.3 `TimeScaleComponent`（engine/ecs/time_scale_component.h）
```cpp
struct TimeScaleComponent { float scale = 1.0f; };  // 逐实体局部缩放，默认 1
```
逐实体生效 dt 由 helper 统一计算：`entity_dt = ctx.scaled_dt * local_scale(默认1)`。

### 4.4 线程化（仅改引擎内部链路，**不动公共插件接口 `IModule::OnUpdate(World&,float)`**）
- `EngineInstance::Tick()`：先对**真实** dt 做首帧/卡顿钳制，再乘 scale 得 `scaled`；
  `accumulator_ += scaled`（`scale=0` → 不累加 → 物理冻结）；`pipeline_->Update(TimeContext)`。
- `FramePipeline::Update(const TimeContext&)` → `RunFrameUpdate` → `RunRuntimeUpdateGraph(const TimeContext&)`。
- `IBuiltinModules::UpdateGameplay2D/3D(World&, const TimeContext&)`、`UpdateFallback3D`（引擎内部接口）。
- 外部插件 `IModule::OnUpdate(world, ctx.scaled_dt)`：保持 `float` 签名，传入 scaled（文档约定）。
- gameplay 2D/3D 模块：UI 子系统传 `ctx.unscaled_dt`（暂停菜单不冻结），其余 gameplay 子系统传 `ctx.scaled_dt`。

### 4.5 逐实体缩放落点
以下逐实体系统在各自实体循环内将 dt 乘以该实体的 `TimeScaleComponent.scale`：
2D `AnimationSystem`、2D `ParticleSystem`、3D `AnimatorSystem`、3D `Particle3DSystem`、Lua/C++ 脚本 `OnUpdate`。
未挂组件的实体 local_scale=1（行为不变）。

### 4.6 Lua API（`dse.app` 全局；`dse.ecs` 逐实体）
```lua
dse.app.set_time_scale(s)        -- 0=暂停, 1=正常, 0.5=半速；clamp s>=0
dse.app.get_time_scale()         -> number
dse.ecs.set_time_scale(e, s)     -- 设/加 TimeScaleComponent（实体局部缩放）
dse.ecs.get_time_scale(e)        -> number（无组件返回 1.0）
```
hit-stop 由脚本侧实现：`set_time_scale(0.05)`，X 毫秒后 `set_time_scale(1)`（用 unscaled 计时）。

## 5. 改动文件清单
| 文件 | 改动 |
|------|------|
| `engine/base/time.{h,cpp}` | `time_scale_` + `set/get_time_scale` + `scaled_delta_time` |
| `engine/base/time_context.h`（新） | `TimeContext` 结构 |
| `engine/ecs/time_scale_component.h`（新） | `TimeScaleComponent` |
| `engine/runtime/engine_app.cpp` | Tick：scaled 喂累加器+Update，构建 TimeContext |
| `engine/runtime/frame_pipeline.{h,cpp}` | `Update(const TimeContext&)` |
| `engine/runtime/runtime_frame_ops.*` / `runtime_update_graph.*` | 透传 TimeContext |
| `engine/runtime/i_builtin_modules.h` + `modules/runtime_bridge/builtin_modules_impl.*` | UpdateGameplay2D/3D 收 TimeContext |
| `modules/gameplay_2d/gameplay_2d_module.*` / `gameplay_3d/gameplay_3d_module.*` | UI 用 unscaled，gameplay 用 scaled |
| `modules/gameplay_2d/animation,particle` / `gameplay_3d/animation,particles` | 逐实体乘 local_scale |
| `engine/scripting/lua/...` | 脚本 OnUpdate 逐实体缩放；`dse.app`/`dse.ecs` 绑定 |
| `docs/api/LUA_API.md` | API 文档 |
| `tests/gtest/...` | 单测 |

## 6. 限制与决策（已定）
- **Option A**：自由动态刚体（碎片/抛射/布娃娃）只跟随**全局** time-scale，不做逐实体差速
  （单一共享物理世界一帧只步进一次，无法 per-body 时间膨胀；这是 Unity/Unreal/Godot 的通例）。
  角色/敌人/Boss 的移动通常是运动学/角色控制器/动画/脚本驱动，走 gameplay 通道，**逐实体缩放有效**。
- **快进上限**：`scale>1` 时固定步数受 `kMaxAccumulator`（10 步/帧）限制，极高 scale 下物理推进被封顶。
- **确定性/网络**：缩放影响固定步数，单机无碍；联机回放本期不涉及。

## 7. 验证计划
- gtest：`scale=0` 实体/物理冻结、UI 计时仍前进；`scale=0.5` 位移≈半；逐实体 `local=2`、全局 `0.5` → 该实体≈正常；无组件实体行为不变。
- llvmpipe 无头跑编辑器：脚本切 `set_time_scale(0/0.25/1)`，观察实体、粒子、暂停菜单。
- 回归：`LuaEcs*` 与 gameplay 单测全绿。
