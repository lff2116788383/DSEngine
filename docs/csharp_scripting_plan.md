# DSEngine C# 脚本系统集成方案

> 状态：**S1 ✅ + S1.5 ✅ + S1.6 ✅ + S1.7a-b ✅ — S1.7c / S1.8 待启动 — S2（Mono）待启动**  
> 最后核对：2026-06（与 `master` 代码现状对齐）  
> 已完成：Lua 绑定层（~6700 行）、C ABI 层、Codegen 工具、**12 组件**字段 get/set 由 `dse_api.gen.cpp` 生成并参与构建

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
│ InternalCall /   │  │ *.cpp         │          │
│ P/Invoke         │  │               │          │
└────────┬─────────┘  └──────┬────────┘          │
         │                   │                   │
         ▼                   ▼                   ▼
┌─────────────────────────────────────────────────────────┐
│            Native API Layer  (C ABI)                     │
│    engine/scripting/native_api/dse_api.h / dse_api.cpp   │
│    纯 C 函数 — Lua 和 C# 共享同一套底层入口               │
│    L1/L2 字段 get/set 由 Codegen 生成；L3 add / L5 服务 API 手写 │
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
2. **Codegen 驱动** — 新增组件字段改 `binding_defs.json`，三端（C ABI / Lua / C#）同步生成
3. **逐实体脚本** — C# `DSBehaviour` 对齐 Lua `ScriptComponent` 语义
4. **异常隔离** — C# 异常在 Mono 层被捕获，不崩溃引擎
5. **开发期热重载** — 改 C# → 重编译 DLL → AppDomain reload（对标 Lua `PumpLuaScriptHotReloads()`）
6. **运行时分离** — **开发用 JIT（Mono），出货按平台选 AOT**（见 §1.1）

### 1.1 运行时策略：开发 JIT vs 出货 AOT

行业惯例（Unity / Godot 4）是**同一份 C# 游戏逻辑，开发期与发行期用不同后端**：

| 阶段 | 运行时 | 目的 |
|------|--------|------|
| **编辑器 / 本机开发** | Mono JIT（或 CoreCLR JIT） | 编译快、AppDomain 热重载、调试方便 |
| **正式发行（尤其 iOS / 主机）** | IL2CPP / NativeAOT 等 AOT | 无 JIT 合规、启动快、体积小、难逆向 |
| **桌面 Mod / 仅编辑器脚本** | 可长期 JIT | 不必 AOT |

**Mono 在本方案中的定位：开发迭代与桌面 MVP 运行时，不是终局方案。**

| 引擎 | 开发期 | 出货期 |
|------|--------|--------|
| Unity | 编辑器嵌入 Mono JIT | Player 多用 IL2CPP（iOS/主机必选） |
| Godot 4 | 桌面 CoreCLR JIT | 移动端 Mono；导出为预编译包 |
| DSEngine（规划） | S2–S4：Mono 嵌入 | S5：按平台 AOT 或继续 JIT |

---

## 二、实施阶段

### Phase S1：C ABI 中间层 ✅ 已完成

将 Lua 绑定中的引擎操作提取为纯 C 函数。**本阶段无需 Mono，即使不加 C# 也能让 Lua 绑定更干净。**

```
engine/scripting/native_api/
├── dse_api.h          ~290 行 — 函数声明（C ABI，手写权威版）
└── dse_api.cpp        ~870 行 — 实现（调用 World / AssetManager）
```

**实际交付：**

- 80+ C 函数覆盖：Entity / Transform / Camera3D / MeshRenderer / 多种 Light /
  Tree / TerrainTile / DynamicObstacle / Input / Assets / App / Metrics
- `dse_native_api_init(world, asset_mgr, audio, quit, title, get_fps, set_fps, get_draw_calls)` — 8 参数完整注入
- `dse_get_world_ptr()` — 供 gen.cpp opt-in 后访问 World
- `lua_binding_context.cpp::ConfigureBindingContext()` 已调用 `dse_native_api_init()`，Lua 与 C# 将共享同一 World
- 全函数空指针保护

**权威实现说明（S1.6 后）：**

