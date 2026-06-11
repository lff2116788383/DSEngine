# DSEngine 内存管理系统设计方案（v2）

> 目标分支：`feature/engine-lib`
> 状态：**设计已评审，进入实现**
> 起始基线：`feature/engine-lib @ ec7e93f2`
> v2 相对 v1 的变化：吃进了批判性评估的 5 条修正（热路径去虚化、追踪口径、帧分配器与多线程渲染的 fence、`MemoryTag` 运行期可扩展、跨 DLL 真实边界），并把单例边界、Reallocate 对齐、Debug 追踪争用等隐性债显式写清。

---

## 0. TL;DR

当前引擎只有「语言级 RAII + 智能指针」加「资产层 LRU 内存预算」，缺少**引擎级统一分配器、临时帧分配器、对象池、分配追踪/预算**。本方案补齐一套**分层内存子系统**：

- 统一门面 `dse::core::Memory` + 可替换后端（`IAllocator`，仅用于「通用堆」这一层）。
- 三类核心分配器：**通用堆**（system / 可选 mimalloc）、**线性帧分配器**（每帧重置、与渲染快照 fence 对齐、含每线程 scratch）、**固定大小池**（替换零使用的死代码 `memory_pool.h`、重写 `object_pool.h`）。
- **标签化追踪 + 子系统预算 + 泄漏报告**（Debug 默认开、Release 默认关），口径明确。
- **STL 适配器**（仅提供、不推广）+ 后续可选 **handle 化**。
- 解决**跨 DLL 分配/释放**所有权（引擎可编为 `DSEngine.dll`）。

原则：**首版接入后行为与现状等价**（门面转发 malloc），随后逐子系统把热路径切到帧分配器/池。

---

## 1. 现状与问题（实现依据）

| 维度 | 现状（代码实证） | 问题 |
|------|------------------|------|
| 通用分配 | 直接 `new`/`make_shared`/STL，无全局 `operator new` 覆盖 | 无法集中追踪/限额/换后端 |
| 临时/每帧 | 无；每帧 `std::vector`/`make_shared` 反复 malloc/free | 堆抖动、帧卡顿、不可预测 |
| 对象池 | `engine/core/memory_pool.h`、`object_pool.h` **零实例化（死代码）** | 既无收益又误导；实现本身有缺陷（每次分配加锁/按值拷贝） |
| 资产 | `AssetManager`：`shared_ptr` 缓存 + LRU + `SetMemoryBudget(bytes)` | ✅ 唯一成体系预算，但仅限资产 |
| ECS | EnTT 内部池 | 不归引擎自管（可接受） |
| 第三方 | Jolt/PhysX/Box2D/miniaudio/Recast 各自分配；PhysX 已接对齐分配回调 | 分散、无统一视图 |
| 追踪 | 无（资产字节预算除外） | 泄漏/增长不可见 |
| 跨模块 | 引擎可编 `DSEngine.dll`；`CMakeLists.txt:404` 注释："Gameplay3D 全部编入 dse_engine，避免跨 DLL EnTT 模板实例化导致的内存损坏" | 跨 CRT 边界 new/delete 是已知崩溃源 |

---

## 2. 总体架构

```
调用方：render / ecs / assets / scripting / physics / job …
   ├─ 通用堆分配  ─────────────► dse::core::Memory  (门面, DSE_EXPORT)
   ├─ 帧临时数据  ─────────────► Memory::Frame()      → FrameAllocator (具体类型, 非虚)
   ├─ 每线程 scratch ──────────► Memory::ThreadScratch() → LinearAllocator (具体类型, 非虚)
   └─ 高频同类对象 ────────────► ObjectPool<T> / PoolAllocator (具体类型, 非虚)

   Memory 门面 ──选择──► IAllocator (虚, 仅"通用堆"层可替换)
                          ├─ SystemAllocator   (默认)
                          └─ MimallocAllocator (可选, DSE_MEM_BACKEND=mimalloc)
                          (Debug 下可包一层 TrackingAllocator)

   MemoryTracker (统计/预算, 每线程缓冲+周期合并)   MemoryBudget (按 tag)
```

**关键设计取舍（v2）**：
- **热路径去虚化**：`LinearAllocator` / `FrameAllocator` / `PoolAllocator` / `ObjectPool<T>` 都是**具体类型，无虚函数**，调用点直接持有具体类型；虚的 `IAllocator` 只用于「通用堆后端可替换」。避免每次分配一次虚调用。
- **门面薄**：无追踪时 `Memory::Alloc` 是对后端的薄转发（可内联）。

