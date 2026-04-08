# DOC-13 调试案例复盘：Spine 独立测试目标在多用例组合下挂起

## 1. 背景

本次调试聚焦于 [`engine.spine`](../tests/spine/CMakeLists.txt) 对应的独立测试目标 [`dse_spine_tests`](../tests/spine/CMakeLists.txt) 在 Windows 上出现的“多用例组合执行后挂起不退出”问题。

现象并不是普通断言失败，而是：

- 单个 Spine 用例通常可以通过；
- 某些两两或三三组合会在最后一个用例看似执行完后仍不退出；
- CTest 侧表现为超时；
- 如果上一次挂起进程未被结束，后续重编还会触发 Windows 链接错误 `LNK1168`。

这类问题很容易误判为：

- Catch2 runner 卡死；
- reporter 没有结束；
- `World` 或 `entt::registry` 析构阻塞；
- Spine runtime 资源释放顺序错误；
- 某个静态对象跨用例污染。

本次文档记录的是：**如何从黑盒现象逐步收敛到“第 3 个测试函数返回给 Catch 之前发生阻塞”这个结论，以及哪些怀疑已经被排除。**

---

## 2. 复现现象

独立目标定义位于 [`tests/spine/CMakeLists.txt`](../tests/spine/CMakeLists.txt)。该目标直接复用 [`main()`](../tests/engine/main.cpp:48)，并通过：

- `engine.spine`：执行标签 `[engine][unit][spine]`
- `engine.spine.smoke`：执行标签 `[smoke][snapshot][spine]`

最初观察到的关键现象是：

1. 完整 `[engine][unit][spine]` 会挂起；
2. 某些单测单独跑是正常的；
3. 问题与“单个断言失败”无关，而是进程不退出；
4. 挂起后需要手动结束进程，否则再次构建会遇到 `LNK1168`。

这意味着问题更像是：

- 生命周期边界阻塞；
- 全局状态或静态状态污染；
- 运行时内存/堆破坏在测试返回边界显现；
- 测试框架调用栈中的返回路径未走完。

---

## 3. 最终收敛出的最小复现组合

调试过程中最重要的收敛不是“哪个 API 报错”，而是**哪组用例组合会稳定挂**。

最终得到的稳定结论是：

1. 前 2 个用例组合正常；
2. 第 3 个用例单独正常；
3. **第 2 + 第 3 个用例组合挂起**；
4. 前 3 个用例组合也挂起；
5. 这说明问题不在某个用例单独逻辑，而在**跨用例边界污染**。

独立测试文件为 [`tests/spine/spine_system_test.cpp`](../tests/spine/spine_system_test.cpp)，其中用于缩圈的 3 个关键用例是：

- [`Given_EmptySpinePaths_When_Update_Then_ComponentStateRemainsUnchanged`](../tests/spine/spine_system_test.cpp:19)
- [`Given_MissingSpineAssets_When_Update_Then_RuntimeObjectsRemainNull`](../tests/spine/spine_system_test.cpp:48)
- [`Given_IncompleteSpinePaths_When_Update_Then_RuntimeObjectsRemainNull`](../tests/spine/spine_system_test.cpp:83)

这里的关键点不是测试名字本身，而是它们覆盖了三种边界：

- 无资源路径；
- 同时配置缺失的 atlas + skeleton 路径；
- 只配置 atlas 或只配置 skeleton 的半初始化路径。

---

## 4. 诊断手段一：先证明不是 `main()` 末尾卡住

为了判断挂起是否发生在测试执行之外，在 [`tests/engine/main.cpp`](../tests/engine/main.cpp) 的 [`main()`](../tests/engine/main.cpp:48) 里加入 runner 级打印，确认：

- 能打印 `before session.run`；
- 挂起时不能打印 `after session.run`。

这一步的价值非常大，因为它直接排除了：

- `PauseOnFailureIfNeeded()` 之类交互暂停逻辑；
- `main()` 后置收尾逻辑；
- 进程退出阶段的普通尾清理。

结论变成：

> 问题发生在 [`Catch::Session::run()`](../tests/engine/main.cpp:54) 内部，而不是测试入口函数外层。

---

## 5. 诊断手段二：给 Catch2 内部路径打点，定位返回边界

为了继续收敛，在 [`depends/catch/catch.hpp`](../depends/catch/catch.hpp) 里对以下路径加了诊断打印：

- `RunContext::~RunContext()`
- `RunContext::runTest()`
- `RunContext::runCurrentTest()`
- `RunContext::invokeActiveTestCase()`
- `TestGroup::execute()`

最终收敛出的最关键结论是：

- 挂起时看不到 `invokeActiveTestCase()` 中 `m_activeTestCase->invoke()` 之后的打印；
- 也看不到 `runCurrentTest()` 后半段打印；
- 说明阻塞点发生在：
  - **测试体已经几乎执行完**，
  - 但**尚未真正从 Catch 的 active test case 调用栈返回**。

这一步非常重要，因为它把问题从“Catch 尾清理阶段卡住”修正为：

> **卡点更靠前，发生在测试函数返回给 Catch 之前。**

这和很多人的第一直觉不同。直觉通常会先怀疑 reporter、summary、`testRunEnded` 或 session teardown，但这次不是。

---

## 6. 诊断手段三：对第 2、第 3 个用例做析构边界级打点

在 [`tests/spine/spine_system_test.cpp`](../tests/spine/spine_system_test.cpp) 中加入 [`PrintSpineDiag()`](../tests/spine/spine_system_test.cpp:11)，然后围绕以下边界布点：

