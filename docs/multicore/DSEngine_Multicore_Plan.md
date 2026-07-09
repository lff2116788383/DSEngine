# DSEngine 最大化多核 CPU 利用 — 完整实现方案

> 目标读者:引擎/并发方向开发者（人或 AI）。
> 基线分支:`feature/engine-lib`。
> 本文所有代码引用均已核对当前工作区源码。

---

## 0. 前提与目标校准

**"100% 占满多核"既不可达也不应追求**——同步点、内存带宽、GPU 提交、Amdahl 定律共同限制上限,且占满会吃掉调度与热余量。

现实目标（对标 UE TaskGraph / Unity DOTS）:
- 可并行的帧工作接近线性加速;
- 串行段压到最短;
- 8 核整体利用率进入 **50~70%** 区间即为一流水准。

---

## 1. 当前状态（已核对）

| 子系统 | 现状 | 证据 |
|---|---|---|
| JobSystem | 单一 `queue_mutex_` + 全局优先级队列;每任务一个 promise + map 记账;`hardware_concurrency()-1` 个 worker | `engine/core/job_system.cpp` Init/Submit/ParallelFor |
| JobSystem 工作窃取 | **死代码**:`local_queues_` 从没被 push,`StealJob` 永远返回空 | `job_system.cpp` 全部 push 到全局 `job_queue_` |
| 帧循环 | `FixedUpdate → Update(N) → Render(N)`;`Render()` 先 `WaitForComplete(N-1)` 再 `Prepare(N)` 再 `SignalNewFrame(N)` 后立即返回 → **已是 1 帧延迟流水线**,Update(N+1) 与 render(N) 并发 | `engine/runtime/engine_app.cpp` Tick;`frame_pipeline.cpp:1090`;`render_thread_manager.cpp:67-105` |
| gameplay | 20+ 系统串行跑主线程,仅 2 处 `ParallelEach` | `modules/gameplay_3d/gameplay_3d_module.cpp:203,236` |
| 骨骼矩阵 | 裸 `std::thread` 手动分片,绕过 JobSystem | `modules/gameplay_3d/animation/animator_system.cpp:701-715` |
| RenderGraph | Pass 间完全串行 | `engine/render/render_graph.cpp:510` 单循环遍历 `compiled_order_` |
| 物理 | Jolt 独立 `JobSystemThreadPool`,默认 2 线程,`FixedUpdate` 在主线程串行 | `engine/physics/physics3d/physics3d_system_jolt.cpp:288`;`physics3d_system_jolt.h:28` |

**已澄清的误区**:分析报告称"渲染时主线程完全闲置/无帧管线化"是**错误**的——引擎已重叠 Update(N+1) 与 render(N)。**严禁把 `WaitForComplete` 移到 `Update` 开头**,那会缩小重叠、造成负优化。

---

## 2. 分阶段方案（顺序即依赖顺序）

### Phase 0 — 测量与基建（先决条件）
没有基线的优化是赌博。
- **插桩**:用现有 `rs_->cpu_profiler_` 打出每阶段（FixedUpdate / 各 gameplay system / PrepareRender / 各 RenderPass）耗时 + 每 worker 忙/闲占比,导出 Chrome-tracing (`chrome://tracing`)。
- **基准场景**:建高负载场景（1k / 10k / 50k 实体;粒子/AI/动画各一档）。
- **正确性基建**:ThreadSanitizer 构建配置 + 确定性校验 harness（同输入跑两次比对世界状态哈希）。后续每个并行化改动都必须过这两关。
- **产出**:一张"各阶段耗时占比 + 当前利用率"真实数据表,用来裁掉后面收益为负的项。

### Phase 1 — 重写 JobSystem 为高性能调度器（核心地基）
真瓶颈是单锁 + 每任务 promise + map 记账,细粒度任务扩展性差。
- **每 worker 本地 deque + 真正的工作窃取**:`Submit` 推到调用线程本地队列,空了从别人队尾偷;全局队列仅作跨线程溢出/优先级入口。
- **ParallelFor 用单个原子计数 barrier** 取代"N 个 promise + N 次 Wait",消除每批 map 插入与堆分配。
- **调用者 helping**:线程在 `Wait`/`ParallelFor` 阻塞时主动帮忙执行队列任务,而不是干等。
- **轻量依赖**:JobHandle 用原子计数器表达依赖（fetch_sub 到 0 即入队),去掉全局锁内的 `completion_signals_`/`completed_jobs_` map。
- 热路径零分配（对象池化 JobEntry）。
- **验收**:`job_system_test`/`job_system_stress_test` 通过 + 微基准显示细粒度吞吐显著优于旧版;TSan 全绿。

