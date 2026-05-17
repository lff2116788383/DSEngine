# DSEngine API 文档缺口分析

> 生成日期：2026-05-17（最后更新：2026-05-17）
> 对比来源：`engine/scripting/lua/bindings/*.cpp`（实际注册） vs `docs/api/LUA_API.md`（现有文档）

---

## 概要统计

| 类别 | 状态 |
|------|:----:|
| **新版 Lua API 文档** | ✅ `docs/api/LUA_API.md`（340+ API，全量覆盖 11 个模块） |
| **C++ 公共 API 文档** | ✅ `docs/api/CPP_API.md`（10 大类 21 个头文件） |
| **完全未文档化的模块** | 0 |
| **旧版幻影函数（仅存于旧文档）** | ~35（新文档已清除，保留此表供补实现参考） |
| **代码中未文档化函数** | ~0（新版文档从绑定源码生成，全量覆盖） |

---

## 一、幻影函数（旧文档有，代码中不存在）

> **注意：** 以下幻影函数只存在于旧版 `docs/reference/LUA_API.md`。
> 新版 `docs/api/LUA_API.md` **已不包含这些函数**，仅保留此列表供参考。
> 如需补实现，可以按需从此列表中挑选。

### 1.1 全局

| 幻影函数 | 文档位置 |
|----------|----------|
| `dse.print_ecs_summary()` | §1 |
| `dse.set_lua_package_path(path)` | §1 |

### 1.2 dse.app

| 幻影函数 | 备注 |
|----------|------|
| `app.set_window_icon(tex_handle)` | |
| ~~`app.set_target_fps(fps)`~~ | ✅ 已实现 |
| ~~`app.quit()`~~ | ✅ 已实现 |
| ~~`app.get_mouse_middle()`~~ | ✅ 已实现 |
| ~~`app.get_mouse_middle_down()`~~ | ✅ 已实现 |
| ~~`app.get_mouse_scroll_dx()`~~ | ✅ 已实现 |
| ~~`app.get_mouse_scroll_dy()`~~ | ✅ 已实现 |

### 1.3 dse.assets

| 幻影函数 | 备注 |
|----------|------|
| `assets.load_texture_async(path, callback)` | |
| `assets.load_dmesh(path)` | |
| `assets.load_material(path)` | |
| `assets.get_asset_info(path)` | |

> 实际代码中 `dse.assets` 仅注册了 `load_texture` 一个函数。

### 1.4 dse.metrics

| 幻影函数 |
|----------|
| ~~`metrics.get_fps()`~~ | ✅ 已实现 |
| ~~`metrics.get_frame_time_ms()`~~ | ✅ 已实现 |

### 1.5 dse.ecs

| 幻影函数 | 所属域 |
|----------|--------|
| ~~`ecs.destroy_entity(e)`~~ | Core ✅ 已实现 |
| `ecs.find_entities_by_name(name)` | Core |
| `ecs.set_sprite_size(e, w, h)` | Sprite |
| `ecs.set_sprite_color(e, r, g, b, a)` | Sprite |
| `ecs.get_camera_3d_position(e)` | Camera |
| `ecs.get_camera_3d_forward(e)` | Camera |
| `ecs.set_mesh_visible(e, visible)` | MeshRenderer |
| `ecs.get_mesh_count(e)` | MeshRenderer |
| `ecs.set_skybox_texture(e, path)` | Skybox |
| `ecs.add_light_probe(e, radius)` | Light |
| `ecs.add_reflection_probe(e, size)` | Light |

### 1.6 dse.audio

| 幻影函数 |
|----------|
| `audio.stop(e)` |
| `audio.set_master_volume(vol)` |
| `audio.set_bgm_volume(vol)` |
| `audio.set_sfx_volume(vol)` |
| `audio.set_occlusion_enabled(e, enabled)` |
| `audio.set_max_concurrent_sfx(count)` |

### 1.7 dse.ui

| 幻影函数 |
|----------|
| `ui.get_joystick_down(e)` |
| `ui.is_point_in_entity(e, x, y)` |

### 1.8 dse.dssl（函数名错误）