- `World` 构造前后；
- `AssetManager` 构造与配置后；
- `SpineSystem` 构造前后；
- [`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:98) 调用后；
- `SpineSystem` 作用域结束前后；
- [`World::Clear()`](../engine/ecs/world.cpp) 前后；
- 测试函数最后一行返回前。

最终得到的高价值结论：

1. 第 2 个用例能够完整跑到“测试返回前”；
2. 第 3 个用例在挂起场景中，也能打印到 `before incomplete-paths test return`；
3. 第 3 个用例的 `SpineSystem` 局部作用域可以退出；
4. 显式调用 [`World::Clear()`](../engine/ecs/world.h) 也能完成；
5. 但进程仍然卡住。

这意味着：

- 不是简单的 `SpineSystem` 局部析构直接卡住；
- 不是普通的 `registry.clear()` 路径直接卡住；
- 不是测试体主体逻辑还没跑完；
- 更像是测试返回边界附近已经发生了更底层的状态破坏或阻塞。

---

## 7. 已被排除的怀疑方向

### 7.1 不是 `World` 的常规清理路径

[`World`](../engine/ecs/world.h) 的实现很薄，核心清理逻辑本质上就是 [`Clear()`](../engine/ecs/world.h) 对 registry 的清空。

而调试中已经证明：

- 第 3 个用例里显式 [`world.Clear()`](../tests/spine/spine_system_test.cpp:124) 能执行完成；
- 之后仍然挂起。

因此可以排除“常规 ECS 容器清理路径本身阻塞”这一层表象根因。

### 7.2 不是 `SpineSystem` 栈对象单纯析构卡死

在第 3 个用例里把 [`SpineSystem`](../modules/gameplay_2d/spine/spine_system.h) 放到局部作用域中，并打印“作用域结束前/后”。

结论是：

- `after incomplete-paths system scope end` 能打印出来；
- 说明该栈对象析构至少能走完可见边界。

所以可以排除“单纯卡在 `SpineSystem::~SpineSystem()`”这一过于直接的猜测。

### 7.3 不是静态 `EngineTextureLoader` 单点污染

[`modules/gameplay_2d/spine/spine_system.cpp`](../modules/gameplay_2d/spine/spine_system.cpp) 中原本 loader 设计很容易让人怀疑模块级静态状态跨用例残留。

因此做过一次实际修复尝试：

- 将纹理 loader 改为 [`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:98) 内的栈对象；
- 每个组件加载时重新绑定 `current_textures` 与 `asset_manager`。

结果是：

- `2 + 3` 组合仍然挂起。

所以这个方向即使不是完全无风险，也**不是当前挂起的主根因**。

### 7.4 不是简单的 Catch2 reporter 尾端卡住

由于 [`depends/catch/catch.hpp`](../depends/catch/catch.hpp) 的诊断已经表明返回点还没走出 `m_activeTestCase->invoke()`，因此可排除：

- `testCaseEnded`
- `testRunEnded`
- summary/reporter 尾清理
- `RunContext` 析构末尾

这些“更后置”的路径作为当前主卡点。

---

## 8. 仍然成立的高概率根因画像

虽然本轮没有完成最终修复，但已经把根因画像收敛得比较具体。

当前最可信的描述是：

> 第 2 个与第 3 个用例之间存在某种跨用例状态污染或堆破坏，导致第 3 个测试函数逻辑上已执行到尾部，但在真正返回给 Catch 调用栈之前进入异常阻塞状态。

它更像以下类型问题之一：

- 内存/堆破坏在返回边界显现；
- 某个 runtime 对象生命周期被错误交叉持有；
- 测试之间共享的全局/静态状态并未完全重置；
- 日志、分配器、Spine runtime 内部全局对象或其他外部依赖在特定顺序下发生未定义行为。

注意：这不是结论说“肯定是堆损坏”，而是说**已经排除掉很多更表层的候选项后，剩余根因更接近未定义行为层面**。

---

## 9. 本次调试最有价值的方法论

### 9.1 先找“最小挂起组合”，不要一上来盯源码

如果没有先定位到 `2 + 3` 组合挂起，而只是盲查 Spine 源码或 Catch2 源码，范围会过大。

这次真正让问题收敛的第一步不是改代码，而是：

- 切分标签；
- 缩减用例组合；
- 识别“单独正常、组合异常”的跨边界特征。

### 9.2 runner 级打点优先级很高

在 [`main()`](../tests/engine/main.cpp:48) 打印 `session.run()` 前后，是非常低成本但极高价值的诊断。

它能迅速回答：

- 卡在测试体内部？
- 卡在框架内部？
- 还是卡在进程退出逻辑？

### 9.3 不要只看“某个对象析构了没有”，要看“有没有真的返回给上一层”

本次一个典型误区是：

- 看到 `SpineSystem` 作用域结束，就以为析构链已经安全；
- 看到 `world.Clear()` 完成，就以为容器层没问题。

但实际上，**局部对象可见析构完成**，并不等于**测试函数已经安全返回到框架**。

### 9.4 对怀疑点要做“可证伪”的最小改动

比如静态 loader 假设：

- 如果只是口头怀疑，没有证据；
- 真正把静态对象改成栈对象后仍然挂，就能把这个方向有效降权。

比起一次性大改，这种“最小替换 + 复测”的方式更能形成可靠结论。

### 9.5 Windows 下挂起问题要同时管理测试与构建症状

本次调试里，挂起不仅影响测试，还会影响下一次链接：

- 测试进程未退出；
- 可执行文件句柄仍被占用；
- 随后触发 `LNK1168`。

因此 Windows 下调试这类问题时，必须把“结束挂起进程”视为流程的一部分，否则会把“链接失败”误判成新的代码问题。

---

## 10. 新一轮实测补充：从“挂起”推进到“可观测堆断言”

在本轮继续对照代码并做最小修复后，问题画像又向前推进了一步。

### 10.1 第一轮修复尝试：把生命周期问题从“诊断跳过”改成“真实清理”

针对 [`SpineRendererComponent`](../engine/ecs/components_2d.h:95) 持有多组 Spine runtime 裸指针这一设计，本轮首先对 [`modules/gameplay_2d/spine/spine_system.cpp`](../modules/gameplay_2d/spine/spine_system.cpp) 做了两类收口：

1. 在 [`CleanupComponent()`](../modules/gameplay_2d/spine/spine_system.cpp:62) 中恢复真实释放，不再保留原先“为了隔离问题而跳过删除”的诊断逻辑；
2. 在 [`Update()`](../modules/gameplay_2d/spine/spine_system.cpp:115) 中加入最小验证日志，并把 atlas / skeleton data / skeleton / animation state data / animation state 的创建改成显式失败回滚路径。

本轮加入的关键辅助点包括：

- [`ResetRuntimePointers()`](../modules/gameplay_2d/spine/spine_system.cpp:27)：统一把运行时裸指针回到空状态；
- [`CleanupComponent()`](../modules/gameplay_2d/spine/spine_system.cpp:62)：集中清理组件内的 Spine runtime 对象；
- [`Update()`](../modules/gameplay_2d/spine/spine_system.cpp:136) 内的 `try/catch`：把创建失败时的资源释放路径从“散落分支”收成“可观测回滚”。

这一步的价值不只是“尝试修复”，更重要的是：

> 它把原本只能以“挂起不退出”形式观察到的问题，推进成了“可以通过堆断言直接指向释放链”的问题。

### 10.2 关键新现象：挂起被推进为 CRT Debug Heap 断言

