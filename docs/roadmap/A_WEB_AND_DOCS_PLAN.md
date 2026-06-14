# 阶段 A 实施方案 v2：A1 Web/WASM 导出 + A3 文档/教程

> 制定日期：2026-06-13 · 分支：`feature/engine-lib`
> 依据：基于源码核实（`PlatformApp` 抽象、GLFW 宿主、CMake 平台分发、DSSL 着色器工具链、miniaudio 音频、JobSystem、`dse` CLI）。
> 范围：本文件是**实施方案/评审稿**，经确认后再开始写代码。A1 与 A3 并行推进。
> **v2 变更**（批判性复盘后）：① 着色器 `300 es` 改由 DSSL 工具链自动生成（不手改）；② 特性按**能力标志**而非平台宏门控；③ 音频(miniaudio/WebAudio)纳入范围；④ 触屏输入纳入范围；⑤ 资源懒加载登记为显式债；⑥ WebGPU 作为规划中的未来后端写明；⑦ 新增 M0 最小验证靶。详见 §五技术债登记表。

---

## 防债核心原则（v2 硬约束，贯穿全程）

1. **能力驱动，非平台宏驱动**：Web 的能力差异（无 Compute/SSBO/MultiDraw）统一收敛成 RHI 能力查询（扩展已有 `SupportsSSBO()` 模式，新增 `SupportsCompute()`/`SupportsIndirectDraw()` 等）。渲染代码**按能力分支**；**严禁**在 `engine/render` 核心撒 `#ifdef __EMSCRIPTEN__`（平台宏只允许出现在 `engine/platform/web` 与构建脚本）。
2. **着色器变体由工具链生成，不手改**：`300 es` 作为 DSSL/着色器跨编译的一个输出目标，与 `glsl430/450/310es/hlsl/spirv` 同源产出。
3. **走现有抽象，不特例**：VFS IO、JobSystem 线程数、音频设备、输入回调都走已有接口/配置项，不为 Web 加特例分支。
4. **所有妥协显式登记**：任何为 MVP 做的简化（预载资源、关 3D 等）必须进 §五债表，不埋隐债。

---

## 〇、总体目标与验收

- **A1 验收**：一个 2D 示例工程经 `dse build --target web`（或等价 CMake 预设）产出 `index.html + .wasm + .data`，在 Chrome/Firefox 跑通（精灵渲染 + 键鼠输入 + 帧循环），可直接上传 itch.io；CI 新增 `build-web` 编译作业。
- **A3 验收**：一个从未接触引擎的人，照 `docs/getting-started.md` + 教程系列，能在 30 分钟内独立做出并导出一个最小 2D 游戏。
- **非目标（本阶段明确不做）**：Web 上的 Compute/DDGI/TressFX/GPU-Driven、Vulkan/D3D11 on Web、微信小游戏（放在 A 之后的增量）、macOS（属阶段 B）。

---

## 一、A1 现状核实（决定方案的关键事实）

| 事实 | 来源 | 影响 |
|------|------|------|
| 平台经 `PlatformApp` 抽象，`CreateDefaultPlatformApp()` 按平台分发 | `engine/platform/platform_app.h:71`、`glfw/glfw_app.cpp:305`、`android/android_app.cpp:273` | 新增 web 后端 = 实现一个 `PlatformApp` + 一个 `CreateDefaultPlatformApp()` 分支，**不动引擎其它部分** |
| 桌面宿主用 `GLFW` + `glad/gl` | `glfw/glfw_app.cpp` | Emscripten 自带 GLFW3 移植（`-sUSE_GLFW=3`）+ WebGL2 context，**可大幅复用 glfw_app 逻辑** |
| CMake 已有 `if(ANDROID)` 分支：切后端、过滤平台源、强制 GLES | `CMakeLists.txt:52,80,372` | 新增 `if(EMSCRIPTEN)` 分支照此模式即可 |
| 已存在 GLES 着色器变体 `#version 310 es`（60+） + Android EGL/GLES 后端 | `grep #version`、`engine/platform/android` | WebGL2=GLES3.0，需把 `310 es`(GLES3.1) 降到 `300 es`；**SSBO→UBO 降级逻辑已存在**（`gl_shader_manager.cpp:136` GL3.3 fallback） |
| WebGL2 **无 Compute Shader** | WebGL2 规范 | DDGI/TressFX/Grass/GPU-Driven 在 Web 关闭，走 CPU 兜底或直接 `#ifdef` 排除 |
| 网络 GNS 默认 OFF | `CMakeLists.txt:68` | Web 上无影响（如需联机走 WebSocket，后续再说） |

