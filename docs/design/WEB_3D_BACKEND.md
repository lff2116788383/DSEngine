# Web/WASM 3D 后端成熟化设计

> 目标：把 Web/WASM 3D 从"M5 尽力版"提升为**桌面级 parity 旗舰后端**。
> 架构原则：**能力声明式自动裁剪（capability-declarative）**——同一条 default 管线
> 在所有平台按设备能力自我裁剪 + 自动降级着色器，杜绝"多剖面清单"约定式技术债。
>
> 状态：设计已评审通过（目标 = parity；采纳能力声明式裁剪）。实现分阶段推进。

---

## 1. 现状（基于源码核实）

**构建/部署：已成熟。**
- Emscripten + WebGL2(=GLES 3.0)，`apps/web_host`（`index.html/.js/.wasm` + MEMFS 预打包）。
- 预设：`web-debug` / `web-release` / `web-debug-3d` / `web-release-3d`（`DSE_WEB_ENABLE_3D=ON`）。
- CI `build-web`：host 编 `dse_shader_compiler` → 生成含 **ESSL300** 的 embed 头 → emcmake 链接 → 校验产物。
- 打包 `engine/project/web_build.*` / `web_dist.*`，含单测。

**RHI/渲染：Web 走 OpenGL 后端编到 WebGL2，3D 仅前向尽力版。**
- 后端：`opengl / dx11 / vulkan`，**无 WebGPU**。
- 能力门控按 capability（非平台 `#ifdef`）：`supports_ssbo_ = GL≥4.3`（`gl_rhi_device.cpp:305`）。
- WebGL2 无 SSBO/Compute → `Forward3D` 剖面（`render_pipeline_profile.cpp:380`）：
  `pre_z → forward_scene → ui → composite → present`。
  - ✅ UBO 版 Clustered Forward PBR（点/聚光降为定长 UBO 循环，ESSL300）。
  - ❌ 阴影、HDR 后处理、IBL、GPU-driven 剔除、延迟着色、WBOIT。
  - `WarmupAllPostProcessShaders()` 在非 SSBO 上下文被跳过（`gl_rhi_device.cpp:311`）。

**运行期固有约束（WebGL2/Emscripten）：**
- 单线程：`job_system.cpp:24`（Web 链接无 `-pthread`，无 SharedArrayBuffer）。
- 无 Compute / 无 SSBO / 无 MultiDrawIndirect / 无 timestamp query / 立方体分层 FBO 受限
  （`gl_rhi_device.cpp:703` 仅占位 +X 面）。

**测试缺口：Web 运行期零自动化验证**（CI 只 `ls` 产物，不验出图）。

> **复用面（重要，决定这不是 web 专属债）**：Web 走的 GLES3.0 档同时服务
> **Android(GLES3.1) 与低端桌面 GL<4.3**。为阴影/后处理/IBL 补的 ESSL300 变体与能力降级
> **三目标共享**，不是为 WebGPU 铺路的弃子。

---

## 2. 架构决策：能力声明式自动裁剪（替代双剖面）

**反模式（已否决）**：维护 `Forward3D`(lite) 与 `Forward3D-HQ` 两份硬编码剖面清单。
每新增一个 pass，作者须记得"塞进哪份剖面、web 要不要关"——必然漏（约定式技术债，
与 time-scale 的 `delta_time()` 语义 footgun 同类）。

**采纳方案（零债）**：让 pass **声明能力需求**，管线按**设备能力**自动裁剪 + 降级。

### 2.1 扩展 `RenderPassMetadata`（已有一半基础设施）
现状 `render_pipeline_profile.h:40` 已有 `requires_hiz` / `requires_gpu_driven`。补全为：
```cpp
struct RenderPassMetadata {
    std::string name;
    bool required = false;
    bool runtime_only = false;
    bool requires_hiz = false;
    bool requires_gpu_driven = false;
    bool requires_compute = false;   // 新增
    bool requires_ssbo    = false;   // 新增
    bool requires_mrt     = false;   // 新增（多渲染目标/延迟）
    // 可选：requires_float_blend / requires_timestamp 等按需扩充
};
```

