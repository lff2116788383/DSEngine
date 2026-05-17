# DSEngine 全面分析报告

> 生成日期：2026-05-17
> 来源：与 AI 的一系列深度对话
> 涵盖：功能评估、竞品对比、架构分析、市场定位、内容策略

---

## 一、项目概览

| 项目 | 数据 |
|:-----|:------|
| 项目名称 | DSEngine（dse_engine） |
| 首次提交 | 2026-03-13 |
| 总提交数 | 263 次 |
| 核心代码量 | ~74,710 行（engine/ 下 .cpp/.h/.hpp） |
| C++ 标准 | C++20 |
| 构建系统 | CMake + Visual Studio 17 2022 |
| 目标平台 | 仅 Windows |
| 渲染后端 | OpenGL 4.5（主）+ Vulkan（可选）+ D3D11（可选） |
| 脚本语言 | Lua 5.4（sol2） |
| 单元测试 | 720+（GoogleTest） |

---

## 二、功能完整度评估

### 2.1 渲染管线 —— 最强子系统

| 模块 | 完成度 | 说明 |
|:-----|:------:|:------|
| DAG RenderGraph | 95% | 31 个 Pass，自动拓扑排序 + 无用 Pass 剔除 |
| PBR Cook-Torrance GGX | 95% | 金属粗糙度工作流 + IBL Split-Sum |
| 延迟渲染 + 前向渲染 | 95% | 双模式支持 |
| Clustered Forward+ | 90% | 256+ 光源支持 |
| CSM 级联阴影 | 95% | 3 级级联 + 混合 |
| PCSS 软阴影 | 90% | 百分比软阴影 |
| TAA | 90% | 时间抗锯齿 |
| SSR | 90% | 屏幕空间反射 |
| DDGI | 85% | 动态漫反射全局光照（一次反弹） |
| Bloom / DOF / Motion Blur | 95% | 完整后处理链 |
| SSAO / Contact Shadow | 90% | 环境光遮蔽 |
| Hi-Z Cull + GPU Cull | 80% | 遮挡剔除 |
| WBOIT 透明 | 85% | 顺序无关半透明 |

### 2.2 风格化渲染（NPR）—— 最大差异化卖点

| 风格 | 完成度 | 代码量 |
|:-----|:------:|:------:|
| KF / Half-Lambert（半兰伯特） | 95% | 19 行 DSSL |
| Toon Basic（基础卡通） | 90% | 分段漫反射 + 离散高光 |
| Toon Rim（边缘光） | 90% | 4 次方 rim falloff |
| Toon Metal（金属卡通） | 85% | 可控高光形状 |
| Toon Cel（硬边卡通） | 90% | 30 行 DSSL |
| Watercolor（水彩） | 85% | 纸张纹理 + 边缘扩散 |
| Watercolor Foliage（水彩植物） | 80% | 植物专用 |
| Hatching（版画/素描） | 80% | 交叉线影 |
| Gradient Ramp（渐变色带） | 85% | 冷暖色带过渡 |
| Minnaert（月球散射） | 80% | 表面粗糙感 |

**独有特色**：Watercolor 和 Hatching 风格在 UE5/Unity/Godot 中均无开箱即用的实现。

### 2.3 ECS 架构

| 组件 | 状态 |
|:-----|:------|
| 核心 ECS（EnTT） | ✅ 完整 |
| 多 World 并行 | ✅ |
| 2D 组件体系 | ✅ Sprite/Tilemap/Spine/UI/Camera |
| 3D 组件体系 | ✅ Mesh/Animation/Particle/Light/Cloth/Hair/GI Probe/Decal |
| Lua 脚本组件 | ✅ |

### 2.4 运行时系统

| 系统 | 状态 |
|:-----|:------|
| 帧流水线（FramePipeline） | ✅ |
| 更新图（Update Graph） | ✅ |
| 固定时间步 | ✅ |
| 事件总线（EventBus） | ✅ |
| 任务系统（JobSystem） | ✅ |
| 场景管理 | ✅ 序列化/保存/加载 |
| Profiler（CPU） | ✅ |

### 2.5 物理系统

| 系统 | 状态 |
|:-----|:------|
| PhysX 3D 物理 | ✅ 刚体/碰撞体/角色控制器/射线/布娃娃/关节 |
| Box2D 2D 物理 | ✅ |
| 碰撞事件回调 | ✅ |
| 破碎/软体/绳索/浮力 | 🟡 实验级 |

### 2.6 资产管线

| 功能 | 状态 |
|:-----|:------|
| AssetBuilder（FBX→.dmesh） | ✅ |
| dpak 打包（StreamingManager） | ✅ |
| 资源流式加载 | ✅ |
| 纹理加载（stb_image） | ✅ |
| 独立 exe 导出（Build Game...） | ✅ |
| 启动器（Tauri launcher） | ✅ |

### 2.7 编辑器

| 面板 | 状态 |
|:-----|:------|
| Hierarchy | 🟡 有但未充分测试 |
| Inspector（20+ 组件类型） | 🟡 有但可能有 bug |
| Viewport + Gizmo | 🟡 有但操作未打磨 |
| Console | ✅ |
| Lua REPL | ✅ |
| Prefab 系统 | 🟡 有 |
| 材质面板 | 🟡 有但不完整 |
| 粒子编辑器 | 🟡 有 |
| 动画时间线 | 🟡 有 |
| 地形编辑器 | 🟡 有 |
| Tilemap 编辑器 | 🟡 有 |
| Undo/Redo | ❓ 未知 |
| Game 视图（运行中编辑） | ❌ 无 |
| 崩溃恢复 | ❌ 无 |
| **综合成熟度** | **~10-15%** |

