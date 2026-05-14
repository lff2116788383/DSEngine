# DSEngine 短板补全计划

> 生成日期：2026-05-14（第六次迭代更新）
> 方法：基于代码实际审查 + 文档交叉验证 + git 提交历史分析
>
> ⚠️ **本轮重大更新说明：** 自 2026-05-13 首次文档生成以来，引擎经历了**重大功能迭代**。前三次迭代完成了渲染管线全功能补齐（延迟渲染、TAA、Contact Shadow、Light Probe SH Bake、Reflection Probe IBL、DOF、Motion Blur、SSR），第四次迭代完成了动画系统 Phase 1 的完整实现（FABRIK IK、LookAt IK、Animation Layering、2D Blend Tree、Bone Mask），第五次迭代完成了 SSS 可配置 tint 参数化（`extra_params2.yzw` 承载 SSS tint，三端 shader + gen.h 全部同步），**第六次迭代完成了 GPU Instancing 三后端**（GL `glDrawElementsInstanced` / Vulkan `vkCmdDrawIndexed(instanceCount)` / DX11 `DrawIndexedInstanced`，含 DX11 `transpose()` 矩阵修正、半透明排除、零堆分配 `InstancingKey`）。当前引擎短板重心已从「缺什么」转向「怎么更好」，参考 docs/GAP_ANALYSIS_MINIMALIST.md 的「轻量级高性能3D极客引擎」方向。

---

## 一、审查结论：文档与源码一致性

### 本轮迭代已完成的功能（自 2026-05-14 第六次更新起新增）

| 功能 | 提交 | 状态 | 说明 |
|------|------|------|------|
| **SSS 可配置 Tint** | — | ✅ 已实现 | `MeshDrawItem::material_sss_tint`，`extra_params2.yzw` 承载 RGB tint，零向量 fallback 到默认肤色，三端 shader + gen.h 全部同步 |
| **GPU Instancing 三后端** | — | ✅ 已实现 | GL `glDrawElementsInstanced` + Vulkan `vkCmdDrawIndexed(instanceCount)` + DX11 `DrawIndexedInstanced`；`MeshDrawItem::instance_transforms`；`InstancingKey` 零堆分配 struct hash；仅 Opaque 合批；DX11 修复 HLSL `transpose()` 行列序 bug |
| **AnimatorSystem 重构** | `169c51f` | ✅ 已实现 | `Update()` 拆分为 `EvaluateBaseAnim()` + `ComputeFinalMatrices()`，中间姿势通过 pose_buffer 传递 |
| **FABRIK IK 求解器** | `169c51f` | ✅ 已实现 | 多迭代 FABRIK + 极向量约束 + 不可达伸展，支持脚/手臂/任意链 |
| **LookAt IK** | `169c51f` | ✅ 已实现 | 头部/眼球朝向目标，局部空间旋转混合 |
| **Animation Layering** | `169c51f` | ✅ 已实现 | Override + Additive 双模式，支持每层骨骼遮罩（bone mask） |
| **Bone Mask** | `169c51f` | ✅ 已实现 | 名称→索引缓存 + O(log n) 二分查找 + 子骨骼传播 |
| **2D Blend Tree** | `169c51f` | ✅ 已实现 | Shepard 逆距离加权法（非网格模式），支持任意点布局 |
| **Anim Clip Eval 模块** | `169c51f` | ✅ 已实现 | `anim_clip_eval.h` 独立内联模板，跨骨架骨骼重映射 |
| **动画 Lua API 绑定** | `8fa07ff` | ✅ 已实现 | 35+ 个 Lua 函数，覆盖层/IK/事件/FSM/root motion |
| **Lua Demo + 测试** | `8fa07ff` | ✅ 已实现 | 3d_animation_ik_layers.lua Demo + 单元测试 + 性能基准测试 |

**结论：** 上一版文档中列出的动画系统全部 P0/P1 短板（IK、Animation Layering、2D Blend Tree）已**全部完成**。

### 已完成的全部里程碑