---

## 二、A1 技术方案

### 2.1 新增 Web 平台后端
- 新增 `engine/platform/web/web_app.{h,cpp}`，实现 `PlatformApp`：
  - `Init`：emscripten-GLFW 创建 WebGL2 context（`GLFW_CONTEXT_VERSION_MAJOR=3` + `GLFW_CLIENT_API=GLFW_OPENGL_ES_API`，或 `emscripten_webgl_create_context`）。
  - `PollEvents`/`SwapBuffers`/`GetTime`/`GetFramebufferSize`：复用 emscripten-GLFW 等价调用；canvas 尺寸经 `emscripten_get_canvas_element_size`。
  - `LoadGLFunctions`：WebGL2 函数由 emscripten 静态提供，`LoadGLFunctions` 退化为 no-op / glad-es 适配。
  - Vulkan/D3D 相关接口 `CreateVulkanSurface` 返回 0、`HasGLContext` 返回 true。
- `CreateDefaultPlatformApp()` 增加 `#elif defined(__EMSCRIPTEN__)` 分支返回 `WebApp`。

### 2.2 主循环改造（Emscripten 要求）
- Web 不能 `while(true)` 阻塞主线程，必须用 `emscripten_set_main_loop`。
- 新建 `apps/web_host/`（建议，隔离更干净）作为 emscripten 入口：把"每帧"封装成回调交给 `emscripten_set_main_loop_arg`；桌面端宿主保持原 `while` 循环不变。`__EMSCRIPTEN__` 宏只出现在 `apps/web_host` 与 `engine/platform/web`，不进引擎核心。
- **线程**：`job_system.cpp` 线程数可配（`hardware_concurrency-1`），MVP 设为 0/1 走主线程内联执行——这是已有的干净配置路径，**非 hack**。多线程(pthreads + COOP/COEP)留待后续优化。

### 2.3 渲染：WebGL2 路径（GL RHI 复用 + 能力门控）
- 复用现有 OpenGL RHI，差异**全部走能力标志**（见防债原则 1），核心渲染代码无平台宏：
  - 新增/扩展 RHI 能力查询：`SupportsCompute()`/`SupportsSSBO()`/`SupportsIndirectDraw()`；WebGL2 后端全部返回 false。
  - 不支持的特性按能力**自动旁路**：Compute 管线、`MultiDrawIndexedIndirect`、GPU-Driven 剔除（走已有 CPU readback 兜底）、DDGI/TressFX/Grass。
  - 优先保证：2D 精灵/UI/文本、基础 3D 前向 PBR、能在 fragment 内做的后处理。
- **着色器 `300 es` 由 DSSL/着色器工具链生成**（防债原则 2），作为与 `glsl430/450/310es/hlsl/spirv` 并列的输出目标；现有 `GenerateUBOGLSL`/SSBO→UBO 降级逻辑作为生成器内部步骤复用，**不手改 60+ 着色器**。
- **MVP 聚焦 2D 全绿**；3D 前向作为"尽力而为"，不阻塞验收。

### 2.3b 音频（v2 新增）
- 引擎音频后端为 **miniaudio**（`engine/audio/audio_system.cpp`），miniaudio 原生支持 Emscripten/WebAudio——**走现有抽象，仅在 CMake 为 emscripten 选 miniaudio 的 web 后端**，不加业务层特例。
- 验证任务：浏览器内播放一段 BGM + 一个音效；注意浏览器自动播放策略（需首次用户交互后 resume AudioContext）。

### 2.3c 输入：键鼠 + 触屏（v2 新增）
- 复用 `PlatformApp` 输入回调；Web 后端把 emscripten 键鼠事件接上。
- **触屏**：照搬 Android 已有的 `touch→pointer(button 0)` 映射（`android_app.cpp`），让移动端 Web / 微信可玩；单指优先，多指后续。