### 2.2 扩展设备能力查询（`RhiDevice` 能力位）
现状已有 `supports_ssbo_`。补：`supports_compute_`、`max_color_attachments`（MRT 上限）、
（可选）`supports_float_blend` 等，由各后端在 Init 时探测填充。

### 2.3 管线装配时自动裁剪
- 取 **唯一的 default 管线**（`MakeForwardPlusDefaultProfile`，含全部 pass）。
- 对每个 pass：`if (meta.requires_X && !device.supports_X) prune(pass)`。
- 着色器：沿用现有 capability-driven lowering（SSBO→UBO 已工作），为新 pass 补 ESSL300 lowering 分支。
- 结果：桌面 GL4.3+/Vulkan/D3D11 跑全链路；WebGL2/Android 自动落到"前向 + 片元能力子集"；
  **新 pass 只声明一次能力需求，到处自动正确**。
- `Forward3D` / `Forward2D` 等保留为**显式覆盖**（用户主动选 lite），但默认路径不再依赖它们区分能力。

> 验证守护：加 gtest 断言"给定一组 device caps，裁剪后的 pass 集合符合预期"，
> 防止未来 pass 漏标能力位。

---

## 3. 路线（目标 = 桌面级 parity）

分两大阶段：**A = GLES 能力档成熟化（广覆盖、即刻可发，三目标复用）**，
**B = WebGPU 后端（compute/SSBO → 桌面 parity）**。A 不阻塞、先落地；B 是 parity 的必经大工程。

### 阶段 A —— WebGL2/GLES 前向成熟（仅用片元能力）
| 子阶段 | 内容 | 依赖能力 |
|--------|------|----------|
| **A0 架构** | 落地 §2 能力声明式裁剪（metadata + caps + 自动 prune + 守护 gtest） | — |
| **A1 阴影** | 方向光 CSM + 聚光阴影 ESSL300 路径；点光立方体阴影逐面（精度受限，登记） | 2D 深度纹理（ES3.0 ✓） |
| **A2 后处理** | tonemap + bloom + FXAA ESSL300 全屏链；非 SSBO 安全 warmup 子集；可选片元 SSAO | 全屏片元 pass |
| **A3 IBL/天空** | 离线烘焙 prefiltered env + BRDF LUT（host 资产步骤）；运行时 IBL 采样 + 天空盒 | mip 采样（ES3.0 ✓） |
| **A4 Web 自动化测试** | headless Chrome(WebGL2/swiftshader) 跑 `dse_web_host` + 截图视觉回归；接入 CI `build-web` | 托管 runner 即可，**不需自托管 GPU** |
| **A5 打磨** | 能力探测自动选路径 + URL 覆盖；加载进度；大资产 MEMFS→fetch 懒加载 | — |

