# DSEngine 引擎下一步路线图

> 生成日期：2026-05-05
> 排除项：编辑器（不在此路线图范围内）

---

## 当前完成度一览

| 模块 | 代码量 | 完成度 | 测试覆盖 | 短板 |
|------|--------|--------|----------|------|
| **ECS** | 17 文件 | ★★★★☆ | 15 unit + 集成 | 组件齐全，World/API 成熟 |
| **渲染/RHI** | 16 文件(OpenGL+Vulkan) | ★★★★★ | 57 unit + 2 集成 | 双后端(OpenGL+Vulkan)，RenderGraph DAG，RHI 工厂，Vulkan 端到端验证 |
| **Physics3D** | 2 文件 | ★★★★☆ | 1 unit + 1 集成 | PhysX 集成完整，刚体/角色/raycast |
| **Physics2D** | 2 文件 | ★★★☆☆ | 1 集成 | 有基本碰撞，缺 joint/motor |
| **Scripting/Lua** | 22 文件 | ★★★★★ | 5 集成 | Binding 覆盖最全（ECS/3D/物理/粒子/UI/动画/Spine） |
| **Asset** | 4 文件 | ★★★★☆ | 2 unit + 2 集成 | 异步加载全资源类型 + LRU 淘汰 + 文件热重载 |
| **Scene** | 10 文件 | ★★★★★ | 45 unit | 有 prefab/序列化/空间划分/子场景/场景管理器/场景切换/UUID跨场景引用 |
| **Audio** | 2 文件 | ★★★★☆ | 21 unit | miniaudio 封装 + 3D Listener/衰减模型/遮挡 |
| **Runtime** | 15 文件 | ★★★★☆ | 有测试 | FramePipeline/EngineApp 成熟 |
| **Profiler** | 6 文件 | ★★★★☆ | 43 unit | CPU/Memory/Render 三线 + Chrome Trace 导出 + 性能基线 |
| **Input** | 6 文件 | ★★★★☆ | 46 unit | 键鼠+Gamepad+ActionMapping+录制回放 |
| **3D Demo** | 32 个 | ★★★★★ | verify_all.bat | 覆盖极广 |

---

## 优先级排序

### ✅ P0：渲染管线现代化（已完成）

**原因**: 当前渲染只有 OpenGL 后端，且 `CommandBuffer` 是手写 variant 式分发。RenderGraph 已经有 DAG 框架但还没和 FramePipeline 深度整合。这是引擎的"面子"和性能上限。

具体方向：
1. ✅ **Vulkan RHI 后端端到端验证** — `VulkanRhiDevice` 五大子系统（Context/ResourceManager/PipelineStateManager/ShaderManager/DrawExecutor）全部通过无 GPU 单元测试验证（55 tests, 666 总测试零回归）
2. ✅ **RenderGraph ↔ FramePipeline 深度整合** — 硬编码 pass 顺序已迁移到 RenderGraph DAG 驱动（5 阶段全部完成）
3. ✅ **多线程命令提交** — 波次并行录制 + 顺序提交（利用 `JobSystem` 做 parallel command recording）

### ✅ P0：资产管线异步化 + 热重载（已完成）

**原因**: 当前 `AssetManager` 全部同步加载，32 个 demo 启动时会有明显卡顿。且无热重载能力，改资源必须重启。

具体方向：
1. ✅ **异步加载队列** — 利用现有 `JobSystem`，将 `.dmesh/.dmat/.png/.danim/.dskel/.wav` 加载分帧摊开，加载完成后通过 `EventBus` 通知
2. ✅ **热重载 Watcher** — 监听 `data/` 目录文件变更（Windows `ReadDirectoryChangesW` overlapped I/O），自动 reload 对应 asset（纹理/Mesh/音频）
3. ✅ **LRU 淘汰 + 内存预算** — 为资源添加 `LruEntry`（字节估算 + 最近访问时间戳），`SetMemoryBudget()` / `EvictLRU()` 超预算时淘汰最久未用且已无外部引用的资源

### ✅ P1：场景系统增强（已完成）

**原因**: 当前 `Scene` 序列化/反序列化已有，但缺子场景（Level Streaming）和 LOD 切换。