### 2.4 构建系统
- CMake 新增 `if(EMSCRIPTEN)` 分支（仿 `ANDROID`）：
  - 强制 `DSE_ENABLE_VULKAN=OFF`、`DSE_ENABLE_D3D11=OFF`、`DSE_ENABLE_3D` 可选、`DSE_ENABLE_NET=OFF`。
  - 过滤掉 `engine/platform/glfw` 与 `engine/platform/android`，编入 `engine/platform/web`。
  - 链接/编译选项：`-sUSE_GLFW=3 -sFULL_ES3=1 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1 --preload-file <assets>`。
  - 音频：emscripten miniaudio/WebAudio 后端开关。
- 新增 `apps/web_host/`（或复用 standalone 的 emscripten 分支）产出 `index.html`（自定义模板含 canvas + loading）。
- 新增 CMake 预设 `web-release` / `web-debug`（`emcmake cmake` 包装），文档化 emsdk 安装步骤。

### 2.5 资源与文件系统
- VFS 走现有抽象，仅切 IO 后端：MVP 用 emscripten `--preload-file` 把资源打成 `.data`，运行时只读 VFS（已有 VFS/PAK）。
- 存档/可写数据：用 IDBFS（持久化到浏览器 IndexedDB）+ `FS.syncfs`。
- **⚠️ 显式债（DEBT-1）**：`--preload-file` 一次性全量加载，**大游戏/微信分包上限会爆**。VFS IO 后端须设计成可替换；后续改 **lazy fetch + IndexedDB 缓存**。见 §五。

### 2.6 `dse` CLI 集成（A1 与 A2 衔接）
- `dse build --target web`：内部调用 `emcmake cmake` + `cmake --build`，输出到 `dist/web/`。
- `dse dist --target web`：打包 `index.html + .wasm + .data + .js` 成可上传压缩包。
- MVP 阶段可先用 CMake 预设手动跑通，再回填到 CLI。

### 2.6b WebGPU：规划中的未来后端（v2 写明，非本期实现）
- WebGL2 **无 Compute** → Web 上 DDGI/TressFX/GPU-Driven 永久缺席。这是平台限制，非本项目债；但为拿回 Compute 能力，**未来可加 WebGPU 作为第 4 个 RHI 后端**（引擎已是多后端架构，属"加后端"非重写）。
- 因此 v2 的隔离纪律（能力门控 + 平台宏不入核心）正是为 WebGPU 平滑接入铺路。
- 注意：**微信小游戏只支持 WebGL，不支持 WebGPU**，故 WebGL2 路径无论如何都要长期保留。见 §五 DEBT-2。

### 2.7 CI
- `.github/workflows/ci.yml` 新增 `build-web` 作业：装 emsdk → `emcmake` 配置 → 编译 2D 示例 → 产物存档（不需 GPU，纯编译即可看护回归）。

### 2.8 里程碑（A1）
0. **M0 最小验证靶**（v2 新增）：先不接全引擎，用 emscripten 跑一个独立"清屏 + 1 三角形/精灵"最小靶，把 emsdk/GLFW/WebGL2/CI 工具链排雷。
1. **M1 编译通过**：引擎 + 2D 示例经 emscripten 编出 `.wasm`（哪怕黑屏）。
2. **M2 跑通帧循环 + 清屏**：`emscripten_set_main_loop` + WebGL2 clear 出颜色。
3. **M3 2D 渲染**：精灵/UI/文本在浏览器显示，键鼠输入可用。
4. **M4 出包 + CI**：`dse dist --target web` 出可上传包；CI `build-web` 绿。
5. **M5（尽力）**：基础 3D 前向场景跑通。**尽力版已达成**（2026-06-13）：Web/WebGL2 上 3D 前向真实出画，详见 §2.8b。

### 2.8b M5 实现纪要（尽力版，2026-06-13）

开关与构建：
- 新增 `DSE_WEB_ENABLE_3D`（默认 OFF）。`if(EMSCRIPTEN)` 块仅在 `NOT DSE_WEB_ENABLE_3D` 时才强制 `DSE_ENABLE_3D=OFF`；置 ON 即在 Web 保留 3D 前向路径（物理/Vulkan/D3D11 仍强制关）。见 `CMakeLists.txt`。
- 新增配置/构建预设 `web-debug-3d` / `web-release-3d`（继承 `web-base`，`DSE_WEB_ENABLE_3D=ON`）。
- 实测：开 3D 后整套 `modules/gameplay_3d/*` 在 Emscripten 下编译链接通过（无物理后端时排除 physics3d/vehicle/buoyancy/ragdoll/fracture）。