- ✅ 🖼️ **延迟渲染管线** — GBuffer + DeferredLightingPass 三后端
- ✅ 🌓 **Contact Shadow** — Screen-space 接触阴影
- ✅ 🎨 **TAA 时间抗锯齿** — Halton 抖动 + 历史帧双缓冲
- ✅ 💡 **Light Probe SH Bake** — 运行时 SH L2 烘焙
- ✅ 🔍 **Reflection Probe + IBL** — Split-Sum IBL + BRDF LUT
- ✅ 🌸 **DOF 景深** — Circle of Confusion
- ✅ 💨 **Motion Blur** — Per-pixel motion vector
- ✅ 🪞 **SSR 屏幕空间反射** — Screen-space ray trace
- ✅ 🎬 **Vignette / Film Grain** — 三后端统一
- ✅ 🎛️ **ACES Filmic Tonemapping**（取代 Reinhard）
- ✅ 🔦 **Clustered Forward+**（256+ 光源）
- ✅ 📊 **PCSS 软阴影 + CSM 级联 smoothstep**
- ✅ 📦 **游戏打包管线** — standalone exe + .dpak 资产打包
- ✅ 🎮 **Lua Console REPL 面板**
- ✅ 🔄 **Lua 热重载**
- ✅ 🧪 **编辑器自动化测试**（12 无头测试）
- ✅ 🎨 **Color Grading LUT**（三后端 3D LUT）
- ✅ 📐 **SSAO / FXAA / Auto Exposure** 三后端
- ✅ 🔧 **全部架构重构项**（单例治理/RHI 拆分/2D 模块化/跨 DLL/UBO/JobSystem/RenderGraph）
- ✅ 🦴 **AnimatorSystem 重构** — `EvaluateBaseAnim` + `ComputeFinalMatrices` 两阶段流水线
- ✅ 🦵 **FABRIK IK 求解器** — 多迭代 + 极向量 + 不可达伸展
- ✅ 👀 **LookAt IK** — 头部朝向目标
- ✅ 🧩 **Animation Layering** — Override / Additive 双模式 + Bone Mask
- ✅ 🌿 **2D Blend Tree** — Shepard 逆距离加权
- ✅ 📜 **动画 Lua API 绑定** — 35+ 函数，覆盖全部功能
- ✅ 🧪 **动画单元测试 / 性能基准测试** — anim_layer_ik_test + anim_perf_benchmark_test
- ✅ 🖥️ **D3D11 后端完整实现** — PBR/阴影/后处理/SSBO 三后端对等
- ✅ 🔍 **物理 Overlap API** — physics_3d_overlap_sphere / physics_3d_overlap_box
- ✅ 🩸 **SSS 可配置 Tint** — `material_sss_tint` + `extra_params2.yzw`，三端 shader 同步，零向量 fallback 默认肤色
- ✅ ⚡ **GPU Instancing** — GL/Vulkan/DX11 三后端，`instance_transforms` 合批，`InstancingKey` 零堆分配，DX11 `transpose()` 修正

---

## 二、当前引擎短板全景图

### 2.1 按严重程度排序

```
严重程度：🔴 P0(缺失) → 🟡 P1(不完整) → 🟢 P2(锦上添花)
```

| 优先级 | 短板 | 范围 | 影响 | 估算工期 |
|--------|------|------|------|---------|
| ~~🔴 P0~~ | ~~**Subsurface Scattering**~~ | ✅ 已完成 | SSS 强度 + 可配置 Tint，三端同步 | — |
| ~~🔴 P0~~ | ~~**通用 Mesh LOD 系统**~~ | ✅ 已完成 | LODGroupComponent + LODSystem，屏幕空间投影，hysteresis 死区，Lua 绑定 | — |
| ~~🔴 P0~~ | ~~**GPU Instancing**~~ | ✅ 已完成 | GL/Vulkan/DX11 三后端，`instance_transforms` 合批，DX11 `transpose()` 修正，零堆分配 key | — |
| 🟡 P1 | **9-Slice UI 缩放** | 2D/UI | UI 控件无法自适应分辨率 | **3 天** |
| 🟡 P1 | **Volumetric Fog / Light Shaft** | Render | 场景氛围最关键因素缺失 | **1-2 周** |
| 🟡 P1 | **Toon / Cel Shading** | Render | 风格化渲染完全空白，DSSL ~20 行可达 | **0.5 天** |
| 🟡 P1 | **Decal System** | Render | 弹孔/路面标记缺失 | **1 周** |
| 🟡 P1 | **Outline / Edge Detection** | Render | 风格化描边缺失，需双 Pass 渲染 | **1 周** |
| 🟡 P1 | **材质系统增强（Clear Coat / Anisotropy / POM）** | Render | Shader 级改进 | **2 天** |
| 🟡 P1 | **2D 多边形碰撞体** | Physics2D | 碰撞精度低，圆形/胶囊外不准 | **2 天** |
| 🟢 P2 | **Linux/macOS 跨平台** | Platform | 仅 Windows | **4-8 周** |
| 🟢 P2 | **网络/多人同步** | Network | 无法联机 | **4-8 周** |
| 🟢 P2 | **Mesh Shader / GPU Driven** | Render | 大规模场景 CPU 瓶颈 | **4-8 周** |
| 🟢 P2 | **D3D12 后端** | Render | 缺 D3D12（有 Vulkan + D3D11） | **2-3 月** |

