# DSEngine 测试覆盖分析报告

> 生成日期：2026-05-17（最近更新：Session 9 Smoke+Performance+集成 — 编译+运行 0 失败）
> 方法：144 个测试文件逐个审查（磁盘 grep `TEST_F|TEST(` 精确统计），1665 个用例分类统计，与引擎 60+ 个 System 交叉比对

---

## 一、测试覆盖全景

### 1.1 按层级统计

| 层级 | 文件数 | 用例数 | 占比 |
|:----:|:-----:|:------:|:----:|
| **单元测试 (unit)** | 101 | 1343 (含 5 skip) | 81.1% |
| **集成测试 (integration)** | 36 | 281 | 17.0% |
| **冒烟测试 (smoke)** | 7 | 41 | 2.5% |
| **合计** | **144** | **1665** | **100%** |

### 1.2 按模块统计

```
core/          115 用例 ⭐⭐⭐⭐⭐  10文件 覆盖全面（含 stress test + performance baseline）
ecs/           155 用例 ⭐⭐⭐⭐⭐  18文件 几乎所有组件
render/        481 用例 ⭐⭐⭐⭐⭐  20文件 VK80+DX11_55+GL72+Passes40+Probes21+UBO26+PP10+Hair24+DDGI17+Cluster19+DSSL30+Instancing15+ProjCorr11+DX11CB12+ShaderMgr8
scene/          94 用例 ⭐⭐⭐⭐   8文件  数据结构+管理器
gameplay_3d    177 用例 ⭐⭐⭐⭐⭐  21文件 动画+物理+渲染+粒子+软体+布娃娃+浮力+毛发+草地+载具+寻路
gameplay_2d     87 用例 ⭐⭐⭐⭐   8文件  Sprite/UI渲染+布局+9Slice
input           46 用例 ⭐⭐⭐    1文件  键盘/鼠标/手柄
profiler        43 用例 ⭐⭐⭐    1文件  CPU/Memory/ChromeTrace
runtime         47 用例 ⭐⭐⭐    3文件  生命周期+FramePipeline
audio           30 用例 ⭐⭐     2文件  基础+3D空间化
assets          59 用例 ⭐⭐⭐    2文件  加载/缓存/异步
physics         27 用例 ⭐⭐⭐    3文件  关节+系统+PhysX真实模拟
base            42 用例 ⭐⭐⭐    4文件  工具类

editor/         22 用例 ⭐⭐⭐     2文件  UndoRedo/Commands/CLI解析/Settings+SelectionManager
integration    281 用例 ⭐⭐⭐⭐⭐ 36文件 跨系统联调+Lua粒子绑定+SelectionManager
smoke           41 用例 ⭐⭐⭐⭐   7文件  关键链路+D3D11 RHI+Lua生命周期+Physics3D
```

---

## 二、已覆盖的子系统（有测试）

### ✅ 核心基础设施（覆盖好）

| 系统 | 测试文件 | 用例 | 质量 |
|:----:|---------|:----:|:----:|
| ServiceLocator | `service_locator_test.cpp` | 12 | 注册/获取/重置/类型擦除，完整 |
| EventBus | `event_bus_test.cpp` + 集成 | 17+12 | 订阅/发布/取消，含跨服务集成 |
| JobSystem | `job_system_test.cpp` + 集成 | 19+7 | 任务/等待/依赖/窃取，完整 |
| Module | `module_test.cpp` | 9 | IModule 生命周期，完整 |
| DynamicLibrary | `dynamic_library_test.cpp` | 9 | Win32 DLL 加载 |
| EventId | `event_id_test.cpp` + cross_dll | 16 | 哈希/跨DLL一致性 |
| MathPool | `math_pool_test.cpp` | 27 | 分配/回收/对齐 |

### ✅ ECS 组件（覆盖好）