在重新运行 [`bin/dse_spine_tests.exe`](../bin/dse_spine_tests.exe) 的 `[engine][unit][spine]` 组合时，实际观察到的新现象不是简单超时，而是出现了 **Microsoft Visual C++ Runtime Library** 弹窗。

弹窗关键信息为：

- 程序：[`bin/dse_spine_tests.exe`](../bin/dse_spine_tests.exe)
- 文件：`minkernel\crts\ucrt\src\appcrt\heap\debug_heap.cpp`
- 行号：`996`
- 断言：`__acrt_first_block == header`

这类断言通常说明：

- 堆块头部已损坏；
- 或者某个指针被错误释放；
- 或者对象释放顺序不满足其内部依赖关系。

这和最初文档中“更像未定义行为/堆破坏”这一画像形成了更强的正向验证。

### 10.3 一个非常重要的推进：现在已经能证明问题不只是“卡住”，而是“释放链错误”

此前文档只能把问题描述为：

- 用例逻辑基本跑完；
- 返回给 Catch 之前阻塞；
- 像堆损坏，但还没有直接证据。

而本轮补充后，可以更具体地说：

> 问题已经不再只是“进程挂起”，而是在某次组件或测试边界释放过程中触发了 CRT heap assertion，这说明根因确实位于 Spine runtime 对象所有权/释放协议，而不是单纯的测试框架路径。

这是一条非常关键的调试经验：

- **“挂起”不一定意味着线程阻塞；**
- 在 Windows Debug CRT 下，很多“看起来像卡死”的现象，本质上可能是**断言弹窗阻塞了进程消息循环**；
- 如果只看终端不看桌面弹窗，很容易误把“heap assertion 等待人工确认”当成“纯粹的无响应挂起”。

### 10.4 释放顺序本身就是候选根因，而不是细节问题

本轮继续对照 Spine runtime 源码后，一个新的高概率点被明确提了出来：

- [`AnimationStateData`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationStateData.h:84) 内部持有 [`SkeletonData*`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationStateData.h:84)；
- [`AnimationState::~AnimationState()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:344) 析构时会继续处理内部 track entry 链；
- [`Atlas::~Atlas()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/Atlas.cpp:69) 也会回调 loader 执行 page 卸载路径。

因此，本轮明确增加了一个新的结论：

> 对 Spine runtime 对象来说，“有没有 delete” 还不够，**delete 的顺序本身就是 bug 面的一部分**。

也就是说，下面这种思路是不够的：

- “把所有 `new` 出来的对象最后都删掉就行”；

真正必须进一步验证的是：

- [`AnimationState`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:344) 是否必须先于 [`AnimationStateData`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationStateData.h:45)；
- [`Skeleton`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Skeleton.h:123) 是否必须先于 [`SkeletonData`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/SkeletonData.h:66)；
- [`Atlas`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Atlas.h:117) 是否必须最后释放，避免 page / region 在其他对象仍存活时被间接访问。

### 10.5 这轮修复的直接收获：把问题从“猜测哪边坏了”推进到“具体哪段释放链可疑”

结合源码与现象，本轮新增的最高价值收获是：

1. [`CleanupComponent()`](../modules/gameplay_2d/spine/spine_system.cpp:62) 现在已不再是“刻意跳过删除”的诊断版本；
2. [`Update()`](../modules/gameplay_2d/spine/spine_system.cpp:115) 的失败回滚已经收口，不再是原先那种 atlas 删除零散、其余对象状态悬空的实现；
3. 问题现象被推进成 **CRT heap assertion**，从而把“可能是堆坏了”升级为“确实进入了堆校验失败”；
4. 真正需要继续收敛的，已经不再是“是不是生命周期问题”，而是：
   - **具体哪条释放顺序错误；**
   - **是否存在重复释放；**
   - **是否有某个对象在失败路径上被部分构造、部分析构。**

---

## 11. 新一轮补充：逐个禁用/恢复用例后的最新结论

在继续做结构性治理（例如将 [`SpineRendererComponent`](../engine/ecs/components_2d.h:95) 收口为统一 runtime 智能句柄）之后，又进行了一轮更直接的二分验证：**不再先改 [`SpineSystem`](../modules/gameplay_2d/spine/spine_system.h:21) 实现，而是反过来逐个裁剪 [`tests/spine/spine_system_test.cpp`](../tests/spine/spine_system_test.cpp) 中的 4 个独立用例，确认到底是谁稳定触发 hang。**

这轮实验的价值在于，它能把“代码看起来像有问题”与“到底哪个测试场景稳定触发问题”分开。结论如下。

### 11.1 禁用第 1 个用例后，挂起仍然存在

先整体禁用 [`Given_EmptySpinePaths_When_Update_Then_ComponentStateRemainsUnchanged`](../tests/spine/spine_system_test.cpp:10) 的测试体，只保留空返回。

结果：

- [`engine.spine`](../tests/spine/CMakeLists.txt) 仍然超时；
- 说明“空路径 no-op 场景”**不是唯一触发点**；
- 也说明问题并不是简单由“完全不配置路径”这个边界触发。

### 11.2 禁用第 3 个用例后，挂起仍然存在

随后再禁用 [`Given_IncompleteSpinePaths_When_Update_Then_RuntimeObjectsRemainNull`](../tests/spine/spine_system_test.cpp:47) 的测试体，只保留空返回。

结果：

- [`engine.spine`](../tests/spine/CMakeLists.txt) 依旧超时；
- 说明“只配一侧路径”的半初始化场景，同样**不是唯一触发点**；
- 这一步排除了此前较强的一个直觉误判：问题并不只集中在 partial-path 分支。

### 11.3 禁用第 4 个用例后，`engine.spine` 可以通过

接着把 [`Given_MultipleMissingSpineComponents_When_Update_Then_AllComponentsRemainStable`](../tests/spine/spine_system_test.cpp:69) 整体禁用。

结果：

- [`engine.spine`](../tests/spine/CMakeLists.txt) 可以正常通过；
- 这说明当前最稳定的触发器，已经从“第 2 + 第 3 个组合”进一步收敛到：
  - **第 4 个用例高度相关；**
  - 或者更准确地说，**只要执行到第 4 个场景，问题就非常容易显现。**

这是本轮最关键的推进，因为它把范围从“多用例组合之间的模糊污染”进一步压缩到了**一个具体测试场景**。

### 11.4 将第 4 个用例恢复成单实体版本后，仍然超时

为了进一步判断是否必须“两个缺失 Spine 组件一起更新”才会触发问题，又把 [`Given_MultipleMissingSpineComponents_When_Update_Then_AllComponentsRemainStable`](../tests/spine/spine_system_test.cpp:69) 暂时改成**只保留第一个 entity** 的单实体版本。

