# DSEngine 新引擎演进步骤方案（2026版）
> 综合 `ModernFullGameEngineArchitecture.md`、`Architecture.md`、`AI_Phased_Architecture.md`、`落地推进方案.md`  
> 目标：让 AI 可按本文档快速、稳定、持续推进项目向商业引擎落地。

---

## 1. 文档定位与总目标

本方案是 DSEngine 的统一执行蓝图，解决“架构目标一致、执行路径分散”的问题。  
核心目标：

- 以 **Phase1（2D）** 为可商用稳定基座完成收口
- 以 **Phase2（3D）** 为横向扩展，不破坏既有 2D 路径
- 以 **SDK 化 + 工程化 + 自动化回归** 为商业落地标准
- 以 **AI 可执行任务模板** 保证持续迭代质量与速度

---

## 2. 统一架构原则（融合四份文档）

### 2.1 分层不变原则（六层模型）
- Logic / Scripting
- Scene / ECS World
- Subsystems（Render/Physics/Audio/UI/Animation）
- Asset System
- Core（Math/Thread/IO/Event/Memory）
- PAL（Platform Abstraction Layer）

### 2.2 Phase 驱动原则
- **Phase1**：只交付高性能 2D 与底层 3D-ready 基建（ECS/RHI/Mat4/多线程）
- **Phase2**：只做新增 3D 组件、3D 系统、3D 资源与 3D 渲染管线
- 严禁“阶段越界开发”导致返工

### 2.3 双轨兼容原则
- 一套底座，2D/3D 可共存
- 2D-only 可裁剪，3D 模块可插拔
- C++ 业务与 Lua 业务均以宿主形式接入引擎 SDK

### 2.4 工程化门禁原则
每个任务必须具备：
- 可验证输出
- 自动化回归
- 可回退点
- 文档同步更新

---

## 3. 当前阶段一（Phase1）已实现能力与优化清单

### 3.1 架构与运行形态
- 引擎已 SDK 化：`src` 产出动态库，`examples` 产出业务宿主
- 双宿主可执行：
  - `DSEngine_c++_debug.exe`
  - `DSEngine_lua_debug.exe`
- 引擎库按配置后缀输出（Debug/Release 等）

### 3.2 业务模式与脚本链路
- C++/Lua 业务模式已解耦，统一复用同一运行循环
- Lua 启动入口由宿主指定，避免引擎硬编码业务目录
- `script/` 定位为引擎脚本接口层，业务脚本位于 `examples/lua`

### 3.3 渲染与性能路径
- Phase1 主线渲染路径已收敛，旧渲染桥接历史路径移除
- RHI 扩展点已具备（RenderPass/PipelineState/RenderTarget）
- 批处理能力与统计闭环已具备（DrawCall/Batch/SpriteCount）

### 3.4 资源与场景工程化
- 资源线程安全治理已完成（缓存与异步回调主线程泵送）
- 场景序列化/反序列化闭环已具备
- schema 迁移工具、迁移报告、趋势与阈值告警已接入发布流程

### 3.5 编辑器联动
- 编辑器与运行时世界联动链路已打通
- 共享内存帧桥接替代低效轮询
- 材质实例、参数编辑、热更新回放与回归链路已闭环

---

## 4. 阶段一后续实现与优化（Phase1 收口为商业可用）

## 4.1 P0（必须先做）
### P0-1 生命周期统一
- 统一 Scene World 与 Runtime World 生命周期
- 消除双状态漂移与隐式兜底路径

### P0-2 去单例化收口
- AssetManager/Runtime 上下文支持注入
- 可测试、可多实例、可并行场景运行

### P0-3 资源生命周期治理
- 建立 GPU 资源台账（Program/Texture/FBO/Buffer）
- 建立创建-引用-释放全链路检查

### P0-4 SDK 发布标准化
- install/export（头文件+lib+dll+脚本接口）
- 明确最小接入模板（C++/Lua 各一套）

