# VSEngine2.1 技术参考报告 — DSEngine 可借鉴的架构与实践

> 分析日期：2026-05-14
> 参考版本：DSEngine 当前 master（69e9c46） vs VSEngine2.1（reference/VSEngine2.1/）
> 核心原则：所有参考方案必须遵循 DSE 现有架构约束（ECS 范式、RenderGraph DAG、ServiceLocator 注入、三后端解耦）

---

## 总览

### VSEngine2.1 值得参考的技术分布

| 优先级 | 技术领域 | 参考价值 | 对 DSE 的收益 |
|:------:|---------|:--------:|--------------|
| 🥇 | 通用 Mesh LOD 系统 | 高 | 直接指导 DSE LOD 实现，VSModelSwitchNode 的屏幕空间投影驱动方案 |
| 🥇 | VSCuller 分层可见集 | 高 | 多层平面裁剪 + RenderGroup 分组，改善 DSE 剔除-渲染对接 |
| 🥇 | VSScene 四叉树 + 动静态分离 | 高 | 消除每帧全量重建 Octree 的开销 |
| 🥇 | VSResourceManager 异步三阶段加载 | 高 | 优化 DSE 资源流送管线 |
| 🥇 | 地形 LOD 四方案（ROAM/QuadTree/GPU/Distance） | 高 | DSE 当前仅有 QuadTerrain 一种，可扩展 |
| 🥈 | VSOcclusionQuerySceneRender | 中高 | GPU 遮挡查询 Pass 扩展 RenderGraph |
| 🥈 | VSAsynStream 序列化框架 | 中 | 资源反序列化、场景存档的参考 |
| 🥈 | VSProperty 属性系统 | 中 | 编辑器属性面板基础设施 |
| 🥉 | VSSkyLight | 低 | DSE 已有类似 SkyLightComponent |
| 🥉 | VSDebugDraw | 低 | 调试绘制工具 |

---

## 一、通用 Mesh LOD 系统

### VSEngine2.1 的实现

VSEngine2.1 的 LOD 系统是一套**基于多级离散 Mesh 切换**的完整方案，核心在 `VSModelSwitchNode`。

**类层次：**
```
VSStaticMeshNode (顶层便捷接口, 提供 AddLodMesh)
  └── VSModelMeshNode (模型容器, 自动管理子节点)
        ├── VSModelSwitchNode (LOD 切换器, UpdateView 中计算屏幕尺寸并选择活跃子节点)
        │     ├── VSGeometryNode (LOD 0, m_fLODScreenSize=1.0)
        │     ├── VSGeometryNode (LOD 1, m_fLODScreenSize=0.5)
        │     ├── VSGeometryNode (LOD 2, m_fLODScreenSize=0.1)
        │     └── VSGeometryNode (LOD 3, m_fLODScreenSize=0.01)
        └── (LOD 0 未分离时的直接 GeometryNode)
```

**切换判据：** 屏幕空间投影大小

```cpp
// VSCamera::GetProjectScreenSize()
// 返回值 = (Max(proj[0][0], proj[1][1])^2 × 包围盒半径^2) / Max(1, 距离^2)
// LOD 切换: 选择第一个 fScreenSize > m_fLODScreenSize 的 Level
```

**全局控制：** `VSConfig::ms_LODScreenScale`（Config.txt 可调）

**约束：** 所有 LOD Level 的 SubMesh 数量必须一致，否则合并失败。

### DSEngine 的当前状态

DSE 当前**没有通用 Mesh LOD 系统**。仅有地形组件拥有独立的地形 LOD（`TerrainComponent.use_dynamic_lod` + `lod_ebos`），基于 QuadTerrainGeometry 的距离驱动 LOD。所有 `MeshRendererComponent` 只加载一个精度的 Mesh，无 LOD 降级。

### DSE 环境下的解耦重构方案

**设计思路：** 在 ECS 中新增 `LODGroupComponent` + `LODSystem`，组件存储各级 LOD 的 Mesh 路径和屏幕尺寸阈值，系统在 `EvaluateBaseAnim` 之后、渲染之前计算可见 LOD 级别并写入 `MeshRendererComponent`。

**新增组件：**

