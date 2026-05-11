# DSEngine 高级物理功能规划

> 布料模拟 · 流体模拟 · 物理破碎

## 现状

DSEngine 当前物理能力：
- **2D**: Box2D（刚体、碰撞体、关节、射线检测）
- **3D**: PhysX 4.x（刚体、Box/Sphere/Mesh Collider、CharacterController、射线检测）

以下三项功能尚未实现，本文档规划其实现方案。

---

## 1. 布料模拟（Cloth Simulation）

### 1.1 方案选型

采用 **XPBD（Extended Position Based Dynamics）** 粒子-约束模型。

该方案与主流引擎一致：
- UE5 Chaos Cloth 底层为 XPBD
- Unity Magica Cloth 2 底层为 PBD/XPBD

### 1.2 核心算法

```
每帧：
  1. 外力积分（重力、风力）→ 预测位置
  2. 约束投影（迭代 N 次）：
     - 距离约束：相邻顶点间保持原始边长
     - 弯曲约束：相邻三角形二面角保持原始角度
     - 碰撞约束：质点不穿透碰撞体（球/胶囊/mesh）
  3. 速度更新 = (新位置 - 旧位置) / dt
  4. 将新位置写入 mesh 顶点缓冲区
```

### 1.3 ECS 组件设计

```cpp
struct ClothComponent {
    // 配置
    uint32_t solver_iterations = 8;       // 约束迭代次数
    float damping = 0.01f;                // 速度阻尼
    float stiffness = 1.0f;              // 距离约束刚度 (0~1)
    float bend_stiffness = 0.5f;         // 弯曲约束刚度
    glm::vec3 gravity = {0, -9.81f, 0};
    glm::vec3 wind = {0, 0, 0};

    // 碰撞
    float collision_radius = 0.02f;       // 质点碰撞半径
    std::vector<entt::entity> colliders;  // 碰撞体列表（球/胶囊）

    // 固定点
    std::vector<uint32_t> pinned_vertices; // 固定不动的顶点索引

    // 运行时状态（不序列化）
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> prev_positions;
    std::vector<glm::vec3> velocities;
    std::vector<float> inv_masses;        // 固定点质量为 0
    struct DistanceConstraint { uint32_t i, j; float rest_length; };
    std::vector<DistanceConstraint> distance_constraints;
};
```

### 1.4 实现步骤

1. **ClothSystem** 类：Init 时从 mesh 顶点构建粒子和约束
2. 每帧 FixedUpdate 中执行 XPBD 求解
3. 求解后更新 mesh vertex buffer（需要 `RhiDevice` 接口支持动态 VBO 更新）
4. 碰撞检测：对场景中的 SphereCollider3D / CapsuleCollider 做质点-球/胶囊距离检测
5. 固定点支持：pinned_vertices 的 inv_mass = 0，位置跟随父骨骼或 Transform

### 1.5 开源参考

