# 3D Demo 索引

按功能分类，所有 demo 遵循相同接口约定：`M.Setup(config)` + `M.Update(dt)`

运行方式：`DSEngine_Game_release.exe --script=samples\lua\main.lua --demo=3d_<name>`

---

## 入门 / 基础图元

| demo 名 | 文件 | 说明 |
|----------|------|------|
| `3d_triangle` | triangle.lua | 最小三角形面片 |
| `3d_square` | square.lua | 四边形 |
| `3d_cube` | cube.lua | 立方体 |

## 渲染 / 材质

| demo 名 | 文件 | 说明 |
|----------|------|------|
| `3d_static_model` | 3d_static_model.lua | .dmesh/.dmat 资源加载 |
| `3d_textured_cube` | 3d_textured_cube.lua | UV + 贴图验证 |
| `3d_material_showcase` | 3d_material_showcase.lua | PBR 材质球阵列 |
| `3d_advanced_pbr_showcase` | 3d_advanced_pbr_showcase.lua | Clear Coat/SSS/POM/Anisotropy |
| `3d_texture_material_slots` | 3d_texture_material_slots.lua | 多槽位贴图绑定 |
| `3d_lighting_showcase` | 3d_lighting_showcase.lua | 多光源类型 |
| `3d_shadow_showcase` | 3d_shadow_showcase.lua | CSM + 点光源阴影 |
| `3d_skybox_environment` | 3d_skybox_environment.lua | 天空盒 + 环境光 |
| `3d_postprocess_showcase` | 3d_postprocess_showcase.lua | Bloom/Tonemap/FXAA |
| `3d_render_quality_showcase` | 3d_render_quality_showcase.lua | 综合渲染质量对比 |

## 动画

| demo 名 | 文件 | 说明 |
|----------|------|------|
| `3d_animation_basic` | 3d_animation_basic.lua | 骨骼蒙皮 + 混合 |
| `3d_animation_ik_layers` | 3d_animation_ik_layers.lua | IK + 动画分层 |
| `3d_character_third_person` | 3d_character_third_person.lua | 第三人称角色 + FSM |
| `3d_character_controller` | 3d_character_controller.lua | 角色控制器 |

## 物理

| demo 名 | 文件 | 说明 |
|----------|------|------|
| `3d_physics_stack` | 3d_physics_stack.lua | 刚体堆叠 |
| `3d_physics_interaction` | 3d_physics_interaction.lua | 物理交互 |
| `3d_physics_raycast_pick` | 3d_physics_raycast_pick.lua | 射线检测拾取 |
| `3d_physics_triggers` | 3d_physics_triggers.lua | 触发器区域 |
| `3d_fracture` | 3d_fracture.lua | Voronoi 碎裂 |
| `3d_cloth` | 3d_cloth.lua | 布料模拟 |
| `3d_ragdoll` | 3d_ragdoll.lua | 布偶物理 |
| `3d_softbody` | 3d_softbody.lua | 软体 |
| `3d_buoyancy` | 3d_buoyancy.lua | 浮力 |
| `3d_vehicle` | 3d_vehicle.lua | 载具系统 |
| `3d_rope` | 3d_rope.lua | 绳索模拟 |
| `3d_fluid` | 3d_fluid.lua | 流体粒子 |

## 音频

| demo 名 | 文件 | 说明 |
|----------|------|------|
| `3d_audio_spatial` | 3d_audio_spatial.lua | 3D 空间音频 |
| `3d_audio_complete` | 3d_audio_complete.lua | 完整音频系统 |

## 地形 / 场景

| demo 名 | 文件 | 说明 |
|----------|------|------|
| `3d_terrain_heightmap` | 3d_terrain_heightmap.lua | 高度图地形 |
| `3d_terrain_lod_zones` | 3d_terrain_lod_zones.lua | 地形 LOD |
| `3d_scene_showcase` | 3d_scene_showcase.lua | 程序化场景组合 |
| `3d_scene_load` | 3d_scene_load.lua | .dscene 反序列化 |
| `3d_asset_pack_showcase` | 3d_asset_pack_showcase.lua | 资源包加载 |

## UI / 输入 / 调试

| demo 名 | 文件 | 说明 |
|----------|------|------|
| `3d_camera_showcase` | 3d_camera_showcase.lua | 相机控制模式 |
| `3d_input_showcase` | 3d_input_showcase.lua | 键鼠/手柄输入 |
| `3d_hud_overlay` | 3d_hud_overlay.lua | HUD 叠加 UI |
| `3d_metrics_debug` | 3d_metrics_debug.lua | 性能指标面板 |

## 程序化生成

| demo 名 | 文件 | 说明 |
|----------|------|------|
| `3d_procedural_mesh` | 3d_procedural_mesh.lua | 程序化网格生成 |
| `3d_particles_showcase` | 3d_particles_showcase.lua | 粒子系统 |

---

## 新增 demo 步骤

1. 在本目录创建 `3d_<name>.lua`，导出 `M.Setup(config)` + `M.Update(dt)`
2. （可选）在 `../config.lua` 添加 `Config.demo_3d_<name> = {...}`
3. 运行：`--demo=3d_<name>`
4. 更新本 README 对应分类表格