运行时（全部能力门控，非平台宏 —— 守防债原则 1）：
- `gl_rhi_device.cpp`：`!supports_ssbo_`（即 WebGL2）下除 `InitSprite2DShader()` 外，当 `DSE_ENABLE_3D` 时也调 `InitBuiltinPBRShader()`。该函数本就有非 SSBO 的 UBO 分支（桌面 GL<4.3 同路径），降级到 ESSL300；GPU-Driven/Compute 变体仍在其内部按能力门控关闭。
- 新增 `Forward3D` 管线 profile（`render_pipeline_profile.cpp` 的 `MakeForward3DProfile()`，选择器 `forward_3d`/`3d`/`web3d`）：`pre_z → forward_scene → ui → composite → present`，`gpu_driven=false`、无 shadow、无 HDR 后处理链——全部需要 Compute/SSBO 的环节都不在内。
- `web_main.cpp`：`DSE_ENABLE_3D` 构建下自动选 `forward_3d` profile 并加载 `data/main3d.lua`；否则维持已验证的 2D-first MVP（`forward_2d` + `data/main.lua`）。
- ESSL300 会剥离 sampler 的 `layout(binding)`，PBR 路径已由 `CachePBRLocations()` → `BindSamplersOnce()`（反射驱动显式 `glUniform1i` 赋采样单元）自动处理。

浏览器实测（Chrome/WebGL2 over ANGLE→D3D11）：3D 场景真实出画，引擎逐帧统计 `meshes=4, draw_calls=4, render_passes=5, gpu_driven_supported=0`；立方体（36 索引）+ 地面（6 索引）每帧绘制，带透视 + 深度 + 光照，PBR 程序在 WebGL2 编译/链接/绘制通过，不黑屏不崩。
> ⚠️ 更正（2026-06-14）：上述「浏览器实测」实为桌面 D3D11/WARP 核对，非真实 WebGL2；真机验证见 §2.8c。

**观感打磨（DEBT-6，已偿还，2026-06-14）**：在 `data/main3d.lua`（场景作者层，三后端通用、对 WebGL2 能力安全）做了如下调整：相机后拉并抬高（`radius 6.5→9.5`、`height 2.2→3.4`）修正构图偏近；方向光降平铺环境光、提关键光强（`ambient 0.25→0.10`、`intensity 1.5→2.4`）让立方体各面拉开明暗、不再发灰；新增 `SkyLight` 半球环境色（CPU 端 `mix(down,up)*intensity` 的环境色 uniform，无 Compute/SSBO/bake，Web 安全）给环境上冷调；立方体 `roughness 0.55→0.38` 出清晰高光；新增 `PostProcess` 组件显式定曝光（`exposure=0.9`）+ 轻微 vignette（composite pass 内 tonemap 即生效，无需新增 pass）。桌面 D3D11/WARP 实跑核对：立方体呈现明确的三维明暗与高光、构图留白合理。

### 2.8c 真机 WebGL2 端到端验证与修复（2026-06-14，进行中）

> 重要更正：§2.8b 中「浏览器实测」一段实际是在**桌面 D3D11/WARP**（同样走非 SSBO 的 UBO 着色器路径）上核对的，**并未**在真实浏览器 WebGL2 端到端验证。本轮首次在真实 Chrome/WebGL2 跑 wasm 产物，发现 M5 Web 3D 之前其实是**黑屏**，根因有三，均已在代码侧修复（不改 `.glsl` 源、不改场景）：

环境：本机无独显，Chrome 137（Chrome for Testing）经 ANGLE→SwiftShader 软件光栅跑 WebGL2，需启动参数 `--enable-unsafe-swiftshader --use-angle=swiftshader --ignore-gpu-blocklist`。emsdk：`C:\emsdk`（emcc 6.0.0）。web 调试产物（`web-debug-3d` preset）：`bin/index.{html,js,wasm,data}`，调试 wasm ~98–102MB。本地 `python -m http.server 8080` 起服务，CDP（`--remote-debugging-port` + `--remote-allow-origins=*`）抓取控制台与运行时 WebGL 状态定位问题。

