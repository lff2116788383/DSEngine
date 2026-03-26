# DSEngine 阶段 1 2D MVP 缺口清单（按 P0/P1/P2 排序）

> 目的：将当前源码相对《DSEngine_新引擎演进步骤方案_2026版》第一阶段 2D MVP 目标的差距，整理为可直接执行的研发任务列表。
> 范围：仅覆盖阶段 1 的 2D MVP，不包含 2D 商业版、3D MVP、3D 商业版。
> 排序原则：
> - **P0**：不完成则第一阶段 2D MVP 不能算闭环
> - **P1**：影响工具可用性、双宿主一致性和核心体验
> - **P2**：影响稳定性、回归效率、工程质量和后续扩展

---

## 一、阶段目标重述

第一阶段 2D MVP 的完成标准应收敛为以下四条：

1. 跑通 `Sprite + Physics + UI + Audio + Tilemap + Animation` 的统一运行链路
2. 保持 **C++ / Lua 双宿主一致行为**
3. 具备 **Launcher → Editor → Runtime** 的最小闭环
4. 具备“可玩、可测、可回归”的最小工程能力

---

## 二、P0 缺口任务清单

### P0-01 修复 AudioSource 自动播放链路

- **问题描述**
  - 当前音频系统中，`play_on_awake` 创建声音后会被状态逻辑立即停止，导致默认播放链路不稳定或直接失效。
- **目标**
  - 修复 `AudioSourceComponent` 的首次创建、自动播放、循环播放、停止、回收逻辑。
- **涉及模块**
  - `engine/audio/audio_system.cpp`
  - `engine/ecs/components_2d.h`
- **研发内容**
  - 梳理 `play_on_awake` 与 `is_playing` 的状态机
  - 区分“首次创建后自动播放”和“外部要求停止”的控制逻辑
  - 处理非循环音频播放结束后的回收分支
  - 确保循环音频不会被错误停止
- **验收标准**
  - 场景启动后，挂载 `AudioSourceComponent` 的实体可自动播放音频
  - 非循环音频播放完成后可正确释放
  - 循环音频在未主动停止前持续播放
  - 控制台或日志可确认音频状态切换正常

### P0-02 补齐 Lua 综合 MVP 场景中的真实音频验证

- **问题描述**
  - 当前 Lua MVP 示例覆盖了 Sprite、Physics、UI、Tilemap、Animation，但没有真正覆盖 Audio。
- **目标**
  - 在 Lua 综合测试场景中加入真实音频资源与播放逻辑，形成完整阶段 1 验收样例。
- **涉及模块**
  - `samples/lua/phase1_2d_mvp.lua`
  - `samples/lua/main.lua`
  - `engine/scripting/lua/lua_runtime.cpp`
  - `data/` 下新增或补充测试音频资源
- **研发内容**
  - 选定一份轻量测试音频资源
  - 在 Lua 场景初始化阶段增加音频实体
  - 增加启动时自动播放或由 UI/交互触发播放的逻辑
  - 增加日志输出来确认音频路径和播放状态
- **验收标准**
  - 运行 Lua 示例时可以听到真实音频
  - Lua 场景中至少有一处可验证的音频播放行为
  - 音频资源路径在开发目录和运行目录下均可正确解析

### P0-03 建立 C++ / Lua 统一的阶段 1 综合验收场景

- **问题描述**
  - 当前 Lua 示例与 C++ 示例覆盖内容不同，不满足“双宿主一致行为”的阶段目标。
- **目标**
  - 定义同一套阶段 1 综合业务内容，并分别在 Lua 与 C++ 宿主下实现。
- **涉及模块**
  - `samples/lua/phase1_2d_mvp.lua`
  - `samples/cpp/phase1_demo_logic.cpp`
  - `engine/scripting/cpp/cpp_business_runtime.cpp`
  - `engine/runtime/frame_pipeline.cpp`
- **研发内容**
  - 统一验收场景结构
  - 统一基础实体布局和系统覆盖范围
  - 统一关键观测指标：DrawCall、Sprite 数、Physics 体数、Animation 状态、音频状态
  - 对比 Lua/C++ 两侧表现差异并消除不一致
