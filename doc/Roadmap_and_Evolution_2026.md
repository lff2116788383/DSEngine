# DSEngine 后续演进与发展规划（2026版）

> 目标：基于本地已有的 2D 基建架构（ECS、RHI、C++/Lua双宿主等），构建出从极简 2D 到全栈 3D 的商业化引擎。
> 核心产品拆分：
> - **Launcher (Nexus Launcher)**：独立启动器，负责账号登录、引擎版本更新与下载、项目管理以及资产商城。
> - **Editor (天工开物)**：游戏编辑器本体，聚焦于场景编排、资源流转、工具链及跨平台打包，不负责热更新。
> - **Runtime**：基于 C++ 与 Lua 的双宿主运行环境，提供底层渲染、物理、ECS 驱动等。

---

## 1. 演进路线图 (Engine Roadmap)

> **最新执行原则（当前建议主线）**
>
> 先完成 **2D 功能闭环与工程化收口**，再进入 **3D MVP**。
>
> 原因：当前 2D Runtime 主链已基本具备 Sprite / Physics / UI / Tilemap / Animation / Spine / Lua/C++ 双宿主等能力，但编辑器闭环、自动化回归、资源链路一致性与性能基线仍需补强；若此时直接扩张 3D，会放大维护成本、稀释测试资源，并导致 Editor / Runtime / Asset Pipeline 三条主线同时失焦。
>
> 因此，后续路线按以下优先级执行：
>
> 1. **P0：2D 功能闭环**
> 2. **P1：2D 商业化工程能力**
> 3. **P2：3D MVP**
> 4. **P3：3D 商业版与生态化**

### 阶段 1：2D MVP 版本（当前主线已完成）
- **引擎内核**：基于 Job System 的任务调度、RHI 接口抽象、全维度数学库支撑的 2D 变换；集成 EnTT 形成纯 ECS 架构。
- **功能链路**：打通 Sprite、Physics(Box2D)、UI、Audio、Tilemap 与 Animation 链路。
- **脚本与宿主**：C++ / Lua 双宿主行为一致，并完成基础接口绑定。
- **开发工具**：基础的 Editor（基于 React + Node-API）打通渲染视口，具备实体创建与属性编辑能力。

### 阶段 2：2D 功能闭环与商业版本（当前最高优先级）
*   **引擎内核进阶**：
    *   **Spine 2D 动画**：引入官方 spine-cpp，完成运行时解析与多网格批处理渲染。
    *   **Prefab 系统**：支持节点树与组件属性的 JSON 序列化，并支持嵌套层级实例化。
    *   **Asset Bundle**：集成 bundle 归档与 tiny-AES-c 加密，实现基于 VFS 的内存挂载机制。
    *   **多实例解耦**：Runtime 主循环切换到实例化 FramePipeline 路径，EngineInstance 可独立承载世界与渲染管线。
*   **Lua 脚本增强**：
    *   **热重载机制**：基于 `OnSerializeState`/`OnDeserializeState` 与文件监控实现运行时替换。
    *   **内存检测**：接管 `lua_newstate` 分配器，暴露 `get_memory_usage_kb` API。
    *   **业务状态机**：提供 `dse.fsm` 轻量级表驱动状态机框架。
- **编辑器增强**：完善左侧 FileSystem（资源 UUID 管理）、实时材质热更预览、性能剖析器 (Profiler) 及打包流水线（一键导出）。
- **启动器 (Launcher) 落地**：接入账号体系、实现 Chunk-based 引擎热更新与云端同步模板。

#### 阶段 2 的强制收口项（建议作为版本门禁）

- **2D 自动化回归补齐**
  - UI：命中检测、遮罩阻挡、层级事件穿透、多分辨率布局
  - Tilemap：大地图分块更新、动态刷块、碰撞同步
  - Spine：资源缺失、动画切换、循环/非循环事件边界
  - Physics2D：刚体同步、销毁回收、脚本与物理写回顺序
  - Lua Runtime：热重载、异常恢复、脚本隔离与多实体并发更新
