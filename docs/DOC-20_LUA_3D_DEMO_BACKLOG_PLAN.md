# Lua 3D Demo Backlog 规划

> 范围：规划 `samples/lua/3d` 后续 Lua 3D demo，不直接实现代码。外部资源允许从 `reference/` 拷贝到 `data/` 下按类型整理。

## 1. 参考资料摘要

### cpp-game-engine-book 路线

- `reference/cpp-game-engine-book/README.md` 的整体路线是：OpenGL 基础图元 → Shader → 纹理 → Mesh/Material → GameObject/Component → Camera → Control → GUI → Sound → Lua → 骨骼动画/蒙皮/FBX → PhysX → 经典光照/Shadow。
- `reference/cpp-game-engine-book/pages/SUMMARY.md` 中与 Lua 3D demo 最相关的章节：
  - 第 5 章：贴图，适合规划 textured cube。
  - 第 7 章：Mesh/Material/MeshRenderer，适合规划材质展示、静态模型加载。
  - 第 10 章：Camera，适合规划多相机、相机排序、相机参数展示。
  - 第 11 章：Control，适合规划 free camera / first-person / orbit camera。
  - 第 15.2 章：3D sound，适合规划 AudioSource + Listener。
  - 第 18/19/20 章：骨骼动画、蒙皮、FBX，适合规划 animation/character demo。
  - 第 22/23/25 章：PhysX、经典光照、Shadow Mapping，适合规划 physics、lighting、shadow demo。
- `reference/cpp-game-engine-book/pages/7. draw_mesh_and_material/7.6 mesh_renderer.md` 的核心思想是：Mesh、Material、Shader、Texture 资源化，由 MeshRenderer 汇总渲染。这正好对应 DSEngine 当前 `MeshRendererComponent` + `.dmesh/.dmat` 路线。

### VSEngine2.1 参考点

- `reference/VSEngine2.1/Demo/13/13.9`：heightmap 地形、DLOD、线框切换。
- `reference/VSEngine2.1/Demo/14/14.9`：SkyLight 上/下半球颜色、SkeletonActor。
- `reference/VSEngine2.1/Demo/15/15.3`：后处理灰度开关、DirectionalLight、SkeletonActor、SkyLight。
- `reference/VSEngine2.1/Demo/15/15.9`：MaterialInstance shader 参数实时调节。
- `reference/VSEngine2.1/Demo/15/15.22`：第一人称相机、多动画角色、地面、SkyLight、PointLight 组合场景。
- `reference/VSEngine2.1/Demo/15/15.27`：Diffuse/Normal/Emissive 多贴图材质绑定与模型保存流程。
- `reference/VSEngine2.1/Demo/14/14.27`：角色 Actor、第三人称相机、角色移动、攻击、AnimTree。

## 2. 当前 Lua/3D 基线能力

### 已有 demo

当前已有基础 demo：

- `samples/lua/3d/triangle.lua`：3 顶点、1 个三角面、基础方向光、free camera。
- `samples/lua/3d/square.lua`：4 顶点、2 个三角面、索引复用、基础材质。
- `samples/lua/3d/cube.lua`：8 顶点、12 个三角面、Lit shader、基础材质、旋转立方体。

这些 demo 已经建立了固定风格：

- 模块表 `Setup/Update`。
- 局部 `state` 存实体和时间。
- `setup_camera/setup_light/setup_xxx` 拆分。
- 启动日志说明目标与操作。
- `Update` 中做旋转或动态变化。

### Lua 分发方式

- `samples/lua/main.lua` 通过 `Config.game_entry` 分发。
- 当前 3D key：`3d_triangle`、`3d_square`、`3d_cube`。
- `samples/lua/config.lua` 目前提供 `Config.basic_3d.camera_distance`。
- 后续建议：每个新增 demo 在 `config.lua` 中独立配置，例如 `Config.demo_3d_static_model`、`Config.demo_3d_lighting_showcase`，避免全部复用 `basic_3d`。

### 已暴露能力

- Transform：创建实体、添加 transform、设置 position/rotation、读取 position。
- Mesh/Material：添加 MeshRenderer、设置 mesh path、设置 dmat material、设置 shader variant、设置 metallic/roughness/ao/emissive/double-sided 等参数。
- Camera：添加 3D camera、设置 priority/enabled、添加 free camera controller。
- Lights：DirectionalLight、PointLight、SpotLight 可从 Lua 创建。
- 进阶组件：Terrain、PostProcess、ParticleSystem3D、RigidBody3D、Collider3D、Animator3D、Skybox 等已有部分 Lua 入口。

### 当前能力边界

- 手写 Lua mesh 只传 position，非 `.dmesh` 路径在渲染侧默认 stride=3、UV=0，因此 textured cube 不适合用纯 Lua 顶点实现，优先走 `.dmesh/.dmat`。
- `.dmesh` 加载路径已能读取 position/normal/uv/skin/tangent 等数据，适合优先验证 `data/models/cube.dmesh` 与 `data/models/cube.dmat`。
- `.obj/.gltf/.fbx` fallback 主要解析 position/index，材质贴图仍应依赖 `.dmat` 或后续资产管线。
- SkyLight 组件和渲染侧读取已存在，但 Lua 缺少 add/set skylight 绑定。
- Terrain 有组件和渲染系统；Lua 已可写入程序化 height data、配置 resolution/LOD 并读取当前 LOD，图片 heightmap 文件采样仍待补齐。
- Physics3D 有 PhysX 系统和刚体/碰撞体绑定，但 Lua raycast 当前仍是 mock miss。
- Particle3D 有系统，但 Lua 参数太少，只能设置 max_particles 和 emission_rate。
- Audio Lua 绑定目前是基础 2D 音源控制，缺 3D source/listener API。
- Animator3D Lua API 已有，但缺稳定成套动画资源与端到端验收样例。

## 3. 资源整理建议

允许从 `reference/` 拷贝资源到 `data/` 后，建议统一整理为：

```text
data/
  models/
    primitives/
    static/
    character/
  materials/
    primitives/
    showcase/
    character/
  textures/
    checker/
    material_showcase/
    environment/
    terrain/
    vse_demo/
  audio/
    sfx/
    spatial/
  terrain/
    heightmaps/
  animation/
    skeletons/
    clips/
```

资源原则：