- `dse_api.h` — 手写权威声明（含全部 `dse_*` 签名）
- `dse_api.cpp` — 手写 **L3/L5**（Context、Entity、组件 `*_add`、Input/Assets/App）
- `dse_api.gen.cpp` — Codegen **L1/L2** 组件字段 get/set（已 opt-in 构建）
- `dse_api.gen.h` — 生成对照版，不参与构建

```c
// dse_api.h — 纯 C ABI，Lua 和 C# P/Invoke / InternalCall 共用
#ifdef __cplusplus
extern "C" {
#endif

DSE_CAPI uint32_t dse_entity_create(void);
DSE_CAPI void     dse_entity_destroy(uint32_t entity);
DSE_CAPI int      dse_entity_valid(uint32_t entity);

DSE_CAPI void dse_transform_get_position(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_transform_set_position(uint32_t e, float x, float y, float z);
// ... 组件 get/set、Input、App 等见 dse_api.h

#ifdef __cplusplus
}
#endif
```

---

### Phase S1.5：Codegen 工具 ✅ 已完成（Lua 部分已 opt-in）

> **从原 S4「可选」提升为必须，原因：同时维护 Lua + C# 两套手写绑定 = 技术债。**

```
tools/codegen/
├── binding_defs.json          组件字段定义（唯一数据源，12 个组件）
├── codegen.py                 生成器主程序（含 --dry-run）
└── templates/
    ├── dse_api.h.j2           → engine/scripting/native_api/dse_api.gen.h
    ├── dse_api.cpp.j2         → engine/scripting/native_api/dse_api.gen.cpp
    ├── lua_binding.cpp.j2     → engine/scripting/lua/bindings/lua_binding_ecs_<prefix>.gen.cpp
    └── csharp_native.cs.j2    → GameScripts/DSEngine/Native.gen.cs
```

**实际交付：**

- `binding_defs.json` 覆盖 **12** 个组件：
  Transform / Camera3D / MeshRenderer / DirectionalLight / PointLight /
  SpotLight / SkyLight / Tree / TerrainTile / DynamicObstacle / NavMeshAutoRebake
- 支持字段类型：`float` / `int` / `bool` / `vec3` / `vec4` / `euler_quat` / `enum_int`（带 `dirty_flag`）；**S1.7 起增加 `string`**
- **Lua gen 已参与构建**：CMakeLists.txt 显式 opt-in 各 `lua_binding_ecs_*.gen.cpp`
- **`dse_api.gen.cpp` 已 opt-in**（S1.6）；组件字段不再手写维护
- CMake target `dse_codegen`（不依赖 `DSE_ENABLE_CSHARP`）：
  `cmake --build build_vs2022 --target dse_codegen`

**一次生成，三端同步（共 13 个文件）：**

```bash
# 更新 binding_defs.json 后执行：
python tools/codegen/codegen.py
# 或
cmake --build build_vs2022 --target dse_codegen

# 输出：
#   engine/scripting/native_api/dse_api.gen.h / dse_api.gen.cpp
#   engine/scripting/lua/bindings/lua_binding_ecs_<prefix>.gen.cpp  （10 个）
#   GameScripts/DSEngine/Native.gen.cs
```

**`Native.gen.cs` 当前范围：**

- 仅含 `binding_defs.json` 中组件字段的 get/set `InternalCall` 声明
- **不含** `dse_entity_*`、`dse_input_*`、`dse_app_*` 等 — 由 S3 手写 `Entity.cs` / `Input.cs`（P/Invoke）或扩展 Codegen 补充

---

### Phase S1.6：`dse_api.gen.cpp` opt-in ✅ 已完成

将 12 个组件的字段 get/set 从手写 `dse_api.cpp` 迁出，由 Codegen 单一维护。

**交付：**

- `dse_api.cpp` 精简至 ~280 行（Context / Entity / `*_add` / Tree 字符串过渡 / Input / Assets / App）
- `dse_api.gen.cpp` 参与构建；`enum_int` 类型支持 `DynamicObstacle.shape`
- 回归测试：`tests/gtest/unit/engine/scripting/dse_api_bindings_test.cpp`（5 用例）