---

## 3. 模块与 API

> 命名空间 `dse::core`，文件位于 `engine/core/memory/`。公共类型加 `DSE_EXPORT`。

### 3.1 `MemoryTag`（运行期可扩展）

内置 tag 用 enum 给定固定 id；插件/模块可在运行期注册新 tag，拿到一个 `uint16_t` id。统计数组按当前已注册数量增长。

```cpp
enum class MemoryTag : uint16_t {
    Default = 0, Render, RHI, Texture, Mesh, Material, Shader,
    Asset, Audio, Physics, ECS, Scene, Scripting, Net, Navigation,
    UI, Editor, Job, FrameTemp, BuiltinCount
};
// 运行期扩展（供 plugins/ 使用）：
uint16_t RegisterMemoryTag(const char* name);   // 返回 >= BuiltinCount 的 id
const char* MemoryTagName(uint16_t id);
inline uint16_t TagId(MemoryTag t) { return static_cast<uint16_t>(t); }
```

### 3.2 `IAllocator`（仅通用堆层）

```cpp
class IAllocator {
public:
    virtual ~IAllocator() = default;
    virtual void* Allocate(size_t size, size_t alignment) = 0;
    virtual void  Deallocate(void* ptr) = 0;
    virtual void* Reallocate(void* ptr, size_t new_size, size_t alignment) = 0;
    virtual const char* Name() const = 0;
};
```

- **块头（block header）**：通用堆在每次分配前置一个紧凑 header（`size + alignment + tag + magic`），因此 `Deallocate` 无需调用方再传 size，统计与校验可靠（修正 v1 的 `size=0` 含糊）。header 大小计入对齐回退。
- **Reallocate + 对齐跨平台**：POSIX 无 aligned realloc、Win 的 `_aligned_realloc` 语义特殊，统一实现为 `alloc(new)+memcpy(min(old,new))+free(old)`，对齐由 header 记录的原 alignment 保证。

### 3.3 门面 `Memory`

```cpp
class DSE_EXPORT Memory {
public:
    static void Init(const MemoryConfig& cfg = {});   // EngineInstance 最早期调用
    static void Shutdown();                            // 最末期；内部先 ReportLeaks

    // 通用堆
    static void* Alloc(size_t size, MemoryTag tag = MemoryTag::Default);
    static void* AllocAligned(size_t size, size_t alignment, MemoryTag tag = MemoryTag::Default);
    static void* Realloc(void* ptr, size_t new_size, MemoryTag tag = MemoryTag::Default);
    static void  Free(void* ptr);

    // 帧临时 / 每线程 scratch（返回具体类型，非虚）
    static FrameAllocator&  Frame();
    static LinearAllocator& ThreadScratch();
    static void* FrameAlloc(size_t size, size_t alignment = 16);

    // 统计 / 预算（查询接口可经 ServiceLocator 暴露给编辑器）
    static MemoryStats Stats(uint16_t tag);
    static MemoryStats TotalStats();
    static void SetBudget(uint16_t tag, size_t bytes);
    static void ReportLeaks();

    static IAllocator& Heap();  // 高级用法
};

template<class T, class... Args> T* New(MemoryTag tag, Args&&... a); // Heap + 构造 + 标签
template<class T> void Delete(T* p);                                  // 析构 + Heap::Free
```

### 3.4 后端 `SystemAllocator` / `MimallocAllocator`

- `SystemAllocator`：Win=`_aligned_malloc/_aligned_free`，POSIX=`posix_memalign/free`。**默认**，零新增依赖。
- `MimallocAllocator`：可选，`DSE_MEM_BACKEND=mimalloc`，默认 **OFF**（不动依赖树）。

### 3.5 `LinearAllocator` + `FrameAllocator`（与渲染快照 fence 对齐）

- `LinearAllocator`（具体类型）：一块缓冲，`Allocate` 移动偏移，仅支持 `Reset()` / `Mark()`+`Rewind()`；对齐感知；越界**回退到堆并告警**（不直接崩）。
- `FrameAllocator`：**N 缓冲，N = 渲染帧延迟 + 1**。

