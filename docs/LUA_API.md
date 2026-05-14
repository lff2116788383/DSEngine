# DSEngine Lua API 参考文档

> 自动生成自 `engine/scripting/lua/bindings/` 源码
> 生成日期：2026-05-14
> 绑定文件：16 个 C++ 源文件，涵盖 8 个顶层模块

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
│   ├── (core)                   -- 实体创建/场景加载
│   ├── transform.*              -- Transform 组件
│   ├── camera.*                 -- 摄像机
│   ├── sprite.*                 -- 2D 精灵
│   ├── mesh_renderer.*          -- 3D 网格渲染
│   ├── light.*                  -- 光照
│   ├── skybox.*                 -- 天空盒
│   ├── terrain.*                -- 地形
│   ├── post_process.*           -- 后处理
│   ├── steering.*               -- AI 转向
│   ├── physics_2d.*             -- 2D 物理
│   ├── physics_3d.*             -- 3D 物理
│   ├── animation_2d.*           -- 2D 帧动画
│   ├── animation_3d.*           -- 3D 骨骼动画（含层系统 + IK）
│   ├── particle.*               -- 粒子系统
│   └── gameplay_tuning.*        -- 游戏调参
├── audio.*                      -- 音频系统
├── spine.*                      -- Spine 2D 骨骼动画
├── ui.*                         -- UI 系统
├── dssl.*                       -- DSSL 材质系统
└── gameplay3d.*                 -- 3D 游戏能力扩展
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
6. [dse.audio — 音频系统](#6-dseaudio--音频系统)
7. [dse.spine — Spine 动画](#7-dsespine--spine-动画)
8. [dse.ui — UI 系统](#8-dseui--ui-系统)
9. [dse.dssl — DSSL 材质系统](#9-dsedssl--dssl-材质系统)
10. [dse.gameplay3d — 3D 游戏能力](#10-dsegameplay3d--3d-游戏能力)

---

## 1. 全局函数

| 函数 | 返回值 | 说明 |
|------|--------|------|
| `dse.get_memory_usage_kb()` | `number` | Lua 虚拟机当前内存使用量（KB） |
| `dse.print_ecs_summary()` | — | 打印当前 ECS World 中各组件实体数量 |
| `dse.set_lua_package_path(path)` | — | 设置 Lua 包搜索路径 |

---

## 2. dse.app — 应用 / 输入

### 系统

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `app.set_data_root(path)` | `string` | — | 设置资源数据根目录 |
| `app.set_window_title(title)` | `string` | — | 设置窗口标题 |
| `app.set_window_icon(tex_handle)` | `int` | — | 设置窗口图标（纹理句柄） |
| `app.time_since_startup()` | — | `number` | 自启动以来的秒数 |
| `app.get_screen_width()` | — | `int` | 屏幕宽度（像素） |
| `app.get_screen_height()` | — | `int` | 屏幕高度（像素） |
| `app.set_target_fps(fps)` | `int` | — | 设置目标帧率 |
| `app.quit()` | — | — | 退出应用 |

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
| `app.get_mouse_middle()` | — | `bool` | 鼠标中键是否按住 |
| `app.get_mouse_middle_down()` | — | `bool` | 鼠标中键当前帧是否按下 |
| `app.get_mouse_left_double_click()` | — | `bool` | 当前帧是否触发了左键双击 |
| `app.get_mouse_left_long_press([duration])` | `[number=0.5]` | `bool` | 左键是否长按超过指定时间（秒） |
| `app.get_mouse_swipe_dx()` | — | `number` | 当前帧滑动/拖拽 X 轴增量（像素） |
| `app.get_mouse_swipe_dy()` | — | `number` | 当前帧滑动/拖拽 Y 轴增量（像素） |
| `app.get_mouse_scroll_dx()` | — | `number` | 鼠标滚轮水平滚动量 |
| `app.get_mouse_scroll_dy()` | — | `number` | 鼠标滚轮垂直滚动量 |

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
| `assets.load_texture_async(path, callback)` | `string, function` | — | 异步加载纹理，完成后回调 callback(handle) |
| `assets.load_dmesh(path)` | `string` | `bool` | 加载 .dmesh 文件 |
| `assets.load_material(path)` | `string` | `bool` | 加载 .dmat 材质文件 |
| `assets.get_asset_info(path)` | `string` | `string` | 获取资产信息（路径/类型/大小） |

---

## 4. dse.metrics — 性能统计

| 函数 | 返回值 | 说明 |
|------|--------|------|
| `metrics.get_draw_calls()` | `int` | 当前帧 draw call 数 |
| `metrics.get_max_batch_sprites()` | `int` | 批次最大精灵数 |
| `metrics.get_sprite_count()` | `int` | 当前精灵总数 |
| `metrics.get_fps()` | `number` | 当前帧率 |
| `metrics.get_frame_time_ms()` | `number` | 上一帧耗时（毫秒） |

---

## 5. dse.ecs — 实体组件系统

### 5.1 核心

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `ecs.create_entity()` | — | `entity` | 创建空实体 |
| `ecs.destroy_entity(e)` | `entity` | — | 销毁实体 |
| `ecs.load_scene(path)` | `string` | `bool, string` | 加载场景文件，返回 (成功?, 错误信息) |
| `ecs.find_entities_by_mesh_path(path)` | `string` | `table` | 查找所有使用指定 mesh 路径的实体 |
| `ecs.find_entities_by_name(name)` | `string` | `table` | 按名称查找实体 |

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
dse.ecs.set_transform_rotation(e, 0, 45, 0)
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
| `ecs.get_camera_3d_position(e)` | entity | `x, y, z` | 获取 3D 摄像机位置 |
| `ecs.get_camera_3d_forward(e)` | entity | `x, y, z` | 获取 3D 摄像机前方向 |
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
| `ecs.set_sprite_size(e, w, h)` | entity, float, float | — | 设置精灵尺寸 |
| `ecs.set_sprite_color(e, r, g, b, a)` | entity, float... | — | 设置精灵颜色 |

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
| `ecs.set_mesh_visible(e, visible)` | entity, bool | 设置网格可见性 |
| `ecs.get_mesh_count(e)` | entity | `int` | 获取网格子网格数 |

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

#### 天空光 + 光照探针

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_sky_light(e, [up_r], [up_g], [up_b], [down_r], [down_g], [down_b], [intensity])` | entity, float... | 0.22,0.28,0.38, 0.04,0.05,0.08, 1 | 添加天空环境光 |
| `ecs.set_sky_light(e, ...)` | entity, float..., [enabled] | — | 修改天空光参数 |
| `ecs.add_light_probe(e, [radius])` | entity, float | 10 | 添加光照探针 |
| `ecs.add_reflection_probe(e, [cubemap_size])` | entity, int | 256 | 添加反射探针 |

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
| `ecs.set_skybox_texture(e, path)` | entity, string | 设置天空盒纹理 |

### 5.8 Terrain

| 函数 | 参数 | 默认值 | 说明 |
|------|------|--------|------|
| `ecs.add_terrain(e, [heightmap], [width], [depth], [max_height])` | entity, string, float... | "", 100, 100, 20 | 添加地形 |
| `ecs.set_terrain_params(e, [res_x], [res_z], [max_lod], [lod_dist_factor], [use_dynamic_lod])` | entity, int, int, int, float, bool | — | 设置地形参数 |
| `ecs.set_terrain_height(e, x, z, height)` | entity, int, int, float | — | 设置单个高度点 |
| `ecs.load_terrain_heightmap(e, path)` | entity, string | — | 加载高度图 |
| `ecs.set_terrain_texture(e, path)` | entity, string | — | 设置地形纹理 |
| `ecs.get_terrain_lod(e)` | entity | `lod, res_x, res_z, max_lod, lod_factor` | 获取 LOD 状态 |
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

### 5.9 PostProcess

| 函数 | 参数 | 说明 |
|------|------|------|
| `ecs.add_post_process(e, bloom_enabled, [threshold], [intensity], [exposure])` | entity, bool, float... | 添加后处理组件 |
| `ecs.set_post_process_bloom(e, [enabled], [bloom], [threshold], [intensity], [exposure])` | entity, bool... | 设置 Bloom 参数 |
| `ecs.set_post_process_color(e, [color_grading], [exposure], [gamma])` | entity, bool, float... | 设置颜色分级参数 |
| `ecs.set_post_process_ssao(e, [enabled], [radius], [bias])` | entity, bool, float, float | 设置 SSAO 参数 |
| `ecs.set_post_process_vignette(e, [enabled], [intensity], [radius], [softness])` | entity, bool, float... | 设置暗角参数 |
| `ecs.set_post_process_film_grain(e, [enabled], [intensity])` | entity, bool, float | 设置胶片颗粒参数 |
| `ecs.get_post_process_state(e)` | entity | 见下方 | 获取完整后处理状态 |

**`get_post_process_state` 返回值：**
`ok, enabled, bloom_on, bloom_threshold, bloom_intensity, color_grading_on, exposure, gamma, ssao_on, ssao_radius, ssao_bias`

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
| `ecs.add_capsule_collider_3d(e, radius, height)` | entity, float, float | — | 添加胶囊碰撞体 |
| `ecs.rigidbody_3d_add_force(e, fx, fy, fz)` | entity, float... | — | 施加持续力 |
| `ecs.rigidbody_3d_add_impulse(e, ix, iy, iz)` | entity, float... | — | 施加瞬时冲量 |
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

local event = dse.ecs.pop_animation_event(e)
if event ~= "" then print("Animation event: " .. event) end
```

### 5.14 Animation 3D

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

**示例：层 + 混合树：**
```lua
local char = dse.ecs.create_entity()
dse.ecs.add_transform(char, 0, 0, 0)
dse.ecs.add_animator_3d(char, "anim/idle.danim", "anim/human.dskel")
-- 添加上半身图层（边走边挥手）
dse.ecs.add_anim_layer_component(char)
local wave_idx = dse.ecs.add_anim_layer(char, "upper_body", 1.0, 0)  -- Override
dse.ecs.set_anim_layer_clip(char, wave_idx, "anim/wave.danim", 1.0, true)
dse.ecs.set_anim_layer_bone_mask(char, wave_idx, {"Spine", "Spine1", "LeftArm", "RightArm"})
```

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

**示例：脚部 IK + LookAt：**
```lua
local char = dse.ecs.create_entity()
dse.ecs.add_transform(char, 0, 0, 0)
dse.ecs.add_animator_3d(char, "anim/run.danim", "anim/human.dskel")
-- 脚部 IK（贴地形）
dse.ecs.add_ik_component(char)
local foot_idx = dse.ecs.add_ik_chain(char, "left_leg", 0, "LeftUpLeg", "LeftFoot", 1.0)
dse.ecs.set_ik_pole_vector(char, foot_idx, 0, 0, -1)
-- 头部 LookAt
local head_idx = dse.ecs.add_ik_chain(char, "head_look", 1, "Head", "Head", 0.8)
dse.ecs.set_ik_target(char, head_idx, 10, 2, 0)  -- 看向 (10, 2, 0)
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
| `audio.stop(e)` | entity | — | 停止播放 |
| `audio.set_loop(e, loop)` | entity, bool | — | 设置循环 |
| `audio.set_volume(e, volume)` | entity, float | — | 设置音量 |
| `audio.set_master_volume(volume)` | float | — | 设置主音量 |
| `audio.set_bgm_volume(volume)` | float | — | 设置 BGM 音量 |
| `audio.set_sfx_volume(volume)` | float | — | 设置 SFX 音量 |
| `audio.set_pitch(e, pitch)` | entity, float | — | 设置音调（≥0.01） |
| `audio.set_3d_mode(e, enabled)` | entity, bool | — | 启用/禁用空间音频 |
| `audio.add_listener(e)` | entity | — | 添加音频监听器 |
| `audio.set_3d_distance(e, [min], [max], [rolloff])` | entity, float... | 1, 20, 1 | 设置 3D 衰减距离 |
| `audio.get_source_state(e)` | entity | 见下方 | 获取音源完整状态 |
| `audio.set_occlusion_enabled(e, enabled)` | entity, bool | — | 设置音频遮挡 |
| `audio.set_max_concurrent_sfx(count)` | int | — | 设置最大并发 SFX 数 |

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
dse.audio.set_playing(sfx, true)
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
| `ui.add_mask(e, [w], [h], [ox], [oy], [block_outside])` | entity, float..., bool | 0,0,0,0,true | 添加 UI 遮罩 |
| `ui.add_rich_text(e, [text], [r], [g], [b], [a], [shadow], [outline])` | entity, string, float..., bool, bool | "",1,1,1,1,false,false | 添加富文本 |
| `ui.set_rich_text(e, text)` | entity, string | — | 修改富文本内容 |
| `ui.add_joystick(e, [max_radius], [follow], [reset])` | entity, float, bool, bool | 64, true, true | 添加虚拟摇杆 |
| `ui.get_joystick_x(e)` | entity | `number` | 获取摇杆 X 轴方向（-1~1） |
| `ui.get_joystick_y(e)` | entity | `number` | 获取摇杆 Y 轴方向（-1~1） |
| `ui.get_joystick_down(e)` | entity | `bool` | 摇杆是否按下 |
| `ui.is_point_in_entity(e, x, y)` | entity, float, float | `bool` | 检测点是否在 UI 区域内 |

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
local jx = dse.ui.get_joystick_x(joystick)
local jy = dse.ui.get_joystick_y(joystick)
```

---

## 9. dse.dssl — DSSL 材质系统

DSSL（DS Shader Language）材质系统允许在运行时加载自定义材质描述文件。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `dssl.load_material(path)` | `string` | `bool` | 加载 .dssl 材质文件 |
| `dssl.apply_material(e, mat_index, dssl_path)` | entity, int, string | `bool` | 应用 DSSL 材质到实体 |
| `dssl.set_material_param(e, mat_index, name, value)` | entity, int, string, float | — | 设置材质标量参数 |
| `dssl.set_material_texture(e, mat_index, slot_name, tex_path)` | entity, int, string, string | — | 设置材质纹理槽 |
| `dssl.get_material_count()` | — | `int` | 获取已加载的材质数 |
| `dssl.reload_all()` | — | — | 重新加载所有 DSSL 材质 |

**示例：**
```lua
-- 应用风格化 Half-Lambert 材质
dse.dssl.load_material("shaders/dssl/half_lambert_kf.dssl")
dse.dssl.apply_material(entity, 0, "half_lambert_kf")
```

---

## 10. dse.gameplay3d — 3D 游戏能力

3D 游戏玩法相关的系统功能。

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `gameplay3d.get_terrain_lod(e)` | entity | `int` | 获取地形当前 LOD 级别 |
| `gameplay3d.set_terrain_lod_params(e, max_lod, dist_factor)` | entity, int, float | — | 设置地形 LOD 参数 |
| `gameplay3d.get_steering_velocity(e)` | entity | `x, y, z` | 获取 AI 转向当前速度 |

---

## 附录：绑定源文件索引

| 模块 | 源文件 | 函数数 |
|------|--------|:------:|
| 全局 + Context | `lua_binding_context.cpp` | 3 |
| app / assets / metrics | `lua_binding_core.cpp` | 28 |
| ecs.core | `lua_binding_ecs_core.cpp` | 5 |
| ecs.transform | `lua_binding_ecs_transform.cpp` | 4 |
| ecs.rendering | `lua_binding_ecs_rendering.cpp` | 55 |
| ecs.physics2d | `lua_binding_ecs_physics2d.cpp` | 15 |
| ecs.phys3d | `lua_binding_ecs_phys3d.cpp` | 14 |
| ecs.animation | `lua_binding_ecs_animation.cpp` | 36 |
| ecs.particles | `lua_binding_ecs_particles.cpp` | 8 |
| ecs.gameplay3d | `lua_binding_ecs_gameplay3d.cpp` | 3 |
| audio | `lua_binding_audio.cpp` | 16 |
| spine | `lua_binding_spine.cpp` | 2 |
| ui | `lua_binding_ui.cpp` | 16 |
| dssl | `lua_binding_dssl.cpp` | 5 |
| registry | `lua_binding_registry.cpp` | 1 |
| **合计** | **16 个文件** | **~210 个绑定函数** |