单实体实验版保留了：

- 缺失的 `skeleton_data_path` / `atlas_path`；
- `current_animation = "idle"`；
- `dirty_animation = true`；
- `time_scale = 0.5f`；
- 调用一次 [`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:249)；
- 断言 [`runtime`](../engine/ecs/components_2d.h:103) 仍为空。

结果却是：

- 单实体实验版**仍然超时**；
- 且测试内最后能稳定打印到“return 前”的日志；
- 表明问题已经不能简单归纳成“只有双组件一起更新才触发”。

这一步非常重要，它纠正了一个很容易产生的误解：

> 第 4 个用例之所以可疑，不一定是因为“两个组件互相影响”；也可能是因为它覆盖了某种此前未被单独隔离出来的缺失资源 + 动画状态边界，而这个边界即使用单实体也能触发更底层问题。

### 11.5 这轮二分后的最佳结论

结合上述实验，当前最可靠的判断是：

1. [`Given_EmptySpinePaths_When_Update_Then_ComponentStateRemainsUnchanged`](../tests/spine/spine_system_test.cpp:10) 不是主触发器；
2. [`Given_IncompleteSpinePaths_When_Update_Then_RuntimeObjectsRemainNull`](../tests/spine/spine_system_test.cpp:47) 不是主触发器；
3. [`Given_MultipleMissingSpineComponents_When_Update_Then_AllComponentsRemainStable`](../tests/spine/spine_system_test.cpp:69) 与 hang **高度相关**；
4. 但该相关性不能再被草率地解释为“必须双组件同时存在”；
5. 因而，问题更可能位于 [`modules/gameplay_2d/spine/spine_system.cpp`](../modules/gameplay_2d/spine/spine_system.cpp:249) 或其调用的底层 Spine runtime 边界，而不是测试本身的业务编排错误。

这也是为什么本轮最终选择：

- **先停止继续二分实验；**
- **先把测试恢复到原始形态；**
- **先把调试经验写进文档；**

避免把诊断态代码长期留在仓库中，影响后续判断。

---

## 12. 后续建议

### 11.1 下一轮优先排查真实释放顺序，而不是继续泛化怀疑“全局态”

在拿到 CRT heap assertion 之后，下一轮优先级应做如下调整：

- 第一优先级：验证 [`CleanupComponent()`](../modules/gameplay_2d/spine/spine_system.cpp:62) 的删除顺序；
- 第二优先级：验证 [`Update()`](../modules/gameplay_2d/spine/spine_system.cpp:115) 内部创建失败后的回滚顺序是否与正常销毁顺序一致；
- 第三优先级：检查是否有对象在“赋值进组件前异常 / 赋值进组件后异常”两种边界下出现重复销毁。

相比之下，下列方向虽然仍可保留，但优先级已经下降：

- [`engine/base/debug.cpp`](../engine/base/debug.cpp) 一类全局日志/关闭路径；
- Spine runtime 内部跨用例静态状态；
- 第 2 用例局部 [`AssetManager`](../engine/assets/asset_manager.h:222) 的外溢影响。

### 11.2 Windows 下要同时观察终端输出和系统弹窗

本轮一个很实用的经验是：

- 终端里看到“最后一行打印后没有后续输出”，并不等于程序只是静默卡住；
- 如果是在 Debug CRT 环境，必须同时观察是否弹出了系统级断言框；
- 否则很容易把“等待用户点击按钮的断言弹窗”误诊成“Catch2 / 测试线程无响应”。

### 11.3 修复期日志应该服务于验证“所有权和顺序”，不是单纯刷更多打印

下一轮日志建议继续保持最小化，只围绕这些点：

- [`CleanupComponent()`](../modules/gameplay_2d/spine/spine_system.cpp:62) 每一步删除前后；
- [`Update()`](../modules/gameplay_2d/spine/spine_system.cpp:136) 的创建成功与失败回滚；
- 触发断言前最后一个成功执行的 delete 边界。

也就是说，日志应优先回答：

- 是谁先删了？
- 谁在删的时候访问了已无效的数据？
- 哪个指针在进入 `delete` 前就已经不再可信？

而不是继续泛化地打印更多测试流程信息。

### 11.4 在真正修复前，不要长期保留侵入式诊断逻辑

当前一些诊断代码仍然是为了缩圈而存在，例如：

- [`PrintSpineDiag()`](../tests/spine/spine_system_test.cpp:11) 大量打印；
- [`tests/engine/main.cpp`](../tests/engine/main.cpp:48) 的 runner 级打印；
- [`depends/catch/catch.hpp`](../depends/catch/catch.hpp) 内部诊断；
- [`CleanupComponent()`](../modules/gameplay_2d/spine/spine_system.cpp:62) 中的大量删除日志；
- [`Update()`](../modules/gameplay_2d/spine/spine_system.cpp:139) 一类失败路径诊断日志。

这些代码对于定位问题非常有效，但不适合长期作为常态实现保留。后续真正修复根因后，应当逐步回收，避免把调试行为误当成正式设计。

### 11.5 为挂起 / 断言问题保留文档化复盘

相比只在聊天记录里保留信息，像本文这样把以下内容沉淀下来更有价值：

- 最小复现组合；
- 已排除方向；
- 关键观察点；
- 新增断言证据；
- 当前最可疑的释放链；
- 下一轮建议。

这样未来即使换人继续排查，也不会重复走已经证伪过的弯路。

---

## 13. 最终结论（截至本轮）

这次 Spine 挂起调试的最重要产出，已经从最初的“把问题收敛到第 3 个用例返回边界”进一步推进为：

> 在 Windows 下，[`dse_spine_tests`](../tests/spine/CMakeLists.txt) 的第 2 + 第 3 个 Spine 用例组合最初表现为挂起；继续加入诊断并收紧 [`SpineSystem`](../modules/gameplay_2d/spine/spine_system.h:21) 的资源回滚/释放逻辑后，问题进一步显化为 CRT debug heap assertion，说明根因确实位于 Spine runtime 对象的所有权与释放协议，而不是 Catch2 runner、`World` 常规清理或单纯的静态 loader 污染。

截至当前，已被排除或降权的方向包括：

- [`main()`](../tests/engine/main.cpp:48) 外层收尾；
- Catch reporter 尾端；
- [`World::Clear()`](../engine/ecs/world.h:48) 常规清理路径；
- [`SpineSystem`](../modules/gameplay_2d/spine/spine_system.h:21) 栈对象“表层析构是否执行到”这一最粗粒度怀疑；
- 静态 [`EngineTextureLoader`](../modules/gameplay_2d/spine/spine_system.cpp:36) 单点污染假设；
- “只是普通超时/线程挂死”的表层判断。

当前最高优先级的根因画像是：

- [`SpineRendererComponent`](../engine/ecs/components_2d.h:95) 对 Spine runtime 裸指针的所有权设计脆弱；
- [`CleanupComponent()`](../modules/gameplay_2d/spine/spine_system.cpp:62) 的真实删除顺序仍需继续验证；
- [`Update()`](../modules/gameplay_2d/spine/spine_system.cpp:115) 的失败回滚虽然已收口，但仍需要通过重编 + 干净复测确认是否彻底消除堆断言。

因此，后续排查应优先聚焦：

- 真实删除顺序是否满足 Spine runtime 依赖关系；
- 失败回滚与正常销毁路径是否一致；
- 是否存在重复释放或已损坏指针进入 `delete` 的情况。

---

## 14. 本轮继续补充：基于现有证据的最新诊断结论

在本轮继续对照源码、补充最小日志方案，并结合新的 Windows 断言截图后，问题画像又进一步收敛。

### 14.1 新增的直接证据：CRT 断言截图确认了“不是纯挂起”

本轮重新运行[`bin/dse_spine_tests.exe`](../bin/dse_spine_tests.exe)的[`[engine][unit][spine]`](../tests/spine/CMakeLists.txt)时，再次观察到 **Microsoft Visual C++ Runtime Library** 弹窗。

截图中关键信息为：

- Program：[`bin/dse_spine_tests.exe`](../bin/dse_spine_tests.exe)
- File：`minkernel\crts\ucrt\src\appcrt\heap\debug_heap.cpp`
- Line：`996`
- Expression：`__acrt_first_block == header`

这一步再次强化了一个关键判断：

> 当前表象虽然仍被 CTest 记录为 timeout，但根因层已经不是“普通线程阻塞”，而是 **Windows Debug CRT 在堆校验阶段拦截到了更早发生的堆破坏/错误释放**。

### 14.2 本轮重新排序后的候选根因

如果把本轮重新考虑过的 6 个来源按概率排序，可以得到：

1. [`SpineRuntimeHandle`](../modules/gameplay_2d/spine/spine_system.cpp:68) 内部 runtime 资源在回滚或销毁时破坏堆；
2. [`AnimationState`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:344)、[`AnimationStateData`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationStateData.h:45)、[`Skeleton`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/Skeleton.cpp:134)、[`SkeletonData`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/SkeletonData.cpp:59)、[`Atlas`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/Atlas.cpp:69) 之间的释放协议不满足；
3. 测试二进制启动后、测试体真正执行前的某个全局/静态对象已带入损坏状态；
4. Catch2 调用栈本身导致未定义行为显化；
5. [`World`](../engine/ecs/world.h) / ECS 清理路径自身破坏堆；
6. 单纯的测试编排逻辑错误。

其中，经过前几轮排除后，当前最高概率的两个来源已经非常集中在前 1、2 项，尤其是第 2 项。

### 14.3 为什么当前继续降权 Catch2 / ECS / 测试编排

本轮没有新的证据支持以下方向上升：

- [`Catch::Session::run()`](../tests/engine/main.cpp:54) 结束后的 runner 清理；
- [`World::Clear()`](../engine/ecs/world.h) 或 registry 常规清空；
- 仅由第 4 个测试用例业务编排本身触发的问题；
- 静态 [`EngineTextureLoader`](../modules/gameplay_2d/spine/spine_system.cpp:86) 作为单点污染源。

原因是：

- 旧文档已经证明问题在 active test case 返回边界之前显化；
- 本轮新的 CRT 断言继续把问题指向堆破坏，而不是框架尾清理；
- 当前最直接受怀疑的对象，仍然是 Spine runtime 各对象之间的所有权和析构顺序。

因此，本轮的最佳判断是：

> **问题主轴仍然在 Spine runtime 生命周期/释放协议，而不是 Catch2、World、或测试业务编排。**

### 14.4 本轮已铺设但尚未完成实测的最小日志方案

为了验证“到底是测试入口前就已损坏，还是进入测试体后由 runtime 释放链触发”，本轮已补充两组最小日志代码。

第一组位于[`modules/gameplay_2d/spine/spine_system.cpp`](../modules/gameplay_2d/spine/spine_system.cpp:68)：

- 给[`SpineRuntimeHandle::ResetWithDiagnostics()`](../modules/gameplay_2d/spine/spine_system.cpp:75)加入逐对象释放前后日志；
- 给[`CleanupComponent()`](../modules/gameplay_2d/spine/spine_system.cpp:170)加入[`comp.runtime.reset()`](../modules/gameplay_2d/spine/spine_system.cpp:182)前后日志；
- 给[`BuildRuntime()`](../modules/gameplay_2d/spine/spine_system.cpp:117)失败回滚路径增加具体原因标签。

这组日志的目标是回答：

- 最后一个成功释放的对象是谁；
- 是否在释放[`AnimationState`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:344)或[`Atlas`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/Atlas.cpp:69)时崩坏；
- 失败回滚与正常销毁是否走了不同的对象顺序。

第二组位于[`tests/spine/spine_system_test.cpp`](../tests/spine/spine_system_test.cpp:1)：

- 给文件级静态初始化加了[`PrintSpineTestDiag()`](../tests/spine/spine_system_test.cpp:9)；
- 给 4 个测试用例都加了 enter / before update / after update / before asserts / return 边界日志。

这组日志的目标是回答：

- 若能看到[`[spine-test] caseX enter`](../tests/spine/spine_system_test.cpp:10)而随后断言，说明损坏仍在测试体执行阶段显化；
- 若连文件静态初始化日志都无法稳定回传，则说明损坏可能早于测试体运行，被 CRT 在更后续堆操作时拦截。

### 14.5 一个现实阻塞：[`LNK1168`](../build_fast_tests.bat) 反复说明旧进程仍占用测试二进制

本轮还再次稳定复现了 Windows 侧的另一个伴随症状：

- [`cmake --build`](../build_fast_tests.bat) 在重编[`dse_spine_tests`](../tests/spine/CMakeLists.txt)时持续报 `LNK1168`；
- 指向[`bin/dse_spine_tests.exe`](../bin/dse_spine_tests.exe)仍被占用；
- 说明挂起/断言后的旧进程没有被彻底结束。

这进一步验证了文档前面的方法论：

> 在 Windows 下，这类“挂起”问题调试时，**结束旧测试进程本身就是调试流程的一部分**，否则链接错误会遮蔽新的验证结果。

### 14.6 截至本轮的最佳结论

截至当前，最可靠的新增结论可以归纳为：

1. 新的 CRT 断言截图继续强化了“这是堆破坏/错误释放，而不是纯挂起”的判断；
2. 当前最高概率根因，仍然集中在[`SpineRuntimeHandle`](../modules/gameplay_2d/spine/spine_system.cpp:68)所持 Spine runtime 对象的销毁链；
3. [`AnimationState`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:344) / [`AnimationStateData`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationStateData.h:45) / [`Skeleton`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/Skeleton.cpp:134) / [`SkeletonData`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/SkeletonData.cpp:59) / [`Atlas`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/Atlas.cpp:69) 之间的所有权与释放顺序，是目前最值得继续验证的主线；
4. Catch2、[`World`](../engine/ecs/world.h) 常规清理、测试编排本身，目前都缺乏新的上升证据；
5. 下一轮如果继续实测，应优先在“彻底结束旧进程、解除[`LNK1168`](../build_fast_tests.bat)文件锁”后，重新运行已铺设的最小日志版本，以确认断言究竟发生在测试入口前还是 runtime 释放链内部。

---

## 15. 新一轮修复与继续定位：第一处根因已修，但仍有第二处堆破坏

### 15.1 已确认并实施的第一处修复

继续深挖[`AnimationState`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:344)后，终于把第一处“可直接落地修复”的根因收敛清楚：

- 运行期[`EventQueue::drain()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:275)在处理[`EventType_Dispose`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationState.h:52)时，会调用[`disposeTrackEntry()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:699)；
- 而[`disposeTrackEntry()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:699)并不会直接 delete，而是先[`TrackEntry::reset()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:198)，再放回对象池[`Pool::free()`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Pool.h:63)；
- 但旧版[`AnimationState::~AnimationState()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:344)却仍在对 `_mixingFrom` / `_next` / `entry` 链做直接 `delete`。

