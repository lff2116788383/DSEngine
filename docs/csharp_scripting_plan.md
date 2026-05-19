# DSEngine C# 脚本系统集成方案

> 状态：**S1 ✅ + S1.5 ✅ 已完成 — S2（Mono 嵌入）待启动**  
> 完成：Lua 绑定层（~6700 行，17 文件），C++ GameApplication 宿主，C ABI 层，Codegen 工具  

---

## 一、整体架构

```
┌─────────────────────────────────────────────────────────┐
│                      游戏逻辑层                          │
│  C# DSBehaviour (.cs)   Lua ScriptComponent (.lua)  C++ │
└──────┬──────────────────────┬──────────────────┬────────┘
       │                      │                  │
       ▼                      ▼                  │ 直接调用
┌──────────────────┐  ┌───────────────┐          │
│ C# Mono Managed  │  │ Lua Binding   │          │
│ Interop Layer    │  │ lua_binding_  │          │
│ InternalCall /   │  │ *.cpp（现有） │          │
│ P/Invoke         │  │               │          │
└────────┬─────────┘  └──────┬────────┘          │
         │                   │                   │
         ▼                   ▼                   ▼
┌─────────────────────────────────────────────────────────┐
│            Native API Layer  (C ABI)                     │
│    engine/scripting/native_api/dse_api.h                 │
│    纯 C 函数 — Lua 和 C# 共享同一套底层入口               │
│    由 Codegen 工具自动生成（binding_defs.json 驱动）      │
└─────────────────────────┬───────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│         Engine Core  (dse_engine.dll / .so)              │
│         World / ECS / Renderer / Assets                  │
└─────────────────────────────────────────────────────────┘
```

**核心设计原则：**
1. **C ABI 中间层** — Lua 与 C# 共享，消除双份维护
2. **Codegen 驱动** — 新增组件改 JSON，三端（C ABI / Lua / C#）同步生成
3. **逐实体脚本** — C# `DSBehaviour` 对齐 Lua `ScriptComponent` 语义
4. **异常隔离** — C# 异常在 Mono 层被捕获，不崩溃引擎
5. **AppDomain 热重载** — 开发期改 C# 文件 → 自动检测 → reload，同 Lua 一样无缝

---

## 二、实施阶段（修正版）

### Phase S1：C ABI 中间层 ✅ 已完成

将现有 Lua 绑定中的引擎操作提取为纯 C 函数。**本阶段无需 Mono，即使不加 C# 也能让 Lua 绑定更干净。**

```
engine/scripting/native_api/
├── dse_api.h          196 行 — 函数声明（C ABI）
└── dse_api.cpp        567 行 — 实现（调用 World / AssetManager）
```

**实际交付：**
- 60+ C 函数覆盖：Entity / Transform / Camera3D / MeshRenderer / DirectionalLight / PointLight / SpotLight / Input / Assets / App / Metrics
- `dse_native_api_init(world, asset_mgr, audio, quit, title, get_fps, set_fps, get_draw_calls)` — 8 参数完整注入
- `dse_get_world_ptr()` — 供 gen.cpp opt-in 后访问 World，无需内部 extern
- `lua_binding_context.cpp::ConfigureBindingContext()` 已接入，Lua 与 C# 共享同一 World
- 全函数空指针保护，零崩溃风险

```c
// dse_api.h — 纯 C ABI，Lua 和 C# P/Invoke 共用
#ifdef __cplusplus
extern "C" {
#endif

// === 实体 ===
DSE_API uint32_t dse_entity_create(void);
DSE_API void     dse_entity_destroy(uint32_t entity);
DSE_API int      dse_entity_valid(uint32_t entity);

// === Transform ===
DSE_API void  dse_transform_get_position(uint32_t e, float* x, float* y, float* z);
DSE_API void  dse_transform_set_position(uint32_t e, float x, float y, float z);
DSE_API void  dse_transform_get_rotation(uint32_t e, float* x, float* y, float* z);
DSE_API void  dse_transform_set_rotation(uint32_t e, float x, float y, float z);
DSE_API void  dse_transform_get_scale   (uint32_t e, float* x, float* y, float* z);
DSE_API void  dse_transform_set_scale   (uint32_t e, float x, float y, float z);

// === Camera / Mesh / Light / Physics / Audio / Input ... ===
// （完整列表由 Codegen 生成）

// === 输入 ===
DSE_API int   dse_input_get_key     (int key_code);
DSE_API int   dse_input_get_key_down(int key_code);
DSE_API float dse_input_get_mouse_x (void);
DSE_API float dse_input_get_mouse_y (void);

#ifdef __cplusplus
}
#endif
```