### 阶段 B —— WebGPU(Dawn/Emscripten) RHI 后端（桌面 parity）
| 子阶段 | 内容 |
|--------|------|
| **B0** ✅ | Dawn/Emscripten WebGPU 工具链集成（`-sUSE_WEBGPU=1`）；`RhiBackend::WebGPU` 枚举 + 工厂分发；`WebGPURhiDevice` 骨架（设备 import + 交换链 + 每帧 clear pass 证明贯通）；shell.html 预创建设备 + `?backend=webgpu` 显式启用 + 失败自动回退 WebGL2。资源/绘制接口为安全占位（句柄发号 / no-op），B1 起替换 |
| **B1** ✅ | `RhiDevice` 资源接口 WebGPU 映射：句柄表（buffers/textures/render_targets/pipeline_states/shaders）→ 原生 WGPU 对象；`wgpuDeviceCreateBuffer`+`wgpuQueueWriteBuffer`（顶点/索引/uniform）、`wgpuDeviceCreateTexture`+`wgpuQueueWriteTexture`（2D/cube/cube-mips/3D，RGBA8/RGBA16F/Depth32）、`wgpuDeviceCreateSampler`（filter/wrap 映射）、离屏 RT（颜色 RGBA16F + 深度 Depth32，MRT/cube）；着色器源暂存 + PSO 子状态登记（WGPURenderPipeline 组装留 B2）。回读异步留 B5 |
| **B2** ✅ | 着色器路径：WGSL（无离线 SPIR-V→WGSL 工具，手写 WGSL，sentinel `// dse-wgsl` 区分引擎 GLSL）+ 命令录制引擎（`WGPURenderPassEncoder` / explicit pipeline-layout + BGL 缓存 / push-constant 模拟 / `Draw*`）+ bring-up 自检上屏验证。拆 B2a/B2b/B2c 推进：<br>　• **B2a** ✅（commit `507d92ea`）：`WebGPUCommandBuffer`→设备级 `Cmd*` 立即转发；`CmdBeginRenderPass`/`CmdSetViewport`/`CmdBind*`/`CmdPushConstants`/`CmdDraw*` 录制到 `frame_encoder_`；`GetOrCreateRenderPipeline`（显式 layout+BGL 缓存，签名=program+pso+顶点布局+颜色/深度格式+采样数+绑定签名）+ `BuildAndSetBindGroups`（group0 push / group1 UBO / group2 tex+sampler / group3 SSBO）+ `CompileWGSL`/`MakeFaceView`/`CollectGroupBindings`；`CreateShaderProgram` 以 `// dse-wgsl` sentinel 识别 WGSL；bring-up 自检（渐变×棋盘）经 `Cmd*` 上屏验证整链。配套修：WebGPU 不占 `#canvas` 的 GL context（`engine_app.cpp`）、`BeginFrame` 惰性建设备+swapchain（web 宿主空窗口句柄不调 `InitDevice`）、去 `wgpuSwapChainPresent`（Emscripten abort）、视口裁剪到 RT 范围。golden `tests/web/baseline/web3d_wgpu.png`（distinctColors=990 / 非背景 0.98 / 0px 确定性）。harness `--backend=webgpu`（Dawn+SwiftShader Vulkan）已就绪。<br>　• **B2b** ✅（commit `3f3e76e2`）：Skybox / composite / fullscreen-blit 真实 WGSL 上屏（替换引擎对应内建 program）；`EnsureInitialized` 惰性建设备修首帧 RT 全 0 黑屏；每帧 UBO 版本环（`AllocUboVersion`/`ubo_versions_`）修「单 dynamic UBO 每 draw `UpdateGpuBuffer` 被 `wgpuQueueWriteBuffer` 合并」致逐材质数据互相覆盖；全屏 blit/composite WGSL 翻转 V（WebGPU 纹理 top-left 原点）；缺绑定守护跳过 draw。<br>　• **B2c** ✅：forward 真实场景上屏 + 进阶 shading WGSL。`kWgslForward`（ForwardPbr，64B PerMaterial，Lambert + albedo）保留；新增 `kWgslForwardShaded`（ForwardShaded/`DrawShaded` 专用，160B PerMaterial）移植 `forward_shaded.frag` 特性子集——Cook-Torrance PBR（GGX NDF + Smith G + Fresnel）、shading_mode（0 PBR / 2 半兰伯特皮肤 / 3 半兰伯特静态 / 4 Toon / Unlit）、clustered 点光（group1 b3 `PointLightsUBO` ≤64）、SSS wrap、clearcoat 二次叶；UBO 逐字段对齐 `FwdShadedMaterialUBO`(160B)/`FwdPerSceneUBO`(560B)/`PointLightsUBO` std140；末尾 Reinhard+gamma（同 `forward_shaded.frag`，配 composite 二次处理与 WebGL2 参考帧对齐，灰地面渐变 + 浅蓝立方体含高光，harness 0.07%<2%）。暂缓：CSM 方向光阴影采样——shadow atlas 为 Depth32，作前向采样绑定与阴影写入同一同步作用域读写冲突（Dawn 校验报错），需先做通道屏障分离；`DirectionalShadow` 暂返回 0（demo `receive_shadow` 关），PerScene CSM 字段已按 std140 声明待接回。法线/MR/自发光贴图、splat/积雪、DDGI/SH、聚光灯、watercolor(5)/faceSDF(6) 留后续。<br>　　– 【先修 ✅】立方体绕序/丢失：根因非 frontFace——引擎前向路径 `BuildShadedWorldVertexBuffer` 每 draw 把世界空间顶点/索引重写进**共享** `vbo_`/`ibo_`（offset=0），与 B2b 的 UBO 同属 `wgpuQueueWriteBuffer` 帧内合并问题：所有 draw 只见最后一次写入（地面顶点覆盖立方体 → 立方体丢失，非被剔）。修复：把版本环机制扩到顶点/索引缓冲（`AllocGeomVersion`/`geom_versions_`，环 usage=Vertex\|Index\|CopyDst、4 对齐），`IssueDraw` 的 `SetVertexBuffer`/`SetIndexBuffer` 改绑当帧最近版本切片，使各 draw 见各自几何。开背面剔除即正确显示灰地面 + 浅蓝立方体（golden `web3d_wgpu.png` 已更新，harness `--backend webgpu` 0.11% < 2%）。<br>　　– 【进阶 shading ✅】见上方 B2c 摘要（`kWgslForwardShaded` 移植 `forward_shaded.frag` 子集；CSM 阴影采样暂缓待通道屏障）。 |
| **B3** | Compute + SSBO 接通 → 复用桌面 **GPU-driven 剔除 / Clustered Forward+ / 延迟** 全链路。拆 B3a/B3b 推进：<br>　• **B3a** ✅（compute 基础设施 + WGSL 自检）：`CreateGpuBuffer` 按 `GpuBufferUsage` 授予 WGPU usage 位（`kStorage`→`Storage`、`kIndirect`→`Indirect`、vertex/index/uniform 照旧），绕开未实现的 deprecated `CreateSSBO`/`CreateIndirectBuffer`；`UpdateGpuBuffer` 对 vertex/index/uniform 委派 `UpdateBuffer`（保留 geom/UBO 版本环，避免合并丢几何），仅 storage/indirect 直写。新增 compute 通路：`CreateComputeShader`（仅 WGSL，`// dse-wgsl` sentinel）/`GetOrCreateComputePipeline`（explicit layout，group1=UBO、group3=SSBO，可见性 Compute）/`BeginComputePass`+`DispatchCompute`+`EndComputePass`（`wgpuComputePassEncoder*`）。每会话一次手写 WGSL compute 自检：dispatch 1×64 线程写 `outbuf[i]=i*2+base`（SSBO）+ 0 号线程写 indirect `DrawCmd`，`copyBufferToBuffer`→MapRead 缓冲，`wgpuBufferMapAsync` 异步逐元素回读校验（PASS 日志）。**不翻转** `SupportsCompute()`/`SupportsSSBOCompute()`（引擎 `CreateComputeShaderEx` 尚无 WGSL 源槽、高层 GPU-driven/bloom/skinning 未手译 WGSL，保持现有渲染路径不变）。验证：web-release-3d 构建通过、桌面 ctest 3/3、harness `--backend webgpu --name web3d_wgpu` 0.057%<2% 无回归、compute 自检 PASS。<br>　• **B3b**：RHI 加 WGSL compute 源通路（`CreateComputeShaderEx` 第四源槽）+ 逐特性手译 compute WGSL（GPU-driven 剔除 / clustered / skinning / bloom），届时翻转能力位 |
| **B4** ✅ | 能力探测与声明式回退矩阵：WebGPU 可用走 parity 路径，否则回退阶段 A 的 WebGL2 路径。落地内容——(1) **JS 侧探测**（shell.html）：`navigator.gpu` 存在且 `?backend=webgpu` 时 push `--backend=webgpu`，preRun 异步 `requestAdapter().requestDevice()` 预创建设备挂 `Module.preinitializedWebGPUDevice`；不支持/失败则不挂设备。(2) **后端解析与编译期回退**（`rhi_factory.cpp`）：`ResolveRhiBackendFromEnv`(`DSE_RHI_BACKEND`)→`ValidateRhiBackend`（未编译的后端降级）→`CreateRhiDevice`。(3) **运行期回退**（`frame_pipeline.cpp`）：`InitDevice` 失败（含 `emscripten_webgpu_get_device()` 返回空句柄）自动重建为 OpenGL 设备。(4) **设备能力探测**（`webgpu_rhi_device.cpp::AcquireDevice`）：`wgpuDeviceGetLimits` 读取实际 `maxColorAttachments` 填充 `GetMaxColorAttachments()`（供 `requires_mrt` 裁剪），并一次性输出能力矩阵日志（`WebGPU[B4] 能力矩阵：max_color_attachments=8 supports_compute=0 supports_ssbo=1 …`）。(5) **声明式裁剪路由**（§2 A0 机制复用）：当前 WebGPU `compute=false`（B3a 未翻转）→ default 管线自动裁掉 compute/gpu-driven pass，与 WebGL2 同走前向能力子集；B3b 翻转 `SupportsCompute()` 后**同一裁剪机制自动放行**重型 pass 路由到 parity 路径，无需额外接线。守护 gtest 新增 `WebGPUB3aRoutesLikeWebGL2ForwardSubset`/`WebGPUParityContextKeepsHeavyPasses` 两用例固化此不变量。验证：web-release-3d 构建通过、桌面 ctest（含 15 个 prune 用例）全绿、harness `--backend webgpu` 0.180%<2% 无回归 + 能力矩阵日志 + compute 自检 PASS |
| **B5** | CI：WebGPU headless（Dawn 软件适配器）渲染回归 |