| 文档中的名称 | 实际不存在 | 实际对应的函数 |
|-------------|-----------|--------------|
| `dssl.set_material_param(e, idx, name, val)` | ✗ | `dssl.set_float` / `set_color` / `set_vec3` |
| `dssl.set_material_texture(e, idx, slot, path)` | ✗ | `dssl.set_texture` / `set_texture_handle` |
| `dssl.get_material_count()` | ✗ | 无 |
| `dssl.reload_all()` | ✗ | 无 |

### 1.9 dse.gameplay3d（整节错误）

文档 §10 描述了一个 `dse.gameplay3d` 表，但实际代码中 **该表不存在**。
`RegisterEcsGameplay3DBindings` 将函数注册到 `dse.ecs.*` 下。
以下函数名也不存在于任何注册中：

| 幻影函数 |
|----------|
| `gameplay3d.get_terrain_lod(e)` |
| `gameplay3d.set_terrain_lod_params(e, ...)` |
| `gameplay3d.get_steering_velocity(e)` |

---

## 二、未文档化的 Lua API（代码有，文档无）—— ✅ 已全部覆盖

> **注意：** 以下列表仅作历史归档。新版 `docs/api/LUA_API.md` 已从绑定源码逐一提取，
> 全部 ~340 个注册函数均已文档化。本节保留用于溯源。

### 2.1 完全未文档化的模块

#### `nav.*` — 导航寻路系统（条件编译 `DSE_ENABLE_NAVMESH`）

全局表 `nav`：

| 函数 | 源文件 |
|------|--------|
| `nav.is_ready()` | `lua_binding_navigation.cpp` |
| `nav.load(path)` | |
| `nav.save(path)` | |
| `nav.find_path(sx, sy, sz, ex, ey, ez)` | |
| `nav.find_nearest(x, y, z)` | |
| `nav.raycast(sx, sy, sz, ex, ey, ez)` | |
| `nav.bake(...)` | |

ECS 扩展（注入 `dse.ecs`）：

| 函数 |
|------|
| `ecs.set_nav_agent(e, ...)` |
| `ecs.set_nav_destination(e, x, y, z)` |
| `ecs.nav_agent_arrived(e)` |

#### `streaming.*` — 资源流式加载系统

全局表 `streaming`：

| 函数 | 源文件 |
|------|--------|
| `streaming.create_zone(...)` | `lua_binding_streaming.cpp` |
| `streaming.destroy_zone(id)` | |
| `streaming.add_asset(zone, path)` | |
| `streaming.add_assets(zone, paths)` | |
| `streaming.set_zone_center(zone, x, y, z)` | |
| `streaming.force_load(zone)` | |
| `streaming.force_unload(zone)` | |
| `streaming.get_zone_state(zone)` | |
| `streaming.get_zone_progress(zone)` | |
| `streaming.set_budget(budget)` | |
| `streaming.get_active_loads()` | |
| `streaming.get_zone_count()` | |

### 2.2 ECS Core（1 个）

| 函数 | 说明 |
|------|------|
| `ecs.load_sub_scene(path)` | 加载子场景 |

### 2.3 ECS Transform（1 个）

| 函数 |
|------|
| `ecs.get_transform_rotation(e)` |

### 2.4 ECS Rendering（36 个）