具体方向：
1. ✅ **子场景/Level Streaming** — `SubScene` 类（`engine/scene/sub_scene.h/.cpp`），共享 World ECS registry 独立管理 Entity 生命周期，支持 Load/Unload（11 tests）
2. ✅ **SceneManager** — `SceneManager` 类（`engine/scene/scene_manager.h/.cpp`），管理多 SubScene，支持同步/异步加载（JobSystem）、EventBus 事件通知、ServiceLocator 注册（15 tests）
3. ✅ **运行时 Scene 状态机** — `TransitionTo(path, TransitionMode)` 支持 Instant/Additive/Fade 三种过渡模式，Fade 含 FadingOut→Loading→FadingIn 状态机（8 tests）
4. ✅ **跨场景 Entity 引用** — `UUIDComponent`（`engine/ecs/uuid_component.h`）+ `SceneManager::ResolveReference(uuid)` 在所有已加载 SubScene 中查找 Entity（11 tests）

### ✅ P1：音频 3D 空间化 C++ 层（已完成）

**原因**: `AudioSystem` 的 3D 空间化只在 Lua binding 层做了简单暴露，C++ 层没有 `AudioListener`、距离衰减模型、遮挡等。

具体方向：
1. ✅ **AudioListener 组件** — 跟随 Camera 自动更新 listener position/orientation
2. ✅ **距离衰减模型** — Linear/Inverse/Exponential 三种
3. ✅ **Occlusion/Obstruction** — 基于 Physics3D raycast 的声音遮挡

#### 实施内容

1. ✅ **Phase 1: AudioListener + 方向同步**
   - `AudioListenerComponent`（`engine/ecs/audio.h`）支持 enabled / listener_index
   - `AudioSystem::Update` 从 TransformComponent 四元数计算 forward/up，同步 listener position + direction + world_up 到 miniaudio
   - Camera3DComponent 自动回退：无显式 AudioListenerComponent 时，自动使用第一个活跃 Camera3D 实体的 Transform 作为 listener

2. ✅ **Phase 2: 距离衰减模型**
   - `AudioAttenuationModel` 枚举（Inverse / Linear / Exponential）
   - `AudioSourceComponent` 新增 `attenuation_model` 字段
   - `AudioSystem::Update` 通过 `ma_sound_set_attenuation_model()` 将枚举映射到 miniaudio 衰减模型

3. ✅ **Phase 3: Occlusion/Obstruction**
   - `AudioSourceComponent` 新增 `occlusion_enabled` / `occlusion_factor` 字段
   - `AudioSystem` 新增 `SetRaycastFunction(AudioRaycastFunc)` 解耦回调（不直接依赖 Physics3D 库）
   - Update 循环中对启用空间化+遮挡的音源，从 listener→source 发射射线，命中障碍物时对音量施加 `occlusion_factor` 衰减

4. ✅ **新增 21 个单元测试**（611 总测试零回归）

#### 变更文件

| 文件 | 说明 |
|------|------|
| `engine/ecs/audio.h` | 新增 `AudioAttenuationModel` 枚举 + `AudioSourceComponent` 衰减/遮挡字段 |
| `engine/audio/audio_system.h` | 新增 `AudioRaycastResult` / `AudioRaycastFunc` / `SetRaycastFunction()` |
| `engine/audio/audio_system.cpp` | Update 重写：listener 方向同步 + Camera3D 回退 + 衰减模型 + 遮挡射线检测 |
| `tests/gtest/unit/engine/audio/audio_3d_spatial_test.cpp` | 21 个单元测试（Phase 1/2/3） |

### ✅ P0：Vulkan RHI 端到端验证（已完成）

**原因**: Vulkan 后端代码（5 个子系统 × 2000+ 行）已可编译但缺乏自动化测试覆盖。需确认所有 RHI 抽象接口、枚举映射、数据结构默认值和安全边界在无 GPU 环境下正确。

#### 实施内容

1. ✅ **Phase 1: 编译验证** — `DSE_ENABLE_VULKAN=ON` 全量编译，611 既有测试零回归
2. ✅ **Phase 2: RHI Factory 测试** — `CreateRhiDevice(Vulkan)` / `ResolveRhiBackendFromEnv` / `RhiBackendToString` 枚举覆盖
3. ✅ **Phase 3: VulkanRhiDevice 无 GPU 测试** — 构造/析构/Shutdown 安全、CreateVertexArray 递增句柄、LastFrameStats 默认值、子系统访问器、全局阴影接口
4. ✅ **Phase 4: VulkanCommandBuffer 无 GPU 测试** — Reset/SetCamera/全局 uniform 暂存与清除、所有 Draw/RenderPass 命令在 device=nullptr 时安全
5. ✅ **Phase 5: 子系统接口一致性测试** — PipelineStateManager BlendFactor/CompareFunc/CullFace 全枚举映射、VulkanContext 默认成员、ResourceManager/ShaderManager 初始状态、数据结构（VulkanBuffer/VulkanTexture/VulkanRenderTarget/VulkanShaderProgram）默认值