> **硬约束（修正 v1 仅"双缓冲"的疏漏）**：`PARALLELIZATION_PLAN` 的主循环是「主线程 `CaptureThinSnapshot` → 等 `render_done` → `FlipSnapshotIndex` → 提交渲染 Job 读 `read_snapshot()`」。若帧临时内存被渲染快照引用，则该帧缓冲必须存活到对应帧的 `render_done`。因此：
> - 帧分配器的缓冲 index **与 snapshot index 绑定**，在 `FlipSnapshotIndex()` 处一起翻页；
> - 只有在某帧的 `render_done==true` 之后，才 `Reset()` 该帧缓冲；
> - 单线程渲染时 N=2 足够；启用并行渲染（帧延迟 1）时 N=2 仍可（因主线程在 Flip 前已等 render_done），但**预留 N 可配**以支持未来更深流水线。
> - 渲染线程内若要再分配临时内存，用渲染线程自己的 `ThreadScratch`，不碰主帧分配器。

- `ThreadScratch`：每线程一个小 `LinearAllocator`，挂在 JobSystem worker 与主/渲染线程上，经 `thread_local` 指针访问；任务/帧出口 `Reset`。

### 3.6 `PoolAllocator` + 重写 `ObjectPool<T>`（替换死代码）

- `PoolAllocator`（具体类型）：固定块大小 + free-list + 分块增长，块对齐可配；提供**无锁单线程版**与**加锁版**两种（避免旧 `MemoryPool` 每次分配都加锁）。
- `ObjectPool<T>`：**原地存储**，`Acquire(args...)` placement-new 返回 `T*`，`Release(T*)` 调析构并回收（修复旧版「按值拷贝、只能存 trivial 句柄」）。

```cpp
template<class T> class ObjectPool {
public:
    explicit ObjectPool(size_t initial = 0, MemoryTag tag = MemoryTag::Default);
    template<class... Args> T* Acquire(Args&&... a);
    void Release(T* p);
    size_t LiveCount() const; size_t FreeCount() const;
};
```

### 3.7 追踪 `TrackingAllocator` + `MemoryTracker`（口径明确 + 低争用）

- **口径**（修正 v1 含糊）：默认只统计**经门面的分配**（引擎自管部分）。第三方库与未改造代码**不在内**——编辑器/日志面板**显式标注口径**为"引擎门面视图"。
- **可选全量**：`DSE_MEM_GLOBAL_NEW_OVERRIDE`（Debug，默认 OFF）覆盖全局 `operator new/delete`，把全量纳入统计；开启后口径标注为"全量"。
- **低争用**：计数走**每线程缓冲 + 周期合并**或纯原子；逐指针登记仅在 `DSE_MEM_TRACK_POINTERS` 时开启（定位泄漏用），避免全局 map+锁在 Debug 改变时序、掩盖竞态。

```cpp
struct MemoryStats { size_t current, peak, alloc_count, free_count; };
```

### 3.8 子系统预算 `MemoryBudget`

- `SetBudget(tag, bytes)`；超限回调（告警 / 触发资产 LRU 淘汰 / 拒绝，策略可配）。
- 对接 `AssetManager::SetMemoryBudget`，把资产预算纳入统一视图，不改其行为。

### 3.9 STL 适配器 `StlAllocator<T, Tag>`（仅提供，不推广）

无状态、tag 为模板非类型参数；用于新代码 / 高频容器按需替换。

> **硬规矩（修正 v1 的类型传染风险）**：`DseVector<T,Tag>` 与 `std::vector<T>` 是不同类型。**跨接口/模块边界一律用 `std` 默认分配器容器**，`Dse*` 容器只在实现内部局部使用，避免类型传染与到处转换。

### 3.10 （后续阶段）Handle 化

`Handle<T>`（index+generation）+ 资源表，逐步替换 RHI/资产热路径的 `shared_ptr`，去原子计数。**不在首版范围**。

---

## 4. 与现有系统集成点

### 4.1 生命周期（单例边界明确）
- `EngineInstance` 最早期 `Memory::Init(cfg)`（早于任何子系统注册）；最末期 `Memory::Shutdown()`（内部先 `ReportLeaks`）。
- `Memory` 为**进程级**（早于 `ServiceLocator`，故不注册为服务）。**边界限定：门面只负责"分配 + 统计采集"，不持有业务状态/策略**；统计与预算的**查询/配置**接口可经 `ServiceLocator` 暴露给编辑器，便于测试注入与解耦（缓解与"去单例"理念的冲突）。

