# DSEngine 架构修复优化方案

> **版本**: v1.0.0  
> **日期**: 2026-04-24  
> **状态**: 进行中（已完成 P4 收尾与 P9 主体落地，P1/P2/P3/P8 仍在推进）
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

**当前进度（2026-04-24）**：
- 已落地 [`ServiceLocator`](engine/core/service_locator.h:42)，并完成 [`JobSystem`](engine/core/job_system.cpp:114)、[`EventBus`](engine/core/event_bus.cpp:13)、[`World`](engine/ecs/world.cpp:9) 的兼容迁移。
- [`EngineInstance`](engine/runtime/engine_app.h:39) 已显式持有一个实例级 [`ServiceLocator`](engine/core/service_locator.h:42)，并通过 [`RegisterRuntimeServices()`](engine/runtime/engine_app.cpp:158) / [`ResetRuntimeServices()`](engine/runtime/engine_app.cpp:175) 统一登记和清理自身托管的 [`FramePipeline`](engine/runtime/frame_pipeline.h:39)、[`World`](engine/ecs/world.h:43)、[`EventBus`](engine/core/event_bus.h:152) 服务。
- [`ServiceLocator`](engine/core/service_locator.h:42) 新增了 [`BridgeTo()`](engine/core/service_locator.h:74) 能力，运行时桥接点已从“手写重复 Register”收敛为“实例级容器登记后统一桥接到兼容全局入口”。
- [`EngineInstance::Init()`](engine/runtime/engine_app.cpp:194) 现在会显式创建并注册实例级 [`EventBus`](engine/core/event_bus.h:152)，不再依赖 [`EventBus::Instance()`](engine/core/event_bus.h:165) 的延迟构造来补齐运行时接线。
- 进一步地，[`AssetManager`](engine/assets/asset_manager.h) 已新增显式 [`EventBus`](engine/core/event_bus.h:152) 注入入口，并在 [`EngineInstance::Init()`](engine/runtime/engine_app.cpp:194) / [`EngineInstance::Shutdown()`](engine/runtime/engine_app.cpp:316) 中完成接线与解绑；[`LoadTextureAsync()`](engine/assets/asset_manager.cpp:605) 这条低风险调用链已优先使用实例级事件总线，只有未注入时才回退到兼容 [`EventBus::Instance()`](engine/core/event_bus.h:165)。
- 当前仍未真实收尾的阻塞点主要集中在兼容层：[`World::Instance()`](engine/ecs/world.cpp:9) 仍保留“未注入时自动创建”的旧路径；[`JobSystem`](engine/core/job_system.cpp:114) 仍依赖 [`InitStatic()`](engine/core/job_system.h:78) / [`ShutdownStatic()`](engine/core/job_system.h:84) / [`ExecuteStatic()`](engine/core/job_system.h:90) 兼容入口。
- 经本轮复核，这两处不能“安全一次性收尾”：
  - [`World::Instance()`](engine/ecs/world.cpp:9) 的 auto-create 目前被 [`tests/gtest/unit/world_test.cpp`](tests/gtest/unit/world_test.cpp:114) 明确断言；若直接改成未注册即失败，会立刻打破现有测试语义，也会改变兼容层约定。
  - [`JobSystem::InitStatic()`](engine/core/job_system.cpp:114) / [`ExecuteStatic()`](engine/core/job_system.cpp:133) / [`ShutdownStatic()`](engine/core/job_system.cpp:124) 仍被 [`EngineInstance`](engine/runtime/engine_app.cpp:253)、[`AssetManager::LoadTextureAsync()`](engine/assets/asset_manager.cpp:621) 以及 [`tests/gtest/unit/job_system_test.cpp`](tests/gtest/unit/job_system_test.cpp:135) 直接依赖；若硬切，需要先补实例级注入通道和测试迁移，不属于“低风险收尾”。