`dse_api.cpp` 通过全局 `DseApiContext`（含 `World*` + `AssetManager*`）调用引擎，与
`lua_binding_context.cpp` 的 `g_binding_context` 模式一致。

---

### Phase S1.5：Codegen 工具 ✅ 已完成

> **从原 S4"可选"提升为必须，原因：同时维护 Lua + C# 两套手写绑定 = 技术债。**

```
tools/codegen/
├── binding_defs.json          组件字段定义（唯一数据源，5 个组件 ~180 行）
├── codegen.py                 生成器主程序（~170 行，含 --dry-run）
└── templates/
    ├── dse_api.h.j2           → engine/scripting/native_api/dse_api.gen.h
    ├── dse_api.cpp.j2         → engine/scripting/native_api/dse_api.gen.cpp
    ├── lua_binding.cpp.j2     → engine/scripting/lua/bindings/lua_binding_ecs_<prefix>.gen.cpp
    └── csharp_native.cs.j2    → GameScripts/DSEngine/Native.gen.cs
```

**实际交付：**
- `binding_defs.json` 覆盖 5 组件：Transform / Camera3D / MeshRenderer / DirectionalLight / PointLight
- 支持字段类型：`float` / `int` / `bool` / `vec3` / `vec4` / `euler_quat`（带 `dirty_flag`）
- `*.gen.cpp` 经 CMakeLists.txt 过滤不参与构建（避免与手写绑定符号冲突）；opt-in 后可替换手写版
- 新增 CMake target `dse_codegen`：`cmake --build build_vs2022 --target dse_codegen`
- 所有 6 个模板问题（未定义符号 / nullptr 回调 / 重复声明 / 命名不一致 / 格式 / 冗余函数）已在审查中修复

**一次生成，三端同步：**

```bash
# 更新 binding_defs.json 后执行：
python tools/codegen/codegen.py
# 输出（8 个文件）：
#   engine/scripting/native_api/dse_api.gen.h/.gen.cpp
#   engine/scripting/lua/bindings/lua_binding_ecs_*.gen.cpp  （5 个，staged）
#   GameScripts/DSEngine/Native.gen.cs                       （C# InternalCall 声明）
```

---

### Phase S2：Mono 嵌入 + 逐实体脚本 + 异常隔离（~600 行）

```
engine/scripting/csharp/
├── mono_runtime.h               生命周期接口
├── mono_runtime.cpp             Init / Tick / Shutdown / HotReload
├── mono_internal_calls.h        InternalCall 注册声明
└── mono_internal_calls.cpp      RegisterInternalCalls()（大部分由 Codegen 生成）
```

#### 为什么选 Mono

| 对比项 | Mono | CoreCLR (.NET 8+) |
|--------|------|-------------------|
| 嵌入复杂度 | **低**（mono_jit_init 一行） | 高（hostfxr + runtimeconfig） |
| Android 支持 | **成熟**（Xamarin/Unity 验证） | NativeAOT 限制反射 |
| 热重载 | AppDomain swap | AssemblyLoadContext（更复杂） |
| 游戏引擎先例 | Unity, Godot | Flax Engine |
| 升级路径 | 后期可换 CoreCLR | — |

#### 关键实现：异常隔离

```cpp
// 每次调用 C# 方法都必须捕获异常，不能让用户脚本崩溃引擎
static bool SafeInvoke(MonoMethod* method, MonoObject* obj, void** args) {
    MonoObject* exception = nullptr;
    mono_runtime_invoke(method, obj, args, &exception);
    if (exception) {
        MonoString* msg = mono_object_to_string(exception, nullptr);
        char* utf8 = mono_string_to_utf8(msg);
        DEBUG_LOG_ERROR("[C# Exception] {}", utf8);
        mono_free(utf8);
        return false;  // 不崩溃，继续运行
    }
    return true;
}
```

#### 关键实现：逐实体 C# 脚本

`ScriptComponent`（现有）加一个 `csharp_class_name` 字段，即可复用 Lua 侧的 Entity-Script 关联模式：

```cpp
// engine/ecs/script.h（扩展）
struct ScriptComponent {
    std::string script_path;        // Lua 脚本路径
    std::string csharp_class_name;  // C# 类名（互斥使用）
    bool enabled = true;
};
```

C# 侧：
```csharp
// 用户继承 DSBehaviour，完全对齐 Lua ScriptComponent 语义
public class PlayerController : DSBehaviour {
    public override void OnAwake(uint entityId) {
        Entity.SetMesh(entityId, "models/player.dmesh");
    }
    public override void OnUpdate(uint entityId, float dt) {
        if (Input.GetKey(KeyCode.W)) {
            var pos = Entity.GetPosition(entityId);
            Entity.SetPosition(entityId, pos.X, pos.Y - 5f * dt, pos.Z);
        }
    }
    public override void OnDestroy(uint entityId) { }
}
```

