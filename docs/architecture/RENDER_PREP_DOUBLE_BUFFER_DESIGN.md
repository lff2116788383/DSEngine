# 渲染帧准备流水线设计与实施门槛（P1b）

> 状态：**设计已复核，暂不实施**。
>
> 旧方案正确识别了“不能只移动 `WaitForComplete()`”这一点，但把整个
> `RenderPassContext`、`LightBuffer`、`ClusterGrid` 等对象直接复制两份仍不完整：
> 当前渲染线程会访问 `World/ECS`，主线程准备阶段也会修改 RHI 全局状态和 Pass
> 可变状态。仅给已列出的五类对象加双缓冲，仍会保留数据竞争。
>
> 推荐方向改为：先建立**完整、不可变的 `RenderFramePacket`**，明确主线程和渲染线程
> 的所有权边界；确认真实 GPU 上存在 CPU 渲染线程瓶颈后，再为帧包引入两槽或可配置
> N 槽流水线。GPU frames-in-flight、上传资源和交换链缓冲必须独立设计。

## 1. 结论

1. 单纯把 `RenderThreadManager::WaitForComplete()` 从 `FramePipeline::Render()` 开头
   移到 `PrepareRenderFrame()` 之后是不安全的。
2. DSE 当前已有 `Update(N+1) || Execute(N)` 重叠，但该重叠尚未建立完整的数据隔离：
   渲染 Pass 和模块回调仍可能读取 `World/ECS`。
3. 在修复现有跨线程所有权之前，不应继续扩大 `Prepare(N+1) || Execute(N)` 的重叠范围。
4. 商业引擎普遍使用游戏线程/渲染线程流水线，但不等于“所有状态固定复制两份”。
   常见实现是不可变渲染代理、命令包或帧包，加两槽、三槽或 N 槽环形队列。
5. DSE 是否需要该优化必须由真实场景的分段性能数据决定；目前没有证据证明
   `PrepareRenderFrame()` 或 `WaitForComplete()` 是主要帧瓶颈。

## 2. 必须区分的三类缓冲

“双缓冲”在渲染系统中可能指完全不同的层次，不能混为一谈。

| 层次 | 解决的问题 | 生命周期与同步 |
|---|---|---|
| CPU 帧包缓冲 | 主线程准备 N 帧时，渲染线程消费 N-1 帧 | CPU 线程同步；本设计的主要对象 |
| GPU frames-in-flight | CPU 提交新命令时，GPU 仍在执行旧帧 | GPU fence/semaphore；通常 2~3 或可配置 |
| 交换链图像 | 扫描输出与 GPU 渲染目标解耦 | Present 模式与平台窗口系统 |

CPU 帧包使用两槽，不代表 GPU 上传资源必须正好两份；交换链有三张图像，也不代表
游戏线程必须允许领先三帧。三者应分别根据线程延迟、GPU fence 和显示延迟配置。

## 3. 当前线程模型

渲染线程开启时，当前 `FramePipeline::Render()` 为：

```text
Render(N):
  WaitForComplete()       // 等待 Execute(N-1) 的 CPU 渲染线程工作结束
  Memory::Frame().BeginFrame()
  PrepareRenderFrame()    // 主线程准备 N
  SignalNewFrame()        // 渲染线程执行 N
```

主循环每帧先 `Update()` 后 `Render()`，因此 `SignalNewFrame(N)` 之后：

```text
主线程:     Update(N+1) -------------------- Wait ---- Prepare(N+1)
渲染线程:          Execute(N) ------------------------->
```

吞吐时间可近似表示为：

```text
T_current = max(T_update, T_execute) + T_prepare
```

如果安全地把 Prepare 也并行化：

```text
T_pipelined = max(T_update + T_prepare, T_execute)
```

所以单帧理论收益为：

```text
benefit = T_current - T_pipelined
```