### 4.2 JobSystem（每线程 scratch + 修裸 new）
- `WorkerThread(index)` 启动建线程局部 `LinearAllocator`；任务边界 `Reset`。
- 顺手修 `job_system.cpp:118/158/186` 的 `new std::promise/std::atomic` 裸 new，改池/门面管理，明确所有权。

### 4.3 FramePipeline（帧边界 + fence）
- 帧循环在 `FlipSnapshotIndex()` 处翻页帧分配器缓冲；某帧缓冲在其 `render_done` 后才 `Reset`（见 §3.5）。

### 4.4 AssetManager（预算对接）
- 保留 LRU/`SetMemoryBudget` 行为，内部把字节统计上报 `MemoryTracker(Tag::Asset/Texture/...)`。

### 4.5 RHI / 渲染
- GPU 资源仍由 RHI 对象 RAII 释放；CPU 侧元数据/上传暂存走门面与帧分配器；对齐统一交 `AllocAligned`。

### 4.6 跨 DLL（真实边界，修正 v1 过度乐观）
- 门面 + 后端实例只存在于 `dse_engine`，导出 `Memory::*`；引擎对象的 new/delete 统一走门面 → 解决跨 CRT 释放问题。
- **但**：头文件里的 `std` 容器在各模块仍用各自 CRT 分配。**因此立硬规矩**：不跨 DLL 传"持有所有权的 `std` 容器"（要么按值在同模块析构，要么传 view/span，要么用引擎导出的接口）。要"全量自动安全"则需开 `DSE_MEM_GLOBAL_NEW_OVERRIDE` 覆盖所有模块的全局 new（代价见 §3.7）。当前默认 STATIC 构建下此问题不触发；SHARED 构建前按本节规矩审查。

---

## 5. 配置与编译开关（CMake）

```
DSE_ENABLE_MEM_TRACKING      Debug=ON / Release=OFF   # 标签计数 + 泄漏报告
DSE_MEM_TRACK_POINTERS       OFF                       # 逐指针登记（定位泄漏时开）
DSE_MEM_GLOBAL_NEW_OVERRIDE  OFF                       # 覆盖全局 new（全量口径/跨DLL）
DSE_MEM_BACKEND              system | mimalloc (system)
DSE_MEM_FRAME_KB             16384                     # 每帧线性缓冲大小
DSE_MEM_FRAME_BUFFERS        2                         # 帧缓冲 N（= 帧延迟+1）
DSE_MEM_SCRATCH_KB           256                       # 每线程 scratch
```

源码 `engine/core/memory/*.cpp` 由 `file(GLOB_RECURSE engine_cpp ... "engine/*.cpp")`（`CMakeLists.txt:307`）自动纳入，新增文件重跑 configure 即可。

---

## 6. 文件布局

```
engine/core/memory/
├── memory.h / memory.cpp            # 门面 + MemoryConfig
├── allocator.h                      # IAllocator + MemoryTag(+运行期注册)
├── system_allocator.h/.cpp          # 默认后端
├── mimalloc_allocator.h/.cpp        # 可选后端
├── linear_allocator.h/.cpp          # LinearAllocator(具体类型)
├── frame_allocator.h/.cpp           # FrameAllocator(N缓冲) + ThreadScratch
├── pool_allocator.h/.cpp            # 固定大小池(具体类型)
├── object_pool.h                    # 重写(原地存储)；替换旧 object_pool.h
├── tracking_allocator.h/.cpp        # TrackingAllocator
├── memory_tracker.h/.cpp            # MemoryTracker + Budget(每线程缓冲)
├── stl_allocator.h                  # StlAllocator + Dse* 别名(仅内部用)
└── handle.h                         # (后续) Handle<T>

删除：engine/core/memory_pool.h（死代码）；object_pool.h 迁入 memory/ 并重写
```

---

## 7. 落地阶段（每阶段：实现 → 补测试 → 更新文档 → 提交推送）

1. **骨架**：`MemoryTag`(+运行期注册) + `IAllocator`(块头) + `SystemAllocator` + `Memory` 门面（转发，含 New/Delete）+ `Init/Shutdown` 接 `EngineInstance`。**行为零变化**。 ✓ **已完成**
   - 文件：`engine/core/memory/{allocator,system_allocator,memory}.{h,cpp}`；`EngineInstance` 构造最早期调用 `Memory::Init()`。
   - `SystemAllocator` 用块头记录 size/对齐/标签 + magic 校验；`Reallocate` = alloc+copy+free（跨平台对齐安全）；总量统计用原子。
   - 测试：`tests/gtest/unit/engine/core/memory_test.cpp`（11 例：标签注册、对齐、块头/统计、Realloc 保数据、门面 New/Delete 构析、空指针安全）。`ctest --preset windows-x64-debug` 三套全绿。