这意味着同一批[`TrackEntry`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationState.h:92)对象，运行过程中可能已经进入对象池，析构时却又被手工 `delete`，属于典型的“池化回收路径”和“原生 delete 路径”并存。

因此，本轮已先对[`AnimationState::~AnimationState()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:344)做了第一处修复：

- 不再直接 `delete` track entry 链；
- 改为统一调用[`disposeTrackEntry()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:699)回收到[`_trackEntryPool`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationState.h:468)。

### 15.2 修复后的实测结果：原断言并未消失

修复上述问题后，重新编译[`dse_spine_tests`](../tests/spine/CMakeLists.txt)成功；重新运行[`engine.spine`](../tests/spine/CMakeLists.txt)后，结果并不是“问题完全消失”，而是：

- 测试仍旧停在第一个用例返回之后；
- 仍旧再次弹出同一个 Windows CRT 断言窗口；
- 断言仍为：[`debug_heap.cpp`](../minkernel/crts/ucrt/src/appcrt/heap/debug_heap.cpp:996) `__acrt_first_block == header`。

这说明：

> “[`AnimationState::~AnimationState()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:344) 直接 delete pooled [`TrackEntry`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationState.h:92)” 确实是一处真实缺陷，但它**不是当前测试里唯一的堆破坏源**。

### 15.3 为什么第一处修复后仍然会炸：新的高概率点反而更清楚了

继续对照 Spine runtime 的容器实现后，新的高概率点比之前更具体：

- [`disposeTrackEntry()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:699) 会调用[`TrackEntry::reset()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:198)；
- [`TrackEntry::reset()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:198) 内部会对[`Vector`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Vector.h:40)成员执行 `clear()`；
- [`Vector::clear()`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Vector.h:62) 会逐个调用[`destroy()`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Vector.h:238)，只析构元素，不释放底层 buffer；
- 而真正释放 buffer 的时机，是[`Vector::~Vector()`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Vector.h:57)；
- [`TrackEntry`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:66) 析构虽为空，但它作为[`SpineObject`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/SpineObject.h:41)子类，在真正 `delete` 时仍会进入[`SpineObject::operator delete()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/SpineObject.cpp:57)并最终调用 `free`。

于是，新问题浮现出来：

- 如果某个[`TrackEntry`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationState.h:92)曾被放回[`Pool`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Pool.h:40)，它本身并没有被析构；
- 其内部[`Vector`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Vector.h:40)的底层 buffer 也继续留在对象里；
- 当[`Pool::~Pool()`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Pool.h:45)最终析构池内对象时，会对池中对象逐个 `delete`；
- 如果此时同一个对象又被别的路径重复加入池、或者对象池中的活跃对象链仍被别处持有并再次回收，就会再次击中同类堆断言。

也就是说，当前第二高概率根因已经从“对象池 + 直接 delete 并存”进一步收敛到：

> [`TrackEntry`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationState.h:92) 可能被**重复放回同一个[`Pool`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Pool.h:40)**，或者在“仍被引用”的状态下提前入池，最终在[`Pool::~Pool()`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Pool.h:45)统一销毁时触发堆链损坏。

### 15.4 下一轮最该验证的位置

既然最新截图表明第一处修复后断言地址与现象基本不变，那么下一轮不应再泛泛检查整个[`SpineRuntimeHandle`](../modules/gameplay_2d/spine/spine_system.cpp:68)，而应更窄地验证以下两个点：

1. [`disposeTrackEntry()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:699) 是否对同一个 entry 触发了两次；
2. [`Pool::free()`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Pool.h:63) 的 `contains` 判重是否足以覆盖“指针别名/链上重复回收”场景；
3. [`clearNext()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:1032)、[`clearTrack()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:536)、[`updateMixingFrom()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:792) 与析构阶段是否会对同一 entry 链重复排队 dispose。

换句话说，当前最值得加的最小日志已经不再是“整个 Spine 对象释放顺序”，而是：

- 每个[`TrackEntry`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/AnimationState.h:92) 的地址；
- 第一次进入[`disposeTrackEntry()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:699)的时机；
- 是否又在另一路径进入[`disposeTrackEntry()`](../depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/AnimationState.cpp:699)；
- 最终是否在[`Pool::~Pool()`](../depends/spine-runtimes/spine-cpp/spine-cpp/include/spine/Pool.h:45)中销毁到一块已经破坏的内存。