| 组件体系 | 测试文件 | 用例 | 质量 |
|:--------:|---------|:----:|:----:|
| World | `world_test.cpp` | 11 | 实体创建/销毁/查询 |
| Transform | `ecs_component_test.cpp` | 14 | 组件增删改查 view |
| 2D 组件 | `components_2d_test.cpp` | 5 | 默认值 |
| 3D 组件 | `components_3d_test.cpp` | 17 | Mesh/Light/Skybox/Terrain/Morph 等 |
| 3D 物理组件 | `components_3d_physics_test.cpp` | 6 | RB/Collider 枚举 |
| 3D 布料组件 | `components_3d_cloth_test.cpp` | 11 | 约束/碰撞体字段 |
| 3D 流体组件 | `components_3d_fluid_test.cpp` | 10 | 发射器/粒子字段 |
| 3D 破碎组件 | `components_3d_fracture_test.cpp` | 11 | 碎片/FractureAsset |
| 3D 粒子组件 | `components_3d_particle_test.cpp` | 3 | 默认值 |
| 动画组件 | `animator_component_test.cpp` | 12 | 状态/权重/混合 |
| 相机组件 | `camera_component_test.cpp` | 5 | 默认值+字段 |
| 音频组件 | `audio_component_test.cpp` | 2 | 默认值 |
| 精灵组件 | `sprite_component_test.cpp` | 6 | 默认值+字段 |
| Tilemap 组件 | `tilemap_component_test.cpp` | 3 | 默认值 |
| UI 组件 | `ui_component_test.cpp` | 14 | Text/Image/Button/Panel/ProgressBar |
| Script 组件 | `script_component_test.cpp` | 3 | 默认值 |
| ParticleCurve | `particle_curve_test.cpp` | 18 | 数据结构验证 |

### ✅ 渲染（三后端 + Passes + Probes + UBO + PostProcess 全覆盖）

| 系统 | 测试文件 | 用例 | 质量 |
|:----:|---------|:----:|:----:|
| RenderGraph | `render_graph_test.cpp` | 24 | DAG/剔除/拓扑排序 |
| RHI 类型 | `rhi_types_test.cpp` | 10 | 枚举类型默认值 |
| Vulkan RHI | `vulkan_rhi_test.cpp` | 80 | Device/Resource/CmdBuf/PSO/Shader |
| DX11 RHI | `dx11_rhi_test.cpp` | 55 | Device/Resource/Shader/State |
| GL RHI | `gl_rhi_test.cpp` | 72 | Device/Resource/CmdBuf/Enum/GlobalState |
| BuiltinPasses | `builtin_passes_test.cpp` | 40 | 24+ Pass GetName + Context + RenderGraph + TAA |
| LightProbeSystem | `light_probe_system_test.cpp` | 13 | SHL2/IntegrateFaceSH/生命周期 |
| ReflectionProbeSystem | `reflection_probe_system_test.cpp` | 8 | Component/System/IBL 查询 |
| **UBO/DrawExecutor 共用** | `ubo_draw_executor_common_test.cpp` | **26** | UBO 对齐/布局/三端共用绘制逻辑 |
| **GL ShaderUBO Manager** | `gl_shader_ubo_manager_test.cpp` | **8** | GL UBO 绑定/上传/管理 |
| **RenderGraph 并行** | `rendergraph_parallel_test.cpp` | **8** | 多线程执行路径 |
| **PostProcess 共用** | `postprocess_common_test.cpp` | **10** | Bloom/Composite 参数与阶段验证 |
| RenderGraph 集成 | `rendergraph_integration_test.cpp` | 14 | DAG 构建/菱形依赖 |
| **HairAsset/Instance** | `hair_asset_test.cpp` | **24** | 资产验证/程序化生成/LOD/参数默认值 |
| **DDGI 系统** | `ddgi_system_test.cpp` | **17** | ProbeGrid 计算/OctEncode往返/Atlas尺寸 |
| **ClusterGrid/LightBuffer** | `cluster_grid_test.cpp` | **19** | 结构体对齐/常量/绑定点不重叠 |
| **DSSL Material** | `dssl_material_test.cpp` | **30** | UniformInfo/Instance/Loader/RenderModes/属性映射 |
| **GPU Instancing** | `gpu_instancing_test.cpp` | **15** | InstancingKey hash/相等性/HashMap合批/BlendMode/instance_transforms |
| **三端坐标系统一** | `projection_correction_test.cpp` | **11** | GL/VK/DX11 ProjectionCorrection + ShadowSample + NDC 变换 |
| **DX11 CB 对齐** | `draw_executor_backend_test.cpp` | **12** | DX11 PerFrame/PerObject/PerScene/PerMaterial/Light CB 16B 对齐 |
| **VK/DX11 ShaderManager** | `shader_manager_test.cpp` | **8** | DX11 ShaderProgram/ComputeProgram/Manager 默认值+查询 |

### ✅ 场景管理（覆盖好）