1. P0 只依赖现有 `data/models/cube.dmesh`、`data/models/cube.dmat`，以及最小 checker 纹理。
2. P1 从 VSEngine2.1 参考中挑选轻量静态模型/贴图，转换或重打包到 DSEngine 可读取格式。
3. P2 再引入 terrain heightmap、character、skeleton、animation、audio 等重资源。
4. 每个资源目录添加 README 或 manifest，记录来源、原路径、转换方式、许可证/用途。

## 4. Demo backlog 总表

| 优先级 | Demo 文件 | 主题 | 当前可行性 | 主要缺口 |
|---|---|---|---|---|
| P0 | `samples/lua/3d/3d_static_model.lua` | `.dmesh/.dmat` 静态模型加载 | 高 | Lua 入口与 config |
| P0 | `samples/lua/3d/3d_material_showcase.lua` | PBR 参数、发光、双面展示 | 高 | 纹理 slot 需 `.dmat` |
| P0 | `samples/lua/3d/3d_lighting_showcase.lua` | Directional/Point/Spot 多光源 | 高 | SkyLight 暂不纳入 |
| P0 | `samples/lua/3d/3d_camera_showcase.lua` | free camera、多相机、自动切换 | 高 | 输入查询可选 |
| P0 | `samples/lua/3d/3d_textured_cube.lua` | 贴图立方体 | 中高 | 必须走 `.dmesh/.dmat` 或补 UV API |
| P1 | `samples/lua/3d/3d_scene_showcase.lua` | 小型综合场景 | 已落地并验证 | 已接入入口/config/验证 preset；暂不引入外部模型资源 |
| P1 | `samples/lua/3d/3d_postprocess_showcase.lua` | bloom/灰度/后处理开关 | 已落地并验证 | 已补 bloom/color grading setter 与组件状态回读；灰度/反相 effect 仍待后续扩展 |
| P1 | `samples/lua/3d/3d_skybox_environment.lua` | skybox/skylight/环境色 | 已落地并验证 | 已补 SkyLight Lua 绑定；cubemap 资源暂留空配置 |
| P1 | `samples/lua/3d/3d_particles_showcase.lua` | 3D 粒子 | 已落地并验证 | 已补粒子参数 setter；当前同时使用可见 emissive fallback markers 确保截图主题明确 |
| P1 | `samples/lua/3d/3d_physics_stack.lua` | 刚体堆叠、碰撞 | 已落地并验证 | Debug 构建未启用 PhysX 时以可见堆叠 marker fallback；真实堆叠依赖 PhysX 构建 |
| P2 | `samples/lua/3d/3d_terrain_heightmap.lua` | heightmap 地形/LOD | 已落地 fallback | 已接入 Terrain 组件入口与可见程序化高度 fallback；真实 heightmap 采样/LOD 仍待专项补齐 |
| P2 | `samples/lua/3d/3d_shadow_showcase.lua` | 阴影展示 | 已接入可用 shadow 参数 API | 已新增 `set_directional_light_shadow` 暴露 cast_shadow、shadow_strength 与 CSM cascade_splits；demo 保留可见投影 fallback 并用日志验收真实 API 调用 |
| P2 | `samples/lua/3d/3d_animation_basic.lua` | 骨骼动画播放 | 已接入最小真实动画资源包 | 已新增 `data/animation/minimal_rig` two-bone `.dmesh/.dskel/.danim/.dmat`，`config.lua` 默认指向该资源；demo 同时保留 cube rig fallback，并用 `get_animator_3d_state` 日志验收真实资源路径、状态、时间、loop、transition、final_bones/has_skeleton |
| P2 | `samples/lua/3d/3d_character_third_person.lua` | 第三人称角色 | 已落地 fallback | 已接入跟随相机、Animator3D 状态和 cube character rig；真实角色控制器/动画资源仍待补齐 |
| P2 | `samples/lua/3d/3d_audio_spatial.lua` | 3D 空间音频 | 已落地 fallback | 已接入 AudioSource volume/pitch 动态 fallback 与环绕可视化；3D source/listener API 仍待补齐 |
| P3 | `samples/lua/3d/3d_physics_raycast_pick.lua` | Physics3D raycast/拾取专项 | 已接入可用 raycast | 已接入 BoxCollider3D/RigidBody3D、可见 ray beam/命中 marker；Lua `physics_3d_raycast` 优先使用 PhysX service，未启用时使用 ECS 3D collider 几何 fallback |
| P3 | `samples/lua/3d/3d_texture_material_slots.lua` | texture/material slot 专项 | 已接入可用 texture slot API | 已接入 `.dmesh/.dmat` 样本与材质 slot marker；Lua `set_mesh_texture(entity, slot, path)` 可绑定 albedo/normal/metallic_roughness/emissive/occlusion，UV/normal/tangent Lua authoring 仍待补齐 |
| P3 | `samples/lua/3d/3d_terrain_lod_zones.lua` | Terrain LOD 分区专项 | 已接入可用 terrain height/LOD API | 已接入 TerrainComponent 程序化 height data、resolution/LOD 参数与 current_lod 查询；near/mid/far tile 密度继续作为可视化标尺，图片 heightmap 文件采样仍待补齐 |

## 5. P0 详细规划

### 5.1 `samples/lua/3d/3d_static_model.lua`

- 目标画面/交互：加载 `data/models/cube.dmesh` 与 `data/models/cube.dmat`，显示一个资源化 cube；旁边放手写 cube 作为对比。
- 参考来源：cpp-game-engine-book 第 7 章 Mesh/Material/MeshRenderer；第 20 章模型导入路线。
- 需要能力：`add_mesh_renderer`、`set_mesh_path`、`set_mesh_material`、`set_mesh_shader_variant`。
- 当前可能缺口：需要确认 `cube.dmat` 中 texture path 是否完整；若无贴图，先验收 mesh path + material scalar。
- 最小实现路径：复制 `cube.lua` 结构，改为 `set_mesh_path("models/cube.dmesh")` 与 `set_mesh_material("models/cube.dmat")`。
- 验收标准：
  - 日志：`[3D][StaticModel] loaded models/cube.dmesh + models/cube.dmat`。
  - 截图：资源 cube 与手写 cube 并排可见。
  - 动态：资源 cube 持续旋转。

