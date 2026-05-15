# DSEngine 短板补全计划

> 生成日期：2026-05-15（第八次迭代更新）
> 方法：基于代码实际审查 + 文档交叉验证 + git 提交历史分析
>
> ⚠️ **本轮重大更新说明：** 自 2026-05-13 首次文档生成以来，引擎经历了**重大功能迭代**。前七次迭代分别完成了延迟渲染/TAA/全局光照/后处理链、动画系统完善、SSS tint 参数化、GPU Instancing 三后端、9-Slice UI 缩放。**第八次迭代完成了风格化渲染 Phase 1 全部收官**：Toon/Cel Shading DSSL 材质 + Clear Coat/Anisotropy/POM 三后端参数管线 + Color Banding IGN 抖动三后端集成，至此 Phase 1「画质细腻度 + 风格化启动」五项任务全部完成。引擎短板重心已从 Phase 1 转移到 Phase 2「性能优化 + 兼容性」与 Phase 3「氛围增强 + 风格化深化」。

---

## 一、审查结论：文档与源码一致性

### 本轮迭代已完成的功能（自 2026-05-15 第八次更新起新增）

| 功能 | 提交 | 状态 | 说明 |
|------|------|------|------|
| **Color Banding + Toon/Cel Shading 三后端集成** | `284f46e` | ✅ 已实现 | IGN 抖动抗色带三后端同步；Toon `shading_mode=4` 三后端完整集成（shadow_color/shadow_threshold/softness/specular/rim 7 参数通过 PerMaterial UBO 传入） |
| **Clear Coat + Anisotropy + POM 参数管线** | `1c2bb0a` | ✅ 已实现 | Clear Coat 强度+粗糙度、各向异性 GGX、16 层 POM 视差遮挡，`extra_params.yzw` + `extra_params2.x` 三端同步；`3d_advanced_pbr_showcase.lua` 演示 |
| **Toon/Cel Shading DSSL 材质** | `e187980` | ✅ 已实现 | `toon_cel.dssl` 表面材质，7 个可配参数，`demo_toon.lua` 演示；生成 `toon_cel.frag/.vert/.meta.json` |
| **9-Slice UI 缩放**（第七次） | `0aff767` | ✅ 已实现 | `UIRendererComponent` 新增 `nine_slice_border`（UV 分量）+ `nine_slice_src_size`（固定角块模式）；`Expand9SliceItems` 展开为最多 9 DrawItem，`reserve(9)` 防扩容；支持弹性面板（`src_size>0`）和均匀缩放（`src_size=0`）双模式；Lua `ui.set_nine_slice(entity, en, l,b,r,t [,sw,sh])`；9 个单元测试 |
| **GPU Instancing 三后端**（第七次） | `7fe2e59` | ✅ 已实现 | GL `glDrawElementsInstanced` + Vulkan `vkCmdDrawIndexed(instanceCount)` + DX11 `DrawIndexedInstanced`；`MeshDrawItem::instance_transforms`；`InstancingKey` 零堆分配 struct hash；仅 Opaque 合批；DX11 修复 HLSL `transpose()` 行列序 bug |
| **通用 Mesh LOD 系统**（第七次） | `e53f6c4` | ✅ 已实现 | LODGroupComponent + LODSystem，屏幕空间投影公式，hysteresis 死区，Lua 绑定，9 个单元测试 |
| **SSS 可配置 Tint**（第七次） | `1324c70` | ✅ 已实现 | `MeshDrawItem::material_sss_tint`，`extra_params2.yzw` 承载 RGB tint，零向量 fallback 到默认肤色，三端 shader + gen.h 全部同步 |
| **AnimatorSystem 重构** | `169c51f` | ✅ 已实现 | `Update()` 拆分为 `EvaluateBaseAnim()` + `ComputeFinalMatrices()`，中间姿势通过 pose_buffer 传递 |
| **FABRIK IK 求解器** | `169c51f` | ✅ 已实现 | 多迭代 FABRIK + 极向量约束 + 不可达伸展，支持脚/手臂/任意链 |
| **LookAt IK** | `169c51f` | ✅ 已实现 | 头部/眼球朝向目标，局部空间旋转混合 |
| **Animation Layering** | `169c51f` | ✅ 已实现 | Override + Additive 双模式，支持每层骨骼遮罩（bone mask） |
| **Bone Mask** | `169c51f` | ✅ 已实现 | 名称→索引缓存 + O(log n) 二分查找 + 子骨骼传播 |
| **2D Blend Tree** | `169c51f` | ✅ 已实现 | Shepard 逆距离加权法（非网格模式），支持任意点布局 |
| **Anim Clip Eval 模块** | `169c51f` | ✅ 已实现 | `anim_clip_eval.h` 独立内联模板，跨骨架骨骼重映射 |
| **动画 Lua API 绑定** | `8fa07ff` | ✅ 已实现 | 35+ 个 Lua 函数，覆盖层/IK/事件/FSM/root motion |
| **Lua Demo + 测试** | `8fa07ff` | ✅ 已实现 | 3d_animation_ik_layers.lua Demo + 单元测试 + 性能基准测试 |