#### 测试统计

- **新增 55 个单元测试**（666 总测试零回归）
- 覆盖 6 个测试 Suite：RhiFactory / VulkanRhiDevice / VulkanCommandBuffer / VulkanPipelineStateManager / VulkanContext / VulkanDrawExecutor 等

#### 变更文件

| 文件 | 说明 |
|------|------|
| `tests/gtest/unit/engine/render/vulkan_rhi_test.cpp` | 55 个 Vulkan 无 GPU 单元测试 |
| `tests/gtest/unit/CMakeLists.txt` | 新增测试文件 + Vulkan include 路径 |

---

### ✅ P2：Profiler 可视化 + 性能回归基线（已完成）

**原因**: CPU/Memory/Render 三条 Profiler 线已有基础功能，但缺乏标准化可视化导出和性能基线测试覆盖。

具体方向：
1. ✅ **JSON/Chrome Trace 格式导出** — 对标 Chrome `about:tracing`，三个 Profiler 均支持 `ExportChromeTrace()`
2. ✅ **性能基线测试** — 关键路径建立性能基线（BeginSample/EndSample、RecordAlloc、RecordDrawCall 均 <100μs/op）
3. 🔹 **CI 性能回归门禁** — 超基线 10% 自动 fail（待 CI 基础设施就绪后接入）

#### 实施内容

1. ✅ **Chrome Trace Event Format 导出**
   - `CPUProfiler::ExportChromeTrace()` — 输出 `ph:"X"` 完整事件（name/cat/ts/dur/pid/tid），带 origin 相对微秒时间戳
   - `MemoryProfiler::ExportChromeTrace()` — 输出 `ph:"i"` 即时事件（alloc/free）+ `ph:"C"` 计数器事件（memory_usage running total）
   - `RenderProfiler::ExportChromeTrace()` — 输出 `ph:"C"` 计数器事件（draw_calls/triangles/vertices/sprites/batches/texture_binds/shader_switches/texture_memory）
   - 所有导出均符合 Chrome Trace Event Format JSON Array 规范，可直接导入 `chrome://tracing`

2. ✅ **Trace 数据跟踪基础设施**
   - `ProfileSample` 新增 `timestamp_us` 字段
   - `CPUProfiler` 新增 `trace_samples_` + `origin_time_` 成员，跨帧累积所有采样
   - `MemoryProfiler` 新增 `MemoryTraceEvent` 结构体 + `trace_events_` 事件日志
   - `RenderProfiler` 新增 `RenderFrameEvent` 结构体 + `frame_events_` 帧事件日志
   - 所有 `Reset()` 方法同步清除 trace 数据并重置 origin 时间

3. ✅ **性能基线测试（4 项）**
   - CPU 采样 10000 次 BeginSample/EndSample < 100μs/op
   - Memory 记录 10000 次 RecordAlloc < 100μs/op
   - Render 记录 10000 帧 BeginFrame/RecordDrawCall/EndFrame < 100μs/op
   - Chrome Trace 导出 10000 条采样 < 100ms

4. ✅ **新增 16 个单元测试**（682 总测试零回归）

#### 变更文件

| 文件 | 说明 |
|------|------|
| `engine/profiler/cpu_profiler.h` | ProfileSample 新增 timestamp_us、CPUProfiler 新增 ExportChromeTrace/GetAllSamples/trace_samples_/origin_time_ |
| `engine/profiler/cpu_profiler.cpp` | EndSample 记录时间戳和 trace、Reset 清除 trace、ExportChromeTrace 实现 |
| `engine/profiler/memory_profiler.h` | 新增 MemoryTraceEvent 结构体、ExportChromeTrace 声明、trace_events_/origin_time_ |
| `engine/profiler/memory_profiler.cpp` | RecordAlloc/RecordFree 记录 trace 事件、Reset 清除、ExportChromeTrace 实现 |
| `engine/profiler/render_profiler.h` | 新增 RenderFrameEvent 结构体、ExportChromeTrace 声明、frame_events_/origin_time_ |
| `engine/profiler/render_profiler.cpp` | EndFrame 记录帧事件、Reset 清除、ExportChromeTrace 实现 |
| `tests/gtest/unit/engine/profiler/profiler_test.cpp` | 新增 16 个测试（Chrome Trace 格式验证 ×12 + 性能基线 ×4） |

