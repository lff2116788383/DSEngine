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
- 默认单线程：`job_system.cpp:25`（默认 web 构建无 `-pthread`，无 SharedArrayBuffer，`JobSystem` 同步执行）。**可选多线程**已落地：`DSE_WEB_ENABLE_THREADS=ON`（preset `web-release-3d-mt`）启用 `-pthread`+worker 线程池，`JobSystem` 按 `__EMSCRIPTEN_PTHREADS__` 起真实线程（已在 harness headless Chrome 验证 7 worker，详见 §3 多线程里程碑）。
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
| **B3** | Compute + SSBO 接通 → 复用桌面 **GPU-driven 剔除 / Clustered Forward+ / 延迟** 全链路。拆 B3a/B3b 推进：<br>　• **B3a** ✅（compute 基础设施 + WGSL 自检）：`CreateGpuBuffer` 按 `GpuBufferUsage` 授予 WGPU usage 位（`kStorage`→`Storage`、`kIndirect`→`Indirect`、vertex/index/uniform 照旧），绕开未实现的 deprecated `CreateSSBO`/`CreateIndirectBuffer`；`UpdateGpuBuffer` 对 vertex/index/uniform 委派 `UpdateBuffer`（保留 geom/UBO 版本环，避免合并丢几何），仅 storage/indirect 直写。新增 compute 通路：`CreateComputeShader`（仅 WGSL，`// dse-wgsl` sentinel）/`GetOrCreateComputePipeline`（explicit layout，group1=UBO、group3=SSBO，可见性 Compute）/`BeginComputePass`+`DispatchCompute`+`EndComputePass`（`wgpuComputePassEncoder*`）。每会话一次手写 WGSL compute 自检：dispatch 1×64 线程写 `outbuf[i]=i*2+base`（SSBO）+ 0 号线程写 indirect `DrawCmd`，`copyBufferToBuffer`→MapRead 缓冲，`wgpuBufferMapAsync` 异步逐元素回读校验（PASS 日志）。**不翻转** `SupportsCompute()`/`SupportsSSBOCompute()`（引擎 `CreateComputeShaderEx` 尚无 WGSL 源槽、高层 GPU-driven/bloom/skinning 未手译 WGSL，保持现有渲染路径不变）。验证：web-release-3d 构建通过、桌面 ctest 3/3、harness `--backend webgpu --name web3d_wgpu` 0.057%<2% 无回归、compute 自检 PASS。<br>　• **B3b-1 ✅（WGSL compute 源槽）**：`IRhiCompute::CreateComputeShaderEx` 末尾加 `const std::string& wgsl_src=""` 第四语言源槽（base 默认与 GL/Vulkan/DX11 override 忽略之，仅各自 GLSL/SPIR-V/HLSL；WebGPU override 取 `wgsl_src`——空槽返回 0 使调用方按句柄 0 优雅回退）。compute 自检改经 `CreateComputeShaderEx("","","",1,0,0,0, <WGSL>)` 创建，验证整条**引擎-facing** 多源 compute 通路（引擎各 compute 特性即按此签名调用）。仍**不翻转** `SupportsCompute()`：GPU-driven/bloom/skinning/hair/grass 等消费方尚未逐特性手译 WGSL，且翻转需逐消费方能力门控审计（多数已按句柄 0 回退，但 hair 仅门控 `SupportsCompute()` 需先核 dispatch 守护），贸然翻转会破坏现有渲染。验证：web-release-3d 构建通过、桌面 ctest 3/3、harness 双后端无回归（WebGPU 0.144% / WebGL2 1.619% < 2%）、compute 自检经新入口 PASS。<br>　• **B3b-2 ✅（GPU-driven 剔除真链路端到端验证，有界）**：(1) 落地真 `CmdDrawIndexedIndirect`（`webgpu_rhi_device.cpp` 替换原 no-op，经新抽出的 `BindPassDrawState` 共享「管线+顶点缓冲+BindGroup+索引缓冲」状态后发 `wgpuRenderPassEncoderDrawIndexedIndirect`，与直接绘制复用同一组装）。(2) 手译 `builtin_passes.cpp::kGPUCullShaderSource` 的视锥剔除部分为 WGSL（6 视锥面 × AABB 正顶点测试；Hi-Z 遮挡因需 storage-image mip 金字塔留后续），经 B3b-1 `wgsl_src` 槽接入。(3) 每会话一次 **GPU-driven 剔除真链路自检**（离屏、不碰 demo 帧/golden、不翻转能力位）：造 4 个实例（2 在视锥内 / 2 出界）→ dispatch WGSL 视锥剔除写 per-instance indirect draw command 的 `instance_count`（视锥内=1、外=0）→ 用真 `DrawIndexedIndirect` 把 4 个不同色/象限的 quad 渲到 64×64 离屏 RT（被剔实例 `instance_count=0` → 硬件不绘制）→ 回读 SSBO + 像素双重校验「instance_count 模式 == `[1,0,1,0]` 且 可见象限有对应颜色、被剔象限为黑」。证明真链路 **compute(视锥剔除)→SSBO(indirect cmd)→DrawIndexedIndirect→像素** 端到端正确。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.015% / WebGL2 1.737% < 2%）、`WebGPU[B3b-2] GPU-driven 剔除自检 PASS`、原 compute 自检仍 PASS、能力位未翻转（`supports_compute=0`/`gpu_driven_active=0`）。<br>　• **B3b-3 ✅（GPU 蒙皮 compute 真链路端到端验证，有界）**：手译 `engine/render/shaders/src/skinning.comp` 的骨骼矩阵调色板蒙皮为 WGSL（线性混合蒙皮 LBS：每顶点按 joints/weights 累加骨骼矩阵变形 pos/normal/tangent，morph_target_count=0 占位），经 B3b-1 `wgsl_src` 槽接入。每会话一次 **GPU 蒙皮真链路自检**（离屏、不碰 demo 帧/golden、不翻转能力位）：造 1 个 quad（4 顶点全 100% 权重于 bone0=平移(0.4,0.4)）→ dispatch WGSL 蒙皮 compute 把绑定空间顶点变形写入 dst SSBO（`storage|vertex` 双用途）→ 该 dst SSBO 直接作顶点缓冲被真绘制消费渲到 64×64 离屏 RT → 双重回读校验：(1) dst SSBO 逐顶点坐标 == 绑定坐标+平移、法线保持 (0,0,1)；(2) RT 像素位移后的红色 quad 落在预期屏幕区域（中心有色、远角为黑）。证明真链路 **compute(蒙皮)→SSBO(变形顶点)→draw(顶点拉取)→像素** 端到端正确。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.001% / WebGL2 1.616% < 2%）、`WebGPU[B3b-3] GPU 蒙皮自检 PASS`、原 compute / B3b-2 自检仍 PASS、能力位未翻转（`supports_compute=0`）。<br>　• **B3b-4 ✅（storage-image compute 写支持，Hi-Z/bloom 前置）**：新增 `CreateComputeWriteTexture2D(w,h)`（`StorageBinding|CopySrc|CopyDst|TextureBinding`，`rgba8unorm`）+ `SetComputeTextureImage(binding,tex,read_only)`（写 `cur_compute_images_`，group2 槽）。`BindingInfo::Kind` 扩 `StorageTexture`（带 `tex_format`/`view_dim`）；`CollectComputeGroupBindings` group2 收集 storage-image、`GetOrCreateComputePipeline` 生成 `storageTexture{access=WriteOnly,format,viewDimension}` BGL 条目、`DispatchCompute` 按 `textureView` 绑定。每会话一次离屏自检（不碰 demo 帧/golden、不翻转能力位）：dispatch `texture_storage_2d<rgba8unorm, write>` 渐变 compute（`r=x/dim、g=y/dim、b=0.25、a=1`）写 storage 纹理 → `copyTextureToBuffer`→MapRead 回读四角/中心像素逐通道校验。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.009% / WebGL2 1.590%，均 <2%）、`WebGPU[B3b-4] storage-image 写自检 PASS`、原 B3a/B3b-2/B3b-3 自检仍 PASS、能力位未翻转（`supports_compute=0`）。<br>　• **B3b-5 ✅（Hi-Z 下采样核心 / r32float storage 读写）**：扩展 compute group2 支持「只读采样纹理（`texture_2d<f32>` + `textureLoad`，无 sampler）」与 r32float storage——`SetComputeTextureImage(binding, tex, read_only)` 按 `read_only` 分流到 `cur_compute_textures_`（采样读）/`cur_compute_images_`（storage 写），`CollectComputeGroupBindings` group2 同时收集二者（r32float 采样纹理声明为 `unfilterable-float`）。每会话一次 Hi-Z 下采样核心自检（两趟 r32float compute）：①生成趟 `texture_storage_2d<r32float, write>` 写已知渐变 `src[x,y]=f32(x)+f32(y)*100`；②下采样趟 `textureLoad` 读 src + 取 2×2 max 写 dst（边长减半，独立 compute pass 间自动屏障保证读后写可见）→ `copyTextureToBuffer` 回读 dst（4×4 r32float）逐像素校验 == CPU 预期 max。验证 compute 采样读 + r32float storage 写的读后写链路（Hi-Z 金字塔逐级下采样核心原语）。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.035% / WebGL2 1.713%，均 <2%）、`WebGPU[B3b-5] Hi-Z 下采样核心自检 PASS`、原 B3a/B3b-2/B3b-3/B3b-4 自检仍 PASS、能力位未翻转（`supports_compute=0`）。<br>　• **B3b-6 ✅（Hi-Z storage-image 多级金字塔 + per-mip 视图绑定）**：在 B3b-5 单级下采样基础上打通**多级金字塔逐级 in-place 下采样**——新增 `ComputeViewBind`（view/format/view_dim）+ `cur_compute_image_views_`/`cur_compute_texture_views_` 两张「显式视图→compute group2 槽」映射 + `SetComputeImageViewExplicit(binding,view,format,view_dim,read_only)`，绕开「句柄→默认全 mip 视图」、把同一纹理的**不同 mip 单层视图**分别作采样读 / storage 写绑定（`CollectComputeGroupBindings` group2 case 扩展消费两张视图映射，r32float 采样判 unfilterable-float）。每会话一次自检：单张 R32Float mip 链纹理（mip0=8×8 → mip1=4×4 → mip2=2×2 → mip3=1×1），①生成趟 `textureStore` 写已知渐变 `mip0[x,y]=f32(x)+f32(y)*100` 到 mip0 单 mip storage 视图；②逐级下采样趟（每级独立 compute pass，pass 间自动屏障保证上级写对本级读可见）`textureLoad` 读 mip[k-1] 单 mip 采样视图 + 取 2×2 max 写 mip[k] 单 mip storage 视图；各级 mip `copyTextureToBuffer` 到回读缓冲 256 对齐分段，单路 `wgpuBufferMapAsync` 逐级逐像素校验 == CPU 预期递归 max（因渐变沿 x/y 单调增，第 k 级 [x,y] 即所覆 2^k×2^k 块右下角 col+row*100）。验证 per-mip 视图绑定 + 多级 storage 金字塔构建（GPU-driven Hi-Z 遮挡剔除金字塔核心原语，Task 4 前置）。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.014% / WebGL2 1.633%，均 <2%）、`WebGPU[B3b-6] Hi-Z storage-image 金字塔自检 PASS`、原 B3a/B3b-2/B3b-3/B3b-4/B3b-5 自检仍 PASS、能力位未翻转（`supports_compute=0`）。<br>　• **B3b-7 ✅（句柄化 per-mip compute 绑定 `SetComputeTextureImageMip`）**：把 B3b-6 的「显式视图」绑定面收敛为引擎 Hi-Z build 真实 API——`SetComputeTextureImageMip(binding, texture_handle, mip_level, read_only, r32f)` 覆写：按 `FindTexture` 解析句柄后对 `(句柄<<16 | mip)` 缓存单层单 mip 视图（`wgpuTextureCreateView` baseMipLevel=mip/mipLevelCount=1，r32f→R32Float），经既有 `SetComputeImageViewExplicit` 路由到 group2 槽；同槽在相邻 dispatch 间可在「采样读↔storage 写」切换（先擦读/写两映射再按本次 `read_only` 落槽，规避无 dispatch 间 `ResetDrawState` 时的陈旧同槽绑定）；视图缓存随 `DeleteTexture`（`InvalidateComputeMipViews` 释放该句柄全部 mip 视图）/`Shutdown` 释放。B3b-6 金字塔自检改走此句柄 API（删手建 `hzp_mip_views_`），同时验证「句柄→单 mip 视图」整条通路。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.537% / WebGL2 1.620%，均 <2%）、`WebGPU[B3b-7] Hi-Z storage-image 金字塔自检 PASS`、原 B3a/B3b-2..6 自检仍 PASS、能力位未翻转（`supports_compute=0`）。<br>　• **B3b-8 ✅（命名 compute uniform + 句柄化采样器绑定）**：补齐引擎各 compute 消费方的参数传递面——(1) `SetComputeTextureSampler(unit, texture_handle)` 把纹理句柄绑到 compute group2 采样槽（清同槽 storage/显式视图避免残留），供 Hi-Z/GPU-driven cull 以 `textureLoad` 读 Hi-Z/深度纹理；(2) `SetComputeUniformInt/Float/Vec2i/Vec2f/Vec3/IVec3/Vec4/Mat4` 命名 uniform 设置器，按名累积进 per-dispatch staging（16B 对齐，`GetOrCreateComputeNamedOffset`/`WriteComputeNamedStaging`），`DispatchCompute` 时整块经 UBO 版本环（256 对齐）上传并绑到 group1 保留 binding(8)，dispatch 后清空 staging+映射（GL/DX11/Vulkan 同语义）。离屏自检：compute 读命名块（a_int=12345/b_float=3.5/c_coord=(2,1)/d_vec4/e_mat=单位阵）+ 在 c_coord 处 `textureLoad` 采样 `SetComputeTextureSampler` 绑定的渐变纹理 + 算 e_mat\*d_vec4，写 9×u32 SSBO→回读异步逐元素校验 == CPU 预期。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.240% / WebGL2 1.625%，均 <2%）、`WebGPU[B3b-8] 命名 uniform + compute 采样器绑定自检 PASS`、原 B3a/B3b-2..7 自检仍 PASS、能力位未翻转（`supports_compute=0`）。<br>　• **B3b-9 ✅（Hi-Z 遮挡剔除真链路自检）**：手译引擎 `HiZCullPass` GLSL 450 为 WGSL（经 B3b-8 命名 uniform `u_view_projection`/`u_screen_size`/`u_mip_count`/`u_object_count` + `SetComputeTextureSampler` 句柄绑定 Hi-Z 纹理 + AABB/可见性 SSBO group3）：AABB 8 角经 `u_view_projection` 投影 → NDC→UV 钳制 + off-screen 拒绝 → 屏幕像素跨度 `ceil(log2)` 选 mip → 5-tap Hi-Z（`textureLoad` 点采样，**base `textureDimensions` 无 level 形式 + 按 mip 右移**避开运行期 level 形式触发 D3D12/Tint 设备移除）max 深度遮挡判定 → 写可见性 SSBO → copy 回读逐元素校验 == CPU 预期 `[1,0,0,1]`（前景可见 / 被遮挡剔 / off-screen 剔 / 中景可见）。证明该消费方真 compute 逻辑 WebGPU 可用；离屏隔离、不翻能力位、不碰 demo golden。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.008% / WebGL2 1.604%，均 <2%）、`WebGPU[B3b-9] Hi-Z 遮挡剔除自检 PASS`、原 B3a/B3b-2..8 自检仍 PASS、能力位未翻转（`supports_compute=0`）。<br>　• **B3b-10 ✅（形变目标 morph 真链路自检）**：手译引擎 `MorphTargetSystem` GLSL 450 为 WGSL（`morph_target_system.cpp::kMorphTargetCompWGSL`，命名 push-constant `_20.u_vertex_count`/`_20.u_target_count` 经 group1 保留 binding + 4×SSBO group3 b0..3：base 顶点 / morph delta `[target][vertex]` / weights / 形变输出）：`pos = base + Σ weight·delta_pos`、`nrm = normalize(base + Σ weight·delta_normal)`、tangent 透传。**输入 SSBO 声明 `read_write` 以匹配 WebGPU RHI compute 布局统一 `BufferBindingType_Storage`**（着色器仅读）。自检布置 4 顶点 / 2 目标 / weights=[0.5,1.0] / 目标0 Δpos=(1,0,0)、目标1 Δpos=(0,2,0)，dispatch 后回读 4×`DeformedVertex`（每 48B）逐顶点校验 pos==base+(0.5,2,0)、`position.w==1`、法线归一化、tangent 透传 == CPU 预期。证明该消费方真 compute 逻辑 WebGPU 可用；离屏隔离、不翻能力位、不碰 demo golden。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.079% / WebGL2 1.629%，均 <2%）、`WebGPU[B3b-10] 形变目标自检 PASS`、原 B3a/B3b-2..9 自检仍 PASS、能力位未翻转（`supports_compute=0`）。<br>　• **B3b-11 ✅（DDGI 探针更新核心真链路自检）**：手译引擎 `DDGISystem` probe-update compute（`ddgi_system.cpp::kDDGIUpdateComputeSource`，GLSL 430）核心为 WGSL（`webgpu_rhi_device.cpp::RecordDDGISelfTest`）：probe SSBO（group3 b0）读探针位 → 每 texel `oct_decode` 解码方向 → 从 RSM（位置/法线/通量，3×句柄采样 group2 b2/b3/b4，`textureLoad` 整数 texel 中心采样替 `texture()`）随机采样 VPL 累积间接辐照度（`vpl_cos·receive_cos·平方衰减` 加权）→ 归一化 ×RSM 面积因子 ×0.01 → `textureStore` 写 irradiance/visibility storage image（group2 b0/b1）+ float 调试 SSBO（group3 b1）。14 命名 uniform（`u_probe_count`/`u_irradiance_texels`/`u_grid_resolution` ivec3/… 经 group1 b8 保留 binding）。自检布置：1 探针 / grid(1,1,1) / irr+vis texel 边长=4 / RSM 2×2（4 样本全同 VPL）/ hysteresis=0（绕开 temporal `imageLoad` 需 read-write storage 的限制，只写不读）。全同样本下权重在归一化中抵消 → 回读逐 texel 校验：octahedral z>0 的内 2×2 共 4 个 texel 命中 `irr==flux×0.01`、其余 ==0，命中数 ==4。证明该消费方核心 compute 逻辑 WebGPU 可用；离屏隔离、不翻能力位、不碰 demo golden。**DDGI 真正翻转能力位前另需消费方两处适配**：(1) storage image 与 RSM sampler 在 group2 的绑定槽错开（消费方现状同号 0/1/2 在 WebGPU 撞槽，`SetComputeTextureSampler` 会 erase 同号 storage image）；(2) temporal 混合的 `imageLoad` 读回 atlas 需 ping-pong 双缓冲或 read-write storage 格式（rgba8unorm 在核心 WebGPU 不支持读写）。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.010% / WebGL2 1.631%，均 <2%）、`WebGPU[B3b-11] DDGI 探针更新自检 PASS`、原 B3a/B3b-2..10 自检仍 PASS、能力位未翻转（`supports_compute=0`）。<br>　• **B3b-12 ✅（头发物理 hair Verlet 积分核心真链路自检）**：手译引擎 `HairInstance::Simulate` 真 compute（`hair_compute_shaders.h::kHairIntegrateSource`，GLSL 430）Pass 1 为 WGSL（`webgpu_rhi_device.cpp::RecordHairSelfTest`）：每顶点 Verlet 积分——4×SSBO（group3 b0..3）`pos_cur`/`pos_prev`/`pos_rest`/`strand_info`（读写）；根顶点（`rest.w<0.001`）固定且 `pos_prev←cur`；非根顶点 `velocity=(cur-prev)·(1-damping)` + 重力 `dir·mag·dt²` + 风力 `wind·dt²·(1+hash11·turb)` → 写回 `pos_cur`、`pos_prev←旧 cur`。12 命名 uniform（`u_num_vertices`/`u_dt`/`u_damping`/`u_g{x,y,z,w}`/`u_w{x,y,z,w}`/`u_time` 经 group1 b8）。自检布置：1 strand / 4 顶点（v0 根 + v1..v3）、dt=1、damping=0.2、重力 (0,-1,0)·2、风=0（绕开 hash11 风扰动对校验的影响，仍执行 hash 路径）、v1 给非零初速度（prev.y=3.1）验阻尼。回读 `pos_cur`+`pos_prev` 共 8×vec4 逐分量校验 == CPU 预期（根固定、阻尼、重力积分均正确）。证明该消费方核心 compute 逻辑 WebGPU 可用；离屏隔离、不翻能力位、不碰 demo golden。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.016% / WebGL2 1.637%，均 <2%）、`WebGPU[B3b-12] 头发物理自检 PASS`、原 B3a/B3b-2..11 自检仍 PASS、能力位未翻转（`supports_compute=0`）。<br>　• **B3b-13 ✅（bloom 双滤波 compute 自检，commit `b4dacb76`）**：手译引擎 `BloomRenderer` 真 compute（`bloom_downsample.comp` / `bloom_upsample.comp`，GLSL 450，`local_size 8×8`，group0 b0 `sampler2D u_src` + b1 `image2D u_dst` rgba16f）核心为 WGSL —— ①gen compute 按 `u_kind` 选公式写已知 rgba16f 渐变进 `src8`(8×8)/`usrc4`(4×4)/`ubase4`(4×4)（避免 CPU float→half 编码，与 CPU 预期共用公式）；②下采样 13-tap Karis 加权（`e*0.125 + 角*0.03125 + 边*0.0625 + 对角*0.125`，`src8→down4`）；③上采样 3×3 tent（`(e*4+十字*2+角)*1/16`）+ 按 `blend=0.5` 累加 base（`usrc4+ubase4→up4`）→ `copyTextureToBuffer` down4/up4 到 256 对齐回读缓冲，异步 map 后半精解码（IEEE754 binary16→float）逐 texel 逐通道校验 == CPU 同公式预期（容差 0.05 含半精舍入）。证明 `BloomRenderer` 下采样/上采样核心 compute 逻辑 WebGPU 可用；离屏隔离、不翻能力位、不碰 demo golden。记录 bloom 真翻转前置项：上采样消费方原 `imageLoad(u_dst)` 读回自身做累加，in-place read-write rgba16f storage 核心 WebGPU 不支持，需 ping-pong 或独立 base 纹理（自检以独立 `ubase4` 替代验证 tent + 累加数学）。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.102% / WebGL2 1.614%，均 <2%）、`WebGPU[B3b-13] bloom 双滤波自检 PASS`、原 B3a/B3b-2..12 自检仍 PASS、能力位未翻转（`supports_compute=0`）。<br>　• **B3b-2 暂缓（多周级）**：引擎完整 GPU-driven 场景路径在 WebGPU 仍缺执行器——`MultiDrawIndexedIndirect`/`SetupGPUDrivenPBRShader`/`BindMegaVAO`/`UpdateGPUDrivenMaterial`/纹理桶绘制 + Hi-Z storage-image 金字塔均 no-op 或未实现，与 GL/Vulkan 执行器深度耦合。逐特性翻转 `SupportsCompute()` 需补齐这些执行器 + 逐消费方门控审计（grass 仅门控 `SupportsCompute()` 等）后再做；本次有界验证已固化「真 compute→indirect→像素」链路与原语，为后续逐特性接入铺底。<br>　• **Task 3 ✅（逐消费方门控审计后翻转 `SupportsCompute()=true`，commit `635afe27`）**：B3b-2..13 逐消费方手译 WGSL 离屏自检全 PASS 后，审计 7 类 compute 消费方（GPUSkinning/Morph/Hair/DDGI/Bloom/GPU-driven cull/Grass）均有 `!SupportsCompute()` 门控优雅回退，翻转 WebGPU `SupportsCompute()=true`。**仅翻 `SupportsCompute()`**，保持 `SupportsSSBOCompute()=false`/`SupportsIndirectDraw()=false`/`CreateHiZTexture()=0`——故 skinning/grass（门控 `SupportsSSBOCompute()`）维持 CPU 回退、GPU-driven/Hi-Z（门控 indirect-draw）维持裁剪、DDGI/hair（无消费方手译 WGSL）经 `CreateComputeShaderEx` 返回 0 句柄优雅回退；morph 已注入手译 WGSL（`kMorphTargetCompWGSL`）真消费方路径激活。配套补齐：(1) `WebGPURhiDevice::BindGpuBuffer`（消费方 Dispatch 实际所调 SSBO 绑定 API）覆写路由到 `cur_ssbos_`（基类默认走 deprecated `BindSSBO` no-op，翻转后不覆写则消费方 SSBO 绑定全丢失），B3b-10 morph 自检改用 `BindGpuBuffer` 验证真消费方绑定通路；(2) `HairInstance` 加 `compute_unavailable_` 编译失败 latch——某后端 `SupportsCompute()=true` 但未提供手译 WGSL（返回 0 句柄）时首帧置位，后续帧直接跳过，避免每帧重试 + 错误刷屏。验证：web-release-3d 构建通过、桌面 ctest 3/3 全绿（gtest 用例 0 失败）、harness 双后端无回归（WebGPU 0.084% / WebGL2 1.534%，均 <2%）、`supports_compute=1` 已生效、B3a/B3b-2..13 全部自检仍 PASS。<br>　• **Task 4-1 ✅（WebGPU `MultiDrawIndexedIndirect` 覆写 + 离屏自检）**：WebGPU 无 `glMultiDrawElementsIndirect`，按已绑引擎 draw state（管线/顶点/索引缓冲经一次 `BindPassDrawState` 建立）循环逐条发 `wgpuRenderPassEncoderDrawIndexedIndirect(cur_pass_, buf, byte_offset + i*stride)`，第 i 条读取 `[indexCount,instanceCount,firstIndex,baseVertex,firstInstance]`，`instance_count=0`（被剔）时硬件不绘制；`draw_calls += draw_count`。每会话一次 B3b 模式离屏自检（`RecordMultiDrawIndirectSelfTest`）：经**引擎-facing API**（`CmdBeginRenderPass`(离屏 RT)+`CmdBindPipeline`+`CmdBindVertexBuffer`+`CmdBindIndexBuffer`+被测 `MultiDrawIndexedIndirect`，非裸 wgpu 绘制）把 4 象限 quad 渲到 64×64 RGBA16F 离屏 RT，预置 indirect cmds `instance_count=[1,0,1,0]` 模拟剔除结果，随帧 `copyTextureToBuffer`→提交后 `wgpuBufferMapAsync` 半精解码逐象限校验「可见象限(红/蓝)有色、被剔象限为黑」（`WebGPU[T4-1] … PASS` 日志）。仍**不翻转** `SupportsIndirectDraw()`（待 Subtask 2-4 全部自检通过并接入真实 `ForwardScenePass` GPU-driven 路径后才翻），不碰 demo golden。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.049% / WebGL2 1.727%，均 <2%）、T4-1 自检 PASS、B3a/B3b-2..13 全部自检仍 PASS。<br>　• **Task 4-2 ✅（WebGPU Mega VAO + 离屏自检）**：WebGPU 无 VAO 对象——`CreateMegaVAO` 经 `CreateGpuBuffer(kVertex/kIndex)` 建 VBO/IBO（始终附带 `CopyDst|CopySrc`）并以发号器登记 `mega_vaos_[id]={vbo,ibo}`，返回 `VertexArrayHandle{id}`；`UpdateMegaVBO/UpdateMegaIBO` 路由 `UpdateGpuBuffer`（顶点/索引经几何版本环避免帧内 `wgpuQueueWriteBuffer` 合并）；`BindMegaVAO` 据记录经引擎-facing `CmdBindVertexBuffer`（**BatchVertex 92B 7 属性**：pos(loc0,vec3,@0) color(loc1,vec4,@12) uv(loc2,vec2,@28) normal(loc3,vec3,@36) tangent(loc4,vec3,@48) weights(loc5,vec4,@60) joints(loc6,vec4,@76)）+ `CmdBindIndexBuffer(UInt32)` 设引擎 draw state；`UnbindVAO` 清顶点/索引绑定（等价 `glBindVertexArray(0)`）；`DeleteMegaVAO` 删 VBO/IBO 并清记录。每会话一次 B3b 模式离屏自检（`RecordMegaVaoSelfTest`）：经被测 Mega VAO API（`CreateMegaVAO`→`UpdateMegaVBO/IBO` 上传 4 象限 BatchVertex(92B) 几何→`BindMegaVAO`→`CmdDrawIndexed`，WGSL 完整声明 7 个 `@location` 与 92B 布局逐一对应）把 4 象限 quad 渲到 64×64 RGBA16F 离屏 RT，`copyTextureToBuffer`→异步回读半精解码逐象限校验「红/绿/蓝/黄各就位」（即 92B 布局 pos@0/color@12 解析与 BindMegaVAO 设状态正确，`WebGPU[T4-2] … PASS` 日志）。仍**不翻转** `SupportsIndirectDraw()`、不碰 demo golden。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.140% / WebGL2 1.634%，均 <2%）、T4-1/T4-2 自检 PASS、B3a/B3b-2..13 全部自检仍 PASS。<br>　• **Task 4-3 ✅（WebGPU GPU-driven PBR 着色器 + 离屏自检）**：手译 GL `gpu_driven_pbr` 为单源 WGSL（`kGpuDrivenPBRWGSL`）——顶点经**实例 SSBO**（`@group(3) @binding(5)`，`GPUInstanceData[]` 80B）按 `@builtin(instance_index)` 取 `model` 变换 BatchVertex 几何 + 传 `material_id`（flat）；片元经**材质 SSBO**（`@group(3) @binding(9)`，`GPUMaterialData[]` 128B）按 `material_id` 取 albedo/metallic/roughness/ao、采样 **albedo 纹理桶**（`@group(2) @binding(0)`/sampler `@binding(1)`），Cook-Torrance 直接光 + 环境项 + Reinhard/gamma；UBO 用 `@group(1) @binding(0)=PerFrame`(vp/view/camera_pos)、`@binding(1)=PerScene`(light_dir/color/params)。`EnsureGpuDrivenPBRShader` 惰性编译程序 + 建 PSO（depth test/write on、cull none、blend off）+ 默认白纹理 + PerFrame/PerScene UBO（跨帧复用、失败置标记不重试）；`SetupGPUDrivenPBRShader` 设 `cur_program_`/`cur_pso_handle_` + `UpdateGpuBuffer` 上传两 UBO + 引擎-facing `CmdBindUniformBuffer(0/1)` 绑入（与 GL `SetupGPUDrivenPBRShader` 同语义，CSM 矩阵本子项不涉及置零）；`BindGPUDrivenTextures` 经 `CmdBindTexture` 绑 albedo→slot0（handle=0→白纹理回退，余 normal/MR/emissive/occlusion 绑入但当前 WGSL 仅声明 albedo，`CollectGroupBindings` 过滤未声明）；`HasGPUDrivenPBRShader` 经一次性惰性编译报告句柄可用性。每会话一次 B3b 模式离屏自检（`RecordGpuDrivenPBRSelfTest`）：经**与 `ForwardScenePass` 同序**的引擎-facing 调用（`SetupGPUDrivenPBRShader`→`BindGpuBuffer` 实例/材质 SSBO→`BindMegaVAO`→`BindGPUDrivenTextures`→`MultiDrawIndexedIndirect`）把 2 个实例（model 平移到左/右半、材质红/绿 albedo）经单条 indirect cmd（`instanceCount=2`）渲到 64×64 离屏 RT（RGBA16F+深度）→ `copyTextureToBuffer` 提交后异步半精解码回读校验「左半红、右半绿」——证明手译 PBR WGSL 经实例 SSBO 取 model、材质 SSBO 取 albedo、纹理采样链路端到端正确。仍**不翻转** `SupportsIndirectDraw()`（待 Subtask 4 Hi-Z + 接入真实 `ForwardScenePass` GPU-driven 路径后才翻），不碰 demo golden。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.078% / WebGL2 1.621%，均 <2%）、`WebGPU[T4-3] GPU-driven PBR 自检 PASS`、T4-1/T4-2/B3a/B3b-2..13 全部自检仍 PASS。<br>　• **Task 4-4 ✅（WebGPU Hi-Z 遮挡剔除资源 + 离屏自检）**：实现 `CreateHiZTexture`/`DeleteHiZTexture`/`GetHiZMipCount`/`GetHiZGpuTexture`（先前 base 默认返回 0 → `HiZBuildPass`/`HiZCullPass` 早退）。WebGPU 无 GL 的逐 mip `glTexImage2D`，改经 `CreateTextureImpl` 建 **R32Float 完整 mip 链**纹理（mip 数与 GL 同式递推，usage=`StorageBinding`(下采样 storage 写)+`TextureBinding`(采样读)+`CopySrc`/`CopyDst`，nearest 过滤）；hiz 句柄独立发号、登记 `hiz_textures_[hiz]=引擎纹理句柄`，`GetHiZGpuTexture` 返回该引擎句柄供消费方传 `SetComputeTextureImageMip`(逐级下采样写)/`SetComputeTextureSampler`(剔除采样读)、`GetHiZMipCount` 经 `FindTexture(...)->mip_levels` 报告。每会话一次 B3b 模式离屏自检（`RecordGpuDrivenHiZCullSelfTest`，区别于 B3b-9 手建单 mip 恒值纹理仅验剔除逻辑——本自检串起**真资源 API + 既有原语**端到端）：经 `CreateHiZTexture(8×8)` 真资源 → 引擎 `HiZBuildPass` 真实绑定面 `SetComputeTextureImageMip` 写 mip0（texel(0,0)=0.9 占位遮挡深度、余 0.1）+ 逐级 2×2 max 下采样（0.9 经 3 级金字塔传至 1×1 顶 mip）→ `HiZCullPass` 手译 WGSL 经 `SetComputeTextureSampler(GetHiZGpuTexture)`+`GetHiZMipCount` 采样金字塔判遮挡 → 回读 3×u32 可见性校验 `[近物可见,远物遮挡剔除,出屏]=[1,0,0]`（近物可见恰证下采样把 0.9 传至顶 mip，否则误剔 → 失配可检）——`WebGPU[T4-4] … PASS`。注：`CreateHiZTexture` 翻为非 0 后引擎 `HiZBuildPass` 开始为真实 1280×720 深度建 Hi-Z 金字塔（日志 `Hi-Z texture created: ... 1280x720 mips=11`），但消费方 `HiZCullPass` 仍受 `gpu_driven`/`SupportsIndirectDraw()` 门控不生效，渲染输出不变。仍**不翻转** `SupportsIndirectDraw()`（待接入真实 `ForwardScenePass` GPU-driven 路径后才翻），不碰 demo golden。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.072% / WebGL2 1.623%，均 <2%）、`WebGPU[T4-4] Hi-Z 遮挡剔除自检 PASS`、T4-1/T4-2/T4-3/B3a/B3b-2..13 全部自检仍 PASS。<br>　• **Subtask 6 ✅（接真实 GPU-driven 路径 + 翻 `SupportsIndirectDraw()`）**：(1) **深度纹理 compute 采样**——`CollectComputeBindings` 对深度格式纹理发射 `texture_depth_2d`+`sampleType=Depth`（非 `texture_2d<f32>`），并把与采样纹理同槽冲突的 storage image 偏移到 `slot+kComputeStorageBindingBase`（GL sampler/image 分命名空间、WebGPU group2 统一命名空间）。(2) **4 份引擎 compute shader 手译 WGSL**（`kHiZCopy/Downsample/Cull` + `kGPUCull`，PC 经 group1 b8、纹理/storage 经 group2、SSBO 经 group3），经 `frame_pipeline` `CreateComputeShaderEx` 第 8 实参接入；4 个程序 WebGPU 编译成功（非 0 句柄）→ `gpu_driven_supported` 门控放行。(3) **翻转 `SupportsIndirectDraw()`→true**。(4) 真实路径三处修复：①`mesh_render_system` GPU draw-cmd SSBO 加 `GpuBufferUsage::kIndirect`（WebGPU 严格校验 indirect usage）；②`PrepareGPUScene` 为 inline mesh 按面法线累加算几何法线（原恒定 `(0,0,1)` 致平坦着色，与 CPU 回退路径不一致）；③间接绘制 `base_instance` 索引实例 SSBO 须 WebGPU `indirect-first-instance` 特性——shell.html 适配器支持则请求之（否则 `@builtin(instance_index)` 不含 `firstInstance`，多实例索引错乱致漏绘）；④PBR WGSL 片元乘顶点色（inline mesh 实色烘进顶点色、材质 albedo=白）。`apps/web_host/CMakeLists.txt` 预载 `samples/lua/pipelines` 使 `?profile=custom_lite` 命中。验证：web-release-3d 构建通过、**CustomLiteLua（gpu_driven）profile 双后端**（WebGPU gpu-driven 实路径 0.022% / WebGL2 CPU 回退 0.237%，均 <2%，各自 golden）渲染正确（蓝 cube + 灰 ground，面着色 + 高光）、默认 Forward3D harness 无回归（WebGPU 0.771% / WebGL2 1.624%）、T4-1..4/B3a/B3b-2..13 全部自检仍 PASS。<br>　• **Task 5.1 ✅（CSM 方向光阴影深度图采样离屏自检 T5-1）**：证明 WebGPU 能把一张 Depth32 shadow atlas（由「阴影深度趟」写）在「随后的前向趟」作 `texture_depth_2d` 经 `textureLoad`（3×3 PCF）采样比较——即 CSM 真实场景「阴影 pass 写 atlas → 前向 pass 采样」的跨 pass 读写，正是早前注释担心的 Dawn 屏障冲突点，本自检证明**分趟即无冲突**（深度纹理由 `RenderAttachment` 自动转换到 `TextureBinding`）。流程（`RecordCSMShadowSelfTest`）：①阴影趟把占据 NDC 中心 [-0.5,0.5]² 的遮挡 quad(z=0.3) 渲入 32×32 Depth32 atlas（其余清深=1.0）；②前向趟全屏 quad 经 `GetRenderTargetDepthTexture` 把 atlas 绑到 group2 slot11，`receiverDepth=0.6` 经 textureLoad 3×3 PCF 采样 → 中心受遮挡为暗、四角受光为亮 → 渲到 64×64 RGBA16Float RT → copy 回读校验。逻辑同 `forward_shaded.frag` 的 `DirectionalShadow`。**不翻能力位、不碰 demo golden**——shipping 前向 WGSL `DirectionalShadow` 仍返回 0（slot11 回退纹理为 RGBA8，与 `texture_depth_2d`(sampleType=Depth) 在 PSO 的 BGL 上声明期冲突；接真实路径需先为 slot11 备一张 Depth32 回退纹理使各 draw sampleType 一致，属后续集成步骤 5.1b）。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.976% / WebGL2 1.621%，均 <2%）、T5-1 + T4-1..4 + B3a/B3b-2..13 全部自检 PASS。<br>　• **Task 5.2 ✅（延迟着色 MRT gbuffer + 延迟光照离屏自检 T5-2）**：证明 WebGPU 能在一趟里把几何渲入「多渲染目标（MRT）gbuffer」（albedo/normal/position 3 个 RGBA16Float 颜色附件，`RenderTargetDesc.color_attachment_count=3`），再在随后的全屏光照趟把这 3 张 gbuffer 纹理一并采样做延迟光照——即延迟管线「几何趟写 gbuffer → 光照趟采样 gbuffer」核心能力。流程（`RecordDeferredSelfTest`）：①几何趟把占据 NDC 中心 [-0.6,0.6]² 的 quad 渲入 64×64×3 gbuffer（albedo=(0.8,0.2,0.2)、normal 编码(0,0,1)、position=0；其余清 0 → 空区 normal 长度 0）；②光照趟全屏 quad 把 gbuffer 三附件（`GetRenderTargetColorTexture` 0/1/2）绑到 group2 slot0/1/2，按 `@builtin(position)` 整数坐标 `textureLoad` 做延迟光照（`albedo·NdotL + ambient`，法线长度<阈视为空像素）→ 中心几何受光为红、四角空像素为黑 → 渲到 64×64 RGBA16Float RT → copy 回读校验。逻辑同 `deferred_lighting.frag`。**不翻能力位、不碰 demo golden**。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 1.048% / WebGL2 1.647%，均 <2%）、T5-2 + T5-1 + T4-1..4 + B3a/B3b-2..13 全部自检 PASS。<br>　• **Task 5.3 ✅（HDR auto-exposure 亮度归约 + ACES tonemap 离屏自检 T5-3）**：证明 WebGPU 能完成 HDR 后处理「亮度归约 → 曝光自适应 → ACES tonemap + gamma」全链。流程（`RecordHDRSelfTest`）：①把已知 HDR 色 (4,2,1) 渲入 8×8 RGBA16Float 场景 RT；②亮度归约趟 `textureLoad` 整张算平均 log 亮度（`avgLum=exp(avgLogLum)`）写 1×1；③lum_adapt 趟算曝光 `0.18/avgLum`（clamp [0.01,10]）写 1×1；④tonemap 趟 `ACES(hdr*exposure)` + gamma 1/2.2 渲到 64×64 RGBA16Float RT → copy 回读，回调用同公式 C++ 复算逐通道比对（容差 0.04，且 r>g>b 保序、非全白非全黑）。逻辑同 `lum_adapt.frag` + `tonemapping.frag`。**不翻能力位、不碰 demo golden**。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.250% / WebGL2 1.606%，均 <2%）、T5-3 + T5-1/5-2 + T4-1..4 + B3a/B3b-2..13 全部自检 PASS。<br>　• **Task 5.4 ✅（IBL split-sum：BRDF LUT + irradiance + prefilter env + PBR 环境项离屏自检 T5-4）**：证明 WebGPU 能完成 IBL「BRDF LUT 积分 → 环境项合成」。流程（`RecordIBLSelfTest`）：①BRDF LUT 趟用 GGX split-sum（Hammersley 256 采样）积分 (A,B) 渲入 64×64 RGBA16Float LUT；②PBR 趟全屏 quad 采样 LUT 的 texel（NdotV≈1、roughness≈0.5）+ 常量 irradiance/prefilter 色按 `kD·diffuse + (F·A+B)·prefilter` 合成环境漫反射+镜面项渲到 64×64 RT → copy 回读，回调用同 split-sum C++ 复算逐通道比对（容差 0.04）。逻辑同引擎 BRDF LUT / IBL 合成。**不翻能力位、不碰 demo golden**。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.250% / WebGL2 1.606%，均 <2%）、T5-4 + T5-1/5-2/5-3 + T4-1..4 + B3a/B3b-2..13 全部自检 PASS。<br>　• **Task 5.5 ✅（WBOIT accum/reveal MRT + resolve 离屏自检 T5-5）**：证明 WebGPU 能完成加权混合 OIT「双层 accum/reveal MRT 累积 → resolve 解析」。流程（`RecordWBOITSelfTest`）：①双层透明片（红 a=0.5 z=0.2、蓝 a=0.5 z=0.6）按深度权重 `w=max(0.01,3·(1-z))` 累积到 accum（RGBA16F 预乘色×权重）+ reveal（透过率）MRT；②resolve 趟 `accum.rgb/accum.a · (1-reveal)` 渲到 64×64 RT → copy 回读，回调用同公式 C++ 复算逐通道比对（容差 0.04，且红蓝双层均有贡献、无绿层）。逻辑同引擎 WBOIT accum/resolve。**不翻能力位、不碰 demo golden**。验证：web-release-3d 构建通过、harness 双后端无回归（WebGPU 0.250% / WebGL2 1.606%，均 <2%）、T5-5 + T5-1..5-4 + T4-1..4 + B3a/B3b-2..13 全部自检 PASS。 |
| **B4** ✅ | 能力探测与声明式回退矩阵：WebGPU 可用走 parity 路径，否则回退阶段 A 的 WebGL2 路径。落地内容——(1) **JS 侧探测**（shell.html）：`navigator.gpu` 存在且 `?backend=webgpu` 时 push `--backend=webgpu`，preRun 异步 `requestAdapter().requestDevice()` 预创建设备挂 `Module.preinitializedWebGPUDevice`；不支持/失败则不挂设备。(2) **后端解析与编译期回退**（`rhi_factory.cpp`）：`ResolveRhiBackendFromEnv`(`DSE_RHI_BACKEND`)→`ValidateRhiBackend`（未编译的后端降级）→`CreateRhiDevice`。(3) **运行期回退**（`frame_pipeline.cpp`）：`InitDevice` 失败（含 `emscripten_webgpu_get_device()` 返回空句柄）自动重建为 OpenGL 设备。(4) **设备能力探测**（`webgpu_rhi_device.cpp::AcquireDevice`）：`wgpuDeviceGetLimits` 读取实际 `maxColorAttachments` 填充 `GetMaxColorAttachments()`（供 `requires_mrt` 裁剪），并一次性输出能力矩阵日志（`WebGPU[B4] 能力矩阵：max_color_attachments=8 supports_compute=0 supports_ssbo=1 …`）。(5) **声明式裁剪路由**（§2 A0 机制复用）：当前 WebGPU `compute=false`（B3a 未翻转）→ default 管线自动裁掉 compute/gpu-driven pass，与 WebGL2 同走前向能力子集；B3b 翻转 `SupportsCompute()` 后**同一裁剪机制自动放行**重型 pass 路由到 parity 路径，无需额外接线。守护 gtest 新增 `WebGPUB3aRoutesLikeWebGL2ForwardSubset`/`WebGPUParityContextKeepsHeavyPasses` 两用例固化此不变量。验证：web-release-3d 构建通过、桌面 ctest（含 15 个 prune 用例）全绿、harness `--backend webgpu` 0.180%<2% 无回归 + 能力矩阵日志 + compute 自检 PASS |
| **B5** | CI：WebGPU headless（Dawn 软件适配器）渲染回归 |

