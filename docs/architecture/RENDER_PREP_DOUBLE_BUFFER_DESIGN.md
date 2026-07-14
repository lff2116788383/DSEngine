# 渲染准备状态双缓冲设计与风险评估（P1b）

> 状态：**已分析，暂不实现**。本文档给出把 `PrepareRenderFrame(N)` 与渲染线程
> `ExecuteRenderFrame(N-1)` 安全重叠所需的完整设计、约束与风险，供将来在
> **具备 GPU + 可压测渲染线程** 的环境落地时参考。当前在无 GPU、单元测试不覆盖
> 渲染线程路径的环境里实现，其竞态正确性无法验证，故不予落地。

## 1. 背景

技术评审文档把「帧管线 `WaitForComplete()` 移位」列为 P1、估工作量约 10 行：即把
`RenderThreadManager::WaitForComplete()` 从 `FramePipeline::Render()` 开头移到末尾，
以获得「真正的双缓冲流水线」。

经核对当前代码（`engine/runtime/`），该估计不成立：单纯移动同步点会引入数据竞争，
真正安全的实现需要对整套渲染准备状态做双缓冲，并解决 Pass 上下文绑定问题。

## 2. 当前线程模型（渲染线程开启时）

`engine/runtime/frame_pipeline.cpp` `FramePipeline::Render()`（线程路径）：

```
Render(N):
  render_thread_mgr_->WaitForComplete();      // 阻塞至 Execute(N-1) 完成
  dse::core::Memory::Frame().BeginFrame();    // 帧分配器推进+复位（安全 fence 点）
  PrepareRenderFrame();                        // 主线程：写共享渲染准备状态
  render_thread_mgr_->SignalNewFrame();        // 唤醒渲染线程执行 Execute(N)
```

主循环（`engine_app.cpp`）每帧先 `Update()` 后 `Render()`。因此现状**已有**一段重叠：
`Render(N)` 末尾 `SignalNewFrame` 之后，主线程返回主循环执行 `Update(N+1)`，这段与渲染
线程执行 `Execute(N)` 并行；`Render(N+1)` 开头的 `WaitForComplete` 才阻塞。

**未重叠的只剩 `PrepareRenderFrame`**：因为它在 `WaitForComplete` 之后、`SignalNewFrame`
之前串行执行。

## 3. 目标

把 `WaitForComplete` 移到 `PrepareRenderFrame` 之后，使：

```
Render(N):
  PrepareRenderFrame();                        // 与 Execute(N-1) 并行
  render_thread_mgr_->WaitForComplete();       // 确保 Execute(N-1) 完成
  <flip 准备状态索引>
  dse::core::Memory::Frame().BeginFrame();
  render_thread_mgr_->SignalNewFrame();         // Execute(N) 读取刚写好的那份
```

从而让 `Prepare(N)`（纯 CPU）与 `Execute(N-1)`（渲染线程）重叠。

## 4. 竞态来源：Prepare 写、Execute 读的共享状态

`PrepareRenderFrame` / `PrepareGPUSceneAndQueues` / `BuildRenderSceneQueues` 写入、
`ExecuteRenderFrame` / 渲染图 Pass 读取的**同一份**状态：

| 状态 | 写入点（Prepare） | 读取点（Execute） | 说明 |
|---|---|---|---|
| `render_pass_context_`（标量+指针） | 遍布 Prepare | 遍布 Execute + 所有 Pass | 核心共享结构 |
| `rs_->scene_view_` | `ExtractRenderSceneView` | Pass 经上下文指针 | 纯 CPU |
| `rs_->light_buffer_`（CPU 光源列表） | `CollectLightsFromView` | `Upload()`（GPU 上传） | **持有 GPU SSBO** |
| `rs_->cluster_grid_`（CPU 网格） | `Build` | `Upload()`（GPU 上传） | **持有 GPU SSBO** |
| `rs_->render_scene_`（渲染队列） | `BuildRenderQueues` | `ApplyCameraOffset` + 渲染图 | 纯 CPU（含大向量） |

> `render_pass_context_.snapshot`（thin snapshot）已经是双缓冲：
> `snapshot_pool_[2]` + `snapshot_write_idx_` + `FlipSnapshotIndex()`（见
> `frame_pipeline.h`）。本设计沿用同一惯用法扩展到上述五类状态。

只要 `Prepare(N)` 与 `Execute(N-1)` 并行、又访问上述任一同一实例，即产生数据竞争
（未定义行为、画面撕裂、崩溃）。

## 5. 硬阻塞：Pass 以引用绑定单一上下文

`engine/render/passes/builtin_passes.h` 中，**每个 Pass** 都这样持有上下文：

```cpp
explicit ForwardScenePass(RenderPassContext& ctx) : ctx_(ctx) {}
// ...
RenderPassContext& ctx_;
```

而在渲染线程模式下，渲染图与 Pass 只在 `Init()` 里 `BuildRenderGraph()`
**构建一次**（`frame_pipeline.cpp` Init 路径）；`Render()` 线程路径每帧只
`ExecuteRenderGraph`，**不重建图**。（同步模式在 `RunFrameRender` 里每帧重建图，
所以同步模式无此问题。）

