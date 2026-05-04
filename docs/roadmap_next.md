# DSEngine 引擎下一步路线图

> 生成日期：2026-05-04
> 排除项：编辑器（不在此路线图范围内）

---

## 当前完成度一览

| 模块 | 代码量 | 完成度 | 测试覆盖 | 短板 |
|------|--------|--------|----------|------|
| **ECS** | 17 文件 | ★★★★☆ | 15 unit + 集成 | 组件齐全，World/API 成熟 |
| **渲染/RHI** | 16 文件(OpenGL+Vulkan) | ★★★★★ | 2 unit + 2 集成 | 双后端(OpenGL+Vulkan)，RenderGraph DAG，RHI 工厂 |
| **Physics3D** | 2 文件 | ★★★★☆ | 1 unit + 1 集成 | PhysX 集成完整，刚体/角色/raycast |
| **Physics2D** | 2 文件 | ★★★☆☆ | 1 集成 | 有基本碰撞，缺 joint/motor |
| **Scripting/Lua** | 22 文件 | ★★★★★ | 5 集成 | Binding 覆盖最全（ECS/3D/物理/粒子/UI/动画/Spine） |
| **Asset** | 4 文件 | ★★★☆☆ | 1 unit + 2 集成 | 仅同步加载，缺异步管线/热重载 |
| **Scene** | 6 文件 | ★★★★☆ | 有序列化 | 有 prefab/序列化/空间划分 |
| **Audio** | 2 文件 | ★★★☆☆ | 无独立测试 | miniaudio 封装，缺 3D 空间化 C++ API |
| **Runtime** | 15 文件 | ★★★★☆ | 有测试 | FramePipeline/EngineApp 成熟 |
| **Profiler** | 6 文件 | ★★★☆☆ | 无测试 | CPU/Memory/Render 三线，但缺集成 |
| **Input** | 2 文件 | ★★★☆☆ | 无独立测试 | 基础键鼠，缺 gamepad/手势 |
| **3D Demo** | 32 个 | ★★★★★ | verify_all.bat | 覆盖极广 |

---

## 优先级排序

### 🔥 P0：渲染管线现代化

**原因**: 当前渲染只有 OpenGL 后端，且 `CommandBuffer` 是手写 variant 式分发。RenderGraph 已经有 DAG 框架但还没和 FramePipeline 深度整合。这是引擎的"面子"和性能上限。

具体方向：
1. **Vulkan RHI 后端** — 新增 `VulkanRhiDevice`，复用 `CommandBuffer` 抽象。这是引擎从"能跑"到"能上生产"的关键分水岭
2. **RenderGraph ↔ FramePipeline 深度整合** — 当前 `frame_pipeline.cpp` 有 1000+ 行，把硬编码的 pass 顺序迁移到 RenderGraph DAG 驱动，实现自动剔除和 barrier
3. **多线程命令提交** — `CommandBuffer` 已有前端录制/后端提交分离，但尚未并行化。利用 `JobSystem` 做 parallel command recording

### 🔥 P0：资产管线异步化 + 热重载

**原因**: 当前 `AssetManager` 全部同步加载，32 个 demo 启动时会有明显卡顿。且无热重载能力，改资源必须重启。

具体方向：
1. **异步加载队列** — 利用现有 `JobSystem`，将 `.dmesh/.dmat/.png` 加载分帧摊开，加载完成后通过 `EventBus` 通知
2. **热重载 Watcher** — 监听 `data/` 目录文件变更，自动 reload 对应 asset（纹理/材质/脚本）
3. **资源引用计数 + LRU 淘汰** — 当前资源只进不出，长时间运行会 OOM

### ⭐ P1：场景系统增强

**原因**: 当前 `Scene` 序列化/反序列化已有，但缺子场景（Level Streaming）和 LOD 切换。

具体方向：
1. **子场景/Level Streaming** — 加载/卸载子场景，共享 World 的 ECS registry 但独立管理生命周期
2. **场景对象引用** — 跨 scene 的 Entity 引用，当前只有 entity ID
3. **运行时 Scene 状态机** — 场景切换过渡（fade/additive load）

### ⭐ P1：音频 3D 空间化 C++ 层

**原因**: `AudioSystem` 的 3D 空间化只在 Lua binding 层做了简单暴露，C++ 层没有 `AudioListener`、距离衰减模型、遮挡等。

