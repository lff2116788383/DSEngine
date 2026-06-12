# DSEngine Lua 绑定缺口分析

> 生成日期：2026-06-12
> 对比来源：`engine/scripting/lua/bindings/*.cpp`（实际注册） vs `docs/api/LUA_API.md`（现有文档）
> 审计工具：`tools/audit/lua_api_audit.py`（函数名提取/对比）、`tools/audit/gen_accessor_doc.py`（Codegen 访问器文档生成）
> 复核方式：在 `docs/api/` 下运行 `python ../../tools/audit/lua_api_audit.py`

---

## 一、概要结论

- **文档覆盖率：100%。** 实测注册 **876** 个 Lua 可见函数名，审计后 **0** 个未文档化。
  本次更新已把全部新增/遗漏的接口补进 `docs/api/LUA_API.md`。
- **本轮新增 24 个手写绑定**：手柄输入（4）+ 场景管理/过渡（11）+ UUID 读取/解析（3）+ FootIK（6）。详见 §五。
- **接口完整性：核心玩法链路完整。** 渲染、光照、物理 2D/3D、动画 2D/3D（FSM/层/IK/FootIK/Root Motion/骨骼挂点）、
  粒子、UI（含输入框/滑块/下拉/滚动/虚拟列表/视觉效果）、音频（BGM/SFX/总线/DSP/快照）、
  地形（splat/瓦片流式/植被/树木）、导航、序列化、本地化、网络/HTTP 均有 Lua 绑定。
- **剩余缺口：少量且非阻塞。** 见 §三，均为内部组件或无运行时系统的占位组件，不影响脚本开发。

| 类别 | 数值 |
|------|:----:|
| 注册 Lua 函数总数 | 876 |
| 其中 Codegen 字段访问器（13 组件 / 165 字段） | 330 |
| 手写绑定函数 | 546 |
| 顶层模块数（含条件编译 http/net/nav） | 19 |
| 审计后未文档化函数 | **0** |

---

## 二、本次补全的接口（之前已绑定但文档缺失）

> 这些函数早已在 C++ 侧注册，但旧版 `LUA_API.md` 未收录。本次已全部补入对应章节。

| 模块 | 章节 | 新增文档项 |
|------|------|-----------|
| ECS 核心 | §5.1 | `set_parent` / `add_parent` / `get_parent` / `clear_parent`；`add_script` / `set/get_script_path` / `set/get_script_enabled` |
| 地形扩展 | §5.8 | `add_foliage` 及风/刚度访问器、`add_tree`、`add_terrain_tile_manager`、`add_dynamic_obstacle`、`add_navmesh_auto_rebake` |
| 后处理 | §5.9 | `set_post_process_ssr`（屏幕空间反射） |
| LOD | §5.13 | `lod_set_min_screen_size` |
| 物理 3D | §5.22 | `rigidbody_3d_set/get_angular_velocity`、`rigidbody_3d_add_torque` |
| Gameplay3D | §5.23 | 大气散射、昼夜循环、体积云、天气、积雪覆盖（共 27 个函数） |
| 动画 3D | §5.25 | 骨骼挂点 `add/set/get/remove_bone_attachment` 系列；`MorphTargetComponent` 系列 |
| 音频 | §8 | 全局 BGM/SFX 播放、`crossfade_bgm`、主/BGM/SFX 音量、`set_source_bus`、混音快照 |
| UI | §10 | TTF 标签、文本输入、滑块、开关、进度条、下拉、滚动视图、虚拟滚动、盒布局、内容自适应、填充图、焦点导航、视觉效果（圆角/渐变/模糊）、事件传递、JSON/文件加载（共 50+ 函数） |
| 导航 | §12 | `get_nav_agent`、`get_nav_destination`、`nav_agent_has_path` |
| 组件字段访问器 | §18（全新） | 13 个组件、330 个 `get_*/set_*` 逐字段访问器（由 `binding_defs.json` 自动生成） |

---

## 三、仍未暴露给 Lua 的引擎能力（剩余缺口）

> 判定方式：扫描 `engine/**` 中全部 `*Component`（102 个），逐一核对是否存在对应 Lua 绑定
> （含经由 `dse_*` C ABI 转发的情况）。以下为确无任何 Lua 入口的组件。

### 3.1 内部 / 自动维护组件（无需暴露，**非缺口**）

| 组件 | 说明 |
|------|------|
| `BoundingBoxComponent` | 由渲染/裁剪系统自动计算，无需脚本写入 |
| `FragmentTagComponent` | 破碎系统运行期内部标记碎块用 |
| `UUIDComponent` | 实体稳定 ID，主要供序列化/编辑器使用 |
| `LuaScriptComponent` | Sol2 路径运行期组件；当前脚本挂载走 `ScriptComponent`（已绑定 `add_script`） |

### 3.2 FootIK3DComponent（已补绑定，本轮）

| 组件 | 状态 |
|------|------|
| `FootIK3DComponent` | **已补绑定（本轮）**。更正前述判断：运行时系统 **早已存在**——`modules/gameplay_3d/animation/foot_ik_system.cpp` 的 `FootIKSystem`（物理 Raycast + FABRIK + 骨盆下沉）已实现，并在 `gameplay_3d_module.cpp` 帧管线中调用。缺的仅是 Lua 绑定，已补齐（见 §5.25 FootIK 系统 / §五）。早期分析只扫描了 `engine/**`，遗漏 `modules/**`，故误判为“占位” |

### 3.3 便利接口（本轮已补 / 可选增强）