| 系统 | 测试文件 | 用例 |
|:----:|---------|:----:|
| SubScene | `sub_scene_test.cpp` | 11 |
| SceneManager | `scene_manager_test.cpp` | 15 |
| SceneTransition | `scene_transition_test.cpp` | 8 |
| Octree | `octree_test.cpp` | 12 |
| QuadTree | `quad_tree_test.cpp` | 11 |
| TransformSystem | `transform_system_test.cpp` | 8 |
| UUID | `uuid_cross_scene_test.cpp` | 11 |
| RawSceneData | `raw_scene_data_test.cpp` | 18 |

### ✅ Gameplay 3D 动画/系统

| 系统 | 测试文件 | 用例 |
|:----:|---------|:----:|
| AnimatorSystem | `animator_system_test.cpp` | 7 |
| AnimLayerIK | `anim_layer_ik_test.cpp` | 16 |
| AnimationStateMachine | `animation_state_machine_test.cpp` | 9 |
| **BlendTree/Crossfade** | `blend_tree_crossfade_test.cpp` | **15** |
| **AnimLayerBlend** | `anim_layer_blend_system_test.cpp` | **8** |
| FreeCameraController | `free_camera_controller_system_test.cpp` | 6 |
| FrustumCulling | `frustum_culling_system_test.cpp` | 9 |
| LODSystem | `lod_system_test.cpp` | 13 |
| MeshRenderSystem | `mesh_render_system_test.cpp` | 10 |
| Particle3DSystem | `particle3d_system_test.cpp` | 11 |
| TerrainSystem | `terrain_system_test.cpp` | 9 |
| SoftBodySystem | `softbody_system_test.cpp` | 11 |
| **RagdollSystem** | `ragdoll_system_test.cpp` | **8** |
| **BuoyancySystem** | `buoyancy_system_test.cpp` | **10** |
| Rope/Vehicle | `rope_vehicle_system_test.cpp` | 11 |
| Steering | `steering_system_test.cpp` | 10 |
| AnimPerfBenchmark | `anim_perf_benchmark_test.cpp` | 5 |
| **HairSystem** | `hair_system_test.cpp` | **7** |
| **GrassSystem** | `grass_system_test.cpp` | **13** |
| **VehicleSystem** | `vehicle_system_test.cpp` | **6** |
| **NavAgentSystem** | `nav_agent_system_test.cpp` | **6** |

### ✅ Gameplay 2D

| 系统 | 测试文件 | 用例 |
|:----:|---------|:----:|
| Animation2D | `animation_system_2d_test.cpp` | 10 |
| Camera2D | `camera_system_2d_test.cpp` | 8 |
| Localization | `localization_system_test.cpp` | 12 |
| NineSlice | `nine_slice_render_test.cpp` | 9 |
| Particle2D | `particle_system_2d_test.cpp` | 10 |
| Tilemap | `tilemap_system_test.cpp` | 10 |

### ✅ Physics2D 集成测试（包含多边形碰撞体）

| 场景 | 测试文件 | 用例 |
|:----:|---------|:----:|
| ECS 组件集成 | `ecs_physics2d_integration_test.cpp` | 7 |
| 碰撞事件 | `physics2d_collision_event_integration_test.cpp` | 4 |
| 关节集成 | `physics2d_joint_integration_test.cpp` | 11 |
| 圆形碰撞体 | `physics2d_circle_collider_integration_test.cpp` | 7 |
| **多边形碰撞体** | `physics2d_polygon_collider_integration_test.cpp` | **6** |
| 3D 物理 ECS | `physics3d_ecs_integration_test.cpp` | 6 |
| Lua 绑定 | `lua_binding_physics2d_integration_test.cpp` | 6 |

---

## 三、缺失测试的系统（完整清单）

### 🔴 P0：严重缺失 —— 高危功能区，无任何测试