> B 体量≈当年新增 Vulkan 后端（多周）。浏览器支持：Chrome/Edge 稳定版已发，
> Safari/Firefox 铺开中 → **必须保留 WebGL2 回退**（正是阶段 A 的价值）。

### 并行能力解锁（parity 的性能前提）—— 多线程 ✅（可选构建）
WebGL2/WASM 默认单线程，重场景 CPU 端（蒙皮/剔除/粒子）压主线程是 parity 的帧率天花板。
解法：**SharedArrayBuffer + COOP/COEP 响应头 + `-pthread`**（同时解锁 Web 上的 Job System）。

**已落地（可选开关，默认 OFF，现有 `web-release-3d` 单线程构建零改动）：**
- `DSE_WEB_ENABLE_THREADS` 选项（`CMakeLists.txt`）：ON 时全局加 `-pthread` 编译选项（pthreads 共享内存模型要求**所有** TU 一致编译），链接侧（`apps/web_host`）加 `-pthread` + `-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency` + `-sPTHREAD_POOL_SIZE_STRICT=0`（预建 worker 池，避免运行时同步建线程阻塞主线程；池耗尽允许按需再建）。
- preset `web-release-3d-mt`（继承 `web-release-3d`）：一键多线程构建。
- `JobSystem::Init`（`job_system.cpp:25`）：单线程提前返回的门控改为 `__EMSCRIPTEN__ && !__EMSCRIPTEN_PTHREADS__`，多线程构建（定义 `__EMSCRIPTEN_PTHREADS__`）下走桌面同款真实线程池（`hardware_concurrency()-1` 个 worker + 工作窃取）。
- **不**加 `-sPROXY_TO_PTHREAD`：GLFW/WebGPU 设备与渲染须留主线程，线程池仅供 `JobSystem` 跑 CPU 作业（剔除/动画/粒子）。
- 验证：preset `web-release-3d-mt` 构建+链接 0 error；harness（headless Chrome，已发 COOP/COEP）实测 `JobSystem 初始化：worker 线程数=7`（SharedArrayBuffer 工作），golden 1.426% < 2% 无视觉回归，compute 自检经 `CreateComputeShaderEx` PASS。
- 部署：托管端须下发跨源隔离头（见 §7.4），否则 `SharedArrayBuffer` 不可用、线程池无法起线程。

