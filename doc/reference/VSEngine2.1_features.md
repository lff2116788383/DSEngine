# VSEngine 2.1 功能与范例梳理

本文档基于 `VSEngine2.1-main` 项目源码进行梳理，详细总结了该引擎实现的核心功能和范例。该引擎是一个体系完整、功能丰富的自研 3D 游戏引擎，非常适合作为自研游戏引擎的参考与指导。

## 一、 引擎核心功能模块 (Core Engine Features)

### 1. 底层系统与应用框架 (VSSystem & VSApplication)
- **平台与应用管理**: 提供跨平台的应用框架抽象 (`VSApplication`)、命令系统 (`VSCommand`) 以及主窗口循环管理。
- **系统基础设施 (`VSSystem`)**: 
  - **内存管理**: 自定义内存分配器 (`VSMemManager`)，集成了 TBB (Threading Building Blocks) 进行内存管理和并行计算加速。
  - **多线程系统**: 提供了线程 (`VSThread`) 和同步锁机制 (`VSSynchronize`)。
  - **文件与日志**: 文件 IO 系统 (`VSFile`)，日志记录系统 (`VSLog`)，以及 XML 解析器 (`VSXML`)。
  - **时间系统**: 高精度计时器 (`VSTimer`)。

### 2. 自研数据结构与数学库 (VSDataStruct & VSMath)
- **数据结构**: 完全脱离 STL 的自研泛型容器，包含动态数组 (`VSArray`)、双向链表 (`VSList`)、哈希表 (`VSHash`)、红黑树/二叉树 (`VSTree`)、字典 (`VSMap`) 和自定义字符串类 (`VSString`) 等。
- **基础数学计算**: 支持 SIMD (SSE) 优化的数学运算，包括二维/三维向量 (`VSVector2`, `VSVector3`)、齐次坐标向量 (`VSVector3W`)、四元数 (`VSQuat`) 和 4x4 矩阵 (`VSMatrix3X3W`)。
- **空间几何与包围盒**: AABB 盒 (`VSAABB3`)、OBB 盒 (`VSOBB3`)、球体 (`VSSphere3`)、圆柱体 (`VSCylinder3`)，以及射线 (`VSRay3`)、线段 (`VSLine3`)、平面 (`VSPlane3`) 和多边形 (`VSPolygon3`)。
- **高级曲线与曲面**: 实现了丰富的参数化曲线曲面算法，如贝塞尔曲线/曲面 (`VSBezierCurve3`, `VSBezierSurface3`)、B样条 (`VSB_SplineCurve3`)、NURBS (`VSNURBSCurve3`)，以及曲线/曲面细分算法。

### 3. 对象与场景管理 (VSGraphic Core)
- **对象系统 (Object System)**: 具备完整的 RTTI (运行时类型识别) 和反射机制 (`VSRtti`)，支持智能指针 (`VSPointer`) 管理对象生命周期和垃圾回收。支持对象的流式序列化 (`VSStream`)。
- **场景图与节点 (Scene Graph)**: 树状场景节点管理 (`VSNode`)，支持空间裁剪 (`VSCuller`)、四叉树场景管理 (`VSSceneManager`) 以及场景的异步加载 (`VSASYNLoader`)。
- **Actor-Component 实体组件模型**: 以 `VSActor` 为基础的实体系统，支持挂载多种组件，如静态网格组件 (`VSStaticMeshComponent`) 和骨骼网格组件 (`VSSkeletonMeshComponent`)。

---

## 二、 渲染系统与图形特性 (Render System)

### 1. 渲染管线与后端
- **多渲染 API 支持**: 抽象了渲染接口 (`VSRenderer`)，实现了 Direct3D 9 (`VSDx9Renderer`) 和 Direct3D 11 (`VSDx11Renderer`) 后端。
- **多线程渲染**: 将逻辑更新与渲染指令提交解耦，支持更新线程 (`VSUpdateThread`) 和渲染线程 (`VSRenderThread`) 并行运行。

### 2. 材质与 Shader 系统
- **节点化 Shader 生成**: 提供了一套基于节点的 Shader 字符串工厂 (`VSShaderStringFactory`)，允许动态拼装组合 Shader 函数 (`VSShaderFunction`)，生成顶点和像素着色器。
- **材质实例**: 支持 `VSMaterial` 及材质参数变量实例化 (`VSPEMaterial`)，支持世界位置偏移 (World Offset) 计算。
- **高级光照与阴影**:
  - 光照模型：实现了 Blinn-Phong 和 Oren-Nayar 光照模型，支持自定义光照计算。
  - 光源类型：支持方向光 (`VSDirectionLight`)、点光源 (`VSPointLight`)、聚光灯 (`VSSpotLight`) 及天光 (`VSSkyLight`)。
  - 阴影技术：全方位的阴影方案，包含贴图阴影 (ProjectShadow)、体积阴影 (ShadowVolume)，以及正交/级联阴影映射 (OSM/CSM)，支持 Dual-Paraboloid 点光源阴影。