---

## 三、架构质量评估

### 3.1 代码分布

| 子系统 | 文件数 | 行数 | 占比 |
|:-------|:-----:|:----:|:----:|
| render/（渲染全栈） | 101 | 48,111 | 64.4% |
| scripting/（脚本） | 28 | 7,920 | 10.6% |
| assets/（资产） | 16 | 4,339 | 5.8% |
| scene/（场景） | 12 | 2,787 | 3.7% |
| runtime/（运行时） | 15 | 2,487 | 3.3% |
| ecs/（组件） | 21 | 2,306 | 3.1% |
| physics/ | 4 | 1,807 | 2.4% |
| core/ | 11 | 1,415 | 1.9% |
| 其他 | 27 | 3,414 | 4.6% |
| **总计** | **~230** | **74,710** | **100%** |

### 3.2 最大文件排行

| 排名 | 文件 | 行数 | 说明 |
|:---:|:-----|:---:|:------|
| 1 | gl_draw_executor.cpp | 2,737 | OpenGL 绘制执行 |
| 2 | builtin_passes.cpp | 2,307 | 31 个渲染 Pass |
| 3 | vulkan_draw_executor.cpp | 2,132 | Vulkan 绘制 |
| 4 | dx11_shader_sources.h | 2,084 | D3D11 shader 源码 |
| 5 | lua_binding_ecs_rendering.cpp | 1,903 | Lua ECS 渲染绑定 |
| 6 | vulkan_shader_sources.h | 1,811 | Vulkan shader 源码 |

### 3.3 架构评分

| 维度 | 评分 | 说明 |
|:-----|:---:|:------|
| 目录分层 | ★★★★☆ | 三层结构清晰 |
| 模块职责 | ★★★★☆ | 职能单一 |
| 抽象接口 | ★★★★★ | RHI / RenderGraph / IRenderPass / ServiceLocator |
| 依赖方向 | ★★☆☆☆ | **engine 违规依赖 modules** |
| 头文件耦合 | ★★☆☆☆ | frame_pipeline.h 引入 13 个 modules/ 头文件 |
| 循环依赖 | ★★★★☆ | 基本无 |
| 全局状态 | ★★☆☆☆ | Screen/Input/Debug 为全局单例 |
| **总体** | **★★★☆☆** | 好的架构理念，但有历史技术债务 |

### 3.4 主要架构问题

1. **engine 依赖 modules**：
   - `engine/runtime/frame_pipeline.h` 直接 include 10+ 个 `modules/` 头文件
   - `engine/ecs/components_3d.h` 依赖 `modules/gameplay_3d/animation/`
   - 违反依赖方向规则 `apps → modules → engine → depends`

2. **RhiDevice 伪抽象**：`RhiDevice` 类直接包含 OpenGL 子系统成员，不是纯虚基类

3. **全局单例**：Screen / Input / Debug 不通过 ServiceLocator 注入

4. **大文件**：top5 文件均在 1700-2700 行，应拆分

---

## 四、功能完成度评分（诚实版）

| 定位 | 完成度 |
|:-----|:------:|
| 作为**游戏引擎产品**（拿来用） | **~15%** |
| 作为**渲染引擎学习项目**（拿来读） | **~80%** |
| 作为**B 站视频素材**（拿来看） | **~90%** |

### 各子系统产品级就绪度

| 子系统 | 就绪度 | 关键短板 |
|:-------|:-----:|:---------|
| PBR 渲染管线 | 60% | 缺少移动端适配、能量补偿项 |
| NPR 风格化 | 40% | 缺少完整游戏集成、参数化不够 |
| 编辑器 | 10% | 未打磨、未测试、无用户验证 |
| 物理引擎 | 15% | 未在真实游戏中验证 |
| Android 跨平台 | 0% | 根本无法运行 |
| SDK 发布 | 0% | 从未发布过、从未被外部用户使用 |
| 游戏产出 | 0% | KF demo 能跑但不代表引擎稳定 |

---

## 五、已知架构技术债务修复优先级

| 优先级 | 问题 | 影响 | 预估工作量 |
|:-----:|:-----|:----|:---------|
| 🚨 P0 | frame_pipeline.h 依赖 modules/ | 编译耦合 + 依赖违规 | 2-3 天 |
| 🚨 P0 | components_3d.h 依赖 modules/ | 编译耦合 + 依赖违规 | 1-2 天 |
| ⚠️ P1 | gl_draw_executor.cpp 2737 行 | 可维护性 | 2-3 天拆分 |
| ⚠️ P1 | RhiDevice 解耦 OpenGL | 架构纯净度 | 3-5 天 |
| 🟢 P2 | Screen/Input/Debug 改 ServiceLocator | 可测试性 | 2-3 天 |

---

## 六、开发效率评估

| 指标 | 数据 |
|:-----|:------|
| 项目历时 | 2 个月零 4 天（2026-03-13 → 2026-05-17） |
| 总提交数 | 263 |
| 日均提交 | ~4 次 |
| 核心代码量 | ~7.5 万行 |
| 日均产出 | ~1,150 行代码 |
| 测试覆盖 | 720+ 用例 |

**结论**：开发效率极高。在国内自研引擎项目中，2 个月达到此完成度的项目极为罕见。