---

### Phase S1.7：Codegen `string` 类型 + Tree 收敛（进行中）

**目标：** L2 字符串字段进 `binding_defs.json`，删除 `tree_ext.cpp` 过渡层。

**C ABI 约定：**

```c
DSE_CAPI void dse_<prefix>_set_<field>(uint32_t e, const char* path);
DSE_CAPI int  dse_<prefix>_get_<field>(uint32_t e, char* buf, int buf_size); // 返回写入字节数
```

**`binding_defs.json` 扩展：**

```json
{
  "name": "mesh_path",
  "type": "string",
  "buffer_size": 512,
  "lua_getter": "get_tree_mesh_path",
  "lua_setter": "set_tree_mesh_path"
}
```

**分批交付：**

| 步骤 | 内容 | 验证 |
|------|------|------|
| S1.7a | `string` 模板（C ABI / Lua / C#） | 单测 round-trip |
| S1.7b | Tree 三字段迁入 gen，删 `tree_ext.cpp` | 现有 Tree 测试 |
| S1.7c | MeshRenderer `mesh_path` + `shader_variant` | `set_mesh_path` 委托 `dse_*` |

**C# 互操作：** string getter 使用 `byte[]` 缓冲区 InternalCall；S3 可补充 `StringBuilder` 封装。远期可选 `dse_*_alloc` + `dse_string_free`（S2 前评估）。

**估时：** S1.7a–c 约 2–3 天。

---

### Phase S1.8：Lua `rendering.cpp` 大模块迁移（待启动）

**现状：** `lua_binding_ecs_rendering.cpp` ~2083 行、~100 API，**零 `dse_*` 委托**，直接访问 ECS + AssetManager + RHI。

**API 五层模型（迁移判据）：**

| 层级 | 特征 | 归属 |
|------|------|------|
| **L1** | 标量/vec/bool/enum 字段 | `binding_defs` → gen |
| **L2** | `std::string` 字段 | `binding_defs` + `string` 类型 |
| **L3** | 组件 `add` / 初始化 | 手写 `dse_api.cpp` |
| **L4** | 复合 setter（一次写多字段） | **默认仅 Lua 薄包装**调多个 `dse_*`，**不**新建 C ABI |
| **L5** | AssetManager / RHI / Physics 服务 | 保留 C++；S2 前规划 `dse_assets_*` / `dse_render_*` 最小集 |

**执行原则：**

1. 先**物理拆文件**（camera / mesh / light / post / terrain / fx），零行为变更
2. `add_*` / 简单 setter 委托已有 `dse_*`
3. PostProcess **分批**进 defs（先覆盖现有 15 个 Lua setter 涉及字段，非一次 50 字段）
4. `dse_api.gen.cpp` 在 PostProcess 入 defs **前**按组件拆分为多 TU（对齐 Lua per-component 结构）
5. **保留**全部现有 Lua API 名与参数顺序（脚本兼容）

**子模块优先级：** Camera/Light/Mesh add → PostProcess 字段化 → Water/Decal/Grass → `world_to_screen`（L5）

**估时：** 1–2 周。

---

### Phase S1.9：phys3d / gameplay3d / animation 收敛（待启动）

| 模块 | 行数 | 策略 |
|------|------|------|
| `lua_binding_ecs_phys3d.cpp` | ~943 | 标量字段 → defs；力/射线/碰撞事件 → L5 `dse_physics3d_*` |
| `lua_binding_ecs_gameplay3d.cpp` | ~978 | 模拟控制 → L5；简单组件字段 → defs |
| `lua_binding_ecs_animation.cpp` | ~788 | `danim_path` → string；FSM 状态 → L4 Lua 包装 |

**S2 前置：** 列出 L5 最小 API 清单（`set_mesh_material`、`world_to_screen`、`physics3d_raycast` 等），与 Mono 嵌入并行规划。

**估时：** 2–4 周（可与 S2 部分并行）。

---

### Phase S2：Mono 嵌入 + 逐实体脚本 + 异常隔离（待启动）

