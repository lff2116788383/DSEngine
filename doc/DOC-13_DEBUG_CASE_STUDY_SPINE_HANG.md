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

## 10. 后续建议

### 10.1 下一轮优先排查全局状态与外部 runtime

已经排除多个表层候选项后，下一轮建议优先检查：

- [`engine/base/debug.cpp`](../engine/base/debug.cpp) 一类全局日志/关闭路径；
- Spine runtime 内部是否存在跨用例静态状态；
- 资源加载失败路径是否留下悬空引用；
- 第 2 用例局部 [`AssetManager`](../engine/assets/asset_manager.h) 生命周期是否仍间接影响第 3 用例。

### 10.2 在真正修复前，不要长期保留侵入式诊断逻辑

当前一些诊断代码是为了缩圈而存在，例如：

- [`PrintSpineDiag()`](../tests/spine/spine_system_test.cpp:11) 大量打印；
- [`tests/engine/main.cpp`](../tests/engine/main.cpp) 的 runner 级打印；
- [`depends/catch/catch.hpp`](../depends/catch/catch.hpp) 内部诊断；
- [`CleanupComponent()`](../modules/gameplay_2d/spine/spine_system.cpp:54) 中“诊断模式跳过删除”的逻辑。

这些代码对于定位问题非常有效，但不适合长期作为常态实现保留。后续真正修复根因后，应当逐步回收，避免把调试行为误当成正式设计。

### 10.3 为挂起问题保留文档化复盘

相比只在聊天记录里保留信息，像本文这样把以下内容沉淀下来更有价值：

- 复现组合；
- 已排除方向；
- 关键观察点；
- 下一轮建议。

这样未来即使换人继续排查，也不会重复走已经证伪过的弯路。

---

## 11. 最终结论

这次 Spine 挂起调试的最重要产出，不是立即修掉 bug，而是把问题从一个模糊的“`engine.spine` 会超时”收敛为：

> 在 Windows 下，[`dse_spine_tests`](../tests/spine/CMakeLists.txt) 的第 2 + 第 3 个 Spine 用例组合会触发挂起；第 3 个用例能执行到返回前打印，`SpineSystem` 作用域与 [`World::Clear()`](../engine/ecs/world.h) 也能走完，但调用栈没有成功返回到 Catch 的 `m_activeTestCase->invoke()` 之后。

已被排除的方向包括：

- `main()` 外层收尾；
- Catch reporter 尾端；
- `World` 常规清理路径；
- `SpineSystem` 栈析构的最表层怀疑；
- 静态 `EngineTextureLoader` 单点污染假设。

因此后续排查应优先聚焦：

- 跨用例共享状态；
- 资源失败路径残留引用；
- 更底层的未定义行为/堆破坏；
- Spine runtime 与全局辅助系统的交互边界。