2. **追踪**：`TrackingAllocator` + `MemoryTracker`(每线程缓冲) + `ReportLeaks` + CMake 开关（含全局 new 覆盖开关）。 ✓ **已完成**
   - 文件：`engine/core/memory/{memory_tracker,tracking_allocator}.{h,cpp}`；`Memory::Init` 在启用追踪时把默认堆包进 `TrackingAllocator`。
   - `MemoryTracker`：按标签原子计数（current/peak/alloc/free，桶数 `kMaxTrackedTags=256`），`Report` 输出按标签占用/泄漏并**标注口径为「engine-facade-only」**（第三方/全局分配不计入）。
   - `TrackingAllocator`：自带紧凑块头（inner 基址/size/tag），**后端无关**（可叠加在 SystemAllocator 或未来 mimalloc 上），保持用户对齐。
   - 门面新增 `Stats(tag)` / `TrackingEnabled()`；`ReportLeaks` 启用追踪时走按标签报告。
   - CMake：`DSE_ENABLE_MEM_TRACKING`（默认 ON，仅 `$<CONFIG:Debug>` 注入定义，Release 零开销）、`DSE_MEM_TRACK_POINTERS`（默认 OFF，逐指针登记定位泄漏）。
   - 测试：新增 7 例（按标签计数/峰值、装饰器对齐与上报、Realloc 保数据、门面按标签统计）。`ctest` 三套全绿。
   - **未做**：`DSE_MEM_GLOBAL_NEW_OVERRIDE`（全量口径/跨 DLL 自动安全）暂未实现——全局 `operator new/delete` 覆盖风险高且需独立验证，留待后续单独提交；当前口径为「门面视图」。
3. **帧/线性**：`LinearAllocator` + `FrameAllocator`(N缓冲, fence 对齐) + `ThreadScratch`，接 FramePipeline / JobSystem；修 job_system 裸 new。 ✓ **已完成**（裸 new 留阶段4）
   - 文件：`engine/core/memory/{linear_allocator,frame_allocator}.{h,cpp}`。
   - `LinearAllocator`：bump-pointer，具体类型**无虚函数**；溢出**不崩溃**（返回 nullptr + 计数 + 告警，调用方退回堆）；支持 `Mark/Rewind/Reset/HighWater`。
   - `FrameAllocator`：N 缓冲轮转（默认 N=2，`kMaxFrameBuffers=4`），`BeginFrame()` 推进+复位。
   - **fence 对齐**：`FramePipeline::Render` 在 `WaitForRenderComplete()` 之后才 `Memory::Frame().BeginFrame()`——保证渲染线程已消费完上一帧快照对应缓冲，杜绝 use-after-reset；单线程路径帧首复位。当前为行为中性接入点（尚无消费者），由 smoke 测试每帧实际跑到。
   - `Memory::Frame()`（进程级主线程帧分配器，按 `MemoryConfig` 惰性初始化）+ `Memory::ThreadScratch()`（`thread_local`，每线程私有零争用）；`MemoryConfig` 增 `frame_buffer_bytes/frame_buffer_count/scratch_bytes`。
   - `JobSystem::WorkerThread` 每个任务执行后 `Memory::ThreadScratch().Reset()`（任务级瞬时内存复位）。
   - 测试：新增 8 例（线性 bump/对齐/溢出/Reset/Mark-Rewind、帧分配器轮转复位/缓冲数夹取、ThreadScratch 8 线程屏障并发无串扰且缓冲互异）。`ctest` 三套全绿（含 smoke 跑帧边界复位）。