### 5.2 `samples/lua/3d/3d_material_showcase.lua`

- 目标画面/交互：多行 cube 展示 base color、metallic、roughness、emissive、double-sided、receive shadow 等差异；一个参数随时间变化。
- 参考来源：cpp-game-engine-book 第 7 章；VSEngine2.1 Demo 15.9 的 MaterialInstance shader 参数实时调节。
- 需要能力：`set_mesh_material`、`set_mesh_material_scalar`、`set_mesh_emissive`。
- 当前可能缺口：Lua 无直接 texture slot setter；base color 动态修改能力不足。
- 最小实现路径：复用 cube 几何，创建 3x3 材质矩阵；Update 中动态修改 roughness 或 emissive。
- 验收标准：
  - 日志：输出每个样本的 metallic/roughness/emissive。
  - 截图：多个 cube 在高光、暗哑、发光上有明显差异。
  - 数值：运行 5 秒内有一个 cube 的发光或粗糙度变化。

### 5.3 `samples/lua/3d/3d_lighting_showcase.lua`

- 目标画面/交互：地面 + 多个 cube，显示 DirectionalLight、PointLight、SpotLight 的影响；PointLight 绕场景移动。
- 参考来源：cpp-game-engine-book 第 23 章经典光照；VSEngine2.1 Demo 15.22。
- 需要能力：`add_directional_light_3d`、`set_directional_light_3d`、`add_point_light_3d`、`add_spot_light_3d`。
- 当前可能缺口：SkyLight Lua 绑定缺失；shadow 暂不纳入。
- 最小实现路径：创建方向光、红/蓝 point light、spot light；用 emissive 小 cube 标记光源位置。
- 验收标准：
  - 日志：输出三类光源参数。
  - 截图：多色照明区域清晰可见。
  - 动态：PointLight 移动时高亮区域随时间移动。

### 5.4 `samples/lua/3d/3d_camera_showcase.lua`

- 目标画面/交互：free camera + 自动 orbit camera + 固定俯视 camera；每 4 秒自动切换 active camera。
- 参考来源：cpp-game-engine-book 第 10/11 章；VSEngine2.1 Demo 15.22 的第一人称相机控制。
- 需要能力：`add_camera_3d`、`set_camera_priority`、`set_camera_enabled`、`add_free_camera_controller`。
- 当前可能缺口：Lua 输入查询能力如不足，先自动切换。
- 最小实现路径：创建多个 camera entity；通过 `set_camera_enabled` 或 priority 轮换。
- 验收标准：
  - 日志：每次切换打印 active camera。
  - 截图：同一场景从不同视角显示。
  - 交互：free camera 可右键 + W/A/S/D/Q/E 移动。

### 5.5 `samples/lua/3d/3d_textured_cube.lua`

- 目标画面/交互：纯色 cube 与 textured cube 对比，验证 UV/texture/material 链路。
- 参考来源：cpp-game-engine-book 第 5/7 章；VSEngine2.1 Demo 15.27 多贴图材质绑定。
- 需要能力：`.dmesh` 带 UV、`.dmat` 绑定 texture slot、MeshRenderer 读取 texture handles。
- 当前可能缺口：纯 Lua 手写 mesh 没 UV；若现有 `cube.dmat` 无贴图，需要新增 checker texture + dmat。
- 最小实现路径：使用 `models/cube.dmesh` + `models/cube.dmat`；必要时从 reference 或自制 checker 资源整理到 `data/textures/checker`。
- 验收标准：
  - 日志：打印 dmat path 和 texture slot 解析状态。
  - 截图：cube 表面出现非纯色纹理。
  - 动态：旋转时各面贴图稳定，无明显 UV 错乱。

## 5.6 当前实施进度

- P0 首批 5 个 Lua 3D demo 已在提交 `d7978a4` 落地并推送。
- P1 本轮已落地 5 个 demo：
  - `samples/lua/3d/3d_scene_showcase.lua`
  - `samples/lua/3d/3d_skybox_environment.lua`
  - `samples/lua/3d/3d_postprocess_showcase.lua`
  - `samples/lua/3d/3d_particles_showcase.lua`
  - `samples/lua/3d/3d_physics_stack.lua`
- P1 demo 均已接入 `samples/lua/main.lua` 的 `Config.game_entry` 分发、`samples/lua/config.lua` 独立配置项，以及 `tools/verify_lua_3d_demos.py` 的 `p1` preset。
- 本轮新增最小 C++ Lua binding：`add_sky_light`、`set_sky_light`、`set_post_process_bloom`、`set_post_process_color`、`get_post_process_state`、`set_particle_system_3d_params`。
- 验证命令已通过：`build_fast_lua.bat && python tools\verify_lua_3d_demos.py --entries p1 --frames 90`，输出 `VERIFY_OK`，截图与日志位于 `tmp/lua_3d_verify/`。
- 已知保留项：`3d_physics_stack` 在当前 Debug 构建中日志显示 `physics_bodies=0`，说明 PhysX 未参与运行；demo 仍创建 3D rigidbody/collider 并显示 fallback 堆叠画面，后续可在 PhysX 构建启用后复验真实掉落堆叠。
- P2 本轮继续补充 5 个 demo：
  - `samples/lua/3d/3d_terrain_heightmap.lua`
  - `samples/lua/3d/3d_shadow_showcase.lua`
  - `samples/lua/3d/3d_animation_basic.lua`
  - `samples/lua/3d/3d_character_third_person.lua`
  - `samples/lua/3d/3d_audio_spatial.lua`
- P2 新增 demo 已接入 `samples/lua/main.lua` 的 `Config.game_entry` 分发、`samples/lua/config.lua` 独立配置项，以及 `tools/verify_lua_3d_demos.py` 的 `p2` preset。
- P2 本轮遵循“先可见、后专项完善”的策略：Terrain 使用程序化高度 marker fallback，Shadow 使用地面暗色投影 marker fallback，Animation 使用分段 cube rig fallback，Character 使用 cube character rig + 自动跟随相机 fallback，Audio 使用环绕音源 marker + volume/pitch fallback；真实 heightmap 采样/LOD、shadow pass 稳定性、骨骼动画资源、角色控制器、3D audio source/listener API 仍作为后续专项保留。
- P3/专项增强首批新增 3 个 demo：
  - `samples/lua/3d/3d_physics_raycast_pick.lua`
  - `samples/lua/3d/3d_texture_material_slots.lua`
  - `samples/lua/3d/3d_terrain_lod_zones.lua`