- **验收标准**
  - Lua 与 C++ 两个入口均能运行相同内容的综合场景
  - 两侧视觉、交互、日志统计结果基本一致
  - 不再出现“Lua 是综合样例、C++ 只是压力测试”的分叉

### P0-04 补齐 Lua 全链路接口绑定的最小闭环

- **状态标注（源码核对）**：已有基础实现，需补齐闭环
- **问题描述**
  - 当前 Lua 已有基础绑定，但 UI 交互、动画参数控制、部分音频控制等仍不完整。
- **目标**
  - 让 Lua 具备完成阶段 1 综合回归所需的最低完整接口集合。
- **涉及模块**
  - `engine/scripting/lua/lua_runtime.cpp`
  - `engine/ecs/components_2d.h`
  - `modules/gameplay_2d/ui/ui_system.cpp`
  - `modules/gameplay_2d/animation/animation_system.cpp`
- **研发内容**
  - 补 `UIButton` / `UIPanel` 基础绑定
  - 补 UI 点击事件、悬停事件等基础交互接口
  - 补动画状态切换、参数设置、播放控制接口
  - 视需要补音频播放/停止/循环控制接口
- **验收标准**
  - Lua 可创建可点击 UI，并通过事件驱动业务逻辑
  - Lua 可切换动画状态并触发显示变化
  - Lua 可触发音效播放并能观测播放结果

### P0-05 打通 Launcher 选择版本与选择项目的真实链路

- **问题描述**
  - 当前 Launcher 具备界面，但版本选择没有真正生效，项目路径也没有被 Editor 真实消费。
- **目标**
  - 让 Launcher 的“选择版本 / 选择项目 / 启动 Editor”形成真实可用链路。
- **涉及模块**
  - `apps/launcher/src/launcher_app.tsx`
  - `apps/launcher/main.js`
  - `apps/editor/main.js`
- **研发内容**
  - 让版本选择参数真正影响 Editor 或 Runtime 启动目标
  - 让项目路径被 Editor 启动时读取并用于加载项目
  - 明确开发态与发布态的启动分支
  - 启动日志中打印当前版本和项目路径
- **验收标准**
  - 在 Launcher 中切换版本后，启动行为发生对应变化
  - 在 Launcher 中选择项目后，Editor 能识别并加载目标项目
  - 不再只是把参数挂到环境变量但无人消费

### P0-06 让 Editor 运行真实综合 MVP 场景

- **状态标注（源码核对）**：已有基础实现，需补齐闭环
- **问题描述**
  - 当前 Editor 仍可回退到桥接层内部演示世界，未真正绑定阶段 1 真实运行时场景。
- **目标**
  - 使 Editor 的 Play 行为运行真实引擎场景，而不是桥接层自绘 mock world。
- **涉及模块**
  - `apps/editor/src/bridge/dsengine_bridge.cpp`
  - `apps/editor/main.js`
  - `apps/editor/src/components/EditorApp.tsx`
- **研发内容**
  - 统一 Editor Play 与真实 Runtime 的连接方式
  - 优先使用共享内存帧作为视口来源
  - 限制 bridge fallback 仅用于调试，不参与正式 MVP 验收
  - 确保 Editor 中看到的内容与独立运行时一致
- **验收标准**
  - 点击 Editor 的播放按钮后，运行真实综合 MVP 场景
  - 视口内容与独立启动的运行时一致
  - 默认不再依赖 bridge 内部的示例实体世界

---

## 三、P1 缺口任务清单

### P1-01 统一 UI 布局、渲染、命中的坐标系

- **问题描述**
  - 当前 UI 系统的布局计算与渲染投影假设不一致，容易导致显示位置与点击区域错位。
- **目标**
  - 统一 UI 的布局原点、屏幕坐标换算和输入命中规则。
- **涉及模块**
  - `modules/gameplay_2d/ui/ui_system.cpp`
  - `modules/gameplay_2d/rendering/sprite_render_system.cpp`
- **研发内容**
  - 明确屏幕原点定义
  - 统一布局矩阵与渲染矩阵
  - 统一鼠标输入到 UI 空间的换算逻辑
- **验收标准**
  - UI 显示位置与点击区域一致
  - Button 的 hover / press / click 行为在不同屏幕区域都正确

### P1-02 Scene Tree 支持创建各类 2D 组件实体