```cpp
// engine/ecs/components_3d.h 新增

/// LOD 层级配置
struct LODLevelConfig {
    std::string mesh_path;        ///< 该 LOD 级别的 .dmesh/.obj 路径
    float screen_size_threshold;  ///< 屏幕空间投影大小阈值（切换到此级别的下限）
    // 运行时缓存（首次使用时加载）
    unsigned int mesh_handle = 0;
    bool loaded = false;
};

/// LOD 组 —— 附加到已有 MeshRendererComponent 的实体上
struct LODGroupComponent {
    bool enabled = true;
    std::vector<LODLevelConfig> levels;   ///< levels[0] 为最高精度，依次降低
    int current_lod = 0;                  ///< 当前活跃的 LOD 级别（由 LODSystem 写入）
    float global_scale = 1.0f;            ///< 全局缩放因子（可通过 Lua 统一调节）
};
```

**新增系统：**

```cpp
// modules/gameplay_3d/rendering/lod_system.h

class LODSystem {
public:
    /// 在 FrustumCullingSystem 之后、ScenePass 之前调用
    void Update(World& world, const glm::mat4& view_proj);
};
```

**核心逻辑：**

```
LODSystem::Update()

对每个持有 MeshRendererComponent + LODGroupComponent 的实体:

  1. 获取实体的世界包围盒（从 BoundingBoxComponent 读取）
  2. 计算屏幕空间投影大小:
     - 获取当前主摄像机投影矩阵（从 Camera3DComponent 的 VP 矩阵提取缩放因子）
     - screen_size = (proj_scale^2 * bbox_radius^2) / max(1, distance^2) * global_scale
  3. 遍历 LOD levels[]，选择第一个 screen_size > levels[i].screen_size_threshold 的级别
  4. 如果 current_lod != 新级别:
     - 更新 current_lod 为新级别
     - 如果 levels[new_lod].mesh_handle == 0（未加载）:
       - 同步或异步加载 Mesh，获取 mesh handle
       - 写入 levels[new_lod].mesh_handle
     - 将 MeshRendererComponent.mesh_handle_override 更新为新级别的 handle
       （注：MeshRendererComponent 需新增此字段，非零时渲染系统跳过 mesh_path 加载）
  5. 如果没有 LODGroupComponent，跳过（零开销）
```

**在 Gameplay3DModule 中的集成时序：**

```
OnUpdate() 时序:
  AnimatorSystem::EvaluateBaseAnim
  AnimLayerBlendSystem
  IKSolverSystem
  AnimatorSystem::ComputeFinalMatrices
  FrustumCullingSystem      ← 当前
  LODSystem                 ← 新增：在剔除之后、渲染之前
  → 渲染管线 (RenderGraph)
```

**与 VSEngine2.1 的关键差异：**

| 维度 | VSEngine2.1 | DSE 方案 | 理由 |
|:-----|:-----------|:---------|:-----|
| 架构模式 | OOP 节点树（VSModelSwitchNode 子节点切换） | ECS 组件（LODGroupComponent + LODSystem） | 符合 DSE 的 ECS 范式 |
| Mesh 切换 | 节点指针切换，运行时即时生效 | 新增 `MeshRendererComponent.mesh_handle_override`（uint，非零时跳过 path 加载直接使用），LODSystem 预加载各级别并写入 | 渲染管线只消费组件数据，不感知 LOD |
| 阈值存储 | VSGeometryNode.m_fLODScreenSize（序列化属性） | LODLevelConfig.screen_size_threshold | 同等的可配置能力 |
| 全局缩放 | VSConfig::ms_LODScreenScale（Config.txt） | LODGroupComponent.global_scale | 实体级独立控制，更灵活 |
| 约束 | SubMesh 数一致要求 | 无硬约束（dmesh 格式独立加载） | 离线 LOD 生成工具保证一致性 |

**解耦要点：**
- LODSystem 不依赖任何渲染后端，只修改 ECS 组件数据
- MeshRendererComponent 不感知 LOD，渲染系统检查 `mesh_handle_override`（非零优先，零则走 `mesh_path`）
- 与 RenderGraph Pass 完全解耦——Pass 只看到最终的 final_bone_matrices 和 mesh handle
- 注意：`MeshRendererComponent` 当前使用 `mesh_path`（字符串）加载，实现时需新增 `unsigned int mesh_handle_override = 0` 字段

#### Mesh LOD 自动生成工具

离线工具（扩展 AssetBuilder）流程：