> B 体量≈当年新增 Vulkan 后端（多周）。浏览器支持：Chrome/Edge 稳定版已发，
> Safari/Firefox 铺开中 → **必须保留 WebGL2 回退**（正是阶段 A 的价值）。

### 并行能力解锁（parity 的性能前提）—— 多线程
WebGL2/WASM 默认单线程，重场景 CPU 端（蒙皮/剔除/粒子）压主线程是 parity 的帧率天花板。
解法：**SharedArrayBuffer + COOP/COEP 响应头 + `-pthread`**（同时解锁 Web 上的 Job System）。
中等工程（构建 flag + 部署头），作为独立里程碑跟踪（非本设计的着色器/管线工作，但 parity 必需）。

### B4 能力矩阵与路由（当前实现快照）
能力位来自 `RhiDevice` 能力查询（非平台 `#ifdef`）；管线按下表自我裁剪（§2 A0 机制）。

| 能力 / 后端 | 桌面 GL4.3+ | 桌面 Vulkan/D3D11 | **WebGPU（B3a 当前）** | WebGL2 / GLES3.0 |
|-------------|:----------:|:-----------------:|:----------------------:|:----------------:|
| `SupportsCompute()` | ✅ | ✅ | ❌（基础设施就绪，B3b 翻转） | ❌ |
| `SupportsSSBO()` | ✅ | ✅ | ✅（`CreateGpuBuffer(kStorage)`） | GL≥4.3 才 ✅ |
| `SupportsSSBOCompute()`（同步回读） | ✅ | ✅ | ❌（异步回读，B3b/B5） | ❌ |
| GPU-driven 剔除 / Hi-Z | ✅ | ✅ | ❌（依赖 compute，自动裁剪） | ❌ |
| `GetMaxColorAttachments()`（MRT） | 探测 | 探测 | **8（`wgpuDeviceGetLimits` 探测）** | 探测（≥4） |
| 前向 PBR + 阴影 + 后处理（片元） | ✅ | ✅ | ✅ | ✅ |
| **default 管线路由** | 全链路 parity | 全链路 parity | **前向能力子集（同 WebGL2）** | 前向能力子集 |