- **问题描述**
  - 当前 Editor 中新增实体基本只会创建 `Transform + Sprite`，无法覆盖阶段 1 要求的 2D 组件实体。
- **目标**
  - 支持从 Editor 快速创建 Sprite、Physics、UI、Tilemap、Animation 等类型节点。
- **涉及模块**
  - `apps/editor/src/bridge/dsengine_bridge.cpp`
  - `apps/editor/src/components/EditorApp.tsx`
- **研发内容**
  - 定义新增节点菜单
  - 为不同节点预设不同的组件组合
  - 让 Scene Tree 能正确显示这些节点
- **验收标准**
  - 用户可在 Editor 中直接新建多种阶段 1 2D 节点
  - 创建后 Inspector 能识别其组件类型

### P1-03 Inspector 支持编辑 Transform 基础属性

- **问题描述**
  - 当前 Inspector 基本是只读展示，缺少实际编辑能力。
- **目标**
  - 支持 Position / Rotation / Scale 的基础编辑与回写。
- **涉及模块**
  - `apps/editor/src/components/EditorApp.tsx`
  - `apps/editor/src/bridge/dsengine_bridge.cpp`
- **研发内容**
  - 将只读输入框改为可编辑输入
  - 完成输入值校验、状态同步、回写引擎
  - 统一鼠标拖拽与 Inspector 数值编辑的行为
- **验收标准**
  - 在 Inspector 中修改 Transform 后，视口实时更新
  - 切换选中对象时，Inspector 数值同步正确

### P1-04 Inspector 补齐 Sprite / Physics / UI 基础属性编辑

- **状态标注（源码核对）**：已有基础实现，需补齐闭环
- **问题描述**
  - 阶段 1 文档要求 Inspector 至少可修改 Sprite、Physics、UI 等基础属性，当前尚未达到。
- **目标**
  - 为阶段 1 常用组件提供基础可编辑面板。
- **涉及模块**
  - `apps/editor/src/components/EditorApp.tsx`
  - `apps/editor/src/bridge/dsengine_bridge.cpp`
  - `engine/ecs/components_2d.h`
- **研发内容**
  - Sprite：纹理、排序、颜色、材质指派
  - Physics：类型、重力、摩擦、弹性、碰撞体大小
  - UI：颜色、尺寸、锚点、交互开关
- **验收标准**
  - 修改属性后在视口中可立刻看到效果变化
  - 修改后的值能回写并在重新读取后保持一致

### P1-05 将 Picking 升级为真实 2D 命中逻辑

- **问题描述**
  - 当前 Picking 主要基于位置距离估算，命中结果不稳定且不精确。
- **目标**
  - 将拾取升级为基于 Sprite/UI/Physics 的真实 2D 命中。
- **涉及模块**
  - `apps/editor/src/bridge/dsengine_bridge.cpp`
  - `engine/physics/physics2d/physics2d_system.cpp`
- **研发内容**
  - 为 Sprite 添加矩形边界命中
  - 为 UI 使用实际 UI rect 命中
  - 为物理对象视情况接入 Box2D Raycast
  - 处理重叠对象的排序优先级
- **验收标准**
  - 点击屏幕中可见对象时，选中结果符合预期
  - 重叠对象按排序规则命中
  - 空白区域不会误选

### P1-06 将 FileSystem 与场景实例化从占位功能改为真实功能

- **问题描述**
  - 当前 FileSystem 展示存在硬编码占位项，`+ Instance` 也是日志占位。
- **目标**
  - 让 Editor 能展示真实项目资源，并支持基础场景加载/实例化。
- **涉及模块**
  - `apps/editor/src/components/EditorApp.tsx`
  - `apps/editor/main.js`
- **研发内容**
  - 从实际项目目录读取资源树
  - 展示真实场景文件与脚本文件
  - 实现最小化场景实例化流程
- **验收标准**
  - FileSystem 内容来自真实本地项目
  - 双击场景可触发加载
  - `+ Instance` 不再只是打印日志

### P1-07 Launcher 改为稳定交付路径，而不是开发态 `npm start`

- **问题描述**
  - 当前 Launcher 启动 Editor 更像研发联调，而不是阶段 1 的可交付启动方式。
- **目标**
  - 明确 Launcher 在开发态和发布态下的启动路径，优先支持打包产物启动。