- P3 首批用于把 P1/P2 fallback 中最关键的底层缺口拆成独立验收入口：Physics raycast 用可见 beam/命中 marker 先固定截图语义，Texture/Material slot 用 `.dmesh/.dmat` + slot marker 固定材质链路验收语义，Terrain LOD 用 near/mid/far 分区 tile 密度固定 LOD 验收语义；真实底层能力补齐后可在同名 demo 中逐步替换 fallback。

## 6. P1 详细规划

### 6.1 `samples/lua/3d/3d_scene_showcase.lua`

- 目标画面/交互：地面、多个静态模型、多光源、简单 UI/log overlay、自由相机，形成小型综合场景。
- 参考来源：VSEngine2.1 Demo 15.22；cpp-game-engine-book 第 7/10/11/23 章。
- 需要能力：P0 的 mesh/material/camera/light API；可选 UI 文本。
- 当前可能缺口：模型资源不够丰富；SkyLight 绑定不足。
- 最小实现路径：复用 P0 helper，先不做 UI，靠日志提示操作；从 reference 拷贝轻量静态模型/贴图并转换到 `data/models/static`、`data/materials/showcase`。
- 实施状态：已完成。当前复用 `models/cube.dmesh`/`models/cube.dmat` 与手写 cube，包含地面、背景墙、5+ 模型、多光源、移动物体、free camera；未新增外部资源。
- 验收标准：至少 1 个地面、5 个模型、2 类光源、1 个移动物体；free camera 可巡游。

### 6.2 `samples/lua/3d/3d_postprocess_showcase.lua`

- 目标画面/交互：bloom 与 color grading 后处理参数开关；emissive 物体触发 bloom。
- 参考来源：VSEngine2.1 Demo 15.3。
- 需要能力：`add_post_process`、PostProcess 参数、后处理 setter 与状态回读。
- 当前可能缺口：Lua 已可设置 bloom/enabled/exposure/gamma 并回读 PostProcessComponent 状态；灰度/反相 effect type 未暴露，shader 侧 gamma 仍需后续接入参数化。
- 最小实现路径：先做 bloom-only；补 `set_post_process_bloom`、`set_post_process_color` 与 `get_post_process_state` 后做时间切换和日志验收。
- 实施状态：已补齐后处理状态回读验收。`set_post_process_bloom` 返回成功布尔值，新增 `set_post_process_color(entity, color_grading_enabled, exposure, gamma)` 与 `get_post_process_state(entity)`，demo 创建 emissive 物体并随时间调节 threshold/intensity/exposure/gamma，日志从真实 `PostProcessComponent` 回读 bloom/color grading/SSAO 状态；灰度/反相 effect type 暂未纳入。
- 验收标准：日志必须包含 `postprocess_state_api`、`get_post_process_state=true`、`set_post_process_bloom=true`、`set_post_process_color=true`、`color_grading=true`、`gamma=`；截图中 emissive 物体周围有 bloom 或后处理差异。

### 6.3 `samples/lua/3d/3d_skybox_environment.lua`

- 目标画面/交互：skybox 或 SkyLight 上下半球环境色；观察模型暗部随环境变化。
- 参考来源：VSEngine2.1 Demo 14.9；cpp-game-engine-book Camera/Lighting。
- 需要能力：`add_skybox`，新增/补齐 `add_sky_light` 与 `set_sky_light`。
- 当前可能缺口：cubemap 资源、SkyLight Lua 绑定。
- 最小实现路径：先验证 skybox path；补 SkyLight API 后添加 up/down color 自动变化。
- 实施状态：已完成 SkyLight 路径。新增 `add_sky_light`/`set_sky_light` 绑定，demo 自动变化 up/down color；`skybox_path` 保留空配置，未引入 cubemap 资源。
- 验收标准：天空背景或环境色可见；模型暗部亮度随环境参数变化。

### 6.4 `samples/lua/3d/3d_particles_showcase.lua`

- 目标画面/交互：粒子喷泉/火花，从发射器位置持续产生粒子。
- 参考来源：DSEngine 当前 3D particle system。
- 需要能力：`add_particle_system_3d`、`set_particle_system_3d_params`、`get_particle_system_3d_state`。
- 当前可能缺口：真实粒子渲染可见性仍保留 fallback markers 兜底，但日志必须证明真实系统已经初始化并持续更新 active particle count。
- 最小实现路径：保留 emissive fallback marker 稳定截图主题，同时新增 runtime 查询 API，把 `active_particles/max_particles/emission_rate/life/size/speed/gravity/color/texture/enabled/initialized` 直接从真实 `ParticleSystem3DComponent` 回读到 Lua 日志；当 `DSE_ENABLE_3D=OFF` 且无 `DSE_Gameplay3D` 动态模块时，由 `FramePipeline` 启用 Particle3D 最小内置更新链路，避免 Lua runtime 构建中组件无人驱动。
- 实施状态：已补齐真实 Particle3D 运行时状态验收 API 与 `DSE_ENABLE_3D=OFF` 内置更新链路。新增 `get_particle_system_3d_state(entity)` 绑定，demo 在 Update 中周期打印 `particle_runtime_api get_particle_system_3d_state=true active_particles=... active_particles_nonzero=true initialized=true`，从而证明真实粒子系统已初始化、发射并更新；18 个动态 emissive fallback markers 继续保留，用于截图稳定兜底。
- 验收标准：截图可见粒子喷泉主题；日志必须包含 `particle_runtime_api`、`get_particle_system_3d_state=true`、`active_particles=`、`active_particles_nonzero=true`、`enabled=true`、`initialized=true`，且数值来自真实组件查询。

### 6.5 `samples/lua/3d/3d_physics_stack.lua`