收益不会超过 `T_prepare`。当 `Execute` 在 `Update` 结束前已经完成时，收益接近零；
只有 CPU 渲染线程持续覆盖到 Prepare 阶段时，才能获得明显收益。GPU 本身是瓶颈时，
该优化通常只减少 CPU 等待，不会提高最终帧率。

## 4. 当前实现中更优先的正确性问题

### 4.1 Execute 仍访问 World/ECS

`frame_pipeline.h` 对 `ExecuteRenderFrame()` 的目标约束是“仅消费快照与已提取数据，
不触碰 World”，但当前实现尚未完全达到：

- `RenderPassContext` 仍保存 `World*`；
- 2D Scene、2D UI 和 Mesh 回调在 Pass 执行时接收 `World&`；
- gameplay 3D Pass 仍通过上下文访问 `ctx.world`；
- 主线程 `Update(N+1)` 可能与上述读取同时修改 registry/component。

因此，即使不移动等待点，现有 `Update(N+1) || Execute(N)` 也可能出现 ECS 数据竞争。
这是实施 P1b 前必须解决的 P0 前置条件。

### 4.2 Prepare 会修改渲染侧可变对象

`PrepareRenderFrame()` 并非严格的纯 CPU 帧包构建：

- 调用 `RhiDevice::SetGlobalFoliageWind/SetGlobalFoliagePush`，写入渲染线程读取的
  `global_render_state_`；
- 主线程调用 `TAAPass::UpdateJitter()`，而渲染线程执行同一 `TAAPass` 并读取
  `current_jitter_`、`frame_index_`；
- `GPUSkinningSystem::BeginFrame/Submit/GetSkinnedOutput` 混合了准备数据和跨帧状态；
- `modules_impl_` 的 GPU Scene、AABB、Hi-Z 可见性缓存同时跨越 Prepare 和 Execute。

这些状态没有包含在旧设计列出的五类对象中。只复制 `render_pass_context_`、
`scene_view_`、`light_buffer_`、`cluster_grid_` 和 `render_scene_` 仍然不安全。

### 4.3 CPU 渲染线程完成不等于 GPU 完成

`WaitForComplete()` 只等待 `ExecuteRenderFrame()` 回调返回。它不是 GPU fence，不能证明
GPU 已停止读取上一帧的 uniform、SSBO、descriptor 或上传内存。因此：

- CPU 帧包槽位可由 CPU 线程完成信号复用；
- GPU 可见资源必须由后端 fence/frames-in-flight 规则复用；
- `Memory::Frame()` 若未来存放 GPU 延迟读取的数据，必须接入 GPU 完成信号，而不能只
  根据 `WaitForComplete()` 推导缓冲数。

## 5. 推荐架构：不可变 RenderFramePacket

### 5.1 数据分层

将当前混合了帧数据、服务指针、GPU 资源和跨帧 Pass 状态的 `RenderPassContext` 拆分为：

```cpp
struct RenderFramePacket {
    uint64_t frame_index;
    RenderThinSnapshot snapshot;
    RenderSceneView scene_view;
    RenderScene render_scene;
    LightFrameData lights;
    ClusterFrameData clusters;
    RenderFrameGlobals globals;
    RenderFeatureState features;
    RenderQueueState queues;
};

struct RenderPassServices {
    RhiDevice& rhi;
    AssetManager& assets;
    MeshRenderer& mesh_renderer;
    RenderPipelineResources& resources;
};
```

约束：

- `RenderFramePacket` 由主线程独占写入，发布后只读；
- 渲染线程不得通过帧包或回调访问 `World/ECS`；
- `RenderPassServices` 和 GPU 资源只由渲染线程使用，或具有明确后端同步；
- TAA history、自动曝光 history、GPU readback 等跨帧状态由对应 Pass 在渲染线程独占；
- 风、湿度、jitter、DDGI 配置等每帧值进入帧包，不由主线程直接写 RHI/Pass。

### 5.2 Pass 接口

不推荐让 30+ 个 Pass 保存 `RenderPassContext* const*` 并依赖外部切换槽位。该方案会隐藏
当前帧依赖，容易缓存失效指针，也难以在测试中验证。

