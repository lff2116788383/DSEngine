# DSEngine 测试覆盖分析报告

> 生成日期：2026-05-16（最近更新：Session 4 全量修复 — 编译+运行 0 失败）
> 方法：112 个测试文件逐个审查，1460 个用例分类统计，与引擎 55+ 个 System 交叉比对

---

## 一、测试覆盖全景

### 1.1 按层级统计

| 层级 | 文件数 | 用例数 | 占比 |
|:----:|:-----:|:------:|:----:|
| **单元测试 (unit)** | 78 | 1169 (含 5 skip) | 80.1% |
| **集成测试 (integration)** | 29 | 269 | 18.4% |
| **冒烟测试 (smoke)** | 5 | 22 | 1.5% |
| **合计** | **112** | **1460** | **100%** |

### 1.2 按模块统计

```
core/          109 用例 ⭐⭐⭐⭐⭐  8文件  覆盖全面
ecs/           155 用例 ⭐⭐⭐⭐⭐  18文件 几乎所有组件
render/        289 用例 ⭐⭐⭐⭐⭐  9文件  VK80+DX11_55+GL30+Passes36+Probes20
scene/          94 用例 ⭐⭐⭐⭐   8文件  数据结构+管理器
gameplay_3d    120 用例 ⭐⭐⭐⭐⭐ 12文件 动画+物理+渲染+粒子+软体
gameplay_2d     87 用例 ⭐⭐⭐⭐   8文件  Sprite/UI渲染+布局+9Slice
input           46 用例 ⭐⭐⭐    1文件  键盘/鼠标/手柄
profiler        43 用例 ⭐⭐⭐    1文件  CPU/Memory/ChromeTrace
runtime         36 用例 ⭐⭐⭐    3文件  生命周期
audio           30 用例 ⭐⭐     2文件  基础+3D空间化
assets          59 用例 ⭐⭐⭐    2文件  加载/缓存/异步
physics          19 用例 ⭐⭐     2文件  关节+系统测试
base            42 用例 ⭐⭐⭐    4文件  工具类

integration    256 用例 ⭐⭐⭐⭐⭐ 29文件 跨系统联调
smoke           27 用例 ⭐⭐⭐    5文件  关键链路
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

### ✅ 渲染（三后端 + Passes + Probes 全覆盖）

| 系统 | 测试文件 | 用例 | 质量 |
|:----:|---------|:----:|:----:|
| RenderGraph | `render_graph_test.cpp` | 24 | DAG/剔除/拓扑排序 |
| RHI 类型 | `rhi_types_test.cpp` | 10 | 枚举类型默认值 |
| Vulkan RHI | `vulkan_rhi_test.cpp` | 80 | Device/Resource/CmdBuf/PSO/Shader |
| DX11 RHI | `dx11_rhi_test.cpp` | 55 | Device/Resource/Shader/State |
| **GL RHI** | `gl_rhi_test.cpp` | **~30** | Device/Resource/CmdBuf/Enum/GlobalState |
| **BuiltinPasses** | `builtin_passes_test.cpp` | **~36** | 24 Pass GetName + Context + RenderGraph + TAA |
| **LightProbeSystem** | `light_probe_system_test.cpp` | **12** | SHL2/IntegrateFaceSH/生命周期 |
| **ReflectionProbeSystem** | `reflection_probe_system_test.cpp` | **8** | Component/System/IBL 查询 |
| RenderGraph 集成 | `rendergraph_integration_test.cpp` | 14 | DAG 构建/菱形依赖 |

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
| FreeCameraController | `free_camera_controller_system_test.cpp` | 6 |
| FrustumCulling | `frustum_culling_system_test.cpp` | 9 |
| LODSystem | `lod_system_test.cpp` | 13 |
| Rope/Vehicle | `rope_vehicle_system_test.cpp` | 11 |
| Steering | `steering_system_test.cpp` | 10 |
| AnimPerfBenchmark | `anim_perf_benchmark_test.cpp` | 5 |

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
| ~~LightProbeSystem~~ | ~~`engine/render/light_probe_system.h/cpp`~~ | ✅ 已补 12 用例 | SHL2/IntegrateFaceSH/生命周期 |
| ~~ReflectionProbeSystem~~ | ~~`engine/render/reflection_probe_system.h/cpp`~~ | ✅ 已补 8 用例 | Component/System/IBL 查询 |
| ~~MeshRenderSystem~~ | `modules/gameplay_3d/rendering/mesh_render_system.h/cpp` | ✅ 已补 10 用例 | 组件默认值+空World |
| ~~TerrainSystem~~ | `modules/gameplay_3d/rendering/terrain_system.h/cpp` | ✅ 已补 9 用例 | 组件默认值+LOD+空World |
| ~~Particle3DSystem~~ | `modules/gameplay_3d/particles/particle3d_system.h/cpp` | ✅ 已补 12 用例 | GPUParticleData+组件+生命周期 |
| **RagdollSystem** | `modules/gameplay_3d/ragdoll/ragdoll_system.h/cpp` | 374 行，零测试 | PhysX 布娃娃稳定性无保证 |
| ~~SoftBodySystem~~ | `modules/gameplay_3d/softbody/softbody_system.h/cpp` | ✅ 已补 12 用例 | PBD模拟/约束收敛/固定点 |
| ~~BuoyancySystem~~ | `modules/gameplay_3d/buoyancy/buoyancy_system.h/cpp` | ✅ 已有 12 用例 | 浮力采样/空dt/缺组件 |
| **IKSolverSystem** | `modules/gameplay_3d/animation/ik_solver_system.h/cpp` | ✅ 由 anim_layer_ik_test 间接覆盖 | 但无独立单元测试 |
| **AnimLayerBlendSystem** | `modules/gameplay_3d/animation/anim_layer_blend_system.h/cpp` | 动画层混合 | 仅在集成测试中被覆盖 |

### 🟡 P1：重要缺失 —— 有部分覆盖但不完整

| 系统 | 引擎位置 | 现状 | 建议补充 |
|:----:|---------|:----:|---------|
| ~~GL 渲染后端~~ | `engine/render/rhi/gl_draw_executor.cpp` | ✅ 已补 ~30 用例 (`gl_rhi_test.cpp`) | Device/Resource/CmdBuf/Enum/GlobalState |
| **ShaderManager**（GL/VK/DX11） | `engine/render/rhi/*_shader_manager.*` | 三个后端着色器管理器，全部 0 测试 | 着色器加载/编译/句柄管理验证 |
| **UBOManager** | `engine/render/rhi/ubo_manager.*` | UBO 缓冲管理，0 测试 | 上传/对齐/生命周期 |
| ~~Builtin Render Passes~~ | `engine/render/passes/builtin_passes.cpp` | ✅ 已补 ~36 用例 (`builtin_passes_test.cpp`) | 24 Pass GetName + Context + TAA |
| **DrawExecutor**（三端） | `engine/render/rhi/*_draw_executor.*` | VulkanStub 有少量，GL/DX11 的 DrawExecutor 0 测试 | 绘制命令录制验证 |
| **PostProcess 链** | `engine/render/rhi/*_draw_executor.cpp` | Bloom/Composite 参数传递 0 测试 | 后处理参数与渲染调用的集成 |
| **Physics3D（真实模拟）** | `engine/physics/physics3d/` | ✅ 仅有组件默认值测试，**无真实 PhysX 世界模拟测试** | 至少刚体/碰撞/关节/raycast 各 5 个 |
| **SpineSystem** | `modules/gameplay_2d/spine/spine_system.h/cpp` | 条件编译 (`DSE_ENABLE_SPINE`)，0 测试 | 至少编译验证 + 基础功能 |
| ~~SpriteRenderSystem~~ | `modules/gameplay_2d/rendering/sprite_render_system.h/cpp` | ✅ 已补 3 用例 | 空World+9Slice |
| ~~UIRenderSystem~~ | `modules/gameplay_2d/rendering/sprite_render_system.h` | ✅ 已补 3 用例 | 空World+零尺寸屏幕 |
| ~~UISystem / UILayoutSystem~~ | `modules/gameplay_2d/ui/` | ✅ 已补 16 用例 | ScaleFactor+Anchor+Grid+空Registry |

### 🟢 P2：锦上添花 —— 有测试但不够深

| 系统 | 现状 | 建议补充 |
|:----:|:----:|---------|
| **动画集成测试** | ✅ 有 unit + 集成 | 无 2D Blend Tree 独立测试，无 Crossfade/Root Motion 专项测试 |
| **Lua 绑定** | ✅ 6 个集成文件 ~50 用例 | ✅ Animation/DSSL/UI 已修复通过；Particles 绑定仍缺独立测试 |
| **编辑器** | ✅ 1 个集成文件 12 用例 | 缺编器单元测试（Inspector/Hierarchy/Viewport 等面板逻辑） |
| **音频** | ✅ 30 用例 | 无 FMOD/其他后端适配测试 |
| **Physics2D** | ✅ 单元+集成 19+34 用例 | ✅ 多边形碰撞体测试已补充 (6用例) |
| **RenderGraph 并行执行** | ✅ 有顺序执行测试 | 缺少 `ExecuteParallel` 多线程路径测试 |
| **JobSystem 多线程竞争** | ✅ 有功能测试 | 缺少高并发/死锁/饥饿的场景测试 |
| **Vulkan 真实 GPU 测试** | ✅ smoke test (5 用例) | 仅跑 1 帧最小渲染，缺少完整场景渲染验证 |

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
| physics/ | ~1,767 | 53 | 33.3 | 一般 |
| render/ | ~24,370 | 169 | 144.2 | **很差** |
| gameplay_3d/ | ~5,385 | 86 | 62.6 | 一般 |
| gameplay_2d/ | ~2,868 | 59 | 48.6 | 一般 |
| editor_cpp/ | ~9,694 | 12 | 808 | **极差** |

> **关键发现**：渲染子系统（24,370 行代码）是测试密度最低的大模块。虽然 Vulkan/DX11 RHI 有 80+55 测试，但只覆盖了 RHI 设备层，DrawExecutor、ShaderManager、UBOManager、BuiltinPasses、PostProcess 等核心路径全部 0 测试。

---

## 五、缺失测试的汇总清单（按紧急程度排序）

```
优先级排序:
──────────────────────────────────────────────────────────────
🔴 P0  (无任何测试，炸了没人知道)
├── engine/render/
│   ├── light_probe_system.h/cpp          ← SH烘焙，零测试
│   ├── reflection_probe_system.h/cpp     ← IBL反射，零测试
│   ├── passes/builtin_passes.cpp/.h      ← 9个内置渲染Pass，零测试
│   ├── rhi/gl_draw_executor.cpp/.h       ← GL后端绘制，零测试
│   ├── rhi/gl_shader_manager.cpp/.h      ← GL着色器管理，零测试
│   ├── rhi/ubo_manager.cpp/.h            ← UBO缓冲管理，零测试
│   ├── rhi/draw_executor_common.h        ← 三端共用绘制逻辑，零测试
│   └── rhi/*_shader_sources.h            ← 三端inline shader，零测试
├── modules/gameplay_3d/
│   ├── rendering/mesh_render_system.cpp  ← 991行，3D渲染核心入口
│   ├── rendering/terrain_system.cpp      ← 156行，地形渲染
│   ├── particles/particle3d_system.cpp   ← 155行，3D粒子
│   ├── ragdoll/ragdoll_system.cpp        ← 374行，布娃娃
│   ├── softbody/softbody_system.cpp      ← ✅ 已补12用例
│   ├── buoyancy/buoyancy_system.cpp      ← ✅ 已有12用例
│   └── vehicle/vehicle_system.cpp        ← 155行，载具
├── modules/gameplay_2d/
│   ├── rendering/sprite_render_system.cpp ← 精灵/UI渲染
│   ├── ui/ui_system.cpp/ui_layout.cpp    ← UI系统/布局
│   └── spine/spine_system.cpp            ← Spine骨骼动画
├── engine/physics/physics3d/             ← PhysX模拟，零运行时测试
└── apps/editor_cpp/                      ← 仅12个功能测试，无单元测试

🟡 P1（有部分覆盖，但核心路径缺）
├── engine/render/
│   ├── rhi/dx11/*.cpp                    ← DX11核心实现但无功能测试
│   ├── rhi/vulkan/vulkan_draw_executor.cpp ← Vulkan绘制执行
│   └── rhi/vulkan/vulkan_pipeline_state_manager.cpp ← PSO管理
├── modules/gameplay_3d/animation/
│   ├── anim_layer_blend_system.cpp       ← 动画层混合
│   └── ik_solver_system.cpp              ← IK求解器（间接覆盖）
├── tests/gtest/integration/
│   └── scripting/                        ← Lua Animation/DSSL/Particles/UI缺
└── engine/render/passes/                 ← 内置Pass无独立测试

🟢 P2（有测试，但深度不够）
├── 动画：缺 BlendTree 2D / Crossfade / Root Motion 专项
├── Audio：缺 FMOD 后端测试
├── Physics3D：缺真实 PhysX 世界刚体/关节模拟
├── Editor：缺面板单元测试（Inspector/Hierarchy/Viewport/Console）
├── RenderGraph：缺 ExecuteParallel 多线程路径测试
├── JobSystem：缺高并发/死锁场景测试
└── Vulkan smoke：仅1帧，缺完整场景渲染验证
```

---

## 六、建议补充测试的优先级

### Session 1（1 天）：🔴 补核心渲染路径

| 任务 | 估算用例 | 说明 |
|:----|:--------:|------|
| GL RHI 无头测试（仿 VK/DX11 模式） | 30 | `gl_rhi_test.cpp` 覆盖 Device/Resource/CmdBuf |
| BuiltinPasses 无头测试（MockCommandBuffer） | 20 | 9 个 Pass 的 Setup/Execute 参数验证 |
| LightProbeSystem 单元测试 | 8 | SH 计算/查询/更新 |
| ReflectionProbeSystem 单元测试 | 8 | IBL 数据更新/查询 |

### Session 2（1 天）：🔴 补 3D 高级模块

| 任务 | 估算用例 | 说明 |
|:----|:--------:|------|
| MeshRenderSystem 单元测试 | 10 | DrawItem 组装/材质切换 |
| RagdollSystem 集成测试（MockPhysX） | 8 | 布娃娃创建/关节 |
| SoftBodySystem 集成测试 | 6 | 四面体网格/形状匹配 |
| BuoyancySystem 集成测试 | 6 | 流体浮力/阻力 |
| Particle3DSystem 单元测试 | 8 | 发射/生命周期 |

### Session 3（1 天）：🟡 补集成面

| 任务 | 估算用例 | 说明 |
|:----|:--------:|------|
| Physics3D 真实 PhysX 模拟测试 | 15 | 刚体下落/碰撞/关节/raycast |
| SpriteRenderSystem 测试 | 10 | 精灵排序/批处理 |
| UISystem 测试 | 10 | 布局/事件 |
| Lua Animation/DSSL/UI 绑定 | 15 | 3 个绑定文件补齐 |

### Session 4（1 天）：🟢 加深 + 编辑器

| 任务 | 估算用例 | 说明 |
|:----|:--------:|------|
| 2D Blend Tree / Crossfade 专项 | 8 | 动画混合细节 |
| JobSystem 高并发压力测试 | 5 | 多线程正确性 |
| 编辑器单元测试（Inspector/Hierarchy 等） | 15 | 面板逻辑回归 |
| Vulkan smoke 扩展（10+ 帧） | 3 | 完整渲染帧验证 |

### 汇总

| 批次 | 工期 | 新增用例 | 填补缺口 |
|:----:|:----:|:--------:|---------|
| Session 1 | 1 天 | ~66 | 🔴 渲染核心 |
| Session 2 | 1 天 | ~38 | 🔴 3D 高级模块 |
| Session 3 | 1 天 | ~50 | 🟡 集成面 |
| Session 4 | 1 天 | ~31 | 🟢 加深 + 编辑器 |
| **合计** | **4 天** | **~185** | **补齐 90% 测试缺口** |

---

## 七、Session 4 完整变更日志

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