具体方向：
1. **AudioListener 组件** — 跟随 Camera 自动更新 listener position/orientation
2. **距离衰减模型** — Linear/Inverse/Exponential 三种
3. **Occlusion/Obstruction** — 基于 Physics3D raycast 的声音遮挡

### 🔹 P2：Profiler 可视化 + 性能回归基线

具体方向：
1. **JSON/Chrome Trace 格式导出** — 对标 Chrome `about:tracing`
2. **Google Benchmark 集成** — 关键路径建立性能基线
3. **CI 性能回归门禁** — 超基线 10% 自动 fail

### 🔹 P2：Input 系统增强

具体方向：
1. **Gamepad 支持** — XInput / SDL Gamepad
2. **Action Mapping** — 抽象 input → action 映射，支持运行时 rebind
3. **Input Recording/Playback** — 用于测试回放

---

## 当前执行：Vulkan RHI 后端

> 状态：进行中（基础框架已搭建）

### 实施计划

1. ✅ 深入分析现有 OpenGL RHI 抽象层（`CommandBuffer` / `RhiDevice` / 各子系统）
2. ✅ 重构 `rhi_types.h`：将 OpenGL 硬编码常量替换为 RHI 无关枚举（`BlendFactor`/`CompareFunc`/`CullFace`）
3. ✅ 创建 `gl_enum_convert.h`：GL 后端枚举映射工具
4. ✅ 创建 `VulkanContext`：Instance/Device/Swapchain 初始化与生命周期管理
5. ✅ 创建 `VulkanResourceManager`：纹理/Buffer/RenderTarget 创建与销毁（含 staging buffer 上传）
6. ✅ 创建 `VulkanRhiDevice` + `VulkanCommandBuffer`：实现 `RhiDevice` 抽象接口
7. ✅ CMake 集成：`DSE_ENABLE_VULKAN` 选项 + `find_package(Vulkan)` + 条件编译排除
8. ✅ GTest 验证：Vulkan=OFF 模式下 3/3 测试全通过，零回归

### 已创建文件

| 文件 | 说明 |
|------|------|
| `engine/render/rhi/vulkan/vulkan_context.h/.cpp` | Vulkan 上下文（Instance/Device/Swapchain/同步） |
| `engine/render/rhi/vulkan/vulkan_resource_manager.h/.cpp` | GPU 资源管理（纹理/Buffer/RenderTarget） |
| `engine/render/rhi/vulkan/vulkan_rhi_device.h/.cpp` | RhiDevice Vulkan 实现 + VulkanCommandBuffer（五子系统协调器） |
| `engine/render/rhi/vulkan/vulkan_shader_manager.h/.cpp` | GLSL→SPIR-V 运行时编译（glslang）+ 反射 + DescriptorSetLayout 缓存 |
| `engine/render/rhi/vulkan/vulkan_pipeline_state_manager.h/.cpp` | VkPipeline/VkRenderPass 缓存 + RHI→Vulkan 枚举映射 |
| `engine/render/rhi/vulkan/vulkan_draw_executor.h/.cpp` | 绘制命令执行（2D/3D/天空盒/后处理/粒子，当前 stub） |
| `engine/render/rhi/gl_enum_convert.h` | GL 后端枚举映射工具 |
| `engine/render/rhi/rhi_types.h` | 重构为 RHI 无关枚举 + `RhiBackend` 后端选择枚举 |
| `engine/render/rhi/rhi_factory.h/.cpp` | RHI 设备工厂（环境变量 `DSE_RHI_BACKEND=opengl|vulkan` 选择后端） |
| `engine/render/rhi/vulkan/vulkan_shader_sources.h` | Vulkan GLSL 450 着色器源码（PBR/天空盒/粒子/后处理全套） |
| `cmake/CMakeLists.txt.vulkan` | Vulkan Headers + Loader 源码集成脚本 |

### 运行时 RHI 后端选择机制