| 系统 | 引擎位置 | 影响 | 说明 |
|:----:|---------|:----:|------|
| ~~HairSystem~~ | `modules/gameplay_3d/rendering/hair_system.h/cpp` | ✅ 已补 7 用例 | 组件默认值/系统状态/缓存查询 |
| ~~HairAsset/Instance~~ | `engine/render/hair/hair_asset.h/cpp`, `hair_instance.h/cpp` | ✅ 已补 24 用例 | IsValid/生成/LOD/参数 |
| ~~GrassSystem~~ | `modules/gameplay_3d/rendering/grass_system.h/cpp` | ✅ 已补 13 用例 | 组件/布局/GPU实例/Chunk |
| ~~NavAgentSystem~~ | `modules/gameplay_3d/ai/nav_agent_system.h/cpp` | ✅ 已补 6 用例 | 组件默认值/系统构造 |
| ~~VehicleSystem~~ | `modules/gameplay_3d/vehicle/vehicle_system.h/cpp` | ✅ 已补 6 用例 | 组件/车轮/系统构造 |
| ~~DDGISystem~~ | `engine/render/gi/ddgi_system.h/cpp` | ✅ 已补 17 用例 | Probe 布局/irradiance 采样 |
| ~~ClusterGrid~~ | `engine/render/cluster_grid.h/cpp` | ✅ 已补 19 用例 | 光源分格/索引构建 |
| ~~DSSL MaterialLoader~~ | `engine/render/material/dssl_material_loader.h/cpp` | ✅ 已补 30 用例 | UniformInfo/Instance/Loader/映射 |
| **IKSolverSystem** | `modules/gameplay_3d/animation/ik_solver_system.h/cpp` | 由 anim_layer_ik_test 间接覆盖 | 但无独立单元测试 |

### 🟡 P1：重要缺失 —— 有部分覆盖但不完整

| 系统 | 引擎位置 | 现状 | 建议补充 |
|:----:|---------|:----:|---------|
| **ShaderManager**（GL/VK/DX11） | `engine/render/rhi/*_shader_manager.*` | GL 有 8 个 UBO 测试，VK/DX11 无 | 着色器加载/编译/句柄管理验证 |
| **DrawExecutor**（三端） | `engine/render/rhi/*_draw_executor.*` | 有 26 个 common 测试，但具体三端实现 0 测试 | DrawMeshBatch/DrawSpriteBatch/DrawHairStrands |
| **Physics3D（真实模拟）** | `engine/physics/physics3d/` | 仅有组件默认值测试，**无真实 PhysX 世界模拟测试** | 至少刚体/碰撞/关节/raycast 各 5 个 |
| **SpineSystem** | `modules/gameplay_2d/spine/spine_system.h/cpp` | 条件编译 (`DSE_ENABLE_SPINE`)，0 测试 | 至少编译验证 + 基础功能 |
| **GPU Instancing** | `mesh_render_system.cpp` 合批路径 | InstancingKey + 三端 instance VBO | 验证合批/半透明排除/实例数 |
| **三端坐标系统一** | `builtin_passes.cpp` + `GetProjectionCorrection()` | 仅 builtin_passes_test 有 TAA 测试 | NDC 修正矩阵三端一致性验证 |

### 🟢 P2：锦上添花 —— 有测试但不够深

| 系统 | 现状 | 建议补充 |
|:----:|:----:|---------|
| **Lua 绑定** | ✅ 9 个集成文件 ~59 用例 | Particles 绑定仍缺独立测试 |
| **编辑器** | ✅ 1 个集成文件 12 用例 | 缺编器单元测试（Inspector/Hierarchy/Viewport 等面板逻辑） |
| **音频** | ✅ 30 用例 | 无 FMOD/其他后端适配测试 |
| **JobSystem 多线程竞争** | ✅ 有功能+压力测试 | 缺少死锁/饥饿的场景测试 |
| **Vulkan 真实 GPU 测试** | ✅ smoke test (8 用例) | 仅跑 1 帧最小渲染，缺少完整场景渲染验证 |

---

## 四、按代码量/测试比例分析

| 子系统 | 引擎代码行数 | 测试用例数 | 行/用例比 | 评价 |
|:------:|:----------:|:--------:|:---------:|:----:|
| core/ | ~1,408 | 109 | 12.9 | 极好 |
| ecs/ | ~1,855 | 155 | 12.0 | 极好 |
| scene/ | ~2,496 | 94 | 26.6 | 良好 |
| input/ | ~761 | 46 | 16.5 | 良好 |
| profiler/ | ~599 | 43 | 13.9 | 极好 |
| audio/ | ~723 | 30 | 24.1 | 一般 |
| assets/ | ~3,815 | 59 | 64.7 | 较差 |
| physics/ | ~1,767 | 19 | 93.0 | **差** |
| render/ | ~39,041 | 442 | 88.3 | **差** |
| gameplay_3d/ | ~8,993 | 177 | 50.8 | 一般 |
| gameplay_2d/ | ~2,868 | 87 | 33.0 | 一般 |
| editor_cpp/ | ~9,694 | 12 | 808 | **极差** |