---

## 16. 最新一轮关键修正：先纠正“调试对象”，再暴露出旧产物/构建污染高概率根因

### 16.1 先发现了一个根本性偏差：之前一直在改错测试文件

本轮最大的信息增益，反而不是来自 Spine runtime 本身，而是来自对测试入口的重新核对。

最初一段时间里，调试工作主要落在[`tests/modules/gameplay_2d/spine/spine_system_test.cpp`](../tests/modules/gameplay_2d/spine/spine_system_test.cpp)；但在继续追踪后，通过列出实际注册到 Catch2 的测试名，最终确认真正被独立目标[`dse_spine_tests`](../tests/spine/CMakeLists.txt)执行的用例并不来自这个文件，而是来自：

- [`tests/spine/spine_system_test.cpp`](../tests/spine/spine_system_test.cpp)

并且实际运行的 case1 是：

- [`Given_EmptySpinePaths_When_Update_Then_ComponentStateRemainsUnchanged`](../tests/spine/spine_system_test.cpp:39)

这个纠偏非常关键，因为它解释了此前一个长期矛盾现象：

- 源码里已经新增了新日志；
- 但运行输出始终显示旧文案；
- 原因并不是构建没生效，而是**真正执行的根本不是当时正在修改的那份测试文件**。

### 16.2 在真正执行的 case1 中完成的最小 A/B 验证