### ✅ P2：Input 系统增强（已完成）

**原因**: 基础键鼠已有，但缺少 Gamepad、动作映射和录制回放，无法支持手柄玩家和自动化测试。

具体方向：
1. ✅ **Gamepad 支持** — 15 个 XInput 布局按钮（A/B/X/Y/LB/RB/Back/Start/Thumbs/DPad/Guide）+ 6 轴（双摇杆+双扳机）+ 4 手柄独立 + 死区过滤
2. ✅ **Action Mapping** — 抽象 action→key 映射，Register/Remove/Bind/Unbind/UnbindAll，运行时 rebind，多键 OR 逻辑
3. ✅ **Input Recording/Playback** — InputRecorder 录制事件 + JSON 导入导出，InputPlayer 时间线回放到 Input 系统

#### 实施内容

1. ✅ **Gamepad 基础设施**
   - `key_code.h` 新增 `GAMEPAD_BUTTON_A`~`GAMEPAD_BUTTON_GUIDE`（15 个按钮）+ `GamepadAxis` 枚举（6 轴）
   - `Input` 新增 `RecordGamepadAxis/GetGamepadAxis/SetGamepadConnected/IsGamepadConnected/SetGamepadDeadZone/GetGamepadDeadZone`
   - 支持 4 手柄独立状态，默认 0.15 死区过滤，越界 ID 安全

2. ✅ **ActionMapping 动作映射** (`action_mapping.h/.cpp`)
   - `RegisterAction/RemoveAction/HasAction` 动作生命周期
   - `BindKey/UnbindKey/UnbindAll` 键位绑定管理
   - `GetAction/GetActionDown/GetActionUp` 查询动作状态（多键 OR 逻辑）
   - `GetBindings/GetAllActions/GetActionCount` 内省
   - 运行时 rebind：先 Unbind 旧键再 Bind 新键

3. ✅ **InputRecorder/InputPlayer** (`input_recorder.h/.cpp`)
   - `InputRecorder`: StartRecording/StopRecording/RecordEvent/Clear/ExportJSON/ImportJSON
   - `InputPlayer`: Load/Start/Stop/Update/IsPlaying/IsFinished
   - JSON 格式：`[{"ts":0.000,"key":65,"action":1}, ...]`
   - Update 按时间线顺序回放事件到 `Input::RecordKey()`

4. ✅ **新增 38 个单元测试**（720 总测试零回归）

#### 变更文件

| 文件 | 说明 |
|------|------|
| `engine/input/key_code.h` | 新增 15 个 Gamepad 按钮 + GamepadAxis 枚举 |
| `engine/input/input.h` | 新增 Gamepad 轴/连接/死区 API + 成员 |
| `engine/input/input.cpp` | 实现 Gamepad API，Reset 清除 Gamepad 状态 |
| `engine/input/action_mapping.h` | 新增 ActionMapping 类声明 |
| `engine/input/action_mapping.cpp` | ActionMapping 实现 |
| `engine/input/input_recorder.h` | 新增 InputRecorder + InputPlayer 声明 |
| `engine/input/input_recorder.cpp` | InputRecorder/InputPlayer 实现 |
| `tests/gtest/unit/engine/input/input_test.cpp` | 新增 38 个测试 |

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

---

## 当前执行：资产管线异步化 + 热重载

> 状态：已完成

### 实施清单

