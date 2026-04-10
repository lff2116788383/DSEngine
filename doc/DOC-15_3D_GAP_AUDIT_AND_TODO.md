# DOC-15 3D 能力审计与补全待办清单

本文档基于当前仓库的真实代码、编辑器面板、测试入口与构建现状，对 DSEngine 当前 3D 能力做一次收敛式审计。

目标不是继续泛泛地说“要补 3D”，而是明确：

- 当前 3D 已经有什么
- 当前 3D 哪些只是能力点，哪些接近工作流
- 距离“轻量级高性能现代化引擎”还差什么
- 下一阶段应该先补哪些，而不是继续无边界扩张

---

## 1. 审计结论

一句话结论：

> 当前 3D **已具备基础能力点与部分编辑器检视支持**，但**尚未形成最小稳定闭环**；当前最欠缺的不是继续增加零散模块，而是把已有 Mesh / Camera / Light / Shadow / Skybox / Scene / Test 这些能力真正收口成 MVP 链路。

更直白一点说：

- **不是没有 3D**
- **而是 3D 还没有达到“可稳定验证、可持续迭代、可明确对外表述”的程度**

---

## 2. 当前 3D 已有能力

从 `modules/gameplay_3d/`、Editor Inspector 和现有测试入口来看，当前仓库已经具备以下 3D 能力点。

### 2.1 模块与子系统层

当前 `modules/gameplay_3d/` 已存在：

- `rendering/mesh_render_system.*`
- `rendering/frustum_culling_system.*`
- `rendering/terrain_system.*`
- `camera/free_camera_controller_system.*`
- `animation/animator_system.*`
- `animation/animation_state_machine.*`
- `particles/particle3d_system.*`
- `ai/steering_system.*`
- `gameplay_3d_module.*`

这说明：

- Mesh 渲染已接入
- 3D Camera 控制已接入
- Animator / 状态机已接入
- Terrain / Particle3D / Frustum Culling / Steering 已有实现
- 3D 模块化装配路径已经存在

### 2.2 运行时渲染链路层

从 `FramePipeline` 和当前渲染主线可见，已经具备：

- Camera3D 选择与投影构建
- Directional Light 基础链路
- Cascaded Shadow Map（至少存在基础 pass 组织）
- Skybox 组件路径
- Mesh 渲染 pass 接入
- 基础 Render Graph pass 结构

### 2.3 编辑器检视层

从 `apps/editor_cpp/src/editor_inspector_panel.cpp` 可见，Inspector 已支持多种 3D 组件：

- `MeshRendererComponent`
- `Camera3DComponent`
- `DirectionalLight3DComponent`
- `PointLightComponent`
- `SkyboxComponent`
- `Animator3DComponent`
- 以及部分 3D Physics / 3D Particle 相关组件

这意味着：

- 3D 不是“代码层有、编辑器层完全没有”
- 至少在 Inspector 维度，3D 组件已经具备基础调参与检视入口

### 2.4 测试层

当前 3D 相关测试入口并不算完全空白，至少可以确认有：

- `tests/engine/physics/physics3d_system_test.cpp`
- 以及仓库中存在过 `engine.3d.*` 相关测试编译/输出痕迹（例如 terrain、skybox、camera_3d、mesh_renderer、frustum、csm、steering 等）

这说明：

- 3D 不是完全没有测试意识
- 但测试口径是否稳定、是否纳入主门禁，仍需进一步收口

---

## 3. 当前 3D 的核心问题

### 3.1 已有能力点很多，但闭环不清楚

当前最大问题不是“模块不够多”，而是：

- 已有能力点之间缺少明确的 MVP 闭环定义
- 不容易回答“现在最小 3D 场景到底能不能稳定制作、保存、运行、回归”

### 3.2 3D 工作流还没有被明确定义为一个最小产品能力

当前代码里有：

- Mesh
- Camera
- Light
- Shadow
- Skybox
- Animator
- Terrain
- Particle3D

但这不等于：

- 已有一个稳定可交付的 3D 最小工作流

### 3.3 测试与文档口径仍偏弱

2D 主线的优势在于：

- 文档口径比较清晰
- 测试入口较明确
- 主链目标比较统一

而 3D 当前更像：

