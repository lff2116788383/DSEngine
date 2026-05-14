# DSEngine 短板补全计划

> 生成日期：2026-05-14（第三次迭代更新）
> 方法：基于代码实际审查 + 文档交叉验证 + git 提交历史分析
>
> ⚠️ **本轮重大更新说明：** 自 2026-05-13 首次文档生成以来，引擎经历了**重大功能迭代**，完成了延迟渲染管线、TAA、Contact Shadow、Light Probe SH Bake、Reflection Probe IBL、DOF、Motion Blur、SSR 等关键渲染功能的三后端实现。本文档已针对最新代码状态全面更新。

---

## 一、审查结论：文档与源码一致性

### 本轮迭代已完成的功能（自 2026-05-13 起新增）

| 功能 | 提交 | 状态 | 说明 |
|------|------|------|------|
| **Contact Shadow（接触阴影）** | `65a420e` | ✅ 三后端完整实现 | 原 P0 短板，Screen-space ray march，现已集成到渲染管线 |
| **Vignette / Film Grain** | `f7bf59a` | ✅ 三后端统一接入 | 原 GL 路径扩展为三后端完整实现，合成参数统一 |
| **Light Probe SH Bake** | `52f9005` | ✅ 三后端完整实现 | 运行时 cubemap 渲染→CPU 回读→SH L2 积分→最近 probe 查询 |
| **Reflection Probe + IBL** | `52f9005` | ✅ 三后端完整实现 | Split-Sum IBL 预滤波 + BRDF LUT + 最近 probe cubemap 采样 |
| **TAA 时间抗锯齿** | `52f9005` + `a3d3e92` | ✅ 三后端完整实现 | Halton 序列抖动 + 历史帧双缓冲 + resize + motion vector RT |
| **DOF 景深** | `e9372eb` | ✅ 三后端完整实现 | 基于 Circle of Confusion 的景深效果 |
| **Motion Blur 运动模糊** | `e9372eb` | ✅ 三后端完整实现 | 依赖 MotionVectorPass 的 per-pixel 运动矢量 |
| **SSR 屏幕空间反射** | `e9372eb` | ✅ 三后端完整实现 | Screen-space ray tracing + 预滤波结果合成 |
| **延迟渲染管线** | `88535cf` | ✅ 三后端完整实现 | GBuffer（位置/法线/基色/材质）+ DeferredLightingPass |
| **DrawExecutor 共享状态提取** | `147fefd` | ✅ | 三后端后处理参数统一到公共头文件 |

**结论：** 上一版文档中列出的渲染管线关键短板（Contact Shadow、Light Probe SH Bake、Reflection Probe IBL、TAA、Deferred Rendering）已**全部完成**。文档更新速度跟上了开发节奏。

### 已完成的全部里程碑

- ✅ 🖼️ **延迟渲染管线** — GBuffer + DeferredLightingPass 三后端
- ✅ 🌓 **Contact Shadow** — Screen-space 接触阴影
- ✅ � **TAA 时间抗锯齿** — Halton 抖动 + 历史帧双缓冲
- ✅ 💡 **Light Probe SH Bake** — 运行时 SH L2 烘焙
- ✅ 🔍 **Reflection Probe + IBL** — Split-Sum IBL + BRDF LUT
- ✅ 🌸 **DOF 景深** — Circle of Confusion
- ✅ 💨 **Motion Blur** — Per-pixel motion vector
- ✅ 🪞 **SSR 屏幕空间反射** — Screen-space ray trace
- ✅ 🌸 **Vignette / Film Grain** — 三后端统一
- ✅ �📦 游戏打包管线 — standalone exe + .dpak 资产打包
- ✅ 🎮 Lua Console REPL 面板
- ✅ 🔄 Lua 热重载
- ✅ 🧪 编辑器自动化测试（12 无头测试）
- ✅ 🎨 Color Grading LUT（三后端 3D LUT）
- ✅ 📐 SSAO / FXAA / Auto Exposure 三后端
- ✅ 🎛️ ACES Filmic Tonemapping（取代 Reinhard）
- ✅ 🔦 Clustered Forward+（256+ 光源）
- ✅ 📊 PCSS 软阴影 + CSM 级联 smoothstep
- ✅ 🔧 全部架构重构项（单例治理/RHI 拆分/2D 模块化/跨 DLL/UBO/JobSystem/RenderGraph）

---

## 二、当前引擎短板全景图

### 2.1 按严重程度排序

```
严重程度：🔴 P0(缺失) → 🟡 P1(不完整) → 🟢 P2(锦上添花)
```