**结论：** 上一版文档中列出的动画系统全部 P0/P1 短板（IK、Animation Layering、2D Blend Tree）已**全部完成**。Phase 1「画质细腻度 + 风格化启动」五项任务（SSS、Toon/Cel、Clear Coat、Anisotropy、POM、Color Banding）也已**全部完成**。

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
- ✅ 🧩 **9-Slice UI 缩放** — 固定角块/弹性面板双模式，`Expand9SliceItems` + Lua 绑定
- ✅ 🎨 **Color Banding IGN 抖动抗色带** — 三后端 GL/Vulkan/DX11 统一，`fragCoord` 伪随机抖动 +-0.5/255
- ✅ 🌈 **Toon/Cel Shading DSSL 材质** — `shading_mode=4`，三后端 7 参数 PerMaterial UBO
- ✅ 🔧 **Clear Coat + Anisotropy BRDF** — `extra_params.yz` 清漆强度/粗糙度 + `extra_params.w` 各向异性 GGX
- ✅ 🏔️ **POM（Parallax Occlusion Mapping）** — `extra_params2.x` height_scale，16 层视差遮挡采样
- ✅ 🖌️ **Phase 1 全部完成** — 画质细腻度 + 风格化启动五项任务全部落地

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
| ~~🟡 P1~~ | ~~**9-Slice UI 缩放**~~ | ✅ 已完成 | 固定角块/弹性面板双模式 | — |
| 🟡 P1 | **Volumetric Fog / Light Shaft** | Render | 场景氛围最关键因素缺失 | **1-2 周** |
| ~~🟡 P1~~ | ~~**Toon / Cel Shading**~~ | ✅ 已完成 | shading_mode=4，三后端同步，DSSL 参数可配 | — |
| 🟡 P1 | **Decal System** | Render | 弹孔/路面标记缺失 | **1 周** |
| 🟡 P1 | **Outline / Edge Detection** | Render | 风格化描边缺失，需双 Pass 渲染 | **1 周** |
| ~~🟡 P1~~ | ~~**材质系统增强（Clear Coat / Anisotropy / POM）**~~ | ✅ 已完成 | 三后端 PBR BRDF 扩展 + POM | — |
| 🟡 P1 | **2D 多边形碰撞体** | Physics2D | 碰撞精度低，圆形/胶囊外不准 | **2 天** |
| 🟢 P2 | **Linux/macOS 跨平台** | Platform | 仅 Windows | **4-8 周** |
| 🟢 P2 | **网络/多人同步** | Network | 无法联机 | **4-8 周** |
| 🟢 P2 | **Mesh Shader / GPU Driven** | Render | 大规模场景 CPU 瓶颈 | **4-8 周** |
| 🟢 P2 | **D3D12 后端** | Render | 缺 D3D12（有 Vulkan + D3D11） | **2-3 月** |

### 2.2 各领域的完整度（2026-05-15 第八次更新）