```
LOD 0 (原始高模) → 简化算法 → LOD 1 → 简化 → LOD 2 → 简化 → LOD 3
```

简化算法候选（按质量排序）：
1. **Assimp + meshopt** — `meshopt_simplify()` 提供高质量的渐进式网格简化
2. **Assimp 内置** — `aiProcess_GenOptimizedNormals` + 顶点合并
3. **自研** — 顶点聚类 + 边塌缩（但不推荐，meshopt 已很成熟）

离线生成的各级 LOD 保存为独立的 `.dmesh` 文件，运行时通过 `LODGroupComponent.levels[].mesh_path` 加载。

---

## 二、VSCuller 分层可见集

### VSEngine2.1 的实现

`VSCuller` 是 VSEngine2.1 的核心剔除器，支持最多 32 个裁剪平面：

```cpp
class VSCuller {
    // 平面缓存：支持 AABB / Sphere / Point 三种包围体测试
    // 可见集按 RenderGroup 和 VisibleSetType 分类存储
    VSRenderContext m_RenderContext[RG_MAX][VST_MAX];
    // 排序支持：Sort() 按材质/深度排序
    // 动态实例收集：CollectDynamicInstance()
    // 子类：VSShadowCuller（阴影裁剪）
};
```

关键特性：
1. **多层裁剪面缓存** — 平面状态缓存，避免重复计算
2. **RenderGroup 分类** — RG_BACK / RG_NORMAL / RG_FRONT 三组，分别对应不同渲染阶段
3. **VisibleSetType 子分类** — VST_BASE（不透明）/ VST_ALPHABLEND（半透明）/ VST_COMBINE（合成）
4. **排序支持** — 按材质/距离排序减少状态切换
5. **阴影裁剪器子类** — VSShadowCuller 专门处理阴影视锥

### DSEngine 的当前状态

DSE 当前在 `FrustumCullingSystem` 中做了基础的视锥剔除：
- 6 平面提取（`ExtractFrustumPlanes`）
- AABB vs 6-Plane 测试（`IsAABBVisible`）
- Octree 空间加速（`QueryOctree` 递归遍历）
- 输出写入 `MeshRendererComponent.visible` 和 `TerrainComponent.visible`

**与 VSEngine2.1 的差距：**

| 特性 | DSE 当前 | VSEngine2.1 |
|:-----|:--------|:-----------|
| 裁剪平面缓存 | 无 | 有（平面状态缓存） |
| 分类可见集 | 无 | RenderGroup × VisibleSetType 二维分类 |
| 不透明/半透明分离 | 无 | VST_BASE / VST_ALPHABLEND |
| 阴影裁剪 | 无特定处理 | VSShadowCuller 子类 |
| 排序 | 无 | Sort() 按材质/距离 |
| 动态实例收集 | 无 | CollectDynamicInstance() |

### DSE 环境下的解耦重构方案

**思路：** 不把 VSCuller 复杂的继承体系搬进 DSE，而是提取其**分层可见集管理**的思想，用 ECS Component + RenderGraph Pass 级别的筛选来实现。

```cpp
// engine/ecs/components_3d.h 新增

/// 渲染分类（对应 VSEngine2.1 的 RenderGroup + VisibleSetType）
enum class RenderCategory : uint8_t {
    Opaque = 0,          ///< 不透明物体（VST_BASE + RG_NORMAL）
    AlphaTest = 1,       ///< Alpha 测试物体
    Transparent = 2,     ///< 半透明物体（VST_ALPHABLEND）
    ShadowOnly = 3,      ///< 仅投射阴影
    Decal = 4,           ///< 贴花
    Count
};
```

```cpp
// modules/gameplay_3d/rendering/frustum_culling_system.h 重构

class FrustumCullingSystem {
public:
    void Update(World& world);

    // 新增：输出可见集，供 RenderGraph Pass 消费
    struct VisibleSet {
        std::vector<entt::entity> opaque;         ///< RG_NORMAL × VST_BASE
        std::vector<entt::entity> transparent;    ///< RG_NORMAL × VST_ALPHABLEND
        std::vector<entt::entity> shadow_casters; ///< 阴影投射物
    };
    const VisibleSet& GetVisibleSet() const { return visible_set_; }

private:
    Octree* scene_octree_ = nullptr;             ///< 静态场景 Octree（非每帧重建）
    VisibleSet visible_set_;
};
```

