# DOC-11 AI 驱动项目测试策略

本文档用于为当前 DSEngine 提供一套更适合 **AI 驱动开发** 的测试策略，目标不是追求测试形式尽可能丰富，而是以**尽量少的人力投入**，最大化保证引擎功能正确性，并降低 AI 持续改动带来的回归风险。

---

## 1. 文档目标

本文档重点回答以下问题：

1. 当前是否应该全面引入视觉回归测试
2. 如果项目代码长期由 AI 驱动，怎样设计测试体系才能更稳定
3. 怎样在尽量减少人工介入的前提下，保证引擎功能正确
4. 未来如果要补视觉回归，应该如何控制范围与成本

本文档不替代现有测试说明文档，而是作为现有测试体系的**策略补充文档**。

相关文档：

- [`doc/DOC-04_TESTING.md`](doc/DOC-04_TESTING.md)
- [`doc/TESTING_CTEST_GUIDE.md`](doc/TESTING_CTEST_GUIDE.md)
- [`doc/DOC-07_ROADMAP.md`](doc/DOC-07_ROADMAP.md)
- [`doc/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md`](doc/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md)

---

## 2. 当前问题定义

如果项目代码长期由 AI 驱动，最常见的真实风险通常不是“程序完全跑不起来”，而是以下几类：

- 改了逻辑但没有意识到影响别的模块
- 改了资源注入链路，导致运行时行为悄悄退化
- 改了系统调度顺序，结果仍能运行但行为错误
- 改了编辑器 / Runtime 边界，导致场景状态不一致
- 改了渲染提交流程，导致结果不正确但不一定立刻崩溃

因此，测试体系的目标不应只是“验证能不能启动”，而应尽量做到：

- 改动后能自动判断功能是否仍然正确
- 不依赖人工肉眼逐帧检查
- 不要求频繁维护大量测试资源
- 能适应 AI 高频小步修改

---

## 3. 总体结论

当前阶段**不建议全面引入视觉回归测试**。

更适合当前 DSEngine 的方案是：

1. 以 [`CTest`](tests/CMakeLists.txt) 组织的**逻辑回归测试**作为主体
2. 增加少量**场景级 smoke 测试**覆盖主链
3. 为 smoke 场景输出**结构化运行快照**，替代大部分人工看画面
4. 只在少数高视觉风险路径上使用**极少量视觉回归测试**

一句话概括：

**逻辑断言优先，场景 smoke 兜底，渲染快照为主，视觉回归极小化。**

---

## 4. 为什么不建议全面视觉回归

视觉回归测试并不是没有价值，但对当前阶段的 DSEngine 来说，如果全面铺开，问题通常会大于收益。

### 4.1 全面视觉回归的主要问题

- 基线图片数量会快速膨胀
- Windows / OpenGL / 驱动差异容易引入噪音
- 正常渲染调整也可能造成大量基线更新
- AI 改动后很容易出现“图片变了但不一定是 bug”的情况
- 维护截图基线会把工作重新推回给人工

### 4.2 AI 驱动项目更常见的问题类型

AI 驱动开发下，更常见的问题其实是：

- 逻辑 silently wrong
- 状态同步错误
- 回调未触发
- 资源注入链路退化
- 更新顺序改变后结果错
- 场景序列化与运行态不一致

这些问题更适合由：

- 单元测试
- 回归测试
- 场景 smoke
- 结构化运行快照
- 工作流级断言

来捕获，而不是优先依赖截图比对。

---

## 5. 推荐测试架构

建议将测试体系固定为四层。

## 5.1 第一层：模块级确定性测试（主力）

这是当前最值得持续投入、也是最省人力的一层。

目标：验证**模块逻辑是否正确**。

继续沿用并强化当前已有测试，例如：

- [`tests/engine/scripting/lua_runtime_test.cpp`](tests/engine/scripting/lua_runtime_test.cpp)
- [`tests/modules/gameplay_2d/ui/ui_system_test.cpp`](tests/modules/gameplay_2d/ui/ui_system_test.cpp)
- [`tests/modules/gameplay_2d/physics/physics2d_system_test.cpp`](tests/modules/gameplay_2d/physics/physics2d_system_test.cpp)
- [`tests/modules/gameplay_2d/tilemap/tilemap_system_test.cpp`](tests/modules/gameplay_2d/tilemap/tilemap_system_test.cpp)

每个高价值系统建议强制覆盖以下四类测试：

1. 默认值 / 初始化
2. 正常路径
3. 边界路径
4. 历史 bug 回归路径

### 这一层的优点