| 优先级 | 短板 | 范围 | 影响 | 估算工期 |
|--------|------|------|------|---------|
| 🔴 P0 | **IK 反向动力学** | Gameplay3D | 角色脚不贴地，手部不与物体交互 | **2 周** |
| 🔴 P0 | **9-Slice UI 缩放** | 2D/UI | UI 控件无法自适应分辨率 | **3 天** |
| 🟡 P1 | **Animation Layering** | Gameplay3D | 无法"边走边挥手"等上层动作 | **1 周** |
| 🟡 P1 | **Mesh LOD 系统** | Render | 无自动 LOD，远处性能浪费 | **1-2 周** |
| 🟡 P1 | **2D 多边形碰撞体** | Physics2D | 碰撞精度低，圆形/胶囊外不准 | **2 天** |
| 🟡 P1 | **2D 碰撞体编辑** | Editor | 无法编辑碰撞体形状 | **3 天** |
| 🟡 P1 | **2D Blend Tree / Motion Matching** | Animation | 动画融合不够丰富 | **2-4 周** |
| 🟢 P2 | **Linux/macOS 跨平台** | Platform | 仅 Windows | **4-8 周** |
| 🟢 P2 | **网络/多人同步** | Network | 无法联机 | **4-8 周** |
| 🟢 P2 | **Mesh Shader / GPU Driven** | Render | 大规模场景 CPU 瓶颈 | **4-8 周** |
| 🟢 P2 | **物理资源流式加载** | Assets | 大世界内存不足 | **2-4 周** |
| 🟢 P2 | **D3D12 后端** | Render | 缺 D3D12（有 D3D11） | **2-3 月** |

### 2.2 各领域的完整度（2026-05-14 更新）

```
渲染品质        ██████████████░░  88%  ✅ 重大提升！延迟渲染/IBL/TAA/SSR/DOF/MotionBlur 都已实现
动画系统        ████████░░░░░░░  55%  缺 IK/AnimationLayering/MotionMatching
2D/UI           ██████████░░░░░  70%  缺 9Slice/多边形碰撞体/碰撞体编辑
物理系统        ████████████░░░  85%  可用的完整度很高
音频系统        ████████░░░░░░░  55%  基础播放已有，缺 FMOD/Wwise/混音总线
跨平台          ██░░░░░░░░░░░░░  15%  仅 Windows
网络            ░░░░░░░░░░░░░░░   0%  完全缺失

总体           ████████████░░░░  72%  ⬆️ 从 60% 提升至 72%，渲染品质接近成熟
```

---

## 三、推荐优先级与路线图（2026-05-14 更新）

### 当前状态概览

> **渲染管线已基本成熟。** 上一轮迭代完成了渲染关键短板的补齐：
> - 延迟渲染（GBuffer + DeferredLighting）解决多光源性能问题
> - TAA 解决了画面闪烁
> - IBL + Light Probe 提供了环境光照
> - SSR/DOF/MotionBlur 提供了高级后处理
> - Contact Shadow 填补了阴影最后空白
>
> **当前重点应转向角色动画系统补全和 2D/UI 完善。**

### Phase 1: 角色动画系统补全（3-5 周）← 当前最高优先级

| 顺序 | 任务 | 工期 | 说明 |
|:----:|------|:----:|------|
| **1** | **FABRIK IK Solver** | **1 周** | 优先实现 FABRIK（Forward And Backward Reaching IK），比 CCD 更稳定、迭代少。先做脚部 IK，让角色脚贴在起伏地形上 |
| **2** | **IK 骨骼链配置** | **3 天** | 在 `Animator3DComponent` 中添加 IK target 骨骼链配置（脚踝/膝盖/髋，手/肘/肩），Lua API 暴露 IK target 位置 |
| **3** | **Animation Layering** | **1 周** | 在 `Animator3DComponent` 中添加 layer mask（bone mask），支持上层动作叠加。实现"走路同时挥手" |
| **4** | **Look-At IK** | **2 天** | 角色头部/眼球跟踪目标（比 FABRIK 简单得多，单独实现） |

**Phase 1 总工期：3-5 周**。完成后角色动画质量从"基础播放"提升到"可以用于第三人称游戏"。

### Phase 2: UI 与 2D 完善（1-2 周）

| 顺序 | 任务 | 工期 | 说明 |
|:----:|------|:----:|------|
| **5** | **9-Slice UI 缩放** | **2-3 天** | 在 `UISpriteComponent` 或 `UIImageComponent` 新增 `slice_left/right/top/bottom` 四边距，sprite render system 中根据九宫格拆分为 9 个 quad 绘制 |
| **6** | **多边形碰撞体编辑器** | **2-3 天** | 在 `BoxCollider2DComponent` 旁新增 `PolygonCollider2DComponent`（顶点数组），Inspector 增加顶点编辑功能 |

### Phase 3: 性能优化（3-4 周）

