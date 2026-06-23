# DSEngine Lua API 参考文档

> 严格对齐 `engine/scripting/lua/bindings/` 源码
> 更新日期：2026-06-12
> 绑定文件：`engine/scripting/lua/bindings/` 下约 28 个手写 C++ 源文件 + 13 个 `*.gen.cpp`，
> 涵盖 16 个顶层模块（其中 `http` / `net` 为条件编译）。
> 实测共注册 **862** 个 Lua 可见函数（含 §18 的 330 个 Codegen 字段访问器）。
> 说明：ECS 高层封装函数见 §5；逐字段底层 getter/setter 见 §18（由 `tools/codegen/binding_defs.json` 自动派生）。
> 完整覆盖率与差距分析见 `docs/api/API_GAP_ANALYSIS.md`。

---

## 概览

Lua API 挂载在以下全局表下：

```lua
dse
├── get_memory_usage_kb()        -- 全局工具
├── app.*                        -- 输入 / 窗口 / 时间 / 屏幕
├── assets.*                     -- 资源加载
├── metrics.*                    -- 性能统计
├── ecs.*                        -- ECS 实体 / 组件 / 场景
│   ├── (core)                   -- 实体创建/场景加载
│   ├── transform.*              -- Transform 组件
│   ├── camera.*                 -- 摄像机
│   ├── sprite.*                 -- 2D 精灵
│   ├── mesh_renderer.*          -- 3D 网格渲染
│   ├── light.*                  -- 光照（平行光/点光/聚光灯/天空光）
│   ├── skybox.*                 -- 天空盒
│   ├── terrain.*                -- 地形（含 splat、LOD）
│   ├── post_process.*           -- 后处理（Bloom/SSAO/FXAA/AutoExposure/Fog/...）
│   ├── decal.*                  -- 贴花
│   ├── water.*                  -- 水面
│   ├── steering.*               -- AI 转向
│   ├── lod.*                    -- LOD 系统
│   ├── grass.*                  -- 草地
│   ├── hair.*                   -- 毛发（TressFX 风格）
│   ├── gi_probe.*               -- GI 探针（DDGI）
│   ├── physics_2d.*             -- 2D 物理
│   ├── physics_3d.*             -- 3D 物理（含关节/碰撞层/事件）
│   ├── gameplay3d.*             -- 3D 游戏能力（布料/流体/软体/绳索/破碎/布娃娃/车辆/浮力）
│   ├── animation_2d.*           -- 2D 帧动画
│   ├── animation_3d.*           -- 3D 骨骼动画（含层系统 + IK）
│   ├── particle.*               -- 粒子系统
│   └── gameplay_tuning.*        -- 游戏调参
├── localization.*               -- 本地化系统（i18n）
├── origin.*                     -- 浮动原点系统（大世界坐标）
├── audio.*                      -- 音频系统（含混音总线 + DSP）
├── spine.*                      -- Spine 2D 骨骼动画
├── ui.*                         -- UI 系统
├── font.*                       -- 字体服务（加载 TTF / CJK / 测量）
├── serialize.*                  -- 自描述二进制序列化（编解码任意 Lua 值/表）
├── http.*                       -- 异步 HTTP(S) 客户端（条件编译 DSE_ENABLE_HTTP）
├── net.*                        -- 游戏 UDP 传输 GNS（条件编译 DSE_ENABLE_NET）
dssl.*                           -- DSSL 材质系统（独立全局表）
nav.*                            -- NavMesh 导航系统（独立全局表，条件编译 DSE_ENABLE_NAVMESH）
streaming.*                      -- 资源流式加载系统（独立全局表）
```

> `dse.http`（HTTPS REST，如 DeepSeek 等 AI API）与 `dse.net`（游戏专用 UDP 可靠/非可靠传输）
> 定位互补：前者连 REST 服务，后者做客户端↔服务端/状态同步，**不能**用 `dse.net` 连 HTTPS。
> 二者均默认 **OFF**，需分别用 `-DDSE_ENABLE_HTTP=ON` / `-DDSE_ENABLE_NET=ON` 构建才出现。

### 通用约定

- **entity** — 整数类型的实体 ID，由 `ecs.create_entity()` 返回
- **可选参数** — 用 `[param=default]` 表示，省略时使用默认值
- **vec3 返回值** — 返回 3 个独立的 number：`x, y, z = func(...)`
- **bool 返回值** — Lua 中 `true`/`false`
- **条件编译** — 标注 `[PhysX]` 的函数需 `DSE_ENABLE_PHYSX`，标注 `[NavMesh]` 的需 `DSE_ENABLE_NAVMESH`，
  整个 `dse.http` 模块需 `DSE_ENABLE_HTTP`，整个 `dse.net` 模块需 `DSE_ENABLE_NET`（均默认 OFF）
- **二进制安全字符串** — `dse.net.send` / `dse.serialize` 收发的 `string` 可含 `\0`，按字节长度处理

---

## 目录