### B4 能力矩阵与路由（当前实现快照）
能力位来自 `RhiDevice` 能力查询（非平台 `#ifdef`）；管线按下表自我裁剪（§2 A0 机制）。

| 能力 / 后端 | 桌面 GL4.3+ | 桌面 Vulkan/D3D11 | **WebGPU（Task 3 后）** | WebGL2 / GLES3.0 |
|-------------|:----------:|:-----------------:|:----------------------:|:----------------:|
| `SupportsCompute()` | ✅ | ✅ | ✅（Task 3 翻转，B3b-2..13 自检全 PASS + 门控审计） | ❌ |
| `SupportsSSBO()` | ✅ | ✅ | ✅（`CreateGpuBuffer(kStorage)`） | GL≥4.3 才 ✅ |
| `SupportsSSBOCompute()`（同步回读） | ✅ | ✅ | ❌（异步回读，B3b/B5） | ❌ |
| GPU-driven 剔除 / Hi-Z | ✅ | ✅ | ✅（Task 4 翻转 `SupportsIndirectDraw()`，CustomLiteLua 真路径 0.022%<2%） | ❌ |
| `GetMaxColorAttachments()`（MRT） | 探测 | 探测 | **8（`wgpuDeviceGetLimits` 探测）** | 探测（≥4） |
| 前向 PBR + 阴影 + 后处理（片元） | ✅ | ✅ | ✅ | ✅ |
| **default 管线路由** | 全链路 parity | 全链路 parity | **前向能力子集（同 WebGL2）** | 前向能力子集 |