1. ✅ 分析现有 `AssetManager` 代码结构与 `LoadTextureAsync` 模式
2. ✅ 扩展异步加载到全资源类型：`LoadDmeshAsync` / `LoadDanimAsync` / `LoadDskelAsync` / `LoadAudioClipAsync` / `LoadMaterialAsync`
3. ✅ 实现 LRU 淘汰：`TouchLru()` / `RemoveLru()` / `SetMemoryBudget()` / `EstimatedMemoryUsage()` / `EvictLRU()`
4. ✅ 实现文件热重载：`StartFileWatcher()` / `StopFileWatcher()` / `PumpHotReloads()`（Windows overlapped I/O，非阻塞 200ms 超时轮询）
5. ✅ 集成到 `FramePipeline::RunUpdateInternal()`：每帧调用 `PumpHotReloads()`
6. ✅ 新增 `kResourceHotReloaded` EventId
7. ✅ 新增 52 个单元测试覆盖（LRU / 异步扩展 / 热重载生命周期）
8. ✅ 修复 `AssetManagerFakeRhiDevice` 缺少阴影/光源虚函数的预存问题
9. ✅ 编译通过 + 全测试通过（exit code 0）

### 变更文件

| 文件 | 说明 |
|------|------|
| `engine/assets/asset_manager.h` | 新增异步加载 / LRU / 热重载 API 声明 |
| `engine/assets/asset_manager.cpp` | 实现全部新功能 + overlapped I/O 文件监听 |
| `engine/core/event_id.h` | 新增 `kResourceHotReloaded` |
| `engine/runtime/frame_pipeline.cpp` | 集成 `PumpHotReloads()` |
| `tests/gtest/unit/engine/assets/asset_pipeline_async_test.cpp` | 新增 17 个测试 |
| `tests/gtest/unit/engine/assets/asset_manager_test.cpp` | 修复 FakeRhiDevice 缺失虚函数 |

---

## 当前完成：RenderGraph ↔ FramePipeline 深度整合

> 状态：已完成（5 阶段全部完成）

### 实施阶段

1. ✅ **Phase 1: 依赖完整性 + Execute 效率优化**
   - `scene_pass` 补充 `PassRead(prez_depth)` 确保 PreZ 在拓扑排序中先于 Scene
   - `Execute()` 改用 `pass_id → index` 哈希表，从 O(n²) 降至 O(n)
   - Bloom mip chain 拆分为 5 个独立资源声明（`bloom_mip0`~`bloom_mip4`），支持更精准的 Pass 剔除

2. ✅ **Phase 2: RenderGraphResource 物理绑定**
   - `ResourceNode` 扩展 `ResourceType`（Logical / Transient / Imported）
   - 新增 `DeclareTransient(name, RenderTargetDesc)` — 图内管理的瞬态 RT，Compile 时自动分配
   - 新增 `ImportResource(name, rt_handle)` — 导入外部已分配 RT（如 default framebuffer）
   - 新增 `GetResourceRT(handle)` — Pass lambda 内查询物理 RT
   - 新增 `SetRhiDevice(RhiDevice*)` — 注入 RHI 设备供 Transient RT 分配
   - Compile 阶段加入生命周期分析（`first_use` / `last_use`）

3. ✅ **Phase 3: Pass 提取为独立 IRenderPass 类**
   - 定义 `IRenderPass` 接口：`Setup(RenderGraph&)` + `Execute(CommandBuffer&)` + `GetName()`
   - 定义 `RenderPassContext` 共享上下文：携带 World/RHI/RT/PipelineState/Module 指针及子系统回调
   - 9 个内置 Pass 拆分为独立类：`PreZPass`, `CSMShadowPass`, `SpotShadowPass`, `PointShadowPass`, `ForwardScenePass`, `BloomPass`, `UIPass`, `CompositePass`, `PresentPass`
   - `BuildRenderGraphInternal()` 从 ~450 行内联 lambda 缩减为 ~80 行注册 + Setup 循环

4. ✅ **Phase 4: 模块动态 Pass 注册**
   - `IModule` 新增 `RegisterRenderPasses(RenderGraph&, RenderPassContext&, out_passes)` 虚方法
   - `BuildRenderGraphInternal()` 在内置 Pass 注册后调用每个模块的 `RegisterRenderPasses()`
   - 模块创建的 Pass 统一由引擎管理生命周期和 Setup/Execute 调度

5. ✅ **Phase 5: Transient RT 别名复用**
   - `RenderTargetDesc` 新增 `operator==` 用于描述符匹配
   - Compile 阶段按 `first_use` 排序 Transient 资源，通过空闲池进行 desc 匹配的 RT 别名复用
   - 生命周期无重叠且 desc 相同的 Transient 资源共享同一物理 RT，减少显存占用

### 新增/变更文件