```
engine/scripting/csharp/          ← 尚未创建
├── mono_runtime.h                生命周期接口
├── mono_runtime.cpp              Init / Tick / Shutdown
├── mono_internal_calls.h         InternalCall 注册声明
└── mono_internal_calls.cpp       RegisterInternalCalls()
```

**S2 最小切片（建议顺序）：**

1. CMake 增加 `DSE_ENABLE_CSHARP`（默认 OFF）
2. Mono 加载空 `GameAssembly.dll`，P/Invoke 调通 `dse_app_get_delta_time`
3. 注册 Codegen 产生的 InternalCall（组件 get/set）
4. 实现 `DSBehaviour` 生命周期 + `SafeInvoke` 异常隔离
5. 再接入 `business_runtime_bridge` 帧循环

#### 为什么 S2 选 Mono（而非直接 CoreCLR）

| 对比项 | Mono | CoreCLR (.NET 8+) |
|--------|------|-------------------|
| 嵌入复杂度 | **低**（`mono_jit_init`、`mono_runtime_invoke`） | 高（`hostfxr`，面向启动应用而非深度嵌入） |
| InternalCall | **原生支持** | 无等价 API，主要靠 P/Invoke |
| 热重载 | AppDomain swap（Unity 编辑器同款） | AssemblyLoadContext（更复杂） |
| 游戏引擎先例 | Unity 编辑器、Godot 移动端 | Godot 4 桌面 |
| 生态趋势 | 维护模式，非 .NET 主线 | 现代 C# / 工具链主线 |
| **本阶段结论** | **S2 MVP 首选** | 留作 S5 桌面迁移或并行验证 |

Microsoft 暂无计划为 CoreCLR 提供 Mono 级 Embedding API；Godot 4 桌面用 CoreCLR、移动端仍用 Mono，说明**不存在「一个运行时走天下」**。

#### 互操作策略：InternalCall + P/Invoke 混用

| API 类别 | 推荐方式 | 原因 |
|----------|----------|------|
| 组件字段 get/set（热路径） | InternalCall → `dse_api` | 零 stub 开销 |
| Entity / Input / App / Metrics | P/Invoke → `dse_api` | 注册表小、将来可迁 CoreCLR |
| 用户脚本生命周期 | `mono_runtime_invoke` + SafeInvoke | 必须异常隔离 |

#### 关键实现：异常隔离

```cpp
static bool SafeInvoke(MonoMethod* method, MonoObject* obj, void** args) {
    MonoObject* exception = nullptr;
    mono_runtime_invoke(method, obj, args, &exception);
    if (exception) {
        MonoString* msg = mono_object_to_string(exception, nullptr);
        char* utf8 = mono_string_to_utf8(msg);
        DEBUG_LOG_ERROR("[C# Exception] {}", utf8);
        mono_free(utf8);
        return false;
    }
    return true;
}
```

#### 关键实现：逐实体 C# 脚本

**推荐**新增独立 `CSharpScriptComponent`（对齐现有 `LuaScriptComponent`），而非在 `ScriptComponent` 内塞互斥字段：

```cpp
// engine/ecs/script.h（规划扩展）
struct ScriptComponent {
    std::string script_path;  // Lua 脚本路径（现状不变）
    bool enabled = true;
};

struct CSharpScriptComponent {   // 新增
    std::string class_name;      // 如 "PlayerController"
    bool enabled = true;
    void* script_instance = nullptr;  // Mono 侧托管对象句柄
};
```

C# 侧：

```csharp
public class PlayerController : DSBehaviour {
    public override void OnAwake(uint entityId) { /* ... */ }
    public override void OnUpdate(uint entityId, float dt) { /* ... */ }
    public override void OnDestroy(uint entityId) { }
}
```

Mono 侧在每帧通过反射调用 `OnUpdate`，与 Lua 的 `CallScriptTableMethod` 逻辑对等。

---

### Phase S3：C# API 封装 + Blittable 结构体（待启动）