路由由声明式裁剪驱动：compute/gpu-driven pass 声明 `requires_compute`/`requires_gpu_driven`，
Task 3 起 WebGPU `compute=true`：morph 等仅门控 `SupportsCompute()` 的消费方已激活，
Task 4 起 `SupportsIndirectDraw()=true`：GPU-driven/Hi-Z 剔除真路径在 `gpu_driven` profile
（如 CustomLiteLua）激活并验证双后端；skinning/grass 仍门控 `SupportsSSBOCompute()`（仍 false）。
default（Forward3D）profile `gpu_driven=false` → 视觉不变（golden 未动）。**翻转无需改裁剪代码**，
同一 default 管线按各消费方能力门控逐步放行重型 pass（守护 gtest
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
- [x] 多线程（SharedArrayBuffer + COOP/COEP + pthreads）：可选开关 `DSE_WEB_ENABLE_THREADS` / preset `web-release-3d-mt`，`JobSystem` 起真实 worker（harness 实测 7 线程，无回归）；默认仍单线程（见 §3 多线程里程碑、§7.4）
- [x] 文档：Web 能力矩阵（§B4 ✅）、限制（§5 ✅）、部署指南（§7 ✅）、多线程部署头（§7.4 ✅）

---

## 5. 风险与残留约束（如实登记）
1. **本机无独立 GPU**：仅能用软件 GL（llvmpipe/swiftshader）验证 harness 与出图逻辑；
   真机/移动端逐设备验证需托管/自托管 runner。Web headless 可用 swiftshader 在**托管** runner 跑（门槛低于桌面 GPU 回归）。
