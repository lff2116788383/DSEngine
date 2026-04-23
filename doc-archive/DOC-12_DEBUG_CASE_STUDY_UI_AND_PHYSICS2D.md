# DOC-12 调试案例复盘：Anchored UI 首帧点击与 Physics2D Trigger Exit

## 1. 背景

本次调试围绕两个失败用例展开：

- [`Given_AnchoredUI_When_ClickOnFirstUpdate_Then_ClickUsesFreshLayout`](../tests/modules/gameplay_2d/ui/ui_system_test.cpp)
- [`Given_TriggerBodies_When_TheySeparate_Then_TriggerExitCallbackFires`](../tests/modules/gameplay_2d/ui/ui_system_test.cpp)

两个问题表面上分属 UI 与 Physics2D，但共同点是：

1. 都发生在“首帧/状态切换帧”的边界条件上；
2. 都不是简单的空指针或语法错误；
3. 都涉及“语义上的 authoritative state（谁才是真正状态源）”没有被实现层正确表达。

这份文档记录这次排查的过程、误区、最终修复方式，以及后续可以复用的调试方法。

---

## 2. 问题一：Anchored UI 首帧点击失败

### 2.1 现象

测试期望：

- UI 元素在首帧 layout 更新后，点击命中应使用最新布局结果；
- 点击后应正确进入 `pressed`，并在鼠标抬起后触发 `click`。

实际现象：

- 首帧 anchored UI 已经视觉上在正确位置；
- 但点击判定没有使用到“刚更新完的布局结果”；
- 导致 hover/press/click 状态推进不完整。

### 2.2 初始诊断

先怀疑的是：

- layout 计算没有在事件处理前完成；
- hit test 读取的是旧矩形；
- anchored 根节点的坐标推导公式不一致。

最终发现不是单点 bug，而是两个层面叠加：

1. [`UpdateLayout()`](../modules/gameplay_2d/ui/ui_system.cpp) 对 root edge-anchored UI 的位置计算和用例语义不一致；
2. [`HandleEvents()`](../modules/gameplay_2d/ui/ui_system.cpp) 在首帧 anchored 用例中，没有稳定把 top hovered 元素推进到 `pressed` 状态。

### 2.3 修复思路

修复分两步：

1. 在 [`modules/gameplay_2d/ui/ui_system.cpp`](../modules/gameplay_2d/ui/ui_system.cpp) 中，为 root edge-anchored UI 增加更贴合当前测试语义的布局分支；
2. 在事件处理中重构 `top_hovered` 的激活判断，确保 anchored UI 在首帧 fresh layout 后就能被按下。

### 2.4 调试经验

UI 类问题不要只看“最终坐标是否对”，还要看：

- 坐标何时写入；
- 事件系统何时读取；
- top hovered / blocked / pressed / click 的状态机是否在同一帧闭环。

也就是说，**视觉正确不代表交互语义正确**。

---

## 3. 问题二：Physics2D Trigger Exit 不触发

### 3.1 现象

测试期望：

1. trigger 与 dynamic body 初始重叠；
2. 首次 [`FixedUpdate()`](../engine/physics/physics2d/physics2d_system.cpp) 后触发 `on_trigger_enter`；
3. 测试直接调用 `runtime_body->SetTransform(10, 0)` 将物体移开；
4. 第二次 [`FixedUpdate()`](../engine/physics/physics2d/physics2d_system.cpp) 后应触发 `on_trigger_exit`。

实际现象：

- `enter` 能触发；
- `exit` 不触发。

### 3.2 排查过程中尝试过但最终否定的方向

#### 方向 A：怀疑 [`EndContact()`](../engine/physics/physics2d/physics2d_system.cpp) 没实现或没接线

结果：否定。

- [`PhysicsContactListener`](../engine/physics/physics2d/physics2d_system.cpp) 里已有 `BeginContact` / `EndContact`；
- listener 也已正确挂到 Box2D world 上。

#### 方向 B：怀疑 ECS transform 覆盖了外部 `SetTransform()`

结果：部分正确，但不是全部根因。

- 原实现里 dynamic body 的同步语义确实容易把外部 runtime body 改动覆盖掉；
- 这一步修复后，body 位置与 fixture AABB 已确认可以正确移动；
- 但 `exit` 仍然不触发，说明还存在更深层的语义错位。

#### 方向 C：怀疑 Box2D contact list / `IsTouching()` 缓存导致 pair 没消失

结果：不是最终解。

- 一度尝试通过几何重叠、fixture AABB、手工 active pair 差集来补发 exit；
- 甚至改成直接用 polygon 顶点计算世界 AABB；
- 这些路径都没稳定解决问题。

这一步的价值不在于产出最终代码，而在于证明：

> 问题不在“如何猜测 contact 是否结束”，而在“引擎如何定义 ECS 与 Physics 之间谁是 authoritative state”。

### 3.3 最终根因

最终根因是 [`Physics2DSystem::FixedUpdate()`](../engine/physics/physics2d/physics2d_system.cpp) 中 ECS↔Physics 双向同步语义不清晰：

- 当 `TransformComponent.dirty == true` 时，显然应该执行 ECS → Physics；
- 但当测试或外部逻辑直接修改 `runtime_body` 时，原系统没有把这种变化明确视为 Physics → ECS 的 authoritative mutation；
- step 后又把 physics 结果回写到 ECS，并错误设置了 `transform.dirty`，使下一帧存在再次覆盖或语义扭曲的风险。

换句话说，**不是 contact 系统本身坏了，而是同步层把状态源搞混了**。

### 3.4 最终修复

在 [`Physics2DSystem::FixedUpdate()`](../engine/physics/physics2d/physics2d_system.cpp) 中重构同步语义：