- 目标画面/交互：动态 cube/sphere 从空中落到静态地面并堆叠；后续 raycast 选择。
- 参考来源：cpp-game-engine-book 第 22 章 PhysX。
- 需要能力：`add_rigidbody_3d`、`add_box_collider_3d`、`add_sphere_collider_3d`、Physics3DSystem。
- 当前可能缺口：PhysX 构建开关；raycast Lua mock；缺 collision event。
- 最小实现路径：只做掉落和堆叠，不做 raycast；打印 y 值变化。
- 实施状态：已完成 fallback 可验收实现。demo 创建 `RigidBody3D` + `BoxCollider3D`，但当前 Debug 验证日志显示 `physics_bodies=0` 且 y 值未变化；在 PhysX 未启用时以静态堆叠画面记录状态，后续需在 PhysX 构建中复验真实掉落。
- 验收标准：方块下落后停止堆叠；日志显示 Physics3DSystem 初始化和 y 值趋稳。

## 7. P2 详细规划

### 7.1 `samples/lua/3d/3d_terrain_heightmap.lua`

- 目标画面/交互：heightmap 地形、动态 LOD、自由相机漫游、可选线框/LOD 信息。
- 参考来源：VSEngine2.1 Demo 13.9。
- 需要能力：`add_terrain`、heightmap 采样、LOD 参数、terrain texture。
- 当前可能缺口：heightmap_path 未真正采样；Lua 无 resolution/lod/texture 参数。
- 最小实现路径：从 reference 拷贝 heightmap 到 `data/terrain/heightmaps`；补 TerrainSystem 采样与 Lua 参数。
- 实施状态：已完成 fallback 版。当前创建 TerrainComponent 并打印 heightmap/尺寸参数，同时用 11x9 程序化高度 tile marker 形成非平面地形画面；near/far LOD marker 用于截图中表达 LOD 主题。
- 验收标准：地形非平面；相机远近移动时 LOD 日志或三角密度变化。

### 7.2 `samples/lua/3d/3d_shadow_showcase.lua`

- 目标画面/交互：cube 在地面上投射方向光阴影，展示 shadow strength 变化。
- 参考来源：cpp-game-engine-book 第 25 章 Shadow Mapping、第 23 章光照；VSEngine2.1 Demo 15.22。
- 需要能力：light cast_shadow、shadow map pass、shadow strength setter。
- 当前可能缺口：Lua 已暴露 cast_shadow、shadow_strength 与 CSM cascade_splits；真实 shadow pass 视觉稳定性仍需在更多 GPU/驱动组合上复验。
- 最小实现路径：补 light shadow setter；地面 + 悬空 cube + 方向光。
- 实施状态：已完成可用 shadow 参数 API。当前 demo 调用 `set_directional_light_shadow(light, true, strength, 12.0, 32.0, 80.0)`，真实写入 `DirectionalLight3DComponent.cast_shadow/shadow_strength/cascade_splits`，悬空 caster 持续移动；同步用地面暗色投影 marker 保证截图主题稳定。
- 验收标准：日志包含 `shadow_param_api`、`set_directional_light_shadow=true`、`cast_shadow=true`、`cascade_splits=12.0/32.0/80.0`；地面有明确投影；shadow_strength 改变后阴影深浅变化。

### 7.3 `samples/lua/3d/3d_animation_basic.lua`

- 目标画面/交互：骨骼模型循环播放 Idle/Walk，并按时间切换。
- 参考来源：cpp-game-engine-book 第 18/19/20 章；VSEngine2.1 Demo 14.9/15.3/15.22。
- 需要能力：`add_animator_3d`、Animator3D FSM、skinned mesh 渲染、动画资源。
- 当前可能缺口：已具备最小 two-bone `.dskel/.danim/.dmesh/.dmat` 资源链路和状态验收；后续缺口是更完整角色资源、多 clip 动画、蒙皮视觉质量与第三人称角色控制器联动。
- 最小实现路径：先补状态查询 API 固定日志验收，再用 `data/animation/minimal_rig` 最小 two-bone 测试资源验收真实资源路径；后续从 reference 拷贝/转换角色资源到 `data/models/character`、`data/animation`。
- 实施状态：已完成最小真实动画资源包专项。当前 `config.lua` 默认配置 `animation/minimal_rig/two_bone.dmesh`、`two_bone.dmat`、`two_bone_idle_walk.danim`、`two_bone.dskel`；demo 创建 Animator3D + FSM + idle/walk 状态，新增 skinned mesh 资源实体，并保留分段 cube rig 作为截图语义兜底。`DSE_ENABLE_3D=OFF` Lua runtime 下由 `FramePipeline` 内置 Animator3D 更新链路驱动 `normalized_time/final_bone_matrices`，demo 调用 `get_animator_3d_state` 读取真实组件 state、normalized_time、clip_time、speed、loop、transition、final_bones 与 has_skeleton。
- 验收标准：日志必须包含 `animator_resource_chain`、`real_animation_resource`、`resource_paths_configured=true`、最小资源路径、`animator_state_api`、`get_animator_3d_state=true`、`state=idle`、`final_bones=`、`has_skeleton=true`、`skinned_mesh_resource`、`mesh_final_bones=`、`mesh_has_skeleton=true`；画面中最小资源 mesh 与 cube rig fallback 均可见，cube rig 保证 Idle/Walk 切换截图语义。

### 7.4 `samples/lua/3d/3d_character_third_person.lua`

- 目标画面/交互：第三人称相机跟随角色；角色移动、转向、攻击；动画状态切换。
- 参考来源：VSEngine2.1 Demo 14.27；cpp-game-engine-book 第 10/11/18/19/20 章。
- 需要能力：3D 输入、角色控制、third-person camera、Animator3D FSM、角色资源。
- 当前可能缺口：Lua `set_camera_follow` 偏 2D；真实角色移动控制与最小 Animator3D 资源联动已接入，后续仍缺 capsule/character controller、真实角色资产、多 clip 动画与攻击/移动独立 clip。
- 最小实现路径：cube 代替角色实现 follow；接入 `add_steering`、`set_steering_target`、`get_steering_state`，让角色根节点由真实 SteeringSystem 推动；复用 `data/animation/minimal_rig` two-bone 资源先验收角色 demo 的 Animator3D 资源链路，再接入真实角色骨骼模型和多 clip 动画。
- 实施状态：已完成角色 Steering + Animator3D 最小资源联动验收。当前使用 cube character rig 表达角色并保留截图 fallback，给根实体添加 `SteeringComponent` 和 Animator3D FSM，在 Update 中通过 `set_steering_target` 切换 seek/arrive 目标，并同步 run/attack Animator3D state；`DSE_ENABLE_3D=OFF` Lua runtime 下由 `FramePipeline` 内置 SteeringSystem 与 Animator3D 更新链路驱动 Transform 移动和 `final_bone_matrices`，第三人称相机跟随真实根节点位置，demo 通过 `get_steering_state` 回读 velocity/speed/max_velocity，并通过 `get_animator_3d_state` 回读最小资源 state/time/final_bones/has_skeleton。
- 验收标准：角色移动时相机稳定跟随；日志包含 `character_steering_api`、`add_steering=true`、`set_steering_target=true`、`get_steering_state=true`、`speed_nonzero=true`、`character_animation_resource`、`character_animator_state_api`、`resource_paths_configured=true`、最小资源路径、`character_skinned_mesh_resource`、`runtime_animation:`、`final_bones=2`、`mesh_final_bones=2`、`has_skeleton=true`、`mesh_has_skeleton=true`；攻击状态可触发并切换 arrive 目标。