Mono 侧在每帧通过反射查找并调用 `OnUpdate`，与 Lua 的 `CallScriptTableMethod` 逻辑对等。

---

### Phase S3：C# API 封装 + Blittable 结构体（~600 行 C#）

```
GameScripts/
├── DSEngine/
│   ├── Native.gen.cs          ← Codegen 生成（InternalCall 声明）
│   ├── Entity.cs              ← 手写封装（调用 Native.*）
│   ├── DSBehaviour.cs         ← 逐实体脚本基类
│   ├── Input.cs
│   ├── Audio.cs
│   └── MathTypes.cs           ← Vector3 / Quaternion（Blittable）
├── GameAssembly.csproj
└── 用户脚本 (.cs)
```

#### GC / Marshaling 策略

```csharp
// Vector3 必须是 Blittable struct，直接内存拷贝，零 GC
[StructLayout(LayoutKind.Sequential)]
public struct Vector3 {
    public float X, Y, Z;
    public Vector3(float x, float y, float z) { X=x; Y=y; Z=z; }
}

// ✅ 正确：out 参数直接映射 float*，无装箱
[MethodImpl(MethodImplOptions.InternalCall)]
internal static extern void dse_transform_get_position(uint e, out float x, out float y, out float z);

// ✅ 正确：string 只在初始化时传递，热路径用 uint ID
[MethodImpl(MethodImplOptions.InternalCall)]
internal static extern void dse_mesh_renderer_set_mesh(uint e, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);

// ❌ 错误：热路径不要用 string 返回值（每次 GC 分配）
// internal static extern string dse_get_name(uint e);  ← 禁止
```

---

### Phase S4：C# Assembly 热重载（~400 行 C++）

> **本阶段是开发体验保障，与 Lua 热重载对等。必须实现，不可省略。**

#### 热重载对比

| | Lua | C# |
|--|-----|-----|
| 粒度 | 单文件 | 整个 DLL |
| 触发条件 | .lua 文件时间戳变化 | GameAssembly.dll 时间戳变化（msbuild 触发） |
| 状态保持 | OnSerializeState / OnDeserializeState | OnSerialize / OnDeserialize（同样机制） |
| 延迟 | <1ms | ~100–500ms（AppDomain 重建） |
| 实现位置 | `PumpLuaScriptHotReloads()` | `PumpCSharpHotReload()` |

#### 热重载流程

```
1. 监测 GameAssembly.dll 变更（文件时间戳）
2. 广播 DSBehaviour.OnSerialize() → 收集所有实体的 C# 状态到字典
3. mono_domain_unload(game_domain_)
4. 创建新 MonoDomain → mono_domain_assembly_open 加载新 DLL
5. 重新注册 InternalCalls
6. 重新实例化所有 CSharpScriptComponent 对应的 DSBehaviour
7. 广播 DSBehaviour.OnDeserialize() → 恢复状态
```

```cpp
// mono_runtime.cpp
int PumpCSharpHotReload() {
    auto dll_time = GetFileWriteTime("GameAssembly.dll");
    if (dll_time == last_dll_time_) return 0;
    last_dll_time_ = dll_time;

    DEBUG_LOG_INFO("[C#] Detected assembly change, hot-reloading...");
    SerializeAllInstances();           // 步骤 2
    mono_domain_unload(game_domain_);  // 步骤 3
    ReloadDomain();                    // 步骤 4-5
    RebuildAllInstances();             // 步骤 6
    DeserializeAllInstances();         // 步骤 7
    DEBUG_LOG_INFO("[C#] Hot-reload complete");
    return 1;
}
```

---

## 三、CMake 集成

```cmake
option(DSE_ENABLE_CSHARP "Enable C# scripting via Mono" OFF)

if(DSE_ENABLE_CSHARP)
    find_package(Mono REQUIRED)
    add_definitions(-DDSE_ENABLE_CSHARP)

    file(GLOB_RECURSE csharp_runtime_cpp "engine/scripting/csharp/*.cpp")
    list(APPEND engine_cpp ${csharp_runtime_cpp})

    target_include_directories(dse_engine PRIVATE ${MONO_INCLUDE_DIRS})
    target_link_libraries(dse_engine PRIVATE ${MONO_LIBRARIES})

    # C# 游戏脚本编译（开发期用 mcs，发布用 AOT）
    add_custom_target(dse_compile_csharp ALL
        COMMAND ${MONO_MCS} -target:library
                -r:${MONO_CLASS_LIB}/mscorlib.dll
                -out:${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/GameAssembly.dll
                ${CMAKE_SOURCE_DIR}/GameScripts/DSEngine/*.cs
                ${CMAKE_SOURCE_DIR}/GameScripts/*.cs
        COMMENT "Compiling C# game scripts"
    )

    # Codegen（S1.5）— 在构建前运行
    add_custom_target(dse_codegen
        COMMAND python ${CMAKE_SOURCE_DIR}/tools/codegen/codegen.py
                --defs ${CMAKE_SOURCE_DIR}/tools/codegen/binding_defs.json
                --out  ${CMAKE_SOURCE_DIR}
        COMMENT "Generating bindings from binding_defs.json"
    )
    add_dependencies(dse_engine dse_codegen)
endif()
```