- **涉及模块**
  - `apps/launcher/main.js`
  - `apps/launcher/package.json`
  - `apps/editor/package.json`
- **研发内容**
  - 区分 dev / release 启动策略
  - 优先启动打包后的 Editor 产物
  - 保留开发态联调用法，但不作为正式 MVP 入口
- **验收标准**
  - 没有 Node 开发环境时，Launcher 仍能正常拉起 Editor
  - 开发态与发布态均有清晰可维护的入口

---

## 四、P2 缺口任务清单

### P2-01 补齐 Physics 实体销毁与场景切换清理

- **问题描述**
  - 当前物理系统主要处理创建与同步，实体删除后的 Box2D 资源清理链条不完整。
- **目标**
  - 建立实体销毁、场景清空、运行时退出时的物理资源清理机制。
- **涉及模块**
  - `engine/physics/physics2d/physics2d_system.cpp`
  - `engine/ecs/world.cpp`
- **研发内容**
  - 追踪 `runtime_body` / `runtime_fixture`
  - 在实体删除和 world clear 时释放 Box2D 资源
  - 防止悬挂指针和重复销毁
- **验收标准**
  - 频繁创建/删除物理对象后无崩溃
  - 场景切换后不会残留旧碰撞体

### P2-02 完善资源生命周期管理

- **状态标注（源码核对）**：已有基础实现，需补齐闭环
- **问题描述**
  - 当前资源管理已有加载、缓存、异步回调，但资源释放与严格生命周期管理仍较弱。
- **目标**
  - 提升 Texture / Shader / AudioClip / Material 的生命周期可控性与可观测性。
- **涉及模块**
  - `engine/assets/asset_manager.h`
  - `engine/assets/asset_manager.cpp`
  - `engine/render/rhi/rhi_device.h`
- **研发内容**
  - 明确资源创建与释放边界
  - 增加资源计数或统计输出
  - 排查长时间运行后的缓存膨胀问题
- **验收标准**
  - 多次进出场景后资源数量稳定
  - 无明显泄漏增长趋势

### P2-03 建立阶段 1 综合场景自动回归入口

- **状态标注（源码核对）**：已有基础实现，需补齐闭环
- **问题描述**
  - 当前“可回归”主要依赖手工运行 demo，缺少明确的自动回归入口。
- **目标**
  - 为 Lua 与 C++ 的阶段 1 综合场景提供脚本化回归方式。
- **涉及模块**
  - `samples/lua/`
  - `samples/cpp/`
  - 构建脚本与测试脚本
- **研发内容**
  - 增加统一启动命令
  - 补关键日志断言
  - 产出 PASS / FAIL 输出约定
- **验收标准**
  - Lua / C++ 均可通过脚本方式执行回归
  - 回归结果具备清晰成功失败标记

### P2-04 增加 Editor 冒烟回归

- **状态标注（源码核对）**：已有基础实现，需补齐闭环
- **问题描述**
  - 当前 Editor 核心功能缺少稳定的冒烟验证路径。
- **目标**
  - 为 Editor 的核心能力建立最小回归流程。
- **涉及模块**
  - `apps/editor/scripts/`
  - `apps/editor/src/components/EditorApp.tsx`
  - `apps/editor/src/bridge/dsengine_bridge.cpp`
- **研发内容**
  - 覆盖启动、加载项目、选择实体、改 Transform、Play、材质应用等关键流程
  - 建立可重复执行的 smoke check
- **验收标准**
  - 每次核心修改后都可快速执行 Editor 冒烟验证
  - 至少覆盖 5 个关键交互路径

### P2-05 收敛 bridge 层与真实 Runtime 的职责边界

- **问题描述**
  - 当前 bridge 层仍承担了部分“自建世界、自绘场景”的职责，容易与真实 Runtime 状态分叉。
- **目标**
  - 明确 bridge 只做编辑器通信、帧获取、命令转发，不再长期维护独立演示世界。
- **涉及模块**
  - `apps/editor/src/bridge/dsengine_bridge.cpp`
  - `apps/editor/main.js`
- **研发内容**
  - 梳理哪些逻辑应保留在 bridge
  - 将真实场景状态尽量下沉到 Runtime
  - 降低 Editor 与 Runtime 的双状态问题