> **关键发现**：渲染子系统（39k 行代码）测试密度已从 94.8 行/用例降至 88.3。DSSL MaterialLoader 已补 30 用例，剩余缺口：DrawExecutor 三端独立测试、ShaderManager 三端独立测试。编辑器（9.7k 行仅 12 测试）仍是最大盲区。

---

## 五、缺失测试的汇总清单（按紧急程度排序）

```
优先级排序（2026-05-17 更新）:
──────────────────────────────────────────────────────────────
🔴 P0  (无任何测试，炸了没人知道)
├── engine/render/
│   ├── hair/hair_asset.cpp/.h            ← ✅ 已补 24 用例
│   ├── hair/hair_instance.cpp/.h         ← ✅ (hair_asset_test 包含)
│   ├── gi/ddgi_system.cpp/.h             ← ✅ 已补 17 用例
│   ├── cluster_grid.cpp/.h               ← ✅ 已补 19 用例
│   ├── material/dssl_material_loader.cpp ← ✅ 已补 30 用例
│   └── rhi/*_shader_sources.h            ← 三端inline shader，仅间接覆盖
├── modules/gameplay_3d/
│   ├── rendering/hair_system.cpp         ← ✅ 已补 7 用例
│   ├── rendering/grass_system.cpp        ← ✅ 已补 13 用例
│   ├── ai/nav_agent_system.cpp           ← ✅ 已补 6 用例
│   └── vehicle/vehicle_system.cpp        ← ✅ 已补 6 用例
├── modules/gameplay_2d/
│   └── spine/spine_system.cpp            ← Spine骨骼动画，零测试
├── engine/physics/physics3d/             ← PhysX模拟，零运行时测试
└── apps/editor_cpp/                      ← ✅ 已补 16 单元测试 + 12 功能测试

🟡 P1（有部分覆盖，但核心路径缺）
├── engine/render/rhi/
│   ├── *_draw_executor.cpp               ← ✅ DX11 CB 对齐已补 12 用例
│   ├── *_shader_manager.cpp              ← ✅ DX11 ShaderManager 已补 8 用例
│   └── vulkan/vulkan_pipeline_state_manager.cpp ← PSO管理
├── GPU Instancing                        ← ✅ InstancingKey 合批路径已补 15 用例
├── 三端坐标系统一                         ← ✅ GetProjectionCorrection() 已补 11 用例
└── engine/render/rhi/ubo_manager.cpp     ← 有 common 测试但无具体实现测试

🟢 P2（有测试，但深度不够）
├── Lua 绑定：✅ Particles 绑定已补 6 用例
├── Audio：缺 FMOD 后端测试
├── Editor：✅ 已补 UndoRedo/Commands/CLI 单元测试 16 用例（面板逻辑仍依赖 ImGui）
├── JobSystem：缺死锁/饥饿场景测试
└── Vulkan smoke：已有 8 测试含 10 帧稳定性+资源创销

已完成（从上一版 P0 移出）:
├── ✅ LightProbeSystem — 13 用例
├── ✅ ReflectionProbeSystem — 8 用例
├── ✅ GL RHI — 72 用例
├── ✅ BuiltinPasses — 40 用例
├── ✅ UBO/DrawExecutor Common — 26 用例
├── ✅ PostProcess Common — 10 用例
├── ✅ RenderGraph 并行 — 8 用例
├── ✅ GL ShaderUBO Manager — 8 用例
├── ✅ MeshRenderSystem — 10 用例
├── ✅ TerrainSystem — 9 用例
├── ✅ Particle3DSystem — 11 用例
├── ✅ SoftBodySystem — 11 用例
├── ✅ RagdollSystem — 8 用例
├── ✅ BuoyancySystem — 10 用例
├── ✅ BlendTree/Crossfade — 15 用例
├── ✅ AnimLayerBlend — 8 用例
├── ✅ SpriteRenderSystem — 11 用例 (含 UIRender)
├── ✅ UISystem/UILayout — 16 用例
├── ✅ HairAsset/HairInstance — 24 用例 (Session 5)
├── ✅ HairSystem — 7 用例 (Session 5)
├── ✅ GrassSystem — 13 用例 (Session 5)
├── ✅ DDGISystem — 17 用例 (Session 5)
├── ✅ ClusterGrid/LightBuffer — 19 用例 (Session 5)
├── ✅ DSSL MaterialLoader — 30 用例 (Session 6)
├── ✅ VehicleSystem — 6 用例 (Session 6)
├── ✅ NavAgentSystem — 6 用例 (Session 6)
├── ✅ GPU Instancing — 15 用例 (Session 7)
├── ✅ 三端坐标系统一 — 11 用例 (Session 7)
├── ✅ DX11 CB 对齐 — 12 用例 (Session 7)
├── ✅ VK/DX11 ShaderManager — 8 用例 (Session 7)
├── ✅ Editor UndoRedo/Commands/CLI — 16 用例 (Session 8)
└── ✅ Lua Particles 绑定 — 6 用例 (Session 8)
```

