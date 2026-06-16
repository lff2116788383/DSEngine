# DSEngine 脚本 API 待办与轻量设计（Scripting API Backlog）

> 创建日期：2026-06-16 · 分支：`feature/engine-lib`
> 来源：基于代码实况的 Lua 绑定完整性核查（102 个 ECS 组件中 98 个已绑定，其余 4 个为内部组件或经 C ABI 间接绑定；`docs/api/LUA_API.md` 与代码零失效项、零未文档化函数）。
> 目标读者：引擎维护者 / 路线图决策者

> **结论前置：** 以下 4 项**均非 0.1.0-alpha 必须**，不阻塞发版。引擎脚本层不含它们也能做出完整游戏。按 YAGNI（You Aren't Gonna Need It），待真实需求出现再实现，届时设计也更有依据。本清单是核查副产物——记录引擎"当前未实现、故也未绑定"的脚本增强点，避免被"0 TODO"的干净表象埋没。
>
> 优先级：`1 time-scale` > `2 debug-draw` > `3 timer/tween（纯 Lua）` ≈ `4 AABB getter`。

---

## 1. 全局 time-scale（慢动作 / 暂停）

- **现状**：`engine/core` 无 `TimeScale`；`dt` 直接来自帧计时器。后处理里有 `post_process_film_grain_time_scale`，但那只影响该特效、不影响游戏逻辑。
- **收益 / 必要性**：**中高 / 非必须**。面向终端玩家——暂停菜单、命中停顿（hit-stop）、子弹时间。几乎每个游戏都需要"暂停"。
- **建议 API**（挂在 `dse.app` 下）：
  ```lua
  dse.app.set_time_scale(s)      -- 0 = 暂停, 1 = 正常, 0.5 = 半速
  dse.app.get_time_scale() -> number
  ```
- **实现要点**（关键：这是引擎功能，不是"绑个接口"那么简单）：
  - 在 `dt` 源头统一乘以 `scale`，并明确区分两条时间通道：
    - `scaled_dt` —— 喂给 gameplay 逻辑、动画 FSM、粒子、Tween、计时器；
    - `unscaled_dt` —— 喂给 UI / 输入 / 暂停菜单本身（否则 `scale=0` 时菜单也卡死）。
  - **物理**：Jolt / Box2D 的固定步长累加器必须用 `scaled_dt` 喂；`scale=0` 时停止累加，避免穿透或抖动。
  - **音频**：默认不缩放（或单独提供 pitch 选项）。
  - 主循环需同时分发 `scaled_dt` 与 `unscaled_dt`，子系统各取所需。
- **工作量 / 风险**：**中**。改动集中在主循环 + 各子系统 `dt` 入口；主要风险是"哪些用 scaled、哪些用 unscaled"的口径一致性，需逐子系统过一遍。
- **排期**：alpha 后**首选**（面向玩家、复用率最高）。最小可用版 = `set/get` + 主循环缩放 + 物理累加器适配，UI/输入/音频走 unscaled。

---

## 2. Immediate-mode debug-draw（脚本可视化调试）

- **现状**：`engine/render` 无 `DebugDraw` / immediate-mode 线渲染子系统；脚本无法画辅助线 / 形状。
- **收益 / 必要性**：**中（仅开发期）/ 非必须**。受益者是开发者：可视化 raycast、AI 路径、触发体、骨骼、AABB。不影响最终出货。
- **建议 API**（一帧有效，帧末自动清空）：
  ```lua
  dse.debug.draw_line(x0,y0,z0, x1,y1,z1, [r,g,b,a])
  dse.debug.draw_sphere(x,y,z, radius, [r,g,b,a])
  dse.debug.draw_box(cx,cy,cz, hx,hy,hz, [r,g,b,a])
  dse.debug.draw_text(x,y,z, str, [r,g,b,a])      -- 可选，billboard 文本
  ```
- **实现要点**：
  - C++ 侧加一个 `DebugDrawQueue`：每帧累积线段 / 点，在渲染末尾用一个 unlit pass 批量绘制，帧末清空。
  - 复用现有 `RenderGraph` 注册一个 debug pass；深度测试可开关（`draw_line` vs `draw_line_no_depth`）。
  - 仅在 dev / 非 shipping 构建启用（宏 `DSE_DEBUG_DRAW`）；release 编译为空函数，零开销。
  - 三后端（Vulkan / D3D11 / OpenGL）都需支持线段 topology。
- **工作量 / 风险**：**中**。是新子系统，但范围小、孤立，不触碰既有管线，风险低。底层可与编辑器 gizmo 复用。
- **排期**：alpha 后，开发体验项。

---

## 3. Timer / Tween 调度器（脚本侧）

- **现状**：无。但本质上**不需要 C++ 绑定**。
- **收益 / 必要性**：**低-中 / 非必须**。脚本便利——延时回调、缓动动画。
- **建议方案**：**纯 Lua 标准库**，不动引擎 / 绑定：
  - 在 `GameScripts` 下提供 `dse_timer.lua` / `dse_tween.lua`，挂在已暴露的 `on_update(dt)` 钩子上消费 `dt`（理想情况下消费 `scaled_dt`，见 #1，这样"暂停时计时器也暂停"）。
  - API 示例：
    ```lua
    local timer = require("dse.timer")
    timer.after(2.0, function() print("2s later") end)
    timer.every(0.5, on_tick)

    local tween = require("dse.tween")
    tween.to(obj, 1.0, { x = 10 }, "ease_out_quad")
    ```
- **实现要点**：纯 Lua；与引擎唯一接触点是从 #1 取得 `scaled / unscaled dt`。
- **工作量 / 风险**：**极低**。无 C++ 改动。
- **排期**：随时。建议作为示例脚本 / 标准库随 SDK 附带；若要支持"暂停联动"则等 #1。

---

## 4.（附）BoundingBoxComponent 只读 getter

- **现状**：`BoundingBoxComponent`（引擎计算的世界 AABB，供剔除 / LOD 使用）未绑定。
- **收益 / 必要性**：**低 / 非必须**。少数场景：脚本做包围盒查询、自定义剔除、放置吸附。
- **建议 API**：
  ```lua
  dse.ecs.get_world_aabb(e) -> min_x,min_y,min_z, max_x,max_y,max_z   -- 无则 nil
  ```
- **实现要点**：只读；实体无 `BoundingBoxComponent` 时返回 `nil`。注意该组件由渲染 / 剔除系统更新，脚本读到的是**上一帧**结果——文档需注明这一时序语义。
- **工作量 / 风险**：**低**。
- **排期**：有具体需求再做。

---

## 优先级与排期汇总

| 项 | 收益 | 必须 | 工作量 | 依赖 | 建议时机 |
|---|---|---|---|---|---|
| 1 time-scale | 中高 | 否 | 中 | — | alpha 后首选 |
| 2 debug-draw | 中（开发期） | 否 | 中 | — | alpha 后 |
| 3 timer/tween | 低-中 | 否 | 极低（纯 Lua） | #1 | 随时（示例库） |
| 4 AABB getter | 低 | 否 | 低 | — | 按需 |

---

> 本清单由一次 Lua 绑定完整性核查产出，详见 `../api/LUA_API.md`。所有项均非发版阻塞，列此仅为留痕与后续排期。