推荐让帧依赖在执行接口上显式出现：

```cpp
class IRenderPass {
public:
    virtual void Execute(
        CommandBuffer& command_buffer,
        const RenderFramePacket& frame,
        RenderPassServices& services) = 0;
};
```

渲染图可以继续只构建一次；每帧执行时传入当前不可变帧包。Pass 自身只保存静态配置和
由渲染线程独占的跨帧资源，不保存可由主线程切换的上下文引用。

### 5.3 CPU 帧包槽位

在当前 `RenderThreadManager` 最多只有一个待执行帧的前提下，两槽足以表达：

```text
slot[write]: 主线程 Prepare(N)
slot[read]:  渲染线程 Execute(N-1)
```

建议发布流程：

```text
Render(N):
  write_slot = AcquireWritablePacketSlot()
  PrepareRenderFrame(write_slot)       // 不访问 RHI/Pass，不修改 read_slot
  WaitForComplete()                    // 确保旧 read_slot 不再被 CPU 渲染线程读取
  PublishPacket(write_slot)            // mutex/condition-variable 建立 happens-before
  SignalNewFrame()
```

不要把 `1 - prep_idx` 分散在业务代码中；由小型 `RenderFramePacketQueue` 封装
`Acquire/Publish/Consume/Release`，集中维护所有权和断言。以后若允许多个排队帧，
可以扩展为 N 槽而不改 Pass。

### 5.4 GPU 资源

`LightFrameData` 和 `ClusterFrameData` 只保存 CPU 数组；`LightBuffer`、`ClusterGrid`
中创建、扩容、更新和销毁 SSBO 的部分应成为渲染线程侧 GPU uploader/resource。

是否复制 GPU buffer 由后端决定：

- 支持 discard/renaming 或 ring upload 的后端可以复用逻辑资源；
- 显式 API 可按 GPU frame index 使用 per-frame buffer/descriptor；
- 复用前必须等待对应 GPU fence；
- 不应因为 CPU 帧包是双缓冲，就无条件把所有 SSBO 和持久资源复制两份。

## 6. 商业引擎的常见做法

商业引擎通常都会解耦游戏状态与渲染消费，但实现并非统一的“整套状态双缓冲”：

- Unreal 使用 Game Thread、Render Thread、可选 RHI Thread，以及 game object 对应的
  render proxy/scene proxy；大量绘制准备再分发到 worker task。
- Unity 主线程生成中间图形命令，由 Render Thread 或 Graphics Jobs 转成底层 API
  命令；驱动排队帧数还可独立限制。
- 显式图形 API 常见 2 frames-in-flight，以平衡 CPU/GPU 并行与输入延迟；高吞吐场景
  也可能使用 3 或更多，但需要接受更高延迟。
- 部分引擎允许关闭独立渲染线程，说明多线程渲染是可选的性能策略，不是正确渲染的
  必要条件。

共同原则是**明确所有权、不可变发布、有限领先和 fence 复用**，而不是缓冲数量必须为 2。

## 7. 收益、代价与适用条件

### 7.1 可能收益

- 隐藏一部分或全部 `PrepareRenderFrame()` CPU 时间；
- 降低主线程在 `WaitForComplete()` 的阻塞时间；
- 提高主线程与渲染线程的核心利用率；
- CPU Render Thread bound 的大场景可能提高吞吐并改善帧时间波动；
- 完整帧包边界还能提升确定性、回放、抓帧和离线渲染测试能力。

最后一点通常比单纯的帧率收益更有长期架构价值：帧包可以序列化、比较、重放，并让
渲染测试摆脱实时 ECS。

### 7.2 代价

- 需要消除所有 Render Execute 对 `World/ECS` 的访问；
- 需要重构 Pass 接口、模块渲染接口和 RHI 全局状态输入；
- 大型 `RenderScene`、动态顶点、骨骼矩阵等数据会增加 CPU 内存峰值；
- 跨帧资源、热重载、窗口 resize、设备丢失和 shutdown 需要明确 drain 流程；
- 错误的槽位复用会形成低概率竞态，只在真机长跑或高负载下暴露；
- 流水线可能增加一帧可见延迟；必须限制 CPU 领先数量。