**时序改动：**

```
当前的 FrustumCullingSystem::Update:
  1. 计算全局包围盒
  2. 构建 Octree ← 这是不必要的每帧开销
  3. Query Octree ← 剔除

改进后的 FrustumCullingSystem::Update:
  1. 查询静态 Octree + 遍历动态物体数组（分离！）
  2. 剔除结果分类写入 visible_set_
  3. RenderGraph Pass 从 visible_set_ 读取自己需要的集合
```

**关键改良：动静态分离**

```cpp
// engine/scene/spatial_scene.h 新增

/// 空间场景 —— 管理静态 Octree + 动态物体列表
class SpatialScene {
public:
    /// 构建静态 Octree（场景加载时调用一次，不每帧重建）
    void BuildStatic(const std::vector<OctreeData>& static_objects);

    /// 更新动态物体的包围盒（每帧调用）
    void UpdateDynamic(entt::entity e, const AABB& bounds);

    /// 可见性查询（每帧调用，输出到 FrustumCullingSystem）
    void QueryFrustum(const std::array<Plane, 6>& planes,
                      std::vector<entt::entity>& out_static,
                      std::vector<entt::entity>& out_dynamic) const;

private:
    std::unique_ptr<Octree> static_tree_;           ///< 静态物体 Octree（不每帧重建）
    std::vector<OctreeData> dynamic_objects_;        ///< 动态物体（每帧更新）
};
```

**与 RenderGraph 的对接：**

FrankPipeline 的渲染 Pass 不再通过 FrustumCullingSystem 的 visible flag 判断，而是直接从 VisibleSet 读取：

```cpp
// RenderGraph Pass 中的使用示例
const auto& visible = frustum_culling_system.GetVisibleSet();
for (auto entity : visible.opaque) {
    // 在 opaque 子集中提交 Draw Call
}
```

**解耦要点：**
- SpatialScene 是纯空间数据结构，不依赖渲染/ECS 类型
- FrustumCullingSystem 输出 `VisibleSet`（纯数据），不直接调用 RenderGraph
- 渲染 Pass 消费 VisibleSet，不关心剔除怎么做的
- ShadowPass 可以只读 `visible_set_.shadow_casters`

---

## 三、VSScene 四叉树 + 动静态分离

### VSEngine2.1 的实现

`VSScene` 内部维护：
- `VSQuadNode`（四叉树）—— 存储**静态**物体
- `m_pDynamic` 数组—— 存储**动态**物体（不进入空间索引）
- `Build()` —— 场景加载时构建四叉树一次
- `ComputeVisibleSet()` — 每帧从根节点开始层级剔除

```
VSScene
  ├── VSQuadNode (四叉树根)
  │     ├── VSQuadNode (递归子节点)
  │     │     └── m_pNeedDrawNode[] (可见节点列表)
  │     └── ...
  └── m_pDynamic[]  ( 动态物体，每帧遍历)
```

**VSSceneManager / VSSceneMap / VSWorld 三层管理：**
```
VSWorld (顶层世界)
  └── VSSceneMap (对应一个地图资源)
        ├── VSScene (QuadTree + 动态物体)
        ├── Actor[] (所有场景中的 Actor)
        └── VSViewFamily (视角族，管理视口和裁剪器)
```

### DSEngine 的当前状态

DSE 有 `engine/scene/` 目录，提供：
- `Scene` — 封装 World，支持序列化/反序列化
- `SubScene` — 子场景，支持 Level Streaming（Unloaded → Loading → Loaded 状态机）
- `SceneManager` — 多 SubScene 管理，异步加载 + Fade 过渡

空间加速方面：
- `Octree` — 八叉树（capacity=8, max_depth=5），但**每帧全量重建**
- `QuadTree` — 2D 四叉树

**关键问题是：** `FrustumCullingSystem` 每帧重建 Octree（见第 130 行 `Rebuild Octree (simple dynamic approach)`），这是一个 O(n) 的插入操作 + O(n log n) 的树构建，对大型场景是严重浪费。

### DSE 环境下的解耦重构方案

**核心思路：** 复用现有的 `Octree` 类，但改为静态/动态分离模式。