```
GameScripts/
├── DSEngine/
│   ├── Native.gen.cs          ← Codegen 生成（InternalCall，仅组件字段）
│   ├── Entity.cs              ← 手写（P/Invoke dse_entity_* / dse_transform_*）
│   ├── DSBehaviour.cs
│   ├── Input.cs               ← P/Invoke dse_input_*
│   ├── App.cs                 ← P/Invoke dse_app_*
│   └── MathTypes.cs           ← Vector3 / Quaternion（Blittable）
├── GameAssembly.csproj
└── 用户脚本 (.cs)
```

#### GC / Marshaling 策略

```csharp
[StructLayout(LayoutKind.Sequential)]
public struct Vector3 {
    public float X, Y, Z;
}

// 组件热路径：InternalCall + out 参数，无装箱
[MethodImpl(MethodImplOptions.InternalCall)]
internal static extern void dse_transform_get_position(uint e, out float x, out float y, out float z);

// 初始化路径：string 可接受；热路径禁止 string 返回值
[DllImport("dse_engine")]
internal static extern void dse_mesh_renderer_set_mesh(uint e,
    [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
```

---

### Phase S4：C# Assembly 热重载（待启动，依赖 S2）

> 开发体验关键，但可在 S2+S3 **最小可运行**后再做；不必与 S2 同批交付。

| | Lua | C# |
|--|-----|-----|
| 粒度 | 单文件 | 整个 DLL |
| 触发 | `.lua` 时间戳变化 | `GameAssembly.dll` 时间戳变化 |
| 状态保持 | OnSerializeState / OnDeserializeState | OnSerialize / OnDeserialize |
| 延迟 | <1ms | ~100–500ms（AppDomain 重建） |
| 实现位置 | `PumpLuaScriptHotReloads()` ✅ 已有 | `PumpCSharpHotReload()` 规划 |

```
1. 监测 GameAssembly.dll 变更
2. DSBehaviour.OnSerialize() → 收集状态
3. mono_domain_unload(game_domain_)
4. 新 MonoDomain → 加载新 DLL → 重新注册 InternalCalls
5. 重建所有 CSharpScriptComponent 实例
6. DSBehaviour.OnDeserialize() → 恢复状态
```

---

### Phase S5：出货运行时 / AOT（远期，按产品需求启动）

**仅在需要 iOS / 主机发行、或要求小体积 / 无 JIT 时启动。**

| 路径 | 适用 | 说明 |
|------|------|------|
| **继续 JIT（Mono）** | 桌面 Mod、内部工具 | 实现成本最低 |
| **.NET NativeAOT** | 桌面 / 部分服务器 | 官方工具链，反射受限 |
| **自研 IL2CPP 式管线** | 全平台统一 | 成本极高，仅大作量级考虑 |
| **CoreCLR + hostfxr** | 桌面-only 产品 | Godot 4 桌面路线；嵌入成本高于 Mono |

发行包**不必**携带 Mono 编辑器运行时；与 Unity「编辑器 Mono、Player IL2CPP」同理。

---

## 三、CMake 集成

**现状：**

- `dse_codegen` target **已存在**（仅需 Python3，与 C# 无关）
- `DSE_ENABLE_CSHARP` / `engine/scripting/csharp/` **尚未加入**

**规划（S2 落地时）：**

```cmake
option(DSE_ENABLE_CSHARP "Enable C# scripting via Mono" OFF)

if(DSE_ENABLE_CSHARP)
    find_package(Mono REQUIRED)
    target_compile_definitions(dse_engine PRIVATE DSE_ENABLE_CSHARP)

    file(GLOB_RECURSE csharp_runtime_cpp CONFIGURE_DEPENDS
         "${CMAKE_SOURCE_DIR}/engine/scripting/csharp/*.cpp")
    list(APPEND engine_cpp ${csharp_runtime_cpp})

    target_include_directories(dse_engine PRIVATE ${MONO_INCLUDE_DIRS})
    target_link_libraries(dse_engine PRIVATE ${MONO_LIBRARIES})

    # 推荐 dotnet build，而非旧版 mcs
    add_custom_target(dse_compile_csharp ALL
        COMMAND ${CMAKE_COMMAND} -E env
            dotnet build ${CMAKE_SOURCE_DIR}/GameScripts/GameAssembly.csproj
                -c Debug
                -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMENT "Compiling C# game scripts"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/GameScripts
    )
    add_dependencies(dse_engine dse_compile_csharp)
endif()
```