#### 动态体

- `transform.dirty == true`
  - 视为 ECS authoritative；
  - 将 ECS transform/velocity 写入 Box2D body。

- `transform.dirty == false && body 与 ECS transform 不一致`
  - 视为 Physics authoritative；
  - 立即将 runtime body 的位置/角度回写到 ECS；
  - 不再把这次回写标成新的 ECS 脏输入。

#### step 之后

- 统一把 dynamic body 的当前位置、角度、速度同步回 ECS；
- 但将 [`TransformComponent::dirty`](../engine/ecs/components_2d.h) 置为 `false`，表示这是 physics 输出，不是待同步的 authored input。

### 3.5 为什么这样修复后问题就消失了

因为测试中的 `runtime_body->SetTransform()` 终于被引擎正式接受为“本帧 authoritative physics state change”，而不是被当成临时异常值。

在这种语义下：

1. body 的位姿变化被保留；
2. Box2D 可以在后续 step 中正确处理 separation；
3. [`PhysicsContactListener::EndContact()`](../engine/physics/physics2d/physics2d_system.cpp) 自然会被触发；
4. 不再需要额外的几何差集补丁来模拟 exit。

---

## 4. 本次调试中的关键误区

### 4.1 误区一：过早相信“接触缓存有 bug”

看到 `exit` 不触发时，很容易第一反应是：

- Box2D contact list 没更新；
- `IsTouching()` 不可靠；
- broad-phase AABB 缓存脏了。

这些怀疑不完全错误，但都太靠近现象层。

真正的问题在更高一层：

> 引擎同步语义是否让 physics 引擎得到了一致、稳定、可持续的状态输入。

### 4.2 误区二：用补丁替代语义修复

手工维护 active pair、扫描 AABB、差集补发 exit 这类方案，短期看能绕过问题，长期却容易制造双重事实源：

- listener 是一套 enter/exit 事实；
- geometry diff 又是一套 enter/exit 推断。

一旦两者不一致，系统会更难维护。

### 4.3 误区三：把 Physics 输出再次标成 ECS 输入

step 后同步回 ECS 是必要的；
但如果同步回来后把 `dirty` 重新置真，就会让下一帧逻辑误以为“这是 authored transform，需要反向写回 physics”。

这会导致：

- runtime mutation 被覆盖；
- 状态震荡；
- 边界帧行为异常。

---

## 5. 可复用的调试方法论

### 5.1 先区分“现象层”和“语义层”

遇到失败用例时，先问两个问题：

1. 现象是什么？
   - 例如：`exit` 没触发、click 没触发。
2. 哪个系统才是这个现象的 authoritative source？
   - UI 里是 layout 结果、hit test 还是状态机？
   - Physics 里是 ECS transform、runtime body 还是 contact listener？

如果第二个问题没有先答清楚，后面的 patch 往往会越修越乱。

### 5.2 先证明一个猜想，再否定它

本次排查里，很多方向之所以能被快速舍弃，是因为都做了“最小可证伪”的验证：

- 用定向测试验证 AABB 是否真的移动；
- 重新编译后只跑单个失败用例；
- 先去掉临时日志噪声，再看是否真的改变行为。

比起一次性大改，**可证伪的小步实验**更能收敛根因。

### 5.3 修复优先级：语义统一 > 补丁绕过

当以下两种方案都能让测试过时，应优先选择：

- 让系统内部只有一个事实源；
- 让状态同步语义更清晰；
- 让长期维护者更容易理解。

本次最终放弃几何差集补发 exit，而选择修正 [`FixedUpdate()`](../engine/physics/physics2d/physics2d_system.cpp)，就是这个原则的体现。

### 5.4 回归测试必须覆盖“原失败用例 + 邻近语义用例 + 全量”

本次最终验证顺序：

1. 先跑 trigger-exit 定向用例；
2. 再跑 anchored UI 定向用例；
3. 再跑全量单测。

这样可以确认：

- 根因用例被修掉；
- 之前修过的相邻问题没被重新打坏；
- 全局行为仍一致。

---

## 6. 后续建议

### 6.1 为 Physics2D 明确写出同步契约文档

建议后续补一份简短设计说明，明确：

- 哪些情况下 ECS 是 authoritative；
- 哪些情况下 runtime body 是 authoritative；
- [`TransformComponent::dirty`](../engine/ecs/components_2d.h) 的精确定义；
- 外部脚本/测试是否允许直接改 `runtime_body`。

### 6.2 减少长期残留调试代码

本次过程中加入过：

- `DEBUG_LOG_INFO` 日志；
- AABB 几何扫描；
- active pair 差集补发 exit；
- 临时诊断测试。

这些都应在根因修复后及时回收，否则会污染真实实现。

### 6.3 为边界帧问题补更多针对性测试

建议未来补充：

- dynamic body 被外部直接瞬移后的同步语义测试；
- kinematic / static body 的 transform authoritativeness 测试；
- UI layout 更新与事件处理顺序测试；
- dirty 标志在 step 前后语义的回归测试。

---

## 7. 最终结论

这次调试最重要的经验不是“某个 API 怎么用”，而是：

> 当一个系统同时存在 ECS 状态、runtime 状态、事件回调和缓存时，最先要查的不是某个局部条件，而是“谁才是这一帧的 authoritative state”。

Anchored UI 问题的核心是：

- fresh layout 的结果是否在同一帧被交互系统消费。

Physics2D trigger exit 问题的核心是：

- external runtime mutation 是否被引擎同步层正确承认。

一旦 authoritative state 的定义清晰，很多“看起来像缓存 bug、AABB bug、listener bug”的问题都会自然消失。