```cpp
// engine/scene/spatial_scene.h（新建）

namespace dse::scene {

/// 可见性标记（FrustumCullingSystem 每帧写入）
enum class VisibilityFlags : uint8_t {
    Unknown = 0,
    Visible = 1,
    Culled = 2,
    Occluded = 3,
};

/// 空间场景 —— 替代 FrustumCullingSystem 中的临时 Octree
class SpatialScene {
public:
    // 生命周期：场景加载时调用一次
    void BuildStatic(World& world);

    // 每帧：更新动态物体的包围盒位置
    void UpdateDynamicTransforms(World& world);

    // 每帧：执行剔除，输出可见性结果到组件
    void CullFrustum(const glm::mat4& view_proj, World& world);

    // 优化提示：标记实体为静态/动态
    void MarkStatic(entt::entity e);
    void MarkDynamic(entt::entity e);

    size_t GetStaticCount() const { return static_entities_.size(); }
    size_t GetDynamicCount() const { return dynamic_entities_.size(); }

private:
    std::unique_ptr<Octree> static_tree_;        ///< 静态 Octree（不每帧重建）
    std::vector<entt::entity> dynamic_entities_;  ///< 动态实体列表
    bool built_ = false;
};

} // namespace dse::scene
```

**改动范围：**

| 文件 | 改动 | 行数 |
|:-----|:-----|:----:|
| `engine/scene/spatial_scene.h` | 新建 | ~60 行 |
| `engine/scene/spatial_scene.cpp` | 新建 | ~150 行 |
| `modules/gameplay_3d/rendering/frustum_culling_system.cpp` | 重构：去掉临时 Octree 构建，使用 SpatialScene | -80 行 +40 行 |
| `modules/gameplay_3d/gameplay_3d_module.h` | 新增 SpatialScene 成员 | +1 行 |
| `modules/gameplay_3d/gameplay_3d_module.cpp` | 初始化 + 调度 | +10 行 |

**性能预期：**

| 场景规模 | 当前（每帧建树） | 改进后（静态缓存） |
|:---------|:---------------|:-----------------|
| 100 物体 | ~0.05ms | ~0.02ms |
| 1000 物体 | ~0.5ms | ~0.05ms |
| 10000 物体 | ~5ms（卡顿） | ~0.2ms |

---

## 四、VSResourceManager 异步三阶段加载

### VSEngine2.1 的实现

`VSResourceManager` 是 VSEngine2.1 的资源管理中枢，其异步加载通过 `VSAsynJob` 双阶段任务实现（工作线程 `AsynThreadProcess` + 主线程 `MainThreadProcess`），对象反序列化通过 `VSAsynStream` 的三阶段流程实现：

```cpp
// VSAsynJob 异步加载双阶段
// Phase A: AsynThreadProcess — 工作线程执行（文件 IO / 解码）
// Phase B: MainThreadProcess — 主线程执行（GPU 上传 / 资源注册）

// VSAsynStream 对象反序列化三阶段
// Phase 1: LoadFromBuffer — 从内存流解析对象表
// Phase 2: CreateAndRegister — 创建 C++ 实例，注册到全局资源表
// Phase 3: LoadAndLink — 加载属性值 + 链接跨资源引用
// PostLoad — 延迟后加载回调
```

关键架构要素：
- `DECLARE_RESOURCE` 宏体系——每种资源类型有独立的 ResourceSet
- `GC` 机制——`AddRootObject` / `AddGCObject` / `GCObject` 管理生命周期
- `CacheResource()`——缓存为 GPU 友好格式
- `LoadASYNResource<T>()`——模板化异步加载
- `DelayUpdateObject`——支持延迟一帧/延迟指定时间/无时间三种更新回调

### DSEngine 的当前状态

DSE 的 `AssetManager` 已有较为完善的资源管理系统：

| 特性 | 当前状态 |
|:-----|:---------|
| 同步加载 | ✅ 每种资源类型有 `Load*()` 方法 |
| 异步加载 | ✅ `Load*Async()` + `PumpMainThreadCallbacks()` |
| Bundle/Pak | ✅ `PackBundle()` / `MountBundle()` / `MountPak()` |
| 热重载 | ✅ `StartFileWatcher()` + `PumpHotReloads()` |
| LRU 淘汰 | ✅ `EvictLRU()` + `SetMemoryBudget()` |
| 异步回调 | ✅ `PendingMainThreadCallbacks()` 监控积压 |