- 执行快
- 稳定性高
- AI 易于补充
- 失败定位清晰
- 不依赖人工观察

### 这一层的要求

后续新增或重构模块时，应优先补齐：

- 正常行为断言
- 错误输入断言
- 边界条件断言
- 回归场景断言

如果一个模块没有确定性断言，就不应认为它具备稳定 AI 迭代基础。

---

## 5.2 第二层：场景级 smoke 测试（主链兜底）

目标：验证**主链是否真的能跑通**。

这类测试不应很多，但必须覆盖当前最关键的功能闭环。

建议维护以下最小 smoke 场景：

1. 2D UI 基础交互场景
2. Physics2D 场景
3. Tilemap 场景
4. Particle 场景
5. Lua Runtime 场景
6. Spine 场景
7. 后续可增加 3D 最小场景

### smoke 场景的验收重点

每个 smoke 场景不追求复杂内容，而是验证：

- 能启动
- 能连续运行固定帧数
- 不崩溃
- 主链关键指标符合预期
- 关键实体或关键状态发生正确变化

例如可以断言：

- `draw_calls > 0`
- `sprite_count > 0`
- `active_particles > 0`
- 场景加载后实体数量正确
- UI 点击后回调触发
- 物理更新后目标实体位置改变
- Spine / Lua / Tilemap 主链未失效

### 设计原则

- 场景数量少而精
- 场景资源固定
- 相机固定
- 分辨率固定
- 每个场景只验证一个最小闭环

---

## 5.3 第三层：渲染 / 运行结构化快照（强烈推荐）

这是最适合当前 DSEngine 的增强层。

目标：用**结构化结果断言**替代大部分人工看画面或截图比对。

建议为 smoke 测试增加统一的测试快照结构，例如：

- 场景名称
- 运行帧数
- 实体数量
- draw calls
- sprite count
- render passes
- max batch sprites
- 物理刚体数量
- 粒子发射器数量
- 活跃粒子数量
- UI 点击次数
- tile runtime entity 数量
- mesh item 数量
- shadow pass 是否执行
- 错误计数

例如输出一个测试专用 JSON：

```json
{
  "scene": "ui_basic",
  "frames": 60,
  "entities": 12,
  "draw_calls": 3,
  "sprite_count": 8,
  "ui_click_count": 1,
  "active_particles": 0,
  "errors": 0
}
```

测试时不要求逐字完全一致，而是比较关键字段是否在预期范围内。

### 推荐原因

相比视觉回归，这种方式：

- 更稳定
- 更适合引擎功能验证
- 不易受驱动和平台差异影响
- 更容易由 AI 自动补充与维护
- 失败后更容易定位是逻辑问题还是渲染提交流程问题

### 可直接利用的现有基础

当前 [`FramePipeline::RunRenderInternal()`](engine/runtime/frame_pipeline.cpp:370) 已经具备部分运行时统计能力，例如：

- draw calls
- sprite count
- render passes
- 物理体数量
- 粒子数量
- 平均 update / fixed / render 时间
- 资源主线程回调队列统计

后续建议在测试模式下，把这些统计整合为统一快照对象。

---

## 5.4 第四层：极少量视觉回归（仅补高视觉风险点）

视觉回归不是完全不做，而是要**缩到最小范围**。

只建议在以下高风险路径使用：

- UI Mask / 裁剪
- 富文本 / 字体 / 本地化排版
- Tilemap 错位类问题
- 3D Shadow / Skybox / 后处理
- 明显必须看画面才容易判断的问题

### 视觉回归使用原则

- 只保留极少数基线图
- 固定分辨率
- 固定素材
- 固定相机
- 优先局部区域比对，而不是整张图比对
- 允许小容差，避免对像素级微差过度敏感

### 不建议做的事

- 不建议给所有 smoke 场景做截图基线
- 不建议把整套渲染 correctness 建立在图像比对上
- 不建议一开始就覆盖大量 UI / 3D 画面

---

## 6. 推荐的实际落地顺序

为了尽量减少人工工作量，建议按以下顺序推进。

## 6.1 第一阶段：先不引入视觉回归

优先完成三件事：

1. 固化当前最小门禁集合
2. 给关键模块补齐四类确定性测试
3. 增加最小场景 smoke + 结构化快照断言

这一阶段完成前，不建议启动全面视觉回归。

## 6.2 第二阶段：跑稳 smoke 体系

当 smoke 测试能稳定运行后，再继续：

- 收紧 [`engine.unit`](tests/engine/CMakeLists.txt:84)
- 收紧 [`engine.lua_runtime`](tests/engine/CMakeLists.txt:175)
- 收紧关键 2D smoke 门禁
- 为 smoke 测试补充统计断言与错误摘要