- **下一阶段最小切口建议**：优先处理 [`JobSystem`](engine/core/job_system.cpp:114) 兼容层，而不是 [`World`](engine/ecs/world.cpp:9)。原因是 [`World`](engine/ecs/world.cpp:9) 的残留兼容语义已经被测试直接固化，而 [`JobSystem`](engine/core/job_system.cpp:114) 虽然调用面更广，但大部分依赖点集中在 [`EngineInstance`](engine/runtime/engine_app.cpp:253) 与 [`AssetManager::LoadTextureAsync()`](engine/assets/asset_manager.cpp:621) 两条主链，具备通过“先补注入、再迁调用、最后删静态入口”的专项方式逐步退役的条件。
- **建议实施顺序**：
  1. 先给 [`RuntimeServices`](engine/runtime/runtime_services.h:9) / [`EngineInstance`](engine/runtime/engine_app.h:39) 增加显式 [`JobSystem`](engine/core/job_system.h:45) 注入与持有能力；
  2. 再把 [`EngineInstance::Init()`](engine/runtime/engine_app.cpp:194)、[`EngineInstance::Shutdown()`](engine/runtime/engine_app.cpp:316)、[`AssetManager::LoadTextureAsync()`](engine/assets/asset_manager.cpp:621) 从 [`InitStatic()`](engine/core/job_system.h:78) / [`ExecuteStatic()`](engine/core/job_system.h:90) / [`ShutdownStatic()`](engine/core/job_system.h:84) 改成优先使用实例级入口；
  3. 最后再迁移 [`tests/gtest/unit/job_system_test.cpp`](tests/gtest/unit/job_system_test.cpp:135) 的静态兼容断言，并评估是否保留仅用于过渡的薄包装。
- 结论：P1 的主运行时架构已经完成收口，剩余部分本质上是兼容层退役工程，而不是主架构缺口；在当前代码基线下，P1 不能被诚实地标记为“100% 一次性收尾完成”，其最终状态应定义为“主链路完成，兼容层残留待后续专项清退”，且下一阶段应以 [`JobSystem`](engine/core/job_system.cpp:114) 兼容层专项为优先入口。

---

### P2: OpenGLRhiDevice 职责拆分

**现状问题**：

- `OpenGLRhiDevice` 头文件 **95KB / 769 行**，承担了资源管理、状态机、着色器、绘制命令四大职责
- uniform location 全部以 `int uniform_xxx_loc_` 手动管理
- 编译慢、维护难、职责不清

**目标架构**：拆分为 4 个子系统

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

**迁移策略**：

| 阶段 | 工作 | 影响范围 |
|------|------|----------|
| Step 1 | 提取 `GLResourceManager` | 资源创建/销毁调用处 |
| Step 2 | 提取 `GLPipelineStateManager` | 状态设置调用处 |
| Step 3 | 提取 `GLShaderManager` + 着色器反射 | uniform location 管理处 |
| Step 4 | 提取 `GLDrawExecutor` | 命令执行路径 |
| Step 5 | 清理 `OpenGLRhiDevice` 为协调器 | 编译依赖 |

**验证标准**：
- [ ] `OpenGLRhiDevice` 头文件 < 300 行
- [ ] 所有 uniform location 通过反射自动获取，不再手动维护
- [ ] 状态切换自动 Diff，减少冗余 GL 调用
- [ ] 现有渲染功能回归通过

**当前进度（2026-04-24）**：
- 仅完成 [`GLResourceManager`](engine/render/rhi/gl_resource_manager.h:75) 的初步抽取。
- [`rhi_device.h`](engine/render/rhi/rhi_device.h) 仍是大体量头文件，且仓库内尚未出现 `GLPipelineStateManager`、`GLShaderManager`、`GLDrawExecutor` 三个关键子系统实现。
- 结论：P2 仍处于 Step 1 起步阶段，距离“一次性收尾”存在明显结构缺口。

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
- [ ] `FramePipeline` 不再直接实例化任何具体系统
- [ ] 2D/3D 模块共享 `IModule` 接口
- [ ] 可通过配置裁剪 2D 模块（纯 3D 项目）
- [ ] 现有 2D 渲染/交互回归通过

