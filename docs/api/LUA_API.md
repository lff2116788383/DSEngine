# DSEngine Lua API 参考文档

> 严格对齐 `engine/scripting/lua/bindings/` 源码
> 更新日期：2026-06-10
> 绑定文件：`engine/scripting/lua/bindings/` 下约 28 个手写 C++ 源文件 + 13 个 `*.gen.cpp`，
> 涵盖 16 个顶层模块（其中 `http` / `net` 为条件编译）。
> 说明：ECS 各子模块的函数计数为约数；网络/序列化/字体模块逐函数对齐源码。

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

> 完整示例：`docs/examples/lua/net_loopback.lua`、`docs/examples/lua/net_message.lua`（net + serialize）、
> `docs/examples/lua/deepseek_npc.lua`（http + AI NPC）。

---

## 附录：绑定源文件索引

| 模块 | 源文件 | 注册函数数 |
|------|--------|:------:|
| 全局 + Context | `lua_binding_context.cpp` | 1 |
| app / assets / metrics / origin | `lua_binding_core.cpp` | ~33 |
| ecs.core | `lua_binding_ecs_core.cpp` | 6 |
| ecs.transform (gen) | `lua_binding_ecs_transform.gen.cpp` | 5 |
| ecs.camera3d (gen) | `lua_binding_ecs_camera3d.gen.cpp` | ~4 |
| ecs.dir_light (gen) | `lua_binding_ecs_dir_light.gen.cpp` | ~3 |
| ecs.point_light (gen) | `lua_binding_ecs_point_light.gen.cpp` | ~3 |
| ecs.mesh_renderer (gen) | `lua_binding_ecs_mesh_renderer.gen.cpp` | ~5 |
| ecs.rendering（入口 + 子模块） | `lua_binding_ecs_rendering.cpp` + `_camera` / `_fx` / `_light` / `_mesh` / `_post` / `_terrain` | ~90 |
| ecs.physics2d | `lua_binding_ecs_physics2d.cpp` | 18 |
| ecs.phys3d | `lua_binding_ecs_phys3d.cpp` | 31 |
| ecs.gameplay3d | `lua_binding_ecs_gameplay3d.cpp` | 39 |
| ecs.animation | `lua_binding_ecs_animation.cpp` | ~36 |
| ecs.particles | `lua_binding_ecs_particles.cpp` | 16 |
| localization | `lua_binding_localization.cpp` | 6 |
| audio | `lua_binding_audio.cpp` | 17 |
| spine | `lua_binding_spine.cpp` | 2 |
| ui | `lua_binding_ui.cpp` | ~34 |
| dssl | `lua_binding_dssl.cpp` | 10 |
| navigation [NavMesh] | `lua_binding_navigation.cpp` | 10 |
| streaming | `lua_binding_streaming.cpp` | 12 |
| font | `lua_binding_font.cpp` | 7 |
| serialize | `lua_binding_serialize.cpp` | 2 |
| http [DSE_ENABLE_HTTP] | `lua_binding_http.cpp` | 5 |
| net [DSE_ENABLE_NET] | `lua_binding_net.cpp` | 12 |
| ecs (入口) | `lua_binding_ecs.cpp` | — |
| registry (入口) | `lua_binding_registry.cpp` | — |
| **合计** | **约 28 个手写文件 + 13 个 gen** | **~400 绑定函数** |

> **关于 LuaSocket**：仓库含可选 `DSE_ENABLE_LUASOCKET` 开关（默认 **OFF**，Android 强制 OFF），
> 启用时会把标准 `socket.core` / `mime.core` 预加载进 Lua。它**不属于 `dse.*` 绑定**，且已被
> `dse.net`（游戏 UDP 传输）+ `dse.http`（HTTPS REST，含 TLS）取代——LuaSocket 自身不支持 HTTPS。
> 当前任何构建/验证路径均未启用它。