### Phase 2 — 统一线程模型,消除超订
让所有并行工作共用同一 worker 池（大小 = `hardware_concurrency`）。
- `AnimatorSystem::ComputeFinalMatrices` 与 `LightmapBaker` 的裸 `std::thread` → 全部改走 JobSystem。
- **Jolt 接入统一池**:实现一个把 Jolt job 转发到我们池的 `JobSystem` 适配器,替换独立的 2 线程池;物理从此弹性伸缩且不与 gameplay 抢核。
- 渲染线程保留（持有 GPU context),其 CPU 侧录制可 dispatch 到池。
- **验收**:进程内不再出现"线程数 > 核数"的超订;物理压力场景吞吐随核数提升。

### Phase 3 — 并行 ECS 系统调度器（收益最大的结构性改动）
把 gameplay 从"主线程串行跑 20+ 系统"变成"自动并行"。
- 每个 System 声明**组件读写集** `ComponentAccess{ reads, writes }`。
- 每帧构建**系统依赖图**:两系统冲突当且仅当一方写了另一方读/写的组件;无冲突系统并行提交（带依赖）。取代 `gameplay_3d_module.cpp` 的手写顺序。
- **系统内**再用 chunk 化 `ParallelEach` 做逐实体数据并行。
- **结构性变更**（建/删实体、加/删组件）不能并发改 archetype → 用 command buffer 延迟到帧内同步点统一 flush;事件发射走线程安全队列。
- **验收**:高实体场景 gameplay 段耗时随核数近线性下降;确定性 harness + TSan 全绿。

### Phase 4 — 深化帧流水线
- **保留** 1 帧延迟渲染流水线,**不要动 `WaitForComplete` 位置**。
- 并行化 `PrepareRenderFrame` 内部:光源裁剪 / cluster 构建 / 快照拷贝用 `ParallelFor`（先用 Phase0 数据确认这段是 CPU 瓶颈而非内存带宽瓶颈）。
- 视数据依赖,让本帧物理与"不依赖本帧物理结果"的动画/AI 在池上重叠。
- **验收**:`frame_pipeline_test`/`frame_pipeline_resize_test` 通过;渲染输出逐像素一致。

### Phase 5 — 并行渲染录制（高成本,依赖 RHI）
- 利用 `RenderGraph::Compile` 已建的 DAG,让无依赖 Pass（Spot/Point 阴影图、G-buffer 子任务）并行录制到多个 secondary command buffer。
- **前置条件**:RHI 后端支持多线程命令录制（D3D12/Vulkan 可,**GL 后端天生不行**,GL 路径保持串行）。收益只在 Vk/D3D12 兑现。

### Phase 6 — 数据导向硬化（贯穿）
- 组件 SoA / archetype chunk 布局,并行迭代缓存友好、避免 false sharing。
- 扩展现有 per-thread `ThreadScratch` 线性分配器;调优 batch size。

---

## 3. 里程碑依赖与预期收益

| 阶段 | 依赖 | 主要收益 | 风险 |
|---|---|---|---|
| 0 测量 | — | 决策依据,防盲目优化 | 低 |
| 1 JobSystem | 0 | 一切并行的地基;细粒度吞吐↑ | 中 |
| 2 统一池 | 1 | 消除超订;物理弹性伸缩 | 中 |
| 3 ECS 调度器 | 1,2 | **gameplay 段近线性加速（最大头）** | 高 |
| 4 流水线深化 | 1 | 主线程串行段↓ | 中 |
| 5 并行渲染 | 1 | 渲染段↓（仅 Vk/D3D12） | 高 |
| 6 数据导向 | 3 | 缓存/带宽效率 + 稳定加速 | 中 |

做完 1~4,8 核上可并行帧工作接近线性、整体利用率进入 50~70%;5/6 再啃渲染与带宽。

---

## 4. 必须持续背负的债务

1. **并发正确性**:每个并行化 PR 必须过 TSan + 确定性 harness,否则埋 data race。
2. **确定性**:并行浮点归约/事件顺序会破坏帧确定性——有网络/回放需求的系统需强制固定归约顺序。
3. **API 表面**:System 新增读写集声明,是一次全模块接口迁移成本。
4. **GL 后端**:Phase 5 在 GL 上无解,需接受该后端渲染串行。
