# DSEngine 短板补全计划

> 生成日期：2026-05-13
> 方法：基于代码实际审查 + 文档交叉验证，所有结论均来自源码确认

---

## 一、审查结论：文档与源码一致性

### 发现的不一致

| 文档 | 声称状态 | 代码实际状态 | 影响 |
|------|---------|-------------|------|
| `NEXT_SESSION_TASKS.md` 「Task 1: Color Grading LUT」 | 列为待完成 | ✅ GL 后端 inline shader 已实现 3D LUT 采样(`gl_draw_executor.cpp`)，`builtin_passes.cpp` 已集成到 composite pass，`PostProcessComponent.color_lut_handle` 已定义 | 文档轻微落后，无需再做 GL 端实现 |
| `NEXT_SESSION_TASKS.md` 「Task 2: Vignette/Film Grain」 | 列为待完成 | ✅ 代码中已包含实现 | 同上 |
| `NEXT_DIRECTION.md` 「P0-B: Lua Console (REPL)」 | 列为待完成 | ✅ `editor_lua_console.h/cpp` 已实现 | 文档轻微落后 |
| `NEXT_DIRECTION.md` 「P0-A: Game Build/Export 管线」 | 列为待完成 | ✅ `apps/standalone/main.cpp` 已存在，`pak_writer/reader` 已实现 | 文档轻微落后 |

**结论**：所有文档中的待完成项在代码中已大部分实现，文档更新速度略慢于代码开发速度。未发现严重的"文档与代码矛盾"问题。

### 已验证的已完成里程碑

- ✅ 📦 游戏打包管线 — standalone exe + .dpak 资产打包
- ✅ 🎮 Lua Console REPL 面板
- ✅ 🔄 Lua 热重载
- ✅ 🧪 编辑器自动化测试（12 无头测试）
- ✅ 🎨 Color Grading LUT（GL inline shader 路径）
- ✅ 🌸 Vignette / Film Grain 后处理
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
| 🔴 P0 | **Contact Shadow（接触阴影）** | Render | CSM 近处细节缺失，物体脚底无阴影 | **3 天** |
| 🔴 P0 | **IK 反向动力学** | Gameplay3D | 角色脚不贴地，手部不与物体交互 | **2 周** |
| 🔴 P0 | **9-Slice UI 缩放** | 2D/UI | UI 控件无法自适应分辨率 | **3 天** |
| 🔴 P0 | **Light Probe SH Bake** | Render | 无烘焙工具，环境光全靠方向光 | **1-2 周** |
| 🔴 P0 | **Reflection Probe + IBL** | Render | 金属表面无反光，PBR 缺间接高光 | **1-2 周** |
| 🟡 P1 | **Animation Layering** | Gameplay3D | 无法"边走边挥手"等上层动作 | **1 周** |
| 🟡 P1 | **Mesh LOD 系统** | Render | 无自动 LOD，远处性能浪费 | **1-2 周** |
| 🟡 P1 | **TAA 时间抗锯齿** | Render | 无时序采样，静态帧和闪烁明显 | **2 周** |
| 🟡 P1 | **2D 多边形碰撞体** | Physics2D | 碰撞精度低，圆形/胶囊外不准 | **2 天** |
| 🟡 P1 | **2D 碰撞体编辑** | Editor | 无法编辑碰撞体形状 | **3 天** |
| 🟢 P2 | **Linux/macOS 跨平台** | Platform | 仅 Windows | **4-8 周** |
| 🟢 P2 | **网络/多人同步** | Network | 无法联机 | **4-8 周** |
| 🟢 P2 | **Mesh Shader / GPU Driven** | Render | 大规模场景 CPU 瓶颈 | **4-8 周** |
| 🟢 P2 | **物理资源流式加载** | Assets | 大世界内存不足 | **2-4 周** |
| 🟢 P2 | **D3D12 后端** | Render | 缺 D3D12（有 D3D11） | **2-3 月** |
| 🟢 P2 | **2D Blend Tree / Motion Matching** | Animation | 动画融合不够丰富 | **2-4 周** |

### 2.2 各领域的完整度

```
渲染品质        ██████████░░░░░  65%  缺 ContactShadow/IBL/LightProbeBake/TAA
动画系统        ████████░░░░░░░  55%  缺 IK/AnimationLayering/MotionMatching
2D/UI           ██████████░░░░░  70%  缺 9Slice/多边形碰撞体/碰撞体编辑
物理系统        ████████████░░░  85%  可用的完整度很高
音频系统        ████████░░░░░░░  55%  基础播放已有，缺 FMOD/Wwise/混音总线
跨平台          ██░░░░░░░░░░░░░  15%  仅 Windows
网络            ░░░░░░░░░░░░░░░   0%  完全缺失

总体           █████████░░░░░░  60%  生产级但离商用门槛还有关键缺口
```

---

## 三、推荐优先级与路线图

### 我的推荐：先做渲染管线收尾，再做角色动画补全