### 2.2 各领域的完整度（2026-05-14 第六次更新）

```
渲染品质        ████████████████  92%  ✅ 延迟/IBL/TAA/SSR/DOF/MotionBlur/SSS/GPU Instancing 都已实现
动画系统        █████████████░░░  82%  ✅ IK/Layering/2DBlend 已实现
2D/UI           ██████████░░░░░  70%  缺 9Slice/多边形碰撞体
物理系统        █████████████░░  88%  ✅ Overlap API 已实现
音频系统        ████████░░░░░░░  55%  基础播放已有，缺混音总线/DSP
风格化渲染      █░░░░░░░░░░░░░░  ~5%  DSSL 基础设施已有，待 Toon/Outline
跨平台          ██░░░░░░░░░░░░░  15%  仅 Windows
D3D11 后端      ███████████████ 100%  ✅ PBR/阴影/后处理/SSBO/Instancing 完整实现
网络            ░░░░░░░░░░░░░░░   0%  完全缺失

总体           ██████████████░░  85%  ⬆️ GPU Instancing 三后端落地后进一步提升
```

---

## 三、推荐优先级与路线图（2026-05-14 第六次更新）

### 当前状态概览

> **GPU Instancing 三后端已完整落地。** 第六次迭代完成：
> - `MeshDrawItem::instance_transforms`（`std::vector<glm::mat4>`）驱动合批
> - `mesh_render_system.cpp` 按 mesh_path + 全量材质 key 分组，仅 Opaque，`InstancingKey` 零堆分配
> - GL `glVertexAttribDivisor` + `glDrawElementsInstanced`；Vulkan instance VBO + `vkCmdDrawIndexed(instanceCount)`；DX11 `DrawIndexedInstanced` + `transpose()` 修正
> - `RenderStats::instanced_draw_calls` + `instanced_mesh_count` 三后端统计
>
> **当前重点已转向风格化渲染（Toon Shading / Volumetric Fog）和 UI 完善（9-Slice）。**

### Phase 1: 画质细腻度 + 风格化启动（1-2 周）← 当前最高优先级

| 顺序 | 任务 | 工期 | 说明 |
|:----:|------|:----:|------|
| ~~**1**~~ | ~~**SSS（次表面散射）**~~ | ✅ 已完成 | SSS 强度 + 可配置 tint，`extra_params2.yzw`，三端 shader + gen.h 全部同步 |
| **2** | **Toon / Cel Shading DSSL 材质** | **0.5 天** | 阶梯漫反射 + 纯色高光，~20 行 DSSL，不需改引擎（利用 DSSL 自定义 light() 回调） |
| **3** | **Clear Coat + Anisotropy BRDF** | **1 天** | 扩展 PBR BRDF 公式，车漆/头发效果 |
| **4** | **POM（Parallax Occlusion Mapping）** | **1 天** | 砖墙/地面深度感增强 |
| **5** | **Color Banding 后处理** | **0.2 天** | 低成本的风格化加量 |

**Phase 1 总工期：1-2 周**。完成后写实渲染从 ~88% 提升至 ~92%，风格化渲染从 0% 启动到 ~20%。

### Phase 2: 性能优化 + 兼容（2-3 周）

| 顺序 | 任务 | 工期 | 说明 |
|:----:|------|:----:|------|
| ~~**6**~~ | ~~**通用 Mesh LOD 系统**~~ | ✅ 已完成 | 路径驱动 LOD，LODGroupComponent + LODSystem，屏幕空间投影公式，hysteresis 死区，Lua 绑定，9 个单元测试 |
| ~~**7**~~ | ~~**GPU Instancing**~~ | ✅ 已完成 | GL/Vulkan/DX11 三后端，`instance_transforms` 合批，`InstancingKey` 零堆分配，DX11 `transpose()` 修正 |
| **8** | **9-Slice UI 缩放** | **3 天** | UISpriteComponent 新增四边距，拆分为 9 个 quad 绘制 |
| **9** | **SSBO → UBO fallback** | **0.5 天** | 降 GPU 要求至 GL 3.3，兼容旧显卡 |
| **10** | **.dds / BCn 纹理直接上传** | **1 天** | 降 VRAM 30-40% + 加载速度提升 |

### Phase 3: 氛围增强 + 风格化深化（3-5 周）

| 顺序 | 任务 | 工期 | 说明 |
|:----:|------|:----:|------|
| **11** | **Volumetric Fog / Light Shaft** | **1-2 周** | 3D 纹理 raymarching，场景氛围最关键因素 |
| **12** | **Decal System** | **1 周** | Screen-space decal，弹孔/路面标记/血迹 |
| **13** | **Outline / Edge Detection** | **1 周** | 双 Pass 渲染（Backface fatten）+ 后处理 Sobel Edge Detection |
| **14** | **DSSL 风格化材质库** | **3 天** | 多个风格化材质（toon、outline、watercolor_hint），封装为可复用的 DSSL 集合 |
| ~~15~~ | ~~D3D11 后端补齐~~ | ✅ 已完成 | PBR/阴影/后处理/SSBO 三后端对等，无需额外工作 |