## 6.3 第三阶段：仅在高视觉风险点补少量截图回归

只有当出现以下情况时，才建议补视觉回归：

- 逻辑断言无法有效覆盖该类错误
- smoke 快照不足以描述视觉异常
- 该问题确实高频复发
- 该场景稳定、基线易维护

首批视觉回归建议不超过以下三个方向：

1. UI Mask
2. RichText / Localization 字形排版
3. 3D Shadow（仅当 3D 成为高优先级方向后）

---

## 7. 推荐的最小执行方案

如果目标是“尽量减少人工工作”，建议采用下面这套最小可执行方案。

## 7.1 保留并强化当前 CTest 主门禁

继续以 [`CTest`](tests/CMakeLists.txt) 为标准测试组织层。

建议日常最小门禁至少保留：

- `engine.unit`
- `engine.lua_runtime`
- `engine.cpp_runtime`
- `engine.resource_injection`
- `engine.spine`
- `engine.2d.ui`
- `engine.2d.physics2d`
- `engine.2d.particle`
- `engine.2d.localization`

这与 [`doc/DOC-04_TESTING.md`](doc/DOC-04_TESTING.md) 和 [`doc/TESTING_CTEST_GUIDE.md`](doc/TESTING_CTEST_GUIDE.md) 当前建议一致。

## 7.2 只维护 6 个最小 smoke 场景

建议仅维护：

- UI 基础交互
- Physics2D
- Tilemap
- Particle
- Lua Runtime
- Spine

后续 3D 成熟后，再增加：

- 3D 最小场景

## 7.3 所有 smoke 必须输出结构化快照

每个 smoke 测试跑完后，应输出统一的可解析结果，例如 JSON 或稳定日志摘要。

验收时只比较关键字段，不要求图像逐像素一致。

## 7.4 暂缓全面视觉回归

当前不引入大规模截图测试。

只有当某些视觉问题：

- 高风险
- 高频复发
- 逻辑断言难以覆盖

时，再单独给该路径增加小范围视觉回归。

---

## 8. AI 驱动开发下的验收原则

为了尽量减少人工投入，建议固定以下原则。

### 8.1 没有断言的功能，不视为可稳定迭代

如果一个模块只能“人工看起来没问题”，那它对 AI 持续修改是不安全的。

### 8.2 每次新增功能都必须绑定一种自动验收方式

优先顺序建议为：

1. 模块级确定性测试
2. 场景 smoke
3. 结构化快照断言
4. 视觉回归（仅必要时）

### 8.3 不以“截图完全一致”作为主验收标准

功能正确性应主要由：

- 逻辑断言
- 运行统计
- 状态快照
- 场景级主链验证

来定义。

### 8.4 失败输出必须可读

所有测试入口都应尽量输出：

- 哪个场景失败
- 哪个关键字段不满足
- 当前快照值是多少
- 预期范围是什么

避免把故障定位重新推回人工排查。

---

## 9. 与现有文档的关系

本策略与现有文档的关系如下：

- [`doc/DOC-04_TESTING.md`](doc/DOC-04_TESTING.md)：继续作为当前测试入口与回归范围说明
- [`doc/TESTING_CTEST_GUIDE.md`](doc/TESTING_CTEST_GUIDE.md)：继续作为本地最小门禁说明
- [`doc/DOC-07_ROADMAP.md`](doc/DOC-07_ROADMAP.md)：继续作为测试、构建、性能门禁的中期路线图
- [`doc/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md`](doc/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md)：继续定义 3D MVP 阶段的 smoke、场景启动和保存加载回归要求

本文档仅补充：

- AI 驱动开发下测试方式的优先级
- 为什么当前不应全面视觉回归
- 如何用更低成本提高功能正确性保障

---

## 10. 最终建议

当前 DSEngine 的最优测试策略不是“把所有内容都做成视觉回归”，而是：

1. 继续以 [`CTest`](tests/CMakeLists.txt) + 模块级逻辑回归为主
2. 用少量场景级 smoke 覆盖主链
3. 为 smoke 增加结构化运行快照
4. 只在极少数高视觉风险路径引入截图回归

因此，当前阶段的明确建议是：

- **不全面引入视觉回归测试**
- **优先建设场景 smoke + 结构化快照断言体系**
- **把视觉回归限制为少量补充手段，而不是主验收方式**

如果按这条路线推进，项目可以在尽量少人工维护的情况下，更稳定地约束 AI 对引擎代码的持续修改。