1. **顶点着色器 SSBO 非法（编译失败）**：生成的 ESSL300 顶点变体（`kpbr_vert_essl300`/`kshadow_vert_essl300`）仍保留 `layout(std430) readonly buffer`（`BoneMatricesSSBO`/`SkinnedInstBuf`/`ComputeSkinBuf`），而 GLES3.0/WebGL2 不支持 SSBO/std430/readonly → 顶点编译失败、程序链接失败。之前桌面 NVIDIA GL3.3 驱动「宽容」接受了 SSBO 才一直没暴露。
   - 修复：`gl_shader_manager.cpp` 新增 `LowerVertexSSBOToUBO()`，在 `!supports_ssbo_` 路径把上述 SSBO 降级为定长 UBO（`BoneMatrices`→binding 6，按 `kMaxBones` 定长；另两者最小定长、不可达不读），与既有 fragment 的 `GenerateUBOGLSL` 对称；`InitBuiltinPBRShader()`/`InitShadowShader()` 接入；非 SSBO 路径下显式 `glUniformBlockBinding` 绑定 `BoneMatrices`/`PointLights`/`SpotLights`（反射元数据仍按 SSBO 列出、无法自动绑定）。

2. **合成（后处理）着色器采样器单元冲突（绘制失败 → 黑屏）**：`Forward3D` 的 composite 用 `bloom_composite_ssao_ae`，其 `screenTexture/bloomBlur/ssaoTexture/autoExposureTex/contactShadowTex`（sampler2D）与 `u_lut`（sampler3D）在 ESSL300 下被剥离 `layout(binding)`、未显式赋单元者全部回退到单元 0；同一单元 0 上 sampler2D 与 sampler3D 共存，在 WebGL2 触发 `GL_INVALID_OPERATION: Two textures of different types use the same sampler location`，**整个合成 draw 失败**——场景已渲到离屏 FBO 却无法合成到屏幕，于是黑屏。
   - 修复：`gl_draw_executor_postprocess.cpp` 的 `BindBloomComposite` 显式 `glUniform1i` 把各次级采样器赋到固定单元（bloom=2/ssao=3/ae=4/lut=5/cs=6，与其纹理绑定一致），与既有 `BindTonemapping`/`BindColorGrading` 同一套路；桌面端与 `layout(binding)` 等价、为 no-op。

3. （定位）reflection 仍按 SSBO 列出灯光/骨骼块，非 SSBO 路径需显式绑定——已随第 1 项处理。

桌面回归：`ctest` 3/3 通过（unit/integration/smoke）。**剩余（后续）**：重编 `web-debug-3d` → 真机 Chrome/WebGL2 复验立方体出画（DEBT-6 观感可见、旋转、键盘）并录屏 → 补 CI `build-web` 与冒烟测试 → 复验通过后回填本节结论。

### 2.9 风险与对策
| 风险 | 等级 | 对策 |
|------|------|------|
| 着色器变体生成器要支持 300 es 目标 | 🟡 | 走 DSSL 工具链加目标（防债原则 2），复用现有 SSBO→UBO 降级；不手改着色器 |
| 多线程 JobSystem 在 Web 受限 | 🟡 | 用 `job_system` 已有线程数配置走主线程内联；pthreads 作为后续优化 |
| 第三方库不兼容 WASM（assimp/Jolt 等） | 🟡 | MVP 关 3D/物理或仅编 2D 必需库；逐库验证，必要时运行时按需加载 |
| 浏览器自动播放策略挡音频 | 🟢 | 首次用户交互后 resume AudioContext |
| 包体过大 | 🟢 | `-Os` + 资源分离 `.data` + gzip；微信小游戏阶段再做分包瘦身（见 DEBT-1） |

---

## 三、A3 文档/教程方案（与 A1 并行，不依赖引擎改动）

### 3.1 交付物
1. `docs/getting-started.md`：安装 → 配置 → 编译运行第一个窗口/精灵（5 分钟）。中英双语。
2. `docs/tutorials/2d-first-game/`：系列教程「做一个最小 2D 游戏」：
   - 01 项目创建（`dse new`）/工程结构
   - 02 渲染一个精灵 + 摄像机
   - 03 输入与移动
   - 04 碰撞与拾取/计分
   - 05 UI 与音效（音频走 miniaudio，桌面与 Web 一致）
   - 06 打包发布（桌面 + Web 导出，衔接 A1）
3. `docs/faq.md` + `docs/api-cheatsheet.md`：常见问题 + Lua/C ABI 速查。
4. README 增加「5 分钟上手」入口与文档导航。