- 有实现
- 有组件
- 有部分测试
- 但缺少统一“完成定义”

### 3.4 3D 物理与构建链路不稳

当前 PhysX 相关构建路径仍不是稳定默认链路，这意味着：

- 即使 3D Physics API/组件存在
- 也还不能把 3D Physics 描述为稳定主线能力

---

## 4. 距离“轻量级高性能现代化引擎”还差什么

### 4.1 渲染架构层

还差：

- 更稳定的材质系统
- 更明确的 shader 变体管理
- 更系统的 PBR 管线收口
- 更完整的后处理链
- 更清晰的 Render Graph / Pass 组织边界
- 更可维护的渲染资源管理策略

### 4.2 性能能力层

还差：

- 更稳定的可见性裁剪链路
- 更清晰的实例化与批次优化策略
- 更成熟的大场景异步资源上传
- 更系统的 CPU/GPU profiling 口径
- 更明确的性能门禁与性能基线

### 4.3 资源工作流层

还差：

- 更稳定的模型/材质/动画资源导入链路
- 更一致的逻辑路径资源模型在 3D 侧的落地
- 更清晰的资源依赖、错误诊断与热重载恢复
- 更稳定的 3D 场景保存 / 加载 / 回归链路

### 4.4 编辑器工作流层

还差：

- 更成熟的 3D Scene View 交互体验
- 更稳定的 3D Play/Edit 隔离
- 更明确的 3D 资源调参与验证闭环
- 不仅能“显示组件”，而且能形成“最小制作流”

### 4.5 工程化层

还差：

- 更明确的 3D 开关与默认构建口径
- 更稳定的 3D smoke / integration / regression 体系
- 更一致的文档、样例、测试与构建表述

---

## 5. 下一阶段优先级建议

## 5.1 P0：必须先补（优先做）

这些不补，3D 仍然只能算“已有能力点”，不能算“MVP 闭环”。

### P0-1 最小 3D 场景闭环定义与验收

建议先固定一个最小 3D 闭环场景，至少包含：

- Mesh
- Camera3D
- Directional Light
- Shadow
- Skybox

并明确其完成定义：

- Runtime 可运行
- Editor 可检视
- 场景可保存 / 加载
- 至少一条 smoke / regression 可重复运行

### P0-2 3D 场景保存 / 加载链路验证

目标不是继续加组件，而是确认：

- 关键 3D 组件是否都能正确保存
- 是否能正确加载回场景
- 参数是否有回归验证

### P0-3 3D smoke / regression 清单化

需要把当前零散的 3D 测试入口整理成一张清单：

- 已存在且稳定
- 已存在但未纳入主门禁
- 名义存在但不稳定/不可重复
- 仍然缺失

### P0-4 3D 构建路径收口

重点确认：

- `DSE_ENABLE_3D=ON` 的稳定构建口径
- PhysX 不可用时 3D 的退化路径
- 哪些 3D target 是当前必须稳定的

## 5.2 P1：应补（在 P0 后推进）

- 材质系统收口
- Animator / 动画状态机最小工作流收口
- 3D Inspector / Scene View 交互体验补强
- 更明确的场景级 profiling 与性能观测

## 5.3 P2：后续扩展（不要抢前面）

- 更完整 Terrain 生态
- 更高级后处理矩阵
- 更完整 3D 粒子生态
- 更系统的 3D 物理编辑器工作流
- 更完整的资源数据库 / meta / UUID 体系

---

## 6. 建议的执行顺序

建议后续不要写成“继续补 3D”，而是按下面顺序推进：

1. 固定最小 3D MVP 场景定义
2. 验证 3D 场景保存 / 加载主链
3. 收拢 3D smoke / regression 清单
4. 收口 3D 构建路径与开关口径
5. 再补材质 / Animator / 编辑器体验等增强项

---

## 7. 最终结论

当前 3D 更准确的表述应为：

> **DSEngine 已具备一批基础 3D 能力点与部分编辑器检视支持，但距离轻量级高性能现代化引擎的 3D 目标，仍缺少一个最小稳定闭环。下一阶段不应继续无边界铺功能，而应优先把 Mesh / Camera / Light / Shadow / Skybox / Scene / Test 这些已有能力收口成 MVP。**