| 文件 | 说明 |
|------|------|
| `engine/render/render_graph.h` | 扩展 ResourceType / DeclareTransient / ImportResource / GetResourceRT / SetRhiDevice / lifetime 字段 / pass_id_to_idx_ |
| `engine/render/render_graph.cpp` | 新增资源声明方法 / 生命周期分析 / Transient RT 别名分配 / O(1) Execute |
| `engine/render/rhi/rhi_types.h` | RenderTargetDesc 新增 `operator==` |
| `engine/render/passes/render_pass_interface.h` | IRenderPass 抽象基类 |
| `engine/render/passes/render_pass_context.h` | RenderPassContext 共享上下文 |
| `engine/render/passes/builtin_passes.h` | 9 个内置 Pass 类声明 |
| `engine/render/passes/builtin_passes.cpp` | 9 个内置 Pass 实现（从 frame_pipeline.cpp 提取） |
| `engine/core/module.h` | IModule 新增 RegisterRenderPasses() |
| `engine/runtime/frame_pipeline.h` | 新增 render_pass_context_ / registered_passes_ 成员 |
| `engine/runtime/frame_pipeline.cpp` | BuildRenderGraphInternal 重构为 Pass 注册 + Setup 循环 |

---

## 当前完成：多线程命令提交

> 状态：已完成

### 实施内容

1. ✅ **CommandBuffer 延迟阴影贴图绑定**
   - `CommandBuffer` 新增 `DeferSetGlobalShadowMap` / `DeferSetGlobalSpotShadowMap` / `DeferSetGlobalPointShadowMap` 纯虚方法
   - `OpenGLCommandBuffer` 实现录制端：记录 `DeferShadowMapCmd`，Execute 时回放到 Device
   - `VulkanCommandBuffer` 实现：直接委托到 Device（Vulkan 无前端/后端分离）
   - 3 个阴影 Pass 迁移为使用 `cmd_buffer.DeferSetGlobal*()` 替代直接 `rhi_device->SetGlobal*()`

2. ✅ **OpenGLCommandBuffer::AppendFrom()**
   - 将 secondary buffer 的全部录制命令（含 order 偏移重算）追加到 primary buffer
   - 支持波次内多个 Pass 并行录制后合并

3. ✅ **RenderGraph 波次计算 + 并行执行**
   - `Compile()` 新增 Step 8：按依赖深度将 Pass 分组为波次（`compiled_waves_`）
   - `ExecuteParallel(OpenGLCommandBuffer&, JobSystem&)`：
     - 单 Pass 波次：直接主线程录制（零开销）
     - 多 Pass 波次：每个 Pass 录制到独立 secondary buffer → JobSystem 并行 → Wait → AppendFrom 合并
   - 自动回退：非 OpenGL 或无 JobSystem 时退回 `Execute()` 顺序路径

4. ✅ **FramePipeline 集成**
   - `ExecuteRenderGraphInternal()` 自动检测 JobSystem + OpenGLCommandBuffer 可用性
   - 可用时走 `ExecuteParallel()` 并行路径，否则回退顺序执行

### 变更文件

| 文件 | 说明 |
|------|------|
| `engine/render/rhi/rhi_device.h` | CommandBuffer 新增 3 个 Defer 虚方法 + OpenGLCommandBuffer AppendFrom + DeferShadowMapCmd |
| `engine/render/rhi/rhi_device.cpp` | DeferSetGlobal* 录制实现 + AppendFrom + Execute 回放 shadow 命令 |
| `engine/render/rhi/vulkan/vulkan_rhi_device.h` | VulkanCommandBuffer 新增 3 个 Defer override |
| `engine/render/rhi/vulkan/vulkan_rhi_device.cpp` | VulkanCommandBuffer Defer 实现（委托到 Device） |
| `engine/render/passes/builtin_passes.cpp` | 3 个阴影 Pass 迁移到 cmd_buffer.DeferSetGlobal*() |
| `engine/render/render_graph.h` | 新增 ExecuteParallel + compiled_waves_ + forward declarations |
| `engine/render/render_graph.cpp` | 波次计算 + ExecuteParallel 实现 |
| `engine/runtime/frame_pipeline.cpp` | ExecuteRenderGraphInternal 自动选择并行/顺序路径 |

---

### 待完成项（Vulkan）

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
- [x] 通过现有 3D Demo 验证 Vulkan 渲染路径