当调试转移到[`tests/spine/spine_system_test.cpp`](../tests/spine/spine_system_test.cpp)后，围绕真实 case1 做了一整轮最小化验证，核心结论如下：

1. 把原始[`REQUIRE`](../depends/catch/catch.hpp:2701)断言改成先做 `bool` 快照，再用 `FAIL` 触发，结果症状不变；
2. 在 case 返回前显式执行[`world.registry().remove<SpineRendererComponent>(entity)`](../tests/spine/spine_system_test.cpp:72)，结果症状不变；
3. [`World::~World()`](../engine/ecs/world.cpp:33) 与[`SpineSystem::~SpineSystem()`](../modules/gameplay_2d/spine/spine_system.cpp:210) 都能完整打印到末尾；
4. 在真实 case1 中新增“不调用[`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267)”的空对照 case，结果该对照 case 正常；
5. 新增“手动模拟[`view<SpineRendererComponent>()`](../modules/gameplay_2d/spine/spine_system.cpp:240) + [`get<SpineRendererComponent>()`](../modules/gameplay_2d/spine/spine_system.cpp:242) + 字段读取”的 manual case，结果也正常。

这轮验证把问题窗口进一步缩到：

> 与真实 case1 的异常最直接相关的，不再是 Catch2 断言表达式、组件移除、[`World`](../engine/ecs/world.h) 清理或普通 `entt` 字段读取，而是**是否真正调用了[`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267)**。

### 16.3 继续缩圈后发现：不是“所有实例成员调用都会炸”，而是 `Update` 这个既有符号最可疑

为了确认是不是普通实例方法调用就会触发异常，继续做了更细的 A/B：

- 先把[`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267)临时缩成只打印 begin/end 后立刻返回；
- 结果：真实 case1 仍旧异常；
- 然后把 case1 中的[`system.Update(...)`](../tests/spine/spine_system_test.cpp:52)换成普通自由函数调用；
- 结果：case1 正常；
- 再换成同翻译单元中的静态成员[`SpineSystem::DebugNoopProbe()`](../modules/gameplay_2d/spine/spine_system.cpp:259)；
- 结果：正常；
- 再换成另一个现有非静态成员[`SpineSystem::SetAssetManager()`](../modules/gameplay_2d/spine/spine_system.cpp:247)；
- 结果：正常；
- 再新增全新非静态成员[`SpineSystem::DebugInstanceNoopProbe()`](../modules/gameplay_2d/spine/spine_system.cpp:251)；
- 结果：仍然正常。

因此，这轮最重要的符号级结论是：

> 当前异常并不是“任意实例成员调用都会触发”，而是**只有既有符号[`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267)在被调用时会触发异常窗口**，即便它的实现已经被临时缩减为与探针几乎相同的空逻辑。

这会把高概率根因从“业务逻辑 bug”进一步推向：

- 旧导出符号残留；
- 增量链接/旧 thunk/stub 未被替换；
- 测试与 DLL 之间使用了旧产物；
- 或某种构建级符号污染。

### 16.4 clean rebuild 暴露出的更强证据：之前很可能一直在混用旧测试产物

为了验证是否存在旧产物/增量链接污染，进一步尝试做彻底 clean rebuild。

这一轮又带来了一个比预期更重要的新结论：

1. 顶层[`CMakeLists.txt`](../CMakeLists.txt:22)中，测试目标由[`DSE_BUILD_ENGINE_TESTS`](../CMakeLists.txt:22)控制，默认值是 `OFF`；
2. clean 重新配置后，如果没有显式传入 `-D DSE_BUILD_ENGINE_TESTS=ON`，新的[`build_vs2022`](../build_vs2022)中根本不会生成[`dse_spine_tests`](../tests/spine/CMakeLists.txt)相关项目；
3. 而此前工作区里长期存在并被直接运行的[`bin/dse_spine_tests.exe`](../bin/dse_spine_tests.exe)，在 clean 之后已经不可再现；
4. clean 后再次检查时，[`bin`](../bin)目录甚至一度为空。

这条证据链的意义非常大：

> 之前被多次直接运行、并承载了大量“`Update` 符号异常”结论的[`bin/dse_spine_tests.exe`](../bin/dse_spine_tests.exe)，很可能本来就是**旧残留测试产物**；而 clean 之后测试目标必须通过显式开启[`DSE_BUILD_ENGINE_TESTS`](../CMakeLists.txt:22)重新生成。

换句话说，当前最高概率根因画像已经不是单纯的 Spine runtime 释放 bug，而是：

- **旧测试 EXE / 旧 DLL / 旧导出符号残留产物被长期重复运行**；
- 在这种污染环境下，只有历史上已存在的[`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267)符号表现异常，而新加探针符号都正常。

### 16.5 截至本轮的最佳结论与下一步

截至当前，这一轮最可靠的新结论可以归纳为：

1. “真实执行测试文件识别错误”已经被纠正，真正的调试对象是[`tests/spine/spine_system_test.cpp`](../tests/spine/spine_system_test.cpp)；
2. 在真实 case1 上完成的所有最小 A/B 验证都表明，源码层面的普通业务路径并没有给出足够证据支撑“`Update` 空逻辑本身写坏堆”；
3. 反而是 clean rebuild 过程中暴露出的测试开关[`DSE_BUILD_ENGINE_TESTS`](../CMakeLists.txt:22)与旧产物消失现象，使“**旧产物混用/构建污染**”上升为当前最高概率根因；
4. 因此，在回到正式修复前，最应该优先完成的是：
   - 使用显式 `-D DSE_BUILD_ENGINE_TESTS=ON` 的 clean 环境重新生成测试；
   - 确认新的[`dse_spine_tests`](../tests/spine/CMakeLists.txt)真实输出路径；
   - 在全新生成的测试 EXE 上重新验证 case1 与[`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267)行为；
   - 只有在确认“新产物仍能稳定复现”后，才值得继续追查源码级 `Update` 符号异常。