```
Reasoning：
  渲染是引擎的"面子"，Contact Shadow + IBL 能让现有所有 3D Demo 的画质
  明显提升。而 IK 和动画分层是角色质量的瓶颈，决定引擎能否支撑第三人称
  游戏开发。
```

### Phase 1: 渲染管线最后拼图（3-4 周）

| 顺序 | 任务 | 工期 | 为什么先做 |
|:----:|------|:----:|-----------|
| **1** | **Contact Shadow** | **3 天** | 最小投入最大收益——Screen-space ray march，CSM 精度不足处的接触阴影细节。只需新增一个 Pass + 一个 frag shader，三后端代码结构已就绪 |
| **2** | **Vignette / Film Grain** | **0.5 天** | 已在 GL inline 路径中实现，只需同步到 bloom pipeline 的独立 shader 文件即可 |
| **3** | **Light Probe SH Bake** | **1-2 周** | 数据结构完全就绪(`LightProbeComponent.sh_coefficients[9]`)。缺 Bake system（渲染 6 面 cubemap → 积分 SH）+ 运行时最近 probe 查询。GI 质量质的飞跃 |
| **4** | **Reflection Probe + IBL** | **1-2 周** | 数据结构已就绪(`ReflectionProbeComponent`)。缺 cubemap 预滤波 + Split-Sum BRDF LUT + `pbr.frag` 中采样环境贴图。金属表面不再发黑 |

**Phase 1 总工期：3-5 周**。完成后 DSE 的渲染能力从"基础 PBR"升级为"完整 PBR + GI 探针"。

### Phase 2: 角色动画系统补全（3-5 周）

| 顺序 | 任务 | 工期 | 说明 |
|:----:|------|:----:|------|
| **5** | **FABRIK IK Solver** | **1 周** | 优先实现 FABRIK（Forward And Backward Reaching IK），比 CCD 更稳定、迭代少。先做脚部 IK，让角色脚贴在起伏地形上 |
| **6** | **IK 骨骼链配置** | **3 天** | 在 `Animator3DComponent` 中添加 IK target 骨骼链配置（脚踝/膝盖/髋，手/肘/肩），Lua API 暴露 IK target 位置 |
| **7** | **Animation Layering** | **1 周** | 在 `Animator3DComponent` 中添加 layer mask（bone mask），支持上层动作叠加。实现"走路同时挥手" |
| **8** | **Look-At IK** | **2 天** | 角色头部/眼球跟踪目标（比 FABRIK 简单得多，单独实现） |

**Phase 2 总工期：3-5 周**。完成后角色动画质量从"基础播放"提升到"可以用于第三人称游戏"。

### Phase 3: UI 与 2D 完善（1-2 周）

| 顺序 | 任务 | 工期 | 说明 |
|:----:|------|:----:|------|
| **9** | **9-Slice UI 缩放** | **2-3 天** | 在 `UISpriteComponent` 或 `UIImageComponent` 新增 `slice_left/right/top/bottom` 四边距，sprite render system 中根据九宫格拆分为 9 个 quad 绘制 |
| **10** | **多边形碰撞体编辑器** | **2-3 天** | 在 `BoxCollider2DComponent` 旁新增 `PolygonCollider2DComponent`（顶点数组），Inspector 增加顶点编辑功能 |

### Phase 4: 抗锯齿与 LOD（3-4 周）

| 顺序 | 任务 | 工期 | 说明 |
|:----:|------|:----:|------|
| **11** | **Mesh LOD 自动生成** | **1-2 周** | 基于 `MeshRendererComponent.mesh_path` 在构建时生成 LOD 级别，`BoundingBoxComponent` 距离判据切换。先做简单距离切换，不做渐隐过渡 |
| **12** | **TAA（时间抗锯齿）** | **2 周** | 需要：历史帧颜色缓冲 + 运动矢量(Motion Vector) Pass + 抖动投影矩阵(Jitter Projection) + 颜色夹紧(Clip/Clamp)。运动矢量需要所有物体都输出 velocity |

---

## 四、我最推荐优先做的具体任务

### 第 1 优先：Contact Shadow（3 天）

**原因：**
- 工期最短（3 天），收益立即可见
- DSE 的阴影系统已非常完善（CSM + PCSS + 斜率自适应 Bias），唯一缺失的就是接触阴影
- 实现难度低：screen-space ray march ≈ 50 行 shader
- 三后端代码结构已完全就绪（`builtin_passes.cpp` 新增 Pass + 3 个后端的 shader 源码），参考 SSAO Pass 的实现模式

**实现步骤：**

```
Step 1: engine/render/shaders/src/contact_shadow.frag  — Screen-space ray march shader
Step 2: engine/render/passes/builtin_passes.h/cpp       — 新增 ContactShadowPass 类
Step 3: engine/render/rhi/gl_draw_executor.cpp           — GL shader 注册
Step 4: engine/render/rhi/vulkan/vulkan_shader_sources.h — Vulkan shader 嵌入
Step 5: engine/render/rhi/dx11/dx11_shader_sources.h     — DX11 shader 嵌入
Step 6: 三端视觉验证
```

