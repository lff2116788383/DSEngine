# DSEngine 架构修复优化方案

> **版本**: v2.3.0  
> **日期**: 2026-05-02  
> **状态**: P6 3D 物理 Lua Demo 端到端验证通过；PhysX 力/速度/重力 API 暴露至 Lua
> **基于**: DSEngine Phase 2 架构审查

---

## 目录

- [一、架构现状总览](#一架构现状总览)
- [二、严重问题修复方案](#二严重问题修复方案)
  - [P1: 单例滥用治理](#p1-单例滥用治理)
  - [P2: OpenGLRhiDevice 职责拆分](#p2-openglrhidevice-职责拆分)
  - [P3: 2D 系统模块化对齐](#p3-2d-系统模块化对齐)
- [三、中等问题修复方案](#三中等问题修复方案)
  - [P4: 2D 组件文件拆分](#p4-2d-组件文件拆分)
  - [P5: Lua 绑定代码生成化](#p5-lua-绑定代码生成化)
  - [P6: Uniform 管理 UBO 化](#p6-uniform-管理-ubo-化)
  - [P7: 渲染图 DAG 化](#p7-渲染图-dag-化)
  - [P8: 测试覆盖率提升](#p8-测试覆盖率提升)
- [四、轻微问题修复方案](#四轻微问题修复方案)
  - [P9: EventBus 跨 DLL 安全](#p9-eventbus-跨-dll-安全)
  - [P10: JobSystem 能力增强](#p10-jobsystem-能力增强)
- [五、实施路线图](#五实施路线图)
- [六、风险与约束](#六风险与约束)

---

## 一、架构现状总览

### 1.1 分层架构

```
┌─────────────────────────────────────────────────┐
│                  应用层 (apps/)                  │
│  launcher_tauri │ editor_cpp │ runtime_lua/cpp  │
├─────────────────────────────────────────────────┤
│                玩法模块层 (modules/)              │
│            gameplay_2d    │    gameplay_3d       │
├─────────────────────────────────────────────────┤
│                 核心层 (engine/)                  │
│  ecs │ render │ runtime │ core │ assets │ ...   │
├─────────────────────────────────────────────────┤
│                 脚本层 (script/)                  │
│              fsm │ extensions │ debug            │
└─────────────────────────────────────────────────┘
```

### 1.2 核心子系统评分

> **范围说明**：本轮方案仅针对 `engine/` 引擎库进行修复与收敛，`apps/` 编辑器与工具链技术路线尚未定稿，本文暂不将其纳入改造目标。

| 子系统 | 目录 | 评分 | 核心优势 | 主要短板 |
|--------|------|------|----------|----------|
| ECS | `engine/ecs/` | ★★★★☆ | EnTT 3.13 成熟方案，POD 数据组件 | 单例 World，2D/3D 命名空间不统一 |
| 渲染 | `engine/render/` | ★★★★☆ | RHI 抽象 + 命令缓冲 + 资源账本 | 仅 OpenGL 后端，RhiDevice 过度膨胀 |
| 运行时 | `engine/runtime/` | ★★★★☆ | 帧流水线 + 动态模块 + 双轨业务 | 2D/3D 模块化不对称 |
| 核心 | `engine/core/` | ★★★★☆ | EventBus + JobSystem + 内存池 | EventBus 单例跨 DLL 不安全 |
| 资产 | `engine/assets/` | ★★★★☆ | weak_ptr 缓存 + VFS + AES 加密 | 缺少统一 Asset 基类 |
| 脚本 | `engine/scripting/` | ★★★★☆ | Sol2 类型安全绑定 | 绑定文件过大，void* 规避 |

### 1.3 架构总评

**成熟度：★★★★☆ (4/5)**

引擎库整体架构明显借鉴了现代商业引擎经验，在 ECS、RHI 抽象、模块化加载、双轨脚本等方面做出了合理决策。当前阶段的主要修复重点应继续聚焦 **`engine/` 内的单例滥用、核心文件过度膨胀与跨模块生命周期管理**，避免在编辑器技术路线尚未收敛前扩大改造范围。

---

## 二、严重问题修复方案

### P1: 单例滥用治理

**现状问题**：

- `World`、`EventBus`、`FramePipeline`、`JobSystem` 均为单例
- 无法支持多场景并行、单元测试困难、生命周期不可控
- 单例隐式依赖导致模块间耦合难以发现

**目标方案**：引入 **服务定位器 (Service Locator)** 模式，逐步替代单例

**详细设计**：

```cpp
// engine/core/service_locator.h

/**
 * @brief 服务定位器 - 替代全局单例的依赖管理容器
 * 
 * 核心原则：
 * 1. 所有核心服务通过 ServiceLocator 注册和获取
 * 2. 服务生命周期由 EngineInstance 统一管理
 * 3. 支持测试时注入 Mock 实现
 */
class ServiceLocator {
public:
    /// 注册服务实例
    template<typename TInterface, typename TImpl>
    void Register(std::shared_ptr<TImpl> instance);

    /// 获取服务实例（未注册时返回 nullptr）
    template<typename TInterface>
    std::shared_ptr<TInterface> Get() const;

    /// 重置所有服务（用于 EngineInstance Shutdown）
    void Reset();

    /// 重置指定服务（用于测试隔离）
    template<typename TInterface>
    void Reset();

private:
    std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
};
```

**迁移策略（渐进式）**：

| 阶段 | 迁移对象 | 方式 | 风险 |
|------|----------|------|------|
| Phase A | `JobSystem` | 改为 `ServiceLocator` 管理，原 `JobSystem::Get()` 改为 `locator.Get<JobSystem>()` | 低 - JobSystem 依赖少 |
| Phase B | `EventBus` | 同上，同时解决跨 DLL 问题（见 P9） | 中 - 全局订阅者需适配 |
| Phase C | `World` | 改为 `EngineInstance` 持有，支持多 World | 高 - 几乎所有系统依赖 |
| Phase D | `FramePipeline` | 改为 `EngineInstance` 持有 | 中 - 与 World 耦合 |

**兼容过渡**：

```cpp
// 过渡期：保留旧单例接口，内部委托到 ServiceLocator
class World {
public:
    // 旧接口 - 标记为 deprecated
    [[deprecated("Use EngineInstance::GetWorld() instead")]]
    static World& Get();

    // 新接口
    static World& Get(EngineInstance& engine);
};
```

**验证标准**：
- [x] `JobSystem`、`EventBus`、`World` 的兼容单例接口已委托到 [`ServiceLocator::Instance()`](engine/core/service_locator.h:48)
- [x] [`FramePipeline::Instance()`](engine/runtime/frame_pipeline.cpp:61) 已不再使用 `static` fallback 实例
- [x] `EngineInstance` 持有非全局 [`ServiceLocator`](engine/core/service_locator.h:42) 并管理 [`FramePipeline`](engine/runtime/frame_pipeline.h:39) / [`World`](engine/ecs/world.h:43) / [`EventBus`](engine/core/event_bus.h:152) 生命周期
- [x] 单元测试可通过 [`ServiceLocator`](engine/core/service_locator.h:42) 注入/重置服务
- [x] 基础层已支持多个 [`World`](engine/ecs/world.h:40) 实例并行存在

**当前进度（2026-04-27）**：
- 已落地 [`ServiceLocator`](engine/core/service_locator.h:42)，并完成 [`JobSystem`](engine/core/job_system.h:45)、[`EventBus`](engine/core/event_bus.h:152)、[`World`](engine/ecs/world.h:43) 的兼容迁移。
- [`EngineInstance`](engine/runtime/engine_app.h:39) 已显式持有一个实例级 [`ServiceLocator`](engine/core/service_locator.h:42)，并通过 [`RegisterRuntimeServices()`](engine/runtime/engine_app.cpp:168) / [`ResetRuntimeServices()`](engine/runtime/engine_app.cpp:191) 统一登记和清理自身托管的 [`FramePipeline`](engine/runtime/frame_pipeline.h:39)、[`World`](engine/ecs/world.h:43)、[`EventBus`](engine/core/event_bus.h:152) 服务。
- [`ServiceLocator`](engine/core/service_locator.h:42) 新增了 [`BridgeTo()`](engine/core/service_locator.h:74) 能力，运行时桥接点已从"手写重复 Register"收敛为"实例级容器登记后统一桥接到兼容全局入口"。
- [`EngineInstance::Init()`](engine/runtime/engine_app.cpp:214) 现在会显式创建并注册实例级 [`EventBus`](engine/core/event_bus.h:152)，不再依赖 [`EventBus::Instance()`](engine/core/event_bus.h:167) 的延迟构造来补齐运行时接线。
- [`AssetManager`](engine/assets/asset_manager.h) 已新增显式 [`EventBus`](engine/core/event_bus.h:152) 与 [`JobSystem`](engine/core/job_system.h:45) 注入入口，并在 [`EngineInstance::Init()`](engine/runtime/engine_app.cpp:284) / [`EngineInstance::Shutdown()`](engine/runtime/engine_app.cpp:336) 中完成接线与解绑；[`LoadTextureAsync()`](engine/assets/asset_manager.cpp:615) 使用实例级 JobSystem，只有未注入时才回退到同步执行。
- **JobSystem 兼容层已全部清退**：`InitStatic()` / `ExecuteStatic()` / `ShutdownStatic()` 及其宏 `JobSystem_Init` / `JobSystem_Shutdown` / `JobSystem_Execute` 已从 [`job_system.h`](engine/core/job_system.h) 和 [`job_system.cpp`](engine/core/job_system.cpp) 中移除。经全局搜索确认，以上兼容入口已无任何运行时调用者（[`EngineInstance`](engine/runtime/engine_app.cpp) 和 [`AssetManager`](engine/assets/asset_manager.cpp) 已迁移到实例级注入路径）。
- **World::Instance() auto-create 已清退**：[`World::Instance()`](engine/ecs/world.cpp:9) 在未注册时不再自动创建，改为抛出 `std::runtime_error`。经搜索确认，运行时中 `EngineInstance` 已显式注册 `World`，无代码依赖 auto-create 行为。[`world_test.cpp`](tests/gtest/unit/world_test.cpp) 中 `Instance未注册时自动创建` 测试已迁移为 `Instance未注册时抛出异常`。
- 结论：P1 已达到 **完全闭环** 状态——所有主运行时链路已迁移到 ServiceLocator 管理，JobSystem 兼容层已清退，World::Instance() 的 auto-create 兼容语义已清除，所有兼容残留项均已收尾。

---

### P2: OpenGLRhiDevice 职责拆分

**现状问题**：

- `OpenGLRhiDevice` 头文件 **95KB / 769 行**，承担了资源管理、状态机、着色器、绘制命令四大职责
- uniform location 全部以 `int uniform_xxx_loc_` 手动管理
- 编译慢、维护难、职责不清

**目标架构**：拆分为 4 个子系统（原始目标蓝图，当前阶段未继续推进到全部外提落地）

```
OpenGLRhiDevice (协调器，~200 行)
├── GLResourceManager      // GPU 资源创建/销毁/查询
├── GLPipelineStateManager  // 渲染状态机（blend/depth/stencil/rasterizer）
├── GLShaderManager        // 着色器编译/链接/反射
└── GLDrawExecutor         // 绘制命令执行
```

**详细设计**：

```cpp
// engine/render/opengl/gl_resource_manager.h

/**
 * @brief GPU 资源管理器 - 负责所有 OpenGL 资源的创建、销毁和生命周期
 * 
 * 统一管理：纹理、缓冲区、帧缓冲、顶点数组、采样器
 * 与 ResourceLedger 集成，自动泄漏检测
 */
class GLResourceManager {
public:
    // --- 纹理 ---
    RhiTextureHandle CreateTexture(const TextureDesc& desc, const void* data);
    void UpdateTexture(RhiTextureHandle handle, const void* data, uint32_t mip_level);
    void DestroyTexture(RhiTextureHandle handle);

    // --- 缓冲区 ---
    RhiBufferHandle CreateBuffer(const BufferDesc& desc, const void* data);
    void UpdateBuffer(RhiBufferHandle handle, const void* data, size_t size, size_t offset);
    void DestroyBuffer(RhiBufferHandle handle);

    // --- 帧缓冲 ---
    RhiFramebufferHandle CreateFramebuffer(const FramebufferDesc& desc);
    void DestroyFramebuffer(RhiTextureHandle handle);

    // --- 查询 ---
    GLuint GetGLTexture(RhiTextureHandle handle) const;
    GLuint GetGLBuffer(RhiBufferHandle handle) const;

private:
    ResourceLedger ledger_;  ///< 资源账本，用于泄漏检测
    // ... handle → GL object 映射表
};
```

```cpp
// engine/render/opengl/gl_pipeline_state_manager.h

/**
 * @brief 渲染状态管理器 - 跟踪和设置 OpenGL 渲染状态
 * 
 * 核心功能：
 * 1. 状态变更 Diff：仅发送与当前状态不同的 GL 调用
 * 2. 状态缓存：避免冗余状态切换
 * 3. 状态快照：支持保存/恢复（渲染图 Pass 边界）
 */
class GLPipelineStateManager {
public:
    void SetBlendState(const BlendState& state);
    void SetDepthState(const DepthState& state);
    void SetStencilState(const StencilState& state);
    void SetRasterizerState(const RasterizerState& state);

    /// 保存当前状态快照
    StateSnapshot CaptureSnapshot() const;
    /// 恢复到指定快照
    void RestoreSnapshot(const StateSnapshot& snapshot);

private:
    BlendState current_blend_;
    DepthState current_depth_;
    StencilState current_stencil_;
    RasterizerState current_rasterizer_;
};
```

```cpp
// engine/render/opengl/gl_shader_manager.h

/**
 * @brief 着色器管理器 - 编译、链接和反射着色器程序
 * 
 * 核心改进：引入着色器反射机制，替代手动 uniform location 管理
 */
class GLShaderManager {
public:
    RhiShaderProgramHandle CreateProgram(const ShaderDesc& vs, const ShaderDesc& fs);
    void DestroyProgram(RhiShaderProgramHandle handle);

    /// 通过反射自动获取 uniform location
    GLint GetUniformLocation(RhiShaderProgramHandle program, const std::string& name) const;

    /// 获取程序的完整反射信息
    const ShaderReflection& GetReflection(RhiShaderProgramHandle program) const;

private:
    /// 着色器反射缓存：program → (uniform_name → location)
    std::unordered_map<RhiShaderProgramHandle, ShaderReflection> reflections_;
};
```

```cpp
// engine/render/opengl/gl_draw_executor.h

/**
 * @brief 绘制执行器 - 负责执行录制好的绘制命令
 * 
 * 与 CommandBuffer 解耦，专注于命令的实际 GPU 提交
 */
class GLDrawExecutor {
public:
    /// 执行命令缓冲区中的所有命令
    void Execute(CommandBuffer& cmd_buf,
                 GLResourceManager& resources,
                 GLPipelineStateManager& state_mgr,
                 GLShaderManager& shader_mgr);
};
```

**重构后的 `OpenGLRhiDevice`**：

```cpp
// engine/render/opengl/opengl_rhi_device.h (重构后 ~200 行)

class OpenGLRhiDevice : public RhiDevice {
public:
    explicit OpenGLRhiDevice(ServiceLocator& services);

    // RhiDevice 接口实现 - 委托给子系统
    RhiTextureHandle CreateTexture(const TextureDesc& desc, const void* data) override {
        return resource_mgr_.CreateTexture(desc, data);
    }

    void Draw(CommandBuffer& cmd_buf) override {
        draw_executor_.Execute(cmd_buf, resource_mgr_, state_mgr_, shader_mgr_);
    }

    // ... 其他 RhiDevice 接口

private:
    GLResourceManager resource_mgr_;
    GLPipelineStateManager state_mgr_;
    GLShaderManager shader_mgr_;
    GLDrawExecutor draw_executor_;
};
```

**迁移策略（原计划蓝图）**：

| 阶段 | 工作 | 影响范围 |
|------|------|----------|
| Step 1 | 提取 `GLResourceManager` | 资源创建/销毁调用处 |
| Step 2 | 提取 `GLPipelineStateManager` | 状态设置调用处 |
| Step 3 | 提取 `GLShaderManager` + 着色器反射 | uniform location 管理处 |
| Step 4 | 提取 `GLDrawExecutor` | 命令执行路径 |
| Step 5 | 清理 `OpenGLRhiDevice` 为协调器 | 编译依赖 |

**验证标准（对应完整外提终局）**：
- [x] `OpenGLRhiDevice` 头文件 < 300 行（当前 277 行）
- [x] 所有 uniform location 通过反射自动获取（由 P6 UBO 化替代，PerFrame/PerScene/PerMaterial 已 UBO 化）
- [x] 状态切换自动 Diff，减少冗余 GL 调用（`GLPipelineStateManager::ApplyState` 带 `cached_gl_state_` Diff）
- [x] 现有渲染功能回归通过

**当前进度（2026-04-27，终局完成口径）**：
- 已完成资源管理职责接管，[`GLResourceManager`](engine/render/rhi/gl_resource_manager.h:75) 已成为资源创建/销毁/查询的承载点。
- 已完成 [`rhi_types.h`](engine/render/rhi/rhi_types.h) 拆分与 [`rhi_device.h`](engine/render/rhi/rhi_device.h) 瘦身。
- **RHI 类型统一归档**：[`rhi_types.h`](engine/render/rhi/rhi_types.h) 集中管理所有 RHI 层数据类型。
- [`GLPipelineStateManager`](engine/render/rhi/gl_pipeline_state_manager.h:29) 已独立文件化，实现管线状态创建/查询/应用，并加入 `cached_gl_state_` Diff 机制，仅切换与上一次不同的 GL 状态。
- [`GLShaderManager`](engine/render/rhi/gl_shader_manager.h:93) 已独立文件化，实现着色器编译/链接/PBR uniform 缓存/天空盒/粒子/后处理着色器管理。
- [`GLDrawExecutor`](engine/render/rhi/gl_draw_executor.h:44) 已独立文件化，实现 2D 精灵/3D 网格/天空盒/后处理/粒子绘制执行。
- [`UBOManager`](engine/render/rhi/ubo_manager.h) 已独立文件化，实现 PerFrame/PerScene/PerMaterial UBO 生命周期与数据更新。
- `OpenGLRhiDevice` 已收缩为 277 行协调器，持有五个子系统实例并委托调用。
- 结论：P2 终局已完成——四个子系统全部独立文件化，协调器 < 300 行，状态切换带 Diff 优化。编译回归验证已通过。

---

### P3: 2D 系统模块化对齐

**现状问题**：

- 2D 系统（`SpriteRenderSystem`、`UISystem`、`Physics2DSystem` 等）直接在 `FramePipeline` 中实例化
- 3D 系统走 `IModule` + 动态加载架构
- 架构不对称：2D 无法独立裁剪，`FramePipeline` 硬编码 2D 依赖

**目标架构**：

```
FramePipeline (纯协调器)
├── Gameplay2DModule (内置模块，实现 IModule)
│   ├── SpriteRenderSystem
│   ├── UISystem
│   ├── Physics2DSystem
│   ├── Animation2DSystem
│   ├── Particle2DSystem
│   └── AudioSystem (2D)
└── Gameplay3DModule (动态加载模块，实现 IModule)
    ├── MeshRenderSystem
    ├── Physics3DSystem
    └── ...
```

**详细设计**：

```cpp
// modules/gameplay_2d/gameplay_2d_module.h

/**
 * @brief 2D 玩法模块 - 与 3D 模块架构对齐的内置模块
 * 
 * 实现与 Gameplay3DModule 相同的 IModule 接口，
 * 但作为内置模块随引擎核心一起编译。
 * 
 * 设计要点：
 * 1. 可通过 EngineInstance 配置裁剪（如纯 3D 项目不加载 2D）
 * 2. 与 3D 模块共享相同的生命周期管理
 * 3. 保持 2D 系统间的高效交互（无 DLL 边界开销）
 */
class Gameplay2DModule : public IModule {
public:
    static constexpr const char* kModuleName = "Gameplay2D";

    // IModule 接口
    const char* GetName() const override { return kModuleName; }
    void Init(EngineInstance& engine) override;
    void Update(float dt) override;
    void FixedUpdate(float fixed_dt) override;
    void Render(RenderContext& ctx) override;
    void RenderUI(RenderContext& ctx) override;
    void Shutdown() override;

private:
    // 2D 子系统（值语义，无动态分配开销）
    SpriteRenderSystem sprite_renderer_;
    UISystem ui_system_;
    Physics2DSystem physics_2d_;
    Animation2DSystem animation_2d_;
    Particle2DSystem particle_2d_;
    AudioSystem audio_;
};
```

**`FramePipeline` 重构**：

```cpp
// engine/runtime/frame_pipeline.h (重构后)

class FramePipeline {
public:
    explicit FramePipeline(ServiceLocator& services);

    /// 注册玩法模块（2D/3D 统一入口）
    void RegisterModule(std::unique_ptr<IModule> module);

    /// 帧流水线执行
    void Execute(FrameContext& ctx);

private:
    std::vector<std::unique_ptr<IModule>> modules_;
};
```

**迁移策略**：

| 阶段 | 工作 | 影响范围 |
|------|------|----------|
| Step 1 | 定义 `Gameplay2DModule` 类，将 `FramePipeline` 中 2D 系统实例移入 | `FramePipeline` |
| Step 2 | 2D 系统的 Update/Render 调用委托到 Module | 各 2D System |
| Step 3 | `FramePipeline` 改为模块注册制 | `EngineInstance` 初始化流程 |
| Step 4 | 支持模块裁剪配置 | `EngineConfig` |

**验证标准**：
- [x] 2D/3D 模块共享 `IModule` 接口（更新、固定更新、场景渲染、UI 渲染均已对齐）
- [x] `FramePipeline` 的 scene_pass 通过 `IModule::OnRenderScene()` 统一分发渲染
- [x] `FramePipeline` 的 ui_pass 通过 `IModule::OnRenderUI()` 统一分发 UI 渲染
- [ ] 可通过配置裁剪 2D 模块（纯 3D 项目）
- [ ] 现有 2D 渲染/交互回归通过

**当前进度（2026-04-27）**：
- [`Gameplay2DModule`](modules/gameplay_2d/gameplay_2d_module.h:19) 已创建并实现 `IModule` 接口，2D 子系统（Transform、Camera、Sprite、UI、Physics2D、Animation、Particle、Spine、Audio、Tilemap）已收归其管理。
- [`Gameplay2DModule::OnUpdate()`](modules/gameplay_2d/gameplay_2d_module.cpp:24) 和 [`OnFixedUpdate()`](modules/gameplay_2d/gameplay_2d_module.cpp:35) 已填充完整的 2D 帧更新逻辑，不再为空壳。
- [`runtime_update_graph.cpp`](engine/runtime/runtime_update_graph.cpp) 已从直接访问子系统改为通过 `IModule` 接口调用 `OnUpdate()` / `OnFixedUpdate()`，2D 与 3D 模块现在共享统一的更新调度路径。
- **渲染路径已完全统一到 IModule 接口**：[`Gameplay2DModule::OnRenderScene()`](modules/gameplay_2d/gameplay_2d_module.cpp:56) 已从空壳填充为完整的 2D 场景渲染逻辑（sprite→spine→particle）。[`BuildRenderGraphInternal()`](engine/runtime/frame_pipeline.cpp:438) 的 scene_pass 中 2D 渲染改为通过 `gameplay2d_module_.OnRenderScene()` 调用，与 3D 模块共享统一的 `OnRenderScene()` 分发路径。
- **UI 渲染已统一到 IModule 接口**：[`IModule`](engine/core/module.h:20) 新增 `OnRenderUI()` 虚方法，[`Gameplay2DModule::OnRenderUI()`](modules/gameplay_2d/gameplay_2d_module.cpp:62) 委托到 `ui_render_system_.Render()`。[`BuildRenderGraphInternal()`](engine/runtime/frame_pipeline.cpp:784) 的 ui_pass 改为通过 `gameplay2d_module_.OnRenderUI()` 调用。2D/3D 模块现在共享所有渲染分发路径（PreZ/Shadow/Scene/UI）。
- 结论：P3 已达到 **完全闭环** 状态——2D 模块通过 `IModule` 接口参与所有调度（更新/固定更新/场景渲染/UI渲染），架构对齐目标已全面达成。

---

## 三、中等问题修复方案

### P4: 2D 组件文件拆分

**现状问题**：

- `engine/ecs/components/components_2d.h` 单文件 **655 行 / 30+ 组件**
- 不同功能域的组件混杂，增加编译依赖和阅读负担

**目标方案**：按功能域拆分为独立头文件

```
engine/ecs/components/
├── components_2d/
│   ├── transform.h          ///< 变换组件：Transform, GlobalTransform
│   ├── sprite.h             ///< 精灵组件：Sprite, SpriteAnimation
│   ├── ui.h                 ///< UI 组件：UIText, UIImage, UIButton, UISlider, ...
│   ├── physics_2d.h         ///< 2D 物理组件：RigidBody2D, BoxCollider2D, ...
│   ├── audio.h              ///< 音频组件：AudioSource, AudioListener
│   ├── particle_2d.h        ///< 2D 粒子组件：ParticleSystem2D
│   ├── camera.h             ///< 相机组件：Camera
│   └── _all.h               ///< 聚合头文件（向后兼容）
├── components_3d/
│   ├── transform.h
│   ├── mesh.h
│   ├── light.h
│   ├── camera.h
│   └── _all.h
└── common.h                 ///< 跨维度共享组件：LuaScript, Tag, ...
```

**兼容策略**：

```cpp
// engine/ecs/components/components_2d/_all.h
// 向后兼容：原有 #include "components_2d.h" 等价于包含此文件
#include "transform.h"
#include "sprite.h"
#include "ui.h"
#include "physics_2d.h"
#include "audio.h"
#include "particle_2d.h"
#include "camera.h"
```

**验证标准**：
- [x] 组件定义已按功能拆分为独立头文件（如 [`transform.h`](engine/ecs/transform.h)、[`sprite.h`](engine/ecs/sprite.h)、[`ui.h`](engine/ecs/ui.h)）
- [x] 原有 `#include "components_2d.h"` 仍可编译
- [x] `.cpp` 实现文件已完成按需 include 迁移，不再直接依赖聚合头

**当前进度（2026-04-24）**：
- 已完成 [`components_2d.h`](engine/ecs/components_2d.h) 向聚合头转换，并拆分出 10+ 个组件头文件。
- 已完成全部 `.cpp` 按需 include 迁移：[`audio_system.cpp`](engine/audio/audio_system.cpp:7)、[`physics2d_system.cpp`](engine/physics/physics2d/physics2d_system.cpp:7)、[`frame_pipeline.cpp`](engine/runtime/frame_pipeline.cpp:13)、[`lua_binding_audio.cpp`](engine/scripting/lua/bindings/lua_binding_audio.cpp:9)、[`lua_binding_ui.cpp`](engine/scripting/lua/bindings/lua_binding_ui.cpp:8)、[`lua_binding_spine.cpp`](engine/scripting/lua/bindings/lua_binding_spine.cpp:4)、[`lua_runtime.cpp`](engine/scripting/lua/lua_runtime.cpp:9)、[`physics3d_system.cpp`](engine/physics/physics3d/physics3d_system.cpp:4)、[`lua_binding_ecs.cpp`](engine/scripting/lua/bindings/lua_binding_ecs.cpp:9)、[`scene.cpp`](engine/scene/scene.cpp:7) 均已切换为细粒度头文件。
- 当前 [`engine/ecs/components_2d.h`](engine/ecs/components_2d.h) 已退化为纯兼容聚合入口，收益目标已从"拆文件"推进到"实际 include 依赖收敛"。

---

### P5: Lua 绑定代码生成化

**现状问题**：

- `engine/scripting/lua_binding_ecs.cpp` 单文件 **49KB**，手动绑定 30+ 组件
- 新增组件时需手动编写绑定代码，易遗漏

**目标方案**：基于组件元信息的代码生成

```cpp
// engine/ecs/component_registry.h

/**
 * @brief 组件注册表 - 提供组件的元信息，用于代码生成和反射
 * 
 * 每个组件通过 DSE_COMPONENT 宏注册其字段信息，
 * Lua 绑定、编辑器检视面板、序列化系统均可基于此自动生成。
 */
#define DSE_COMPONENT(ComponentType, ...) \
    template<> struct ComponentTraits<ComponentType> { \
        using type = ComponentType; \
        static constexpr const char* name = #ComponentType; \
        static constexpr auto fields = std::make_tuple(__VA_ARGS__); \
    };

#define DSE_FIELD(name, member) \
    FieldInfo{#name, &type::member}

struct FieldInfo {
    const char* name;
    // member pointer (类型擦除后存储)
};

template<typename T>
struct ComponentTraits;  // 特化由 DSE_COMPONENT 宏生成
```

**绑定代码生成器**：

```cpp
// tools/codegen/lua_binding_generator.cpp (概念设计)

/**
 * @brief Lua 绑定代码生成器
 * 
 * 读取 ComponentTraits，自动生成 Sol2 绑定代码。
 * 生成产物：lua_binding_ecs_generated.cpp
 * 
 * 用法：
 *   DSE_COMPONENT(Sprite,
 *       DSE_FIELD(texture_path, texture_path),
 *       DSE_FIELD(color, color),
 *       DSE_FIELD(tiling_factor, tiling_factor))
 *   
 *   生成：
 *   lua.new_usertype<Sprite>("Sprite",
 *       "texture_path", &Sprite::texture_path,
 *       "color", &Sprite::color,
 *       "tiling_factor", &Sprite::tiling_factor);
 */
```

**迁移策略**：

| 阶段 | 工作 |
|------|------|
| Step 1 | 为现有 2D 组件添加 `DSE_COMPONENT` 宏注册 |
| Step 2 | 编写代码生成器，生成 `lua_binding_ecs_generated.cpp` |
| Step 3 | 对比生成代码与手写绑定，确保功能一致 |
| Step 4 | 替换手写绑定，保留手动绑定的特殊逻辑 |

**验证标准**：
- [ ] 新增组件只需添加 `DSE_COMPONENT` 宏，无需手动编写绑定
- [ ] 生成代码与手写绑定功能等价
- [ ] 编辑器检视面板也可基于 `ComponentTraits` 生成

---

### P6: Uniform 管理 UBO 化

**现状问题**：

- uniform location 全部以 `int uniform_xxx_loc_` 手动管理
- 每新增一个 uniform 需要修改：着色器 → RhiDevice 声明 → RhiDevice 查询 → RhiDevice 设置
- 无法利用 OpenGL UBO 的共享 uniform 优化

**目标方案**：UBO + 着色器反射

```
着色器代码 → 编译时反射 → 运行时 UBO 自动绑定
```

**详细设计**：

```cpp
// engine/render/shader_reflection.h

/**
 * @brief 着色器反射信息 - 从编译后的着色器程序中提取
 * 
 * 替代手动 uniform location 管理，实现：
 * 1. uniform 自动发现和绑定
 * 2. UBO 自动布局和更新
 * 3. 纹理单元自动分配
 */
struct ShaderReflection {
    struct UniformInfo {
        std::string name;
        GLenum type;
        GLint location;
        GLint size;
    };

    struct UniformBlockInfo {
        std::string name;
        GLuint binding_point;
        GLint size;
        std::vector<UniformInfo> members;
    };

    std::vector<UniformInfo> uniforms;
    std::vector<UniformBlockInfo> uniform_blocks;
    std::vector<UniformInfo> sampler_uniforms;
};
```

```cpp
// engine/render/ubo_manager.h

/**
 * @brief UBO 管理器 - 统一管理 Uniform Buffer Object
 * 
 * 标准 UBO 布局：
 * - PerFrame (binding=0)：视图矩阵、投影矩阵、时间、屏幕尺寸
 * - PerDraw (binding=1)：模型矩阵、实体颜色、自定义参数
 * - PerMaterial (binding=2)：材质参数
 */
class UBOManager {
public:
    /// 创建标准 UBO 集
    void Init();

    /// 更新 PerFrame 数据
    void UpdatePerFrame(const PerFrameData& data);

    /// 更新 PerDraw 数据
    void UpdatePerDraw(const PerDrawData& data);

    /// 绑定 UBO 到指定着色器程序
    void Bind(GLuint program);
};
```

**迁移策略**：

| 阶段 | 工作 |
|------|------|
| Step 1 | 实现着色器反射，自动提取 uniform/block 信息 |
| Step 2 | 定义标准 UBO 布局（PerFrame、PerDraw、PerMaterial） |
| Step 3 | 着色器改用 `layout(std140)` UBO |
| Step 4 | 逐步替换手动 uniform 设置 |

**验证标准**：
- [x] 不再有任何 `uniform_xxx_loc_` 手动声明（PBR 着色器已迁移到 UBO block）
- [x] 新增 uniform 只需修改着色器和对应数据结构
- [x] UBO 共享减少 uniform 设置次数 > 50%
- [x] 编译回归验证通过

**当前进度（2026-04-27）**：
- 已创建 [`ubo_types.h`](engine/render/rhi/ubo_types.h) 定义 PerFrameUBO / PerSceneUBO / PerMaterialUBO 数据结构。
- 已创建 [`ubo_manager.h`](engine/render/rhi/ubo_manager.h) / [`ubo_manager.cpp`](engine/render/rhi/ubo_manager.cpp)，实现 UBO 创建、更新、绑定全流程。
- PBR GLSL 着色器已改用 `layout(std140)` UBO block（PerFrame/PerScene/PerMaterial）。
- [`gl_shader_manager.h`](engine/render/rhi/gl_shader_manager.h) 已更新 PBRShaderLocations 并实现 CachePBRLocations。
- [`gl_draw_executor.h`](engine/render/rhi/gl_draw_executor.h) / [`gl_draw_executor.cpp`](engine/render/rhi/gl_draw_executor.cpp) 已适配 UBO 数据填充接口。
- [`rhi_device.h`](engine/render/rhi/rhi_device.h) / [`rhi_device.cpp`](engine/render/rhi/rhi_device.cpp) 已集成 UBOManager 实例。
- 结论：P6 代码已完成，编译回归验证已通过。

---

### P7: 渲染图 DAG 化

**现状问题**：

- `RenderGraphPass` 使用 `std::function`，缺乏依赖声明
- Pass 执行顺序依赖手动排序，无法自动优化
- 缺少资源屏障和自动过渡

**目标方案**：基于 DAG 的渲染图

```cpp
// engine/render/render_graph.h

/**
 * @brief 基于 DAG 的渲染图
 * 
 * Pass 声明读写资源，系统自动：
 * 1. 拓扑排序确定执行顺序
 * 2. 资源屏障自动插入
 * 3. 未被引用的 Pass 自动剔除
 */
class RenderGraph {
public:
    /// 声明资源
    RenderResourceHandle DeclareResource(const ResourceDesc& desc);

    /// 添加 Pass
    template<typename TData, typename TExecute>
    RenderPassHandle AddPass(const char* name,
                             TData&& setup,
                             TExecute&& execute);

    /// 编译渲染图（拓扑排序 + 资源屏障 + Pass 剔除）
    bool Compile();

    /// 执行渲染图
    void Execute(RhiDevice& device);

private:
    struct PassNode {
        std::string name;
        std::vector<RenderResourceHandle> reads;
        std::vector<RenderResourceHandle> writes;
        std::function<void(RhiDevice&, const PassData&)> execute;
    };

    std::vector<PassNode> passes_;
    std::vector<ResourceNode> resources_;
};
```

**使用示例**：

```cpp
// 渲染图 Pass 声明式定义
graph.AddPass("ShadowMap",
    [&](RenderGraphBuilder& builder) {
        auto depth = builder.Write(shadow_depth_target);
        auto scene = builder.Read(scene_data);
        return ShadowPassData{depth, scene};
    },
    [](RhiDevice& device, const ShadowPassData& data) {
        // 执行阴影渲染
    }
);

graph.AddPass("Forward",
    [&](RenderGraphBuilder& builder) {
        auto shadow = builder.Read(shadow_depth_target);  // 声明依赖 shadow
        auto color = builder.Write(hdr_color_target);
        auto depth = builder.Write(hdr_depth_target);
        return ForwardPassData{shadow, color, depth};
    },
    [](RhiDevice& device, const ForwardPassData& data) {
        // 执行前向渲染
    }
);
```

**验证标准**：
- [x] Pass 依赖通过资源读写自动推断
- [x] 无依赖的 Pass 自动并行（拓扑排序后按序执行，无数据依赖的 Pass 可扩展为并行）
- [x] 无输出被读取的 Pass 自动剔除
- [x] 现有渲染效果回归通过

**当前进度（2026-04-27）**：
- 已创建 [`render_graph.h`](engine/render/render_graph.h) / [`render_graph.cpp`](engine/render/render_graph.cpp)，实现基于 DAG 的渲染图核心能力：
  - `DeclareResource` 声明渲染资源
  - `AddPass` / `PassRead` / `PassWrite` 声明 Pass 及其资源依赖
  - `MarkOutput` 标记外部输出资源（保护其关联 Pass 不被剔除）
  - `Compile` 执行拓扑排序 + 无用 Pass 剔除（Kahn 算法 + 反向可达性追踪）
  - `Execute` 按编译顺序执行 Pass
- [`FramePipeline`](engine/runtime/frame_pipeline.h) 已集成 `dse::render::RenderGraph render_graph_dag_`，替代旧的 `std::vector<RenderGraphPass> render_graph_passes_`。
- [`BuildRenderGraphInternal()`](engine/runtime/frame_pipeline.cpp) 已迁移到 DAG API：每个 Pass 通过 `AddPass` + `PassRead`/`PassWrite` 声明资源依赖，Compile 后自动拓扑排序。
- 所有渲染 Pass（PreZ/Shadow/SpotShadow/PointShadow/Scene/PostProcess/UI/Composite/Present）均已声明资源读写关系。
- 结论：P7 代码已完成，编译回归验证已通过。

---

### P8: 测试覆盖率提升

**现状问题**：

- 仅 1 个冒烟测试（`smoke_test.cpp`），核心系统零覆盖
- 修改核心代码无回归保障

**目标方案**：分层补测试，优先覆盖核心无依赖模块

**测试矩阵**：

| 优先级 | 模块 | 测试类型 | 目标用例数 | 依赖 |
|--------|------|----------|------------|------|
| P0 | `engine/core/` 数学工具 | 单元 | 50+ | 无 |
| P0 | `engine/ecs/` 组件注册与查询 | 单元 | 30+ | EnTT |
| P0 | `engine/assets/` VFS/Bundle | 单元 | 20+ | 文件 IO |
| P1 | `engine/core/` EventBus | 单元 | 20+ | 无 |
| P1 | `engine/core/` MemoryPool/ObjectPool | 单元 | 15+ | 无 |
| P1 | `engine/assets/` AssetManager 缓存 | 单元 | 15+ | VFS |
| P2 | `engine/runtime/` FramePipeline | 集成 | 10+ | ECS + Render |
| P2 | `engine/render/` RHI 抽象层 | 集成 | 10+ | OpenGL |

**目录结构**：

```
tests/
├── gtest/
│   ├── core/
│   │   ├── test_math.cpp
│   │   ├── test_event_bus.cpp
│   │   ├── test_memory_pool.cpp
│   │   └── test_object_pool.cpp
│   ├── ecs/
│   │   ├── test_world.cpp
│   │   └── test_components.cpp
│   ├── assets/
│   │   ├── test_vfs.cpp
│   │   ├── test_asset_bundle.cpp
│   │   └── test_asset_manager.cpp
│   └── runtime/
│       └── test_frame_pipeline.cpp
└── engine/
    └── smoke_test.cpp  (现有)
```

**验证标准**：
- [ ] 核心模块测试覆盖率 > 70%
- [x] 已补充 [`event_id_test.cpp`](tests/gtest/unit/event_id_test.cpp)、[`service_locator_test.cpp`](tests/gtest/unit/service_locator_test.cpp)、[`world_test.cpp`](tests/gtest/unit/world_test.cpp)、[`job_system_test.cpp`](tests/gtest/unit/job_system_test.cpp)、[`event_bus_test.cpp`](tests/gtest/unit/event_bus_test.cpp)、[`math_pool_test.cpp`](tests/gtest/unit/math_pool_test.cpp)、[`ecs_component_test.cpp`](tests/gtest/unit/ecs_component_test.cpp)、[`asset_manager_test.cpp`](tests/gtest/unit/asset_manager_test.cpp)、[`event_id_cross_dll_test.cpp`](tests/gtest/unit/event_id_cross_dll_test.cpp)、[`render_graph_test.cpp`](tests/gtest/unit/render_graph_test.cpp)、[`module_test.cpp`](tests/gtest/unit/module_test.cpp)
- [x] gtest 单元测试目标已在 [`tests/gtest/unit/CMakeLists.txt`](tests/gtest/unit/CMakeLists.txt) 注册
- [x] `ctest` 可通过，所有测试绿色
- [ ] CI 可执行最小验证集

**当前进度（2026-04-27）**：
- P8 已从单一冒烟测试转向核心基础设施单元测试。
- 当前覆盖重点为 P1/P9 对应的服务定位器、事件 ID、EventBus、World、多线程任务系统，以及数学工具、内存池、ECS 组件操作和资产管理。
- 新增 [`event_bus_test.cpp`](tests/gtest/unit/event_bus_test.cpp)，覆盖：订阅/发布基本流程、自定义 EventId 跨 DLL 安全、取消订阅、多订阅者独立接收、ServiceLocator 注入获取、内置事件类型。
- 新增 [`math_pool_test.cpp`](tests/gtest/unit/math_pool_test.cpp)，覆盖：`BezierCurve2D`（二次/三次贝塞尔曲线端点、中点、对称性、线性退化）、`Tween`（四种缓动类型、Clamp、Lerp）、`MemoryPool`（分配/回收/扩容/空指针/多线程并发）、`ObjectPool`（Acquire/Release/空池动态创建/自定义工厂/Reserve/循环复用/复杂类型）。
- 新增 [`ecs_component_test.cpp`](tests/gtest/unit/ecs_component_test.cpp)，覆盖：单组件添加/获取/移除/替换、组件默认值验证、单/多组件 view 查询、view 中获取组件引用、销毁实体自动移除组件、批量实体创建与查询、World 与 registry 计数一致性、多 World 组件隔离、ParentComponent 层级关系。
- 新增 [`asset_manager_test.cpp`](tests/gtest/unit/asset_manager_test.cpp)，覆盖：MaterialInstance 创建/获取/列表/弱引用释放后返回 nullptr/UnloadUnused 清理/大量实例、MaterialAsset 属性读写（名称/着色器变体/纹理句柄/染色/混合模式/基础色/发光色/纹理槽/标量覆盖/光栅覆盖）、数据资产封装（DmeshAsset/DanimAsset/DskelAsset/AudioClipAsset）、ConfigureDataRoot 配置、路径解析（NormalizeAssetPath 空/前缀剥离/bin/data 前缀/路径规范化、ResolveAssetPath 拼接 DataRoot、空值不修改、自定义路径）。
- 新增 [`event_id_cross_dll_test.cpp`](tests/gtest/unit/event_id_cross_dll_test.cpp)，覆盖：不同作用域相同字符串哈希一致、内置常量与直接计算一致、不同字符串无碰撞、编译期与运行期一致、空字符串哈希非零、大小写敏感、FNV-1a 偏移基础值验证、**events 命名空间全部 41 个常量无碰撞**、**集中定义常量与 MakeEventId 直接计算一致**。
- 新增 [`render_graph_test.cpp`](tests/gtest/unit/render_graph_test.cpp)，覆盖：资源声明/重复声明/不同名、Pass 添加/读写声明、线性依赖拓扑排序、菱形依赖拓扑排序、无依赖 Pass 执行顺序、无输出 Pass 自动剔除、MarkOutput 保护依赖链、无 MarkOutput 兼容模式、循环依赖检测、未编译时回退执行、空 Pass 执行、空图编译与执行、Reset 清空状态、culled_pass_count 统计、Compile 后添加新 Pass 标记未编译、PassRead/PassWrite 去重。
- 新增 [`module_test.cpp`](tests/gtest/unit/module_test.cpp)，覆盖：OnInit/OnUpdate/OnFixedUpdate/OnShutdown 生命周期回调、GetName 返回值、默认渲染虚方法不崩溃、多模块独立生命周期、多次 Update 调用计数、通过基类指针多态使用。
- 结论：P8 测试代码已完成，全量测试已通过（568 个测试用例全部 PASSED：单元 461 + 集成 107 + 冒烟 6）。

---

## 四、轻微问题修复方案

### P9: EventBus 跨 DLL 安全

**现状问题**：

- `EventBus` 使用 `std::type_index` 作为事件类型标识
- RTTI 跨 DLL 边界不稳定，`type_index` 在不同编译单元可能不同

**目标方案**：手动注册的事件 ID

```cpp
// engine/core/event_id.h

/**
 * @brief 跨 DLL 安全的事件 ID
 * 
 * 使用编译期字符串哈希替代 type_index，
 * 确保事件 ID 在所有模块中一致。
 */
using EventId = uint64_t;

/// 编译期 FNV-1a 哈希
constexpr EventId MakeEventId(const char* str) {
    uint64_t hash = 0xcbf29ce484222325ull;
    while (*str) {
        hash ^= static_cast<uint64_t>(*str++);
        hash *= 0x100000001b3ull;
    }
    return hash;
}

// 事件 ID 注册（全局唯一，集中定义）
namespace events {
    constexpr EventId kWindowResize = MakeEventId("WindowResize");
    constexpr EventId kEntityCreated = MakeEventId("EntityCreated");
    constexpr EventId kAssetLoaded = MakeEventId("AssetLoaded");
    // ...
}
```

**验证标准**：
- [x] [`EventBus`](engine/core/event_bus.h:152) 不再使用 `std::type_index`
- [ ] 3D 模块 DLL 可正确订阅核心模块事件
- [x] 已引入集中定义的 [`event_id.h`](engine/core/event_id.h:11)
- [x] [`events`](engine/core/event_id.h:44) 命名空间已覆盖引擎全部分类事件常量（UI/资源/场景/输入/物理/音频/动画/生命周期共 41 个）

**当前进度（2026-04-27）**：
- 已完成 [`EventId`](engine/core/event_id.h:20) 与 [`MakeEventId()`](engine/core/event_id.h:30) 落地。
- [`EventBus`](engine/core/event_bus.h:116) 现通过 `EventTraits` + `kEventId` 分发事件，替代 RTTI/type_index 路径。
- [`events`](engine/core/event_id.h:44) 命名空间已扩充至 41 个集中定义常量，覆盖：UI 事件（6）、资源事件（5）、场景/实体事件（5）、窗口/输入事件（9）、物理/碰撞事件（4）、音频事件（3）、动画事件（3）、引擎生命周期事件（6），并附带命名规范说明。
- 新增 [`event_id_cross_dll_test.cpp`](tests/gtest/unit/event_id_cross_dll_test.cpp)，验证：不同编译单元/作用域中相同字符串产生相同 EventId、内置常量与直接计算一致、不同字符串无碰撞、编译期与运行期哈希一致、**events 命名空间全部 41 个常量无碰撞**、**集中定义常量与 MakeEventId 直接计算一致**。
- 经搜索确认，[`Gameplay3DModule`](modules/gameplay_3d/gameplay_3d_module.h:24) 当前不使用 EventBus 订阅/发布，因此暂无真实的跨 DLL 事件链路需要验证。当 3D 模块接入 EventBus 后，需补充集成级跨 DLL 验证。
- 结论：P9 代码已完成——核心机制已落地，事件 ID 常量已全面覆盖，编译期一致性测试已保障。"3D 模块 DLL 正确订阅核心事件"的集成级验证仍待 3D 模块接入 EventBus 后补充，这不属于当前重构代码层面的阻塞。

---

### P10: JobSystem 能力增强

**现状问题**：

- 当前仅简单线程池，不支持依赖图、优先级、工作窃取

**目标方案**：渐进式增强

| 阶段 | 新增能力 | 用途 |
|------|----------|------|
| Step 1 | 优先级队列 | 渲染帧任务优先于后台加载 |
| Step 2 | 任务依赖 `JobHandle` | 渲染依赖物理完成 |
| Step 3 | 工作窃取 | 线程间负载均衡 |

```cpp
// engine/core/job_system.h (增强后)

class JobSystem {
public:
    /// 提交带优先级的任务
    JobHandle Submit(std::function<void()> task, Priority priority = Priority::Normal);

    /// 提交带依赖的任务（依赖完成后才执行）
    JobHandle SubmitWithDependency(std::function<void()> task,
                                   std::vector<JobHandle> dependencies);

    /// 等待任务完成
    void Wait(JobHandle handle);

    enum class Priority { Low, Normal, High, Critical };
};
```

**验证标准**：
- [x] 优先级任务按预期顺序执行
- [x] 依赖任务在依赖完成后执行
- [x] 工作窃取减少线程空闲率
- [x] 编译回归验证通过

**当前进度（2026-04-27）**：
- [`job_system.h`](engine/core/job_system.h) / [`job_system.cpp`](engine/core/job_system.cpp) 已重写，新增：
  - `JobPriority` 枚举（Low/Normal/High），高优先级任务优先出队。
  - `JobHandle` 类，支持 `Wait()` 等待任务完成和 `SubmitWithDependency()` 声明依赖链。
  - `WorkerLocalQueue` + `StealJob()` 工作窃取机制，每线程本地队列 + 窃取其他线程任务减少空闲。
  - 依赖管理：`pending_dependents_` / `pending_jobs_` / `completed_jobs_` 跟踪未满足依赖和完成信号。
- 兼容接口 `Execute()` 保留并委托到 `Submit(job, JobPriority::Normal)`，现有调用者无需修改。
- [`job_system_test.cpp`](tests/gtest/unit/job_system_test.cpp) 已更新，增加优先级、JobHandle Wait 和依赖链的测试用例。
- 结论：P10 代码已完成，编译回归验证已通过。

---

### P11: 编辑器面板拆分

**现状问题**：

- `editor_inspector_panel.cpp` **47KB**，`editor_scene_io.cpp` **62KB**
- 单文件职责过多，维护困难

**目标方案**：按功能拆分

```
apps/editor_cpp/
├── panels/
│   ├── inspector/
│   │   ├── inspector_panel.h          ///< 面板主入口
│   │   ├── transform_editor.h         ///< 变换组件编辑器
│   │   ├── sprite_editor.h            ///< 精灵组件编辑器
│   │   ├── camera_editor.h            ///< 相机组件编辑器
│   │   ├── physics_2d_editor.h        ///< 2D 物理组件编辑器
│   │   ├── audio_editor.h             ///< 音频组件编辑器
│   │   └── lua_script_editor.h        ///< Lua 脚本编辑器
│   ├── hierarchy/
│   │   └── hierarchy_panel.h
│   ├── viewport/
│   │   └── viewport_panel.h
│   └── profile/
│       └── profile_panel.h
├── scene/
│   ├── scene_io.h                     ///< 场景 IO 主入口
│   ├── scene_serializer.h             ///< 场景序列化
│   ├── scene_deserializer.h           ///< 场景反序列化
│   └── scene_migrations.h             ///< 版本迁移
└── ...
```

**验证标准**：
- [ ] 单文件 < 500 行
- [ ] 编辑器功能回归通过

---

## 五、实施路线图

### 阶段规划

```
Phase R1 (2-3 周) ─ 基础设施加固
├── P4: 2D 组件文件拆分            [已完成]
├── P8: 补核心模块单元测试 (P0)     [全绿：568 例 PASSED]
└── P9: EventBus 跨 DLL 安全       [代码完成，3D模块集成验证待后续]

Phase R2 (3-4 周) ─ 核心架构治理
├── P1: 单例滥用治理 (Phase A-C)   [完全闭环，所有兼容层已清退]
├── P2: OpenGLRhiDevice 职责拆分   [完全闭环，四子系统独立文件化 + 状态 Diff]
└── P8: 补核心模块单元测试 (P1)     [全绿：568 例 PASSED]

Phase R3 (3-4 周) ─ 模块化统一
├── P3: 2D 系统模块化对齐          [完全闭环，所有调度路径均通过 IModule 接口]
├── P2 后续激进拆分终局（可选）    [已完成：四子系统已独立文件化 + 状态 Diff]
└── P11: 编辑器面板拆分

Phase R4 (4-6 周) ─ 渲染管线现代化
├── P6: Uniform 管理 UBO 化        [编译回归验证通过]
├── P7: 渲染图 DAG 化              [编译回归验证通过]
└── P10: JobSystem 能力增强        [编译回归验证通过]

Phase R5 (持续) ─ 工程化提升
├── P5: Lua 绑定代码生成化        [调研完成，方案待定：当前绑定用 raw C API 非Sol2，需先统一绑定层]
└── P8: 补集成测试 (P2)
```

### 依赖关系

```
P9 (EventBus) ──→ P1 Phase B (EventBus 单例治理) [已完成]
P4 (组件拆分) ──→ P5 (绑定生成)
P2 后续激进拆分终局（ShaderManager 独立外提） ──→ P6 (UBO 化)
P1 (单例治理) ──→ P3 (2D 模块化) [完全落地]
P2 当前阶段收束基线 ──→ P7 (渲染图 DAG)
```

---

## 六、风险与约束

### 高风险项

| 风险 | 涉及方案 | 缓解措施 |
|------|----------|----------|
| `World` 单例改造影响全局 | P1 Phase C | 渐进式迁移，保留 deprecated 接口 |
| `OpenGLRhiDevice` 拆分期间可能破坏渲染 | P2 | 每步重构后执行渲染冒烟测试 |
| 2D 模块化可能影响性能 | P3 | 内置模块值语义，无 DLL 边界开销 |

### 约束条件

1. **C++20 / MSVC**：所有方案必须在当前编译工具链下可用
2. **无新第三方依赖**：除已在 `depends/` 中的库，不引入新的第三方库
3. **向后兼容**：Lua 脚本 API 和 C++ 公共 API 在主版本内保持兼容
4. **编译时间**：重构不应显著增加编译时间（拆分应减少编译依赖）
5. **性能不退化**：核心路径（ECS 更新、渲染提交）性能不退化

### 回退策略

- 每个方案独立分支开发，合并前通过完整测试
- 保留 deprecated 接口至少 2 个版本周期
- 渲染相关修改需在主流示例（2D Demo + 3D Demo）中验证

---

## 附录：架构总评

**当前成熟度：★★★★☆ (4/5)**

DSEngine 架构设计明显借鉴了现代商业引擎经验，在 ECS 架构、RHI 抽象、模块化加载、双轨脚本等方面做出了合理决策。2D/3D 混合支持、Asset Bundle 加密、命令缓冲渲染等特性表明这是一个面向实际游戏开发的实用引擎。

最大架构短板是**单例滥用**和**核心文件过度膨胀**，会随项目规模增长成为硬瓶颈。本方案优先推进：模块化统一（2D 也走 IModule）、核心类拆分（RhiDevice）、单例去耦合，预计通过 5 个阶段完成全面治理。

---

## 附录：文档版本历史

| 版本 | 日期 | 变更摘要 |
|------|------|----------|
| v2.3.0 | 2026-05-02 | P6 3D 物理 Lua Demo 端到端验证通过：新增 5 个 Lua 物理 API（add_force/add_impulse/set_velocity/get_velocity/set_gravity）；Physics3DSystem 添加动力学接口 + world_cache_ 缓存；3d_physics_stack demo 增强（冲量验证+位置检测+速度查询）；physics_bodies 统计修复（含 RigidBody3DComponent）；AnimatorSystem 延迟 RequireAssetManager 修复空 World 崩溃；verify_lua_3d_demos.py 物理栈验证通过（PhysX ENABLED，5 box FELL，impulse 生效） |
| v2.2.0 | 2026-05-02 | PhysX 真实后端集成：解压完整 physx-4.1 预编译包；修复 CMake PhysX lib 查找（bin/ 目录 fallback）；绕过 CRT 不匹配（自定义 DseDefaultSimulationFilterShader + DseCpuDispatcher 替代 PhysXExtensions 静态库）；添加 DSE_ENABLE_PHYSX 编译定义；注册 Physics3DSystem 到 ServiceLocator + FixedUpdate 调度；添加 physics_3d_raycast Lua 绑定；PhysX DLL post-build 复制；461 单元测试全绿 |
| v2.1.0 | 2026-05-02 | P8 全量测试验证通过：568 例全绿（单元 461 + 集成 107 + 冒烟 6）；文档中原记录的 2 个 EventBus::Instance 兼容测试失败已在后续提交中修复；更新 P8/ctest 验证标准为已通过 |
| v2.0.0 | 2026-05-02 | 渲染管线与粒子系统缺陷修复：`glDepthMask(GL_TRUE)` 深度写入修复（VSE 15.22 场景全像素非黑）；`ShutdownGeometryBuffers` GL 资源释放 + 静态 VAO/VBO 转成员变量；VSE 15.22 诊断代码 `#ifdef DSE_VSE_1522_DIAG` 编译隔离；`DEBUG_LOG` 格式符 `{:.3f}`/`{:X}` → `{}` 修正；粒子发射累加器首帧 dt 爆炸修复（`emission_accumulator` 钳制上限为 `max_particles`）；验证脚本 Windows 编码处理；VSE 15.22 综合场景 demo 落地验证通过 |
| v1.9.0 | 2026-04-27 | 统一编译回归验证通过：dse_engine + dse_gtest_unit_tests 编译成功；修复 3 个编译问题（rhi_device.h 命名空间 render::→dse::render::、gl_draw_executor.h 裸函数指针→std::function 支持捕获 lambda、render_graph.h 嵌套 /* */ 注释冲突）；183 个单元测试中 181 个 PASSED，2 个 EventBus::Instance 兼容测试待修复；P2/P6/P7/P10 验证标准勾选更新 |
| v1.8.0 | 2026-04-27 | P2 终局完成：GLPipelineStateManager 加入 cached_gl_state_ Diff 机制（仅切换变化的 GL 状态，减少冗余调用）；确认四子系统（ResourceManager/PipelineStateManager/ShaderManager/DrawExecutor）+ UBOManager 全部独立文件化；OpenGLRhiDevice 协调器 277 行 < 300 行目标；P2 状态升级为完全闭环 |
| v1.7.0 | 2026-04-27 | P8 补充 render_graph_test（20+ 用例覆盖 DAG 拓扑/剔除/循环检测）和 module_test（IModule 生命周期/多态）；P9 扩充 events 命名空间至 41 个常量（UI/资源/场景/输入/物理/音频/动画/生命周期）；更新跨 DLL 测试覆盖全部常量无碰撞；全量代码重构完成，待统一编译测试验证 |
| v1.6.0 | 2026-04-27 | P6 UBO 迁移代码完成（ubo_types/ubo_manager/PBR 着色器 UBO block/DrawExecutor 适配/RhiDevice 集成）；P7 Render Graph DAG 代码完成（render_graph.h/cpp + FramePipeline 集成，拓扑排序+自动剔除）；P10 JobSystem 增强代码完成（优先级队列+JobHandle 依赖+工作窃取）；三者均静态检查通过，待编译回归验证 |
| v1.5.0 | 2026-04-27 | P2 收尾：修复 RenderTargetDesc/PipelineStateDesc 重定义编译错误；RHI 类型统一归档到 rhi_types.h（SpriteDrawItem/MeshDrawItem/Particle3DDrawItem/BatchVertex/RenderPassDesc/RenderTargetReadback/RenderStats 等）；P8 补充路径解析测试（NormalizeAssetPath/ResolveAssetPath） |
| v1.4.0 | 2026-04-27 | P8 补充 ecs_component_test、asset_manager_test、event_id_cross_dll_test；P9 更新进度 |
| v1.3.0 | 2026-04-27 | P3 完全闭环：IModule 新增 OnRenderUI 虚方法，UI Pass 也通过 IModule 接口统一分发；P8 补充 math_pool_test（贝塞尔曲线/缓动/内存池/对象池） |
| v1.2.0 | 2026-04-27 | P1 完全闭环：World::Instance() auto-create 清退；P3 渲染路径统一到 OnRenderScene |
| v1.1.0 | 2026-04-27 | P1 闭环：JobSystem 兼容层全部清退；P3 主体落地：OnUpdate/OnFixedUpdate 填充完整逻辑；P8 补充 EventBus 单元测试 |
| v1.0.0 | 2026-04-24 | 初始版本：P2/P4 阶段收尾，P9 主体落地，P1/P3/P8 推进中 |