2. **WebGL2 无 compute**：阶段 A 永远拿不到 GPU-driven/clustered compute/延迟 → parity 必须靠阶段 B(WebGPU)。
3. **默认单线程**：默认构建重场景 CPU 端压主线程仍是帧率天花板；多线程已作**可选构建**落地（`web-release-3d-mt`，见 §3），默认 OFF 以保产物零特殊托管头/最大兼容，按需开启。
4. **点光立方体阴影**：ES3.0 仅逐面渲染（`gl_rhi_device.cpp:703`），精度/性能弱于桌面分层 FBO。
5. **着色器双路径**：ES3.0 变体由自动 lowering 兜住，成本低但需保证每个新 pass 有降级分支（A0 守护 gtest 兜底）。
6. **IBL 离线烘焙**：引入一个 host 资产步骤，走现有资产管线，不算散落债。

---

## 6. 工作节奏
每阶段：**先更新本文档/相关文档 → 提交推送 → 实现 → 全量回归（不破现有 2273 unit + 546 integration）**。
阶段 A 优先（高 ROI、广覆盖、三目标复用）；阶段 B 作为 parity 必经的独立大里程碑接续。

---

## 7. 部署指南（Web/WASM）

### 7.1 构建产物
`cmake --build --preset web-release-3d --target dse_web_host`（需 EMSDK 3.1.64）产出到 `bin/`：