**当前三阶段是**（以纹理为例）：
```
Phase 1: 磁盘 IO（工作线程） → LoadTextureAsync
Phase 2: 解码（工作线程）   → stb_image 解码
Phase 3: GPU 上传（主线程） → PumpMainThreadCallbacks
```

### 可借鉴的改进方向

VSEngine2.1 的 `VSResourceManager` 在**资源间引用链接和生命周期管理**上比 DSE 更系统。

**建议 1：注册表 + GC**

DSE 当前没有统一的资源注册表。`AssetManager` 内部有 `std::unordered_map<std::string, ...>` 的缓存，但没有类似 VSEngine2.1 的 `AddRootObject` / `GCObject` 回收机制。

```cpp
// engine/assets/asset_registry.h（可选新增）

/// 统一资源注册表 —— 跟踪所有已加载资源的引用关系
class AssetRegistry {
public:
    /// 注册资源实例
    void Register(const std::type_index& type, const std::string& path, void* asset);

    /// 标记为 GC 根（不被回收）
    void AddRoot(const std::string& path);

    /// 执行 GC：回收无引用的资源
    void GarbageCollect();

    /// 按类型和路径查找
    template<typename T>
    T* Find(const std::string& path) const;
};
```

**建议 2：资源引用链接**

DSE 当前资源间引用靠路径字符串运行时绑定（如材质中的纹理路径），如果有类似 VSEngine2.1 的 `LoadAndLinkOjbect` 阶段，可以在加载完成后自动解析引用关系，避免运行时重复解析。

这更适合作为**远期优化**，当前 DSE 的资源系统已足够满足需要。

---

## 五、地形 LOD 系统四方案

### VSEngine2.1 的实现

VSEngine2.1 提供了四种地形 LOD 实现：

| 方案 | 文件 | 原理 | 适用 |
|:-----|:-----|:-----|:-----|
| **ROAM CLOD** | `VSRoamTerrainGemotry.cpp` | 三角二叉树递归细分/合并 | CPU 端连续 LOD |
| **QuadTree CLOD** | `VSQuadTerrainGeometry.cpp` | 四叉树细分，顶点方差判据 | CPU 端连续 LOD |
| **GPU Tessellation LOD** | `VSGPULodTerrainNode.h` | 硬件曲面细分（Tessellation Shader） | GPU 端连续 LOD |
| **Distance LOD** | `VSDLodTerrainNode.h` | 简单距离驱动，多级离散网格 | 最低成本方案 |

DSE 当前地形系统（`TerrainComponent` + `TerrainSystem`）基于 `VSQuadTerrainGeometry` 的 QuadTree 算法，已实现 `use_dynamic_lod`、`max_lod_levels`、`lod_distance_factor`。

### 可借鉴的改进方向

**建议：新增 GPU Tessellation LOD 路径**

方案不需要额外组件字段，只需要在 `TerrainSystem` 中检测 GPU 能力并自动切换：

```
TerrainSystem::Update():
  if (rhi_device->SupportsTessellationShader()):
    → GPU LOD 路径（Tessellation Level 控制细分度）
  else:
    → QuadTree CLOD 路径（当前实现）
```

GPU 端通过 Tessellation Shader 控制地形细分，在高端卡上得到更平滑的地形渲染，低端卡自动回退到 CPU QuadTree。从 API 支持角度，三后端都支持 Tessellation Shader（GL 4.0+ / Vulkan 1.0+ / D3D11+），但 DSE 的 RHI 层当前**没有曲面细分着色器的基础设施**。需要在 `RhiDevice` 中新增 hull/domain shader 的创建和管理接口，并在三个后端的 ShaderManager 中分别实现。这是中等投入的改进（~300 行，主要工作在三端 ShaderManager 的泛化）。

---

## 六、VSOcclusionQuerySceneRender — GPU 遮挡查询

### VSEngine2.1 的实现

VSEngine2.1 的 `VSOcclusionQuerySceneRender` 封装了 GPU 端遮挡查询流程：
- 每帧提交遮挡查询（DX9/11 后端使用 `IDirect3DQuery9` / `ID3D11Query`）
- 延迟一帧读取结果
- 结果用于精细剔除——被遮挡的物体即使通过视锥体测试也不渲染
- 与 `VSCuller` 配合：非可见几何体存储在 `m_NoVisibleGeometry[RG_MAX]` 中