| 顺序 | 任务 | 工期 | 说明 |
|:----:|------|:----:|------|
| **7** | **Mesh LOD 自动生成** | **1-2 周** | 基于 `MeshRendererComponent.mesh_path` 在构建时生成 LOD 级别，`BoundingBoxComponent` 距离判据切换。先做简单距离切换，不做渐隐过渡 |
| **8** | **2D Blend Tree** | **1-2 周** | 实现 2D 混合树（Cartesian/Directional），支持前后左右方向融合 |

---

## 四、当前最推荐优先做的具体任务

### 第 1 优先：FABRIK IK（1 周）

**原因：**
- IK 是目前唯一的 P0 级未完成功能（其余 P0 渲染短板已全部补齐）
- 角色脚不贴地直接影响 3D 游戏的基础体验
- 其他引擎都是直接提供 2-Bone IK，DSE 目前完全缺失

### 第 2 优先：9-Slice UI 缩放（3 天）

**原因：**
- UI 自适应是 2D 游戏的基本需求
- 实现简单，收益明显
- 2D 引擎分析文档明确将其列为"严重缺失"

### 第 3 优先：Animation Layering（1 周）

**原因：**
- 没有分层就无法实现"一边走路一边挥手"，表现为严重受限
- 动画系统完整度将从 55% 提升至 70%

---

## 五、不建议近期做的事

| 事情 | 为什么不建议 | 什么时候适合做 |
|------|-------------|---------------|
| **Mega 重构：切换到 GPU Driven 管线** | 这是引擎架构最底层的变革，涉及渲染管线、资源管理、剔除系统全面重写。预计 4-8 周，期间无法推进任何其他功能 | 当现有 CPU 剔除成为场景规模瓶颈时（当前场景远未达到） |
| **从零写网络模块** | 4-8 周的纯投入期，且联机游戏还需要配套的服务器架构、状态同步、防作弊。DSE 目前连一个联机 demo 都没有 | 当需要做一款具体联机游戏时，而非提前泛化实现 |
| **D3D12 / Metal 后端** | D3D11 和 Vulkan 已覆盖 Windows/Android 核心平台。D3D12 和 Metal 的投入（各 2-3 月）在当前阶段没有足够的用户价值 | 当有明确的 Mac/Xbox 发布需求时 |
| **Linux 跨平台** | GLFW 和 OpenGL/Vulkan 后端理论上可编译，但需要处理文件路径、输入法、窗口管理等大量边缘问题。2-4 周的纯兼容性工作 | 当目标用户明确需要 Linux 支持时 |
| **材质编辑器大幅升级** | 当前编辑器已有 PBR 属性编辑 + 纹理槽拖拽 + 预览球 + Shader 选择，对独立游戏开发已足够 | 编辑器已标记为"功能足够，无需继续扩展" |

---

## 六、快速参考：各任务的代码入口

| 任务 | 关键代码文件 | 预计新增代码 |
|------|-------------|:----------:|
| IK (FABRIK) | `modules/gameplay_3d/animation/` 新增 `ik_solver.*` | ~400 行 |
| Animation Layering | `modules/gameplay_3d/animation/animator_system.*` | ~300 行 |
| 9-Slice UI | `modules/gameplay_2d/rendering/sprite_render_system.*`, `modules/gameplay_2d/ui/ui_system.*` | ~300 行 |
| Mesh LOD | `engine/assets/asset_manager.*`, `modules/gameplay_3d/rendering/mesh_render_system.*` | ~500 行 |
| 2D Blend Tree | `modules/gameplay_3d/animation/` | ~400 行 |

---

## 七、执行顺序建议

```
Session 1 (1周): FABRIK IK + 骨骼链配置               ← 角色，当前最高优先级
Session 2 (1周): Animation Layering                    ← 角色
Session 3 (2天): Look-At IK                            ← 角色
──────────────────────────────────────────────── 至此角色系统完善
Session 4 (3天): 9-Slice UI 缩放                        ← UI
Session 5 (3天): 多边形碰撞体 + 编辑器                   ← 2D 物理
──────────────────────────────────────────────── 至此达到"可发布"水准
Session 6 (1-2周): Mesh LOD                            ← 性能
Session 7 (1-2周): 2D Blend Tree                       ← 动画增强
──────────────────────────────────────────────── 至此引擎进入成熟期
```

---

## 八、一句话总结

> **经过本轮迭代，DSEngine 的渲染管线已从"基础 PBR"升级为"完整 PBR + 延迟渲染 + 全局光照探针 + IBL + TAA + 全后处理链"——渲染品质达到成熟引擎水准。当前最缺的不再是渲染功能，而是角色动画（IK + Layering）和 UI 完善（9-Slice）。接下来的开发重心应从渲染转向角色系统。**