- **性能基线建立**
  - Sprite 批渲染压测
  - UI 大量控件场景压测
  - Tilemap 大地图压测
  - Lua Tick / 事件分发 / Physics Step 基准
- **编辑器闭环**
  - Scene 保存/恢复
  - Prefab 编辑/实例化/覆盖
  - Undo/Redo
  - Play In Editor 状态隔离
  - 资源 UUID / meta 文件一致性
- **工程化门禁**
  - `engine.unit` + `engine.lua_runtime` 进入 CI
  - 关键 2D 子系统引入 smoke/integration 标签
  - Debug/Release 双配置持续构建

### 阶段 3：3D MVP 版本
- **引擎扩展**：以可插拔形式引入 `MeshRendererComponent`、`Camera3D` 及基础 3D 渲染管线（前向渲染），接入 Jolt/PhysX 物理库进行 3D 碰撞验证。
- **资源解析**：引入 Assimp/cgltf 支持 GLTF/FBX 模型。
- **编辑器 3D 升级**：视口支持 3D 自由相机漫游、3D Gizmo 与射线拾取，开放基础 3D 材质参数。

#### 阶段 3 的约束原则

- 3D MVP 必须建立在 **2D 已稳定闭环** 的基础上，不与 2D 收口任务并行争抢主线资源。
- 3D 资源格式优先以 **glTF 2.0** 为主，FBX 作为导入兼容层，不作为运行时主格式。
- 3D MVP 仅覆盖：
  - MeshRenderer / Camera3D / Directional Light
  - 基础 PBR
  - 单场景前向或轻量延迟渲染
  - 基础阴影
  - Frustum Culling
  - 3D 视口 / Gizmo / Pick
- 暂不在 MVP 阶段追求大而全特性（GI、完整 Shader Graph、复杂地形生态、全套后处理）。

### 阶段 4：3D 商业与全栈版本
- **现代 3D 渲染管线**：实现 PBR、延迟渲染 (Deferred Rendering)、高级光影 (CSM, SSAO, GI) 及后处理堆栈 (Bloom, TAA)。
- **全栈混合渲染**：确保 2D/3D 在同一场景下的无缝混合渲染（2D UI 叠加于 3D，或 2D 精灵作为 3D 场景元素）。
- **复杂编辑器特性**：基于节点的 Visual Shader Graph、3D 地形与植被编辑、骨骼动画状态机编辑器。
- **Launcher 生态**：推出资产商城 (Marketplace)，支持 3D 引擎按需下载（模块化组件包）。

---

## 2. 商业化工具链发展细节

### 2.1 Nexus Launcher (启动器) 发展计划
采用 `React` + `TypeScript` + `Electron` 构建，提供企业级工程与生态体验。
- **身份与鉴权**：支持 OAuth2.0，校验云端 License，支持无网环境的离线授权模式。
- **更新与分发**：实现基于 Manifest 的增量更新，配置多区域 CDN 加速下载。
- **资产商城**：内置 Marketplace，支持购买插件与美术资源，并一键注入本地项目。
- **遥测与异常捕获**：集成 APM 收集崩溃日志与 Dump，用于优化迭代。

### 2.2 天工开物 Editor (编辑器) 发展计划
基于“国产修仙复古风格” UI 与 Node-API 后端，打磨 WYSIWYG（所见即所得）的编辑体验。
- **场景交互升级**：完善 Transform 的 Gizmo 拖拽、多选批量修改、撤销/重做 (Undo/Redo) 指令系统。
- **垂直编辑器**：新增 Timeline Editor（动画时间线）、Visual Shader Graph（节点连线材质）以及 Particle Editor（粒子发射器）。
- **状态隔离调试**：实现 Play In Editor 模式，运行时与静态场景数据隔离，停止后自动恢复场景，防止污染。
- **跨端打包发布**：提供目标平台（Windows, macOS, Linux, WASM 等）的详细设置面板，自动剔除无用资源并进行纹理压缩。