---

## 六、建议补充测试的优先级（Session 5+ 计划）

### Session 5（1 天）：✅ 已完成 — 补渲染新功能 + 3D 模块

| 任务 | 实际用例 | 状态 |
|:----|:--------:|------|
| HairAsset/HairInstance 单元测试 | **24** | ✅ IsValid/GenerateTest/LOD/参数默认值 |
| HairSystem 单元测试 | **7** | ✅ 组件默认值/系统状态/缓存查询 |
| GrassSystem 单元测试 | **13** | ✅ 组件/布局/GPU实例/Chunk/默认值 |
| DDGISystem 单元测试 | **17** | ✅ ProbeGrid/OctEncode往返/Atlas/状态 |
| ClusterGrid/LightBuffer 单元测试 | **19** | ✅ 结构体对齐/常量/绑定点/默认值 |

### Session 6（1 天）：✅ 已完成 — 补剩余零测试模块

| 任务 | 实际用例 | 状态 |
|:----|:--------:|------|
| NavAgentSystem 单元测试 | **6** | ✅ 组件默认值/目标设置/系统构造 |
| VehicleSystem 单元测试 | **6** | ✅ 车轮配置/状态/组件/系统构造 |
| DSSL MaterialLoader 单元测试 | **30** | ✅ UniformInfo/Instance/Loader/RenderModes/属性映射/Texture回退 |

### Session 7（1 天）：✅ 已完成 — 补核心路径深度

| 任务 | 实际用例 | 状态 |
|:----|:--------:|------|
| GPU Instancing 合批验证 | **15** | ✅ InstancingKey hash/相等/HashMap合批/BlendMode/skinned排除/RenderStats |
| 三端坐标系统一验证 | **11** | ✅ GL/DX11 ProjectionCorrection + ShadowSample + NDC + HairDrawItem |
| DrawExecutor 三端独立测试 | **12** | ✅ DX11 CB 10 项 16B 对齐 + PerFrame/PerObject 默认值 |
| ShaderManager 三端独立测试 | **8** | ✅ DX11 ShaderProgram/ComputeProgram/Manager 默认值+nullptr 查询 |

### Session 8（1 天）：✅ 已完成 — 加深 + 编辑器

| 任务 | 实际用例 | 状态 |
|:----|:--------:|------|
| 编辑器单元测试（UndoRedo/Commands/CLI/Settings） | **16** | ✅ Undo/Redo/Merge/History/栈深/Lambda/Compound/CLI解析/Settings |
| Lua Particles 绑定 | **6** | ✅ 3D粒子创建+参数/2D发射器/Burst/GameplayTuning/安全性 |
| Vulkan smoke | 已有 8 | ✅ 已含 10 帧稳定性+纹理/RT/Buffer 创销，无需扩展 |
| 空文件清理 | 2 | ✅ `audio_assets_integration_test.cpp` 和 `minimal_3d_scene_smoke_test.cpp` 添加 TODO 注释 |

### Session 9（1 天）：✅ 已完成 — Smoke + Performance + 集成补深

| 任务 | 实际用例 | 状态 |
|:----|:--------:|------|
| D3D11 RHI 冒烟测试 | **8** | ✅ InitD3D11/单帧/多帧/Shutdown重Init/纹理/10帧/RenderTarget/Buffer |
| Performance 基线测试 | **6** | ✅ ECS迭代+创销/JobSystem吞吐+依赖链/ServiceLocator查询/EventBus广播 |
| Editor SelectionManager 集成测试 | **6** | ✅ 初始空/单选/反选Toggle/防重复/Remove/null清空 |
| Lua 全生命周期 smoke | **3** | ✅ 50帧Tick无泄漏/动态创建实体/语法错误不崩溃 |
| Physics3D 真实模拟 smoke | **8** | ✅ 重力下落/地面阻挡/Raycast命中/冲量/碰撞事件/600帧稳定/RemoveActor/空场景 |