```
渲染品质        ████████████████  94%  ✅ 延迟/IBL/TAA/SSR/DOF/MotionBlur/SSS/GPU Instancing/Toon/ClearCoat/Anisotropy/POM/ColorBanding 均已实现
动画系统        █████████████░░░  82%  ✅ IK/Layering/2DBlend 已实现
2D/UI           ████████████░░░  80%  ✅ 9-Slice 已实现，缺多边形碰撞体
物理系统        █████████████░░  88%  ✅ Overlap API 已实现
音频系统        ████████░░░░░░░  55%  基础播放已有，缺混音总线/DSP
风格化渲染      ████████░░░░░░░  ~35%  ✅ Toon/Cel + Color Banding 已完成，待 Outline/Edge Detection 深化
跨平台          ██░░░░░░░░░░░░░  15%  仅 Windows
D3D11 后端      ███████████████ 100%  ✅ PBR/阴影/后处理/SSBO/Instancing 完整实现
网络            ░░░░░░░░░░░░░░░   0%  完全缺失

总体           ████████████████  88%  ⬆️ Phase 1 全部完成后进一步提升
```

---

## 三、推荐优先级与路线图（2026-05-15 第八次更新）

### 当前状态概览

> **Phase 1「画质细腻度 + 风格化启动」全部完成。** 第八次迭代完成：
> - Toon/Cel Shading DSSL 材质 + 三后端集成（`shading_mode=4`，7 参数 PerMaterial UBO）
> - Clear Coat + Anisotropy + POM 三后端参数管线（`extra_params.yzw` + `extra_params2.x`）
> - Color Banding IGN 抖动三后端同步（GL/Vulkan/DX11 统一抗色带）
> - `3d_advanced_pbr_showcase.lua` / `demo_toon.lua` 演示脚本
>
> **当前重点已转向 Phase 2「性能优化 + 兼容性」（SSBO→UBO fallback、.dds 纹理）和 Phase 3「氛围增强」（Volumetric Fog、Decal、Outline/Edge Detection）。**

### Phase 1: 画质细腻度 + 风格化启动 🎉 ✅ 全部完成

| 顺序 | 任务 | 工期 | 状态 |
|:----:|------|:----:|:----:|
| **1** | **SSS（次表面散射）** | — | ✅ 已完成 |
| **2** | **Toon / Cel Shading DSSL 材质** | — | ✅ 已完成 |
| **3** | **Clear Coat + Anisotropy BRDF** | — | ✅ 已完成 |
| **4** | **POM（Parallax Occlusion Mapping）** | — | ✅ 已完成 |
| **5** | **Color Banding 后处理** | — | ✅ 已完成 |

### Phase 2: 性能优化 + 兼容（2-3 周）← 当前最高优先级

| 顺序 | 任务 | 工期 | 说明 |
|:----:|------|:----:|------|
| **6** | **SSBO → UBO fallback** | **0.5 天** | 降 GPU 要求至 GL 3.3，兼容旧显卡 |
| **7** | **.dds / BCn 纹理直接上传** | **1 天** | 降 VRAM 30-40% + 加载速度提升 |
| **8** | **Release 构建脚本 + LTCG** | **0.5 天** | 补齐 `--release` 构建流程，`/GL` + `/LTCG` 降二进制 10-20% |
| **9** | **CMake 残留引用清理** | **0.2 天** | 删除 `spscqueue/rttr/timetool` 三个不存在的目录引用 |

### Phase 3: 氛围增强 + 风格化深化（3-5 周）

| 顺序 | 任务 | 工期 | 说明 |
|:----:|------|:----:|------|
| **10** | **Volumetric Fog / Light Shaft** | **1-2 周** | 3D 纹理 raymarching，场景氛围最关键因素 |
| **11** | **Decal System** | **1 周** | Screen-space decal，弹孔/路面标记/血迹 |
| **12** | **Outline / Edge Detection** | **1 周** | 双 Pass 渲染（Backface fatten）+ 后处理 Sobel Edge Detection |
| **13** | **DSSL 风格化材质库** | **3 天** | 多个风格化材质（toon、outline、watercolor_hint），封装为可复用的 DSSL 集合 |