**`*.gen.cpp` 构建规则（当前）：**

```cmake
# 默认排除所有 *.gen.cpp
list(FILTER engine_cpp EXCLUDE REGEX ".*\\.gen\\.cpp$")

# dse_api.gen.cpp — 已 opt-in（S1.6）

# Lua gen 已 opt-in（含 navmesh_rebake；S1.7 后 tree_ext 删除，string 并入 tree.gen）
```

---

## 四、文件结构（当前 + 规划）

```
engine/scripting/
├── native_api/
│   ├── dse_api.h                     ✅ 手写权威声明
│   ├── dse_api.cpp                   ✅ L3/L5 手写实现
│   ├── dse_api.gen.cpp               ✅ L1/L2 字段（已 opt-in）
│   └── dse_api.gen.h                 对照版（排除构建）
├── csharp/                           ⏳ S2 待建
├── lua/
│   ├── bindings/
│   │   ├── lua_binding_ecs_*.gen.cpp ✅ 12 组件已 opt-in
│   │   ├── lua_binding_ecs_rendering*.cpp  ⏳ S1.8 拆分 + 迁移
│   │   └── ...（physics / audio / nav 等）
│   └── lua_runtime.cpp               ✅ PumpLuaScriptHotReloads()
└── cpp/

tools/codegen/                        ✅
GameScripts/DSEngine/Native.gen.cs    ✅（仅组件 InternalCall）
```

---

## 五、实施优先级

| Phase | 内容 | 估时 | 前置 | 状态 |
|-------|------|------|------|------|
| **S1** | C ABI `dse_api.h/cpp` | — | 无 | ✅ |
| **S1.5** | Codegen + Lua 12 组件 opt-in | — | S1 | ✅ |
| **S1.6** | `dse_api.gen.cpp` opt-in / 消除字段双份维护 | 2–3 天 | S1.5 | ✅ |
| **S1.7** | `string` 类型 + Tree/Mesh 路径收敛 | 2–3 天 | S1.6 | 🔄 a-b ✅，c 待做 |
| **S1.8** | `rendering.cpp` 拆分 + L1–L4 迁移 | 1–2 周 | S1.7 | ⏳ |
| **S1.9** | phys3d / gameplay3d / animation + L5 清单 | 2–4 周 | S1.8 | ⏳ |
| **S2** | Mono 嵌入 + DSBehaviour + 异常隔离 | 1–2 周 | S1.7 + L5 清单 | ⏳ |
| **S3** | C# API 封装（P/Invoke + Blittable） | 3–5 天 | S2 | ⏳ |
| **S4** | Assembly 热重载 | 3–5 天 | S2 最小版 | ⏳ |
| **S5** | 出货 AOT / CoreCLR 迁移 | 按平台 | 产品定稿 | 📋 远期 |

原估「3–5 天全流程」偏乐观；S2 单独含 Mono 集成与调试通常需 1–2 周。

---

## 六、依赖

