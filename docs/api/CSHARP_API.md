# DSEngine C# API 参考文档

> 严格对齐 `GameScripts/DSEngine.Runtime/` 源码与 `tools/codegen/binding_defs.json` 数据源
> 更新日期：2026-07-02
> 运行时：.NET 8 CoreCLR (hostfxr)，P/Invoke 通过 `[LibraryImport("dse_engine")]` 源生成器
> 组件字段访问器由 `tools/codegen/codegen.py` 从 `binding_defs.json` 自动生成至
> `GameScripts/DSEngine.Runtime/Generated/Native.gen.cs`（45 组件 / 816 访问器）；
> 组件高级封装类自动生成至 `Generated/Components.gen.cs`。
> 手写 C ABI（`dse_api.h`，动画 FSM/输入/物理动力学/载具/天气/音频/导航/UI/场景/本地化等 252 函数）由
> `tools/codegen/gen_csharp_manual.py` 生成 `Generated/NativeManual.gen.cs`（P/Invoke）
> 与 `Generated/ApiManual.gen.cs`（`DSEngine.Api` 公开门面，45 个静态类）。
> 其余高级封装位于 `Core/`、`Math/`、`Components/`、`Network/` 子目录。

---

## 概览

```
DSEngine.Runtime (net8.0)
├── Native              (internal) — 自动生成的 P/Invoke 声明层
│                          Native.gen.cs (codegen 组件访问器 816)
│                          NativeManual.gen.cs (手写 C ABI 252)
├── DSEngine.Api        (public)   — 手写 C ABI 门面（Input/Anim3D/RigidBody3D/Vehicle/Audio/Nav/Ui 等 45 类）
├── NativeRepl          (internal) — 网络复制层 P/Invoke 声明
├── Core/
│   ├── Entity          — 实体句柄（轻量 struct，uint ID）
│   ├── DseScript       — 用户脚本基类（类 MonoBehaviour）
│   ├── ScriptRegistry  — 脚本发现与生命周期管理
│   └── Callbacks       — [UnmanagedCallersOnly] 导出（C++ → C#）
├── Math/
│   ├── Vector3         — 三维向量（StructLayout.Sequential）
│   └── Vector4         — 四维向量（StructLayout.Sequential）
├── Components/
│   ├── Transform       — 位移/旋转/缩放
│   ├── Camera3D        — 摄像机参数
│   └── MeshRenderer    — 网格渲染材质属性
└── Network/
    ├── ReplicationServer — 服务器权威复制（delta/AOI/RPC）
    ├── ReplicationClient — 客户端镜像（握手/快照/RPC）
    ├── AoiPolicy        — AOI 策略枚举（Always/Distance）
    └── RpcTarget        — RPC 目标枚举（Server/Client/Multicast）

DSEngine.Game (net8.0) — 用户游戏脚本项目
└── [用户 DseScript 子类]
```

---

## 目录