| 函数 | 子系统 |
|------|--------|
| `ecs.set_mesh_advanced_material(e, ...)` | MeshRenderer |
| `ecs.sample_terrain_height(e, x, z)` | Terrain |
| `ecs.set_post_process_fxaa(e, ...)` | PostProcess |
| `ecs.set_post_process_auto_exposure(e, ...)` | PostProcess |
| `ecs.set_post_process_color_lut(e, ...)` | PostProcess |
| `ecs.set_post_process_outline(e, ...)` | PostProcess |
| `ecs.set_post_process_fog(e, ...)` | PostProcess |
| `ecs.set_post_process_light_shaft(e, ...)` | PostProcess |
| `ecs.add_decal(e, ...)` | Decal |
| `ecs.set_decal(e, ...)` | Decal |
| `ecs.add_water(e, ...)` | Water |
| `ecs.set_water(e, ...)` | Water |
| `ecs.get_water(e)` | Water |
| `ecs.lod_add_level(e, ...)` | LOD |
| `ecs.lod_set_scale(e, ...)` | LOD |
| `ecs.lod_set_enabled(e, ...)` | LOD |
| `ecs.add_grass(e, ...)` | Grass |
| `ecs.set_grass_params(e, ...)` | Grass |
| `ecs.set_grass_color(e, ...)` | Grass |
| `ecs.set_grass_wind(e, ...)` | Grass |
| `ecs.set_grass_lod(e, ...)` | Grass |
| `ecs.set_grass_enabled(e, ...)` | Grass |
| `ecs.get_grass_stats(e)` | Grass |
| `ecs.add_hair(e, ...)` | Hair |
| `ecs.set_hair_physics(e, ...)` | Hair |
| `ecs.set_hair_render(e, ...)` | Hair |
| `ecs.set_hair_wind(e, ...)` | Hair |
| `ecs.set_hair_enabled(e, ...)` | Hair |
| `ecs.set_hair_lod(e, ...)` | Hair |
| `ecs.add_gi_probe(e, ...)` | GI Probe (DDGI) |
| `ecs.set_gi_probe(e, ...)` | GI Probe |
| `ecs.set_gi_probe_enabled(e, ...)` | GI Probe |
| `ecs.get_gi_probe(e)` | GI Probe |
| `ecs.world_to_screen(e, x, y, z)` | Utility |

### 2.5 ECS Physics 3D（16 个）

| 函数 | 子系统 |
|------|--------|
| `ecs.add_terrain_heightmap(e, ...)` | TerrainHeightmap |
| `ecs.terrain_heightmap_set_data(e, ...)` | TerrainHeightmap |
| `ecs.terrain_get_height(e, x, z)` | TerrainHeightmap |
| `ecs.physics_3d_overlap_sphere(cx, cy, cz, r)` | Overlap Query |
| `ecs.physics_3d_overlap_box(...)` | Overlap Query |
| `ecs.physics_3d_get_collision_events(e)` | Events |
| `ecs.physics_3d_get_trigger_events(e)` | Events |
| `ecs.add_mesh_collider_3d(e, ...)` | Collider |
| `ecs.add_joint_3d(e, ...)` | Joint |
| `ecs.set_joint_3d_hinge_limits(e, ...)` | Joint |
| `ecs.set_joint_3d_spring(e, ...)` | Joint |
| `ecs.set_joint_3d_distance(e, ...)` | Joint |
| `ecs.is_joint_3d_broken(e)` | Joint |
| `ecs.set_collision_layer(e, layer)` | Collision Layer |
| `ecs.set_collider_trigger(e, trigger)` | Helpers |
| `ecs.set_collider_material(e, ...)` | Helpers |

### 2.6 ECS Physics 2D（2 个）

| 函数 |
|------|
| `ecs.add_polygon_collider(e, ...)` |
| `ecs.set_polygon_collider_trigger(e, trigger)` |

### 2.7 Audio — 混音总线系统（7 个）

| 函数 |
|------|
| `audio.bus_set_volume(bus_name, vol)` |
| `audio.bus_set_muted(bus_name, muted)` |
| `audio.bus_create(bus_name)` |
| `audio.bus_remove(bus_name)` |
| `audio.bus_add_effect(bus_name, effect_type)` |
| `audio.bus_remove_effect(bus_name, effect_type)` |
| `audio.bus_get_names()` |

### 2.8 UI（7 个）

| 函数 |
|------|
| `ui.set_position(e, x, y)` |
| `ui.set_size(e, w, h)` |
| `ui.set_anchor(e, ax, ay)` |
| `ui.set_color(e, r, g, b, a)` |
| `ui.set_visible(e, visible)` |
| `ui.set_uv(e, u, v, w, h)` |
| `ui.set_nine_slice(e, ...)` |

### 2.9 DSSL（整节需重写，8 个未文档化）

