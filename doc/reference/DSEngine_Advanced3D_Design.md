# DSEngine 高级 3D 架构设计：基于 VSEngine2.1 的技术提取与重构

在完成了基础的 3D 渲染（PBR、Shadow Mapping、骨骼动画）后，为支撑“RPG 烽火与炊烟版经营模拟”这样的大型场景与复杂动作逻辑，我们需要从硬核商业级引擎 `VSEngine2.1` 中提取高级算法，并用现代 C++ (C++17) 和 ECS 架构重构到 `DSEngine` 中。

本文档总结了从 `VSEngine2.1` 源码中提取的三大核心技术的算法思路与重构计划。

---

## 1. 大面积地形渲染系统 (Terrain System)

### VSEngine2.1 源码分析
在 `VSEngine2.1` 的 `VSGraphic/` 目录下，地形系统被深度抽象，支持多种细节层次(LOD)算法：
- **核心基类**: `VSTerrainNode` (继承自 `VSMeshNode`)，负责高度图(`m_pHeight`)和细分等级(`m_uiTessellationLevel`)的管理。
- **CLOD (Continuous Level of Detail) 算法**: 包含 `VSRoamTerrainGeometry` (基于 ROAM 算法的双二叉树视点相关多边形) 和 `VSQuadTerrainGeometry` (四叉树算法)。
- **算法核心**:
  - `ComputeVariance()`: 预处理计算地形块的高度方差，作为运行时合并/分裂（评价崎岖度）的依据。
  - `RecursiveComputeVariance()`: 递归计算，评估当前地形块顶点高度与相邻顶点插值高度的误差。

### DSEngine 重构计划 (ECS 适配)
摒弃 `VSEngine2.1` 中基于深层继承树的面向对象设计（`VSTerrainNode -> VSMeshNode -> VSSpatial`），改为 ECS 扁平架构。

1. **TerrainComponent (地形组件)**:
   - 存储高度图纹理 Handle 或 Heightmap CPU 内存数据。
   - 存储地形缩放(`height_scale`, `xz_scale`)和尺寸。
   - 存储地形网格的细分参数。
2. **TerrainSystem (地形系统)**:
   - **阶段一 (基础)**: 实现均匀网格的地形渲染，在 `AssetBuilder` 中解析灰度图生成基础网格数据。
   - **阶段二 (高级 - 提取自 VSEngine)**: 实现四叉树 (QuadTree) 空间划分与视椎体剔除(Frustum Culling)。在 CPU 端根据相机距离动态决定渲染的叶子节点层级。

---

## 2. 动画状态机与混合树 (AnimTree & Blend Tree)

### VSEngine2.1 源码分析
`VSEngine2.1` 实现了一套非常完整的节点式动画树驱动系统，解决了角色动作平滑过渡和上下半身分离播放的问题。
- **核心基类**: `VSAnimTree`，作为容器管理所有的动画节点功能(`m_pAnimFunctionArray`)。
- **节点抽象 (`VSAnimFunction`)**:
  - `VSAnimSequenceFunc`: 基础的单向动画播放序列。
  - `VSAnimBlendFunction`: 混合节点基类。包含静态函数 `AdditiveBlend` (加性混合) 和 `LineBlendTwo` (线性插值混合)。
- **高级混合节点**:
  - `VSRectAnimBlend`: 基于 2D 参数 (X,Y) 的四向动画混合（如根据速度和方向混合 走/跑/左平移/右平移）。
  - `VSPartialAnimBlend`: 局部骨骼混合（Partial Blend）。支持通过 `m_BoneWeight` 对特定的骨骼分支（如仅上半身 Spine_01 及子节点）应用不同的权重，实现边跑边攻击。

### DSEngine 重构计划 (ECS 适配)
当前 `DSEngine` 的 `AnimatorSystem` 仅支持播放单一 `.danim` 文件。我们需要将其升级为**基于图/树的计算模型**。

1. **AnimTree 数据结构定义**:
   - 在 `Animator3DComponent` 中引入 `AnimGraph` 概念，替代原有的单一 `danim_path`。
   - 使用节点图(Node Graph)定义，包含：状态节点(State)、过渡边(Transition)、混合树(BlendTree, 1D/2D)。
2. **AnimatorSystem 升级**:
   - 提取 `VSAnimBlendFunction::LineBlendTwoAll` 的核心算法，实现基于权重的骨骼局部变换(Local Transform)插值（位置使用 Lerp，旋转使用 Slerp/Squad）。
   - 实现**局部混合 (Partial Blend)**: 引入骨骼权重遮罩(Bone Weight Mask)，在层级计算(Forward Kinematics)前，根据遮罩分配不同动画通道的影响力。