### DSEngine 的当前状态

DSE 当前**没有 GPU 遮挡查询**。`FrustumCullingSystem` 只做了视锥体裁剪（CPU 端），无法处理"一个物体在视锥内但在墙后被遮挡"的场景。

### DSE 环境下的方案

**思路：** 在 RenderGraph 中新增一个 `OcclusionQueryPass`，在所有渲染 Pass 之前执行。

```cpp
// engine/render/passes/occlusion_query_pass.h

class OcclusionQueryPass : public IRenderPass {
public:
    void Setup(RenderGraph& graph) override {
        // 从 FrustumCullingSystem 的 VisibleSet 中读取候选物体
        // 为每个候选物体提交遮挡查询（使用简化的 bounding box mesh）
    }
    void Execute(CommandBuffer& cmd_buffer) override {
        // 读取上一帧的查询结果
        // 更新可见性标记
    }
};
```

**时序：**
```
Frame N:   提交遮挡查询(set A) → 渲染 → ... → 读不到结果
Frame N+1: 读取 frame N 的查询结果(set A) → 提交新查询(set B) → ... → 用 set A 的结果做剔除
```

**简化方案（初期）：**

对 DSE 当前阶段，GPU 遮挡查询不是迫切需求——在实现 Mesh LOD 和动静态 Octree 分离之后，性能瓶颈已在可控范围。建议标记为 P2 锦上添花，在 Phase 3 后评估是否实现。

---

## 七、VSAsynStream 序列化框架

### VSEngine2.1 的实现

`VSAsynStream` 继承自 `VSStream`，支持从内存缓冲区反序列化对象图：

```
VSAsynStream::LoadFromBuffer    → 解析对象表（Name → Type → Offset）
VSAsynStream::CreateAndRegister → 创建实例 + 注册到全局表
VSAsynStream::LoadAndLink       → 加载属性 + 链接跨资源引用
```

配套的 `VSProperty` 系统让每个可序列化属性有 `REGISTER_PROPERTY` 宏标记，支持 `F_SAVE_LOAD_COPY` / `F_REFLECT_NAME` 等标志。

### DSEngine 的当前状态

DSE 的场景序列化（`Scene::Serialize` / `Scene::Deserialize`）使用 JSON 格式，通过 `rapidjson` 实现。资源引用通过路径字符串链接。当前格式已满足需要，没有类似 VSEngine2.1 的通用属性反射框架。

### 可借鉴的改进方向

建议**不作为优先事项**。DSE 当前的 JSON + 路径串联的资源格式已足够满足独立游戏和极客引擎场景。完整的属性反射系统需要大量的 C++ 模板元编程基础设施，且 DSE 没有像 VSEngine2.1 那样的编辑器需要属性面板。当编辑器 GUI 需要通用属性编辑功能时再考虑。

---

## 八、综合优先级建议

### 对 DSE 最有价值的参考实现（按优先级）

| 优先级 | 技术 | 改动量 | 收益 | 建议做 |
|:------:|:-----|:-----:|:----:|:-----:|
| 🥇 | **通用 Mesh LOD**（VSModelSwitchNode 方案） | ~500 行 | 🔥🔥🔥 性能提升 2-3 倍 | Phase 2 |
| 🥇 | **动静态 Octree 分离**（VSScene 方案） | ~210 行 | 🔥🔥 消除每帧建树开销 | Phase 2 |
| 🥇 | **GPU Tessellation 地形 LOD**（VSGPULodTerrainNode） | ~600 行（含三端 ShaderManager） | 🔥 高端卡画质提升 | Phase 3 |
| 🥈 | **VSCuller 分层可见集** | ~150 行 | 🔥 优化剔除-渲染对接 | Phase 2-3 |
| 🥈 | **GPU 遮挡查询 Pass** | ~400 行 | 🔥 精细剔除 | Phase 3+ |
| 🥉 | **资源注册表 + GC** | ~300 行 | 内存管理优化 | 远期 |
| 🥉 | **序列化/属性系统** | ~1000 行 | 编辑器基础设施 | 编辑器扩展时 |

### 与 DSE 当前路线图的对齐