| 函数 |
|------|
| `dssl.create_instance(dssl_path)` |
| `dssl.set_float(e, mat_idx, name, value)` |
| `dssl.set_color(e, mat_idx, r, g, b, a)` |
| `dssl.set_vec3(e, mat_idx, name, x, y, z)` |
| `dssl.set_texture(e, mat_idx, slot, path)` |
| `dssl.set_texture_handle(e, mat_idx, slot, handle)` |
| `dssl.get_float(e, mat_idx, name)` |
| `dssl.get_color(e, mat_idx)` |

### 2.10 ECS Gameplay3D（全部未文档化，~25 个）

注意：这些函数注册在 `dse.ecs.*` 下，不是独立 `gameplay3d` 表。

#### 布料模拟（Cloth）
| 函数 |
|------|
| `ecs.add_cloth(e, ...)` |
| `ecs.set_cloth_wind(e, ...)` |
| `ecs.set_cloth_gravity(e, ...)` |
| `ecs.cloth_pin_vertices(e, ...)` |
| `ecs.cloth_add_sphere_collider(e, ...)` |

#### 流体（Fluid）
| 函数 |
|------|
| `ecs.add_fluid_emitter(e, ...)` |
| `ecs.set_fluid_physics(e, ...)` |
| `ecs.set_fluid_rendering(e, ...)` |
| `ecs.set_fluid_emit_direction(e, ...)` |
| `ecs.set_fluid_floor(e, ...)` |
| `ecs.get_fluid_particle_count(e)` |

#### 软体（SoftBody）
| 函数 |
|------|
| `ecs.add_softbody(e, ...)` |
| `ecs.softbody_set_gravity(e, ...)` |
| `ecs.softbody_pin_vertex(e, ...)` |
| `ecs.softbody_get_particle_count(e)` |

#### 绳索（Rope）
| 函数 |
|------|
| `ecs.add_rope(e, ...)` |
| `ecs.rope_set_anchors(e, ...)` |
| `ecs.rope_get_positions(e)` |
| `ecs.rope_set_gravity(e, ...)` |

#### 破碎（Fracture，需 PhysX）
| 函数 |
|------|
| `ecs.add_fracture(e, ...)` |
| `ecs.set_fracture_params(e, ...)` |
| `ecs.fracture_apply_damage(e, ...)` |
| `ecs.fracture_trigger(e)` |
| `ecs.fracture_is_fractured(e)` |

#### 布娃娃（Ragdoll，需 PhysX）
| 函数 |
|------|
| `ecs.add_ragdoll(e, ...)` |
| `ecs.ragdoll_activate(e)` |
| `ecs.ragdoll_deactivate(e)` |
| `ecs.ragdoll_is_active(e)` |
| `ecs.set_ragdoll_collision_layer(e, layer)` |

#### 车辆（Vehicle，需 PhysX）
| 函数 |
|------|
| `ecs.add_vehicle(e, ...)` |
| `ecs.vehicle_add_wheel(e, ...)` |
| `ecs.vehicle_set_input(e, ...)` |
| `ecs.vehicle_get_speed(e)` |
| `ecs.vehicle_get_wheel_count(e)` |

#### 浮力（Buoyancy，需 PhysX）
| 函数 |
|------|
| `ecs.add_buoyancy(e, ...)` |
| `ecs.buoyancy_add_sample_point(e, ...)` |
| `ecs.buoyancy_set_water_level(e, level)` |
| `ecs.buoyancy_get_submerge_ratio(e)` |
| `ecs.buoyancy_set_use_fluid(e, use)` |

---

## 三、C++ 公共 API 文档 ✅ 已创建

**文件：** `docs/api/CPP_API.md`

已覆盖以下公共 API（10 大类，21 个头文件）：

| 章节 | 覆盖内容 |
|------|----------|
| §1 引擎生命周期 | `EngineInstance`, `EngineRunConfig` |
| §2 核心服务 | `ServiceLocator`, `EventBus`, `JobSystem` |
| §3 ECS | `World`, 组件头文件索引 |
| §4 资产管理 | `AssetManager`, 全部资产类型, 异步加载, LRU, 热重载, Pak |
| §5 渲染系统 | `RhiDevice`, `CommandBuffer`, `RenderGraph`, `IRenderPass` |
| §6 输入系统 | `Input`（键盘/鼠标/手柄/手势） |
| §7 音频系统 | `AudioSystem`, `AudioBusManager` |
| §8 场景管理 | `SceneManager`（子场景/过渡/UUID 引用） |
| §9 模块系统 | `IModule` 接口及所有虚方法 |
| §10 帧流水线 | `FramePipeline` |