| 项目 | 语言 | 用途 |
|---|---|---|
| [PositionBasedDynamics](https://github.com/InteractiveComputerGraphics/PositionBasedDynamics) | C++ | PBD/XPBD 标准参考实现（Bender 教授） |
| [Ten Minute Physics - PBD Cloth](https://matthias-research.github.io/pages/tenMinutePhysics/) | JS | Müller 本人的极简教学实现 |
| [cloth-simulation](https://github.com/dissimulate/Cloth) | JS | 几百行核心实现，适合快速理解 |

### 1.6 预估工作量

核心求解器 ~500-800 行 C++，碰撞 + VBO 更新 ~300 行，合计约 **1000-1200 行**。

---

## 2. 流体模拟（Fluid Simulation）

### 2.1 方案选型

采用 **粒子特效 + Screen-Space Fluid Rendering** 的视觉效果方案。

该方案与主流引擎一致：
- UE5 使用 Niagara 粒子 + shader trick
- Unity 使用 VFX Graph + shader trick
- 游戏中几乎没有引擎做真实体积流体模拟

如需物理交互级流体，可后续追加 **GPU SPH/DFSPH** 求解器。

### 2.2 视觉效果方案（Level 1，推荐先做）

#### 渲染管线

```
1. 粒子系统发射大量点精灵（point sprite）
2. Depth Pass：渲染粒子球到深度缓冲
3. Depth Smoothing：高斯/双边滤波平滑深度（消除粒子感）
4. Normal Reconstruction：从平滑后的深度重建法线
5. Shading：基于法线 + 深度做 Fresnel 反射/折射/焦散着色
```

#### 扩展 ParticleSystem3D

在现有 `ParticleSystem3DComponent` 基础上增加流体力场：

```cpp
struct FluidEmitterComponent {
    // 发射配置
    float emission_rate = 500.0f;         // 粒子/秒
    float particle_radius = 0.05f;
    float lifetime = 3.0f;

    // 力场
    glm::vec3 gravity = {0, -9.81f, 0};
    float viscosity = 0.01f;              // 粘性（影响粒子间速度平滑）
    float surface_tension = 0.05f;        // 表面张力

    // 渲染
    glm::vec4 color = {0.2f, 0.5f, 0.9f, 0.8f};
    float refraction_strength = 0.3f;
    float fresnel_power = 2.0f;
};
```

### 2.3 物理模拟方案（Level 2，可选）

若需要流体与刚体双向交互，实现 **GPU DFSPH（Divergence-Free SPH）**：

```
每步：
  1. 邻居搜索（空间哈希 / compact hashing）
  2. 密度估计 + 不可压缩性求解（DFSPH 两次迭代）
  3. 粘性力、表面张力
  4. 位置积分
  5. 与刚体交互（双向力传递）
```

需要 Compute Shader pipeline（DSEngine Vulkan 后端支持）。

### 2.4 开源参考

| 项目 | 语言 | 用途 |
|---|---|---|
| [SPlisHSPlasH](https://github.com/InteractiveComputerGraphics/SPlisHSPlasH) | C++ | SPH 流体标准参考，含 DFSPH/IISPH/PBF 多种求解器 |
| [Fluid Engine Dev](https://github.com/doyubkim/fluid-engine-dev) | C++ | 《流体引擎开发》配套代码，教学级 |
| [Ten Minute Physics - Euler Fluid](https://matthias-research.github.io/pages/tenMinutePhysics/) | JS | Müller 的极简网格流体 |
| [Crest Ocean](https://github.com/wave-harmonic/crest) | C# | 海洋 screen-space 渲染参考 |

### 2.5 预估工作量

- **Level 1**（视觉效果）：粒子扩展 ~300 行 + screen-space rendering pass ~500 行 GLSL/SPIR-V
- **Level 2**（GPU SPH）：Compute Shader ~1500 行 + CPU 调度 ~500 行

---

## 3. 物理破碎（Destruction / Fracture）

### 3.1 方案选型

采用 **离线 Voronoi 预切分 + 运行时碎片激活** 方案，推荐集成 **NVIDIA Blast**。

该方案与主流引擎一致：
- UE5 Chaos Destruction = 预切分 + damage propagation + clustering
- Unity RayFire = 预切分 + 运行时激活

NVIDIA Blast 与 DSEngine 已有的 PhysX 后端共享 Foundation，集成成本最低。

### 3.2 架构设计

```
┌──────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  离线工具     │     │  运行时           │     │  物理引擎        │
│              │     │                  │     │                 │
│ Voronoi 切分 │────▶│ FractureComponent │────▶│ PhysX Dynamic   │
│ (Voro++)     │     │ 完整 → 碎片群     │     │ RigidBody per   │
│              │     │                  │     │ fragment        │
└──────────────┘     └──────────────────┘     └─────────────────┘
```

### 3.3 离线预切分工具

输入一个完整 mesh，使用 Voronoi 分割为 N 个碎片 mesh：

```
输入：model.dmesh + 切分参数（碎片数、随机种子、切分模式）
输出：
  - fragments/model_frag_00.dmesh ~ model_frag_N.dmesh
  - model_fracture.json（碎片间邻接关系、质心、体积）
```

可使用 Python + Voro++ 实现，也可集成 Blender Cell Fracture 插件导出。

### 3.4 ECS 组件设计

```cpp
struct FractureComponent {
    // 配置
    std::string fracture_asset;           // 指向 fracture.json
    float break_force = 1000.0f;          // 断裂所需冲击力阈值
    float health = 100.0f;               // 累积伤害模式下的生命值
    bool use_damage_accumulation = false;  // true=累积伤害, false=单次冲击

    // 碎片配置
    float fragment_lifetime = 5.0f;       // 碎片存活时间（秒）
    float fragment_fade_time = 1.0f;      // 消失前淡出时间
    float explosion_force = 50.0f;        // 碎裂时向外的爆炸力

    // 运行时状态（不序列化）
    bool is_fractured = false;
    std::vector<entt::entity> fragment_entities;
};
```

### 3.5 运行时流程

```
1. 完整物体正常渲染（单 mesh + 单 Static/Dynamic RigidBody3D）
2. 检测触发条件：
   a. 碰撞回调中累积冲击力 > break_force
   b. 或 health 降为 0
3. 触发断裂：
   a. 隐藏/销毁原实体
   b. 从 fracture_asset 加载碎片 mesh 列表
   c. 为每个碎片创建实体：MeshComponent + Dynamic RigidBody3D
   d. 碎片初始位置 = 原物体位置 + 碎片局部偏移
   e. 施加径向爆炸力
4. 碎片生命周期：
   a. 存活 fragment_lifetime 秒后开始 fade
   b. fade 完毕后销毁实体释放资源
```

### 3.6 NVIDIA Blast 集成（可选进阶）

Blast 提供了比简单 Voronoi 更完善的功能：
- **多层级碎裂**（碎片可以再碎）
- **Damage propagation**（力沿碎片邻接图传播）
- **Clustering**（远距离碎片合并为一个刚体，减少性能开销）

集成路径：
1. 从 [PhysX monorepo](https://github.com/NVIDIA-Omniverse/PhysX) 拉取 `blast/` 目录
2. Blast SDK 依赖 PhysX Foundation（DSEngine 已有）
3. 用 Blast 的 `NvBlastExtAuthoring` 替代自研 Voronoi 切分
4. 运行时用 `NvBlastActor` 管理碎片激活/合并

### 3.7 开源参考

| 项目 | 语言 | 用途 |
|---|---|---|
| [NVIDIA Blast](https://github.com/NVIDIA-Omniverse/PhysX/tree/main/blast) | C++ | PhysX 官方破碎库，与现有后端天然兼容 |
| [Voro++](https://math.lbl.gov/voro++/) | C++ | Voronoi 细胞计算库 |
| [Blender Cell Fracture](https://docs.blender.org/manual/en/latest/addons/object/cell_fracture.html) | Python | 离线切分工具，可导出碎片 mesh |

### 3.8 预估工作量

- **基础方案**（自研 Voronoi + 运行时激活）：离线工具 ~500 行 Python + 运行时 ~600 行 C++
- **Blast 集成**：~800 行 C++ 封装 + CMake 配置

---

## 实施优先级

| 顺序 | 功能 | 难度 | 理由 |
|---|---|---|---|
| 1 | 物理破碎 | ⭐⭐ | 最简单，NVIDIA Blast 与 PhysX 天然兼容，效果直观 |
| 2 | 布料模拟 | ⭐⭐⭐ | XPBD 核心代码量不大，但需处理碰撞和动态 VBO 更新 |
| 3 | 流体模拟 | ⭐⭐⭐⭐ | 需要 Compute Shader pipeline + 专用渲染 pass |

## 未来升级路径

如计划将 PhysX 从 4.x 升级到 5.x，布料和破碎可直接受益于：
- PhysX 5 内置 **FEM Soft Body** 求解器（替代自研 XPBD 布料）
- PhysX 5 monorepo 中 **Blast** 已深度集成
- PhysX 5 **GPU 加速**（GRB）覆盖刚体 + 软体 + 布料

这是性价比最高的长期演进路径。
