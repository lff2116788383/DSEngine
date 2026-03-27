# KF_ModelAnalyzer 功能梳理

`KF_ModelAnalyzer` 是一个基于 C++ 和 DirectX 9 开发的 3D 模型解析与预览工具，主要用于分析和处理游戏开发中的 3D 模型资产。该项目采用组件化（Component-Based）架构，并集成了 ImGui 作为工具界面。

以下是根据代码结构梳理出的核心功能模块：

## 1. 核心架构与系统 (Core System)
- **组件化架构 (Component System)**：采用 GameObject-Component 设计模式，包括 `gameObjectActor` 和基础的 `component` 接口。
- **场景与对象管理 (Manager System)**：通过 `manager.cpp` 统筹管理整个应用的各个子系统（如 `meshManager`, `textureManager`, `lightManager`, `gameObjectManager` 等）。
- **输入系统 (Input System)**：基于 DirectX Input (`inputDX.cpp`, `inputManager.cpp`) 实现键盘和鼠标事件的捕获与处理。
- **数学库 (Math Library)**：自定义了数学工具类 `KF_Math`，用于处理 3D 向量、矩阵、旋转等数学运算。

## 2. 模型解析与转换 (Model Analyzer & FBX)
- **FBX 模型解析 (`KF_UtilityFBX.cpp`)**：
  - 集成了官方的 Autodesk FBX SDK (`libfbxsdk.dll`)。
  - 支持解析 `.fbx` 格式中的网格数据（Mesh）、材质（Material）、骨骼节点（Skeleton）以及动画数据（Animation）。
- **自定义模型格式导出/导入**：
  - 支持将解析后的 FBX 数据转换为引擎自定义格式（如 `.mesh`, `.material`, `.avatar`, `.json` 等），并保存在 `data/` 目录下。

## 3. 渲染系统 (Rendering System)
- **DirectX 9 渲染后端 (`rendererDX.cpp`)**：基于 D3D9 的底层渲染封装。
- **渲染组件 (Render Components)**：
  - `2DDrawComponent` / `2DMeshComponent`: 用于 2D 元素的渲染。
  - `3DMeshDrawComponent` / `3DMeshComponent`: 用于 3D 网格模型的渲染。
- **摄像机系统 (Camera System)**：
  - `editorCamera` / `modelAnalyzerCamera`: 用于工具编辑器视角的自由相机控制。
  - `actionGameCamera`: 用于动作游戏视角的第三人称相机。
- **材质与光照 (`materialManager.cpp`, `lightManager.cpp`)**：
  - 支持基础光照计算（Directional Light）。
  - 支持材质属性解析和纹理贴图（Diffuse, Normal, Specular）的应用（如 `data/TEXTURE` 目录下的贴图）。

## 4. 动画系统 (Animation System)
- **动画控制器 (`animator.cpp`, `animatorComponent.cpp`)**：
  - 支持骨骼动画的播放与更新。
  - 支持多段动画的解析和切换（参考 `data/FBX` 下角色包含的 Idle, Run, Attack 等丰富动画序列）。

## 5. 物理与碰撞系统 (Physics & Collision)
- **碰撞系统 (`KF_CollisionSystem.cpp`, `KF_CollisionUtility.cpp`)**：
  - 实现了多种包围盒碰撞检测算法。
  - 支持的碰撞体组件包括：
    - `AABBColliderComponent` (轴对齐包围盒)
    - `OBBColliderComponent` (有向包围盒)
    - `sphereColliderComponent` (球体碰撞)
    - `boxColliderComponent` / `fieldColliderComponent`。
- **物理刚体 (`KF_PhysicsSystem.cpp`, `3DRigidbodyComponent.cpp`)**：
  - 基础的刚体动力学模拟，如重力计算和简单的物理响应。

## 6. 用户界面与工具交互 (UI & Tool Interaction)
- **ImGui 集成**：使用 Dear ImGui (`ImGui/imgui_impl_dx9.cpp`) 构建工具的图形界面。
- **模型分析器行为 (`modelAnalyzerBehaviorComponent.cpp`)**：
  - 界面提供下拉框（Combo）和列表框（ListBox）用于选择模型。
  - 支持切换显示选项（如：显示/隐藏骨骼、显示/隐藏网格、显示/隐藏碰撞体及包围球）。
  - 支持一键导出（SaveModel）自定义格式模型。

## 总结
`KF_ModelAnalyzer` 是一个功能完备的模型预览和格式转换工具。它不仅展示了如何从零手写一套基于 DX9 的 GameObject-Component 渲染框架，还完整展示了**如何利用 FBX SDK 提取模型顶点、骨骼、动画数据并将其持久化为自定义文件格式**的全过程，非常适合作为游戏引擎资源管道（Asset Pipeline）开发的参考案例。