因此：30+ 个 Pass 的 `ctx_` 引用**永久绑定到唯一一份** `render_pass_context_`。
如果只是把 `render_pass_context_` 改成 `[2] + flip`，Pass 看不到索引切换，
**双缓冲失效、仍然竞态**——比不改更糟（给出了"已双缓冲"的假象）。

## 6. 可行方案

### 方案 A1（推荐：正确、可编译验证）

- 把五类状态改为双份持有 + 索引/flip：
  - `render_pass_context_pool_[2]`；
  - `RenderState` 中 `scene_view_`、`render_scene_` 各 `[2]`；
  - `light_buffer_[2]`、`cluster_grid_[2]`（**各自持有独立 SSBO，GPU 显存翻倍**，
    这两块通常量级为几十 KB~几 MB，可忽略）。
- Prepare 阶段代码写 `prep_*()`（索引 = `prep_idx_`）；Execute 阶段代码读
  `exec_*()`（索引 = `1 - prep_idx_`）。上下文指针字段指向**同索引**的子系统实例。
- **解除 Pass 绑定**：把每个 Pass 从 `RenderPassContext& ctx_` 改为经**可切换指针**
  间接访问（例如持有 `RenderPassContext* const* ctx_slot_`，或统一改为
  `const RenderPassContext& ctx() const { return *slot_; }`）。触及 30+ Pass 的
  构造签名、成员、以及所有 `ctx_.field` 用法。
- flip 时机：`PrepareRenderFrame()` 之后、`WaitForComplete()` 之后、`SignalNewFrame()`
  之前翻转 `prep_idx_`；同步模式保持单帧行为（写完即读同一索引）。
- 帧分配器 `Memory::Frame()` 已是轮转多缓冲（`kMaxFrameBuffers=4`）；当前无任何
  代码经 `Memory::FrameAlloc` 实际分配（只调用 `BeginFrame`），故其复位顺序目前
  不构成竞态；但若将来 Prepare/Execute 真正使用帧分配器，需把帧延迟从 1 提升到 2、
  相应把 `frame_buffer_count` 配到 3，并确保 `BeginFrame` 复位的缓冲对应帧已完成。

**成本**：动 30+ Pass + 五类状态双缓冲；**收益**：仅重叠一段相对便宜的 CPU Prepare。

### 方案 A2（较小改动，但有代价）

渲染线程模式也每帧重建渲染图（像同步模式那样），使 Pass 每帧用当前索引上下文
重新构建。改动小，但：每帧重建图有性能代价（分配/编译 DAG）；且 Pass 每帧在
线程路径重建本身未必线程安全（部分 Pass 可能持有跨帧 GPU 状态）。不推荐。

### 方案 C（最小、收益也最小）

只双缓冲纯 CPU 上下文、`WaitForComplete` 仍置于 GPU/分配器相关工作之前，不追求
完全重叠。基本等同现状，收益极小。

## 7. 为什么当前不实现

1. **收益小**：现状已有 `Update(N+1) || Execute(N)` 重叠；P1b 仅多争取
   `Prepare(N+1) || Execute(N)`，而 Prepare 是相对便宜的纯 CPU 工作，通常不是帧瓶颈。
2. **风险大且不可验证**：正确实现需动 30+ Pass 或改图重建模型 + 五类状态双缓冲；
   而**竞态正确性只能靠开启渲染线程 + 真实 GPU 多帧压测验证**。本环境无 GPU、
   单元测试不跑渲染线程路径，编译通过 ≠ 无竞态。任一处 prep/exec 归类错误都会产生
   只在真机多帧才暴露的撕裂/崩溃。

## 8. 落地前置条件（将来）

- 具备真实 GPU 的运行环境，可开启 `DSE_RENDER_THREAD=1` 长时间多帧运行。
- 具备撕裂/竞态检测手段（TSan 或等价、逐帧像素回读比对、长跑稳定性）。
- 在此前置条件下，按 **方案 A1** 实施，并补充渲染线程路径的针对性测试。

## 9. 涉及文件（实现时）

- `engine/runtime/frame_pipeline.h` / `frame_pipeline_impl.h`：五类状态改双份 + flip 访问器。
- `engine/runtime/frame_pipeline.cpp`：`Render()` 移位 + flip。
- `engine/runtime/frame_pipeline_thread.cpp`：`PrepareRenderFrame` 写 `prep_*()`、
  `ExecuteRenderFrame` 读 `exec_*()`。
- `engine/runtime/frame_pipeline_render.cpp`：`PrepareGPUSceneAndQueues` /
  `BuildRenderSceneQueues` / `BuildRenderGraphInternal` 归类到对应阶段索引。
- `engine/render/passes/builtin_passes.h` 及各 Pass 实现：解除 `RenderPassContext&`
  绑定，改为可切换指针间接访问（30+ Pass）。