### 3. 高级图形特性
- **LOD 系统**: 支持静态网格的多级细节 (Level of Detail) 切换和动态合并。
- **地形渲染系统**: 提供了极其丰富的地形算法实现，包含四叉树地形 (`VSQuadTerrainGeometry`)、ROAM 算法地形 (`VSRoamTerrainGemotry`)、连续 LOD (`VSCLodTerrainGeometry`)、离散 LOD (`VSDLodTerrainGeometry`) 以及基于 GPU 的 LOD 地形。
- **GPU 渲染特性 (DX11)**:
  - 硬件实例化 (Hardware Instancing): 静态模型与骨骼模型的动态 Instance 渲染。
  - 曲面细分 (Tessellation): 支持硬件细分。
  - 计算着色器 (Compute Shader): 支持通过 CS 处理 Buffer、StructBuffer 和 Texture。
  - 硬件遮挡剔除 (Hardware Occlusion Culling)。
- **后期处理 (Post-Processing)**: 灵活的后期特效链 (`VSPostEffectSet`)，实现了 Bloom、屏幕灰度、自定义材质后期等。

---

## 三、 动画系统 (Animation System)

该引擎具备一个成熟的 3D 骨骼与表情动画系统：
- **基础骨骼动画**: 支持骨骼节点 (`VSBoneNode`) 和蒙皮网格渲染。
- **动画树 (AnimTree)**: 基于状态树的动画系统，支持各种混合逻辑 (`VSAnimBlendFunction`)。
- **平滑与立即混合**: 支持两个动画动作间的平滑过渡混合或立即切换。
- **部分骨骼混合 (Partial Blend)**: 允许上半身和下半身播放不同的动画（如“边走边攻击”）。
- **叠加动画 (Additive Animation)**: 提取两帧差异生成叠加姿态。
- **根骨骼运动 (Root Motion)**: 动画可以直接驱动角色胶囊体的世界位移。
- **Morph 目标动画**: 支持顶点形变表情动画 (`VSMorphSet`)。

---

## 四、 工具与其他 (Tools & Input)

- **输入系统**: 封装了 `VSDx9Input` (基于 DirectInput) 以处理键盘和鼠标事件。
- **资产转换工具**:
  - **FBXConverter**: 使用官方 FBX SDK 解析 FBX 模型、材质、骨骼和动画，并转换为引擎自有的序列化格式 (`.STMODEL`, `.ACTION` 等)。
  - **FontTool**: 字体提取工具，用于生成引擎可用的位图字体格式 (`.FONT`)。
  - **NVCompression**: 集成 NVIDIA Texture Tools (NVTT) 进行纹理的 DXT 压缩转换。

---

## 五、 教学范例清单 (Demo Features)

项目 `Demo` 文件夹内包含了按章节编号的大量演示程序，是引擎各模块用法的最佳参考：

*   **Demo 3~8**: 引擎基础架构测试、智能指针、RTTI、资源克隆及对象序列化 (Save/Load) 演示。
*   **Demo 11**: 四叉树 (`QuadTree`) 场景管理与海量物体渲染测试。
*   **Demo 12**: FBX 导入流程演示；展示如何加载静态石头模型 (`Stone.STMODEL`) 并为其创建和赋予材质，最终进行渲染。
*   **Demo 13**: 
    *   LOD 网格：加载 5 个层级的 LOD Mesh 并自动根据距离切换，以及多 LOD Mesh 的合并技术。
    *   地形算法展示：分别演示 ROAM、CLOD、DLOD 地形的线框与实体渲染，并支持加载高程图。
*   **Demo 14 (动画系统核心)**:
    *   蒙皮模型材质加载与渲染。
    *   AnimTree 动画树的配置，走路与待机动画的平滑/立即混合调节。
    *   叠加动画演示与计算。
    *   部分混合动画：混合攻击动画上半身与走路动画下半身。
    *   基于引擎格式的根骨动画 (Root Motion) 解析与效果。
    *   Morph 脸部表情动画的参数调节与混合。
*   **Demo 15 (光照与材质特效)**:
    *   光照模型：Blinn-Phong、Oren-Nayar 及自定义光照点乘测试。
    *   后期处理：屏幕灰度滤镜、Bloom 泛光以及叠加自定义材质的后期链。
    *   阴影技术：方向光的 OSM / CSM 阴影；点光源传统阴影及 Dual-Paraboloid 阴影；聚光灯阴影；以及 ProjectShadow 和 ShadowVolume 的演示。
    *   灯光函数 (LightFunction)：光源投射纹理过滤效果。
    *   Material WorldOffset (材质世界坐标偏移) 顶点动画演示。
*   **Demo 16 (异步与多线程)**:
    *   场景资源的异步加载。
    *   开启多线程更新 (Update) 与多线程渲染。
    *   双 IndexBuffer 轮换计算地形网格索引，并将计算任务安全发送至渲染线程。
*   **Demo 18 (硬件实例化 Instancing)**:
    *   静态模型的动态 Instance 渲染。
    *   蒙皮骨骼动画网格 (Skin Instance Mesh) 的实例化及动画树驱动。
    *   DX11 材质变量实例演示。
*   **Demo 19**: DirectX 11 曲面细分 (Tessellation) 地面模型演示。
*   **Demo 20**: Compute Shader 测试，包含对 Buffer、StructBuffer 和 Texture 资源的计算写入操作。
*   **Demo 21**: 硬件遮挡剔除 (Hardware Occlusion Culling) 测试。

> **自研指导建议**：
> VSEngine 2.1 展现了传统 C++ 商业级自研引擎的标准架构（不依赖 STL，全量自研数据结构和数学库，高度抽象的 RTTI 与序列化），其对多地形算法、完整的光照阴影管线以及基于树节点的动画系统（AnimTree）的实现非常扎实。可以作为构建“纯自研底层架构”和“进阶渲染/动画系统”的绝佳教科书代码。