这一点也意味着：

> 如果下一轮在全新 clean 生成的测试产物上，问题不再复现，那么此前大量“只有[`Update`](../modules/gameplay_2d/spine/spine_system.cpp:267)会触发异常”的现象，本质上更像是**构建残留造成的假象**，而不是最终的运行时根因。

### 16.6 clean rebuild 的最新补充结果

在文档记录完上述诊断后，又继续做了一次更严格的 clean rebuild：

- 删除[`build_vs2022`](../build_vs2022)；
- 删除[`bin`](../bin)；
- 重新以显式 `-D DSE_BUILD_ENGINE_TESTS=ON` 配置；
- 重建[`dse_spine_tests`](../tests/spine/CMakeLists.txt)；
- 立即运行真实 case1 [`Given_EmptySpinePaths_When_Update_Then_ComponentStateRemainsUnchanged`](../tests/spine/spine_system_test.cpp:39)。

这一次命令整体返回值为 `0`，且不再出现：

- “测试目标不存在”；
- “旧[`bin/dse_spine_tests.exe`](../bin/dse_spine_tests.exe)路径找不到”；
- 或 clean 后找不到工程输出目录等先前伴随症状。

虽然本次终端没有回传完整测试文本输出，但在当前环境规则下，这至少可以先视为：

1. clean rebuild 流程本身已经真正跑通；
2. 此前“旧测试产物/旧 DLL 混用”的构建污染路径，已得到明显削弱；
3. “必须显式开启[`DSE_BUILD_ENGINE_TESTS`](../CMakeLists.txt:22) 才能在 clean 环境下重新拿到真实测试目标”这一点已经被再次实证。

因此，当前阶段最稳妥的工作结论是：

> 旧产物混用并不是边缘噪声，而是这次调试链中的核心干扰项。只有在显式开启[`DSE_BUILD_ENGINE_TESTS`](../CMakeLists.txt:22)并完成 clean rebuild 后得到的新产物上，后续所有关于[`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267)的判断才值得继续成立。

### 16.7 在全新产物上的第一条真实运行结果

在确认 clean rebuild 后的新产物确实存在：

- [`bin/dse_spine_tests.exe`](../bin/dse_spine_tests.exe)
- [`bin/DSEngine_debug.dll`](../bin/DSEngine_debug.dll)

之后，重新运行真实 case1 [`Given_EmptySpinePaths_When_Update_Then_ComponentStateRemainsUnchanged`](../tests/spine/spine_system_test.cpp:39)，其当前调用路径为新增实例探针[`SpineSystem::DebugInstanceNoopProbe()`](../modules/gameplay_2d/spine/spine_system.cpp:251)。

实测结果是：

- 日志稳定打印到 case1 返回；
- [`SpineSystem::~SpineSystem()`](../modules/gameplay_2d/spine/spine_system.cpp:210) 完整打印；
- [`World::~World()`](../engine/ecs/world.cpp:33) 与[`World::Clear()`](../engine/ecs/world.cpp:24) 都完整打印到结束；
- 没有再出现先前那种“在 case 返回后窗口异常显化”的旧症状。

这条结果的意义在于：

1. clean rebuild 之后，至少“新产物 + 当前实例探针路径”是稳定的；
2. 先前一整轮围绕[`Update`](../modules/gameplay_2d/spine/spine_system.cpp:267)的异常现象，确实受到了旧产物混用的强干扰；
3. 后续若要继续判断[`Update`](../modules/gameplay_2d/spine/spine_system.cpp:267)是否仍然异常，必须在**当前这套新产物环境**下逐步回放，而不能再直接引用旧运行结论。

因此，下一步最合理的验证顺序已经变成：

- 先把真实 case1 从[`DebugInstanceNoopProbe()`](../modules/gameplay_2d/spine/spine_system.cpp:251)切回当前空版[`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267)；
- 在同一套 clean 生成的新产物上重新运行；
- 观察“旧异常”是否还能重新出现。

### 16.8 新产物上的回放结果：空版 `Update` 已不再复现旧异常

在完成上述回放后，又立即基于同一套 clean 生成的新产物重新编译并运行真实 case1。此时：

- case1 已从[`DebugInstanceNoopProbe()`](../modules/gameplay_2d/spine/spine_system.cpp:251)切回调用当前空版[`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267)；
- [`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267) 内部仍是 short-circuit 版本，只打印 begin/end；
- 测试输出稳定打印到 [`World::~World()`](../engine/ecs/world.cpp:33) 结束；
- 旧的 case 返回后异常窗口没有再出现。

这条结果非常关键，因为它直接推翻了旧产物时代得到的一条强结论：

> “只要调用[`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267)，哪怕函数体为空，异常也会稳定复现。”

在新产物环境下，这个命题已经不成立。

因此截至当前，最稳固的阶段性判断已经变成：

1. 旧测试 EXE / 旧 DLL / 旧导出残留产物混用，极大概率就是上一阶段诊断链中的核心干扰项；
2. 至少在全新 clean 生成的新产物上，空版[`SpineSystem::Update()`](../modules/gameplay_2d/spine/spine_system.cpp:267)本身不会复现旧异常；
3. 后续如果还要继续追查真正的运行时根因，必须采用“从当前稳定的新产物基线，逐步回滚调试改动”的方式，而不能再沿用旧产物时期的异常结论。

也就是说，下一步最合理的回归路线已经从“继续猜 `Update` 符号污染”切换为：

- 从当前稳定的 short-circuit [`Update`](../modules/gameplay_2d/spine/spine_system.cpp:267) 出发；
- 逐步恢复为“带 [`view<SpineRendererComponent>()`](../modules/gameplay_2d/spine/spine_system.cpp:240) / [`get<SpineRendererComponent>()`](../modules/gameplay_2d/spine/spine_system.cpp:242) 的早退版本”；
- 再进一步恢复真实业务逻辑；
- 观察具体是哪一层恢复后，异常才会重新出现。