---

## 四、当前最推荐优先做的具体任务

### ✅ 第 1 优先（已完成）：SSS（次表面散射）

- SSS 强度 + 可配置 tint（`material_sss_tint`），零向量 fallback 到内置肤色
- `extra_params2.yzw` 承载 tint RGB，三端 shader + gen.h 全部同步

### 第 1 优先（当前）：通用 Mesh LOD 系统（1-2 周）

**原因：**
- 当前仅有地形 LOD，通用网格远距离无降级，VRAM 和 Fillrate 浪费严重
- 实现后大规模场景性能可提升 2-3 倍
- 是「低硬件门槛」目标的基础设施前置条件

### 第 3 优先：Toon Shading DSSL 材质（0.5 天）

**原因：**
- 风格化渲染当前完全空白
- DSSL 材质系统已支持自定义 light() 回调，Toon Shading 不需改引擎核心
- ~20 行 DSSL 即可实现基础的 3-step 阶梯漫反射 + 纯色高光

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
| SSS | `engine/render/shaders/src/pbr.frag` + 三端 shader 源 | ~30 行 |
| Toon Shading | `engine/render/shaders/dssl/toon_default.dssl`（新建） | ~20 行 |
| Clear Coat / Anisotropy | `engine/render/shaders/src/pbr.frag` + 三端 shader | ~30 行 |
| POM | `engine/render/shaders/src/pbr.frag` + 三端 shader | ~40 行 |
| Mesh LOD | `engine/ecs/components_3d.h`, `modules/gameplay_3d/rendering/` | ~500 行 |
| ~~GPU Instancing~~ | ✅ 已完成：`rhi_types.h` + 三后端 draw_executor + `dx11_shader_sources.h` + `mesh_render_system.cpp` | ✅ |
| 9-Slice UI | `modules/gameplay_2d/rendering/sprite_render_system.*` | ~300 行 |
| SSBO → UBO fallback | `engine/render/light_buffer.h` + `cluster_grid.cpp` | ~100 行 |
| Volumetric Fog | `engine/render/passes/` 新建 Pass + shader | ~600 行 |
| Decal System | `engine/render/passes/` 新建 Pass + shader | ~400 行 |
| Outline / Edge Detection | `engine/render/passes/` 修改 + shader | ~300 行 |
| ~~D3D11 后端补齐~~ | `engine/render/rhi/dx11/*` | ✅ 已完成 |

---

## 七、执行顺序建议

```
Session 1 (1天): SSS + Toon Shading DSSL 材质            ← 画质，最高 ROI
Session 2 (1天): Clear Coat + Anisotropy + POM           ← Shader 级增强
Session 3 (0.5天): Color Banding + SSBO→UBO fallback     ← 风格化 + 兼容
──────────────────────────────────────────────── 至此画质细腻度 + 风格化启动
Session 4 (1-2周): Mesh LOD 系统                          ← 性能 ✅ 已完成
Session 5 (1周): GPU Instancing                           ← 性能 ✅ 已完成
Session 6 (3天): 9-Slice UI 缩放                         ← UI  ← 当前下一优先
──────────────────────────────────────────────── 至此达到性能基线
Session 7 (1-2周): Volumetric Fog                         ← 氛围
Session 8 (1周): Decal System                             ← 场景细节
Session 9 (1周): Outline / Edge Detection                 ← 风格化深化
Session 10 (3天): DSSL 风格化材质库                        ← 风格化深化
──────────────────────────────────────────────── 至此引擎进入成熟期
```

---

## 八、一句话总结

> **经过六次迭代，DSEngine 的渲染管线已从"基础 PBR"升级为"完整 PBR + 延迟渲染 + 全局光照探针 + IBL + TAA + 全后处理链 + 动画层/IK/2DBlend + GPU Instancing"。渲染品质达到成熟引擎水准，动画系统从"基础播放"升级为"可用于第三人称游戏"，性能短板（Mesh LOD + GPU Instancing）已全部补齐。**
>
> **当前最缺的不再是渲染功能、动画或性能基础，而是：风格化渲染（Toon Shading/Volumetric Fog/Outline）和 UI 完善（9-Slice）。D3D11 后端已完整实现，三后端对等。引擎战略方向参考 docs/GAP_ANALYSIS_MINIMALIST.md 的「轻量级高性能3D极客引擎」定位。**