**当前进度（2026-04-24）**：
- [`FramePipeline`](engine/runtime/frame_pipeline.h:42) 仍直接持有 [`SpriteRenderSystem`](engine/runtime/frame_pipeline.h:209)、[`Physics2DSystem`](engine/runtime/frame_pipeline.h:211)、[`AnimationSystem`](engine/runtime/frame_pipeline.h:212) 等 2D 具体系统实例。
- 运行时中只看到了 3D 模块的动态模块化路径，尚未出现 `Gameplay2DModule` 或统一的 2D/3D 模块注册入口。
- 结论：P3 尚未进入真正实现阶段，无法与 P1/P2 一并“收尾”。

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
- 当前 [`engine/ecs/components_2d.h`](engine/ecs/components_2d.h) 已退化为纯兼容聚合入口，收益目标已从“拆文件”推进到“实际 include 依赖收敛”。

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
- [ ] 不再有任何 `uniform_xxx_loc_` 手动声明
- [ ] 新增 uniform 只需修改着色器和对应数据结构
- [ ] UBO 共享减少 uniform 设置次数 > 50%

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
- [ ] Pass 依赖通过资源读写自动推断
- [ ] 无依赖的 Pass 自动并行
- [ ] 无输出被读取的 Pass 自动剔除
- [ ] 现有渲染效果回归通过

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
- [x] 已补充 [`event_id_test.cpp`](tests/gtest/unit/event_id_test.cpp)、[`service_locator_test.cpp`](tests/gtest/unit/service_locator_test.cpp)、[`world_test.cpp`](tests/gtest/unit/world_test.cpp)、[`job_system_test.cpp`](tests/gtest/unit/job_system_test.cpp)
- [x] gtest 单元测试目标已在 [`tests/gtest/unit/CMakeLists.txt`](tests/gtest/unit/CMakeLists.txt) 注册
- [ ] `ctest` 可通过，所有测试绿色
- [ ] CI 可执行最小验证集

**当前进度（2026-04-24）**：
- P8 已从单一冒烟测试转向核心基础设施单元测试。
- 当前覆盖重点为 P1/P9 对应的服务定位器、事件 ID、World、多线程任务系统。
- 目前确认测试目标已接入 CMake，但当前工作区缺少现成构建目录，尚未完成本轮修改后的本地编译回归验证。
- 本轮按需 include 迁移已通过差异检查确认仅涉及头文件依赖收敛，未改动运行时逻辑路径。

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

**当前进度（2026-04-24）**：
- 已完成 [`EventId`](engine/core/event_id.h:20) 与 [`MakeEventId()`](engine/core/event_id.h:30) 落地。
- [`EventBus`](engine/core/event_bus.h:116) 现通过 `EventTraits` + `kEventId` 分发事件，替代 RTTI/type_index 路径。
- 仍缺少跨 DLL 的集成级验证，这也是 P1 Phase B 完全闭环前的剩余工作。

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
- [ ] 优先级任务按预期顺序执行
- [ ] 依赖任务在依赖完成后执行
- [ ] 工作窃取减少线程空闲率

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
├── P4: 2D 组件文件拆分            [进行中，主体已完成]
├── P8: 补核心模块单元测试 (P0)     [进行中，已注册 gtest 单测目标]
└── P9: EventBus 跨 DLL 安全       [进行中，核心实现已落地]

Phase R2 (3-4 周) ─ 核心架构治理
├── P1: 单例滥用治理 (Phase A-C)   [进行中，已完成 JobSystem/EventBus/World 兼容迁移]
├── P2: OpenGLRhiDevice 拆分 (Step 1-3) [进行中，已落地 GLResourceManager]
└── P8: 补核心模块单元测试 (P1)     [进行中]

Phase R3 (3-4 周) ─ 模块化统一
├── P3: 2D 系统模块化对齐
├── P2: OpenGLRhiDevice 拆分 (Step 4-5)
└── P11: 编辑器面板拆分

Phase R4 (4-6 周) ─ 渲染管线现代化
├── P6: Uniform 管理 UBO 化
├── P7: 渲染图 DAG 化
└── P10: JobSystem 能力增强

Phase R5 (持续) ─ 工程化提升
├── P5: Lua 绑定代码生成化
└── P8: 补集成测试 (P2)
```

### 依赖关系

```
P9 (EventBus) ──→ P1 Phase B (EventBus 单例治理)
P4 (组件拆分) ──→ P5 (绑定生成)
P2 Step 3 (ShaderManager) ──→ P6 (UBO 化)
P1 (单例治理) ──→ P3 (2D 模块化)
P2 (RhiDevice 拆分) ──→ P7 (渲染图 DAG)
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