## 4.2 P1（高价值增强）
### P1-1 Prefab 最小商用闭环升级
- Prefab 资源化、实例化、覆盖参数、版本迁移

### P1-2 层级系统强化
- Parent/Children 关系与拓扑更新稳定化
- 序列化兼容与编辑器回写一致性

### P1-3 ScriptComponent 工程化
- OnAwake/OnUpdate/OnDestroy 稳定生命周期
- 脚本错误隔离、热重载状态迁移

### P1-4 资源异步预算化
- 主线程上传预算、队列背压、统计观测

## 4.3 P2（商业化配套）
### P2-1 打包与发布链路
- 资源打包、增量更新、版本签名
### P2-2 质量体系
- 回归矩阵（场景、材质、脚本、迁移、性能）
- 长期趋势看板（崩溃率、加载时延、帧稳定）

## 4.4 当前完成标记（2026-03-22）
> 说明：以下为“阶段一后续实现与优化”的第一步落地完成项，不代表阶段一后续全部完成。
- [x] P0-1 生命周期统一第一步：`scene::Scene` 支持外部 `Phase1World` 绑定，宿主可统一 Scene 与 Runtime World 生命周期
- [x] P0-2 去单例化收口第一步：`EngineRunConfig` 支持注入 `world/asset_manager`，`FramePipeline/LuaRuntime` 支持使用注入 AssetManager 上下文
- [x] P0-3 资源生命周期治理第一步：`OpenGLRhiDevice` 新增 GPU 资源台账（创建/释放统计 + 泄漏疑似告警）
- [x] P0-4 SDK 发布标准化第一步：CMake 支持 install/export（动态库/宿主可执行/头文件/script Lua 接口）
- [x] P1-1 Prefab 升级第一步：新增 `prefab_schema_version` 与实例化参数覆盖入口（位置/旋转/缩放）
- [x] P1-2 层级系统强化：Parent 关系传播与序列化链路已具备
- [x] P1-3 ScriptComponent 工程化第一步：实体级 `OnAwake/OnUpdate/OnDestroy` 已接入，支持热重载状态迁移（`OnSerializeState/OnDeserializeState`）
- [x] P1-4 资源异步预算化第一步：主线程上传预算、队列观测、高水位统计与拥塞告警已接入运行时
- [x] P2-1/P2-2 商业化发布链路与质量看板增强第一步：`editor/scripts/build_pipeline.js` 已集成发布包清单（SHA256）与质量看板（迁移健康度 + 材质回放回归 + 严格阈值门禁）

## 4.5 未完成清单（按优先级）
### P0（最高优先级）
- [ ] P0-1 生命周期统一第二步：SceneManager 与运行时入口统一使用可注入 World 上下文，移除残余隐式构造路径
- [ ] P0-2 去单例化收口第二步：AssetManager/LuaRuntime 提供显式实例生命周期管理，减少 `Instance()` 兜底调用
- [ ] P0-3 资源生命周期治理第二步：资源台账接入自动化回归（启动/退出资源差分必须为 0）
- [ ] P0-4 SDK 发布标准化第二步：补齐 CMakePackageConfig 与版本文件，支持外部 `find_package(DSEngine)`

### P1（高优先级）
- [ ] P1-1 Prefab 升级第二步：支持深层组件参数覆盖、批量实例化参数集与 prefab schema 迁移工具
- [ ] P1-2 层级系统强化第二步：补齐循环引用检测、层级脏标记最小化传播与编辑器批量操作一致性
- [ ] P1-3 ScriptComponent 工程化第二步：热重载失败回滚、脚本状态版本兼容与崩溃隔离策略
- [ ] P1-4 资源异步预算化第二步：引入动态预算调节（按帧时长/队列压力）与可视化预算面板

### P2（中优先级，Phase1 商业化配套收口）
- [ ] P2-1 打包与发布链路第二步：支持增量包、签名与渠道化输出（dev/staging/release）
- [ ] P2-2 质量体系第二步：统一质量基线（构建、迁移、回放、性能）并生成长期趋势看板