- **验收标准**
  - bridge 不再依赖默认演示实体才能工作
  - Editor 状态与 Runtime 状态保持单一事实来源

### P2-06 统一阶段 1 KPI 与日志输出

- **状态标注（源码核对）**：已有基础实现，需补齐闭环
- **问题描述**
  - 当前各入口的统计信息存在但不统一，不利于回归比较和阶段验收。
- **目标**
  - 为 Lua、C++、Editor 三类入口建立统一的阶段 1 指标集合。
- **涉及模块**
  - `engine/runtime/frame_pipeline.cpp`
  - `samples/lua/`
  - `samples/cpp/`
  - `apps/editor/src/components/EditorApp.tsx`
- **研发内容**
  - 统一输出 DrawCall、Batch、Sprite 数、Physics 体数、回调积压、播放状态等指标
  - 统一日志格式和采样节奏
- **验收标准**
  - 三个入口输出可直接横向对比
  - 阶段验收时可依据指标判断是否回退

---

## 五、推荐迭代顺序

### 第一迭代：先打通运行闭环

- P0-01 修复 AudioSource 自动播放链路
- P0-02 补齐 Lua 综合 MVP 场景中的真实音频验证
- P0-03 建立 C++ / Lua 统一的阶段 1 综合验收场景
- P0-04 补齐 Lua 全链路接口绑定的最小闭环

### 第二迭代：打通工具闭环

- P0-05 打通 Launcher 选择版本与选择项目的真实链路
- P0-06 让 Editor 运行真实综合 MVP 场景
- P1-01 统一 UI 布局、渲染、命中的坐标系
- P1-02 Scene Tree 支持创建各类 2D 组件实体
- P1-03 Inspector 支持编辑 Transform 基础属性

### 第三迭代：完善 Editor 可用性

- P1-04 Inspector 补齐 Sprite / Physics / UI 基础属性编辑
- P1-05 将 Picking 升级为真实 2D 命中逻辑
- P1-06 将 FileSystem 与场景实例化从占位功能改为真实功能
- P1-07 Launcher 改为稳定交付路径，而不是开发态 `npm start`

### 第四迭代：增强稳定性与回归能力

- P2-01 补齐 Physics 实体销毁与场景切换清理
- P2-02 完善资源生命周期管理
- P2-03 建立阶段 1 综合场景自动回归入口
- P2-04 增加 Editor 冒烟回归
- P2-05 收敛 bridge 层与真实 Runtime 的职责边界
- P2-06 统一阶段 1 KPI 与日志输出

---

## 六、阶段 1 里程碑完成定义

### M1：运行闭环完成

- Lua / C++ 均跑通 `Sprite + Physics + UI + Audio + Tilemap + Animation`
- 双宿主行为一致
- 有可复现的综合场景

### M2：工具闭环完成

- Launcher 可选项目、选版本并启动 Editor
- Editor 可加载真实项目并运行真实阶段 1 场景
- Scene Tree、Inspector、Picking 满足最小 2D MVP 编辑需求

### M3：回归闭环完成

- Lua / C++ / Editor 均有最小回归入口
- 关键日志和核心指标统一
- 每次改动可做基础回归验证

### M4：稳定闭环完成

- Physics 资源清理可靠
- Asset 生命周期稳定
- bridge 与 Runtime 职责清晰

---

## 七、建议作为研发管理系统中的任务字段

后续若要拆进 Jira / 禅道，建议每条任务统一补充以下字段：

- 任务编号
- 优先级
- 目标说明
- 改动文件
- 技术风险
- 联调依赖
- 验收步骤
- 回归范围
- 输出物

---

## 八、结论

当前 DSEngine 第一阶段 2D MVP 的主要问题不是“完全没实现”，而是：

- 核心系统已有骨架，但 **闭环尚未完成**
- Lua / C++ / Launcher / Editor 四条链路之间 **未真正统一**
- Editor 与 Launcher 仍偏演示态，尚未达到“最小可交付工具链”标准

因此，阶段 1 最合理的推进方式不是继续横向扩功能，而是先按照本清单完成：

1. **运行闭环**
2. **双宿主一致**
3. **工具链闭环**
4. **回归闭环**

完成以上四步后，才算真正具备进入“第一阶段 2D 商业版本”的基础。
