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
| **B0** | Dawn/Emscripten WebGPU 工具链集成；`RhiBackend::WebGPU` 枚举 + 工厂；设备/交换链创建 |
| **B1** | `RhiDevice` 全接口 WebGPU 映射（缓冲/纹理/采样/绑定组/管线状态对象） |
| **B2** | 着色器路径：WGSL（或 SPIR-V→WGSL via Tint）；接入现有 shader 生成流水 |
| **B3** | Compute + SSBO 接通 → 复用桌面 **GPU-driven 剔除 / Clustered Forward+ / 延迟** 全链路 |
| **B4** | 能力探测：WebGPU 可用走 parity 路径，否则回退阶段 A 的 WebGL2 路径 |
| **B5** | CI：WebGPU headless（Dawn 软件适配器）渲染回归 |

> B 体量≈当年新增 Vulkan 后端（多周）。浏览器支持：Chrome/Edge 稳定版已发，
> Safari/Firefox 铺开中 → **必须保留 WebGL2 回退**（正是阶段 A 的价值）。

### 并行能力解锁（parity 的性能前提）—— 多线程
WebGL2/WASM 默认单线程，重场景 CPU 端（蒙皮/剔除/粒子）压主线程是 parity 的帧率天花板。
解法：**SharedArrayBuffer + COOP/COEP 响应头 + `-pthread`**（同时解锁 Web 上的 Job System）。
中等工程（构建 flag + 部署头），作为独立里程碑跟踪（非本设计的着色器/管线工作，但 parity 必需）。

---

## 4. 成熟（parity）判定清单
- [x] §2 能力声明式裁剪落地 + 守护 gtest（A0）
- [x] 方向光 CSM / 聚光阴影（A1）；点光立方体阴影逐面（A1，精度登记）
- [x] HDR tonemap+bloom+FXAA（A2）；片元 SSAO（A2，可选）
- [ ] IBL（离线 prefilter + 运行时）+ 天空盒（A3）
- [ ] MSAA/AA 默认开启
- [ ] Web headless 视觉回归接入 CI（A4）
- [ ] 资产 fetch 懒加载（A5）
- [ ] WebGPU 后端 + compute/SSBO 全链路（B0–B3）
- [ ] WebGPU/WebGL2 能力探测与回退（B4）
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