---

## 3. 级联阴影贴图 (CSM - Cascaded Shadow Maps)

### VSEngine2.1 源码分析
在 `VSDirectionLight.cpp` 中，实现了对大型场景至关重要的 CSM 算法。
- **核心函数**: `VSDirectionLight::DrawCSM()`
- **算法流程**:
  1. 将相机的视椎体(Frustum)按距离划分为多个层级（如 `Range[CSM_LEVLE + 1] = {fNear, 2000.0f, 7000.0f, fFar}`）。
  2. 针对每个 Cascade 层级，计算其在光源空间(Light Space)下的 AABB。
  3. 动态计算正交投影矩阵 (`LightCamera.SetOrthogonal(...)`)，使得当前层级的阴影贴图刚好覆盖该子视椎体。
  4. 将生成的多个 Light Matrix (`m_CSMLightShadowMatrix[3]`) 传入 Shader。

### DSEngine 重构计划 (ECS 适配)
我们现有的 Shadow Pass 是单张贴图，在 RPG 视角的广阔地形下会出现严重锯齿。

1. **RHI 层扩展**:
   - 引入 Texture Array 或 Texture Atlas 用于存储多级 Depth Map。
   - `FramePipeline::BuildRenderGraph()` 中的 Shadow Pass 修改为循环渲染 3-4 次（对于每个 Cascade）。
2. **相机截截头计算**:
   - 提取 `VSDirectionLight::DrawCSM` 中对相机 Frustum 切片和光源正交矩阵包围盒(AABB)的计算代码。
3. **PBR Shader 升级**:
   - 在 `rhi_device.cpp` 的 PBR 材质 Shader 中，根据片段的深度值(View Space Z)动态选择对应的 Cascade 索引进行阴影采样。

---

## 开发路线图建议

为了保证引擎稳定性，建议按以下顺序推进：
1. **优先：AnimTree 骨骼动画融合**。这对角色的表现力提升最大，且只涉及 CPU 端数学计算和 ECS 数据结构扩展，风险较低。（已实现）
2. **其次：TerrainSystem 基础地形**。实现基于高度图的地形生成和多材质 Splatting，这是构建 RPG 世界的前提。（已实现）
3. **最后：CSM 级联阴影**。在地形场景搭建好之后，现有的单图阴影劣势会凸显，届时再引入 CSM 提升画质。（已实现）

---

## 4. 未来高级特性探索 (VSEngine2.1 剩余价值池)

在我们完成了上述三大核心特性的初步提取与实现后，`VSEngine2.1` 源码库（特别是 `VSGraphic` 等模块）中仍蕴含着大量工业级的高级算法实现，可作为 `DSEngine` 后续迭代（尤其是大世界和次世代渲染）的“立体代码教科书”：

### 4.1 多光源系统与局部阴影 (Point/Spot Lights & Local Shadows)
- **VSEngine 源码**: `VSPointLight`, `VSSpotLight`, `VSSkyLight`, `VSShadowPass`
- **当前现状**: DSEngine 仅实现了平行光 (`DirectionalLight3DComponent`) 及 CSM。
- **挖掘价值**: 对于 RPG 游戏中的地牢探索、火把、魔法特效等，点光源与聚光灯是烘托氛围的核心。进一步可以提取其基于 Cubemap 的点光源全向阴影映射 (Omnidirectional Shadow Mapping)。

### 4.2 Morph 目标动画 (Blend Shapes / 顶点变形动画)
- **VSEngine 源码**: `VSMorphSet.h`, `VSMorph`
- **当前现状**: DSEngine 仅实现了骨骼蒙皮动画 (`Animator3DComponent`)。
- **挖掘价值**: RPG 游戏的剧情演出、NPC 对话口型同步 (Lip-sync)、面部微表情（捏脸系统）均强依赖于 Morph 动画。提取此模块将极大提升角色的面部表现力。

### 4.3 高级后处理特效与 PreZ (Advanced Post-Processing & Screen Space)
- **VSEngine 源码**: `VSCommonPostEffect.h`, `VSNormalDepthPass`, `VSPreZPass`
- **当前现状**: 已提取 Bloom 和 Tone Mapping。
- **挖掘价值**: `VSEngine` 中包含饱和度调整、老照片滤镜等更多后处理。更重要的是，提取它的 `VSPreZPass` (深度预渲染) 或法线深度 Pass，可以为我们后续实现 SSAO（屏幕空间环境光遮蔽）和 DoF（景深）打下 G-Buffer 基础。