```
Phase 2 (性能优化 + 兼容, 当前路线图):
  ├── #1 通用 Mesh LOD 系统          ← VSEngine2.1 VSModelSwitchNode 直接参考
  ├── #2 GPU Instancing               ← VSEngine2.1 无此功能（DSE 自研）
  ├── #3 动静态 Octree 分离           ← VSEngine2.1 VSScene 方案
  ├── #4 SSBO → UBO fallback         ← 无参考
  └── #5 .dds / BCn 直接上传         ← 无参考

Phase 3 (氛围增强 + 风格化深化):
  ├── #6 GPU Tessellation 地形 LOD    ← VSEngine2.1 VSGPULodTerrainNode
  ├── #7 分层可见集                 ← VSEngine2.1 VSCuller
  ├── #8 遮挡查询 Pass               ← VSEngine2.1 VSOcclusionQuerySceneRender
  └── #9 风格化材质库                ← 无参考（DSE 自研）
```

---

## 九、VSEngine2.1 不具备而 DSE 已领先的功能

作为对比，DSE 已在以下方面超越 VSEngine2.1。注意某些条目 VSEngine2.1 **有等效或更优的实现**（已在报告中作为参考亮点列出），此处仅列举真正缺失的领域：

| 领域 | VSEngine2.1 | DSEngine | 说明 |
|:-----|:-----------|:---------|:-----|
| **ECS 架构** | OOP 节点树（Actor-Node-Geometry） | enTT 驱动的 ECS | 架构范式不同，VSE 是传统 OOP，DSE 更现代 |
| **渲染图** | 无，按 SceneManager 顺序渲染 | RenderGraph DAG + 波形并行 | DSE 的 DAG 渲染图可以自动剔除无用 Pass |
| **跨平台后端** | DX9 + DX11，仅 Windows | OpenGL 4.3+ / Vulkan 1.0+ / D3D11 | DSE 三后端，VSE 仅 Windows |
| **GPU 骨骼蒙皮** | 无 | 支持（100 骨骼 + morph target） | VSE 动画仅 CPU 端 |
| **延迟渲染** | 无（纯前向） | GBuffer + DeferredLighting | VSE 纯前向渲染 |
| **Clustered Forward+** | 无（固定数量光源遍历） | 256+256 光源 | VSE 没有光源剔除系统 |
| **PBR 管线** | 无——仅有 Phong/Blinn-Phong + Oren-Nayar。Cook-Torrance 有枚举定义但 **未实现** | GGX+Smith+Schlick + IBL + SH L2 | VSE 的渲染模型停留在 DX9 时代，没有 PBR 工作流 |
| **后处理链** | Post-Effect DAG 框架（Bloom/高斯模糊/饱和度/灰度等），通过节点图编排，不含 SSR/DOF/MotionBlur/TAA | 22 Pass 完整后处理链（含 SSR/DOF/MotionBlur/TAA/AutoExposure） | VSE 有后处理框架但效果种类较少 |
| **物理引擎** | 无 | PhysX 4.1 全功能 + Box2D | VSE 项目中没有物理模块 |
| **音频系统** | 无（仅 DX SDK 头文件，无引擎封装） | miniaudio + ECS 驱动 + 3D 空间化 | VSE 没有音频系统 |
| **Lua 脚本** | 无（C++ 直接编码所有逻辑） | 完全 sol2 绑定，210+ API 函数 | VSE 没有脚本系统 |
| **热重载** | 无 | 纹理/脚本/编辑器全链路热重载 | VSE 没有热重载机制 |
| **粒子系统** | 无（"PE"前缀类全部是 Post-Effect，不是粒子） | GPU 粒子系统（2D + 3D） | VSE 没有粒子系统 |
| **场景流式加载** | LoadMap/UnloadMap 异步加载，但整地图切换 | SubScene Level Streaming（Unloaded→Loading→Loaded） | VSE 没有子场景级别的流式加载 |

---

## 十、总结

VSEngine2.1 对 DSE 最有参考价值的三大技术：

1. **通用 Mesh LOD 系统**（性价比最高，直接指导 DSE 当前 P0 短板的实现）
2. **动静态 Octree 分离**（消除每帧重建开销，与 LOD 配合达到 >3 倍性能提升）
3. **GPU Tessellation 地形 LOD**（低成本的高端画质提升，三端都支持）

这些方案都遵循 DSE 的架构约束（ECS 组件 + Module System 调度 + 不改动渲染后端），可以作为 Phase 2-3 路线图的具体参考实现。