| 依赖 | 用途 | 备注 |
|------|------|------|
| Mono 运行时 | S2–S4 开发期 JIT | ~30 MB；[mono-project.com](https://www.mono-project.com/) 或 vcpkg |
| .NET SDK | 编译 `GameAssembly.csproj` | 替代 `mcs` |
| Python + Jinja2 | 构建期 Codegen | 不打包到发布 |

---

## 七、Lua 绑定迁移路径

```
已完成：
  dse_api.gen.cpp（L1/L2 字段）     →  12 组件  →  已参与构建
  lua_binding_ecs_<prefix>.gen.cpp   →  委托 dse_*  →  已参与构建

S1.7（进行中）：
  string 类型                        →  Tree 路径并入 tree.gen，删 tree_ext

S1.8–S1.9（待迁移）：
  lua_binding_ecs_rendering.cpp      →  ~2083 行，~100 API，零 dse_ 委托
  lua_binding_ecs_phys3d.cpp         →  ~943 行
  lua_binding_ecs_gameplay3d.cpp     →  ~978 行
  lua_binding_ecs_animation.cpp      →  ~788 行
  lua_binding_audio.cpp / navigation.cpp / ui.cpp（ui 另立项）
```

**迁移规则：** L1/L2 → `binding_defs`；L3 → `dse_api.cpp`；L4 → Lua 薄包装（默认不进 C ABI）；L5 → 服务 API 单列里程碑。

---

## 八、技术债与残余债清单（v4）

| 问题 | v3 表述 | v4 现状 / 修正 |
|------|---------|----------------|
| 组件数量 | 10 个 | **12 个**（含 NavMeshAutoRebake） |
| `dse_api` 字段维护 | 双份手写 + gen | **S1.6 已收敛**（字段仅 gen） |
| `dse_api.cpp` 行数 | ~870 | **~280**（仅 L3/L5） |
| Lua gen | 10 个 opt-in | **12 个** opt-in，tree_ext 已删除 |
| 支持类型 | 无 string/enum | **enum_int** ✅；**string** S1.7 |

**残余债（有意识保留，须设退出条件）：**

| 类别 | 内容 | 严重度 | 退出条件 |
|------|------|--------|----------|
| SSOT 漂移 | `binding_defs` 与 C++ struct 手工同步 | 中 | CI 字段名检查（S1.8） |
| Header 双轨 | `dse_api.h` 手写 vs `dse_api.gen.h` | 低–中 | 合并声明或 gen.h include |
| API 双轨 | L4 复合 setter + 字段级 getter 并存 | 低 | 文档标明；C# 仅暴露字段级 |
| L5 未进 C ABI | `set_mesh_material`、`world_to_screen`、物理 raycast | **高（卡 S2）** | S1.9 L5 清单 + `dse_*` 最小集 |
| string 缓冲 API | C# getter 需 byte[] 缓冲 | 中 | S3 封装层 / 可选 alloc API |
| gen 单 TU | `dse_api.gen.cpp` 随组件增大 | 低–中 | PostProcess 前按组件拆分 |
| rendering 直连 ECS | ~2083 行未走 dse_api | 高 | S1.8 完成 |
| C# 运行时 | Mono / `DSE_ENABLE_CSHARP` 未实现 | 中 | S2 |
| 全平台 AOT | 出货策略未定 | 低 | S5 按产品需求 |

**已清偿：**

- ~~`dse_api.cpp` 与 `dse_api.gen.cpp` 字段双份维护~~（S1.6）
- ~~`tree_ext.cpp` 过渡层~~（S1.7a-b ✅）

---

## 九、参考：主流引擎 C# 运行时对照

| 引擎 | 开发期 | 出货期 |
|------|--------|--------|
| Unity | 编辑器 Mono JIT | Player IL2CPP（多数平台） |
| Godot 4 | 桌面 CoreCLR JIT | 移动 Mono；导出预编译 |
| DSEngine | Mono JIT（S2 规划） | NativeAOT / 平台 AOT（S5 规划） |

同一份游戏 C# 代码，开发用 JIT 迭代，出货用 AOT 打包 — 这是行业常态，并非 DSEngine 特例。

---

## 十、脚本选型建议（Lua vs C#）

| 场景 | 推荐 |
|------|------|
| **全平台游戏逻辑、当前可开发** | **Lua**（`ScriptComponent` 已可用，绑定广，打包简单） |
| **iOS / 主机发行** | **Lua**（标准解释器无 JIT 禁令；C# 需 S5 AOT） |
| **编辑器工具 / 内部管线** | C#（S2 完成后可选） |
| **桌面 Mod、团队以 C# 为主** | C#（接受 Mono 体积与日后 AOT 成本） |

**结论：**

- **Lua 是当前实用首选** — 能写玩法、能热重载、开发与出货同一套模型，无 Mono 运行时负担。
- **C# 是可选增强，不是 Lua 替代品** — `dse_api` / Codegen 仍值得保留（净化 Lua 绑定、为将来 C# 铺路），但不必为「能否做游戏」而急于完成 S2。
- C# 接入的合理触发条件：编辑器需要 C# 扩展、明确桌面-only 产品、或团队技能栈以 C# 为主。