| 能力 | 现状 | 状态 |
|------|------|------|
| 实体 UUID 读取 | `ecs.get_uuid` / `set_uuid` / `resolve_uuid` | **已补（本轮）** |
| 子场景/预制体 | `ecs.load_sub_scene_async` / `unload_sub_scene` / `unload_all_sub_scenes` / 查询计数 | **已补（本轮）** |
| 材质实例参数读回 | `dssl.get_*` 已覆盖 DSSL 实例 | `MaterialInstanceComponent` 的非 DSSL 路径暂走 `set_mesh_material*`，按需扩展 |

---

## 三·补、子系统 / 服务层面的缺口（不止组件）

> 判定方式：枚举 `ServiceLocator` 注册的服务与各管理器公共 API，对比 Lua 绑定。
> 服务清单：`AssetManager`、`EventBus`、`JobSystem`、`NavMeshSystem`、`Physics2D/3DSystem`、
> `World`、`LocalizationManager`、`FontService`、`StreamingManager`、`SceneManager`。

### 3a.1 故意不暴露（设计如此，**非缺口**）

| 能力 | 原因 |
|------|------|
| `JobSystem`（多线程任务调度） | 引擎内部驱动并行；脚本不应直接操作线程，避免数据竞争 |
| `EventBus` 的 `Publish<T>/Subscribe<T>` | 基于 C++ 模板与静态事件类型，无法直接跨 Lua 边界。脚本侧事件改用**轮询队列**暴露（碰撞/触发 `physics_3d_get_*_events`、动画 `pop_*_event`、UI 事件传递） |
| GPU-driven 管线 / 批处理内部 | 只读统计已通过 `dse.metrics.*` 暴露（draw calls、实例数等），无需写接口 |
| 画质 / VSync / MSAA / 分辨率缩放 | 引擎**无运行时 API**（由配置/启动参数控制），故不属“被遗漏”——`get_quality` 是网络连接质量，非渲染 |

### 3a.2 能力已存在、Lua 未覆盖 —— 本轮已全部补齐

| 能力 | C++ 入口 | Lua 绑定（新增） | 状态 |
|------|----------|----------|------|
| **手柄输入 Gamepad** | `engine/input/input.h`：`GetGamepadAxis` / `IsGamepadConnected` / `Set/GetGamepadDeadZone`（支持 4 个手柄） | `app.get_gamepad_axis` / `is_gamepad_connected` / `set/get_gamepad_dead_zone` | 已补（仍只有“轴”，无手柄按键） |
| **场景管理 SceneManager** | `engine/scene/scene_manager.h`：`LoadSubSceneAsync`、`UnloadSubScene`、`UnloadAll`、`GetLoadedSubScenes`、`IsSubSceneLoaded`、`LoadedCount/PendingCount` | `ecs.load_sub_scene_async` / `unload_sub_scene` / `unload_all_sub_scenes` / `is_sub_scene_loaded` / `get_loaded_sub_scenes` / `get_sub_scene_count` / `get_pending_scene_count` / `get_active_scene` | 已补 |
| **场景切换过渡** | `SceneManager::TransitionTo(path, mode, fade)` + `GetTransitionState` / `GetFadeProgress` | `ecs.transition_to` / `get_transition_state` / `get_fade_progress` | 已补 |
| **UUID → 实体解析** | `SceneManager::ResolveReference(uuid)` + `UUIDComponent` | `ecs.get_uuid` / `set_uuid` / `resolve_uuid` | 已补 |

> 这 4 项（外加 §3.2 的 FootIK）即上一轮识别出的全部真实缺口，本轮已全部实现并文档化。

---

## 五、本轮新增绑定明细（场景/资源生命周期 + 稳定引用 + 手柄 + FootIK）

> 实现文件：`lua_binding_core.cpp`（手柄）、`lua_binding_ecs_core.cpp`（场景/UUID）、
> `dse_api_animation.cpp` + `dse_api.h` + `lua_binding_ecs_animation.cpp`（FootIK C ABI + 绑定）。
> 全部经 `debug-vulkan-jolt` 构建通过，`ctest -C Debug` 三套用例（unit/integration/smoke）全绿。

- **手柄输入（4）**：`app.get_gamepad_axis`、`app.is_gamepad_connected`、`app.set_gamepad_dead_zone`、`app.get_gamepad_dead_zone`
- **场景管理/过渡（11）**：`ecs.load_sub_scene_async`、`unload_sub_scene`、`unload_all_sub_scenes`、`is_sub_scene_loaded`、`get_loaded_sub_scenes`、`get_sub_scene_count`、`get_pending_scene_count`、`transition_to`、`get_transition_state`、`get_fade_progress`、`get_active_scene`（均经 `ServiceLocator::Get<scene::SceneManager>()`，路径按 data root 解析）
- **UUID（3）**：`ecs.get_uuid`、`set_uuid`、`resolve_uuid`（`resolve_uuid` 仅解析经 SceneManager 加载的子场景实体）
- **FootIK（6）**：`ecs.add_foot_ik_component`、`add_foot_ik_foot`、`set_foot_ik_foot_weight`、`set_foot_ik_foot_height`、`set_foot_ik_pelvis`、`set_foot_ik_enabled`（薄包装委托至新增 `dse_foot_ik_*` C ABI）

---

## 四、维护方式

1. **字段访问器（§18）**：唯一数据源是 `tools/codegen/binding_defs.json`，
   改动组件字段后重跑代码生成即可同步 C ABI / Lua / C#。文档用 `tools/audit/gen_accessor_doc.py` 再生成。
2. **覆盖率回归**：新增手写绑定后，在 `docs/api/` 运行 `python ../../tools/audit/lua_api_audit.py`，
   确认 “BOUND but NOT in doc” 计数为 0。
3. 审计脚本与中间产物位于 `tools/audit/`（不参与构建）。
