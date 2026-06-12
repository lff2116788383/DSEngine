# DSEngine Lua 绑定缺口分析

> 生成日期：2026-06-12
> 对比来源：`engine/scripting/lua/bindings/*.cpp`（实际注册） vs `docs/api/LUA_API.md`（现有文档）
> 审计工具：`tools/audit/lua_api_audit.py`（函数名提取/对比）、`tools/audit/gen_accessor_doc.py`（Codegen 访问器文档生成）
> 复核方式：在 `docs/api/` 下运行 `python ../../tools/audit/lua_api_audit.py`

---

## 一、概要结论

- **文档覆盖率：100%。** 实测注册 **862** 个 Lua 可见函数名，审计后 **0** 个未文档化。
  本次更新已把全部新增/遗漏的接口补进 `docs/api/LUA_API.md`。
- **接口完整性：核心玩法链路完整。** 渲染、光照、物理 2D/3D、动画 2D/3D（FSM/层/IK/Root Motion/骨骼挂点）、
  粒子、UI（含输入框/滑块/下拉/滚动/虚拟列表/视觉效果）、音频（BGM/SFX/总线/DSP/快照）、
  地形（splat/瓦片流式/植被/树木）、导航、序列化、本地化、网络/HTTP 均有 Lua 绑定。
- **剩余缺口：少量且非阻塞。** 见 §三，均为内部组件或无运行时系统的占位组件，不影响脚本开发。

| 类别 | 数值 |
|------|:----:|
| 注册 Lua 函数总数 | 862 |
| 其中 Codegen 字段访问器（13 组件 / 165 字段） | 330 |
| 手写绑定函数 | 532 |
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

### 3.2 已定义但缺运行时系统的占位组件（**建议先补系统再绑定**）

| 组件 | 状态 |
|------|------|
| `FootIK3DComponent` | 定义于 `engine/ecs/components_3d_animation.h`，但无对应 System 实现、无绑定。属脚手架占位，暂不暴露；如需脚部 IK 应先实现系统。通用 IK 已由 `IKChain3D`（§5.25）覆盖 |

### 3.3 可考虑补充的便利接口（**可选增强，非必需**）

| 能力 | 现状 | 建议 |
|------|------|------|
| 实体 UUID 读取 | 无 `ecs.get_uuid(e)` | 若脚本需稳定跨存档/网络引用实体，可补只读访问器 |
| 子场景/预制体 | 已有 `ecs.load_sub_scene`，但无卸载/查询 | 视需求补 `unload_sub_scene` / 状态查询 |
| 材质实例参数读回 | `dssl.get_*` 已覆盖 DSSL 实例 | `MaterialInstanceComponent` 的非 DSSL 路径暂走 `set_mesh_material*`，按需扩展 |

---

## 四、维护方式

1. **字段访问器（§18）**：唯一数据源是 `tools/codegen/binding_defs.json`，
   改动组件字段后重跑代码生成即可同步 C ABI / Lua / C#。文档用 `tools/audit/gen_accessor_doc.py` 再生成。
2. **覆盖率回归**：新增手写绑定后，在 `docs/api/` 运行 `python ../../tools/audit/lua_api_audit.py`，
   确认 “BOUND but NOT in doc” 计数为 0。
3. 审计脚本与中间产物位于 `tools/audit/`（不参与构建）。