### 7.5 `samples/lua/3d/3d_audio_spatial.lua`

- 目标画面/交互：音源绕 listener/camera 移动，戴耳机可感知左右/远近变化；画面用小球标记音源和 listener。
- 参考来源：cpp-game-engine-book 第 15.2 章 3D 音效。
- 需要能力：3D AudioSource、AudioListener、位置同步、距离衰减。
- 当前可能缺口：可用 3D source/listener/distance API 与最小真实 wav 音源已接入；后续主要补更丰富的空间音效资源与手动试听场景。
- 最小实现路径：补 `audio.set_3d_mode`、`audio.add_listener`、`audio.set_3d_distance`、`audio.get_source_state`，并提供 `data/audio/spatial/spatial_ping.wav` 作为可提交的最小循环提示音。
- 实施状态：已完成可用 3D Audio API 与实际音频资源接入。当前 demo 创建 listener/source 可视化 marker，调用 `dse.audio.add_listener`、`dse.audio.set_3d_mode`、`dse.audio.set_3d_distance` 与 `dse.audio.get_source_state`；`Config.demo_3d_audio_spatial.audio_path` 默认指向 `audio/spatial/spatial_ping.wav`，AudioSystem 每帧从 Transform 同步 listener/source 位置到底层 miniaudio 3D spatialization，并由日志回读 clip_loaded/spatial_enabled/runtime_handle。
- 验收标准：日志打印 source/listener position 与 `set_3d_mode/add_listener/set_3d_distance/get_source_state`；`audio_state_api=true`、`clip_loaded=true`、`spatial_enabled=true`、`runtime_handle_nonzero=true`；配置音频资源后声像随音源移动变化。

## 8. 推荐实施顺序

### 第一阶段：P0 可立即做

1. `3d_static_model`：先验证资源化 mesh/material，这是从 cube 到真实资产的最短路径。
2. `3d_material_showcase`：在资源/几何稳定后，系统展示材质参数。
3. `3d_lighting_showcase`：把现有方向光扩展为多光源，为综合场景和 shadow 打基础。
4. `3d_camera_showcase`：复杂场景前先把观察方式标准化。
5. `3d_textured_cube`：最后验证 texture/material 链路，因为它依赖 `.dmesh/.dmat` 和贴图资源。

### 第二阶段：P1 小幅补 API/资源

6. `3d_scene_showcase`：整合 P0 成果，形成可展示的 3D 小场景。（已完成）
7. `3d_skybox_environment`：补 SkyLight 绑定和环境资源。（已完成 SkyLight，环境资源待后续）
8. `3d_postprocess_showcase`：补后处理 setter/toggle。（已完成 bloom/color grading setter 与 `get_post_process_state` 状态回读；灰度/反相 effect mode 待后续）
9. `3d_particles_showcase`：补粒子参数 API与真实运行时状态验收。（已完成参数 setter + `get_particle_system_3d_state` + `DSE_ENABLE_3D=OFF` 内置 Particle3D 更新链路 + 可见 fallback markers）
10. `3d_physics_stack`：确认 PhysX 构建和刚体稳定后实现。（已完成 fallback，真实 PhysX 堆叠待启用构建后复验）

### 第三阶段：P2 资产/底层专项

11. `3d_terrain_heightmap`：补 heightmap 和 LOD 可视化。（已完成可用图片 heightmap 文件采样、terrain texture 绑定与 LOD 参数/日志验收；仍保留 marker grid 作为可视参考）
12. `3d_shadow_showcase`：补 shadow 开关与稳定验收。（已完成可用 shadow 参数 API：Lua 可设置方向光 cast_shadow、shadow_strength 与 CSM cascade_splits；demo 日志强制验收真实 API 调用，真实 shadow pass 视觉稳定性继续专项复验）
13. `3d_animation_basic`：补动画资源和 skinned mesh 验证。（已完成最小真实动画资源包：`data/animation/minimal_rig` 提供 two-bone `.dmesh/.dskel/.danim/.dmat`，demo 默认加载并通过 Animator3D 状态/骨骼 token 验收；真实角色、多 clip 和更完整蒙皮资产待后续）
14. `3d_character_third_person`：在动画和相机控制稳定后做角色综合 demo。（已完成 cube rig fallback + 真实 SteeringSystem 移动 + 最小 Animator3D 资源联动验收；真实 capsule/character controller、角色资源与多 clip 攻击/移动动画待补齐）
15. `3d_audio_spatial`：补 3D audio source/listener 后实现。（已完成可用 3D audio API 与最小真实 wav 音源：Lua 可设置 source 3D mode、listener、distance attenuation，并通过 `get_source_state` 回读 clip/runtime 状态；AudioSystem 同步 Transform 到 miniaudio）

### 第四阶段：P3 专项增强批次