### 第 2 优先：9-Slice UI 缩放（3 天）

**原因：**
- UI 自适应是 2D 游戏的基本需求，DSE 目前完全缺失
- 相比 Light Probe/IK 等动辄 1-2 周的大任务，9-Slice 是"立等可取"的功能
- 2D 引擎分析文档(`2d-capability-analysis.md`)明确将其列为"严重缺失"

### 第 3 优先：Light Probe SH Bake（1-2 周）

**原因：**
- 数据层已完全就绪（`LightProbeComponent` 有 `sh_coefficients[9]`）
- 渲染层 `pbr.frag` 已有 `EvaluateSH(N)` 函数
- 三端 UBO 已有 `dse_scene_ubo.sh_coefficients[9]`
- 只缺 Bake 系统和运行时查询，是"最后一公里"的典型任务
- 完成后 GI 质量质的飞跃，尤其对室内场景

---

## 五、不建议近期做的事

| 事情 | 为什么不建议 | 什么时候适合做 |
|------|-------------|---------------|
| **Mega 重构：切换到 GPU Driven 管线** | 这是引擎架构最底层的变革，涉及渲染管线、资源管理、剔除系统全面重写。预计 4-8 周，期间无法推进任何其他功能 | 当现有 CPU 剔除成为场景规模瓶颈时（当前场景远未达到） |
| **从零写网络模块** | 4-8 周的纯投入期，且联机游戏还需要配套的服务器架构、状态同步、防作弊。DSE 目前连一个联机 demo 都没有 | 当需要做一款具体联机游戏时，而非提前泛化实现 |
| **D3D12 / Metal 后端** | D3D11 和 Vulkan 已覆盖 Windows/Android 核心平台。D3D12 和 Metal 的投入（各 2-3 月）在当前阶段没有足够的用户价值 | 当有明确的 Mac/Xbox 发布需求时 |
| **Linux 跨平台** | GLFW 和 OpenGL/Vulkan 后端理论上可编译，但需要处理文件路径、输入法、窗口管理等大量边缘问题。2-4 周的纯兼容性工作 | 当目标用户明确需要 Linux 支持时 |
| **材质编辑器大幅升级** | 当前编辑器已有 PBR 属性编辑 + 纹理槽拖拽 + 预览球 + Shader 选择，对独立游戏开发已足够 | 编辑器已标记为"功能足够，无需继续扩展"（`NEXT_DIRECTION.md`） |

---

## 六、快速参考：各任务的代码入口

| 任务 | 关键代码文件 | 预计新增代码 |
|------|-------------|:----------:|
| Contact Shadow | `render/passes/builtin_passes.*`, `render/shaders/src/contact_shadow.frag`, 三后端 shader sources | ~200 行 |
| 9-Slice UI | `modules/gameplay_2d/rendering/sprite_render_system.*`, `modules/gameplay_2d/ui/ui_system.*` | ~300 行 |
| Light Probe Bake | `modules/gameplay_3d/rendering/` 新增 `probe_bake_system.*` | ~800 行 |
| Reflection Probe IBL | `render/passes/builtin_passes.*`, `render/shaders/src/pbr.frag`, 三后端 draw executor | ~600 行 |
| IK (FABRIK) | `modules/gameplay_3d/animation/` 新增 `ik_solver.*` | ~400 行 |
| Animation Layering | `modules/gameplay_3d/animation/animator_system.*` | ~300 行 |
| TAA | `render/passes/builtin_passes.*`, `render/shaders/src/taa.frag`, 三后端 shader + velocity pass | ~500 行 |
| Mesh LOD | `engine/assets/asset_manager.*`, `modules/gameplay_3d/rendering/mesh_render_system.*` | ~500 行 |

---

## 七、执行顺序建议

```
Session 1 (3天): Contact Shadow                    ← 渲染，收益/成本比最高
Session 2 (3天): 9-Slice UI 缩放                    ← UI，第二快
Session 3 (1-2周): Light Probe SH Bake              ← GI，大任务
Session 4 (1-2周): Reflection Probe + IBL            ← PBR 补全
──────────────────────────────────────────────── 至此渲染管线进入成熟期
Session 5 (1周): FABRIK IK + 骨骼链配置              ← 角色
Session 6 (1周): Animation Layering                  ← 角色
Session 7 (2天): Look-At IK                          ← 角色
──────────────────────────────────────────────── 至此角色系统成熟
Session 8 (1周): 多边形碰撞体 + 编辑器                 ← 2D 物理
Session 9 (1-2周): Mesh LOD                          ← 性能
Session 10 (2周): TAA                                 ← 画质
──────────────────────────────────────────────── 至此达到"可发布"水准
```

---

## 八、一句话总结

> **DSEngine 当前最缺的不是架构改造（架构已够现代），而是功能补全。前 6 个会话（总计约 5-8 周）应该聚焦在 Contact Shadow + 9-Slice UI + Light Probe + Reflection Probe + IK + Animation Layering。做完这些，DSE 的 3D 渲染品质和角色动画就能达到"可以用于实际项目"的水准。**