1. [全局函数](#1-全局函数)
2. [dse.app — 应用 / 输入](#2-dseapp--应用--输入)
3. [dse.assets — 资源加载](#3-dseassets--资源加载)
4. [dse.metrics — 性能统计](#4-dsemetrics--性能统计)
5. [dse.ecs — 实体组件系统](#5-dseecs--实体组件系统)
6. [dse.localization — 本地化](#6-dselocalization--本地化)
7. [dse.origin — 浮动原点](#7-dseorigin--浮动原点)
8. [dse.audio — 音频系统](#8-dseaudio--音频系统)
9. [dse.spine — Spine 动画](#9-dsespine--spine-动画)
10. [dse.ui — UI 系统](#10-dseui--ui-系统)
11. [dssl — DSSL 材质系统](#11-dssl--dssl-材质系统)
12. [nav — NavMesh 导航系统](#12-nav--navmesh-导航系统)
13. [streaming — 资源流式加载](#13-streaming--资源流式加载)
14. [dse.font — 字体服务](#14-dsefont--字体服务)
15. [dse.serialize — 二进制序列化](#15-dseserialize--二进制序列化)
16. [dse.http — 异步 HTTP(S) 客户端](#16-dsehttp--异步-https-客户端dse_enable_http)
17. [dse.net — 游戏网络传输 (GNS)](#17-dsenet--游戏网络传输-gnsdse_enable_net)

---

## 1. 全局函数

| 函数 | 返回值 | 说明 |
|------|--------|------|
| `dse.get_memory_usage_kb()` | `number` | Lua 虚拟机当前内存使用量（KB） |

---

## 2. dse.app — 应用 / 输入

### 系统

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `app.set_data_root(path)` | `string` | — | 设置资源数据根目录（`DSE_DATA_ROOT` 环境变量优先） |
| `app.set_window_title(title)` | `string` | — | 设置窗口标题 |
| `app.time_since_startup()` | — | `number` | 自启动以来的秒数 |
| `app.get_screen_width()` | — | `int` | 屏幕宽度（像素） |
| `app.get_screen_height()` | — | `int` | 屏幕高度（像素） |
| `app.quit()` | — | — | 退出应用 |
| `app.set_target_fps(fps)` | `number` | — | 设置目标帧率（0 = 不限制） |

### 鼠标

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `app.get_mouse_x()` | — | `number` | 鼠标 X 坐标 |
| `app.get_mouse_y()` | — | `number` | 鼠标 Y 坐标 |
| `app.get_mouse_left()` | — | `bool` | 鼠标左键是否按住 |
| `app.get_mouse_left_down()` | — | `bool` | 鼠标左键当前帧是否按下 |
| `app.get_mouse_left_up()` | — | `bool` | 鼠标左键当前帧是否松开 |
| `app.get_mouse_right()` | — | `bool` | 鼠标右键是否按住 |
| `app.get_mouse_right_down()` | — | `bool` | 鼠标右键当前帧是否按下 |
| `app.get_mouse_right_up()` | — | `bool` | 鼠标右键当前帧是否松开 |
| `app.get_mouse_left_double_click()` | — | `bool` | 当前帧是否触发了左键双击 |
| `app.get_mouse_left_long_press([duration])` | `[number=0.5]` | `bool` | 左键是否长按超过指定时间（秒） |
| `app.get_mouse_middle()` | — | `bool` | 鼠标中键是否按住 |
| `app.get_mouse_middle_down()` | — | `bool` | 鼠标中键当前帧是否按下 |
| `app.get_mouse_scroll_dx()` | — | `number` | 滚轮 X 轴增量（GLFW 仅报告纵向，始终返回 0） |
| `app.get_mouse_scroll_dy()` | — | `number` | 滚轮 Y 轴增量 |
| `app.get_mouse_swipe_dx()` | — | `number` | 当前帧滑动/拖拽 X 轴增量（像素） |
| `app.get_mouse_swipe_dy()` | — | `number` | 当前帧滑动/拖拽 Y 轴增量（像素） |

### 键盘

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `app.get_key(key_code)` | `int` | `bool` | 指定按键是否按住 |
| `app.get_key_down(key_code)` | `int` | `bool` | 指定按键当前帧是否按下 |
| `app.get_key_up(key_code)` | `int` | `bool` | 指定按键当前帧是否松开 |

> **key_code** 使用 GLFW 键码整数值（如 `W=87`, `A=65`, `S=83`, `D=68`, `Space=32`, `Escape=256`）

### 手柄（Gamepad）

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `app.get_gamepad_axis([gamepad_id=0], axis)` | `[int]`, `int` | `number` | 读取手柄摇杆轴值（已应用死区，范围约 -1~1）。省略 `gamepad_id` 时默认 0 号手柄 |
| `app.is_gamepad_connected([gamepad_id=0])` | `[int]` | `bool` | 指定手柄是否已连接 |
| `app.set_gamepad_dead_zone(dead_zone)` | `number` | — | 设置全局摇杆死区（|轴值| < 死区时归零，默认 0.15） |
| `app.get_gamepad_dead_zone()` | — | `number` | 获取当前死区阈值 |

> 最多支持 4 个手柄（`gamepad_id` 0~3），每个手柄 6 个轴（`axis` 0~5）。当前仅暴露摇杆“轴”，无手柄按键。`get_gamepad_axis` 返回的值已自动过滤死区。

### 设备

| 函数 | 返回值 | 说明 |
|------|--------|------|
| `app.get_device_shake()` | `bool` | 设备是否处于摇晃状态 |

---

## 3. dse.assets — 资源加载

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `assets.load_texture(path)` | `string` | `int` | 加载纹理，返回句柄（0 表示失败） |

---

## 4. dse.metrics — 性能统计

| 函数 | 返回值 | 说明 |
|------|--------|------|
| `metrics.get_draw_calls()` | `int` | 当前帧 draw call 数 |
| `metrics.get_max_batch_sprites()` | `int` | 批次最大精灵数 |
| `metrics.get_sprite_count()` | `int` | 当前精灵总数 |
| `metrics.get_fps()` | `number` | 当前帧率（1/dt） |
| `metrics.get_frame_time_ms()` | `number` | 当前帧时间（毫秒） |
| `metrics.get_gpu_driven_active()` | `bool` | GPU Driven 渲染路径是否已激活 |
| `metrics.get_gpu_indirect_draw_count()` | `int` | 当前帧 GPU Indirect Draw 数量 |
| `metrics.get_gpu_total_instances()` | `int` | 当前帧 GPU Driven 总实例数 |

---

## 5. dse.ecs — 实体组件系统

### 5.1 核心

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.create_entity()` | — | `entity` | 创建空实体 |
| `ecs.destroy_entity(e)` | `entity` | — | 销毁实体及其全部组件 |
| `ecs.load_scene(path)` | `string` | `bool, string` | 加载场景文件，返回 (成功?, 错误信息) |
| `ecs.load_sub_scene(path)` | `string` | `bool, int/string` | 加载子场景，成功返回 (true, entity_count)，失败返回 (false, error) |
| `ecs.find_entities_by_mesh_path(path)` | `string` | `table` | 查找所有使用指定 mesh 路径的实体 |
| `ecs.set_parent(e, parent)` | entity, entity | — | 设置父实体（建立层级） |
| `ecs.add_parent(e, parent)` | entity, entity | — | 设置父实体（`set_parent` 别名） |
| `ecs.get_parent(e)` | entity | `entity` / nil | 获取父实体（无父则 nil） |
| `ecs.clear_parent(e)` | entity | — | 解除父子关系 |
| `ecs.add_script(e, path)` | entity, string | — | 挂载 Lua 脚本组件并设置脚本路径 |
| `ecs.set_script_path(e, path)` | entity, string | — | 设置脚本路径 |
| `ecs.get_script_path(e)` | entity | `string` / nil | 获取脚本路径 |
| `ecs.set_script_enabled(e, enabled)` | entity, bool | — | 启用/禁用脚本 |
| `ecs.get_script_enabled(e)` | entity | `bool` | 查询脚本是否启用 |

**示例：**
```lua
local ok, err = dse.ecs.load_scene("data/scenes/main.dscene")
if not ok then print("Load failed: " .. err) end
```

#### 场景管理 / 过渡（SceneManager）

子场景以 **data root 相对路径** 传入，内部会拼接成完整路径（与 `load_sub_scene` 行为一致）；查询接口返回的也是拼接后的完整路径。需要引擎注册了 `SceneManager` 服务才生效。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.load_sub_scene_async(path)` | `string` | `bool` | 异步加载子场景（有 JobSystem 时后台读取，否则回退同步）。返回是否已派发 |
| `ecs.unload_sub_scene(path)` | `string` | — | 卸载指定子场景及其实体 |
| `ecs.unload_all_sub_scenes()` | — | — | 卸载全部已加载子场景 |
| `ecs.is_sub_scene_loaded(path)` | `string` | `bool` | 查询子场景是否已加载 |
| `ecs.get_loaded_sub_scenes()` | — | `table` | 返回已加载子场景完整路径列表 |
| `ecs.get_sub_scene_count()` | — | `int` | 已加载子场景数量 |
| `ecs.get_pending_scene_count()` | — | `int` | 正在异步加载的子场景数量 |
| `ecs.transition_to(path, [mode="fade"], [fade_duration=0.5])` | `string`, `[string/int]`, `[number]` | — | 触发场景过渡。`mode`：`"instant"`/`"additive"`/`"fade"`（或 `0`/`1`/`2`） |
| `ecs.get_transition_state()` | — | `string` | 过渡状态：`"idle"`/`"fading_out"`/`"loading"`/`"fading_in"` |
| `ecs.get_fade_progress()` | — | `number` | 淡入淡出进度 [0,1] |
| `ecs.get_active_scene()` | — | `string` | 当前激活场景路径 |

#### UUID（跨场景稳定引用）

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.get_uuid(e)` | entity | `string` / nil | 读取实体的 UUID（16 位十六进制字符串）；无 `UUIDComponent` 或为 0 时返回 nil |
| `ecs.set_uuid(e, [uuid_str])` | entity, `[string]` | `string` | 设置/添加 `UUIDComponent`；省略 `uuid_str` 时自动生成。返回最终 UUID 十六进制字符串 |
| `ecs.resolve_uuid(uuid)` | `string`/`int` | `entity` / nil | 通过 UUID 解析实体（仅能解析经 SceneManager 加载的子场景实体）。接受十六进制字符串或整数 |

#### 通用实体查询（按组件名）

按组件名字符串查询/遍历实体，适合经济、人口、AI 等批量模拟逻辑。`component_name` 见下方支持列表；传入未知名会抛出 Lua 错误。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.find_entities_with(component_name)` | `string` | `table` | 返回持有该组件的全部实体（数组表，可能为空） |
| `ecs.count_entities_with(component_name)` | `string` | `int` | 持有该组件的实体数量 |
| `ecs.has_component(e, component_name)` | entity, `string` | `bool` | 实体是否持有该组件 |
| `ecs.get_queryable_components()` | — | `table` | 返回全部支持查询的组件名 |

> **支持的 `component_name`（共 60+）**：
> `transform`、`parent`、`script`、`uuid`、`gameplay_tuning`；
> `sprite_renderer`、`spine_renderer`、`material_instance`、`camera`、`camera_follow`、`rigidbody_2d`、`box_collider_2d`、`circle_collider_2d`；
> `mesh_renderer`、`lod_group`、`camera_3d`、`free_camera_controller`、`post_process`、`decal`、`directional_light`、`point_light`、`spot_light`、`sky_light`、`skybox`、`water`、`grass`、`hair`、`light_probe`、`reflection_probe`、`gi_probe_volume`、`morph_target`、`sub_scene`；
> `terrain`、`terrain_tile_manager`、`tree`、`foliage`；
> `rigidbody_3d`、`box_collider_3d`、`sphere_collider_3d`、`capsule_collider_3d`、`mesh_collider_3d`、`joint_3d`、`character_controller_3d`、`terrain_heightmap`、`ragdoll`、`soft_body`、`vehicle`、`rope`、`buoyancy`、`cloth`、`fluid_emitter`；
> `animator_3d`、`anim_layer`、`ik_chain_3d`、`foot_ik`、`bone_attachment`；
> `particle_system_3d`、`atmosphere`、`volumetric_cloud`、`day_night_cycle`、`weather`、`snow_cover`、`fracture`；
> `dynamic_obstacle`、`navmesh_auto_rebake`。
> 运行时可调用 `ecs.get_queryable_components()` 获取权威列表。

#### 场景 / 预制体保存（写盘）

补齐此前只读的加载侧（`load_scene` / `load_sub_scene`），支持把运行期世界写回磁盘，是存档/读档的基础。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.save_scene(path)` | `string` | `bool, string` | 将当前 World 完整序列化到 `path`（路径按字面使用，不做 data root 拼接）。返回 (成功?, 错误信息) |
| `ecs.save_prefab(e, path)` | entity, `string` | `bool` | 将单个实体保存为预制体文件 |
| `ecs.instantiate_prefab(path, [x, y, z])` | `string`, `[float...]` | `entity` / nil | 实例化预制体到世界；给定坐标时覆盖其位置，否则用预制体内置 Transform |

> 存档/读档示例：
> ```lua
> dse.ecs.save_scene("save/slot1.dscene")          -- 存档
> -- ...
> dse.ecs.load_scene("save/slot1.dscene")          -- 读档
> ```

#### 包围盒（BoundingBox，只读 AABB 查询）

`BoundingBoxComponent` 由渲染/剔除系统从 mesh 顶点计算并维护，脚本只读。用于自定义剔除、放置吸附、范围查询等。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.get_world_aabb(e)` | entity | `min_x,min_y,min_z, max_x,max_y,max_z`（6 个 number）/ nil | 世界空间 AABB：模型空间包围盒经实体 `local_to_world` 变换得到（算法与引擎剔除一致）。无 `BoundingBoxComponent` 时返回 nil；无 `TransformComponent` 时按单位变换返回 |
| `ecs.get_local_aabb(e)` | entity | `min_x,min_y,min_z, max_x,max_y,max_z`（6 个 number）/ nil | 模型空间 AABB：`BoundingBoxComponent` 原始值，不施加 Transform。无组件时返回 nil |

> **时序语义**：该组件由渲染/剔除系统在每帧更新，脚本读到的是**上一帧**的结果。刚创建/刚改 mesh 的实体可能尚未生成包围盒（返回 nil）。
>
> ```lua
> local minx,miny,minz, maxx,maxy,maxz = dse.ecs.get_world_aabb(e)
> if minx then
>     local cx = (minx + maxx) * 0.5  -- 世界空间中心
> end
> ```

### 5.2 Transform

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.add_transform(e, x, y, [z], [sx], [sy], [sz])` | `entity, float...` | — | 添加 Transform 组件 |
| `ecs.get_transform_position(e)` | `entity` | `x, y, z` | 获取世界坐标 |
| `ecs.set_transform_position(e, x, y, z)` | `entity, float, float, float` | — | 设置世界坐标（自动标记 dirty） |
| `ecs.set_transform_rotation(e, x, y, z)` | `entity, float, float, float` | — | 设置欧拉角旋转（度数） |
| `ecs.get_transform_rotation(e)` | `entity` | `x, y, z` | 获取欧拉角旋转（度数） |

**默认值：** `z=0, sx=1, sy=1, sz=1`

**示例：**
```lua
local e = dse.ecs.create_entity()
dse.ecs.add_transform(e, 0, 5, 0)
dse.ecs.set_transform_rotation(e, 0, 45, 0)
local x, y, z = dse.ecs.get_transform_position(e)
local rx, ry, rz = dse.ecs.get_transform_rotation(e)
```

### 5.3 Camera

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_camera(e, [ortho_size], [priority])` | entity, float, int | 10, 0 | 添加 2D 正交摄像机 |
| `ecs.add_camera_3d(e, [fov], [priority], [near], [far])` | entity, float, int, float, float | 60, 0, 0.1, 1000 | 添加 3D 透视摄像机 |
| `ecs.set_camera_priority(e, priority)` | entity, int | — | 设置摄像机优先级 |
| `ecs.set_camera_enabled(e, enabled)` | entity, bool | — | 启用/禁用摄像机 |
| `ecs.set_camera_follow(e, target, [damp], [dz_x], [dz_y], [off_x], [off_y])` | entity... | 0.12, 0, 0, 0, 0 | 设置摄像机跟随目标 |
| `ecs.add_free_camera_controller(e, [speed], [sensitivity])` | entity, float, float | 5, 0.1 | 添加自由视角控制器（WASD+鼠标） |

**示例：**
```lua
local cam = dse.ecs.create_entity()
dse.ecs.add_transform(cam, 0, 10, 20)
dse.ecs.add_camera_3d(cam, 60, 0, 0.1, 500)
dse.ecs.add_free_camera_controller(cam, 8.0, 0.15)
```

### 5.4 Sprite

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_sprite(e, [r], [g], [b], [a], [order], [tex])` | entity, float... | 1,1,1,1, 0, 0 | 添加精灵渲染器 |
| `ecs.set_sprite_uv_scroll(e, sx, sy)` | entity, float, float | — | 设置 UV 滚动速度 |
| `ecs.set_sprite_uv_offset(e, ox, oy)` | entity, float, float | — | 设置 UV 偏移 |

### 5.5 MeshRenderer

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_mesh_renderer(e, [r], [g], [b], [a], [vertices], [indices])` | entity, float..., table, table | 添加网格渲染器（可选内联顶点/索引） |
| `ecs.set_mesh_path(e, path)` | entity, string | 设置 mesh 文件路径（.obj/.fbx） |
| `ecs.set_mesh_material(e, ...)` | entity, string 或 float... | 加载 .dmat 文件 **或** 直接设置 PBR 标量参数 |
| `ecs.set_mesh_shader_variant(e, variant)` | entity, string | 设置着色器变体名 |
| `ecs.set_mesh_depth_state(e, depth_test, [depth_write])` | entity, bool, [bool] | 设置深度测试/写入状态 |
| `ecs.set_mesh_material_scalar(e, name, value)` | entity, string, float | 按名称设置材质标量参数 |
| `ecs.set_mesh_texture(e, slot, path)` | entity, string, string | 加载并绑定贴图到指定槽位 |
| `ecs.set_mesh_uvs(e, uv_table)` | entity, table | 设置 UV 坐标数组 |
| `ecs.set_mesh_normals(e, normal_table)` | entity, table | 设置法线数组 |
| `ecs.set_mesh_tangents(e, tangent_table)` | entity, table | 设置切线数组 |
| `ecs.set_mesh_emissive(e, r, g, b)` | entity, float, float, float | 设置自发光颜色 |
| `ecs.set_mesh_advanced_material(e, ...)` | entity, float... | 高级材质设置（Toon/Watercolor 参数） |

**材质标量参数名：** `"metallic"`, `"roughness"`, `"ao"`, `"normal_strength"`, `"material_alpha_cutoff"`

**贴图槽位名：** `"albedo"`/`"base_color"`/`"diffuse"`, `"normal"`/`"normal_map"`, `"metallic_roughness"`/`"mr"`, `"emissive"`/`"emission"`, `"occlusion"`/`"ao"`

**`set_mesh_texture` 返回值：** `success, handle, width, height`

**示例：**
```lua
local e = dse.ecs.create_entity()
dse.ecs.add_transform(e, 0, 0, 0)
dse.ecs.add_mesh_renderer(e)
dse.ecs.set_mesh_path(e, "data/models/cube.obj")
dse.ecs.set_mesh_material(e, "data/materials/brick.dmat")
dse.ecs.set_mesh_texture(e, "albedo", "data/textures/brick_albedo.png")
dse.ecs.set_mesh_material_scalar(e, "roughness", 0.8)
```

### 5.6 Light

#### 平行光

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_directional_light_3d(e, [dx], [dy], [dz], [r], [g], [b], [intensity], [ambient], [shadow])` | entity, float... | -0.4,-1,-0.3, 1,1,1, 1, 0.2, 0.35 | 添加平行光 |
| `ecs.set_directional_light_3d(e, [enabled], ...)` | entity, bool, float... | — | 修改平行光参数 |
| `ecs.set_directional_light_shadow(e, [cast], [strength], [c0], [c1], [c2])` | entity, bool, float... | — | 设置阴影参数（级联分割） |

#### 点光源

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_point_light_3d(e, [r], [g], [b], [intensity], [radius])` | entity, float... | 1,1,1, 1, 10 | 添加点光源 |
| `ecs.set_point_light_3d(e, [r], [g], [b], [intensity], [radius])` | entity, float... | — | 修改点光源参数（nil 跳过） |
| `ecs.set_point_light_shadow(e, [cast_shadow])` | entity, bool | true | 设置点光阴影 |

#### 聚光灯

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_spot_light_3d(e, [dx], [dy], [dz], [r], [g], [b], [intensity], [radius], [inner], [outer])` | entity, float... | 0,-1,0, 1,1,1, 1, 20, 12.5, 17.5 | 添加聚光灯 |
| `ecs.set_spot_light_3d(e, [dx], [dy], [dz], [r], [g], [b], [intensity], [radius], [inner], [outer])` | entity, float... | — | 修改聚光灯参数（nil 跳过） |
| `ecs.set_spot_light_shadow(e, [cast_shadow])` | entity, bool | — | 设置聚光灯阴影 |

#### 天空光

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_sky_light(e, [up_r], [up_g], [up_b], [down_r], [down_g], [down_b], [intensity])` | entity, float... | 0.22,0.28,0.38, 0.04,0.05,0.08, 1 | 添加天空环境光 |
| `ecs.set_sky_light(e, ...)` | entity, float..., [enabled] | — | 修改天空光参数 |

**示例：**
```lua
local sun = dse.ecs.create_entity()
dse.ecs.add_transform(sun, 0, 0, 0)
dse.ecs.add_directional_light_3d(sun, -0.5, -1.0, -0.3, 1, 0.95, 0.9, 1.5, 0.15, 0.5)
dse.ecs.set_directional_light_shadow(sun, true, 0.4, 10, 30, 80)
```

### 5.7 Skybox

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_skybox(e, [cubemap_path])` | entity, [string=""] | 添加天空盒（cubemap 路径） |

### 5.8 Terrain

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_terrain(e, [heightmap], [width], [depth], [max_height])` | entity, string, float... | "", 100, 100, 20 | 添加地形 |
| `ecs.set_terrain_params(e, [res_x], [res_z], [max_lod], [lod_dist_factor], [use_dynamic_lod])` | entity, int, int, int, float, bool | — | 设置地形参数 |
| `ecs.set_terrain_height(e, x, z, height)` | entity, int, int, float | — | 设置单个高度点 |
| `ecs.load_terrain_heightmap(e, path)` | entity, string | — | 加载高度图 |
| `ecs.set_terrain_texture(e, path)` | entity, string | — | 设置地形纹理 |
| `ecs.get_terrain_lod(e)` | entity | `lod, res_x, res_z, max_lod, lod_factor` | 获取 LOD 状态 |
| `ecs.sample_terrain_height(e, wx, wz)` | entity, float, float | `number` | 采样地形高度 |
| `ecs.set_terrain_splat_texture(e, layer_idx, path)` | entity, int, string | — | 设置地形 splat 层纹理 |

**`load_terrain_heightmap` 返回值（成功时）：** `true, img_w, img_h, channels, res_x, res_z`
**`set_terrain_texture` 返回值（成功时）：** `true, handle, tex_w, tex_h`

**示例：**
```lua
local terrain = dse.ecs.create_entity()
dse.ecs.add_transform(terrain, 0, 0, 0)
dse.ecs.add_terrain(terrain, "data/heightmaps/terrain01.png", 200, 200, 30)
dse.ecs.set_terrain_params(terrain, 128, 128, 4, 50.0, true)
dse.ecs.set_terrain_texture(terrain, "data/textures/grass.png")
```

#### Terrain 扩展（植被 / 树木 / 动态障碍 / 瓦片流式 / NavMesh 自动重烘焙）

> 对应 `lua_binding_ecs_rendering_terrain.cpp`；各组件逐字段访问器见 §18。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.add_foliage(e)` | entity | — | 添加植被（风摆动）组件 |
| `ecs.set_foliage_enabled(e, enabled)` | entity, bool | — | 启用/禁用植被 |
| `ecs.get_foliage_enabled(e)` | entity | `bool` | 查询植被启用状态 |
| `ecs.set_foliage_wind_strength(e, v)` | entity, float | — | 设置风力强度 |
| `ecs.get_foliage_wind_strength(e)` | entity | `number` | 获取风力强度 |
| `ecs.set_foliage_stiffness(e, v)` | entity, float | — | 设置刚度 |
| `ecs.get_foliage_stiffness(e)` | entity | `number` | 获取刚度 |
| `ecs.add_tree(e, [asset_path])` | entity, [string=""] | — | 添加树木组件（可选 .tree 资源路径） |
| `ecs.add_terrain_tile_manager(e)` | entity | — | 添加地形瓦片流式管理组件 |
| `ecs.add_dynamic_obstacle(e, shape)` | entity, int | — | 添加动态障碍（用于 NavMesh 避障）。shape 取值见 §18 |
| `ecs.add_navmesh_auto_rebake(e)` | entity | — | 添加 NavMesh 自动重烘焙组件 |

### 5.9 PostProcess

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_post_process(e, bloom_enabled, [threshold], [intensity], [exposure])` | entity, bool, float... | 添加后处理组件 |
| `ecs.set_post_process_bloom(e, [enabled], [bloom], [threshold], [intensity], [exposure])` | entity, bool... | 设置 Bloom 参数 |
| `ecs.set_post_process_color(e, [color_grading], [exposure], [gamma])` | entity, bool, float... | 设置颜色分级参数 |
| `ecs.set_post_process_ssao(e, [enabled], [radius], [bias])` | entity, bool, float, float | 设置 SSAO 参数 |
| `ecs.set_post_process_fxaa(e, [enabled])` | entity, bool | 设置 FXAA 抗锯齿 |
| `ecs.set_post_process_auto_exposure(e, [enabled], [min_ev], [max_ev], [speed_up], [speed_down], [compensation])` | entity, bool, float... | 设置自动曝光 |
| `ecs.set_post_process_vignette(e, [enabled], [intensity], [radius], [softness])` | entity, bool, float... | 设置暗角参数 |
| `ecs.set_post_process_film_grain(e, [enabled], [intensity])` | entity, bool, float | 设置胶片颗粒参数 |
| `ecs.set_post_process_color_lut(e, [lut_path], [intensity])` | entity, string/nil, [float=1] | 设置颜色 LUT 查找表（传 nil 清除，支持 .cube 文件） |
| `ecs.set_post_process_outline(e, [enabled], [r], [g], [b], [thickness], [depth_threshold], [normal_threshold])` | entity, bool, float... | 设置轮廓线 |
| `ecs.set_post_process_fog(e, [enabled], [density], [height_falloff], [height_offset], [start], [end], [steps], [sun_scatter], [r], [g], [b])` | entity, bool, float... | 设置体积雾 |
| `ecs.set_post_process_light_shaft(e, [enabled], [density], [weight], [decay], [exposure], [intensity], [samples], [r], [g], [b])` | entity, bool, float... | 设置光束/体积光 |
| `ecs.set_post_process_ssr(e, enabled, [max_distance], [fade_distance], [max_roughness], [thickness], [step_size], [max_steps])` | entity, bool, float..., [int] | 设置屏幕空间反射（SSR），未传参数沿用当前值，返回 bool |
| `ecs.get_post_process_state(e)` | entity | 见下方 | 获取完整后处理状态 |

**`get_post_process_state` 返回值 (19 个)：**
`ok, enabled, bloom_on, bloom_threshold, bloom_intensity, color_grading_on, exposure, gamma, ssao_on, ssao_radius, ssao_bias, fxaa_on, vignette_on, vignette_intensity, vignette_radius, vignette_softness, film_grain_on, film_grain_intensity, film_grain_time_scale`

### 5.10 Decal

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_decal(e)` | entity | 添加贴花组件（返回 bool） |
| `ecs.set_decal(e, [enabled], [albedo_handle], [r], [g], [b], [a], [angle_fade])` | entity, bool, int, float... | 修改贴花参数（返回 bool） |

### 5.11 Water

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_water(e)` | entity | 添加水面组件（返回 bool） |
| `ecs.set_water(e, ...)` | entity, 26个可选参数 | 设置水面参数（返回 bool） |
| `ecs.get_water(e)` | entity | 获取水面状态 |

**`set_water` 参数顺序 (26 个)：**
`enabled, water_level, deep_r, deep_g, deep_b, shallow_r, shallow_g, shallow_b, max_depth, transparency, wave_amplitude, wave_frequency, wave_speed, wave_dir_x, wave_dir_y, refraction_strength, reflection_strength, specular_power, caustic_intensity, caustic_scale, foam_intensity, foam_depth_threshold, underwater_fog_density, uw_fog_r, uw_fog_g, uw_fog_b`

**`get_water` 返回值 (19 个)：**
`ok, enabled, water_level, deep_r,g,b, shallow_r,g,b, max_depth, transparency, wave_amplitude, wave_frequency, wave_speed, wave_dir_x, wave_dir_y, refraction_strength, reflection_strength, specular_power`

**示例：**
```lua
local water = dse.ecs.create_entity()
dse.ecs.add_transform(water, 0, 0, 0)
dse.ecs.add_water(water)
dse.ecs.set_water(water, true, 0.0,
    0.0, 0.1, 0.3,     -- deep_color
    0.1, 0.3, 0.5,     -- shallow_color
    10.0, 0.6,          -- max_depth, transparency
    0.5, 2.0, 1.0,      -- wave: amplitude, frequency, speed
    1.0, 0.0)            -- wave direction
```

### 5.12 Steering (AI 转向)

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_steering(e, [max_vel], [max_force], [mass])` | entity, float... | 5, 10, 1 | 添加转向组件 |
| `ecs.set_steering_target(e, behavior, tx, ty, tz)` | entity, string, float... | — | 设置转向行为和目标 |
| `ecs.get_steering_state(e)` | entity | 见下方 | 获取完整转向状态 |

**behavior 值：** `"seek"`, `"flee"`, `"arrive"`
**`set_steering_target` 返回值：** `bool` (成功)

**`get_steering_state` 返回值 (22 个)：**
`ok, enabled, seek_on, flee_on, arrive_on, vel_x, vel_y, vel_z, speed, max_vel, max_force, mass, arrive_decel_radius, seek_x, seek_y, seek_z, flee_x, flee_y, flee_z, arrive_x, arrive_y, arrive_z`

### 5.13 LOD

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.lod_add_level(e, mesh_path, threshold)` | entity, string, float | 添加 LOD 层级（自动创建 LODGroupComponent） |
| `ecs.lod_set_scale(e, scale)` | entity, float | 设置全局 LOD 距离缩放 |
| `ecs.lod_set_enabled(e, enabled)` | entity, bool | 启用/禁用 LOD |
| `ecs.lod_set_min_screen_size(e, min_size)` | entity, float | 低于此屏幕占比时裁剪隐藏实体 |

### 5.14 Grass

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_grass(e, [density], [spawn_radius], [blade_height], [blade_width])` | entity, float... | 1, 50, 1, 0.1 | 添加草地组件 |
| `ecs.set_grass_params(e, [density], [radius], [height], [width], [height_var], [chunk_size], [seed])` | entity, float... | — | 设置草地参数 |
| `ecs.set_grass_color(e, base_r, base_g, base_b, [tip_r], [tip_g], [tip_b])` | entity, float... | — | 设置草地颜色（底部 + 尖端） |
| `ecs.set_grass_wind(e, dir_x, dir_z, [speed], [strength], [turbulence])` | entity, float... | — | 设置草地风力 |
| `ecs.set_grass_lod(e, lod_near, lod_far, [cast_shadow], [shadow_distance])` | entity, float... | — | 设置草地 LOD 和阴影 |
| `ecs.set_grass_enabled(e, enabled)` | entity, bool | — | 启用/禁用草地 |
| `ecs.get_grass_stats(e)` | entity | `int` | 获取缓存的草实例数 |

### 5.15 Hair（TressFX 风格毛发）

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_hair(e, asset_path, [num_follow_per_guide])` | entity, string, [int] | 添加毛发组件，加载 .dhair 资源 |
| `ecs.set_hair_physics(e, [damping], [stiffness_local], [stiffness_global], [gravity])` | entity, float... | 设置毛发物理参数 |
| `ecs.set_hair_render(e, root_r,g,b,a, tip_r,g,b,a, [fiber_radius], [opacity])` | entity, float... | 设置毛发渲染参数（根/尖颜色） |
| `ecs.set_hair_wind(e, wx, wy, wz, [turbulence])` | entity, float... | 设置毛发风力 |
| `ecs.set_hair_enabled(e, enabled)` | entity, bool | 启用/禁用毛发 |
| `ecs.set_hair_lod(e, [lod0], [lod1], [lod2], [cull])` | entity, float... | 设置毛发 LOD 距离 |

### 5.16 GI Probe（DDGI 全局光照探针）

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_gi_probe(e, [ox,oy,oz], [ex,ey,ez], [rx,ry,rz])` | entity, float... | 添加 GI 探针体积 |
| `ecs.set_gi_probe(e, [origin], [extent], [res], [gi_intensity], [normal_bias], [hysteresis])` | entity, float... | 修改 GI 探针参数 |
| `ecs.set_gi_probe_enabled(e, enabled)` | entity, bool | 启用/禁用 GI 探针 |
| `ecs.get_gi_probe(e)` | entity | 见下方 | 获取 GI 探针状态 |

**`get_gi_probe` 返回值 (12 个)：**
`enabled, ox, oy, oz, ex, ey, ez, rx, ry, rz, gi_intensity, normal_bias`

### 5.17 Utility

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.world_to_screen(wx, wy, wz)` | float, float, float | `sx, sy, visible` | 世界坐标投影到屏幕坐标（使用最高优先级摄像机） |
| `ecs.screen_to_world_ray(sx, sy)` | float, float | `ox,oy,oz, dx,dy,dz` / nil | 屏幕像素反投影出世界拾取射线（起点=相机位置，方向已归一化）。无主相机返回 nil |
| `ecs.pick_entity(sx, sy, [max_dist=1000])` | float, float, [float] | `entity, hx,hy,hz, nx,ny,nz, dist` / nil | 便捷拾取：屏幕像素 → 主相机射线 → 3D 物理 raycast，返回命中实体+命中点+法线+距离。无主相机或未命中返回 nil |

> 拾取示例（鼠标点击选中实体）：
> ```lua
> local mx, my = dse.app.get_mouse_x(), dse.app.get_mouse_y()
> local e = dse.ecs.pick_entity(mx, my)
> if e then print("picked", e) end
> ```

### 5.18 Morph Target

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.add_morph(e)` | entity | — | 添加 Morph 组件 |
| `ecs.morph_add_target(e, name, [weight])` | entity, string, [float=0] | — | 添加变形目标 |
| `ecs.morph_set_weight(e, name_or_index, weight)` | entity, string/int, float | — | 设置变形目标权重（按名称或索引） |
| `ecs.morph_get_weight(e, name_or_index)` | entity, string/int | `number` | 获取变形目标权重 |
| `ecs.set_morph_enabled(e, enabled)` | entity, bool | — | 启用/禁用 Morph |

**示例：**
```lua
local face = dse.ecs.create_entity()
dse.ecs.add_morph(face)
dse.ecs.morph_add_target(face, "smile", 0.0)
dse.ecs.morph_add_target(face, "blink", 0.0)
dse.ecs.morph_set_weight(face, "smile", 0.8)
local w = dse.ecs.morph_get_weight(face, 0)  -- 按索引
```

### 5.19 Light Probe

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_light_probe(e, [influence_radius])` | entity, [float=10] | — | 添加 Light Probe（SH 间接光照采样点） |
| `ecs.set_light_probe(e, [influence_radius], [needs_rebake])` | entity, float, bool | — | 修改 Light Probe 参数 |
| `ecs.set_light_probe_enabled(e, enabled)` | entity, bool | — | 启用/禁用 Light Probe |

### 5.20 Reflection Probe

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_reflection_probe(e, [influence_radius])` | entity, [float=15] | — | 添加反射探针（环境 cubemap） |
| `ecs.set_reflection_probe(e, [radius], [box_x], [box_y], [box_z], [resolution])` | entity, float... | — | 修改反射探针参数 |
| `ecs.set_reflection_probe_enabled(e, enabled)` | entity, bool | — | 启用/禁用反射探针 |

### 5.21 Physics 2D

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_rigid_body(e, [type], [gravity_scale], [fixed_rotation])` | entity, int, float, int | 2, 1, 0 | 添加 2D 刚体 |
| `ecs.set_rigid_body_velocity(e, vx, vy)` | entity, float, float | — | 设置 2D 刚体速度 |
| `ecs.add_box_collider(e, w, h, [density], [friction], [restitution])` | entity, float... | 1, 0.3, 0 | 添加矩形碰撞体 |
| `ecs.set_box_collider_trigger(e, is_trigger)` | entity, bool | — | 设置碰撞体为触发器 |
| `ecs.add_circle_collider(e, radius, [density], [friction], [restitution])` | entity, float... | 1, 0.3, 0 | 添加圆形碰撞体 |
| `ecs.set_circle_collider_trigger(e, is_trigger)` | entity, bool | — | 设置圆形碰撞体为触发器 |
| `ecs.add_polygon_collider(e, ...)` | entity, ... | — | 添加多边形碰撞体 |
| `ecs.set_polygon_collider_trigger(e, is_trigger)` | entity, bool | — | 设置多边形碰撞体为触发器 |
| `ecs.raycast_2d(sx, sy, ex, ey)` | float... | — | 2D 射线检测 |
| `ecs.poll_collision_event(e)` | entity | — | 轮询碰撞事件 |

**刚体类型：** `0=Static, 1=Kinematic, 2=Dynamic`

**`raycast_2d` 返回值（命中时）：** `true, entity, hit_x, hit_y, normal_x, normal_y`
**`poll_collision_event` 返回值（有事件时）：** `true, other_entity, is_trigger, is_enter`

#### 2D 关节

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_joint_2d(e, type, entity_a, entity_b, [ax], [ay], [bx], [by], [collide])` | entity, int... | 添加关节 |
| `ecs.set_joint_2d_revolute(e, [limit], [lower], [upper], [motor], [speed], [torque])` | entity, bool, float... | 设置铰链关节参数 |
| `ecs.set_joint_2d_distance(e, [min_len], [max_len], [stiffness], [damping])` | entity, float... | 设置距离关节参数 |
| `ecs.set_joint_2d_prismatic(e, [ax], [ay], [limit], [lower], [upper], [motor], [speed], [force])` | entity, float... | 设置滑动关节参数 |
| `ecs.destroy_joint_2d(e)` | entity | — | 销毁关节 |

**关节类型：** `0=Revolute, 1=Distance, 2=Prismatic, 3=Weld`

#### Tilemap

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_tilemap(e, width, height, [tile_size], [tex_handle])` | entity, int, int, float, int | 1, 0 | 添加 Tilemap |
| `ecs.set_tile(e, x, y, tile_id)` | entity, int, int, int | — | 设置单个 Tile |

### 5.22 Physics 3D

#### 刚体与碰撞体

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_rigidbody_3d(e, [type], [mass])` | entity, int, float | 2, 1 | 添加 3D 刚体 |
| `ecs.add_box_collider_3d(e, x, y, z)` | entity, float... | — | 添加 3D 盒碰撞体 |
| `ecs.add_sphere_collider_3d(e, radius)` | entity, float | — | 添加球碰撞体 |
| `ecs.add_capsule_collider_3d(e, radius, height, [direction], [is_trigger])` | entity, float, float, [int=1], [bool=false] | — | 添加胶囊碰撞体（direction: 0=X, 1=Y, 2=Z） |
| `ecs.add_mesh_collider_3d(e, [convex], [is_trigger])` | entity, [bool=false], [bool=false] | — | 添加网格碰撞体 |
| `ecs.rigidbody_3d_add_force(e, fx, fy, fz)` | entity, float... | — | 施加持续力 |
| `ecs.rigidbody_3d_add_impulse(e, ix, iy, iz)` | entity, float... | — | 施加瞬时冲量 |
| `ecs.rigidbody_3d_set_velocity(e, vx, vy, vz)` | entity, float... | — | 设置线速度 |
| `ecs.rigidbody_3d_get_velocity(e)` | entity | `vx, vy, vz` | 获取线速度 |
| `ecs.rigidbody_3d_set_gravity(e, enabled)` | entity, bool | — | 启用/禁用重力 |
| `ecs.rigidbody_3d_set_angular_velocity(e, wx, wy, wz)` | entity, float... | — | 设置角速度 |
| `ecs.rigidbody_3d_get_angular_velocity(e)` | entity | `wx, wy, wz` | 获取角速度 |
| `ecs.rigidbody_3d_add_torque(e, tx, ty, tz)` | entity, float... | — | 施加扭矩 |
| `ecs.physics_3d_raycast(ox, oy, oz, dx, dy, dz, [max_dist])` | float... | 1000 | 3D 射线检测 |

**3D 刚体类型：** `0=Static, 1=Kinematic, 2=Dynamic`

**`physics_3d_raycast` 返回值（命中时）：**
`true, entity, hit_x, hit_y, hit_z, normal_x, normal_y, normal_z, distance`

#### CharacterController 3D

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_character_controller_3d(e, [radius], [height], [slope_limit], [step_offset])` | entity, float... | 0.3, 1, 45, 0.3 | 添加角色控制器 |
| `ecs.character_controller_3d_move(e, dx, dy, dz, [min_dist], [dt])` | entity, float... | 0, 1/60 | 移动角色 |
| `ecs.character_controller_3d_jump(e, [jump_speed])` | entity, float | 5 | 角色跳跃 |
| `ecs.character_controller_3d_is_grounded(e)` | entity | `bool` | 是否着地 |
| `ecs.character_controller_3d_get_position(e)` | entity | `x, y, z` | 获取角色位置 |

**`character_controller_3d_move` 返回值：**
`is_grounded, vel_x, vel_y, vel_z, collision_flags`

#### Terrain Heightmap

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_terrain_heightmap(e)` | entity | 添加地形高度图组件 |
| `ecs.terrain_heightmap_set_data(e, origin_x, origin_z, block_size, scale_y, flip_z, cols, rows, heights)` | entity, float..., int, int, table | 设置高度图数据 |
| `ecs.terrain_get_height(world_x, world_z)` | float, float | `number` | 采样所有高度图，返回最高值 |

#### Overlap Query

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.physics_3d_overlap_sphere(cx, cy, cz, radius)` | float... | `table` | 球体重叠查询，返回实体数组 |
| `ecs.physics_3d_overlap_box(min_x, min_y, min_z, max_x, max_y, max_z)` | float... | `table` | AABB 重叠查询，返回实体数组 |

#### Collision & Trigger Events [PhysX]

| 函数 | 返回值 | 说明 |
|------|--------|------|
| `ecs.physics_3d_get_collision_events()` | `table` | 获取当前帧碰撞事件列表 |
| `ecs.physics_3d_get_trigger_events()` | `table` | 获取当前帧触发器事件列表 |

**碰撞事件字段：** `{type, entity_a, entity_b, px, py, pz, nx, ny, nz, impulse}`
**触发器事件字段：** `{type, trigger_entity, other_entity}`

#### 3D 关节 (Joint3D)

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_joint_3d(e, entity_b_id, [type], [ax,ay,az], [bx,by,bz], [break_force], [break_torque])` | entity, int, [int=0], float..., [float=FLT_MAX], [float=FLT_MAX] | 添加 3D 关节 |
| `ecs.set_joint_3d_hinge_limits(e, lower_deg, upper_deg)` | entity, float, float | 设置铰链关节角度限制 |
| `ecs.set_joint_3d_spring(e, stiffness, damping)` | entity, float, float | 设置弹簧关节参数 |
| `ecs.set_joint_3d_distance(e, min_dist, max_dist)` | entity, float, float | 设置距离关节范围 |
| `ecs.is_joint_3d_broken(e)` | entity | `bool` | 查询关节是否已断裂 |

**关节类型：** 由 `Joint3DType` 枚举定义

#### Collision Layers

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.set_collision_layer(e, layer, mask)` | entity, int, int | 设置碰撞层和掩码（对 RigidBody3D） |

#### Collider Helpers

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.set_collider_trigger(e, is_trigger)` | entity, bool | 设置实体所有碰撞体的 trigger 标志 |
| `ecs.set_collider_material(e, friction, bounciness)` | entity, float, float | 设置实体所有碰撞体的物理材质 |

**示例：**
```lua
local player = dse.ecs.create_entity()
dse.ecs.add_transform(player, 0, 1, 0)
dse.ecs.add_character_controller_3d(player, 0.3, 1.8, 45, 0.3)

-- 碰撞事件查询
local events = dse.ecs.physics_3d_get_collision_events()
for _, ev in ipairs(events) do
    print("Collision:", ev.entity_a, ev.entity_b, "impulse:", ev.impulse)
end

-- Overlap 查询
local nearby = dse.ecs.physics_3d_overlap_sphere(0, 1, 0, 5.0)
for _, eid in ipairs(nearby) do
    print("Entity in range:", eid)
end
```

### 5.23 Gameplay3D（ECS 组件）

#### 破碎系统 [PhysX]

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_fracture(e, [source], [fragment_count], [break_force], [health])` | entity, int... | 1, 8, 1000, 100 | 添加破碎组件。source: 0=Prefractured, 1=RuntimeVoronoi |
| `ecs.set_fracture_params(e, [explosion_force], [fragment_lifetime], [fade_duration], [fragment_mass_scale])` | entity, float... | — | 设置破碎参数 |
| `ecs.fracture_apply_damage(e, damage, [impact_x], [impact_y], [impact_z])` | entity, float, [float...] | — | 对可破碎物施加伤害 |
| `ecs.fracture_trigger(e, [impact_x], [impact_y], [impact_z])` | entity, [float...] | — | 强制触发破碎 |
| `ecs.fracture_is_fractured(e)` | entity | `bool` | 查询是否已破碎 |

#### 布料系统

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_cloth(e, [solver_iterations], [stiffness], [damping], [bend_stiffness])` | entity, int, float... | 8, 1.0, 0.01, 0.5 | 添加布料组件 |
| `ecs.set_cloth_wind(e, wx, wy, wz, [turbulence])` | entity, float... | — | 设置布料风力 |
| `ecs.set_cloth_gravity(e, gx, gy, gz)` | entity, float, float, float | — | 设置布料重力 |
| `ecs.cloth_pin_vertices(e, {v1, v2, ...})` | entity, table | — | 固定指定顶点 |
| `ecs.cloth_add_sphere_collider(e, collider_entity, [radius])` | entity, entity, [float=0.5] | — | 为布料添加球碰撞体 |

#### 流体系统

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_fluid_emitter(e, [shape], [emission_rate], [particle_lifetime], [emit_speed])` | entity, int, float... | 0, 500, 3, 2 | 添加流体发射器。shape: 0=Point, 1=Sphere, 2=Box |
| `ecs.set_fluid_physics(e, [viscosity], [surface_tension], [rest_density], [gas_stiffness])` | entity, float... | — | 设置流体物理参数 |
| `ecs.set_fluid_rendering(e, r, g, b, [a], [refraction], [fresnel], [specular])` | entity, float... | a=0.8 | 设置流体渲染参数 |
| `ecs.set_fluid_emit_direction(e, dx, dy, dz, [spread])` | entity, float... | — | 设置发射方向 |
| `ecs.set_fluid_floor(e, [floor_y], [restitution])` | entity, float... | — | 设置流体地面 |
| `ecs.get_fluid_particle_count(e)` | entity | `int` | 获取活跃粒子数 |

#### 布娃娃系统 [PhysX]

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_ragdoll(e, [total_mass], [auto_setup], [joint_stiffness], [joint_damping])` | entity, float, bool, float, float | 10, true, 0, 50 | 添加布娃娃 |
| `ecs.ragdoll_activate(e)` | entity | — | 激活布娃娃 |
| `ecs.ragdoll_deactivate(e)` | entity | — | 停用布娃娃 |
| `ecs.ragdoll_is_active(e)` | entity | `bool` | 查询布娃娃是否激活 |
| `ecs.set_ragdoll_collision_layer(e, layer, mask)` | entity, int, int | — | 设置布娃娃碰撞层 |

#### 软体系统

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_softbody(e, [stiffness], [iterations], [damping], [volume_stiffness])` | entity, float, int, float, float | 0.5, 4, 0.99, 0.5 | 添加软体 |
| `ecs.softbody_set_gravity(e, use_gravity, [gravity_scale])` | entity, bool, [float] | — | 设置软体重力 |
| `ecs.softbody_pin_vertex(e, vertex_index)` | entity, int | — | 固定指定顶点 |
| `ecs.softbody_get_particle_count(e)` | entity | `int` | 获取粒子数 |

#### 车辆系统 [PhysX]

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_vehicle(e, [max_engine_force], [max_brake_force], [max_steer_angle])` | entity, float... | 5000, 3000, 35 | 添加车辆 |
| `ecs.vehicle_add_wheel(e, px, py, pz, [radius], [is_drive], [is_steer], [susp_stiffness], [susp_damping])` | entity, float..., [bool], [bool], float... | 0.3, true, false, 30000, 4500 | 添加车轮 |
| `ecs.vehicle_set_input(e, throttle, brake, steering)` | entity, float, float, float | — | 设置输入（throttle/steering: -1~1, brake: 0~1） |
| `ecs.vehicle_get_speed(e)` | entity | `number` | 获取当前速度（m/s） |
| `ecs.vehicle_get_wheel_count(e)` | entity | `int` | 获取车轮数 |

#### 绳索系统

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_rope(e, [segment_count], [segment_length], [damping], [iterations])` | entity, int, float, float, int | 10, 0.2, 0.99, 8 | 添加绳索 |
| `ecs.rope_set_anchors(e, anchor_a_entity, anchor_b_entity, [off_ax,ay,az], [off_bx,by,bz])` | entity, int, int, [float...] | 0,0,0 | 设置绳索锚点 |
| `ecs.rope_get_positions(e)` | entity | `table` | 获取绳索节点位置 `{{x,y,z}, ...}` |
| `ecs.rope_set_gravity(e, use_gravity, [gravity_scale])` | entity, bool, [float] | — | 设置绳索重力 |

#### 浮力系统 [PhysX]

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_buoyancy(e, [water_level], [buoyancy_force], [water_drag], [angular_drag], [submerge_depth])` | entity, float... | 0, 10, 3, 1, 1 | 添加浮力 |
| `ecs.buoyancy_add_sample_point(e, ox, oy, oz, [force_scale])` | entity, float..., [float=1] | — | 添加浮力采样点 |
| `ecs.buoyancy_set_water_level(e, water_level)` | entity, float | — | 设置水面高度 |
| `ecs.buoyancy_get_submerge_ratio(e)` | entity | `number` | 获取淹没比率 [0,1] |
| `ecs.buoyancy_set_use_fluid(e, use_fluid_system)` | entity, bool | — | 使用流体系统水面 |

#### 天气 / 大气 / 昼夜 / 积雪 / 体积云

> 对应 `lua_binding_ecs_gameplay3d.cpp`。多个 `set_*` 函数的参数均为**可选**，
> 省略（或传 nil）时保留原值，仅修改传入的字段。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.add_atmosphere(e)` | entity | — | 添加大气散射组件 |
| `ecs.set_atmosphere_params(e, [planet_radius], [atmosphere_radius], [sun_intensity])` | entity, float... | — | 设置大气基础参数 |
| `ecs.set_atmosphere_rayleigh(e, [r], [g], [b], [scale_height])` | entity, float... | — | 设置瑞利散射系数与标高 |
| `ecs.set_atmosphere_mie(e, [coefficient], [scale_height], [g])` | entity, float... | — | 设置米氏散射参数 |
| `ecs.set_atmosphere_sun_intensity(e, [intensity], [sun_disk_size], [sun_disk_intensity])` | entity, float... | — | 设置太阳强度/日面 |
| `ecs.add_day_night_cycle(e, [time_of_day], [auto_advance], [speed])` | entity, [float=12], [bool=false], [float=1] | — | 添加昼夜循环（time_of_day 为 0~24 小时） |
| `ecs.set_day_night_time(e, time_of_day)` | entity, float | — | 设置当前时间（0~24） |
| `ecs.get_day_night_time(e)` | entity | `number` | 获取当前时间 |
| `ecs.set_day_night_speed(e, speed)` | entity, float | — | 设置时间流速倍率 |
| `ecs.set_day_night_auto_advance(e, enabled)` | entity, bool | — | 启用/禁用自动推进时间 |
| `ecs.set_day_night_location(e, [latitude], [longitude], [day_of_year])` | entity, float, float, [int=-1] | — | 设置地理位置（影响太阳轨迹） |
| `ecs.get_sun_direction(e)` | entity | `x, y, z` | 获取当前太阳方向 |
| `ecs.get_sun_elevation(e)` | entity | `number` | 获取太阳仰角 |
| `ecs.add_volumetric_cloud(e)` | entity | — | 添加体积云组件 |
| `ecs.set_cloud_layer(e, [coverage], [density], [altitude], [thickness])` | entity, float... | — | 设置云层参数 |
| `ecs.set_cloud_wind(e, [wind_x], [wind_z], [speed])` | entity, float... | — | 设置云层风向/速度 |
| `ecs.add_weather(e, [type], [intensity])` | entity, [string="snow"], [float=0.5] | — | 添加天气组件（雨/雪等） |
| `ecs.set_weather(e, [type], [intensity], [wind_x], [wind_z])` | entity, string, float... | — | 设置天气类型与强度 |
| `ecs.set_weather_spawn(e, [spawn_radius], [spawn_height], [max_particles])` | entity, float, float, [int=-1] | — | 设置天气粒子生成参数 |
| `ecs.add_snow_cover(e)` | entity | — | 添加积雪覆盖组件 |
| `ecs.remove_snow_cover(e)` | entity | — | 移除积雪覆盖组件 |
| `ecs.set_snow_cover_enabled(e, enabled)` | entity, bool | — | 启用/禁用积雪 |
| `ecs.set_snow_cover(e, [amount], [min_slope], [height_threshold])` | entity, float... | — | 设置积雪覆盖量与阈值 |
| `ecs.get_snow_cover(e)` | entity | `amount, height, enabled` | 获取积雪状态 |
| `ecs.set_snow_appearance(e, [r], [g], [b], [sparkle], [roughness], [metallic], [coverage_sharpness])` | entity, float... | — | 设置积雪外观 |
| `ecs.set_snow_displacement(e, [depth], [scale])` | entity, float... | — | 设置积雪位移（踩踏痕迹） |
| `ecs.set_snow_texture(e, [path], [tiling])` | entity, [string], [float] | — | 设置积雪贴图 |

### 5.24 Animation 2D

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_animator(e)` | entity | 添加 2D 动画器 |
| `ecs.add_animation_state(e, name, fps, loop, [frame_handles])` | entity, string, float, bool, [table] | 添加动画状态 |
| `ecs.add_animation_event(e, state_name, normalized_time, event_name)` | entity, string, float, string | 添加动画事件（0~1 归一化时间） |
| `ecs.play_animation(e, state_name)` | entity, string | 播放指定动画状态 |
| `ecs.play_animation_segment(e, start_frame, end_frame, loop)` | entity, int, int, bool | 播放帧段 |
| `ecs.pop_animation_event(e)` | entity | `string` | 弹出已触发的动画事件名（空字符串=无） |

### 5.25 Animation 3D

#### 基础骨骼动画

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_animator_3d(e, [danim_path], [dskel_path])` | entity, string, string | 添加 3D 骨骼动画器 |
| `ecs.set_animator_3d_state(e, [state_or_path], [speed], [loop])` | entity, string, float, bool | 设置动画状态/路径 |
| `ecs.get_animator_3d_state(e)` | entity | 见下方 | 获取动画器完整状态 |

**`get_animator_3d_state` 返回值：**
`ok, current_state, normalized_time, time, speed, loop, is_transitioning, bone_count, has_skeleton`

#### 状态机（FSM）

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.init_animator_3d_fsm(e)` | entity | 初始化状态机 |
| `ecs.add_animator_3d_state(e, name, danim_path, loop, [speed])` | entity, string, string, bool, float | 添加 FSM 状态 |
| `ecs.add_animator_3d_transition(e, from, to, [duration], has_exit_time, [exit_time], [conditions])` | entity, string... | 添加状态转换 |
| `ecs.set_animator_3d_param_float(e, name, value)` | entity, string, float | 设置 FSM float 参数 |
| `ecs.set_animator_3d_param_trigger(e, name)` | entity, string | 触发 FSM trigger 参数 |

**转换条件表格式（Lua table of tables）：**
```lua
{ {"speed", 0, 0.1}, {"is_jumping", 1, 0} }  -- {param_name, mode, threshold}
```

**条件模式：** `0=Greater, 1=Less, 2=Equal, 3=NotEqual`

#### Root Motion

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.set_animator_3d_lock_root_motion(e, lock)` | entity, bool | — | 锁定/解锁根骨骼动画 |
| `ecs.set_animator_3d_extract_root_motion(e, extract)` | entity, bool | — | 启用/禁用根骨骼运动提取 |
| `ecs.get_animator_3d_root_motion_delta(e)` | entity | `dx, dy, dz` | 获取当前帧根骨骼位移增量 |

#### 3D 动画事件

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.add_animator_3d_event(e, event_name, trigger_time)` | entity, string, float | — | 添加动画事件（归一化时间 0~1） |
| `ecs.pop_animator_3d_event(e)` | entity | `string` | 弹出已触发的 3D 动画事件名 |

#### 动画层系统（AnimLayer）

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.add_anim_layer_component(e)` | entity | — | 添加动画层组件（与 animator_3d 搭配使用） |
| `ecs.add_anim_layer(e, [name], [weight], [blend_mode])` | entity, string, float, int | `int` | 添加动画层，返回层索引。blend_mode: 0=Override, 1=Additive |
| `ecs.set_anim_layer_clip(e, idx, danim_path, [speed], [loop])` | entity, int, string, float, bool | — | 设置层的动画剪辑源 |
| `ecs.set_anim_layer_weight(e, idx, weight)` | entity, int, float | — | 设置层权重 |
| `ecs.set_anim_layer_bone_mask(e, idx, bone_names)` | entity, int, table | — | 设置骨骼遮罩（如 `{"Spine", "LeftArm"}`） |
| `ecs.set_anim_layer_blend_tree_1d(e, idx, nodes)` | entity, int, table | — | 设置层为 1D 混合树 |
| `ecs.set_anim_layer_blend_param(e, idx, value)` | entity, int, float | — | 设置层混合参数值 |
| `ecs.set_anim_layer_enabled(e, enabled)` | entity, bool | — | 启用/禁用动画层组件 |

#### IK 系统

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.add_ik_component(e)` | entity | — | 添加 IK 组件（与 animator_3d 搭配使用） |
| `ecs.add_ik_chain(e, [name], [type], [root_bone], [tip_bone], [weight])` | entity, string, int, string, string, float | `int` | 添加 IK 链，返回索引。type: 0=FABRIK, 1=LookAt |
| `ecs.set_ik_target(e, idx, x, y, z)` | entity, int, float... | — | 设置 IK 目标位置（世界坐标） |
| `ecs.set_ik_target_entity(e, idx, target_entity)` | entity, int, entity | — | 设置 IK 跟随目标实体 |
| `ecs.set_ik_weight(e, idx, weight)` | entity, int, float | — | 设置 IK 链权重 |
| `ecs.set_ik_pole_vector(e, idx, x, y, z)` | entity, int, float... | — | 设置极向量（控制弯曲方向） |
| `ecs.set_ik_iterations(e, idx, iterations)` | entity, int, int | — | 设置 FABRIK 迭代次数 |
| `ecs.set_ik_enabled(e, enabled)` | entity, bool | — | 启用/禁用 IK 组件 |

#### FootIK 系统（脚部贴地）

> 与 `Animator3DComponent` 搭配使用，运行时由 `FootIKSystem`（gameplay_3d 模块）求解：用物理 **Raycast** 检测地面，FABRIK 调整腿部贴地，并按需下沉骨盆。需要场景内有 3D 物理系统（地面碰撞体）才能检测到地面。可选浮点参数省略时保持组件默认值。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.add_foot_ik_component(e)` | entity | — | 添加 FootIK 组件 |
| `ecs.add_foot_ik_foot(e, [name], [foot_bone], [hip_bone], [foot_height], [max_ground_distance], [blend_speed], [weight])` | entity, string, string, string, float, float, float, float | `int` | 添加一只脚的配置，返回索引。`foot_bone`/`hip_bone` 为骨骼名，链从 hip 到 foot |
| `ecs.set_foot_ik_foot_weight(e, idx, weight)` | entity, int, float | — | 设置指定脚的 IK 权重 |
| `ecs.set_foot_ik_foot_height(e, idx, height)` | entity, int, float | — | 设置脚底离地高度偏移 |
| `ecs.set_foot_ik_pelvis(e, [pelvis_weight], [max_pelvis_offset])` | entity, float, float | — | 设置骨盆调整权重与最大下沉偏移 |
| `ecs.set_foot_ik_enabled(e, enabled)` | entity, bool | — | 启用/禁用 FootIK 组件 |

#### 骨骼挂点 (Bone Attachment)

> 将一个实体挂接到另一个骨骼动画实体的指定骨骼上（如武器挂手骨）。对应 `lua_binding_ecs_animation.cpp`。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.add_bone_attachment(e, target_entity, bone_name)` | entity, entity, string | — | 添加挂点：将 e 挂到 target 的 bone_name 骨骼 |
| `ecs.set_bone_attachment_target(e, target_entity)` | entity, entity | — | 修改挂接目标实体 |
| `ecs.set_bone_attachment_bone(e, bone_name)` | entity, string | — | 修改挂接骨骼名 |
| `ecs.set_bone_attachment_offset(e, px,py,pz, rx,ry,rz,rw, [sx,sy,sz])` | entity, float... | — | 设置挂点偏移（位置+四元数旋转+可选缩放） |
| `ecs.get_bone_world_position(target_entity, bone_name)` | entity, string | `x, y, z` | 获取指定骨骼的世界坐标 |
| `ecs.remove_bone_attachment(e)` | entity | — | 移除挂点组件 |

#### Morph Target（骨骼动画模块）

> 请注意与 §5.18 的 Morph Target 为不同组件，此处对应 `lua_binding_ecs_animation.cpp` 的 `MorphTargetComponent`。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.add_morph_target_component(e)` | entity | — | 添加骨骼动画 Morph 组件 |
| `ecs.morph_set_weight_index(e, index, weight)` | entity, int, float | — | 按索引（0-based）设置权重 |
| `ecs.morph_get_target_count(e)` | entity | `int` | 获取变形目标数量 |

### 5.26 Particle

#### 3D 粒子系统

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_particle_system_3d(e, [max], [emission_rate])` | entity, int, float | 1000, 100 | 添加 3D 粒子系统 |
| `ecs.set_particle_system_3d_params(e, ...)` | entity, float... | — | 设置粒子参数（见下） |
| `ecs.get_particle_system_3d_state(e)` | entity | 见下方 | 获取粒子系统状态 |

**`set_particle_system_3d_params` 参数顺序：**
`entity, life_min, life_max, size_min, size_max, speed_min, speed_max, color_r, color_g, color_b, color_a, gravity_x, gravity_y, gravity_z, [texture_path]`

**`get_particle_system_3d_state` 返回值 (21 个)：**
`ok, active_count, max, rate, life_min, life_max, size_min, size_max, speed_min, speed_max, grav_x, grav_y, grav_z, color_r, color_g, color_b, color_a, texture_path, enabled, initialized, tex_handle`

#### 2D 粒子发射器

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_particle_emitter(e, [tex_handle], [max], [emit_rate])` | entity, int, int, float | 0, 100, 10 | 添加 2D 粒子发射器 |
| `ecs.set_particle_density(e, scale)` | entity, float | — | 设置发射密度缩放（≥0） |
| `ecs.particle_burst(e, count)` | entity, int | — | 立即爆发发射 N 个粒子 |
| `ecs.set_particle_random(e, vmin_x,y,z, vmax_x,y,z, [life_min], [life_max], [size_min], [size_max])` | entity, float... | — | 启用随机参数（速度范围、生命范围、尺寸范围） |
| `ecs.set_particle_size_curve(e, enabled, [start], [end])` | entity, bool, float, float | — | 设置粒子尺寸曲线 |
| `ecs.set_particle_alpha_curve(e, enabled, [start], [end])` | entity, bool, float, float | — | 设置粒子透明度曲线 |
| `ecs.set_particle_speed_curve(e, enabled, [start], [end])` | entity, bool, float, float | — | 设置粒子速度曲线 |
| `ecs.set_particle_gravity(e, gx, gy, gz)` | entity, float, float, float | — | 设置粒子重力 |
| `ecs.set_particle_collision(e, enabled, [mode], [bounce], [friction], [life_loss], [ground_y])` | entity, bool, int, float... | — | 设置粒子碰撞。mode: 0=None, 1=GroundPlane, 2=Box2D |
| `ecs.set_particle_color_curve(e, enabled, [end_r], [end_g], [end_b], [end_a])` | entity, bool, float... | — | 设置粒子颜色渐变曲线 |
| `ecs.set_particle_rotation(e, [rot_min], [rot_max], [ang_vel_min], [ang_vel_max])` | entity, float... | — | 设置粒子旋转参数 |

### 5.27 GameplayTuning

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_gameplay_tuning(e)` | entity | 添加调参组件 |
| `ecs.set_gameplay_tuning(e, [leaf_min_dist], [move_left], [move_right], [jump_scale], [jump_max], [cam_damp])` | entity, float... | 设置调参参数 |

---

## 6. dse.localization — 本地化

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `localization.load(locale, json_string)` | `string, string` | `bool` | 从 JSON 字符串加载指定语言的翻译表 |
| `localization.set_locale(locale)` | `string` | `bool` | 切换当前语言 |
| `localization.get_locale()` | — | `string` | 获取当前语言标识（如 `"zh-CN"`） |
| `localization.get(key [, default])` | `string, [string]` | `string` | 查找翻译文本，key 不存在时返回 default（默认为 key 本身） |
| `localization.has_key(key)` | `string` | `bool` | 判断当前语言是否含有该 key |
| `localization.get_locales()` | — | `table` | 获取已加载的所有语言标识列表 |

**示例：**
```lua
local json = [[{"hello":"你好","quit":"退出"}]]
dse.localization.load("zh-CN", json)
dse.localization.set_locale("zh-CN")
print(dse.localization.get("hello"))     -- 你好
print(dse.localization.get("missing", "N/A"))  -- N/A
```

---

## 7. dse.origin — 浮动原点

大世界坐标系支持，通过周期性重置场景原点来避免浮点精度丢失。挂载在 `dse.origin`。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `origin.get_accumulated()` | — | `x, y, z` | 获取累计原点偏移量（double 精度） |
| `origin.to_absolute(lx, ly, lz)` | `float, float, float` | `ax, ay, az` | 本地坐标 → 绝对世界坐标（double 精度） |
| `origin.to_local(ax, ay, az)` | `number, number, number` | `lx, ly, lz` | 绝对世界坐标 → 本地渲染坐标 |
| `origin.set_rebase_threshold(threshold)` | `float` | — | 设置触发原点重置的距离阈值（默认 5000） |
| `origin.get_rebase_threshold()` | — | `number` | 获取当前重置阈值 |

**示例：**
```lua
dse.origin.set_rebase_threshold(2000)
local ox, oy, oz = dse.origin.get_accumulated()
print("World offset:", ox, oy, oz)
local lx, ly, lz = dse.origin.to_local(1234567, 0, 9876543)
```

---

## 8. dse.audio — 音频系统

### 音源

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `audio.add_source(e, path, play_on_awake, loop, [volume])` | entity, string, bool, bool, float | 1.0 | 添加音源组件 |
| `audio.set_playing(e, playing)` | entity, bool | — | 播放/暂停 |
| `audio.restart(e)` | entity | — | 从头重新播放 |
| `audio.set_loop(e, loop)` | entity, bool | — | 设置循环 |
| `audio.set_volume(e, volume)` | entity, float | — | 设置音量 |
| `audio.set_pitch(e, pitch)` | entity, float | — | 设置音调（≥0.01） |
| `audio.set_3d_mode(e, enabled)` | entity, bool | — | 启用/禁用空间音频 |
| `audio.add_listener(e)` | entity | — | 添加音频监听器 |
| `audio.set_3d_distance(e, [min], [max], [rolloff])` | entity, float... | 1, 20, 1 | 设置 3D 衰减距离 |
| `audio.get_source_state(e)` | entity | 见下方 | 获取音源完整状态 |

**`get_source_state` 返回值 (12 个)：**
`ok, has_clip, is_playing, spatial, min_dist, max_dist, rolloff, volume, pitch, runtime_handle, clip_size, clip_path`

### 全局播放 / 音量 / 快照

> 不需实体的全局 BGM/SFX 接口，以及主/BGM/SFX 音量、音源总线路由与混音快照。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `audio.play_bgm(path, [volume], [loop])` | string, [float=1.0], [bool] | `bool` | 播放背景音乐 |
| `audio.pause_bgm()` | — | — | 暂停 BGM |
| `audio.resume_bgm()` | — | — | 恢复 BGM |
| `audio.stop_bgm()` | — | — | 停止 BGM |
| `audio.crossfade_bgm(path, duration, [volume], [loop])` | string, float, [float=1.0], [bool] | `bool` | 交叉混响切换 BGM |
| `audio.play_sfx(path, [volume], [loop])` | string, [float=1.0], [bool=false] | — | 播放音效 |
| `audio.play_sfx_random(path, [volume], [pitch_min], [pitch_max])` | string, [float=1.0], [float=0.9], [float=1.1] | — | 播放音效（随机音调） |
| `audio.stop_all_sfx()` | — | — | 停止全部音效 |
| `audio.preload(path)` | string | `bool` | 预加载音频资源 |
| `audio.set_master_volume(volume)` | float | — | 设置主音量 |
| `audio.set_bgm_volume(volume)` | float | — | 设置 BGM 音量 |
| `audio.set_sfx_volume(volume)` | float | — | 设置 SFX 音量 |
| `audio.set_source_bus(e, bus_name)` | entity, string | — | 将音源路由到指定混音总线 |
| `audio.snapshot_save(name)` | string | `bool` | 保存当前混音总线状态为快照 |
| `audio.snapshot_load(name)` | string | `bool` | 加载混音快照 |
| `audio.snapshot_list()` | — | `table` | 列出所有快照名（字符串数组） |

### 混音总线 + DSP 效果链

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `audio.bus_create(name, [parent], [volume])` | string, [string="master"], [float=1.0] | `bool` | 创建混音总线 |
| `audio.bus_remove(name)` | string | `bool` | 删除混音总线 |
| `audio.bus_set_volume(name, volume)` | string, float | `bool` | 设置总线音量 |
| `audio.bus_set_muted(name, muted)` | string, bool | `bool` | 设置总线静音 |
| `audio.bus_add_effect(bus_name, type, [cutoff_hz], [q], [delay_time_ms], [feedback], [wet_mix])` | string, int, float... | 1000, 0.707, 250, 0.3, 0.5 | 添加 DSP 效果 |
| `audio.bus_remove_effect(bus_name, index)` | string, int | `bool` | 删除指定索引的效果 |
| `audio.bus_get_names()` | — | `table` | 获取所有总线名称 |

**DSP 效果类型（int）：** 由 `DspEffectType` 枚举定义

**示例：**
```lua
local bgm = dse.ecs.create_entity()
dse.ecs.add_transform(bgm, 0, 0, 0)
dse.audio.add_source(bgm, "data/audio/bgm.wav", true, true, 0.8)

-- 混音总线
dse.audio.bus_create("sfx", "master", 0.8)
dse.audio.bus_set_volume("sfx", 0.6)
dse.audio.bus_add_effect("sfx", 0, 2000.0)  -- 低通滤波器
```

---

## 9. dse.spine — Spine 动画

| 函数 | 参数 | 说明 |
|------|------|------|
| `spine.add_renderer(e, skel_path, atlas_path)` | entity, string, string | 添加 Spine 渲染器 |
| `spine.set_animation(e, anim_name, loop)` | entity, string, bool | 播放指定 Spine 动画 |

**示例：**
```lua
local char = dse.ecs.create_entity()
dse.ecs.add_transform(char, 0, 0, 0)
dse.spine.add_renderer(char, "data/spine/hero.skel", "data/spine/hero.atlas")
dse.spine.set_animation(char, "run", true)
```

---

## 10. dse.ui — UI 系统

### 创建 UI 组件

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ui.add_renderer(e, [tex], [r], [g], [b], [a], [order], [w], [h])` | entity, int, float... | 0, 1,1,1,1, 0, 640, 160 | 添加 UI 渲染器 |
| `ui.add_label(e, text, [font_tex], [r], [g], [b], [a], [gw], [gh], [spacing], [cols], [rows], [ascii], [ox], [oy])` | entity, string, int, float... | — | 添加位图文字标签 |
| `ui.set_label_text(e, text)` | entity, string | — | 修改标签文本 |
| `ui.set_label_number(e, number)` | entity, int | — | 设置标签为数字模式 |
| `ui.add_panel(e, blocks_input)` | entity, bool | — | 添加 UI 面板 |
| `ui.add_button(e, [r], [g], [b], [a])` | entity, float... | — | 添加按钮 |
| `ui.set_button_scale(e, [hover_scale], [pressed_scale], [lerp_speed])` | entity, float... | 1.08, 0.94, 12 | 设置按钮缩放动画 |
| `ui.add_mask(e, [w], [h], [ox], [oy], [block_outside])` | entity, float..., bool | 0,0,0,0,true | 添加 UI 遮罩 |
| `ui.add_rich_text(e, [text], [r], [g], [b], [a], [shadow], [outline])` | entity, string, float..., bool, bool | "",1,1,1,1,false,false | 添加富文本 |
| `ui.set_rich_text(e, text)` | entity, string | — | 修改富文本内容 |
| `ui.add_joystick(e, [max_radius], [follow], [reset])` | entity, float, bool, bool | 64, true, true | 添加虚拟摇杆 |

### 查询 UI 状态

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ui.get_joystick_x(e)` | entity | `number` | 获取摇杆 X 轴方向（-1~1） |
| `ui.get_joystick_y(e)` | entity | `number` | 获取摇杆 Y 轴方向（-1~1） |

### 设置 UI 属性

| 函数 | 参数 | 说明 |
|------|------|------|
| `ui.set_position(e, x, y)` | entity, float, float | 设置 UI 元素位置 |
| `ui.set_size(e, w, h)` | entity, float, float | 设置 UI 元素尺寸 |
| `ui.set_anchor(e, ax, ay)` | entity, float, float | 设置锚点 |
| `ui.set_color(e, r, g, b, a)` | entity, float, float, float, float | 设置颜色 |
| `ui.set_visible(e, visible)` | entity, bool | 设置可见性 |
| `ui.set_uv(e, u0, v0, u1, v1)` | entity, float... | 设置 UV 坐标 |
| `ui.set_nine_slice(e, left, right, top, bottom)` | entity, float... | 设置九宫格切片 |

### UIAnchorComponent

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ui.add_anchor(e, [anchor_type], [offset_x], [offset_y])` | entity, int, float, float | 5, 0, 0 | 添加锚点组件（anchor_type 对应 UIAnchor 枚举: 0=TopLeft…9=Stretch） |
| `ui.set_anchor_type(e, anchor_type)` | entity, int | — | 设置锚点类型 |
| `ui.set_anchor_offset(e, offset_x, offset_y)` | entity, float, float | — | 设置锚点偏移 |

### UIGridLayoutComponent

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ui.add_grid_layout(e, [columns], [cell_w], [cell_h], [spacing_x], [spacing_y])` | entity, int, float... | 1, 100, 100, 10, 10 | 添加网格布局 |
| `ui.set_grid_layout(e, [columns], [rows], [cell_w], [cell_h], [spacing_x], [spacing_y], [alignment])` | entity, int..., float... | — | 修改网格布局参数（nil 跳过） |

### UICanvasScalerComponent

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ui.add_canvas_scaler(e, [ref_w], [ref_h], [match_width_or_height])` | entity, float, float, bool | 1920, 1080, true | 添加画布缩放器 |
| `ui.set_canvas_scaler(e, [ref_w], [ref_h], [scale_factor], [match_width_or_height])` | entity, float..., bool | — | 修改画布缩放参数 |

### UIAnimationComponent

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ui.add_ui_animation(e, [duration], [easing], [loop], [ping_pong], [delay])` | entity, float, int, bool, bool, float | 0.3, 0, false, false, 0 | 添加 UI 动画组件。easing: 0=linear, 1=ease-in, 2=ease-out, 3=ease-in-out |
| `ui.animate_position(e, target_x, target_y)` | entity, float, float | — | 启动位置动画 |
| `ui.animate_scale(e, target_sx, target_sy)` | entity, float, float | — | 启动缩放动画 |
| `ui.animate_alpha(e, target_alpha)` | entity, float | — | 启动透明度动画 |
| `ui.animate_color(e, r, g, b, [a])` | entity, float..., [float=1] | — | 启动颜色动画 |
| `ui.stop_ui_animation(e)` | entity | — | 停止 UI 动画 |

### 输入与交互控件

> 以下控件均在 `lua_binding_ui.cpp` 中注册；entity 为 UI 实体。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ui.add_ttf_label(e, text, font_id, [font_size], [r], [g], [b], [a])` | entity, string, string, [float=32], float... | — | 添加 TTF/SDF 文字标签 |
| `ui.add_text_input(e, [placeholder], [max_length], [is_password])` | entity, [string=""], [int=0], [bool] | — | 添加文本输入框 |
| `ui.set_text_input_text(e, text)` | entity, string | — | 设置输入框文本 |
| `ui.get_text_input_text(e)` | entity | `string` | 获取输入框文本 |
| `ui.set_text_input_placeholder(e, placeholder)` | entity, string | — | 设置占位文本 |
| `ui.set_text_input_focus(e, focused)` | entity, bool | — | 设置输入框焦点 |
| `ui.add_slider(e, [min], [max], [value], [whole_numbers])` | entity, [float=0], [float=1], [float=0], [bool] | — | 添加滑块 |
| `ui.set_slider_value(e, value)` | entity, float | — | 设置滑块值 |
| `ui.get_slider_value(e)` | entity | `number` | 获取滑块值 |
| `ui.set_slider_range(e, min, max)` | entity, float, float | — | 设置滑块范围 |
| `ui.set_slider_vertical(e, vertical)` | entity, bool | — | 设置滑块为竖向 |
| `ui.set_slider_handle_size(e, size)` | entity, float | — | 设置滑块手柄尺寸 |
| `ui.set_slider_colors(e, track_r,g,b,a, fill_r,g,b,a, handle_r,g,b,a)` | entity, float×12 | — | 设置轨道/填充/手柄颜色 |
| `ui.add_toggle(e, is_on, [group])` | entity, bool, [int=-1] | — | 添加开关（可选单选组） |
| `ui.set_toggle(e, is_on)` | entity, bool | — | 设置开关状态 |
| `ui.get_toggle(e)` | entity | `bool` | 获取开关状态 |
| `ui.add_progress_bar(e, [value], [max_value])` | entity, [float=0], [float=1] | — | 添加进度条 |
| `ui.set_progress(e, value)` | entity, float | — | 设置进度值 |
| `ui.get_progress(e)` | entity | `number` | 获取进度值 |
| `ui.add_dropdown(e, [item_height], [max_visible_items])` | entity, [float=40], [int=5] | — | 添加下拉框 |
| `ui.dropdown_add_option(e, text, [value])` | entity, string, [string] | — | 添加下拉选项 |
| `ui.dropdown_clear_options(e)` | entity | — | 清空下拉选项 |
| `ui.set_dropdown_index(e, index)` | entity, int | — | 设置选中索引 |
| `ui.get_dropdown_index(e)` | entity | `int` | 获取选中索引 |
| `ui.get_dropdown_value(e)` | entity | `string` | 获取选中项文本 |
| `ui.set_dropdown_open(e, open)` | entity, bool | — | 展开/收起下拉框 |

### 滚动与布局扩展

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ui.add_scroll_view(e, [content_w], [content_h], [horizontal], [vertical])` | entity, float, float, bool, bool | — | 添加滚动视图 |
| `ui.set_scroll_offset(e, x, y)` | entity, float, float | — | 设置滚动偏移 |
| `ui.get_scroll_offset(e)` | entity | `x, y` | 获取滚动偏移 |
| `ui.set_scroll_content_size(e, w, h)` | entity, float, float | — | 设置滚动内容尺寸 |
| `ui.add_virtual_scroll(e, total_count, [item_height])` | entity, int, [float=50] | — | 添加虚拟滚动列表（大量项复用） |
| `ui.set_virtual_scroll_count(e, count)` | entity, int | — | 设置虚拟滚动总项数 |
| `ui.get_virtual_scroll_range(e)` | entity | `first, last` | 获取当前可见项索引范围 |
| `ui.destroy_virtual_scroll(e)` | entity | — | 销毁虚拟滚动列表 |
| `ui.add_box_layout(e, vertical, spacing, [pad_x], [pad_y], [align_main], [align_cross], [reverse])` | entity, bool, float..., int, int, bool | — | 添加线性盒布局 |
| `ui.set_box_layout(e, vertical, spacing, pad_x, pad_y, align_main, align_cross, reverse)` | entity, bool, float..., int, int, bool | — | 修改盒布局参数 |
| `ui.add_content_size_fitter(e, fit_w, fit_h, [min_w], [min_h], [max_w], [max_h])` | entity, int, int, float... | — | 添加内容自适应尺寸组件 |
| `ui.set_label_font(e, font_id, [font_size])` | entity, string, [float] | — | 设置标签字体 |
| `ui.set_label_layout(e, max_width, [align], [overflow], [max_lines], [line_spacing])` | entity, float, int..., float | — | 设置标签排版（换行/对齐/溢出） |

### 填充图 / 焦点导航 / 视觉效果 / 事件传递 / 预制加载

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ui.add_filled_image(e, [fill_amount], [fill_method], [fill_origin], [clockwise])` | entity, [float=1], [int=0], [int=0], bool | — | 添加填充图（血条/冷却） |
| `ui.set_fill_amount(e, amount)` | entity, float | — | 设置填充量（0~1） |
| `ui.get_fill_amount(e)` | entity | `number` | 获取填充量 |
| `ui.set_fill_method(e, method, origin, clockwise)` | entity, int, int, bool | — | 设置填充方式与起点 |
| `ui.add_focus_navigable(e, [tab_index])` | entity, [int=0] | — | 添加焦点导航（手柄/键盘） |
| `ui.set_focus_nav(e, up, down, left, right)` | entity, int×4 | — | 设置四方向焦点邻居实体 |
| `ui.is_focused(e)` | entity | `bool` | 查询是否拥有焦点 |
| `ui.set_focus_tint(e, r, g, b, [a])` | entity, float..., [float=1] | — | 设置焦点高亮色 |
| `ui.add_visual_effect(e)` | entity | — | 添加视觉效果组件（圆角/渐变/模糊） |
| `ui.set_corner_radius(e, radius)` | entity, float | — | 设置圆角半径 |
| `ui.set_gradient(e, r1,g1,b1,[a1], r2,g2,b2,[a2], [direction])` | entity, float..., [int=1] | — | 设置双色渐变 |
| `ui.set_blur(e, strength, [downsample])` | entity, float, [float=1] | — | 设置背景模糊 |
| `ui.add_event_propagation(e, block, stop)` | entity, bool, bool | — | 添加事件传递控制 |
| `ui.stop_propagation(e)` | entity | — | 阻止事件向上冒泡 |
| `ui.load_from_json(json_string)` | string | `table` | 从 JSON 字符串加载 UI，返回实体 id 数组 |
| `ui.load_from_file(file_path)` | string | `table` | 从文件加载 UI，返回实体 id 数组 |

**示例：**
```lua
local btn = dse.ecs.create_entity()
dse.ecs.add_transform(btn, 400, 300, 0)
dse.ui.add_renderer(btn, 0, 0.2, 0.6, 1.0, 1.0, 10, 200, 60)
dse.ui.add_button(btn, 0.2, 0.6, 1.0, 1.0)
dse.ui.set_position(btn, 400, 300)
dse.ui.set_color(btn, 1.0, 1.0, 1.0, 0.8)
```

---

## 11. dssl — DSSL 材质系统

DSSL（DS Shader Language）材质系统，挂载在独立全局表 `dssl` 下（不是 `dse.dssl`）。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `dssl.load_material(path)` | `string` | `bool` | 加载 .dssl 材质文件 |
| `dssl.create_instance(mat_name)` | `string` | `int` | 创建材质实例，返回实例 ID |
| `dssl.set_float(instance_id, name, value)` | int, string, float | — | 设置 float 参数 |
| `dssl.set_color(instance_id, name, r, g, b, [a])` | int, string, float... | — | 设置颜色参数 |
| `dssl.set_vec3(instance_id, name, x, y, z)` | int, string, float... | — | 设置 vec3 参数 |
| `dssl.set_texture(instance_id, name, tex_path)` | int, string, string | — | 绑定纹理（从路径加载） |
| `dssl.set_texture_handle(instance_id, name, handle)` | int, string, int | — | 绑定纹理（使用已有句柄） |
| `dssl.apply_material(e, mat_index, dssl_path_or_instance)` | entity, int, string/int | — | 应用 DSSL 材质到 MeshRenderer |
| `dssl.get_float(instance_id, name)` | int, string | `number` | 获取 float 参数 |
| `dssl.get_color(instance_id, name)` | int, string | `r, g, b, a` | 获取颜色参数 |

**示例：**
```lua
dssl.load_material("shaders/dssl/half_lambert_kf.dssl")
local inst = dssl.create_instance("half_lambert_kf")
dssl.set_float(inst, "roughness", 0.5)
dssl.set_color(inst, "base_color", 0.8, 0.2, 0.2, 1.0)
dssl.apply_material(entity, 0, inst)
```

---

## 12. nav — NavMesh 导航系统

> **条件编译：** 仅在 `DSE_ENABLE_NAVMESH` 启用时可用。挂载在独立全局表 `nav` 下。

### 全局 nav 表

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `nav.is_ready()` | — | `bool` | NavMesh 是否已加载就绪 |
| `nav.load(path)` | `string` | `bool` | 加载 .navmesh 文件 |
| `nav.save(path)` | `string` | `bool` | 保存 .navmesh 文件 |
| `nav.find_path(sx,sy,sz, ex,ey,ez)` | `float...` | `table` / `nil` | 寻路，返回路径点 `{{x,y,z}, ...}` |
| `nav.find_nearest(x,y,z)` | `float...` | `x,y,z` / `nil` | 查找最近 navmesh 上的点 |
| `nav.raycast(sx,sy,sz, ex,ey,ez)` | `float...` | `hit, hx, hy, hz` | NavMesh 射线检测 |
| `nav.bake(verts_flat, tris_flat, [config])` | `table, table, [table]` | `bool` | 从三角面烘焙 navmesh |

**bake config 字段（可选）：** `cell_size`, `cell_height`, `agent_height`, `agent_radius`, `agent_max_climb`, `agent_max_slope`

### ECS NavAgent 扩展（注入 `dse.ecs`）

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.set_nav_agent(e, config)` | entity, table | — | 添加/设置导航代理。config: `{speed, acceleration, stopping_dist, radius, height}` |
| `ecs.set_nav_destination(e, x, y, z)` | entity, float... | — | 设置导航目标位置 |
| `ecs.nav_agent_arrived(e)` | entity | `bool` | 查询是否到达目标 |
| `ecs.get_nav_agent(e)` | entity | `table` / nil | 获取导航代理配置（无则 nil） |
| `ecs.get_nav_destination(e)` | entity | `x, y, z` | 获取导航目标位置 |
| `ecs.nav_agent_has_path(e)` | entity | `bool` | 查询是否已规划出路径 |

**`ecs.get_nav_agent(e)` 返回表字段：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `speed` | number | 移动速度 |
| `acceleration` | number | 加速度 |
| `stopping_dist` | number | 到点停止距离 |
| `radius` | number | 代理半径 |
| `height` | number | 代理高度 |
| `dest_x` / `dest_y` / `dest_z` | number | 当前目标位置 |
| `has_path` | bool | 是否已规划出路径（与 `nav_agent_has_path` 一致） |
| `path_pending` | bool | 是否正在等待重新规划路径 |
| `arrived` | bool | 是否已到达目标 |
| `current_waypoint` | integer | 当前路径点索引（从 0 开始） |

**示例：**
```lua
nav.load("data/navmesh/level01.navmesh")
local npc = dse.ecs.create_entity()
dse.ecs.add_transform(npc, 0, 0, 0)
dse.ecs.set_nav_agent(npc, { speed = 5, radius = 0.5, height = 1.8 })
dse.ecs.set_nav_destination(npc, 10, 0, 20)
```

---

## 13. streaming — 资源流式加载

挂载在独立全局表 `streaming` 下。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `streaming.create_zone(name, cx, cy, cz, load_radius, [unload_radius])` | string, float... | `zone_id` | 创建流式加载区域 |
| `streaming.destroy_zone(zone_id)` | `int` | — | 销毁区域 |
| `streaming.add_asset(zone_id, path, [type])` | int, string, [string="texture"] | — | 添加单个资源到区域 |
| `streaming.add_assets(zone_id, paths, [type])` | int, table, [string] | — | 批量添加资源到区域 |
| `streaming.set_zone_center(zone_id, cx, cy, cz)` | int, float... | — | 更新区域中心位置 |
| `streaming.force_load(zone_id)` | `int` | — | 强制加载区域 |
| `streaming.force_unload(zone_id)` | `int` | — | 强制卸载区域 |
| `streaming.get_zone_state(zone_id)` | `int` | `string` | 获取状态：`"unloaded"` / `"loading"` / `"loaded"` / `"unloading"` |
| `streaming.get_zone_progress(zone_id)` | `int` | `number` | 获取加载进度 [0.0, 1.0] |
| `streaming.set_budget(per_frame, [max_concurrent])` | int, [int=32] | — | 设置每帧加载预算 |
| `streaming.get_active_loads()` | — | `int` | 获取当前活跃加载数 |
| `streaming.get_zone_count()` | — | `int` | 获取总区域数 |

**资源类型（type 参数）：** `"texture"`, `"mesh"`, `"animation"`, `"skeleton"`, `"audio"`, `"material"`

**示例：**
```lua
local zone = streaming.create_zone("town", 0, 0, 0, 100, 150)
streaming.add_assets(zone, {
    "data/textures/town_ground.png",
    "data/textures/town_wall.png",
}, "texture")
streaming.add_asset(zone, "data/models/house.obj", "mesh")
streaming.force_load(zone)
print(streaming.get_zone_state(zone))  -- "loading" or "loaded"
```

---

## 14. dse.font — 字体服务

挂载在 `dse.font` 下；底层走 `render::FontService`。无字体服务时各函数返回安全默认值（`false`/`0`）。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `dse.font.load(font_id, ttf_path)` | string, string | `bool` | 加载 TTF 字体并以 `font_id` 注册 |
| `dse.font.load_cjk(font_id, ttf_path)` | string, string | `bool` | 同上，并自动追加 ~800 高频汉字 + 常用标点（临时用 4096² 图集） |
| `dse.font.unload(font_id)` | string | — | 卸载字体 |
| `dse.font.set_default(font_id)` | string | `bool` | 设为默认字体 |
| `dse.font.measure(text [, font_id [, font_size]])` | string, [string=""], [number=0] | `number` | 文本宽度（`font_id`/`font_size` 省略用默认） |
| `dse.font.line_height([font_id [, font_size]])` | [string=""], [number=0] | `number` | 行高 |
| `dse.font.get_texture([font_id])` | [string=""] | `int` | 字形图集的 GPU 纹理句柄（省略用默认字体） |

```lua
dse.font.load_cjk("main", "data/fonts/NotoSansSC-Regular.ttf")
dse.font.set_default("main")
local w = dse.font.measure("你好 World", "main", 24)
local h = dse.font.line_height("main", 24)
```

---

## 15. dse.serialize — 二进制序列化

挂载在 `dse.serialize` 下。自描述紧凑二进制编解码器，把任意 Lua 值（含嵌套表）一行编码、一行解码，
省去手动拼/解字节。纯 Lua/C 无外部依赖，**始终可用**（不受 NET/HTTP 开关影响）。典型用途是配合 `dse.net` 收发结构化消息。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `dse.serialize.encode(value)` | any | `string` | 编码任意支持的值为二进制串 |
| `dse.serialize.decode(string [, pos])` | string, [int=1] | `value, next_pos` | 从 `pos`(1 基) 解码一个值，返回值与下一字节位置 |

- **支持类型**：`nil` / `boolean` / `integer` / `number`(double) / `string`(二进制安全) / `table`(数组或字典，可任意嵌套)。
- **不支持**：`function` / `userdata` / `thread`（编码报错）。表嵌套上限 **100** 层（防环引用）。
- `integer` 与 `number` 子类型分别保留，往返后 `math.type` 不变。
- `decode` 数据被截断/损坏时报错（可用 `pcall` 捕获）；返回的 `next_pos` 可用于顺序解码拼接的多个值。

```lua
local s = dse.serialize.encode({ cmd="move", x=100, y=200.5, items={"sword","shield"} })
dse.net.send(conn, s, dse.net.RELIABLE, 0)
-- 接收端
local msg, next_pos = dse.serialize.decode(data)   -- msg.cmd=="move", msg.items[1]=="sword"
```

二进制格式（小端，标签 1 字节）：`0=nil 1=false 2=true`，`3=int`(zigzag LEB128)，`4=number`(8 字节 double)，
`5=string`(ULEB128 长度 + 字节)，`6=array`(ULEB128 个数 + 值)，`7=map`(ULEB128 对数 + 键值对)。

---

## 16. dse.http — 异步 HTTP(S) 客户端（DSE_ENABLE_HTTP）

挂载在 `dse.http` 下；**仅当 `-DDSE_ENABLE_HTTP=ON` 构建时存在**（底层 IXWebSocket + OpenSSL TLS）。
请求在后台线程发出，回调在主线程/脚本线程触发（引擎每帧 `PumpHttp` 自动驱动），不阻塞游戏循环。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `dse.http.request(opts)` | table | `request_id` | 通用请求，`opts` 见下 |
| `dse.http.get(url [, on_done])` | string, [function] | `request_id` | GET |
| `dse.http.post(url, body [, content_type] [, on_done])` | string, string, [string], [function] | `request_id` | POST（`content_type` 省略默认 `application/json`） |
| `dse.http.update()` | — | `int` | 手动触发已完成回调，返回本次完成数（引擎 Tick 亦自动调用） |
| `dse.http.available()` | — | `bool` | 是否编译进真实后端 |

`request(opts)` 的 `opts` 字段：

| 字段 | 类型 | 默认 | 说明 |
|------|------|------|------|
| `url` | string | （必填） | 含 scheme，如 `https://...` |
| `method` | string | `"GET"` | `"GET"`/`"POST"`/... |
| `headers` | table(map) | — | 请求头 `{ ["Authorization"]="Bearer ..." }` |
| `body` | string | — | 请求体 |
| `timeout` | number | `30` | 秒 |
| `verify_peer` | bool | `true` | HTTPS 是否校验证书 |
| `ca_file` | string | — | 自定义 CA bundle 路径 |
| `on_done` | function | — | 完成回调 `function(resp)` |

回调收到的 `resp` 表：`{ id, status, body, error, ok (bool), headers={K=V,...} }`。

```lua
dse.http.request{
    url = "https://api.deepseek.com/chat/completions",
    method = "POST",
    headers = { ["Authorization"] = "Bearer "..key, ["Content-Type"] = "application/json" },
    body = body_json,
    on_done = function(resp)
        if resp.ok then print(resp.status, resp.body) else print("err:", resp.error) end
    end,
}
```

---

## 17. dse.net — 游戏网络传输 (GNS)（DSE_ENABLE_NET）

挂载在 `dse.net` 下；**仅当 `-DDSE_ENABLE_NET=ON` 构建时存在**（底层 GameNetworkingSockets，默认加密）。
游戏专用 UDP 可靠/非可靠传输（客户端↔服务端/状态同步），**不能**用来连 HTTPS REST（那用 `dse.http`）。
进程内单一传输实例；事件经回调在 `poll()` 所在线程同步触发（引擎每帧 `PumpNet` 自动驱动），单线程安全。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `dse.net.init([log_debug])` | [bool=false] | `bool` | 创建并初始化后端（幂等） |
| `dse.net.shutdown()` | — | — | 关闭并销毁，解绑所有回调 |
| `dse.net.available()` | — | `bool` | 是否编译进真实后端 |
| `dse.net.listen(port)` | int | `bool` | 监听端口（服务端） |
| `dse.net.connect(host, port)` | string, int | `conn` | 连接，返回连接句柄（`0`=失败） |
| `dse.net.close(conn [, reason])` | int, [int=CLOSE_NORMAL] | — | 关闭连接 |
| `dse.net.configure_lanes(conn, priorities [, weights])` | int, table, [table] | `bool` | 配置多 lane（防队头阻塞） |
| `dse.net.send(conn, data [, mode] [, lane])` | int, string, [string/int], [int=0] | `bool` | 发送；`mode`：`"reliable"`(默认)/`"unreliable"` 或常量 |
| `dse.net.flush(conn)` | int | — | 立即冲刷（绕过 Nagle 合批） |
| `dse.net.poll()` | — | — | 每帧泵：派发事件回调 |
| `dse.net.get_quality(conn)` | int | `table｜nil` | 连接质量，见下 |
| `dse.net.on(event, fn)` | string, function | — | 注册事件回调 |

事件（`dse.net.on`）：

| event | 回调签名 | 说明 |
|-------|----------|------|
| `"connecting"` | `fn(conn, host, port)` | 正在连接（回传对端地址） |
| `"connected"` | `fn(conn)` | 已连接 |
| `"closed"` | `fn(conn, reason)` | 连接关闭（`reason` 见常量） |
| `"message"` | `fn(conn, data, lane)` | 收到消息（`data` 二进制安全字符串） |

`get_quality(conn)` 返回表：`{ ping_ms, packet_loss, out_bytes_per_sec, in_bytes_per_sec, pending_reliable }`。

**常量**：`dse.net.RELIABLE` / `UNRELIABLE`；`dse.net.CLOSE_NORMAL` / `CLOSE_BY_PEER` / `CLOSE_PROBLEM` / `CLOSE_REJECTED`。

```lua
assert(dse.net.init(false))
dse.net.on("message", function(conn, data, lane)
    local msg = dse.serialize.decode(data)   -- 配合 dse.serialize
end)
dse.net.listen(27620)
local conn = dse.net.connect("127.0.0.1", 27620)
dse.net.on("connected", function(c)
    dse.net.send(c, dse.serialize.encode({hello="world"}), dse.net.RELIABLE, 0)
end)
-- 每帧：dse.net.poll()（引擎宿主自动驱动）
```

> 完整示例：`examples/lua/net_loopback.lua`、`examples/lua/net_message.lua`（net + serialize）、
> `examples/lua/deepseek_npc.lua`（http + AI NPC）。

## 18. ECS 组件字段访问器（Codegen 自动生成）

> 本节由 `tools/codegen/binding_defs.json` 自动派生，对应 `engine/scripting/lua/bindings/lua_binding_ecs_*.gen.cpp`。
> 这些是逐字段的底层 getter/setter，统一注册在 `dse.ecs` 表下，与上文高层封装函数互补。
> 调用约定：getter 形如 `ecs.get_<prefix>_<field>(e)`，setter 形如 `ecs.set_<prefix>_<field>(e, ...)`。

> 合计：13 个组件，330 个访问器函数。


### 18.1 TransformComponent — 前缀 `transform`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `position` | vec3 | `ecs.get_transform_position(e)` → x, y, z | `ecs.set_transform_position(e, x, y, z)` | `—` |
| `rotation` | euler_quat | `ecs.get_transform_rotation(e)` → x, y, z | `ecs.set_transform_rotation(e, x, y, z)` | `—` |
| `scale` | vec3 | `ecs.get_transform_scale(e)` → x, y, z | `ecs.set_transform_scale(e, x, y, z)` | `—` |

### 18.2 Camera3DComponent — 前缀 `camera3d`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `fov` | float | `ecs.get_camera3d_fov(e)` → number | `ecs.set_camera3d_fov(e, value:number)` | `60.0f` |
| `near_clip` | float | `ecs.get_camera3d_near_clip(e)` → number | `ecs.set_camera3d_near_clip(e, value:number)` | `0.1f` |
| `far_clip` | float | `ecs.get_camera3d_far_clip(e)` → number | `ecs.set_camera3d_far_clip(e, value:number)` | `1000.0f` |
| `enabled` | bool | `ecs.get_camera3d_enabled(e)` → boolean | `ecs.set_camera3d_enabled(e, value:bool)` | `true` |
| `priority` | int | `ecs.get_camera3d_priority(e)` → integer | `ecs.set_camera3d_priority(e, value:int)` | `0` |

### 18.3 MeshRendererComponent — 前缀 `mesh_renderer`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `color` | vec4 | `ecs.get_mesh_color(e)` → x, y, z, w | `ecs.set_mesh_color(e, x, y, z, w)` | `—` |
| `visible` | bool | `ecs.get_mesh_visible(e)` → boolean | `ecs.set_mesh_visible(e, value:bool)` | `true` |
| `metallic` | float | `ecs.get_mesh_metallic(e)` → number | `ecs.set_mesh_metallic(e, value:number)` | `0.0f` |
| `roughness` | float | `ecs.get_mesh_roughness(e)` → number | `ecs.set_mesh_roughness(e, value:number)` | `0.5f` |
| `emissive` | vec3 | `ecs.get_mesh_emissive(e)` → x, y, z | `ecs.set_mesh_emissive(e, x, y, z)` | `—` |
| `receive_shadow` | bool | `ecs.get_mesh_receive_shadow(e)` → boolean | `ecs.set_mesh_receive_shadow(e, value:bool)` | `true` |
| `mesh_path` | string | `ecs.get_mesh_path(e)` → string | `ecs.set_mesh_path(e, value:string)` | `—` |
| `shader_variant` | string | `ecs.get_mesh_shader_variant(e)` → string | `ecs.set_mesh_shader_variant(e, value:string)` | `—` |

### 18.4 DirectionalLight3DComponent — 前缀 `dir_light`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `direction` | vec3 | `ecs.get_dir_light_direction(e)` → x, y, z | `ecs.set_dir_light_direction(e, x, y, z)` | `—` |
| `color` | vec3 | `ecs.get_dir_light_color(e)` → x, y, z | `ecs.set_dir_light_color(e, x, y, z)` | `—` |
| `intensity` | float | `ecs.get_dir_light_intensity(e)` → number | `ecs.set_dir_light_intensity(e, value:number)` | `1.0f` |
| `ambient_intensity` | float | `ecs.get_dir_light_ambient(e)` → number | `ecs.set_dir_light_ambient(e, value:number)` | `0.2f` |
| `cast_shadow` | bool | `ecs.get_dir_light_cast_shadow(e)` → boolean | `ecs.set_dir_light_cast_shadow(e, value:bool)` | `true` |
| `shadow_strength` | float | `ecs.get_dir_light_shadow_strength(e)` → number | `ecs.set_dir_light_shadow_strength(e, value:number)` | `0.35f` |
| `enabled` | bool | `ecs.get_dir_light_enabled(e)` → boolean | `ecs.set_dir_light_enabled(e, value:bool)` | `true` |

### 18.5 PointLightComponent — 前缀 `point_light`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `color` | vec3 | `ecs.get_point_light_color(e)` → x, y, z | `ecs.set_point_light_color(e, x, y, z)` | `—` |
| `intensity` | float | `ecs.get_point_light_intensity(e)` → number | `ecs.set_point_light_intensity(e, value:number)` | `1.0f` |
| `radius` | float | `ecs.get_point_light_radius(e)` → number | `ecs.set_point_light_radius(e, value:number)` | `10.0f` |
| `enabled` | bool | `ecs.get_point_light_enabled(e)` → boolean | `ecs.set_point_light_enabled(e, value:bool)` | `true` |
| `cast_shadow` | bool | `ecs.get_point_light_cast_shadow(e)` → boolean | `ecs.set_point_light_cast_shadow(e, value:bool)` | `false` |

### 18.6 SpotLightComponent — 前缀 `spot_light`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `color` | vec3 | `ecs.get_spot_light_color(e)` → x, y, z | `ecs.set_spot_light_color(e, x, y, z)` | `—` |
| `intensity` | float | `ecs.get_spot_light_intensity(e)` → number | `ecs.set_spot_light_intensity(e, value:number)` | `1.0f` |
| `radius` | float | `ecs.get_spot_light_radius(e)` → number | `ecs.set_spot_light_radius(e, value:number)` | `20.0f` |
| `inner_cone_angle` | float | `ecs.get_spot_light_inner_cone_angle(e)` → number | `ecs.set_spot_light_inner_cone_angle(e, value:number)` | `12.5f` |
| `outer_cone_angle` | float | `ecs.get_spot_light_outer_cone_angle(e)` → number | `ecs.set_spot_light_outer_cone_angle(e, value:number)` | `17.5f` |
| `direction` | vec3 | `ecs.get_spot_light_direction(e)` → x, y, z | `ecs.set_spot_light_direction(e, x, y, z)` | `—` |
| `enabled` | bool | `ecs.get_spot_light_enabled(e)` → boolean | `ecs.set_spot_light_enabled(e, value:bool)` | `true` |
| `cast_shadow` | bool | `ecs.get_spot_light_cast_shadow(e)` → boolean | `ecs.set_spot_light_cast_shadow(e, value:bool)` | `false` |

### 18.7 SkyLightComponent — 前缀 `sky_light`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `up_color` | vec3 | `ecs.get_sky_light_up_color(e)` → x, y, z | `ecs.set_sky_light_up_color(e, x, y, z)` | `—` |
| `down_color` | vec3 | `ecs.get_sky_light_down_color(e)` → x, y, z | `ecs.set_sky_light_down_color(e, x, y, z)` | `—` |
| `intensity` | float | `ecs.get_sky_light_intensity(e)` → number | `ecs.set_sky_light_intensity(e, value:number)` | `1.0f` |
| `enabled` | bool | `ecs.get_sky_light_enabled(e)` → boolean | `ecs.set_sky_light_enabled(e, value:bool)` | `true` |

### 18.8 TreeComponent — 前缀 `tree`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `enabled` | bool | `ecs.get_tree_enabled(e)` → boolean | `ecs.set_tree_enabled(e, value:bool)` | `true` |
| `density` | float | `ecs.get_tree_density(e)` → number | `ecs.set_tree_density(e, value:number)` | `0.02f` |
| `spawn_radius` | float | `ecs.get_tree_spawn_radius(e)` → number | `ecs.set_tree_spawn_radius(e, value:number)` | `120.0f` |
| `chunk_size` | float | `ecs.get_tree_chunk_size(e)` → number | `ecs.set_tree_chunk_size(e, value:number)` | `32.0f` |
| `min_scale` | float | `ecs.get_tree_min_scale(e)` → number | `ecs.set_tree_min_scale(e, value:number)` | `0.8f` |
| `max_scale` | float | `ecs.get_tree_max_scale(e)` → number | `ecs.set_tree_max_scale(e, value:number)` | `1.3f` |
| `lod1_distance` | float | `ecs.get_tree_lod1_distance(e)` → number | `ecs.set_tree_lod1_distance(e, value:number)` | `60.0f` |
| `cull_distance` | float | `ecs.get_tree_cull_distance(e)` → number | `ecs.set_tree_cull_distance(e, value:number)` | `200.0f` |
| `wind_strength` | float | `ecs.get_tree_wind_strength(e)` → number | `ecs.set_tree_wind_strength(e, value:number)` | `0.3f` |
| `wind_speed` | float | `ecs.get_tree_wind_speed(e)` → number | `ecs.set_tree_wind_speed(e, value:number)` | `1.0f` |
| `cast_shadow` | bool | `ecs.get_tree_cast_shadow(e)` → boolean | `ecs.set_tree_cast_shadow(e, value:bool)` | `true` |
| `shadow_distance` | float | `ecs.get_tree_shadow_distance(e)` → number | `ecs.set_tree_shadow_distance(e, value:number)` | `80.0f` |
| `seed` | int | `ecs.get_tree_seed(e)` → integer | `ecs.set_tree_seed(e, value:int)` | `12345` |
| `height_variation` | float | `ecs.get_tree_height_variation(e)` → number | `ecs.set_tree_height_variation(e, value:number)` | `0.2f` |
| `random_rotation` | bool | `ecs.get_tree_random_rotation(e)` → boolean | `ecs.set_tree_random_rotation(e, value:bool)` | `true` |
| `billboard_distance` | float | `ecs.get_tree_billboard_distance(e)` → number | `ecs.set_tree_billboard_distance(e, value:number)` | `150.0f` |
| `mesh_path` | string | `ecs.get_tree_mesh_path(e)` → string | `ecs.set_tree_mesh_path(e, value:string)` | `—` |
| `lod1_mesh_path` | string | `ecs.get_tree_lod1_mesh_path(e)` → string | `ecs.set_tree_lod1_mesh_path(e, value:string)` | `—` |
| `billboard_texture_path` | string | `ecs.get_tree_billboard_texture_path(e)` → string | `ecs.set_tree_billboard_texture_path(e, value:string)` | `—` |

### 18.9 TerrainTileManagerComponent — 前缀 `terrain_tile`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `enabled` | bool | `ecs.get_terrain_tile_enabled(e)` → boolean | `ecs.set_terrain_tile_enabled(e, value:bool)` | `true` |
| `tile_world_size` | float | `ecs.get_terrain_tile_world_size(e)` → number | `ecs.set_terrain_tile_world_size(e, value:number)` | `64.0f` |
| `tile_resolution` | int | `ecs.get_terrain_tile_resolution(e)` → integer | `ecs.set_terrain_tile_resolution(e, value:int)` | `64` |
| `max_height` | float | `ecs.get_terrain_tile_max_height(e)` → number | `ecs.set_terrain_tile_max_height(e, value:number)` | `20.0f` |
| `load_radius` | float | `ecs.get_terrain_tile_load_radius(e)` → number | `ecs.set_terrain_tile_load_radius(e, value:number)` | `200.0f` |
| `unload_radius` | float | `ecs.get_terrain_tile_unload_radius(e)` → number | `ecs.set_terrain_tile_unload_radius(e, value:number)` | `250.0f` |
| `use_procedural` | bool | `ecs.get_terrain_tile_use_procedural(e)` → boolean | `ecs.set_terrain_tile_use_procedural(e, value:bool)` | `true` |
| `procedural_base_height` | float | `ecs.get_terrain_tile_procedural_base_height(e)` → number | `ecs.set_terrain_tile_procedural_base_height(e, value:number)` | `0.0f` |
| `max_lod_levels` | int | `ecs.get_terrain_tile_max_lod_levels(e)` → integer | `ecs.set_terrain_tile_max_lod_levels(e, value:int)` | `4` |
| `lod_distance_factor` | float | `ecs.get_terrain_tile_lod_distance_factor(e)` → number | `ecs.set_terrain_tile_lod_distance_factor(e, value:number)` | `50.0f` |

### 18.10 DynamicObstacleComponent — 前缀 `dyn_obstacle`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `enabled` | bool | `ecs.get_dyn_obstacle_enabled(e)` → boolean | `ecs.set_dyn_obstacle_enabled(e, value:bool)` | `true` |
| `shape` | enum_int | `ecs.get_dyn_obstacle_shape(e)` → integer | `ecs.set_dyn_obstacle_shape(e, value:int)` | `0` |
| `box_extents` | vec3 | `ecs.get_dyn_obstacle_box_extents(e)` → x, y, z | `ecs.set_dyn_obstacle_box_extents(e, x, y, z)` | `—` |
| `cylinder_radius` | float | `ecs.get_dyn_obstacle_cylinder_radius(e)` → number | `ecs.set_dyn_obstacle_cylinder_radius(e, value:number)` | `1.0f` |
| `cylinder_height` | float | `ecs.get_dyn_obstacle_cylinder_height(e)` → number | `ecs.set_dyn_obstacle_cylinder_height(e, value:number)` | `2.0f` |

### 18.11 NavMeshAutoRebakeComponent — 前缀 `navmesh_rebake`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `enabled` | bool | `ecs.get_navmesh_auto_rebake_enabled(e)` → boolean | `ecs.set_navmesh_auto_rebake_enabled(e, value:bool)` | `true` |
| `tile_size` | float | `ecs.get_navmesh_auto_rebake_tile_size(e)` → number | `ecs.set_navmesh_auto_rebake_tile_size(e, value:number)` | `48.0f` |
| `rebake_cooldown` | float | `ecs.get_navmesh_auto_rebake_cooldown(e)` → number | `ecs.set_navmesh_auto_rebake_cooldown(e, value:number)` | `1.0f` |
| `collect_terrain` | bool | `ecs.get_navmesh_auto_rebake_collect_terrain(e)` → boolean | `ecs.set_navmesh_auto_rebake_collect_terrain(e, value:bool)` | `true` |
| `collect_mesh_renderers` | bool | `ecs.get_navmesh_auto_rebake_collect_mesh_renderers(e)` → boolean | `ecs.set_navmesh_auto_rebake_collect_mesh_renderers(e, value:bool)` | `true` |
| `agent_height` | float | `ecs.get_navmesh_auto_rebake_agent_height(e)` → number | `ecs.set_navmesh_auto_rebake_agent_height(e, value:number)` | `2.0f` |
| `agent_radius` | float | `ecs.get_navmesh_auto_rebake_agent_radius(e)` → number | `ecs.set_navmesh_auto_rebake_agent_radius(e, value:number)` | `0.6f` |
| `agent_max_climb` | float | `ecs.get_navmesh_auto_rebake_agent_max_climb(e)` → number | `ecs.set_navmesh_auto_rebake_agent_max_climb(e, value:number)` | `0.9f` |
| `agent_max_slope` | float | `ecs.get_navmesh_auto_rebake_agent_max_slope(e)` → number | `ecs.set_navmesh_auto_rebake_agent_max_slope(e, value:number)` | `45.0f` |
| `cell_size` | float | `ecs.get_navmesh_auto_rebake_cell_size(e)` → number | `ecs.set_navmesh_auto_rebake_cell_size(e, value:number)` | `0.3f` |
| `cell_height` | float | `ecs.get_navmesh_auto_rebake_cell_height(e)` → number | `ecs.set_navmesh_auto_rebake_cell_height(e, value:number)` | `0.2f` |

### 18.12 PostProcessComponent — 前缀 `post_process`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `enabled` | bool | `ecs.get_post_process_enabled(e)` → boolean | `ecs.set_post_process_enabled(e, value:bool)` | `true` |
| `bloom_enabled` | bool | `ecs.get_post_process_bloom_enabled(e)` → boolean | `ecs.set_post_process_bloom_enabled(e, value:bool)` | `true` |
| `bloom_threshold` | float | `ecs.get_post_process_bloom_threshold(e)` → number | `ecs.set_post_process_bloom_threshold(e, value:number)` | `1.0f` |
| `bloom_intensity` | float | `ecs.get_post_process_bloom_intensity(e)` → number | `ecs.set_post_process_bloom_intensity(e, value:number)` | `0.5f` |
| `bloom_knee` | float | `ecs.get_post_process_bloom_knee(e)` → number | `ecs.set_post_process_bloom_knee(e, value:number)` | `0.1f` |
| `bloom_mip_weight` | float | `ecs.get_post_process_bloom_mip_weight(e)` → number | `ecs.set_post_process_bloom_mip_weight(e, value:number)` | `0.005f` |
| `color_grading_enabled` | bool | `ecs.get_post_process_color_grading_enabled(e)` → boolean | `ecs.set_post_process_color_grading_enabled(e, value:bool)` | `true` |
| `exposure` | float | `ecs.get_post_process_exposure(e)` → number | `ecs.set_post_process_exposure(e, value:number)` | `1.0f` |
| `gamma` | float | `ecs.get_post_process_gamma(e)` → number | `ecs.set_post_process_gamma(e, value:number)` | `2.2f` |
| `ssao_enabled` | bool | `ecs.get_post_process_ssao_enabled(e)` → boolean | `ecs.set_post_process_ssao_enabled(e, value:bool)` | `false` |
| `ssao_radius` | float | `ecs.get_post_process_ssao_radius(e)` → number | `ecs.set_post_process_ssao_radius(e, value:number)` | `0.5f` |
| `ssao_bias` | float | `ecs.get_post_process_ssao_bias(e)` → number | `ecs.set_post_process_ssao_bias(e, value:number)` | `0.025f` |
| `ssao_sample_count` | int | `ecs.get_post_process_ssao_sample_count(e)` → integer | `ecs.set_post_process_ssao_sample_count(e, value:int)` | `32` |
| `ssao_power` | float | `ecs.get_post_process_ssao_power(e)` → number | `ecs.set_post_process_ssao_power(e, value:number)` | `2.0f` |
| `ssao_intensity` | float | `ecs.get_post_process_ssao_intensity(e)` → number | `ecs.set_post_process_ssao_intensity(e, value:number)` | `1.0f` |
| `auto_exposure_enabled` | bool | `ecs.get_post_process_auto_exposure_enabled(e)` → boolean | `ecs.set_post_process_auto_exposure_enabled(e, value:bool)` | `false` |
| `exposure_min` | float | `ecs.get_post_process_exposure_min(e)` → number | `ecs.set_post_process_exposure_min(e, value:number)` | `0.1f` |
| `exposure_max` | float | `ecs.get_post_process_exposure_max(e)` → number | `ecs.set_post_process_exposure_max(e, value:number)` | `10.0f` |
| `adaptation_speed_up` | float | `ecs.get_post_process_adaptation_speed_up(e)` → number | `ecs.set_post_process_adaptation_speed_up(e, value:number)` | `2.0f` |
| `adaptation_speed_down` | float | `ecs.get_post_process_adaptation_speed_down(e)` → number | `ecs.set_post_process_adaptation_speed_down(e, value:number)` | `1.0f` |
| `exposure_compensation` | float | `ecs.get_post_process_exposure_compensation(e)` → number | `ecs.set_post_process_exposure_compensation(e, value:number)` | `0.0f` |
| `color_lut_intensity` | float | `ecs.get_post_process_color_lut_intensity(e)` → number | `ecs.set_post_process_color_lut_intensity(e, value:number)` | `1.0f` |
| `vignette_enabled` | bool | `ecs.get_post_process_vignette_enabled(e)` → boolean | `ecs.set_post_process_vignette_enabled(e, value:bool)` | `false` |
| `vignette_intensity` | float | `ecs.get_post_process_vignette_intensity(e)` → number | `ecs.set_post_process_vignette_intensity(e, value:number)` | `0.35f` |
| `vignette_radius` | float | `ecs.get_post_process_vignette_radius(e)` → number | `ecs.set_post_process_vignette_radius(e, value:number)` | `0.75f` |
| `vignette_softness` | float | `ecs.get_post_process_vignette_softness(e)` → number | `ecs.set_post_process_vignette_softness(e, value:number)` | `0.35f` |
| `film_grain_enabled` | bool | `ecs.get_post_process_film_grain_enabled(e)` → boolean | `ecs.set_post_process_film_grain_enabled(e, value:bool)` | `false` |
| `film_grain_intensity` | float | `ecs.get_post_process_film_grain_intensity(e)` → number | `ecs.set_post_process_film_grain_intensity(e, value:number)` | `0.08f` |
| `film_grain_time_scale` | float | `ecs.get_post_process_film_grain_time_scale(e)` → number | `ecs.set_post_process_film_grain_time_scale(e, value:number)` | `1.0f` |
| `fxaa_enabled` | bool | `ecs.get_post_process_fxaa_enabled(e)` → boolean | `ecs.set_post_process_fxaa_enabled(e, value:bool)` | `true` |
| `taa_enabled` | bool | `ecs.get_post_process_taa_enabled(e)` → boolean | `ecs.set_post_process_taa_enabled(e, value:bool)` | `false` |
| `taa_blend_factor` | float | `ecs.get_post_process_taa_blend_factor(e)` → number | `ecs.set_post_process_taa_blend_factor(e, value:number)` | `0.1f` |
| `contact_shadow_enabled` | bool | `ecs.get_post_process_contact_shadow_enabled(e)` → boolean | `ecs.set_post_process_contact_shadow_enabled(e, value:bool)` | `false` |
| `contact_shadow_strength` | float | `ecs.get_post_process_contact_shadow_strength(e)` → number | `ecs.set_post_process_contact_shadow_strength(e, value:number)` | `0.5f` |
| `contact_shadow_steps` | int | `ecs.get_post_process_contact_shadow_steps(e)` → integer | `ecs.set_post_process_contact_shadow_steps(e, value:int)` | `16` |
| `contact_shadow_step_size` | float | `ecs.get_post_process_contact_shadow_step_size(e)` → number | `ecs.set_post_process_contact_shadow_step_size(e, value:number)` | `0.5f` |
| `dof_enabled` | bool | `ecs.get_post_process_dof_enabled(e)` → boolean | `ecs.set_post_process_dof_enabled(e, value:bool)` | `false` |
| `dof_focus_distance` | float | `ecs.get_post_process_dof_focus_distance(e)` → number | `ecs.set_post_process_dof_focus_distance(e, value:number)` | `100.0f` |
| `dof_focus_range` | float | `ecs.get_post_process_dof_focus_range(e)` → number | `ecs.set_post_process_dof_focus_range(e, value:number)` | `50.0f` |
| `dof_bokeh_radius` | float | `ecs.get_post_process_dof_bokeh_radius(e)` → number | `ecs.set_post_process_dof_bokeh_radius(e, value:number)` | `4.0f` |
| `motion_blur_enabled` | bool | `ecs.get_post_process_motion_blur_enabled(e)` → boolean | `ecs.set_post_process_motion_blur_enabled(e, value:bool)` | `false` |
| `motion_blur_intensity` | float | `ecs.get_post_process_motion_blur_intensity(e)` → number | `ecs.set_post_process_motion_blur_intensity(e, value:number)` | `1.0f` |
| `motion_blur_samples` | int | `ecs.get_post_process_motion_blur_samples(e)` → integer | `ecs.set_post_process_motion_blur_samples(e, value:int)` | `8` |
| `ssr_enabled` | bool | `ecs.get_post_process_ssr_enabled(e)` → boolean | `ecs.set_post_process_ssr_enabled(e, value:bool)` | `false` |
| `ssr_max_distance` | float | `ecs.get_post_process_ssr_max_distance(e)` → number | `ecs.set_post_process_ssr_max_distance(e, value:number)` | `100.0f` |
| `ssr_thickness` | float | `ecs.get_post_process_ssr_thickness(e)` → number | `ecs.set_post_process_ssr_thickness(e, value:number)` | `0.5f` |
| `ssr_step_size` | float | `ecs.get_post_process_ssr_step_size(e)` → number | `ecs.set_post_process_ssr_step_size(e, value:number)` | `1.0f` |
| `ssr_max_steps` | int | `ecs.get_post_process_ssr_max_steps(e)` → integer | `ecs.set_post_process_ssr_max_steps(e, value:int)` | `64` |
| `ssr_fade_distance` | float | `ecs.get_post_process_ssr_fade_distance(e)` → number | `ecs.set_post_process_ssr_fade_distance(e, value:number)` | `0.2f` |
| `ssr_max_roughness` | float | `ecs.get_post_process_ssr_max_roughness(e)` → number | `ecs.set_post_process_ssr_max_roughness(e, value:number)` | `0.5f` |
| `outline_enabled` | bool | `ecs.get_post_process_outline_enabled(e)` → boolean | `ecs.set_post_process_outline_enabled(e, value:bool)` | `false` |
| `outline_color` | vec3 | `ecs.get_post_process_outline_color(e)` → x, y, z | `ecs.set_post_process_outline_color(e, x, y, z)` | `—` |
| `outline_thickness` | float | `ecs.get_post_process_outline_thickness(e)` → number | `ecs.set_post_process_outline_thickness(e, value:number)` | `1.0f` |
| `outline_depth_threshold` | float | `ecs.get_post_process_outline_depth_threshold(e)` → number | `ecs.set_post_process_outline_depth_threshold(e, value:number)` | `0.1f` |
| `outline_normal_threshold` | float | `ecs.get_post_process_outline_normal_threshold(e)` → number | `ecs.set_post_process_outline_normal_threshold(e, value:number)` | `0.4f` |
| `light_shaft_enabled` | bool | `ecs.get_post_process_light_shaft_enabled(e)` → boolean | `ecs.set_post_process_light_shaft_enabled(e, value:bool)` | `false` |
| `light_shaft_color` | vec3 | `ecs.get_post_process_light_shaft_color(e)` → x, y, z | `ecs.set_post_process_light_shaft_color(e, x, y, z)` | `—` |
| `light_shaft_density` | float | `ecs.get_post_process_light_shaft_density(e)` → number | `ecs.set_post_process_light_shaft_density(e, value:number)` | `0.84f` |
| `light_shaft_weight` | float | `ecs.get_post_process_light_shaft_weight(e)` → number | `ecs.set_post_process_light_shaft_weight(e, value:number)` | `0.04f` |
| `light_shaft_decay` | float | `ecs.get_post_process_light_shaft_decay(e)` → number | `ecs.set_post_process_light_shaft_decay(e, value:number)` | `0.97f` |
| `light_shaft_exposure` | float | `ecs.get_post_process_light_shaft_exposure(e)` → number | `ecs.set_post_process_light_shaft_exposure(e, value:number)` | `0.4f` |
| `light_shaft_intensity` | float | `ecs.get_post_process_light_shaft_intensity(e)` → number | `ecs.set_post_process_light_shaft_intensity(e, value:number)` | `1.0f` |
| `light_shaft_samples` | int | `ecs.get_post_process_light_shaft_samples(e)` → integer | `ecs.set_post_process_light_shaft_samples(e, value:int)` | `64` |
| `fog_enabled` | bool | `ecs.get_post_process_fog_enabled(e)` → boolean | `ecs.set_post_process_fog_enabled(e, value:bool)` | `false` |
| `fog_color` | vec3 | `ecs.get_post_process_fog_color(e)` → x, y, z | `ecs.set_post_process_fog_color(e, x, y, z)` | `—` |
| `fog_density` | float | `ecs.get_post_process_fog_density(e)` → number | `ecs.set_post_process_fog_density(e, value:number)` | `0.02f` |
| `fog_height_falloff` | float | `ecs.get_post_process_fog_height_falloff(e)` → number | `ecs.set_post_process_fog_height_falloff(e, value:number)` | `0.3f` |
| `fog_height_offset` | float | `ecs.get_post_process_fog_height_offset(e)` → number | `ecs.set_post_process_fog_height_offset(e, value:number)` | `0.0f` |
| `fog_start` | float | `ecs.get_post_process_fog_start(e)` → number | `ecs.set_post_process_fog_start(e, value:number)` | `0.0f` |
| `fog_end` | float | `ecs.get_post_process_fog_end(e)` → number | `ecs.set_post_process_fog_end(e, value:number)` | `1000.0f` |
| `fog_steps` | int | `ecs.get_post_process_fog_steps(e)` → integer | `ecs.set_post_process_fog_steps(e, value:int)` | `16` |
| `fog_sun_scatter` | float | `ecs.get_post_process_fog_sun_scatter(e)` → number | `ecs.set_post_process_fog_sun_scatter(e, value:number)` | `0.6f` |

### 18.13 Animator3DComponent — 前缀 `animator3d`

| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |
|------|------|--------------|--------------|--------|
| `enabled` | bool | `ecs.get_animator3d_enabled(e)` → boolean | `ecs.set_animator3d_enabled(e, value:bool)` | `true` |
| `danim_path` | string | `ecs.get_animator3d_danim_path(e)` → string | `ecs.set_animator3d_danim_path(e, value:string)` | `—` |
| `dskel_path` | string | `ecs.get_animator3d_dskel_path(e)` → string | `ecs.set_animator3d_dskel_path(e, value:string)` | `—` |
| `speed` | float | `ecs.get_animator3d_speed(e)` → number | `ecs.set_animator3d_speed(e, value:number)` | `1.0f` |
| `loop` | bool | `ecs.get_animator3d_loop(e)` → boolean | `ecs.set_animator3d_loop(e, value:bool)` | `true` |
| `use_anim_tree` | bool | `ecs.get_animator3d_use_anim_tree(e)` → boolean | `ecs.set_animator3d_use_anim_tree(e, value:bool)` | `false` |
| `blend_parameter` | string | `ecs.get_animator3d_blend_parameter(e)` → string | `ecs.set_animator3d_blend_parameter(e, value:string)` | `—` |
| `blend_parameter_value` | float | `ecs.get_animator3d_blend_parameter_value(e)` → number | `ecs.set_animator3d_blend_parameter_value(e, value:number)` | `0.0f` |

---

## 附录：绑定源文件索引

> 注册函数数为实测精确值（由 `tools/audit/lua_api_audit.py` 统计，截至 2026-06-12）。

| 模块 | 源文件 | 注册函数数 |
|------|--------|:------:|
| 全局 + Context | `lua_binding_context.cpp` | 1 |
| app / assets / metrics / origin | `lua_binding_core.cpp` | 41 |
| ecs.core | `lua_binding_ecs_core.cpp` | 15 |
| ecs.rendering.camera | `lua_binding_ecs_rendering_camera.cpp` | 9 |
| ecs.rendering.fx (steering/lod/hair) | `lua_binding_ecs_rendering_fx.cpp` | 14 |
| ecs.rendering.light | `lua_binding_ecs_rendering_light.cpp` | 22 |
| ecs.rendering.mesh | `lua_binding_ecs_rendering_mesh.cpp` | 15 |
| ecs.rendering.post | `lua_binding_ecs_rendering_post.cpp` | 16 |
| ecs.rendering.terrain | `lua_binding_ecs_rendering_terrain.cpp` | 29 |
| ecs.physics2d | `lua_binding_ecs_physics2d.cpp` | 17 |
| ecs.phys3d | `lua_binding_ecs_phys3d.cpp` | 34 |
| ecs.gameplay3d | `lua_binding_ecs_gameplay3d.cpp` | 66 |
| ecs.animation | `lua_binding_ecs_animation.cpp` | 47 |
| ecs.particles | `lua_binding_ecs_particles.cpp` | 16 |
| localization | `lua_binding_localization.cpp` | 6 |
| audio | `lua_binding_audio.cpp` | 33 |
| spine | `lua_binding_spine.cpp` | 2 |
| ui | `lua_binding_ui.cpp` | 88 |
| dssl | `lua_binding_dssl.cpp` | 10 |
| navigation [NavMesh] | `lua_binding_navigation.cpp` | 13 |
| streaming | `lua_binding_streaming.cpp` | 12 |
| font | `lua_binding_font.cpp` | 7 |
| serialize | `lua_binding_serialize.cpp` | 2 |
| http [DSE_ENABLE_HTTP] | `lua_binding_http.cpp` | 5 |
| net [DSE_ENABLE_NET] | `lua_binding_net.cpp` | 12 |
| **Codegen 字段访问器（13 个 `*.gen.cpp`，见 §18）** | `lua_binding_ecs_*.gen.cpp` | 330 |
| ↳ transform / camera3d / mesh_renderer | `_transform` 6 · `_camera3d` 10 · `_mesh_renderer` 16 | — |
| ↳ dir_light / point_light / spot_light / sky_light | `_dir_light` 14 · `_point_light` 10 · `_spot_light` 16 · `_sky_light` 8 | — |
| ↳ tree / terrain_tile / dyn_obstacle / navmesh_rebake | `_tree` 38 · `_terrain_tile` 20 · `_dyn_obstacle` 10 · `_navmesh_rebake` 22 | — |
| ↳ post_process / animator3d | `_post_process` 144 · `_animator3d` 16 | — |
| ecs / registry（入口，不直接注册函数） | `lua_binding_ecs.cpp` / `lua_binding_registry.cpp` | — |
| **合计** | **27 个手写文件 + 13 个 gen** | **862 绑定函数** |

> **关于 LuaSocket（已移除）**：旧的可选 `socket.core` / `mime.core`（LuaSocket）已从引擎移除
> （子模块 + `DSE_ENABLE_LUASOCKET` 开关 + 门控代码）。其能力已被 `dse.net`（游戏 UDP 传输）
> + `dse.http`（HTTPS REST，含 TLS）取代——LuaSocket 自身不支持 HTTPS。
> 注：第三方调试器脚本 `script/lua_panda.lua` 仍以 `pcall` 方式尝试 `require("socket.core")`，
> 失败时自动降级，不影响引擎运行。