---

## 5. 阶段二后续实现与优化（Phase2：3D/全栈扩展）

## 5.1 Phase2 进入条件（Gate）
以下全部满足后才进入 3D 主开发：
- Phase1 P0 全部完成
- SDK 发布路径可复用
- 2D 回归通过率稳定
- RHI 通用绘制接口稳定

## 5.2 Phase2-M1：3D 最小可运行闭环
- 新增组件：
  - `MeshRendererComponent`
  - `LightComponent`
  - `Camera3D`（透视）
  - `RigidBody3D/Collider3D`
- 新增系统：
  - `MeshRenderSystem`
  - `Physics3DSystem`
- 验收：
  - 可加载并渲染基础 3D 场景
  - 2D-only 构建不受影响

## 5.3 Phase2-M2：3D 资源与材质闭环
- GLTF/FBX 导入（建议先 GLTF）
- PBR 材质参数与贴图链路
- 模型/纹理异步与流式加载
- 验收：
  - 模型+材质+灯光稳定渲染
  - 资源引用计数与释放正确

## 5.4 Phase2-M3：渲染图升级
- Forward+/Deferred 管线（按开关）
- 阴影（CSM/点光阴影）与后处理（Bloom/ToneMapping）
- 2D UI/HUD 叠加 3D 画面
- 验收：
  - 3D 主场景 + 2D UI 混合显示稳定
  - 帧统计可观测

## 5.5 Phase2-M4：编辑器 3D 化
- 3D Gizmo、模型与灯光面板
- 3D 场景调试与剔除可视化
- 验收：
  - 编辑器完整编辑 3D 资源与场景并运行验证

---

## 6. AI 直接执行协议（强约束）

## 6.1 任务输入模板
- 任务编号（如 P0-1 / M2-3）
- 目标
- 约束
- 验收标准

## 6.2 任务输出模板
- 变更文件列表
- 关键实现点（<=10条）
- 验证命令与结果
- 风险与回退点
- 文档更新点

## 6.3 强制验证
- 构建：
  - `cmake --build build_vs2022 --config Debug --target DSEngine dse_example_lua`
- 诊断：
  - 语言诊断必须为 0
- 回归：
  - 场景 round-trip
  - schema 迁移
  - 材质回放（如适用）

## 6.4 禁止事项
- 禁止阶段越界（Phase1 写 3D 业务逻辑）
- 禁止绕过 RHI 直接写业务 `gl*`
- 禁止引擎层硬编码业务目录

---

## 7. 推荐执行顺序（未来 8~12 周节奏）

- 批次A（Phase1 P0）  
  P0-1 生命周期统一 → P0-2 去单例化 → P0-3 资源生命周期 → P0-4 SDK 发布
- 批次B（Phase1 P1）  
  Prefab → 层级系统 → ScriptComponent 工程化 → 异步预算化
- 批次C（Phase2 M1-M2）  
  3D 组件系统最小闭环 → 3D 资源导入与 PBR
- 批次D（Phase2 M3-M4）  
  渲染图升级 → 编辑器 3D 化 → 商业化性能压测

---

## 8. 商业落地定义（完成标记）

满足以下条件视为“可商业交付”：
- SDK 包可被外部业务工程直接接入
- C++ 与 Lua 业务链路长期稳定
- 2D 生产能力完整（场景/资源/脚本/编辑器/回归）
- 3D 最小生产能力可用（模型/材质/灯光/相机/基础后处理）
- 发布链路具备自动迁移、质量门禁、失败阻断与报告可视化

---

## 9. 一句话结论

DSEngine 当前最优路径不是“重写”，而是：  
**以已完成的 Phase1 工程化成果为底座，按本方案完成收口与门禁，再以可裁剪、可回归、可发布的方式横向扩展 Phase2，最终落地为商业化全栈引擎。**

---