### 待补充

| 头文件 | 说明 | 优先级 |
|--------|------|--------|
| `engine/assets/streaming_manager.h` | StreamingManager 详细 API | P2 |
| `engine/render/rhi/rhi_types.h` | 全部 RHI 数据结构体 | P2 |
| `engine/input/key_code.h` | 键码常量枚举 | P3 |
| `engine/audio/audio_bus.h` | AudioBusManager 详细 API | P2 |

---

## 四、建议修复优先级

### P0 — ✅ 全部完成
1. ~~**删除/标记幻影函数**~~ ✅ 新版 LUA_API.md 从源码重写，不含幻影函数
2. ~~**DSSL 文档重写**~~ ✅ 新版 §9 完整覆盖 10 个函数
3. ~~**§10 gameplay3d 整节重写**~~ ✅ 新版 §5.23 覆盖 8 个子系统 39 个函数

### P1 — ✅ 全部完成
4. ~~**nav / streaming 两个完整模块**~~ ✅ §10 / §11
5. ~~**Rendering 新增子系统**~~ ✅ §5.10–§5.22（Decal/Water/LOD/Grass/Hair/GI Probe 共 94 函数）
6. ~~**Physics 3D 新增功能**~~ ✅ §5.20（Joints/Events/Overlap/TerrainHeightmap）
7. ~~**Gameplay3D 全部子系统**~~ ✅ §5.23（Cloth/Fluid/SoftBody/Rope/Fracture/Ragdoll/Vehicle/Buoyancy）

### P2 — ✅ 全部完成
8. ~~**PostProcess 新增效果**~~ ✅ §5.9（FXAA/AutoExposure/ColorLUT/Outline/Fog/LightShaft）
9. ~~**Audio 混音总线**~~ ✅ §6
10. ~~**UI 新增函数**~~ ✅ §8
11. ~~**C++ 公共 API 文档**~~ ✅ `docs/api/CPP_API.md`

---

## 五、绑定源文件 → 函数数（新版文档统计）

| 模块 | 源文件 | 实际注册数 | 新版文档数 | 状态 |
|------|--------|:---------:|:--------:|:----:|
| Context | `lua_binding_context.cpp` | 1 | 1 | ✅ |
| app / assets / metrics | `lua_binding_core.cpp` | ~25 | ~25 | ✅ |
| ecs.core | `lua_binding_ecs_core.cpp` | 4 | 4 | ✅ |
| ecs.transform | `lua_binding_ecs_transform.cpp` | 5 | 5 | ✅ |
| ecs.rendering | `lua_binding_ecs_rendering.cpp` | 94 | 94 | ✅ |
| ecs.physics2d | `lua_binding_ecs_physics2d.cpp` | 18 | 18 | ✅ |
| ecs.phys3d | `lua_binding_ecs_phys3d.cpp` | 31 | 31 | ✅ |
| ecs.gameplay3d | `lua_binding_ecs_gameplay3d.cpp` | 39 | 39 | ✅ |
| ecs.animation | `lua_binding_ecs_animation.cpp` | ~36 | ~36 | ✅ |
| ecs.particles | `lua_binding_ecs_particles.cpp` | ~17 | ~17 | ✅ |
| audio | `lua_binding_audio.cpp` | 17 | 17 | ✅ |
| spine | `lua_binding_spine.cpp` | 2 | 2 | ✅ |
| ui | `lua_binding_ui.cpp` | ~34 | ~34 | ✅ |
| dssl | `lua_binding_dssl.cpp` | 10 | 10 | ✅ |
| nav | `lua_binding_navigation.cpp` | 10 | 10 | ✅ |
| streaming | `lua_binding_streaming.cpp` | 12 | 12 | ✅ |
| **合计** | **19 个文件** | **~340** | **~340** | ✅ |