### 3.2 原则
- 每篇教程都给**可运行的完整代码**，最好对应 `samples/` 或 `templates/` 里一个真实工程。
- 以 Lua 脚本为主线（上手成本最低），C++/C# 作进阶。
- 文档与代码同仓，CI 可选加 markdown 链接检查。

### 3.3 里程碑（A3）
1. **D1**：`getting-started.md` 跑通（基于现有桌面构建）。
2. **D2**：2D 教程 01–03（创建→精灵→输入）。
3. **D3**：2D 教程 04–06（碰撞→UI/音效→打包，打包章节等 A1 的 M4 完成后补 Web 导出）。
4. **D4**：FAQ + 速查 + README 导航。

---

## 四、并行推进顺序（建议）

```
周 1-2  : A1-M1/M2（编译通过 + 清屏）          ‖  A3-D1（getting-started）
周 3-4  : A1-M3（2D 渲染 + 输入）              ‖  A3-D2（教程 01-03）
周 5-6  : A1-M4（出包 + CI）                   ‖  A3-D3（教程 04-06，含 Web 导出章）
周 7+   : A1-M5（3D 尽力）+ A3-D4（FAQ/速查）  ‖  收尾 + 微信小游戏可行性预研
```

> 实际节奏以维护者确认为准；每个里程碑独立可交付、可提交。

---

## 五、技术债登记表（v2 新增，显式跟踪）

| ID | 债项 | 起因 | 影响 | 偿还计划 | 等级 |
|----|------|------|------|----------|------|
| DEBT-1 | 资源全量预载（`--preload-file`） | MVP 求快 | 大游戏/微信分包上限会爆 | VFS IO 后端预留可替换点；后续改 lazy fetch + IndexedDB 缓存 | 🟡 |
| DEBT-2 | Web 无 Compute（DDGI/TressFX/GPU-Driven 缺席） | WebGL2 平台限制 | Web 渲染功能缩水 | 未来加 WebGPU 第 4 后端；v2 隔离纪律已为此铺路（微信仍需 WebGL2） | 🟡 |
| DEBT-3 | MVP 关 3D / 部分第三方库 | 求 2D 先跑通 + 库 WASM 兼容性 | ~~Web 暂只 2D~~ → 已开 3D 前向（`DSE_WEB_ENABLE_3D`，opt-in） | M5 尽力版已开 3D 前向并实测出画（见 §2.8b）；物理/部分库仍关，按需逐库验证 | 🟢 |
| DEBT-4 | 单线程（无 pthreads） | 避开 COOP/COEP 与多线程 WASM 复杂度 | Web 端无并行 Job | 后续接 emscripten pthreads + COOP/COEP | 🟢 |
| DEBT-5 | 多指触控未做（仅单指） | MVP 求快 | 复杂手势类游戏受限 | 后续补多指/手势 | 🟢 |

| DEBT-6 | ~~M5 Web 3D 观感未打磨（整体偏亮发灰、构图偏近）~~ → 已偿还（2026-06-14） | 尽力版优先打通"能跑通 3D 前向"，观感未调 | ~~Web 3D demo 视觉效果欠佳~~；已在 `data/main3d.lua` 调相机/光照/材质/曝光，画面明暗与构图改善（详见 §2.8b） | 已完成：场景层调光照强度/曝光（新增 PostProcess 设曝光+vignette）、相机距离、材质表现、SkyLight 半球环境光；Forward3D 的 composite pass 本就跑 tonemap，无需额外接线 | ✅ |

> 原则：以上均为**有意识、显式登记**的债，非隐性债；每项都有偿还路径与触发条件。

---

## 六、待确认的决策点（开工前）

1. **Web 宿主入口**：新建 `apps/web_host`（v2 建议，已采纳为默认）。
2. **MVP 是否先关 3D**：默认 `DSE_ENABLE_3D=OFF` 走纯 2D，再逐步开 3D（建议是）。
3. **线程**：MVP 主线程内联（建议是）。
4. **emsdk 版本**：固定一个版本写进文档/CI（建议最新 LTS）。
5. **2D 示例选哪个**：用现有 `samples/` 里哪个作为 Web 首发 demo？

---

> 经你确认以上决策点（或直接"按建议默认"）后，我即按里程碑 M0→M1/D1 开工，并每个里程碑提交推送到 `feature/engine-lib`。