### 7.3 值得实施的判定条件

至少满足以下条件再进入实现：

1. 真实目标平台采集 `Update`、`Prepare`、`WaitForComplete`、`Execute`、GPU frame
   五段时间和 P50/P95/P99；
2. `WaitForComplete` 在目标场景持续占用可观 CPU 帧预算；
3. `Execute` 经常长于 `Update`，并覆盖到 Prepare 可执行的时间窗口；
4. 项目主要瓶颈是 CPU 提交/渲染准备，而不是 GPU；
5. 有真实 GPU 自动化环境，可运行渲染线程长稳、像素回读和 resize/shutdown 压测；
6. 可使用 TSan 或等价的线程竞态检测手段覆盖 CPU 数据路径。

建议经验门槛：如果 `WaitForComplete` 的 P95 小于目标帧预算的 5%，或
`PrepareRenderFrame` 的 P95 小于约 0.5 ms，优先优化其他瓶颈。最终阈值应按目标平台、
帧率和输入延迟要求调整，不应作为固定标准。

## 8. 分阶段实施计划

### Phase 0：测量与现状正确性

- 为五段时间增加 profiler scope；
- 增加渲染线程启用状态、队列深度和等待原因统计；
- 列出所有 Execute 路径中的 `World/ECS`、主线程 RHI、共享 Pass 状态访问；
- 先修复现有 `Update(N+1) || Execute(N)` 的竞态。

### Phase 1：完整帧包，暂不移动等待点

- 引入单实例 `RenderFramePacket`；
- 所有 Pass 和模块仅消费帧包，不访问 World；
- RHI 每帧全局值由 Execute 从帧包应用；
- 保持当前同步点，先验证画面与功能等价。

### Phase 2：双槽 CPU 帧包

- 引入 `RenderFramePacketQueue` 两槽所有权；
- 把 `WaitForComplete()` 移到 Prepare 后；
- 增加 slot generation、frame index 和只读期断言；
- 验证同步和渲染线程两种模式。

### Phase 3：GPU per-frame resources

- 各后端基于 GPU fence 管理上传 ring、descriptor 和临时 buffer；
- 缓冲数由 frames-in-flight 配置决定；
- 处理 resize、设备重建和 shutdown drain。

### Phase 4：按数据决定是否保留

- 对比改造前后 P50/P95/P99、输入延迟、CPU/GPU 利用率和内存峰值；
- 若收益低于复杂度成本，保留不可变帧包边界，但可以关闭 Prepare/Execute 重叠。

## 9. 测试要求

- 单元测试：帧包构建不保留 ECS 指针，发布后内容不可修改；
- 线程测试：随机延迟 Prepare/Execute，验证槽位不会同时读写；
- 确定性测试：相同 World 输入生成相同帧包摘要；
- 回放测试：保存帧包后脱离 World 重放并比较命令/像素结果；
- 真机长稳：`DSE_RENDER_THREAD=1` 多后端长时间运行；
- 生命周期测试：窗口 resize、最小化/恢复、场景切换、热重载和 shutdown；
- GPU fence 测试：CPU 帧包释放不能被误认为 GPU 资源可立即复用。

## 10. 当前建议

当前不直接实施旧方案 A1，也不只移动同步点。优先完成 Phase 0 和 Phase 1：

1. 修复渲染线程对 `World/ECS` 和主线程可变 RHI/Pass 状态的访问；
2. 建立不可变 `RenderFramePacket`，即使暂时仍串行 Prepare，也能先获得正确的数据边界；
3. 在真实 GPU 上采集性能数据；
4. 只有数据证明 CPU 渲染线程等待是重要瓶颈时，再实施双槽流水线。

因此，**不可变帧包边界有必要，Prepare/Execute 双缓冲是否有必要目前尚未被证明**。