16. `3d_physics_raycast_pick`：把 Lua `physics_3d_raycast` 接到真实 Physics3DSystem Raycast 后，验证准星/射线拾取、命中点和命中实体。（已完成可用 raycast 接入：优先 PhysX service，未启用时走 ECS Box/Sphere collider 几何 fallback；demo 可打印 hit/entity/position/normal/distance）
17. `3d_texture_material_slots`：补 `set_mesh_texture(entity, slot, path)` 或材质实例 texture slot API 后，验证 albedo/normal/roughness/emissive 多贴图链路。（已完成可用 texture slot API：Lua 可绑定 albedo/normal/metallic_roughness/emissive/occlusion；新增 `set_mesh_uvs/set_mesh_normals/set_mesh_tangents`，可为 Lua 手写 mesh author UV/normal/tangent 并驱动贴图采样）
18. `3d_terrain_lod_zones`：实现 Terrain heightmap sampling、resolution、LOD renderer 后，验证 near/mid/far LOD 分区和密度变化。（已完成可用 terrain height/LOD API：Lua 可设置 resolution、写入程序化 height grid、配置动态 LOD 参数并读取 current_lod；图片 heightmap 文件采样与 terrain texture 已在 `3d_terrain_heightmap` 专项补齐）

## 9. 首批最建议实现的 5 个 demo

1. `samples/lua/3d/3d_static_model.lua`
   - 从 `cube.lua` 的手写顶点自然过渡到 `.dmesh/.dmat`。
   - 最快验证资源链路。
2. `samples/lua/3d/3d_material_showcase.lua`
   - 从单 cube 材质过渡到材质矩阵。
   - 可作为材质参数回归截图。
3. `samples/lua/3d/3d_lighting_showcase.lua`
   - 从单方向光过渡到 Directional/Point/Spot。
   - 为 scene/shadow 打基础。
4. `samples/lua/3d/3d_camera_showcase.lua`
   - 从“能看 cube”过渡到“能观察复杂 3D 场景”。
   - 后续 demo 可复用相机 helper。
5. `samples/lua/3d/3d_textured_cube.lua`
   - 从材质 scalar 过渡到贴图资源。
   - 验证 UV、texture slot、dmat。

## 10. 验收与截图方案

