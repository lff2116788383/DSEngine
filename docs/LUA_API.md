# DSEngine Lua API 参考文档

> 自动生成自 `engine/scripting/lua/bindings/` 源码  
> 生成日期：2026-05-07  
> 绑定文件：14 个 C++ 源文件，涵盖 7 个顶层模块

---

## 概览

所有 Lua API 挂载在全局表 `dse` 下，结构如下：

```lua
dse
├── get_memory_usage_kb()        -- 全局工具
├── app.*                        -- 输入 / 窗口 / 时间 / 屏幕
├── assets.*                     -- 资源加载
├── metrics.*                    -- 性能统计
├── ecs.*                        -- ECS 实体 / 组件 / 场景
├── audio.*                      -- 音频系统
├── spine.*                      -- Spine 骨骼动画
└── ui.*                         -- UI 系统
```

### 通用约定

- **entity** — 整数类型的实体 ID，由 `ecs.create_entity()` 返回
- **可选参数** — 用 `[param=default]` 表示，省略时使用默认值
- **vec3 返回值** — 返回 3 个独立的 number：`x, y, z = func(...)`
- **bool 返回值** — Lua 中 `true`/`false`

---

## 目录

1. [全局函数](#1-全局函数)
2. [dse.app — 应用 / 输入](#2-dseapp--应用--输入)
3. [dse.assets — 资源加载](#3-dseassets--资源加载)
4. [dse.metrics — 性能统计](#4-dsemetrics--性能统计)
5. [dse.ecs — 实体组件系统](#5-dseecs--实体组件系统)
   - [5.1 核心](#51-核心)
   - [5.2 Transform](#52-transform)
   - [5.3 Camera](#53-camera)
   - [5.4 Sprite](#54-sprite)
   - [5.5 MeshRenderer](#55-meshrenderer)
   - [5.6 Light](#56-light)
   - [5.7 Skybox](#57-skybox)
   - [5.8 Terrain](#58-terrain)
   - [5.9 PostProcess](#59-postprocess)
   - [5.10 Steering (AI 转向)](#510-steering-ai-转向)
   - [5.11 Physics 2D](#511-physics-2d)
   - [5.12 Physics 3D](#512-physics-3d)
   - [5.13 Animation 2D](#513-animation-2d)
   - [5.14 Animation 3D](#514-animation-3d)
   - [5.15 Particle](#515-particle)
   - [5.16 GameplayTuning](#516-gameplaytuning)
6. [dse.audio — 音频系统](#6-dseaudio--音频系统)
7. [dse.spine — Spine 动画](#7-dsespine--spine-动画)
8. [dse.ui — UI 系统](#8-dseui--ui-系统)

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
| `app.set_data_root(path)` | `string` | — | 设置资源数据根目录 |
| `app.set_window_title(title)` | `string` | — | 设置窗口标题 |
| `app.time_since_startup()` | — | `number` | 自启动以来的秒数 |
| `app.get_screen_width()` | — | `int` | 屏幕宽度（像素） |
| `app.get_screen_height()` | — | `int` | 屏幕高度（像素） |

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
| `app.get_mouse_swipe_dx()` | — | `number` | 当前帧滑动/拖拽 X 轴增量（像素） |
| `app.get_mouse_swipe_dy()` | — | `number` | 当前帧滑动/拖拽 Y 轴增量（像素） |

### 键盘

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `app.get_key(key_code)` | `int` | `bool` | 指定按键是否按住 |
| `app.get_key_down(key_code)` | `int` | `bool` | 指定按键当前帧是否按下 |
| `app.get_key_up(key_code)` | `int` | `bool` | 指定按键当前帧是否松开 |

> **key_code** 使用 GLFW 键码整数值（如 `W=87`, `A=65`, `S=83`, `D=68`, `Space=32`, `Escape=256`）

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

---

## 5. dse.ecs — 实体组件系统

### 5.1 核心

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.create_entity()` | — | `entity` | 创建空实体 |
| `ecs.load_scene(path)` | `string` | `bool, string` | 加载场景文件，返回 (成功?, 错误信息) |
| `ecs.find_entities_by_mesh_path(path)` | `string` | `table` | 查找所有使用指定 mesh 路径的实体 |

**示例：**
```lua
local ok, err = dse.ecs.load_scene("data/scenes/main.dscene")
if not ok then print("Load failed: " .. err) end
```

### 5.2 Transform

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.add_transform(e, x, y, [z], [sx], [sy], [sz])` | `entity, float...` | — | 添加 Transform 组件 |
| `ecs.get_transform_position(e)` | `entity` | `x, y, z` | 获取世界坐标 |
| `ecs.set_transform_position(e, x, y, z)` | `entity, float, float, float` | — | 设置世界坐标（自动标记 dirty） |
| `ecs.set_transform_rotation(e, x, y, z)` | `entity, float, float, float` | — | 设置欧拉角旋转（度数） |

**默认值：** `z=0, sx=1, sy=1, sz=1`

**示例：**
```lua
local e = dse.ecs.create_entity()
dse.ecs.add_transform(e, 0, 5, 0)
dse.ecs.set_transform_rotation(e, 0, 45, 0)  -- Y 轴旋转 45°
local x, y, z = dse.ecs.get_transform_position(e)
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
| `ecs.set_mesh_material(e, dmat_path, [mat_index])` | entity, string, [int=0] | 从 .dmat 文件加载材质实例 |
| `ecs.set_mesh_material(e, metallic, roughness, ao, er, eg, eb, normal_str, ...)` | entity, float... | 直接设置 PBR 材质标量参数 |
| `ecs.set_mesh_shader_variant(e, variant)` | entity, string | 设置着色器变体名 |
| `ecs.set_mesh_depth_state(e, depth_test, [depth_write])` | entity, bool, [bool] | 设置深度测试/写入状态 |
| `ecs.set_mesh_material_scalar(e, name, value)` | entity, string, float | 按名称设置材质标量参数 |
| `ecs.set_mesh_texture(e, slot, path)` | entity, string, string | 加载并绑定贴图到指定槽位 |
| `ecs.set_mesh_uvs(e, uv_table)` | entity, table | 设置 UV 坐标数组 |
| `ecs.set_mesh_normals(e, normal_table)` | entity, table | 设置法线数组 |
| `ecs.set_mesh_tangents(e, tangent_table)` | entity, table | 设置切线数组 |
| `ecs.set_mesh_emissive(e, r, g, b)` | entity, float, float, float | 设置自发光颜色 |

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

**`set_directional_light_shadow` 返回值：** `success, cast_shadow, strength, c0, c1, c2`

#### 点光源

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_point_light_3d(e, [r], [g], [b], [intensity], [radius])` | entity, float... | 1,1,1, 1, 10 | 添加点光源 |
| `ecs.set_point_light_shadow(e, [cast_shadow])` | entity, bool | true | 设置点光阴影 |

#### 聚光灯

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_spot_light_3d(e, [dx], [dy], [dz], [r], [g], [b], [intensity], [radius], [inner], [outer])` | entity, float... | 0,-1,0, 1,1,1, 1, 20, 12.5, 17.5 | 添加聚光灯 |

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

### 5.9 PostProcess

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_post_process(e, bloom_enabled, [threshold], [intensity], [exposure])` | entity, bool, float... | 添加后处理组件 |
| `ecs.set_post_process_bloom(e, [enabled], [bloom], [threshold], [intensity], [exposure])` | entity, bool... | 设置 Bloom 参数 |
| `ecs.set_post_process_color(e, [color_grading], [exposure], [gamma])` | entity, bool, float... | 设置颜色分级参数 |
| `ecs.get_post_process_state(e)` | entity | 见下方 | 获取完整后处理状态 |

**`get_post_process_state` 返回值：**  
`ok, enabled, bloom_on, bloom_threshold, bloom_intensity, color_grading_on, exposure, gamma, ssao_on, ssao_radius, ssao_bias`

**示例：**
```lua
local pp = dse.ecs.create_entity()
dse.ecs.add_post_process(pp, true, 1.0, 1.5, 1.2)
dse.ecs.set_post_process_color(pp, true, 1.0, 2.2)
```

### 5.10 Steering (AI 转向)

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_steering(e, [max_vel], [max_force], [mass])` | entity, float... | 5, 10, 1 | 添加转向组件 |
| `ecs.set_steering_target(e, behavior, tx, ty, tz)` | entity, string, float... | — | 设置转向行为和目标 |
| `ecs.get_steering_state(e)` | entity | 见下方 | 获取完整转向状态 |

**behavior 值：** `"seek"`, `"flee"`, `"arrive"`  
**`set_steering_target` 返回值：** `bool` (成功)

**`get_steering_state` 返回值 (22 个)：**  
`ok, enabled, seek_on, flee_on, arrive_on, vel_x, vel_y, vel_z, speed, max_vel, max_force, mass, arrive_decel_radius, seek_x, seek_y, seek_z, flee_x, flee_y, flee_z, arrive_x, arrive_y, arrive_z`

### 5.11 Physics 2D

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_rigid_body(e, [type], [gravity_scale], [fixed_rotation])` | entity, int, float, int | 2, 1, 0 | 添加 2D 刚体 |
| `ecs.set_rigid_body_velocity(e, vx, vy)` | entity, float, float | — | 设置 2D 刚体速度 |
| `ecs.add_box_collider(e, w, h, [density], [friction], [restitution])` | entity, float... | 1, 0.3, 0 | 添加矩形碰撞体 |
| `ecs.set_box_collider_trigger(e, is_trigger)` | entity, bool | — | 设置碰撞体为触发器 |
| `ecs.add_circle_collider(e, radius, [density], [friction], [restitution])` | entity, float... | 1, 0.3, 0 | 添加圆形碰撞体 |
| `ecs.set_circle_collider_trigger(e, is_trigger)` | entity, bool | — | 设置圆形碰撞体为触发器 |
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

**示例：**
```lua
local player = dse.ecs.create_entity()
dse.ecs.add_transform(player, 0, 5, 0)
dse.ecs.add_rigid_body(player, 2, 1.0, 0)
dse.ecs.add_box_collider(player, 1.0, 1.0, 1.0, 0.3, 0.1)

-- 每帧轮询碰撞事件
local has_event, other, is_trigger, is_enter = dse.ecs.poll_collision_event(player)
if has_event and is_enter then
    print("Collided with entity: " .. other)
end
```

### 5.12 Physics 3D

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_rigidbody_3d(e, [type], [mass])` | entity, int, float | 2, 1 | 添加 3D 刚体 |
| `ecs.add_box_collider_3d(e, x, y, z)` | entity, float... | — | 添加 3D 盒碰撞体 |
| `ecs.add_sphere_collider_3d(e, radius)` | entity, float | — | 添加球碰撞体 |
| `ecs.rigidbody_3d_add_force(e, fx, fy, fz)` | entity, float... | — | 施加持续力（需 PhysX） |
| `ecs.rigidbody_3d_add_impulse(e, ix, iy, iz)` | entity, float... | — | 施加瞬时冲量（需 PhysX） |
| `ecs.rigidbody_3d_set_velocity(e, vx, vy, vz)` | entity, float... | — | 设置线速度 |
| `ecs.rigidbody_3d_get_velocity(e)` | entity | `vx, vy, vz` | 获取线速度 |
| `ecs.rigidbody_3d_set_gravity(e, enabled)` | entity, bool | — | 启用/禁用重力 |
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

**示例：**
```lua
local player = dse.ecs.create_entity()
dse.ecs.add_transform(player, 0, 1, 0)
dse.ecs.add_character_controller_3d(player, 0.3, 1.8, 45, 0.3)

-- 每帧移动
local grounded = dse.ecs.character_controller_3d_is_grounded(player)
local dy = grounded and 0 or -9.8 * dt
dse.ecs.character_controller_3d_move(player, dx, dy, dz)
```

### 5.13 Animation 2D

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_animator(e)` | entity | 添加 2D 动画器 |
| `ecs.add_animation_state(e, name, fps, loop, [frame_handles])` | entity, string, float, bool, [table] | 添加动画状态 |
| `ecs.add_animation_event(e, state_name, normalized_time, event_name)` | entity, string, float, string | 添加动画事件（0~1 归一化时间） |
| `ecs.play_animation(e, state_name)` | entity, string | 播放指定动画状态 |
| `ecs.play_animation_segment(e, start_frame, end_frame, loop)` | entity, int, int, bool | 播放帧段 |
| `ecs.pop_animation_event(e)` | entity | `string` | 弹出已触发的动画事件名（空字符串=无） |

**示例：**
```lua
local e = dse.ecs.create_entity()
dse.ecs.add_animator(e)
dse.ecs.add_animation_state(e, "idle", 12, true, {tex1, tex2, tex3, tex4})
dse.ecs.add_animation_event(e, "idle", 0.5, "half_way")
dse.ecs.play_animation(e, "idle")

-- 每帧检查动画事件
local event = dse.ecs.pop_animation_event(e)
if event ~= "" then print("Animation event: " .. event) end
```

### 5.14 Animation 3D

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_animator_3d(e, [danim_path], [dskel_path])` | entity, string, string | 添加 3D 骨骼动画器 |
| `ecs.set_animator_3d_state(e, [state_or_path], [speed], [loop])` | entity, string, float, bool | 设置动画状态/路径 |
| `ecs.get_animator_3d_state(e)` | entity | 见下方 | 获取动画器状态 |
| `ecs.init_animator_3d_fsm(e)` | entity | — | 初始化状态机 |
| `ecs.add_animator_3d_state(e, name, danim_path, loop, [speed])` | entity, string, string, bool, float | 添加 FSM 状态 |
| `ecs.add_animator_3d_transition(e, from, to, [duration], has_exit_time, [exit_time], [conditions])` | entity, string... | 添加状态转换 |
| `ecs.set_animator_3d_param_float(e, name, value)` | entity, string, float | 设置 FSM float 参数 |
| `ecs.set_animator_3d_param_trigger(e, name)` | entity, string | 触发 FSM trigger 参数 |

**`get_animator_3d_state` 返回值：**  
`ok, current_state, normalized_time, time, speed, loop, is_transitioning, bone_count, has_skeleton`

**转换条件表格式（Lua table of tables）：**
```lua
{ {"speed", 0, 0.1}, {"is_jumping", 1, 0} }  -- {param_name, mode, threshold}
```
**条件模式：** `0=Greater, 1=Less, 2=Equal, 3=NotEqual`

**示例：**
```lua
local char = dse.ecs.create_entity()
dse.ecs.add_animator_3d(char, "", "data/animation/human.dskel")
dse.ecs.init_animator_3d_fsm(char)
dse.ecs.add_animator_3d_state(char, "idle", "data/animation/idle.danim", true)
dse.ecs.add_animator_3d_state(char, "run",  "data/animation/run.danim",  true, 1.2)
dse.ecs.add_animator_3d_transition(char, "idle", "run", 0.25, false, 1.0,
    { {"speed", 0, 0.5} })  -- speed > 0.5 时切换到 run
dse.ecs.set_animator_3d_param_float(char, "speed", 3.0)
```

### 5.15 Particle

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

### 5.16 GameplayTuning

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_gameplay_tuning(e)` | entity | 添加调参组件 |
| `ecs.set_gameplay_tuning(e, [leaf_min_dist], [move_left], [move_right], [jump_scale], [jump_max], [cam_damp])` | entity, float... | 设置调参参数 |

---

## 6. dse.audio — 音频系统

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

**示例：**
```lua
local bgm = dse.ecs.create_entity()
dse.ecs.add_transform(bgm, 0, 0, 0)
dse.audio.add_source(bgm, "data/audio/bgm.wav", true, true, 0.8)

local sfx = dse.ecs.create_entity()
dse.ecs.add_transform(sfx, 10, 0, 5)
dse.audio.add_source(sfx, "data/audio/explosion.wav", false, false, 1.0)
dse.audio.set_3d_mode(sfx, true)
dse.audio.set_3d_distance(sfx, 1.0, 50.0, 1.0)
dse.audio.set_playing(sfx, true)  -- 触发播放
```

---

## 7. dse.spine — Spine 动画

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

## 8. dse.ui — UI 系统

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ui.add_renderer(e, [tex], [r], [g], [b], [a], [order], [w], [h])` | entity, int, float... | 0, 1,1,1,1, 0, 640, 160 | 添加 UI 渲染器 |
| `ui.add_label(e, text, [font_tex], [r], [g], [b], [a], [gw], [gh], [spacing], [cols], [rows], [ascii], [ox], [oy])` | entity, string, int, float... | — | 添加位图文字标签 |
| `ui.set_label_text(e, text)` | entity, string | — | 修改标签文本 |
| `ui.set_label_number(e, number)` | entity, int | — | 设置标签为数字模式 |
| `ui.add_panel(e, blocks_input)` | entity, bool | — | 添加 UI 面板 |
| `ui.add_button(e, [r], [g], [b], [a])` | entity, float... | — | 添加按钮 |
| `ui.set_button_scale(e, [hover_scale], [pressed_scale], [lerp_speed])` | entity, float... | 1.08, 0.94, 12 | 设置按钮缩放动画 |
| `ui.add_mask(e, [w], [h], [ox], [oy], [block_outside])` | entity, float..., bool | 0, 0, 0, 0, true | 添加 UI 遮罩 |
| `ui.add_rich_text(e, [text], [r], [g], [b], [a], [shadow], [outline])` | entity, string, float..., bool, bool | "", 1,1,1,1, false, false | 添加富文本 |
| `ui.set_rich_text(e, text)` | entity, string | — | 修改富文本内容 |
| `ui.add_joystick(e, [max_radius], [follow], [reset])` | entity, float, bool, bool | 64, true, true | 添加虚拟摇杆 |
| `ui.get_joystick_x(e)` | entity | `number` | 获取摇杆 X 轴方向（-1~1） |
| `ui.get_joystick_y(e)` | entity | `number` | 获取摇杆 Y 轴方向（-1~1） |

**富文本标签语法：** `<color=#ff0000>红色文字</color>`

**示例：**
```lua
local btn = dse.ecs.create_entity()
dse.ecs.add_transform(btn, 400, 300, 0)
dse.ui.add_renderer(btn, 0, 0.2, 0.6, 1.0, 1.0, 10, 200, 60)
dse.ui.add_button(btn, 0.2, 0.6, 1.0, 1.0)
dse.ui.add_label(btn, "Play Game", font_tex)

local joystick = dse.ecs.create_entity()
dse.ecs.add_transform(joystick, 100, 500, 0)
dse.ui.add_joystick(joystick, 64, true, true)
-- 每帧读取
local jx = dse.ui.get_joystick_x(joystick)
local jy = dse.ui.get_joystick_y(joystick)
```

---

## 附录：绑定源文件索引

| 模块 | 源文件 | 函数数 |
|------|--------|--------|
| 全局 + Context | `lua_binding_context.cpp` | 1 |
| app / assets / metrics | `lua_binding_core.cpp` | 21 |
| ecs.core | `lua_binding_ecs_core.cpp` | 3 |
| ecs.transform | `lua_binding_ecs_transform.cpp` | 4 |
| ecs.rendering | `lua_binding_ecs_rendering.cpp` | 40 |
| ecs.physics2d | `lua_binding_ecs_physics2d.cpp` | 15 |
| ecs.phys3d | `lua_binding_ecs_phys3d.cpp` | 14 |
| ecs.animation | `lua_binding_ecs_animation.cpp` | 14 |
| ecs.particles | `lua_binding_ecs_particles.cpp` | 8 |
| audio | `lua_binding_audio.cpp` | 10 |
| spine | `lua_binding_spine.cpp` | 2 |
| ui | `lua_binding_ui.cpp` | 13 |
| **合计** | **14 个文件** | **~145 个绑定函数** |