## 10. Phase1 现代 2D 引擎完整体最小 MVP 方案（2026-03-23）

### 10.1 MVP 范围与目标
- 在现有 Phase1 主干上形成“可玩、可测、可回归”的现代 2D 引擎最小完整体
- 跑通 Sprite + Physics + UI + Audio + Tilemap + Animation 的统一运行链路
- 保持 C++/Lua 双宿主一致行为，不引入 3D 越界实现

### 10.2 MVP 六个里程碑（M1-M6）

#### M1 运行时主链补齐
- [x] 将 `AnimationSystem` 正式接入 `Phase1FramePipeline::Update`
- [x] 保持既有 `Transform/Camera/UI/Audio/Tilemap` 调度顺序稳定
- [ ] 验收：动画状态机切换与序列帧播放在主循环可见

#### M2 Tilemap 最小可用渲染与碰撞
- [x] `TilemapSystem` 从数据校验升级为可渲染批次生成
- [x] 增加静态碰撞体生成入口（按图层或标记）
- [ ] 验收：中等规模瓦片地图稳定渲染并参与物理碰撞

#### M3 UI 最小控件集
- [x] 在现有 UI 组件体系上补齐 `Button/Panel/Label` 最小控件能力
- [x] 先实现基础文本渲染（位图字体路径），保留后续 MSDF 升级口
- [ ] 验收：按钮交互、文本显示、UI 批渲染在同一帧链路稳定

#### M4 Audio 最小完整体
- [x] 在当前 `AudioSystem` 基础上补齐 `Master/BGM/SFX` 分组音量
- [x] 补齐 `play/pause/stop/loop` 控制与最小事件触发能力
- [ ] 验收：场景事件和 UI 事件可稳定触发音效与背景音乐控制

#### M5 事件总线工程化接入
- [x] 将 `EventBus` 接入 UI 事件、资源加载完成、场景生命周期事件
- [x] 增加订阅生命周期管理（句柄与解绑）
- [ ] 验收：关键跨系统通信不再依赖硬编码直接调用

#### M6 MVP 验收与门禁
- [ ] 固化一个 `examples` 演示关卡作为 MVP 回归样板
- [ ] 扩展现有回归链路，纳入性能与稳定性阈值门禁
- [ ] 验收：构建通过、回归通过、核心统计指标达标

### 10.3 MVP 退出条件（Definition of Done）
- 功能闭环：六个里程碑全部达成
- 质量闭环：场景序列化/反序列化回归与兼容回归持续通过
- 性能闭环：DrawCall、Batch、资源回调队列指标可观测且稳定

---

## 11. Phase1 后续优化方案（商业可用收口）

### 11.1 P0 稳定性与工程化（最高优先级）
- 生命周期统一第二步：SceneManager 与 Runtime 统一可注入 World 上下文
- 去单例化第二步：AssetManager 与事件上下文支持显式实例生命周期
- 资源生命周期治理第二步：GPU 资源台账纳入自动化差分门禁
- SDK 发布标准化第二步：补齐 `CMakePackageConfig` 与版本化 `find_package` 路径

### 11.2 P1 高价值能力增强
- UI 完整化：布局约束、文本排版、九宫格、输入焦点管理
- 动画增强：Tween 调度器、动画事件、参数驱动状态机完善
- Tilemap 优化：分块缓存、可见性裁剪、局部更新
- 空间索引实战化：QuadTree/网格索引接入渲染查询与碰撞查询主链

### 11.3 P2 商业化配套
- 资源 Bundle 管线：打包、增量、签名、压缩/加密
- 热重载完整体：从脚本热更扩展到材质/纹理/图集/配置
- 质量看板：构建、迁移、性能、崩溃率长期趋势化

### 11.4 推荐推进顺序
- 第一批：M1 → M2 → M3 → M4（先打通玩家可感知体验）
- 第二批：M5 → M6（完成解耦与质量门禁）
- 第三批：P0 → P1 → P2（收口为商业可用形态）