1. [Entity — 实体句柄](#1-entity--实体句柄)
2. [DseScript — 脚本基类](#2-dsescript--脚本基类)
3. [Vector3 — 三维向量](#3-vector3--三维向量)
4. [Vector4 — 四维向量](#4-vector4--四维向量)
5. [Transform — 变换组件](#5-transform--变换组件)
6. [Camera3D — 摄像机组件](#6-camera3d--摄像机组件)
7. [MeshRenderer — 网格渲染组件](#7-meshrenderer--网格渲染组件)
8. [Native P/Invoke 层（Codegen 自动生成）](#8-native-pinvoke-层codegen-自动生成)
9. [Callbacks / ScriptRegistry — 运行时管理](#9-callbacks--scriptregistry--运行时管理)
10. [生命周期流程](#10-生命周期流程)
11. [热重载机制](#11-热重载机制)
12. [Network — 网络复制层](#12-network--网络复制层)
13. [DSEngine.Api — 手写 C ABI 门面](#13-dsengineapi--手写-c-abi-门面)

---

## 1. Entity — 实体句柄

> 文件：`GameScripts/DSEngine.Runtime/Core/Entity.cs`

```csharp
public readonly struct Entity : IEquatable<Entity>
```

轻量值类型，仅包装一个 `uint Id`。所有组件访问通过 Entity ID 中转 C ABI，热重载安全。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `Id` | `uint` | 底层实体 ID（ECS 注册表键） |
| `IsValid` | `bool` | 实体是否有效（调用 `dse_entity_valid`） |
| `Transform` | `Transform` | 获取该实体的 Transform 组件访问器 |
| `Camera3D` | `Camera3D` | 获取该实体的 Camera3D 组件访问器 |
| `MeshRenderer` | `MeshRenderer` | 获取该实体的 MeshRenderer 组件访问器 |

### 静态方法

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `Entity.Create()` | `Entity` | 创建新实体（调用 `dse_entity_create`） |
| `Entity.Null` | `Entity` | 无效实体常量 (Id=0) |

### 实例方法

| 方法 | 说明 |
|------|------|
| `Destroy()` | 销毁实体（调用 `dse_entity_destroy`） |

### 示例

```csharp
var player = Entity.Create();
bool alive = player.IsValid;     // true
player.Destroy();
alive = player.IsValid;          // false
```

---

## 2. DseScript — 脚本基类

> 文件：`GameScripts/DSEngine.Runtime/Core/DseScript.cs`

```csharp
public abstract class DseScript
```

所有用户脚本必须继承此类。运行时通过反射发现所有 `DseScript` 子类并自动实例化。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `Entity` | `Entity` | 该脚本绑定的实体（运行时自动赋值） |

### 虚方法（可重写）

| 方法 | 签名 | 说明 |
|------|------|------|
| `OnStart` | `virtual void OnStart()` | 脚本激活时调用一次 |
| `OnUpdate` | `virtual void OnUpdate(float dt)` | 每帧调用（dt 为帧间隔秒） |
| `OnFixedUpdate` | `virtual void OnFixedUpdate(float dt)` | 固定时间步调用（物理 tick） |
| `OnDestroy` | `virtual void OnDestroy()` | 脚本/实体销毁时调用 |

### 示例

```csharp
using DSEngine;
using System;

public class PlayerController : DseScript {
    private float _speed = 5.0f;

    public override void OnStart() {
        Console.WriteLine($"Player entity: {Entity.Id}");
    }

    public override void OnUpdate(float dt) {
        var t = Entity.Transform;
        var pos = t.Position;
        pos.X += _speed * dt;
        t.Position = pos;
    }
}
```

---

## 3. Vector3 — 三维向量

> 文件：`GameScripts/DSEngine.Runtime/Math/Vector3.cs`

```csharp
[StructLayout(LayoutKind.Sequential)]
public struct Vector3 : IEquatable<Vector3>
```

与 C ABI 的 `float x, float y, float z` 参数直接对应（blittable，零拷贝）。

### 字段

| 字段 | 类型 |
|------|------|
| `X` | `float` |
| `Y` | `float` |
| `Z` | `float` |

### 构造函数

```csharp
public Vector3(float x, float y, float z)
```

### 静态属性

| 属性 | 值 |
|------|------|
| `Vector3.Zero` | (0, 0, 0) |
| `Vector3.One` | (1, 1, 1) |
| `Vector3.Up` | (0, 1, 0) |
| `Vector3.Forward` | (0, 0, -1) |
| `Vector3.Right` | (1, 0, 0) |

### 方法

| 方法 | 签名 | 说明 |
|------|------|------|
| `Length` | `float Length()` | 向量长度 |
| `Normalized` | `Vector3 Normalized()` | 单位化（零向量返回 Zero） |
| `Dot` | `static float Dot(a, b)` | 点积 |
| `Cross` | `static Vector3 Cross(a, b)` | 叉积 |
| `Lerp` | `static Vector3 Lerp(a, b, t)` | 线性插值 |

### 运算符

`+`, `-`, `*`(标量), `/`(标量), 一元 `-`, `==`, `!=`

---

## 4. Vector4 — 四维向量

> 文件：`GameScripts/DSEngine.Runtime/Math/Vector4.cs`

```csharp
[StructLayout(LayoutKind.Sequential)]
public struct Vector4 : IEquatable<Vector4>
```

### 字段

| 字段 | 类型 |
|------|------|
| `X` | `float` |
| `Y` | `float` |
| `Z` | `float` |
| `W` | `float` |

### 静态属性

`Vector4.Zero`, `Vector4.One`

### 运算符

`+`, `-`, `*`(标量), `/`(标量), `==`, `!=`

---

## 5. Transform — 变换组件

> 文件：`GameScripts/DSEngine.Runtime/Components/Transform.cs`
> C ABI：`dse_transform_get/set_position/rotation/scale`

```csharp
public readonly struct Transform
```

通过 Entity ID 访问 ECS 中的 TransformComponent。每次 get/set 调用对应一次 P/Invoke。

### 属性

| 属性 | 类型 | C ABI | 说明 |
|------|------|-------|------|
| `Position` | `Vector3` | `dse_transform_get/set_position` | 世界坐标位置 |
| `Rotation` | `Vector3` | `dse_transform_get/set_rotation` | 欧拉角旋转 (度) |
| `Scale` | `Vector3` | `dse_transform_get/set_scale` | 缩放因子 |

### 示例

```csharp
var t = entity.Transform;
t.Position = new Vector3(10, 0, 5);
t.Rotation = new Vector3(0, 90, 0);  // Y 轴旋转 90°
t.Scale = Vector3.One * 2.0f;         // 均匀放大 2 倍
```

---

## 6. Camera3D — 摄像机组件

> 文件：`GameScripts/DSEngine.Runtime/Components/Camera3D.cs`
> C ABI：`dse_camera3d_get/set_*`

```csharp
public readonly struct Camera3D
```

### 属性

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `Fov` | `float` | 60.0 | 视场角（度） |
| `NearClip` | `float` | 0.1 | 近裁剪面 |
| `FarClip` | `float` | 1000.0 | 远裁剪面 |

### 示例

```csharp
var cam = entity.Camera3D;
cam.Fov = 75.0f;
cam.NearClip = 0.5f;
cam.FarClip = 500.0f;
```

---

## 7. MeshRenderer — 网格渲染组件

> 文件：`GameScripts/DSEngine.Runtime/Components/MeshRenderer.cs`
> C ABI：`dse_mesh_renderer_get/set_*`

```csharp
public readonly struct MeshRenderer
```

### 属性

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `Visible` | `int` | 1 | 可见性（0=隐藏, 1=显示） |
| `ReceiveShadow` | `int` | 1 | 是否接收阴影 |
| `Metallic` | `float` | 0.0 | 金属度 (0.0–1.0) |
| `Roughness` | `float` | 0.5 | 粗糙度 (0.0–1.0) |
| `Color` | `Vector4` | (1,1,1,1) | RGBA 颜色 |
| `Emissive` | `Vector3` | (0,0,0) | 自发光颜色 (HDR) |
| `MeshPath` | `string` | "" | 网格资源路径 |
| `ShaderVariant` | `string` | "" | 着色器变体名 |

### 示例

```csharp
var mesh = entity.MeshRenderer;
mesh.MeshPath = "models/hero.glb";
mesh.Metallic = 0.8f;
mesh.Roughness = 0.2f;
mesh.Color = new Vector4(1, 0.8f, 0.6f, 1);
mesh.Emissive = new Vector3(0.5f, 0, 0);  // 微红自发光
```

---

## 8. Native P/Invoke 层（Codegen 自动生成）

> 文件：`GameScripts/DSEngine.Runtime/Generated/Native.gen.cs`（由 `tools/codegen/codegen.py` 生成，勿手动修改）

```csharp
internal static partial class Native
```

包含对 `dse_engine` 共享库的所有 `[LibraryImport]` P/Invoke 声明。

### 覆盖组件

| 组件 | 前缀 | 字段数 | 函数数 (get+set) |
|------|------|--------|:------:|
| TransformComponent | `transform` | 3 | 6 |
| Camera3DComponent | `camera3d` | 5 | 10 |
| MeshRendererComponent | `mesh_renderer` | 8 | 16 |
| DirectionalLight3DComponent | `dir_light` | 7 | 14 |
| PointLightComponent | `point_light` | 5 | 10 |
| SpotLightComponent | `spot_light` | 8 | 16 |
| SkyLightComponent | `sky_light` | 4 | 8 |
| TreeComponent | `tree` | 19 | 38 |
| TerrainTileManagerComponent | `terrain_tile` | 10 | 20 |
| DynamicObstacleComponent | `dyn_obstacle` | 5 | 10 |
| NavMeshAutoRebakeComponent | `navmesh_rebake` | 11 | 22 |
| PostProcessComponent | `post_process` | 72 | 144 |
| Animator3DComponent | `animator3d` | 8 | 16 |
| …（共 45 个组件，完整列表见 `binding_defs.json` 与 `LUA_API.md` §18，两侧共享同一数据源） | | | |
| **合计** | — | **45 组件** | **816 函数** |

另有 `Generated/Components.gen.cs`（同一 codegen 生成）为全部 45 个组件提供
面向对象的属性封装类（`entity.GetTransform().Position` 风格，见 `ComponentExtensions`）。

### 额外手写声明（Entity 操作）

| 函数 | 签名 | 说明 |
|------|------|------|
| `dse_entity_create` | `→ uint` | 创建实体 |
| `dse_entity_destroy` | `(uint e)` | 销毁实体 |
| `dse_entity_valid` | `(uint e) → int` | 检查实体有效性 |

### 类型映射

| binding_defs.json 类型 | C ABI 签名 | C# P/Invoke |
|---|---|---|
| `float` | `float dse_xx_get(uint e)` | `float dse_xx_get(uint e)` |
| `int` / `bool` | `int dse_xx_get(uint e)` | `int dse_xx_get(uint e)` |
| `vec3` | `void dse_xx_get(uint e, float* x, y, z)` | `void dse_xx_get(uint e, out float x, out float y, out float z)` |
| `vec4` | `void dse_xx_get(uint e, float* x, y, z, w)` | `void dse_xx_get(uint e, out float x, out float y, out float z, out float w)` |
| `string` (get) | `int dse_xx_get(uint e, char* buf, int sz)` | `int dse_xx_get(uint e, [Out] byte[] buf, int bufSize)` |
| `string` (set) | `void dse_xx_set(uint e, const char* v)` | `void dse_xx_set(uint e, string v)` (StringMarshalling.Utf8) |

### 重新生成

```bash
cd tools/codegen
python codegen.py
# 输出：GameScripts/DSEngine.Runtime/Generated/Native.gen.cs
```

---

## 9. Callbacks / ScriptRegistry — 运行时管理

### ScriptRegistry

> 文件：`GameScripts/DSEngine.Runtime/Core/ScriptRegistry.cs`

```csharp
public static class ScriptRegistry
```

| 方法 | 说明 |
|------|------|
| `DiscoverAndInstantiate(Assembly?)` | 扫描程序集，实例化所有 DseScript 子类 |
| `InvokeStart()` | 触发所有脚本的 OnStart（幂等，仅首次） |
| `InvokeUpdate(float dt)` | 触发所有脚本的 OnUpdate |
| `InvokeFixedUpdate(float dt)` | 触发所有脚本的 OnFixedUpdate |
| `DestroyAll()` | 触发所有脚本的 OnDestroy 并清空注册表 |
| `Count` | 当前活跃脚本实例数 |

### Callbacks

> 文件：`GameScripts/DSEngine.Runtime/Core/Callbacks.cs`

```csharp
public static class Callbacks
```

所有方法标记 `[UnmanagedCallersOnly]`，由 C++ CSharpHost 通过函数指针直接调用。

| 方法 | C++ 调用时机 | 说明 |
|------|-------------|------|
| `Initialize(IntPtr path, int len)` | hostfxr 初始化后 | 加载游戏程序集，发现脚本 |
| `Start()` | 引擎就绪后首帧 | 触发 OnStart |
| `Update(float dt)` | 每帧逻辑更新 | 触发 OnUpdate |
| `FixedUpdate(float dt)` | 固定物理步 | 触发 OnFixedUpdate |
| `Reload(IntPtr path, int len)` | 文件变更检测触发 | 卸载旧 ALC → 加载新 DLL → 重新发现脚本 |
| `Shutdown()` | 引擎退出 | 销毁所有脚本，卸载 ALC |

---

## 10. 生命周期流程

```
C++ Engine                         C# Runtime (CoreCLR)
───────────                        ─────────────────────
hostfxr_initialize()
  └→ load_assembly(Runtime.dll)
      └→ Callbacks.Initialize(game.dll path)
           └→ ALC.Load(Game.dll)
              └→ ScriptRegistry.DiscoverAndInstantiate()

engine.Start()
  └→ Callbacks.Start()
       └→ ScriptRegistry.InvokeStart()
            └→ [每个 DseScript].OnStart()

engine.Update(dt)
  └→ Callbacks.Update(dt)
       └→ ScriptRegistry.InvokeUpdate(dt)
            └→ [每个 DseScript].OnUpdate(dt)
                 └→ entity.Transform.Position = ...  ─→ P/Invoke ─→ dse_transform_set_position()

engine.FixedUpdate(dt)
  └→ Callbacks.FixedUpdate(dt)
       └→ ScriptRegistry.InvokeFixedUpdate(dt)

engine.Shutdown()
  └→ Callbacks.Shutdown()
       └→ ScriptRegistry.DestroyAll()
            └→ [每个 DseScript].OnDestroy()
```

---

## 11. 热重载机制

DSEngine C# 运行时支持编辑期热重载（无需重启引擎）：

1. **FileWatcher** 监控 `GameScripts/DSEngine.Game/` 目录
2. 检测到 `.cs` 文件变更 → 触发 `dotnet build`
3. 编译成功 → C++ 调用 `Callbacks.Reload(new_dll_path)`
4. C# 侧流程：
   - `ScriptRegistry.DestroyAll()` — 调用所有 OnDestroy
   - `AssemblyLoadContext.Unload()` — 卸载旧程序集（collectible ALC）
   - `GC.Collect()` + `GC.WaitForPendingFinalizers()`
   - 新建 ALC → 加载新 DLL → `ScriptRegistry.DiscoverAndInstantiate()`
   - `ScriptRegistry.InvokeStart()` — 调用新脚本的 OnStart

**设计约束：**
- DseScript 子类不可持有非托管资源引用（卸载后悬空）
- Entity ID 在重载后仍有效（ECS 实体不随脚本重载销毁）
- 静态状态丢失（每次重载从零开始）

---

## 附录 A：项目结构

```
GameScripts/
├── DSEngine.sln                     — 解决方案文件
├── DSEngine.Runtime/
│   ├── DSEngine.Runtime.csproj      — net8.0, AllowUnsafeBlocks
│   ├── Generated/
│   │   ├── Native.gen.cs            — [LibraryImport] 声明（codegen 组件访问器）
│   │   ├── Components.gen.cs        — 组件高级封装类（codegen 产物）
│   │   ├── NativeManual.gen.cs      — 手写 C ABI 的 [LibraryImport] 声明
│   │   └── ApiManual.gen.cs         — DSEngine.Api 公开门面（45 个静态类）
│   ├── Core/
│   │   ├── Entity.cs
│   │   ├── DseScript.cs
│   │   ├── ScriptRegistry.cs
│   │   └── Callbacks.cs
│   ├── Math/
│   │   ├── Vector3.cs
│   │   └── Vector4.cs
│   └── Components/
│       ├── Transform.cs
│       ├── Camera3D.cs
│       └── MeshRenderer.cs
└── DSEngine.Game/
    ├── DSEngine.Game.csproj         — 引用 DSEngine.Runtime
    └── SampleScript.cs              — 示例脚本
```

## 附录 B：构建方式

```bash
# 生成 P/Invoke 声明
cd tools/codegen && python codegen.py

# 构建 C# 项目
dotnet build GameScripts/DSEngine.sln -c Debug

# 构建 C++ 引擎（含 C# 支持）
cmake --preset windows-x64-debug -DDSE_ENABLE_CSHARP=ON
cmake --build out/build/windows-x64-debug

# 单独构建 C# 目标（CMake）
cmake --build out/build/windows-x64-debug --target dse_csharp_build
```

## 附录 C：与 Lua API 对照

| 操作 | Lua | C# |
|------|-----|-----|
| 创建实体 | `dse.ecs.create()` | `Entity.Create()` |
| 销毁实体 | `dse.ecs.destroy(e)` | `entity.Destroy()` |
| 获取位置 | `dse.ecs.get_transform_position(e)` → x,y,z | `entity.Transform.Position` → Vector3 |
| 设置位置 | `dse.ecs.set_transform_position(e, x, y, z)` | `entity.Transform.Position = new Vector3(x, y, z)` |
| 获取 FOV | `dse.ecs.get_camera3d_fov(e)` → float | `entity.Camera3D.Fov` → float |
| 设置网格 | `dse.ecs.set_mesh_path(e, path)` | `entity.MeshRenderer.MeshPath = path` |
| 脚本基类 | 无（Lua 用约定函数名） | `class MyScript : DseScript` |
| 热重载 | 内置（Lua 源码重载） | `AssemblyLoadContext.Unload()` + 重编译 |

> **设计哲学**：Lua 和 C# 共享同一套 C ABI 底层（400+ 导出函数），高层封装风格不同：
> Lua 为函数式（`get_xxx(e)` / `set_xxx(e, v)`），C# 为面向对象（`entity.Component.Property`）。

---

## 12. Network — 网络复制层

> 对应文件：`GameScripts/DSEngine.Runtime/Network/`
> P/Invoke 声明：`NativeRepl.cs`（对应 C ABI `engine/net/repl_c_api.h`）
> 需要 `DSE_ENABLE_NET=ON` 构建引擎

### 12.1 AoiPolicy 枚举

```csharp
public enum AoiPolicy {
    Always = 0,     // 所有实体对所有客户端可见
    Distance = 1,   // 仅距离内的实体可见
}
```

### 12.2 RpcTarget 枚举

```csharp
public enum RpcTarget {
    Server = 0,     // Client → Server（服务器校验）
    Client = 1,     // Server → 特定 Client
    Multicast = 2,  // Server → 所有相关 Client（广播）
}
```

### 12.3 ReplicationServer

服务器权威复制管理器：协议握手、delta 压缩、AOI 裁剪、RPC 派发。

```csharp
public sealed class ReplicationServer : IDisposable
```

| 成员 | 类型 | 说明 |
|------|------|------|
| `ReplicationServer()` | ctor | 创建实例 |
| `Init(nint transport, nint registry)` | `bool` | 绑定传输与 ECS registry |
| `MarkReplicated(uint entityId, uint ownerConn)` | `uint` | 将实体纳入复制，返回 NetId（0=失败） |
| `SetOwner(uint entityId, uint ownerConn)` | `void` | 变更实体属主 |
| `Unreplicate(uint entityId)` | `void` | 移出复制（广播 despawn） |
| `Tick()` | `void` | 每帧调用：发送快照/delta + AOI |
| `SetAoi(AoiPolicy policy, float radius)` | `void` | 设置 AOI 策略 |
| `ClientCount` | `uint` | 已连接客户端数（只读） |
| `CurrentSeq` | `uint` | 当前快照序号（只读） |
| `BroadcastRpc(ushort rpcId, uint targetNetId, ReadOnlySpan<byte> payload)` | `void` | 广播 RPC |
| `Handle` | `nint` | 原生句柄（高级互操作用） |
| `Dispose()` | `void` | 销毁原生资源 |

### 12.4 ReplicationClient

客户端复制管理器：协议握手、快照/delta 接收、输入发送、RPC。

```csharp
public sealed class ReplicationClient : IDisposable
```

| 成员 | 类型 | 说明 |
|------|------|------|
| `ReplicationClient()` | ctor | 创建实例 |
| `Init(nint transport, nint registry)` | `bool` | 绑定传输与 ECS registry |
| `SendMove(uint netId, float dx, float dy, float dz)` | `void` | 发送移动输入 |
| `SendMove(uint netId, Vector3 delta)` | `void` | 发送移动输入（Vector3 重载） |
| `IsConnected` | `bool` | 协议握手是否完成（只读） |
| `MirrorCount` | `uint` | 镜像实体数量（只读） |
| `ToEntity(uint netId)` | `uint` | NetId → 本地实体 ID（0xFFFFFFFF=未知） |
| `SendRpc(ushort rpcId, uint targetNetId, ReadOnlySpan<byte> payload)` | `bool` | 发送 RPC 到服务器 |
| `Handle` | `nint` | 原生句柄（高级互操作用） |
| `Dispose()` | `void` | 销毁原生资源 |

### 12.5 NativeRepl（内部 P/Invoke 声明）

> 用户不直接调用；通过高级封装类访问。

```csharp
internal static partial class NativeRepl {
    // Server
    [LibraryImport("dse_engine")] internal static partial nint dse_repl_server_create();
    [LibraryImport("dse_engine")] internal static partial void dse_repl_server_destroy(nint srv);
    [LibraryImport("dse_engine")] internal static partial int dse_repl_server_init(nint srv, nint transport, nint registry);
    [LibraryImport("dse_engine")] internal static partial uint dse_repl_server_mark(nint srv, uint entity, uint ownerConn);
    [LibraryImport("dse_engine")] internal static partial void dse_repl_server_set_owner(nint srv, uint entity, uint ownerConn);
    [LibraryImport("dse_engine")] internal static partial void dse_repl_server_unreplicate(nint srv, uint entity);
    [LibraryImport("dse_engine")] internal static partial void dse_repl_server_tick(nint srv);
    [LibraryImport("dse_engine")] internal static partial void dse_repl_server_set_aoi(nint srv, int policy, float radius);
    [LibraryImport("dse_engine")] internal static partial uint dse_repl_server_client_count(nint srv);
    [LibraryImport("dse_engine")] internal static partial uint dse_repl_server_seq(nint srv);
    // Client
    [LibraryImport("dse_engine")] internal static partial nint dse_repl_client_create();
    [LibraryImport("dse_engine")] internal static partial void dse_repl_client_destroy(nint cli);
    [LibraryImport("dse_engine")] internal static partial int dse_repl_client_init(nint cli, nint transport, nint registry);
    [LibraryImport("dse_engine")] internal static partial void dse_repl_client_send_move(nint cli, uint netId, float dx, float dy, float dz);
    [LibraryImport("dse_engine")] internal static partial int dse_repl_client_connected(nint cli);
    [LibraryImport("dse_engine")] internal static partial uint dse_repl_client_mirror_count(nint cli);
    [LibraryImport("dse_engine")] internal static partial uint dse_repl_client_to_entity(nint cli, uint netId);
    // RPC
    [LibraryImport("dse_engine")] internal static partial ushort dse_rpc_server_register(nint srv, string name, int target, nint handler, nint validator, nint userdata);
    [LibraryImport("dse_engine")] internal static partial ushort dse_rpc_client_register(nint cli, string name, int target, nint handler, nint userdata);
    [LibraryImport("dse_engine")] internal static partial int dse_rpc_client_send(nint cli, ushort rpcId, uint targetNetId, nint payload, nuint payloadSize);
    [LibraryImport("dse_engine")] internal static partial void dse_rpc_server_broadcast(nint srv, ushort rpcId, uint targetNetId, nint payload, nuint payloadSize);
}
```

### 12.6 使用示例

```csharp
using DSEngine;

public class NetworkGameScript : DseScript {
    private ReplicationServer? _server;

    public override void OnStart() {
        _server = new ReplicationServer();
        // transport 和 registry 从引擎内部获取（通常通过回调注入）
        // _server.Init(transportHandle, registryHandle);
        _server.SetAoi(AoiPolicy.Distance, 50.0f);
    }

    public override void OnUpdate(float dt) {
        _server?.Tick();
    }

    public void ReplicateEntity(uint entityId, uint ownerConn) {
        uint netId = _server!.MarkReplicated(entityId, ownerConn);
        if (netId == 0) {
            // 失败处理
        }
    }

    public override void OnDestroy() {
        _server?.Dispose();
    }
}
```

```csharp
using DSEngine;

public class ClientScript : DseScript {
    private ReplicationClient? _client;

    public override void OnStart() {
        _client = new ReplicationClient();
        // _client.Init(transportHandle, registryHandle);
    }

    public override void OnUpdate(float dt) {
        if (_client != null && _client.IsConnected) {
            // 发送移动输入
            _client.SendMove(myNetId, new Vector3(1.0f, 0, 0));

            // 发送 RPC
            byte[] payload = System.Text.Encoding.UTF8.GetBytes("interact");
            _client.SendRpc(rpcId, targetNetId, payload);

            // 查询镜像
            uint mirrorCount = _client.MirrorCount;
            uint localEntity = _client.ToEntity(someNetId);
        }
    }

    public override void OnDestroy() {
        _client?.Dispose();
    }
}
```

---

## 13. DSEngine.Api — 手写 C ABI 门面

> 文件：`GameScripts/DSEngine.Runtime/Generated/ApiManual.gen.cs`
> P/Invoke 声明：`Generated/NativeManual.gen.cs`
> 生成脚本：`tools/codegen/gen_csharp_manual.py`（解析 `engine/scripting/native_api/dse_api.h`；随 `codegen.py` 自动运行）

`namespace DSEngine.Api` 下的公开静态类，按子系统分组转发到手写 C ABI（与 Lua 手写绑定同一底层）。
方法签名与 C ABI 一一对应（`int` 作布尔、`out float` 作输出参数、`float[]` 作向量/数组缓冲）。

### 类与函数数（共 45 类 / 252 函数）

| 类 | 函数数 | 覆盖 C ABI 前缀 | 说明 |
|------|:---:|------|------|
| `Input` | 10 | `dse_input_*` | 键盘/鼠标/手柄查询 |
| `App` | 6 | `dse_app_*` | quit / set_title / target_fps / draw_calls |
| `Assets` | 2 | `dse_assets_*` | 资产根路径 / 预加载 |
| `Metrics` | 1 | `dse_metrics_*` | 帧统计 |
| `Render` | 2 | `dse_render_*` | world_to_screen / screen_to_world_ray |
| `Anim2D` / `Anim3D` / `AnimLayer` | 6/13/8 | `dse_anim2d_* / anim3d_* / animlayer_*` | 动画剪辑、FSM、动画层/混合树 |
| `Ik` / `FootIk` / `BoneAttachment` / `MorphTarget` | 8/6/6/6 | `dse_ik_* / foot_ik_* / bone_attach_* / morph_*` | IK、足部 IK、骨骼挂点、形态目标 |
| `Physics3D` | 3 | `dse_physics3d_*` | raycast / overlap 查询 |
| `RigidBody3D` | 9 | `dse_rigidbody3d_*` | 力/冲量/速度/重力 |
| `BoxCollider3D` / `SphereCollider3D` / `CapsuleCollider3D` / `MeshCollider3D` | 各 1 | `dse_*_collider3d_add` | 碰撞体挂载 |
| `Collider` / `Collision` | 2/1 | `dse_collider_* / collision_*` | trigger/材质/碰撞层 |
| `Joint3D` | 5 | `dse_joint3d_*` | 关节、铰链限制、弹簧 |
| `CharacterController3D` | 5 | `dse_character_controller3d_*` | 角色控制器移动/跳跃/贴地 |
| `Terrain` | 3 | `dse_terrain_*` | 高度图碰撞/采样 |
| `Vehicle` | 5 | `dse_vehicle_*` | 载具输入/状态 |
| `Cloth` / `Rope` / `SoftBody` / `Fluid` / `Buoyancy` / `Ragdoll` / `Fracture` | 5/4/4/6/4/4/5 | 对应前缀 | 布料/绳索/软体/流体/浮力/布娃娃/破碎 |
| `Weather` / `Snow` / `Atmosphere` / `DayNight` / `Cloud` | 3/8/4/7/3 | 对应前缀 | 天气/积雪/大气/昼夜/体积云 |
| `Components` | 10 | 其余 `dse_*_add` 等 | Transform/相机/灯光/MeshRenderer 挂载 |
| `Audio` | 12 | `dse_audio_*` | BGM/SFX 播放、Crossfade、音量、预加载 |
| `AudioSource` / `AudioListener` | 10/1 | `dse_audio_source_* / audio_listener_*` | ECS 音源组件（3D 空间音频/总线）与监听器 |
| `Nav` | 6 | `dse_nav_*` | NavMesh 加载/保存/寻路/最近点/射线 |
| `NavAgent` | 5 | `dse_nav_agent_*` | NavMeshAgent 参数/目的地/状态查询 |
| `Localization` | 5 | `dse_l10n_*` | 语言包加载、locale 切换、键值查询 |
| `Scene` | 4 | `dse_scene_*` | 场景加载/保存、Prefab 保存/实例化 |
| `Ui` | 31 | `dse_ui_*` | 渲染器/按钮/标签/摇杆/滑条/开关/进度条/输入框/布局文件加载 |

### 示例

```csharp
using DSEngine.Api;

public class CarController : DseScript {
    public override void OnUpdate(float dt) {
        float throttle = Input.GetKey(87) != 0 ? 1f : 0f;   // W
        float steer = Input.GetGamepadAxis(0, 0);
        Vehicle.SetInput(Entity.Id, throttle, 0f, steer);

        if (Input.GetKeyDown(32) != 0)                       // Space
            RigidBody3D.AddImpulse(Entity.Id, 0f, 500f, 0f);
    }
}
```

### 重新生成

```bash
python tools/codegen/codegen.py           # 一并生成 NativeManual.gen.cs + ApiManual.gen.cs
# 或单独：python tools/codegen/gen_csharp_manual.py
```