---

## 四、当前最推荐优先做的具体任务

### ✅ 已完成：Phase 1 全部内容

- SSS（次表面散射）强度 + 可配置 tint
- Toon / Cel Shading DSSL 材质 + 三后端集成
- Clear Coat + Anisotropy BRDF 扩展
- POM（Parallax Occlusion Mapping）
- Color Banding IGN 抖动三后端同步

### 第 1 优先（当前）：SSBO → UBO fallback（0.5 天）

**原因：**
- 当前所有后处理和 Cluster Forward+ 依赖 GL 4.3+ SSBO，降 GPU 要求至 GL 3.3 是扩大硬件兼容面最关键一步
- 改动量小（`light_buffer.h` + `cluster_grid.cpp`），~100 行，ROI 极高

### 第 2 优先：.dds / BCn 纹理直接上传（1 天）

**原因：**
- 当前纹理上传走 stb_image 加载 PNG → 逐像素拷贝到 GPU，VRAM 浪费 30-40%
- .dds 直接上传可利用 GPU 硬件解压 BC1-BC7，加载速度提升 10x+
- 对低 VRAM 场景（2GB 显卡）效果显著

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
| ~~SSS~~ | ✅ 已完成 | ✅ |
| ~~Toon Shading~~ | ✅ 已完成 | ✅ |
| ~~Clear Coat / Anisotropy~~ | ✅ 已完成 | ✅ |
| ~~POM~~ | ✅ 已完成 | ✅ |
| ~~Mesh LOD~~ | ✅ 已完成 | ✅ |
| ~~GPU Instancing~~ | ✅ 已完成 | ✅ |
| ~~9-Slice UI 缩放~~ | ✅ 已完成 | ✅ |
| SSBO → UBO fallback | `engine/render/light_buffer.h` + `cluster_grid.cpp` | ~100 行 |
| .dds / BCn 纹理上传 | `engine/render/rhi/texture_loader.h` + 三端 texture.cpp | ~200 行 |
| Volumetric Fog | `engine/render/passes/` 新建 Pass + shader | ~600 行 |
| Decal System | `engine/render/passes/` 新建 Pass + shader | ~400 行 |
| Outline / Edge Detection | `engine/render/passes/` 修改 + shader | ~300 行 |

---

## 七、执行顺序建议

```
Session 1 (0.5天): SSBO → UBO fallback                          ← 兼容，最高 ROI
Session 2 (1天): .dds / BCn 纹理直接上传                         ← 性能，VRAM 优化
Session 3 (0.5天): Release 构建脚本 + LTCG + CMake 清理          ← 构建
──────────────────────────────────────────────── 至此达到性能基线 + 兼容面扩展
Session 4 (1-2周): Volumetric Fog                                ← 氛围
Session 5 (1周): Decal System                                    ← 场景细节
Session 6 (1周): Outline / Edge Detection                        ← 风格化深化
Session 7 (3天): DSSL 风格化材质库                                ← 风格化深化
──────────────────────────────────────────────── 至此引擎进入成熟期
```

---

## 八、一句话总结

> **经过八次迭代，DSEngine 的渲染管线已从"基础 PBR"升级为"完整 PBR + 延迟渲染 + 全局光照探针 + IBL + TAA + 全后处理链 + 动画层/IK/2DBlend + GPU Instancing + 9-Slice UI"，风格化渲染已完成 Toon/Cel Shading + Color Banding，材质系统扩展了 Clear Coat/Anisotropy/POM。**
>
> **Phase 1「画质细腻度 + 风格化启动」全部完成。** Toon Shading 通过 `shading_mode=4` 集成进三后端 PBR shader，7 个参数通过 PerMaterial UBO 传入；Clear Coat/Anisotropy/POM 通过 `extra_params` 扩展，三端同步；Color Banding 使用 IGN 伪随机抖动抗色带。**当前最高优先级：SSBO→UBO fallback（降 GPU 要求至 GL 3.3）和 .dds / BCn 纹理直接上传（降 VRAM 30-40%）。D3D11 后端已完整实现，三后端对等。**
