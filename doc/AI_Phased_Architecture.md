# DSEngine Phased Architecture (AI-Driven 2D to 3D)

> **目标与原则**：本架构文档专为 AI 辅助开发设计，采用**“高射炮底座打蚊子”**的策略。底层基建必须以满足现代 3D 引擎的要求（纯 ECS、RHI 抽象、全维度数学库、多线程）来构建，但在第一阶段仅实现并验证 2D 渲染与逻辑。第二阶段则在不修改底层接口的前提下，通过横向扩展组件与系统，无缝升级为 3D/全栈引擎。

---

# 阶段一：筑基与高性能 2D 引擎 (Phase 1: Foundation & 2D)

**AI 执行指令**：在此阶段，AI **禁止**实现任何与 3D 渲染（如 PBR、延迟管线）、3D 物理（PhysX）相关的业务逻辑。但 AI **必须**使用支持 3D 维度的底层数据结构（如 Mat4、Vec3）和现代渲染硬件接口（RHI）抽象。

## 1.1 核心基建（按 3D 标准设计）

*   **全维度数学库**：
    *   **实现**：引入/封装完整的 3D 数学库（`Vec2`, `Vec3`, `Vec4`, `Mat4`, `Quaternion`）。
    *   **约束**：即使是 2D 变换，底层也必须使用 `Mat4`，只需保持 Z 坐标为 0，以保证未来 3D 扩展时数据结构的一致性。
*   **纯 ECS 架构引入**：
    *   **实现**：彻底废弃基于 `GameObject` 的 OOP 继承树。直接引入成熟的 ECS 框架（强烈建议使用 `EnTT`）。
    *   **设计**：实体仅为 ID，组件为纯数据结构（SoA/稀疏集内存布局），逻辑全交由 System 执行。
*   **现代 RHI（渲染硬件接口）抽象**：
    *   **实现**：**禁止**在业务代码中直接调用 `gl*` 函数。必须抽象出 `CommandBuffer`, `RenderPass`, `Pipeline`, `Buffer` (VBO/UBO), `Texture` 等接口。
    *   **目标**：当前后端可先用 OpenGL 3.3+ 实现，但接口设计必须兼容未来的 Vulkan/WebGPU。
*   **基础工具链**：
    *   **实现**：日志系统（如 `spdlog`）、时间系统、基础文件 IO、平台窗口管理（GLFW封装，屏蔽细节）。

## 1.2 2D 核心组件与系统 (2D ECS)

*   **组件 (Components)**：
    *   `TransformComponent`（内部使用 Vec3 位置和 Quat 旋转，但在 2D 语境下提供 Vec2 的便捷访问）。
    *   `SpriteRendererComponent`（纹理引用、颜色、UV）。
    *   `CameraComponent`（配置为正交投影 - Orthographic）。
    *   `RigidBody2DComponent`, `BoxCollider2DComponent`。
*   **系统 (Systems)**：
    *   `TransformSystem`（更新局部/全局矩阵）。
    *   `SpriteRenderSystem`（收集所有 Sprite，进行极速批处理/Batching，提交给 RHI）。
    *   `Physics2DSystem`（集成轻量级的 Box2D，更新 Transform）。

## 1.3 2D 资源管理与渲染前端

*   **资源管理器 (Asset Manager)**：
    *   实现基于引用计数的基础资源加载（纹理、着色器）。
    *   预留异步加载接口。
*   **2D 渲染管线**：
    *   **Sprite Batcher**：实现基于动态 VBO 的精灵批处理，单帧将同材质的 Sprite 合并为一个 Draw Call。
    *   **UI 渲染**：基于正交相机的基础 UI 渲染（可复用 Sprite Batcher）。

**阶段一交付标准**：
1. 跑通一个纯 2D 场景（包含成百上千个物理碰撞的 Sprite）。
2. Draw Call 保持在个位数（验证批处理是否生效）。
3. 源码中没有任何 `GameObject` 或虚函数 `Update()`，完全由 `EnTT` 驱动。

---

# 阶段二：横向扩展与 3D 引擎 (Phase 2: 3D & Full Stack)

**AI 执行指令**：在此阶段，AI 假设第一阶段的底层（ECS、RHI、数学库）已绝对稳定。主要任务是**新增** 3D 专用的组件、系统、资源解析器和渲染管线。2D 逻辑作为特定子系统继续并行存在。

## 2.1 3D 核心组件与系统扩展

*   **新增组件 (3D Components)**：
    *   `MeshRendererComponent`（Mesh 引用、Material 引用）。
    *   `LightComponent`（Directional, Point, Spot，包含颜色、强度、阴影参数）。
    *   `CameraComponent` 扩展（支持透视投影 - Perspective）。
    *   `RigidBody3DComponent`, `Collider3DComponent`。
*   **新增系统 (3D Systems)**：
    *   `MeshRenderSystem`（视锥体剔除、收集渲染对象）。
    *   `Physics3DSystem`（集成 Jolt Physics 或 PhysX，处理 3D 碰撞与动态模拟）。

## 2.2 3D 资源管理进阶

*   **模型与材质导入**：
    *   引入 Assimp 或 cgltf，支持加载 GLTF/FBX 模型（包含顶点、法线、切线、UV）。
    *   实现 Material 资源系统，支持 PBR 贴图（Albedo, Normal, Metallic, Roughness, AO）。
*   **流式与异步**：
    *   完善阶段一预留的异步加载机制，将大尺寸纹理和模型加载放入后台线程。

## 2.3 现代 3D 渲染管线 (Render Graph)

基于阶段一稳定的 RHI 接口，构建复杂的 3D 渲染流：
*   **Forward / Deferred Pipeline**：实现 PBR（基于物理的渲染）光照模型。
*   **阴影系统**：实现级联阴影贴图 (CSM) 或点光源阴影。
*   **后处理 (Post-Processing)**：基于 Render Target 堆栈，实现 Bloom、Tone Mapping、Gamma Correction。
*   **2D/3D 混合渲染**：
    *   在渲染管线末端，使用正交相机将阶段一的 `SpriteRenderSystem` 结果叠加到 3D 画面之上（作为 UI 或 HUD）。

**阶段二交付标准**：
1. 跑通一个包含 PBR 材质、动态光照和阴影的 3D 场景。
2. 3D 场景中可无缝叠加 2D 物理与 2D UI 元素，两者互不冲突。
3. 纯 2D 项目可通过编译宏或配置，完全剔除 3D 渲染与物理模块的开销。

---

# AI 开发执行指南 (AI Execution Protocol)

为了确保按部就班且不产生架构冲突，AI 在阅读本指令后需遵循以下开发协议：

1. **Step-by-Step 确认**：必须先 100% 跑通 Phase 1，经过用户测试确认 Draw Call 优化和 ECS 运行无误后，才能开启 Phase 2 的代码生成。
2. **严禁越界**：在执行 Phase 1 时，绝对禁止写出 `glDrawElements` 这种直接调用，必须强迫自己写 RHI 接口（如 `Renderer::DrawIndexed(cmd)`）。
3. **拥抱开源**：不要手写复杂的造轮子代码。ECS 强依赖 `EnTT`，数学依赖 `GLM`，2D 物理依赖 `Box2D`。把精力放在**架构粘合**与**渲染管线设计**上。