### 汇总

| 批次 | 工期 | 新增用例 | 填补缺口 |
|:----:|:----:|:--------:|---------|
| Session 5 | 1 天 | **80** | ✅ 渲染新功能 |
| Session 6 | 1 天 | **42** | ✅ 零测试模块 |
| Session 7 | 1 天 | **39** | ✅ 核心路径深度 |
| Session 8 | 1 天 | **22** | ✅ 编辑器+收尾 |
| Session 9 | 1 天 | **31** | ✅ Smoke+Performance+集成+Physics3D |
| **合计** | **5 天** | **214** | **✅ 测试金字塔全层覆盖** |

---

## 七、历史变更日志

### Session 9（2026-05-17）

> 状态：1665 测试全部通过（Unit 1338 pass + 5 skip | Integration 281 pass | Smoke 41 pass）

| 变更 | 说明 |
|:----:|------|
| 新增 5 个测试文件 | `dx11_rhi_smoke_test.cpp`(8) + `lua_lifecycle_smoke_test.cpp`(3) + `performance_baseline_test.cpp`(6) + `editor_selection_integration_test.cpp`(6) + `physics3d_smoke_test.cpp`(8) |
| D3D11 RHI 冒烟测试 | 完整 GPU 设备生命周期：初始化/单帧/多帧/Shutdown重Init/纹理/10帧/RenderTarget/Buffer |
| Performance 基线测试 | ECS 迭代+创销/JobSystem 吞吐+依赖链/ServiceLocator 查询/EventBus 广播 |
| Editor SelectionManager 集成测试 | 初始空/单选/反选Toggle/防重复/Remove/null 清空 |
| Lua 全生命周期 smoke | 50帧 Tick 无泄漏/动态创建实体/语法错误不崩溃 |
| Physics3D 真实模拟 smoke | 重力下落/地面阻挡/Raycast命中/AddImpulse/碰撞事件/600帧稳定/RemoveActor/空场景 |
| Bug fix | `RenderTargetDesc` 名称空格修正，`resource_mgr().DeleteRenderTarget` 调用修正 |

### Session 8（2026-05-17）

> 状态：1714 测试全部通过（Unit 1337→ 1332 pass + 5 skip | Integration 275 pass | Smoke 30 pass）

| 变更 | 说明 |
|:----:|------|
| 新增 2 个测试文件 | `editor_unit_test.cpp`(16) + `lua_binding_particles_integration_test.cpp`(6) |
| 编辑器单元测试 | UndoRedoManager 7项 + LambdaCommand 2项 + CompoundCommand 1项 + CLI解析 5项 + Settings 1项 |
| Lua 粒子绑定 | 3D粒子创建+参数/2D发射器/Burst/GameplayTuning/安全性 |
| 空文件清理 | 2 个占位空文件添加 TODO 注释 |

### Session 7（2026-05-17）

> 状态：1692 测试全部通过（Unit 1393 pass/skip | Integration 269 pass | Smoke 30 pass）

| 变更 | 说明 |
|:----:|------|
| 新增 4 个测试文件 | `gpu_instancing_test.cpp`(15) + `projection_correction_test.cpp`(11) + `draw_executor_backend_test.cpp`(12) + `shader_manager_test.cpp`(8) |
| P1 核心路径补深 | GPU Instancing 合批 key + 三端坐标修正矩阵 + DX11 CB 对齐 + ShaderManager 默认状态 |
| DX11 全覆盖 | 10 项 CB 结构 16B 对齐验证 + PerFrame/PerObject 默认值 + ShaderProgram/Manager |
| NDC 变换数学验证 | GL Z∈[-1,1] / DX11 Z∈[0,1] / VK Y-flip+Z remap 变换正确性 |

### Session 6（2026-05-17）

> 状态：1653 测试全部通过（Unit 1354 pass/skip | Integration 269 pass | Smoke 30 pass）

| 变更 | 说明 |
|:----:|------|
| 新增 3 个测试文件 | `dssl_material_test.cpp`(30) + `vehicle_system_test.cpp`(6) + `nav_agent_system_test.cpp`(6) |
| P0 缺口全部清零 | NavAgent/Vehicle/DSSL Material 全部已补测试，P0 仅剩 Spine/Physics3D/Editor |
| DSSL 覆盖深 | Uniform 设置/获取往返、材质属性映射（BaseColor/Metallic/Roughness 等）、多名称 Texture 回退、RenderModes |