路由由声明式裁剪驱动：compute/gpu-driven pass 声明 `requires_compute`/`requires_gpu_driven`，
WebGPU 当前 `compute=false` 故被自动裁剪 → 与 WebGL2 同走前向子集。**B3b 翻转 `SupportsCompute()`
后无需改裁剪代码**，同一 default 管线即放行重型 pass 路由到 parity 路径（守护 gtest
`WebGPUParityContextKeepsHeavyPasses` 已前瞻固化）。回退链：JS 侧 `navigator.gpu` 探测 →
后端解析/编译期降级（`ValidateRhiBackend`）→ 运行期 `InitDevice` 失败重建 OpenGL。

---

## 4. 成熟（parity）判定清单
- [x] §2 能力声明式裁剪落地 + 守护 gtest（A0）
- [x] 方向光 CSM / 聚光阴影（A1）；点光立方体阴影逐面（A1，精度登记）
- [x] HDR tonemap+bloom+FXAA（A2）；片元 SSAO（A2，可选）
- [x] IBL（离线 prefilter mip 链 GGX 重要性采样 + 运行时 textureLod 采样）+ 天空盒（A3）
- [x] MSAA/AA 默认开启（A6，GL 后端多重采样 renderbuffer FBO + EndRenderPass 解析 blit；GLES/WebGL2 通用，按 GL_MAX_SAMPLES clamp；scene RT 默认 4x）
- [x] Web headless 视觉回归 harness（A4，swiftshader + 帧计数 + rAF 抓帧 + 非平凡守护 + golden diff；CI 接入按需后挂）
- [x] 运行期能力探测自动选路径 + URL 覆盖（?mode/?profile/?lua）+ 加载进度 + 大资产 MEMFS→fetch 懒加载（A5）
- [x] WebGPU 后端工具链 + 设备/交换链骨架 + 每帧 clear pass（B0）
- [~] WebGPU 后端 + compute/SSBO 全链路（B1–B2 ✅、B3a compute 基础设施+WGSL 自检 ✅、B3b 逐特性手译 WGSL 待做）
- [x] WebGPU/WebGL2 能力探测与回退（B4：JS 探测 + 后端解析/编译期降级 + 运行期回退 + `wgpuDeviceGetLimits` 能力探测 + 声明式裁剪路由 + 守护 gtest）
- [ ] 多线程（SharedArrayBuffer + COOP/COEP + pthreads）
- [ ] 文档：Web 能力矩阵、限制、部署指南