| Demo | 日志验收 | 截图/视觉验收 | 交互/数值验收 |
|---|---|---|---|
| `3d_static_model` | mesh/material path 加载日志 | 资源 cube 与手写 cube 并排 | 资源 cube 旋转 |
| `3d_material_showcase` | 输出每个材质参数 | 多 cube 高光/发光/粗糙差异 | roughness/emissive 动态变化 |
| `3d_lighting_showcase` | 输出 light 类型和参数 | 多色光照区域 | PointLight 绕圈 |
| `3d_camera_showcase` | 输出 active camera | 不同视角截图序列 | free camera 或自动切换 |
| `3d_textured_cube` | dmat/texture slot 日志 | cube 表面有纹理 | 旋转观察各面 |
| `3d_scene_showcase` | 对象/光源/相机数量 | 小型场景完整可见 | free camera 巡游 |
| `3d_postprocess_showcase` | `postprocess_state_api`、`get_post_process_state=true`、`set_post_process_bloom=true`、`set_post_process_color=true`、`color_grading=true`、`gamma=` 日志 | bloom/color grading 参数差异，emissive 物体周围 bloom 主题可见 | Lua `get_post_process_state` 回读真实 `PostProcessComponent` enabled/bloom/threshold/intensity/color_grading/exposure/gamma/SSAO 状态，Update 中持续调节 bloom 与 exposure/gamma |
| `3d_skybox_environment` | skybox/skylight 参数 | 天空/环境色变化 | 环境强度变化 |
| `3d_particles_showcase` | `particle_runtime_api`、`get_particle_system_3d_state`、`active_particles`、`active_particles_nonzero=true`、`enabled`、`initialized=true` 日志 | 粒子喷泉主题可见，fallback marker 保底 | Lua `get_particle_system_3d_state` 返回真实 `ParticleSystem3DComponent` active/max/rate/life/size/speed/gravity/color/texture/enabled/initialized`；`DSE_ENABLE_3D=OFF` Lua runtime 下由内置 Particle3D 更新链路驱动 |
| `3d_physics_stack` | PhysX init + y 值 | cube 下落堆叠 | y 值下降后稳定 |
| `3d_terrain_heightmap` | terrain_heightmap_api、load_terrain_heightmap、set_terrain_texture、real_sampling 日志 | 图片 heightmap 采样出的非平面地形并绑定 terrain texture | Lua `load_terrain_heightmap` 返回 image/sample resolution；`set_terrain_texture` 返回 handle/size；`get_terrain_lod` 返回 current_lod |
| `3d_shadow_showcase` | shadow_param_api、set_directional_light_shadow、cast_shadow、cascade_splits 日志 | 地面投影与 fallback marker 共同保证阴影主题可见 | Lua `set_directional_light_shadow` 返回并写入 cast_shadow、shadow_strength、CSM cascade_splits，Update 中持续调节 shadow_strength |
| `3d_animation_basic` | `animator_resource_chain`、`real_animation_resource`、`resource_paths_configured=true`、最小资源路径、`animator_state_api`、`get_animator_3d_state=true`、`state=idle`、`final_bones=`、`has_skeleton=true`、`skinned_mesh_resource`、`mesh_final_bones=`、`mesh_has_skeleton=true` 日志 | `data/animation/minimal_rig` 最小资源 mesh 与分段 cube rig fallback 同屏可见；cube rig 继续保证截图语义 | Lua `get_animator_3d_state` 返回真实 Animator3D 组件 state、normalized_time、clip_time、speed、loop、transition、final_bones 与 has_skeleton；Lua runtime 内置 Animator3D 更新链路驱动最小资源骨骼矩阵 |
| `3d_character_third_person` | `character_steering_api`、`add_steering=true`、`set_steering_target=true`、`get_steering_state=true`、`speed_nonzero=true`、`character_animation_resource`、`character_animator_state_api`、`resource_paths_configured=true`、最小资源路径、`character_skinned_mesh_resource`、`runtime_animation:`、`final_bones=2`、`mesh_final_bones=2`、`has_skeleton=true`、`mesh_has_skeleton=true` 日志 | 角色+跟随相机，目标 marker 和最小 skinned mesh 可见，cube rig fallback 保留 | Lua `get_steering_state` 回读真实 SteeringComponent velocity/speed；Lua `get_animator_3d_state` 回读角色 Animator3D state/time/final_bones/has_skeleton；`DSE_ENABLE_3D=OFF` Lua runtime 下由内置 SteeringSystem + Animator3D 更新链路驱动角色根节点移动/攻击切换与最小资源骨骼矩阵 |
| `3d_audio_spatial` | real_3d_audio、get_source_state、clip_loaded、spatial_enabled、runtime_handle_nonzero 日志 | 音源/listener 标记和距离环可见 | Lua `get_source_state` 回读真实 AudioSourceComponent clip/runtime/3D 参数；最小 wav 音源随 Transform 绕 listener 移动 |
| `3d_physics_raycast_pick` | raycast hit/entity/position/normal/distance | ray beam、目标、命中 marker 清晰可见 | Lua `physics_3d_raycast` 返回真实命中信息；PhysX 不可用时 ECS collider fallback 仍可命中 |
| `3d_texture_material_slots` | mesh/material path、set_mesh_texture slot 与 mesh_authoring_api 日志 | albedo/roughness/emissive/normal slot 样本行可见，额外 authored quad 使用 Lua UV 贴图采样 | Lua `set_mesh_texture` 返回 handle/size；`set_mesh_uvs/set_mesh_normals/set_mesh_tangents` 返回 attribute count 并影响 MeshRenderSystem 顶点属性 |
| `3d_terrain_lod_zones` | TerrainComponent 参数、terrain_api 与 runtime_lod 日志 | near/mid/far 三段 tile 密度差异明显，TerrainSystem 使用程序化 height grid | Lua `set_terrain_params/set_terrain_height/get_terrain_lod` 可设置网格并返回 current_lod；图片 heightmap 文件采样与 terrain texture 已由 `3d_terrain_heightmap` 验收 |
| `3d_audio_spatial` | real_3d_audio、set_3d_mode/add_listener/set_3d_distance/get_source_state 与 source/listener position 日志 | 音源/listener 标记和距离环可见 | Lua `set_3d_mode/add_listener/set_3d_distance/get_source_state` 可配置并回读 3D 音源、listener、距离衰减、clip 加载和 runtime handle；AudioSystem 同步 Transform 到 miniaudio |

## 11. 当前引擎缺口与最小补齐路径

1. Lua demo 分发：在 `samples/lua/main.lua` 增加新增 demo key；在 `samples/lua/config.lua` 增加独立配置。
2. Lua mesh UV：已新增 `set_mesh_uvs/set_mesh_normals/set_mesh_tangents`，MeshRenderSystem 对 Lua 手写 mesh 使用显式 UV/normal/tangent；更完整的 `add_mesh_renderer_ex` 可后续作为便捷封装。
3. Texture slot：短期依赖 `.dmat`；中期新增 `set_mesh_texture(entity, slot, path)`。（已完成可用接入，支持 albedo/normal/metallic_roughness/emissive/occlusion；已补 Lua UV/normal/tangent authoring，手写 mesh 可直接显示贴图采样）
4. SkyLight：新增 Lua `add_sky_light`、`set_sky_light`。（已完成）
5. PostProcess：新增 Lua `set_post_process_enabled`、`set_bloom`、`set_exposure_gamma`、`set_post_effect_mode`。（已完成 `set_post_process_bloom`、`set_post_process_color` 与 `get_post_process_state`；enabled/bloom/exposure/gamma/color_grading 可调并可回读，灰度/反相 effect mode 与 shader gamma 参数化待后续）
6. Terrain：实现 heightmap 采样；Lua 暴露 resolution、LOD、texture。（已完成 resolution/程序化 height data/LOD 参数与 current_lod 查询；新增 `load_terrain_heightmap` 从图片文件采样高度并新增 `set_terrain_texture` 绑定 terrain texture）
7. Particle3D：Lua 暴露颜色、大小、速度、生命、重力、贴图路径，并提供真实运行时状态验收查询。（已完成 `set_particle_system_3d_params` 与 `get_particle_system_3d_state`；`DSE_ENABLE_3D=OFF` Lua runtime 下已由 `FramePipeline` 内置 Particle3D 更新链路驱动真实初始化与 active particle count）
8. Physics3D raycast：把 Lua `physics_3d_raycast` 接到真实 Physics3DSystem Raycast；未启用 PhysX 时使用 ECS 3D collider 几何 fallback。（已完成可用接入，待 PhysX 构建复验真实后端）
9. Animator3D：已新增 `get_animator_3d_state(entity)`，Lua 可查询 Animator3D 真实组件 state、normalized_time、clip_time、speed、loop、transition 与 final_bones；已准备 `data/animation/minimal_rig` 最小 two-bone `.dmesh/.dskel/.danim/.dmat` 资源包并接入 `3d_animation_basic` 自动验收。后续重点转向真实角色/多 clip 资产、蒙皮视觉质量和第三人称角色控制器联动。
9b. Character/Steering：已新增 `get_steering_state(entity)`，`set_steering_target` 现在返回是否成功；`DSE_ENABLE_3D=OFF` Lua runtime 下已由 `FramePipeline` 内置 SteeringSystem 更新链路驱动真实 velocity 与 Transform 移动，`3d_character_third_person` 已复用 `data/animation/minimal_rig` 资源并用 `get_animator_3d_state` 同时验收角色 Animator3D 资源联动。后续重点是 capsule/character controller、真实角色资产和多 clip 攻击/移动动画。
10. 3D Audio：已新增 `set_3d_mode`、`add_listener`、`set_3d_distance` 与 `get_source_state`，AudioSystem 使用 miniaudio spatialization 同步 source/listener Transform；已提供 `data/audio/spatial/spatial_ping.wav` 最小真实空间音效资源用于自动验收。
11. Shadow：已新增 `set_directional_light_shadow(entity, cast_shadow, shadow_strength, cascade0, cascade1, cascade2)`，Lua 可稳定配置方向光阴影开关、强度与 CSM 分段；真实 shadow pass 视觉稳定性继续专项复验。

## 12. 结论

优先落地 P0 的 5 个 demo：`3d_static_model`、`3d_material_showcase`、`3d_lighting_showcase`、`3d_camera_showcase`、`3d_textured_cube`。它们能从当前 triangle/square/cube 平滑过渡到 Mesh/Material、材质参数、多光源、相机控制、贴图链路，并且大概率不需要大规模底层改动。

随后以 `3d_scene_showcase` 整合 P0 成果，再逐步补齐 SkyLight、PostProcess、Particle、Physics、Terrain、Shadow、Animation、Character、3D Audio。资源从 `reference/` 复制时应统一进入 `data/` 分类目录，并记录来源与转换方式，避免 demo 依赖 reference 路径。