4. **池**：`PoolAllocator` + 重写 `ObjectPool`，删死代码 `memory_pool.h`，迁 1–2 个高频点示范。 ✓ **已完成**
   - 文件：`engine/core/memory/pool_allocator.{h,cpp}`（具体类型、无虚函数）。
   - `PoolAllocator`：固定大小块 + 侵入式空闲链表（空闲块复用为 next 指针），O(1) 分配/回收、**无碎片**；容量不足按 chunk 增长（底层经 Memory 门面申请），`Shutdown` 释放全部 chunk；块步长自动抬升至 `>= sizeof(void*)` 并按对齐取整。
   - **重写 `ObjectPool<T>`**（`engine/core/object_pool.h`）：由旧的「按值拷贝存储 + 工厂」改为 **PoolAllocator 支撑 + 原地 placement-new**——`Acquire(args...)` 在池内存上构造并返回 `T*`，`Release(T*)` 显式析构后归还；支持**不可拷贝/重对象**，无拷贝开销。
   - **删除死代码** `engine/core/memory_pool.h`（每次分配加锁、只增不减、无对齐），并从 `engine/dse.h` 移除其 include，改纳入 `memory/memory.h` + `memory/pool_allocator.h`。
   - **示范接入**：`JobSystem` 完成信号 `std::promise<void>` 由每次 `new/delete` 改为 `ObjectPool<std::promise<void>> promise_pool_`（`Acquire/Release` 全在 `queue_mutex_` 保护下，线程安全）。
   - 测试：原 `math_pool_test.cpp` 的 MemoryPool/ObjectPool 旧用例迁出，新增 6 例于 `memory_test.cpp`（池：分配/回收复用无碎片、超容量自动扩容保对齐、块步长下限与对齐；对象池：原地构造/析构 live 计数、借还千次不泄漏不增容、支持不可拷贝类型）。`ctest` 三套全绿。
5. **预算 + 资产对接**：统一预算视图，`AssetManager` 上报；数据查询接口 + 日志（不做 UI 面板）。
6. **STL 适配器 + 标签化**：仅提供 + 示范，按硬规矩约束使用边界。
7. **（可选后续）** mimalloc 后端 + Handle 化热路径。

每阶段一个提交/PR，独立可回滚；首版（1–4）即构成可用地基。

---

## 8. 测试（GoogleTest，`tests/gtest/`）

- 单测：各分配器对齐/边界/Reset/Mark-Rewind；块头记录的 size/alignment/tag 正确；池 Acquire/Release 的构造析构次数（用计数类型）；ObjectPool 原地存储正确性；Tracker 计数/峰值/泄漏报告；运行期 tag 注册。
- 多线程：ThreadScratch 零争用；FrameAllocator 在「翻页—render_done—Reset」时序下无 use-after-reset / data race（配合 MSVC `/fsanitize=address`）。
- 集成：引擎 Init→若干帧→Shutdown，`ReportLeaks()` 为零；资产加载/卸载预算统计对齐。
- 回归：`ctest --preset windows-x64-debug`（unit/integration/smoke）全绿。

---

## 9. 性能与开销

- Release 默认：门面薄转发、追踪关闭 → 与现状基本等价。
- 帧分配器：每帧 N 次 malloc/free → 指针自增 + 每帧一次 Reset。
- 池：O(1) alloc/free，无碎片。
- 追踪（Debug）：每线程缓冲计数，周期合并；逐指针登记仅按需开。

---

## 10. 残留技术债（诚实清单）

**刻意推迟、可接受：**
- Handle 化推后 → 热路径 `shared_ptr` 原子计数开销仍在（已标注）。
- mimalloc 默认 OFF → 碎片/多线程吞吐上限收益未拿满（按需开）。
- 无 compaction/碎片整理 → 通用堆长期碎片化风险（主流引擎多数也不做整理，靠分配器策略，属可接受）。

**已正视并写入设计：**
- `Memory` 进程级单例与"去单例"理念的冲突 → 用 §4.1 的「门面只管分配、查询/配置经 ServiceLocator」边界缓解。
- 跨 DLL **部分解决** → §4.6 立硬规矩 + 可选全局 new 覆盖，不再过度乐观。
- 追踪口径 → §3.7 明确"门面视图 vs 全量"，面板标注。
- Debug 追踪争用 → 每线程缓冲，避免全局锁掩盖竞态。

---

## 11. 验收标准

- [ ] `ctest --preset windows-x64-debug` 全绿（含新增内存单测/集成测试）。
- [ ] Debug 下引擎正常启停，`ReportLeaks()` 无泄漏。
- [ ] FramePipeline/JobSystem 接入帧/scratch 分配器后运行稳定，无 use-after-reset。
- [ ] 死代码 `memory_pool.h` 移除，`ObjectPool` 重写并有示范使用点。
- [ ] 可经接口/日志查看按标签的内存统计（含口径标注）。
- [ ] Release 构建性能无明显回退。