### 4.4 空间划分与层次视锥体剔除 (Spatial Partitioning BVH/Octree)
- **VSEngine 源码**: `VSSceneManager`, `VSSpatial` 树状层级
- **当前现状**: 目前 DSEngine 的 `FrustumCullingSystem` 使用的是线性遍历 ($O(N)$ 复杂度)。
- **挖掘价值**: 面对大世界中海量的实体，线性剔除会成为 CPU 瓶颈。提取 `VSEngine` 的八叉树 (Octree) 或层次包围盒 (BVH) 算法，将剔除复杂度降至 $O(\log N)$。

### 4.5 AI 状态机与群聚/避障行为 (AI & Steering Behaviors)
- **VSEngine 源码**: `VSAIState`, `VSSteer`
- **当前现状**: DSEngine 侧重渲染，暂无 AI 行为层。
- **挖掘价值**: `VSSteer` 包含了标准的转向行为算法（寻路、避障、群聚等），非常适合直接重构为 ECS 架构下的 `SteeringSystem` 和 `AIStateComponent`，为怪物的 AI 逻辑提供底层支持。

### 4.6 异步资源与流式加载系统 (Asynchronous Loading & Streaming)
- **VSEngine 源码**: `VSASYNLoader`, `VSStreamingManager`
- **当前现状**: 资源加载主要是同步的（或仅简单的 Budget Pump）。
- **挖掘价值**: 面向大地图无缝切换与内存管理的刚需。流式加载管理器可根据视点距离动态加载/卸载纹理和网格数据。

### 4.7 高级地形与动态 LOD (Advanced Terrain & GPU LOD)
- 目前 DSEngine 的 `TerrainSystem` 仅实现了基础的均分网格生成，而 `VSEngine2.1` 提供了多种连续与离散的 LOD 地形方案：
  - **`VSCLodTerrainGeometry` / `VSDLodTerrainGeometry`**：经典的连续与离散细节层次算法。**(✅ 已部分提取：在 `TerrainSystem` 中实现了基于距离的 Quad 动态网格步长 LOD，可有效减少远景顶点数)**
  - **`VSGPULodTerrainGeometry`**：基于 GPU 细分或 Compute Shader 的现代地形渲染，极大地解放 CPU。
  - **`VSQuadTerrainGeometry`**：大世界地图切分与调度的核心（四叉树地形）。

### 4.2 后处理管线与特效 (Post-Processing & Screen Space Effects)
- `VSEngine2.1` 具备完整的后处理架构（`VSPostEffectPass`, `VSCommonPostEffect` 等），其中包含了现代游戏必备的视觉增强算法：
  - 泛光 (Bloom) 和 色调映射 (Tone Mapping) **(✅ 已提取并实现为基于 RenderGraph 的 `PostProcessComponent` 和对应的后处理 Pass)**
  - 屏幕空间环境光遮蔽 (SSAO)
  - 屏幕空间反射 (SSR)
  - 景深 (Depth of Field)、运动模糊 (Motion Blur)

### 4.3 异步资源与流式加载系统 (Asynchronous Loading & Streaming)
- 面向大地图无缝切换与内存管理的刚需：
  - **`VSASYNLoader`**：多线程资源加载，避免主线程卡顿。
  - **`VSStreamingManager`**：流式加载管理器，可根据视点距离动态加载/卸载纹理和网格数据。这对于我们第二/第三阶段重构 `AssetManager` 极具参考价值。

### 4.4 场景管理与精细剔除 (Scene Management & Culling)
- 随着实体数量的激增，线性的 ECS 遍历将成为性能瓶颈：
  - **`VSSceneManager`** 及其派生类：八叉树 (Octree) / 四叉树 (QuadTree) 的空间划分。
  - **`VSCuller`**：精细的视锥体剔除和遮挡剔除算法，用于在渲染提交前快速过滤不可见物体。**(✅ 已部分实现：提取 VSCuller 平面裁剪算法，完成了基于 `BoundingBoxComponent` 的 `FrustumCullingSystem`)**

### 4.5 扩展动画系统 (Advanced AnimTree Features)
- 现有的 AnimTree 仅搭建了基础混合，未来可引入：
  - **`VSRectAnimBlend`**：2D 混合空间（如根据 X/Y 速度轴平滑混合站立、走、跑、左右横移）。
  - **`VSOneParamSmoothAnimBlend`**：一维平滑过渡，处理更细腻的动作切分。

### 4.6 行为与人工智能 (AI State Machine & Steering)
- 为后续的 Gameplay 提供支撑：
  - **`VSAIState`**：轻量级状态机。
  - **`VSSteer`**：转向行为算法（寻路、避障、群聚），适合 RPG 中的 NPC 自动控制。