- **工厂函数**：`dse::render::CreateRhiDevice(RhiBackend)` 根据枚举创建对应后端实例
- **环境变量**：`DSE_RHI_BACKEND=opengl` 或 `DSE_RHI_BACKEND=vulkan` 指定后端
- **基类接口统一**：阴影/光源方法（`SetGlobalShadowMap` 等）已提升到 `RhiDevice` 基类虚函数，消除了所有 `dynamic_cast<OpenGLRhiDevice*>` 依赖
- **名称隐藏修复**：`OpenGLRhiDevice` 和 `VulkanRhiDevice` 均添加 `using RhiDevice::SetGlobalSpotShadowMap` 和 `using RhiDevice::SetGlobalSpotLightSpaceMatrix` 以解决 C++ 名称隐藏

### Vulkan 依赖集成方式

- **Vulkan Headers v1.3.296**：已接入 `depends/Vulkan-Headers/`，以 `add_subdirectory` 方式编入引擎
- **Vulkan Loader v1.3.296**：已接入 `depends/Vulkan-Loader/`，以 `add_subdirectory` 方式编入引擎
  - 条件编译：`cmake/CMakeLists.txt.vulkan` 仅在 `DSE_ENABLE_VULKAN=ON` 时加载
  - 优先级：源码 > 系统 SDK > 回退禁用
  - 编译产出：`Vulkan::Headers`（接口库）+ `Vulkan::Loader`（vulkan-1.dll）
  - CRT 一致性：自动将 Vulkan Loader 子目标强制为 `/MD` DLL CRT
  - **开发者无需单独安装 Vulkan SDK**，只需克隆子模块即可编译 Vulkan 路径
- **glslang 15.0.0**：已接入 `depends/glslang/`，以 `add_subdirectory` 方式编入引擎
  - 条件编译：`cmake/CMakeLists.txt.glslang` 仅在 `DSE_ENABLE_VULKAN=ON` 时加载
  - 编译产出：`glslang`, `SPIRV`, `MachineIndependent`, `GenericCodeGen` 静态库链接进 `dse_engine`
  - CRT 一致性：自动将 glslang 子目标强制为 `/MD` DLL CRT
  - 运行时守卫：`DSE_HAS_GLSLANG` 宏控制 `vulkan_shader_manager.cpp` 中的编译/非编译路径
  - 优化器未接入：`ENABLE_OPT=OFF`，不依赖 SPIRV-Tools，减少编译时间
- **SPIRV-Reflect**：完整反射能力，后续替代当前简化版反射（规划中）

### 待完成项

- [x] 运行时 RHI 后端选择（OpenGL ↔ Vulkan 切换）
- [x] glslang 源码接入 `depends/` + CMake 条件编译
- [x] 将现有 GLSL 着色器源码适配为 Vulkan 兼容版本（`#version 450` + separate sampler）
- [x] 完善绘制命令实现（VulkanDrawExecutor 从 stub 到真实 vkCmdDraw*）
- [x] 修复 Vulkan 路径编译错误（RHI 类型适配：MeshDrawItem/RenderPassDesc/RenderStats 等）
- [x] 实现 VulkanRhiDevice::Submit / EndFrame 真实提交流程（vkQueueSubmit + PresentFrame）
- [x] 实现 VulkanCommandBuffer 全局 uniform 暂存（SetGlobalMat4/Array → pending 消费）
- [x] 延迟 Pipeline 创建（VulkanPipelineStateManager::GetOrCreateVkPipeline）
- [x] 修复 BeginRenderPass 的 swapchain Framebuffer 获取
- [x] 修复 gl_enum_convert.h 宏重定义警告（改为 GLConst 命名空间常量）
- [x] Vulkan=ON 编译通过 + Vulkan=OFF 零回归验证
- [x] Vulkan UBO 上传（PerFrame/PerScene/PerMaterial → DescriptorSet 更新）
- [x] Vulkan DescriptorPool 创建与 DescriptorSet 分配
- [x] 所有 Draw 调用绑定真实 DescriptorSet（替换 VK_NULL_HANDLE）
- [x] 内置着色器初始化（PBR/Skybox/Particle/Sprite/PostProcess）
- [x] Vulkan 2D 精灵批处理完整实现（DrawSpriteBatch 顶点组装 + 绘制）
- [x] Vulkan 后处理着色器独立化（PostProcess 专用着色器 + DrawPostProcess 使用）
- [x] RenderTarget 像素回读（ReadRenderTargetColorRgba8 via vkCmdCopyImageToBuffer）
- [ ] 通过现有 3D Demo 验证 Vulkan 渲染路径