| 文件 | 说明 |
|------|------|
| `index.html` | 入口（即 `apps/web_host/shell.html`，含能力探测 + 路径选择 JS） |
| `index.js` | Emscripten 胶水（加载 wasm、MEMFS、运行时） |
| `index.wasm` | 引擎 + 应用代码 |
| `index.data` | 预打包资产（MEMFS 镜像；大资产可改 fetch 懒加载，见 A5） |

### 7.2 静态托管（零服务端）
四个产物同目录静态托管即可，无需任何后端。务必配置正确 MIME（与 harness `tests/web/visual_regression.mjs` 一致）：

| 扩展名 | Content-Type |
|--------|--------------|
| `.wasm` | `application/wasm` |
| `.js` | `text/javascript` |
| `.data` | `application/octet-stream` |

`.wasm` 缺正确 MIME 会退化为非流式编译（变慢），部分 CSP 下甚至失败。

### 7.3 运行期 URL 参数（同一份产物多路径）
shell.html 据 URL 参数 + 浏览器能力算好后经 `Module.arguments` 传入（见 `apps/web_host/web_main.cpp`）：

| 参数 | 取值 | 作用 |
|------|------|------|
| `?mode=` | `2d` \| `3d`（缺省 3D） | 渲染路径；3D 但无 WebGL2 自动回退 2D |
| `?backend=` | `webgpu` \| `webgl2`（缺省 webgl2） | RHI 后端；`webgpu` 仅在 `mode=3d` 且 `navigator.gpu` 存在时启用，设备获取失败自动回退 WebGL2 |
| `?profile=` | 剖面名 | 覆盖 `DSE_RENDER_PIPELINE_PROFILE` |
| `?lua=` | 脚本路径 | 覆盖启动 Lua 脚本 |

例：`https://host/index.html?mode=3d&backend=webgpu`（WebGPU 优先，自动回退）。

### 7.4 跨源隔离（多线程构建必需）
默认单线程产物（`web-release-3d`）**无需**特殊响应头。**多线程构建**（`web-release-3d-mt`，`-pthread` + SharedArrayBuffer，见 §3 多线程里程碑）则托管端必须下发跨源隔离头使 `SharedArrayBuffer` 可用、worker 线程池得以起线程：

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

其下所有跨源子资源需带 `Cross-Origin-Resource-Policy`/CORP 或经 CORS，否则被拦截。该头不影响单线程产物，可提前配置。多线程产物额外多一个 `index.worker.js`（pthread worker 引导），须与 `index.{html,js,wasm,data}` 同目录托管。harness 静态 server（`tests/web/visual_regression.mjs`）已下发上述两头，故 headless Chrome 下 `SharedArrayBuffer` 可用、`JobSystem` 实测起 7 worker。缺头时浏览器 `crossOriginIsolated=false`，pthreads 构建无法起线程（应回退默认单线程产物）。