---

## 5. 风险与残留约束（如实登记）
1. **本机无独立 GPU**：仅能用软件 GL（llvmpipe/swiftshader）验证 harness 与出图逻辑；
   真机/移动端逐设备验证需托管/自托管 runner。Web headless 可用 swiftshader 在**托管** runner 跑（门槛低于桌面 GPU 回归）。
2. **WebGL2 无 compute**：阶段 A 永远拿不到 GPU-driven/clustered compute/延迟 → parity 必须靠阶段 B(WebGPU)。
3. **单线程**：重场景帧率天花板，需多线程里程碑解。
4. **点光立方体阴影**：ES3.0 仅逐面渲染（`gl_rhi_device.cpp:703`），精度/性能弱于桌面分层 FBO。
5. **着色器双路径**：ES3.0 变体由自动 lowering 兜住，成本低但需保证每个新 pass 有降级分支（A0 守护 gtest 兜底）。
6. **IBL 离线烘焙**：引入一个 host 资产步骤，走现有资产管线，不算散落债。

---

## 6. 工作节奏
每阶段：**先更新本文档/相关文档 → 提交推送 → 实现 → 全量回归（不破现有 2273 unit + 546 integration）**。
阶段 A 优先（高 ROI、广覆盖、三目标复用）；阶段 B 作为 parity 必经的独立大里程碑接续。