---

## 四、文件结构预览（完整）

```
engine/scripting/
├── native_api/                    ← S1 新增：C ABI 中间层
│   ├── dse_api.h                  ~50 行手写 + Codegen 生成部分
│   └── dse_api.cpp                ~100 行手写 + Codegen 生成部分
├── csharp/                        ← S2 新增：Mono 运行时
│   ├── mono_runtime.h
│   ├── mono_runtime.cpp           ~350 行（含热重载）
│   ├── mono_internal_calls.h
│   └── mono_internal_calls.cpp    ~150 行手写 + Codegen 生成部分
├── lua/                           ← 现有（逐步迁移调用 dse_api）
│   ├── bindings/                  17 文件 ~6700 行 → 逐步替换为 .gen.cpp
│   └── lua_runtime.h/cpp
└── cpp/                           ← 现有
    ├── game_application.h/cpp
    └── cpp_business_runtime.h/cpp

tools/codegen/                     ← S1.5 新增：Codegen 工具
├── binding_defs.json              唯一数据源
├── codegen.py
└── templates/
    ├── dse_api.h.j2
    ├── dse_api.cpp.j2
    ├── lua_binding.cpp.j2
    └── csharp_native.cs.j2

GameScripts/                       ← S3 新增：C# 脚本目录
├── DSEngine/
│   ├── Native.gen.cs              Codegen 生成
│   ├── Entity.cs
│   ├── DSBehaviour.cs             逐实体脚本基类
│   ├── Input.cs
│   ├── Audio.cs
│   └── MathTypes.cs               Blittable Vector3 / Quaternion
├── GameAssembly.csproj
└── 用户脚本示例 (.cs)
```

---

## 五、实施优先级（修正版）

| Phase | 内容 | 工作量 | 前置 | 备注 |
|-------|------|--------|------|------|
| **S1** | C ABI 中间层 `dse_api.h/cpp` | ~500 行 C++ | 无 | 单独有价值，净化 Lua 绑定 |
| **S1.5** | Codegen 工具 | ~800 行 Python | S1 | **必须**，三端同步生成 |
| **S2** | Mono 嵌入 + 逐实体脚本 + 异常隔离 | ~600 行 C++ | S1.5 | |
| **S3** | C# API 封装（Blittable + 零 GC） | ~600 行 C# | S2 | |
| **S4** | C# Assembly 热重载 | ~400 行 C++ | S2 | **必须**，开发体验关键 |

**总工作量：~1500 行 C++ + ~800 行 Python + ~600 行 C#，约 3-5 天。**

---

## 六、依赖

| 依赖 | 来源 | 大小 |
|------|------|------|
| Mono 运行时（桌面） | [mono-project.com](https://www.mono-project.com/) 或 vcpkg | ~30 MB |
| Mono 运行时（Android） | Xamarin 预编译 / mono-android NDK 构建 | ~15 MB |
| Python + Jinja2（构建期） | 仅 Codegen 工具使用，不打包到发布 | — |

---

## 七、Lua 绑定迁移路径（渐进式，不需要一次替换）

```
现状：lua_binding_ecs_transform.cpp     手写 ~74 行
       ↓ Phase S1
迁移：lua_binding_ecs_transform.cpp     内部改为调用 dse_api_transform()
       ↓ Phase S1.5 Codegen
替换：lua_binding_ecs_transform.gen.cpp 自动生成，手写版存档删除
```

可以按模块逐步迁移（先 Transform，再 Camera，再 Rendering...），不影响线上版本。

---

## 八、技术债评估（v2 版本）

| 问题 | v1 方案 | v2 修正后 |
|------|---------|-----------|
| C# 只有全局脚本 | ❌ 无逐实体支持 | ✅ DSBehaviour + CSharpScriptComponent |
| C# 热重载 | ❌ 未设计 | ✅ Phase S4 AppDomain swap |
| Codegen 可选 | ❌ 双份手写维护 | ✅ 必须，S1.5 前置 |
| GC/Marshaling | ❌ 未提 | ✅ Blittable struct 策略明确 |
| 异常隔离 | ❌ 用户脚本可崩溃引擎 | ✅ SafeInvoke 每处包裹 |

**v2 版本无遗留技术债。**