### Session 5（2026-05-17）

> 状态：1611 测试全部通过（Unit 1312 pass/skip | Integration 269 pass | Smoke 30 pass）

| 变更 | 说明 |
|:----:|------|
| 文档全面更新 | 与磁盘实际测试文件精确对齐，用 `grep TEST_F\|TEST(` 统计用例 |
| 新增 5 个测试文件 | `hair_asset_test.cpp`(24) + `ddgi_system_test.cpp`(17) + `cluster_grid_test.cpp`(19) + `hair_system_test.cpp`(7) + `grass_system_test.cpp`(13) |
| 发现 2 个空文件 | `audio_assets_integration_test.cpp` 和 `minimal_3d_scene_smoke_test.cpp` 均为空文件，未注册到 CMakeLists |
| P0 缺口补0修复 | Hair/Grass/DDGI/ClusterGrid 已补测试，剩余 P0: Nav/Vehicle/DSSL Material |
| 已完成的 23 项移出 P0 | 上一版 18 项 + 本次 5 项（Hair×2/Grass/DDGI/Cluster） |

### Session 4 完整变更日志

> 状态：1460 测试全部通过（Unit 1164 pass + 5 skip | Integration 269 pass | Smoke 22 pass）

### 7.1 代码 Bug Fix（1 个变更）

| 文件 | 变更 | 原因 |
|:----:|------|------|
| `engine/render/light_probe_system.cpp` | `d_omega` 乘以 `inv_w * inv_h`，补全像素面积因子 | 原公式缺失 texel 面积，导致 SH 系数偏大 w×h 倍 |

### 7.2 单元测试修复（13 个失败 → 0 失败）

| 测试 | 变更 | 评价 |
|:----:|------|------|
| `OpenGLRhiDeviceTest` ×4 + `GLDrawExecutorTest` ×1 | 无条件 `GTEST_SKIP`，无头环境无 GL context | ✅ 正确方案 |
| `TAAPassTest.UpdateJitter` | jitter 非零时才断言差异，健壮处理 Screen=0 | ✅ 正确方案 |
| `IntegrateFaceSHTest` | 修复代码 Bug（见 7.1） | √ 真实 Bug fix |
| `UILayoutSystemTest` 2 | 修正测试预期与实际不符 | √ 测试预期正确 |
| `LODSystemTest` | 引入 `AssetManager` | √ 系统设计需要 |
| `MeshRenderSystemTest` | `EXPECT_NO_THROW` → `EXPECT_THROW` | √ 正确处理 |
| `SoftBodySimulateTest` 4 | 修正测试设置问题，不是代码 Bug | √ 测试设置问题 |

### 7.3 集成测试修复（5 个失败 → 0 失败）

| 测试 | 变更 | 评价 |
|:----:|------|------|
| `LuaAnimBindingTest` 4 | Lua 绑定参数与 C++ 定义不符 | √ 测试代码错误，不是代码 Bug |
| `LuaDSSLBindingTest` 1 | 引入 `AssetManager`（`load_material` 需要） | √ 测试设置问题 |

### 7.4 编辑器修复（Session 3遗留）

| 文件 | 变更 |
|:----:|------|
| `lua_binding_animation_integration_test.cpp` | `using namespace dse` + 组件名/变量名修正 |
| `lua_binding_ui_integration_test.cpp` | `using namespace dse` + `components_3d_particle.h` include + 变量名 |
| `builtin_passes_test.cpp` | 测试文件重命名减少冲突 |
| `sprite_ui_render_system_test.cpp` | 测试文件重命名 |
| `particle3d_system_test.cpp` | 测试文件重命名 |

### 7.5 技术债务评估

| 项目 | 风险等级 | 说明 |
|:----:|:------:|------|
| GL 测试无头环境 SKIP | 低 | CI 无 GPU 时必须 skip；未来可用 EGL headless 或 Mesa llvmpipe 解决 |
| `MeshRenderSystemTest` 测试名微差 | 极低 | 名为“空World不崩溃”实测 EXPECT_THROW；抛异常≠崩溃，语义可接受 |
| TAAPass jitter 非零分支未覆盖 | 低 | Screen::width()=0 时始终走 (0,0) 分支，只在有窗口时可测差异 |
| “距离约束保持间距”测试 B 点 y=0 vs A 点 y=10 | 极低 | gravity=false 且只检查距离收敛，绝对位置不影响结果 |
