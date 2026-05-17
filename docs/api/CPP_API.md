# DSEngine C++ 公共 API 参考

> **版本：** 与 `dse_version.h` 同步  
> **聚合头文件：** `#include "engine/dse.h"` 一次引入所有公共接口  
> **C++ 标准：** C++20（CMakeLists.txt 设定）

---

## 目录

1. [引擎生命周期](#1-引擎生命周期)
2. [核心服务](#2-核心服务)
3. [ECS 世界与实体](#3-ecs-世界与实体)
4. [资产管理](#4-资产管理)
5. [渲染系统](#5-渲染系统)
6. [输入系统](#6-输入系统)
7. [音频系统](#7-音频系统)
8. [场景管理](#8-场景管理)
9. [模块系统](#9-模块系统)
10. [帧流水线](#10-帧流水线)

---

## 1. 引擎生命周期

**头文件：** `engine/runtime/engine_app.h`

### EngineRunConfig

引擎启动配置结构体。

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `window_width` | `int` | 800 | 窗口宽度 |
| `window_height` | `int` | 600 | 窗口高度 |
| `window_title` | `string` | `"DSEngine Phase 2"` | 窗口标题 |
| `business_mode` | `BusinessMode` | `Lua` | 业务逻辑模式（Lua / C++） |
| `enable_editor` | `bool` | `false` | 是否启用编辑器模式 |
| `startup_lua_script_path` | `string` | `""` | Lua 入口脚本路径 |
| `services` | `RuntimeServices` | — | 可注入的运行时服务 |
| `world` | `World*` | `nullptr` | 向后兼容别名 |
| `asset_manager` | `AssetManager*` | `nullptr` | 向后兼容别名 |
| `job_system` | `JobSystem*` | `nullptr` | 向后兼容别名 |

### EngineInstance

引擎应用运行实例，管理生命周期、服务装配与主循环。

```cpp
dse::runtime::EngineRunConfig config;
config.window_width = 1280;
config.window_height = 720;
config.business_mode = dse::runtime::BusinessMode::Lua;
config.startup_lua_script_path = "script/application.lua";

dse::runtime::EngineInstance engine(config);
int exit_code = engine.Run();  // 完整生命周期
```

| 方法 | 签名 | 说明 |
|------|------|------|
| `Init()` | `bool Init()` | 初始化引擎（不含主循环） |
| `Tick()` | `void Tick()` | 单帧更新 |
| `Shutdown()` | `void Shutdown()` | 清理资源 |
| `Run()` | `int Run()` | 完整生命周期（Init→主循环→Shutdown），返回退出码 |
| `pipeline()` | `FramePipeline*` | 获取帧流水线 |
| `service_locator()` | `ServiceLocator&` | 获取全局服务定位器 |

**便捷函数：**

```cpp
int dse::runtime::RunEngine(const EngineRunConfig& config);
```

### GameApplication（C++ 宿主便捷基类）

**头文件：** `engine/scripting/cpp/game_application.h`  
**命名空间：** `dse::runtime`

继承此类可快速搭建 C++ 游戏，无需手动注册 `CppBusinessHooks`。

```cpp
class MyGame : public dse::runtime::GameApplication {
protected:
    void OnInit() override {
        auto cam = CreateCamera3D({0, 5, 15});
        auto sun = CreateDirectionalLight({-0.5f, -1.0f, -0.3f});
        auto box = CreateMesh({0, 0, 0}, "models/cube.dmesh");
    }
    void OnUpdate(float dt) override { }
    void OnShutdown() override { }
};
int main() { return MyGame().Run({.window_width=1280, .window_height=720}); }
```

**生命周期钩子：**

| 虚方法 | 说明 |
|--------|------|
| `OnInit()` | 引擎初始化完毕后调用 |
| `OnUpdate(float dt)` | 每帧调用 |
| `OnShutdown()` | 引擎关停前调用 |

**服务访问：**

| 方法 | 返回类型 | 说明 |
|------|----------|------|
| `GetWorld()` | `World&` | 获取 ECS 世界 |
| `GetAssetManager()` | `AssetManager&` | 获取资产管理器 |

**ECS 操作：**

| 方法 | 说明 |
|------|------|
| `CreateEntity()` | 创建空实体 |
| `DestroyEntity(e)` | 销毁实体 |
| `Emplace<T>(e, args...)` | 添加/替换组件 |
| `Get<T>(e)` | 获取组件指针（无则返回 nullptr） |
| `Has<T>(e)` | 是否拥有组件 |
| `Remove<T>(e)` | 移除组件 |

**实体工厂：**

| 方法 | 说明 |
|------|------|
| `CreateEntityAt(pos, scale)` | 创建带 Transform 的实体 |
| `CreateCamera3D(pos, fov, near, far)` | 创建 3D 相机（含 FreeCameraController） |
| `CreateDirectionalLight(dir, color, intensity, shadow)` | 创建平行光 |
| `CreatePointLight(pos, color, intensity, radius)` | 创建点光源 |
| `CreateMesh(pos, path, scale)` | 创建网格实体（PBR 默认材质） |
| `LoadTexture(path)` | 加载纹理，返回 handle |

### DSE_ENABLE_LUA 条件编译

CMake 选项 `DSE_ENABLE_LUA`（默认 `ON`）控制 Lua 运行时是否编入引擎：

```bash
# 纯 C++ 构建，裁剪 Lua 解释器 + 全部绑定代码
cmake -S . -B build -DDSE_ENABLE_LUA=OFF
```

| 开关状态 | 效果 |
|----------|------|
| `ON`（默认） | Lua 解释器 + 340+ 绑定函数编入 `DSEngine.dll` |
| `OFF` | 排除 `depends/lua/*` + `engine/scripting/lua/*.cpp`，二进制更小 |

> 注意：`DSE_ENABLE_LUA=OFF` 时若 `BusinessMode` 设为 `Lua`，引擎将输出错误日志并启动失败。

---

## 2. 核心服务

### 2.1 ServiceLocator

**头文件：** `engine/core/service_locator.h`  
**命名空间：** `dse::core`

全局服务容器，线程安全。替代全局单例的依赖管理容器。

```cpp
auto& locator = dse::core::ServiceLocator::Instance();

// 注册
locator.Register<JobSystem, JobSystem>(std::make_shared<JobSystem>());
locator.Emplace<EventBus, EventBus>();  // 便捷：直接构造

// 获取
auto* job_sys = locator.Get<JobSystem>();          // 原始指针
auto  job_sp  = locator.GetShared<JobSystem>();    // shared_ptr

// 查询 & 重置
bool has = locator.Has<JobSystem>();
locator.Reset<JobSystem>();   // 单个
locator.ResetAll();           // 全部（Shutdown 时）

// 桥接到另一个 locator
locator.BridgeTo<JobSystem>(other_locator);
```

| 方法 | 说明 |
|------|------|
| `Instance()` | 获取全局单例 |
| `Register<TInterface, TImpl>(shared_ptr)` | 注册服务 |
| `Emplace<TInterface, TImpl>(args...)` | 便捷注册（直接构造） |
| `Get<T>()` → `T*` | 获取服务（原始指针，nullptr=未注册） |
| `GetShared<T>()` → `shared_ptr<T>` | 获取服务（共享指针） |
| `Has<T>()` → `bool` | 检查是否已注册 |
| `Reset<T>()` | 移除指定服务 |
| `ResetAll()` | 移除全部服务 |
| `BridgeTo<T>(target)` → `bool` | 桥接到另一个 locator |

### 2.2 EventBus

**头文件：** `engine/core/event_bus.h`  
**命名空间：** `dse::core`

类型安全的发布-订阅事件总线。使用编译期 FNV-1a 哈希的 `EventId` 确保跨 DLL 安全。

```cpp
auto* bus = ServiceLocator::Instance().Get<EventBus>();

// 订阅
auto handle = bus->Subscribe<UiClickEvent>([](const UiClickEvent& e) {
    LOG_INFO("UI clicked: entity {}", e.entity);
});

// 发布
bus->Publish<UiClickEvent>(entity_id);

// 取消订阅
bus->Unsubscribe(handle);
```

**内建事件类型：**

| 事件 | 字段 | 说明 |
|------|------|------|
| `UiClickEvent` | `uint32_t entity` | UI 点击 |
| `ResourceLoadedEvent` | `string path`, `bool success` | 资源加载完成 |
| `SceneLifecycleEvent` | `SceneLifecyclePhase phase` | 场景初始化/关闭 |
| `SubSceneLoadedEvent` | `string path` | 子场景加载完成 |
| `SubSceneUnloadedEvent` | `string path` | 子场景卸载完成 |

**自定义事件：**

```cpp
struct MyEvent : public dse::core::Event {
    int data;
    explicit MyEvent(int d) : data(d) {}
    static constexpr EventId kEventId = dse::core::events::MakeEventId("MyEvent");
};
```

### 2.3 JobSystem

**头文件：** `engine/core/job_system.h`  
**命名空间：** `dse::core`

基于线程池的异步任务系统，支持三级优先级和任务依赖链。

```cpp
auto* job_sys = ServiceLocator::Instance().Get<JobSystem>();

// 简单异步
job_sys->Execute([] { /* 后台工作 */ });

// 带优先级
auto h = job_sys->Submit(task, JobPriority::High);

// 带依赖
auto physics = job_sys->Submit(physics_task, JobPriority::High);
auto render  = job_sys->SubmitWithDependency(render_task, {physics});
job_sys->Wait(render);
```

| 方法 | 说明 |
|------|------|
| `Init()` | 初始化线程池 |
| `Shutdown()` | 关闭，等待所有任务完成 |
| `Execute(job)` | 提交 Normal 优先级任务（兼容旧接口） |
| `Submit(job, priority)` → `JobHandle` | 提交带优先级任务 |
| `SubmitWithDependency(job, deps, priority)` → `JobHandle` | 提交带依赖任务 |
| `Wait(handle)` | 等待指定任务完成 |

**优先级：** `JobPriority::Low` / `Normal` / `High`

---

## 3. ECS 世界与实体

**头文件：** `engine/ecs/world.h`

基于 EnTT 的实体组件系统。`World` 封装 `entt::registry`，不再强制单例。

```cpp
auto* world = ServiceLocator::Instance().Get<World>();

Entity e = world->CreateEntity();
world->registry().emplace<TransformComponent>(e);
world->registry().emplace<SpriteRendererComponent>(e);

bool alive = world->IsAlive(e);
size_t count = world->EntityCount();

world->DestroyEntity(e);
world->Clear();
```

| 方法 | 签名 | 说明 |
|------|------|------|
| `CreateEntity()` | `Entity` | 创建空实体 |
| `DestroyEntity(e)` | `void` | 销毁实体及其所有组件 |
| `Clear()` | `void` | 清空世界 |
| `IsAlive(e)` | `bool` | 实体是否存活 |
| `EntityCount()` | `size_t` | 当前存活实体数 |
| `registry()` | `entt::registry&` | 底层 EnTT registry（组件操作） |

### 组件定义

组件定义在以下头文件中，全部为 POD-like 结构体，通过 `registry().emplace<T>()` 挂载：

| 头文件 | 包含组件 |
|--------|---------|
| `engine/ecs/transform.h` | `TransformComponent` |
| `engine/ecs/camera.h` | `CameraComponent`, `Camera3DComponent` |
| `engine/ecs/sprite.h` | `SpriteRendererComponent` |
| `engine/ecs/components_2d.h` | `SteeringComponent`, `TilemapComponent`, `Physics2DCollisionEvent` 等 |
| `engine/ecs/components_3d.h` | `MeshRendererComponent`, `PointLightComponent`, `SpotLightComponent`, `DirectionalLightComponent`, `SkyboxComponent`, `TerrainComponent`, `WaterComponent`, `PostProcessSettingsComponent`, `DecalComponent`, `LODGroupComponent`, `GrassComponent`, `HairComponent`, `LightProbeComponent`, `ReflectionProbeComponent`, `MorphComponent` 等 |
| `engine/ecs/components_3d_particle.h` | `ParticleSystem3DComponent` |
| `engine/ecs/components_3d_physics.h` | `RigidBody3DComponent`, `BoxCollider3DComponent`, `SphereCollider3DComponent`, `CapsuleCollider3DComponent`, `MeshCollider3DComponent` |
| `engine/ecs/animation.h` | `AnimatorComponent`, `Animator3DComponent`, `AnimLayerComponent`, `IKComponent` |
| `engine/ecs/particle_2d.h` | `ParticleEmitterComponent` |
| `engine/ecs/physics_2d.h` | `RigidBody2DComponent`, `BoxCollider2DComponent`, `CircleCollider2DComponent` |
| `engine/ecs/ui.h` | `UiRendererComponent`, `UiLabelComponent`, `UiPanelComponent`, `UiButtonComponent`, `UIAnchorComponent`, `UIGridLayoutComponent`, `UICanvasScalerComponent`, `UIAnimationComponent` 等 |
| `engine/ecs/gameplay.h` | `GameplayTuningComponent`, `NavMeshAgentComponent` |
| `engine/ecs/audio.h` | `AudioSourceComponent`, `AudioListenerComponent` |
| `engine/ecs/script.h` | `ScriptComponent` |
| `engine/ecs/uuid_component.h` | `UUIDComponent` |

---

## 4. 资产管理

**头文件：** `engine/assets/asset_manager.h`

### AssetManager

统一的资源加载、缓存和生命周期管理。

```cpp
auto* am = ServiceLocator::Instance().Get<AssetManager>();
am->SetRhiDevice(rhi_device);
am->ConfigureDataRoot("data");

// 同步加载
auto tex = am->LoadTexture("textures/brick.png");
auto clip = am->LoadAudioClip("audio/bgm.wav");
auto mesh = am->LoadDmesh("models/hero.dmesh");
auto cubemap = am->LoadCubemap("skybox/");  // 智能检测：目录/十字展开/全景图

// 异步加载（IO 在 JobSystem 工作线程，GPU 上传在主线程 pump）
am->LoadTextureAsync("textures/big.png", [](auto tex) { /* 回调 */ });
am->PumpMainThreadCallbacks();  // 每帧主线程调用
```

#### 同步加载

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `LoadTexture(path)` | `shared_ptr<TextureAsset>` | 加载纹理 |
| `LoadCubemap(path)` | `shared_ptr<CubemapAsset>` | 智能加载立方体贴图 |
| `LoadCubemapDirectory(path)` | `shared_ptr<CubemapAsset>` | 从六面图目录加载 |
| `LoadCubemapPanorama(path, face_size=512)` | `shared_ptr<CubemapAsset>` | 从全景图加载 |
| `LoadCubemapCross(path)` | `shared_ptr<CubemapAsset>` | 从十字展开图加载 |
| `LoadShader(name, vert, frag)` | `shared_ptr<ShaderAsset>` | 加载着色器 |
| `LoadAudioClip(path)` | `shared_ptr<AudioClipAsset>` | 加载音频 |
| `LoadDmesh(path)` | `shared_ptr<DmeshAsset>` | 加载网格 |
| `LoadDanim(path)` | `shared_ptr<DanimAsset>` | 加载动画 |
| `LoadDskel(path)` | `shared_ptr<DskelAsset>` | 加载骨架 |

#### 异步加载

| 方法 | 说明 |
|------|------|
| `LoadTextureAsync(path, callback)` | 异步加载纹理 |
| `LoadDmeshAsync(path, callback)` | 异步加载网格 |
| `LoadDanimAsync(path, callback)` | 异步加载动画 |
| `LoadDskelAsync(path, callback)` | 异步加载骨架 |
| `LoadAudioClipAsync(path, callback)` | 异步加载音频 |
| `LoadMaterialAsync(dmat_path, index, callback)` | 异步加载材质 |
| `PumpMainThreadCallbacks(max)` | 主线程处理完成回调 |

#### 材质管理

| 方法 | 说明 |
|------|------|
| `CreateMaterialInstance(name)` → `shared_ptr<MaterialAsset>` | 创建空白材质实例 |
| `LoadMaterialInstanceFromDmat(path, index)` → `shared_ptr<MaterialAsset>` | 从 .dmat 文件加载 |
| `GetMaterialInstance(id)` → `shared_ptr<MaterialAsset>` | 按 ID 获取 |
| `ListMaterialInstanceIds()` → `vector<unsigned int>` | 列出所有材质 ID |

#### 内存管理与热重载

| 方法 | 说明 |
|------|------|
| `SetMemoryBudget(bytes)` | 设置内存预算，超出按 LRU 淘汰 |
| `EstimatedMemoryUsage()` → `size_t` | 当前内存估算 |
| `EvictLRU()` → `size_t` | 手动触发 LRU 淘汰 |
| `UnloadUnused()` | 卸载无引用资源 |
| `StartFileWatcher()` | 启动热重载监听 |
| `StopFileWatcher()` | 停止热重载监听 |
| `PumpHotReloads()` → `size_t` | 主线程处理热重载 |

#### Asset Bundle / Pak

| 方法 | 说明 |
|------|------|
| `PackBundle(input_dir, output, aes_key)` → `bool` | 打包加密资产包 |
| `MountBundle(path, aes_key)` → `bool` | 挂载资产包到 VFS |
| `MountPak(path)` → `bool` | 挂载 .dpak 归档 |
| `NormalizeAssetPath(path)` → `string` | 规范化为逻辑路径 |
| `ResolveAssetPath(path)` → `string` | 解析为磁盘可访问路径 |

### 资产类型

| 类 | 主要方法 |
|----|---------|
| `TextureAsset` | `GetHandle()`, `GetWidth()`, `GetHeight()`, `GetChannels()` |
| `CubemapAsset` | `GetHandle()`, `GetWidth()`, `GetHeight()`, `GetPath()` |
| `ShaderAsset` | `GetHandle()` |
| `DmeshAsset` | `GetPath()`, `GetData()` |
| `DanimAsset` | `GetPath()`, `GetData()` |
| `DskelAsset` | `GetPath()`, `GetData()` |
| `AudioClipAsset` | `GetPath()`, `GetData()` |
| `MaterialAsset` | `GetId()`, `GetName()`, `GetShaderVariant()`, `GetBaseColor()`, `GetTextureSlots()`, `GetScalarOverrides()`, `GetBlendMode()` 等 |

---

## 5. 渲染系统

### 5.1 RhiDevice（渲染硬件接口）

**头文件：** `engine/render/rhi/rhi_device.h`（纯虚基类 + `CommandBuffer` + `OpenGLCommandBuffer`）

抽象基类，三个后端实现位于各自头文件：
- `OpenGLRhiDevice` → `engine/render/rhi/gl_rhi_device.h`
- `VulkanRhiDevice` → `engine/render/rhi/vulkan/vulkan_rhi_device.h`（条件编译 `DSE_ENABLE_VULKAN`）
- `DX11RhiDevice` → `engine/render/rhi/dx11/dx11_rhi_device.h`（条件编译 `DSE_ENABLE_D3D11`）

#### GPU 资源创建

| 方法 | 说明 |
|------|------|
| `CreateTexture2D(w, h, rgba8, linear)` → `uint` | 创建 2D 纹理 |
| `CreateTextureCube(w, h, faces[6], linear)` → `uint` | 创建立方体贴图 |
| `CreateTexture3D(w, h, d, rgba8, linear)` → `uint` | 创建 3D 纹理 |
| `CreateCompressedTexture2D(format, mips, linear)` → `uint` | 创建压缩纹理 |
| `DeleteTexture(handle)` | 删除纹理 |
| `CreateRenderTarget(desc)` → `uint` | 创建渲染目标 |
| `GetRenderTargetColorTexture(rt)` → `uint` | 获取 RT 颜色纹理 |
| `GetRenderTargetDepthTexture(rt)` → `uint` | 获取 RT 深度纹理 |
| `CreateShaderProgram(vert, frag)` → `uint` | 创建着色器程序 |
| `CreatePipelineState(desc)` → `uint` | 创建管线状态 |
| `CreateBuffer(size, data, dynamic, index)` → `uint` | 创建 GPU 缓冲 |
| `CreateCommandBuffer()` → `shared_ptr<CommandBuffer>` | 创建命令缓冲 |

#### SSBO / Compute

| 方法 | 说明 |
|------|------|
| `CreateSSBO(size, data)` → `uint` | 创建 SSBO |
| `UpdateSSBO(handle, offset, size, data)` | 更新 SSBO |
| `BindSSBO(handle, binding)` | 绑定 SSBO |
| `SupportsSSBO()` → `bool` | 是否支持 SSBO |
| `CreateComputeShader(source)` → `uint` | 创建 Compute Shader |
| `DispatchCompute(shader, gx, gy, gz)` | 调度 Compute |
| `ComputeMemoryBarrier()` | 插入内存屏障 |
| `SupportsCompute()` → `bool` | 是否支持 Compute |

#### 阴影 / 光源全局状态

| 方法 | 说明 |
|------|------|
| `SetGlobalShadowMap(index, handle)` | 绑定 CSM 阴影贴图 |
| `SetGlobalSpotShadowMap(index, handle)` | 绑定聚光灯阴影贴图 |
| `SetGlobalPointShadowMap(index, handle)` | 绑定点光阴影贴图 |
| `SetGlobalLightSpaceMatrix(index, mat)` | 设置光空间矩阵 |
| `SetGlobalCascadeSplit(index, split)` | 设置级联分割距离 |
| `SetGlobalLightProbeSH(sh[9], enabled)` | 设置 Light Probe SH |

#### 平台差异

| 方法 | 说明 |
|------|------|
| `NeedsTextureYFlip()` → `bool` | 加载时是否需要 Y 翻转 |
| `NeedsReadbackYFlip()` → `bool` | 回读时是否需要 Y 翻转 |
| `GetProjectionCorrection()` → `mat4` | 投影矫正矩阵（GL=identity, VK=Y-flip+Z-remap, DX=Z-remap） |
| `GetShadowSampleCorrection()` → `mat4` | 阴影采样矫正矩阵 |

### 5.2 CommandBuffer

**头文件：** `engine/render/rhi/rhi_device.h`

命令缓冲抽象类，每帧录制渲染命令后提交。

| 虚方法 | 说明 |
|--------|------|
| `BeginRenderPass(desc)` / `EndRenderPass()` | 渲染 Pass 边界 |
| `SetPipelineState(handle)` | 设置管线状态 |
| `SetCamera(view, proj)` | 设置相机矩阵 |
| `DrawBatch(items)` | 绘制 2D 批次 |
| `DrawMeshBatch(items)` | 绘制 3D 网格批次 |
| `DrawSpriteBatch(items)` | 绘制精灵批次 |
| `DrawSkybox(cubemap)` | 绘制天空盒 |
| `DrawPostProcess(src, effect, params)` | 后处理 |
| `DrawParticles3D(items, view, proj)` | 绘制 3D 粒子 |
| `DrawHairStrands(items, view, proj)` | 绘制毛发 |
| `ClearColor(color)` | 清屏 |
| `SetGlobalMat4(name, mat)` | 设置全局 mat4 |
| `SetGlobalMat4Array(name, mats)` | 设置全局 mat4 数组 |

### 5.3 RenderGraph（渲染图）

**头文件：** `engine/render/render_graph.h`  
**命名空间：** `dse::render`

基于 DAG 的渲染图，自动推断依赖、拓扑排序、无用 Pass 剔除。

```cpp
dse::render::RenderGraph graph;
graph.SetRhiDevice(rhi_device);

auto shadow = graph.DeclareResource("shadow_depth");
auto color  = graph.DeclareResource("scene_color");

auto p1 = graph.AddPass("ShadowMap");
graph.PassWrite(p1, shadow);
graph.PassSetExecute(p1, [&](CommandBuffer& cmd) { /* 阴影渲染 */ });

auto p2 = graph.AddPass("Forward");
graph.PassRead(p2, shadow);
graph.PassWrite(p2, color);
graph.PassSetExecute(p2, [&](CommandBuffer& cmd) { /* 前向渲染 */ });

graph.MarkOutput(color);
graph.Compile();
graph.Execute(cmd_buffer);
```

| 方法 | 说明 |
|------|------|
| `DeclareResource(name)` → `RenderResourceHandle` | 声明逻辑资源 |
| `DeclareTransient(name, desc)` → `RenderResourceHandle` | 声明瞬态 RT |
| `ImportResource(name, handle)` → `RenderResourceHandle` | 导入外部 RT |
| `GetResourceRT(resource)` → `uint` | 查询物理 RT 句柄 |
| `AddPass(name)` → `RenderPassHandle` | 添加 Pass |
| `PassRead(pass, resource)` | 声明读取依赖 |
| `PassWrite(pass, resource)` | 声明写入依赖 |
| `PassSetExecute(pass, fn)` | 设置执行函数 |
| `MarkOutput(resource)` | 标记外部输出（不被剔除） |
| `Compile()` → `bool` | 编译（排序+剔除） |
| `Execute(cmd)` | 顺序执行 |
| `ExecuteParallel(primary, job_system)` | 并行执行 |
| `Reset()` | 重置 |

### 5.4 IRenderPass（渲染 Pass 接口）

**头文件：** `engine/render/passes/render_pass_interface.h`  
**命名空间：** `dse::render`

所有具体渲染 Pass 的基类。

```cpp
class MyPass : public dse::render::IRenderPass {
public:
    void Setup(RenderGraph& graph) override {
        // 声明资源依赖
    }
    void Execute(CommandBuffer& cmd) override {
        // 录制渲染命令
    }
    const char* GetName() const override { return "MyPass"; }
};
```

| 虚方法 | 说明 |
|--------|------|
| `Setup(graph)` | 在 RenderGraph 上声明读写资源 |
| `Execute(cmd)` | 录制渲染命令 |
| `GetName()` → `const char*` | Pass 名称（调试用） |

---

## 6. 输入系统

**头文件：** `engine/input/input.h`

全局静态类，无需实例化。

### 键盘

| 方法 | 说明 |
|------|------|
| `Input::GetKey(code)` → `bool` | 当前帧按键是否按下 |
| `Input::GetKeyDown(code)` → `bool` | 当前帧刚按下 |
| `Input::GetKeyUp(code)` → `bool` | 当前帧刚松开 |
| `Input::GetDoubleClick(code)` → `bool` | 当前帧双击 |
| `Input::GetLongPress(code, sec)` → `bool` | 长按检测 |

### 鼠标

| 方法 | 说明 |
|------|------|
| `Input::GetMouseButton(index)` → `bool` | 鼠标按钮持续按下 (0=左, 1=右, 2=中) |
| `Input::GetMouseButtonDown(index)` → `bool` | 当前帧刚按下 |
| `Input::GetMouseButtonUp(index)` → `bool` | 当前帧刚松开 |
| `Input::mousePosition()` → `vec2` | 鼠标位置 |
| `Input::mouseScroll()` → `float` | 滚轮值 |
| `Input::GetSwipeDelta()` → `vec2` | 滑动增量（像素） |

### 手柄

| 方法 | 说明 |
|------|------|
| `Input::GetGamepadAxis(id, axis)` → `float` | 获取手柄轴值 |
| `Input::IsGamepadConnected(id)` → `bool` | 手柄是否连接 |
| `Input::SetGamepadDeadZone(zone)` | 设置死区阈值 |

### 其他

| 方法 | 说明 |
|------|------|
| `Input::IsDeviceShaking()` → `bool` | 设备摇晃检测 |
| `Input::Update()` | 帧结束时重置状态（引擎自动调用） |
| `Input::Reset()` | 完全重置所有输入状态（测试用） |

**相关头文件：**
- `engine/input/key_code.h` — 键码常量定义
- `engine/input/action_mapping.h` — 动作映射系统
- `engine/input/input_recorder.h` — 输入录制/回放

---

## 7. 音频系统

**头文件：** `engine/audio/audio_system.h`  
**命名空间：** `dse::gameplay2d`

基于 miniaudio 的音频引擎。

```cpp
AudioSystem audio;
audio.Initialize(asset_manager);

audio.PlayBgm("data/audio/bgm.ogg", 0.8f);
audio.PlaySfx("data/audio/hit.wav", 1.0f);
audio.SetMasterVolume(0.9f);

// 混音总线
auto& bus_mgr = audio.GetBusManager();
// ...

audio.Shutdown();
```

| 方法 | 说明 |
|------|------|
| `Initialize(asset_manager)` → `bool` | 初始化 |
| `Update(registry, dt)` | 每帧更新 ECS 音频组件 |
| `Shutdown()` | 关闭 |
| `PlaySound(path, volume)` | 播放单次音效 |
| `PlaySfx(path, volume, loop)` | 播放 SFX |
| `PlayBgm(path, volume, loop)` → `bool` | 播放 BGM（替换当前） |
| `PauseBgm()` / `ResumeBgm()` / `StopBgm()` | BGM 控制 |
| `StopAllSfx()` | 停止所有 SFX |
| `SetMasterVolume(v)` | 设置主音量 |
| `SetBgmVolume(v)` | 设置 BGM 音量 |
| `SetSfxVolume(v)` | 设置 SFX 音量 |
| `SetEntityPitch(entity, pitch)` | 设置实体音源音高 |
| `SetMaxConcurrentSfxPerClip(n)` | 同名音效最大并发数 |
| `SetSfxTriggerCooldownMs(ms)` | SFX 触发冷却 |
| `SetRaycastFunction(func)` | 注入遮挡射线回调 |
| `GetBusManager()` → `AudioBusManager&` | 获取混音总线管理器 |

---

## 8. 场景管理

**头文件：** `engine/scene/scene_manager.h`  
**命名空间：** `scene`

管理多个子场景（SubScene），支持异步加载、场景切换与淡入淡出。

```cpp
auto* mgr = ServiceLocator::Instance().Get<scene::SceneManager>();
mgr->LoadSubSceneAsync("data/scenes/town.json");
mgr->TransitionTo("data/scenes/dungeon.json", scene::TransitionMode::Fade, 0.5f);
```

| 方法 | 说明 |
|------|------|
| `LoadSubScene(path)` → `bool` | 同步加载子场景 |
| `LoadSubSceneAsync(path)` | 异步加载（IO 在 JobSystem） |
| `UnloadSubScene(path)` | 卸载子场景 |
| `UnloadAll()` | 卸载所有 |
| `Update(dt)` | 每帧 pump 异步加载 & 更新过渡状态 |
| `IsSubSceneLoaded(path)` → `bool` | 查询是否已加载 |
| `GetLoadedSubScenes()` → `vector<string>` | 获取已加载列表 |
| `LoadedCount()` / `PendingCount()` → `size_t` | 统计 |
| `TransitionTo(path, mode, fade_duration)` | 场景切换 |
| `GetTransitionState()` → `TransitionState` | 过渡状态 |
| `GetFadeProgress()` → `float` | 淡入淡出进度 [0,1] |
| `GetActiveScenePath()` → `string` | 当前活跃场景路径 |
| `ResolveReference(uuid)` → `Entity` | 跨场景 UUID 查找 |

**过渡模式：** `TransitionMode::Instant` / `Additive` / `Fade`  
**过渡状态：** `TransitionState::Idle` / `FadingOut` / `Loading` / `FadingIn`

---

## 9. 模块系统

**头文件：** `engine/core/module.h`  
**命名空间：** `dse::core`

动态模块接口，功能模块（如 3D/物理/网络）按需加载。

```cpp
class MyModule : public dse::core::IModule {
public:
    const char* GetName() const override { return "MyModule"; }
    bool OnInit(World& w, RhiDevice* rhi, AssetManager* am) override { return true; }
    void OnUpdate(World& w, float dt) override { /* 逻辑 */ }
    void OnFixedUpdate(World& w, float fdt) override { /* 物理 */ }
    void OnRenderScene(World& w, CommandBuffer& cmd, const glm::mat4& clip) override { /* 渲染 */ }
    void OnShutdown(World& w) override { /* 清理 */ }
    
    void RegisterRenderPasses(RenderGraph& graph, RenderPassContext& ctx,
        std::vector<std::unique_ptr<IRenderPass>>& out) override {
        // 注册自定义 RenderPass
    }
};
```

| 虚方法 | 说明 |
|--------|------|
| `GetName()` → `const char*` | 模块名称 |
| `OnInit(world, rhi, am)` → `bool` | 初始化 |
| `OnUpdate(world, dt)` | 逻辑帧更新 |
| `OnFixedUpdate(world, fdt)` | 固定步长更新 |
| `OnRenderPreZ(world, cmd)` | 深度预渲染 |
| `OnRenderShadow(world, cmd, cascade, view, proj)` | 阴影渲染 |
| `OnRenderScene(world, cmd, clip)` | 主场景渲染 |
| `OnRenderTransparent(world, cmd, clip, wboit_mode)` | WBOIT 透明渲染 |
| `OnRenderUI(world, cmd, w, h, clip)` | UI 渲染 |
| `RegisterRenderPasses(graph, ctx, out)` | 注册自定义 Pass |
| `OnShutdown(world)` | 关闭 |

---

## 10. 帧流水线

**头文件：** `engine/runtime/frame_pipeline.h`

引擎主循环流水线，协调渲染、物理、脚本等子系统。通常由 `EngineInstance` 自动管理，不需手动调用。

```cpp
auto* pipeline = engine.pipeline();
pipeline->SetWorld(world);
pipeline->SetAssetManager(asset_manager);
pipeline->Init();
// ... 由 EngineInstance::Tick() 自动驱动 ...
pipeline->Shutdown();
```

| 方法 | 说明 |
|------|------|
| `Init()` → `bool` | 初始化 |
| `Shutdown()` | 关闭 |
| `Update(dt)` | 逻辑更新 |
| `FixedUpdate(fdt)` | 固定步长更新 |
| `Render()` | 渲染 |
| `SetWorld(world)` | 注入 World |
| `SetAssetManager(am)` | 注入 AssetManager |
| `SetBusinessMode(mode)` | 设置业务模式 |
| `EnableEditorMode(enable)` | 编辑器模式（Init 前） |
| `SetEditorCamera(view, proj)` | 设置编辑器相机 |
| `DisableEditorCamera()` | 恢复游戏相机 |
| `GetSceneTextureId()` → `uint` | 场景纹理句柄（编辑器用） |
| `GetMainTextureId()` → `uint` | 最终合成纹理句柄 |
| `ReadSceneColorRgba8()` → `vector<uchar>` | 回读场景像素 |
| `LastDrawCalls()` → `int` | 上帧 DrawCall 数 |
| `LastSpriteCount()` → `int` | 上帧精灵数 |

---

## 附录：源文件索引

| 模块 | 头文件 |
|------|--------|
| 聚合入口 | `engine/dse.h` |
| 版本 | `engine/dse_version.h` |
| 引擎实例 | `engine/runtime/engine_app.h` |
| ServiceLocator | `engine/core/service_locator.h` |
| EventBus | `engine/core/event_bus.h` |
| JobSystem | `engine/core/job_system.h` |
| IModule | `engine/core/module.h` |
| World | `engine/ecs/world.h` |
| AssetManager | `engine/assets/asset_manager.h` |
| RhiDevice | `engine/render/rhi/rhi_device.h` |
| RenderGraph | `engine/render/render_graph.h` |
| IRenderPass | `engine/render/passes/render_pass_interface.h` |
| Input | `engine/input/input.h` |
| AudioSystem | `engine/audio/audio_system.h` |
| SceneManager | `engine/scene/scene_manager.h` |
| FramePipeline | `engine/runtime/frame_pipeline.h` |
| 2D 组件 | `engine/ecs/components_2d.h` |
| 3D 组件 | `engine/ecs/components_3d.h` |
| UI 组件 | `engine/ecs/ui.h` |
| 物理 3D 组件 | `engine/ecs/components_3d_physics.h` |
| 粒子 3D 组件 | `engine/ecs/components_3d_particle.h` |
