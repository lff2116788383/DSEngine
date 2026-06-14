# DSEngine 当前进度 · 对比主流引擎 · 发布 SDK 差距分析

> 生成日期：2026-06-10（与代码现状重新核对，仅以源码为准）
> 二次复核：2026-06-10（逐项 grep 核实；更正风格化渲染 Outline/Edge-Detect 已实现的过时表述；确认 CSM 级联阴影、反射探针(Split-Sum IBL)、本地化、编辑器动画时间轴/撤销重做/性能分析器 均已在代码落地；新增第六节「生产级就绪度评估」）
> 三次复核：2026-06-13（基于 `feature/engine-lib@f4d8e652`，自上次报告后 +88 提交；仅以源码为准重新逐项核实）。本轮主要变化：① 新增**内存管理子系统**（`engine/core/memory/`：Memory 门面 + Linear/Frame/Pool 分配器 + Handle/HandleTable + 可选 mimalloc 后端）；② 网络新增**玩法级复制层 MVP**（`engine/net/replication/`：服务器权威 Transform 同步 + spawn/快照/属主输入 RPC）；③ **资产热重载新增 Linux inotify 后端**（`asset_manager.cpp`，原仅 Windows）；④ headless **dse CLI** + 端到端加密资源打包；⑤ 启动 Splash（编辑器/运行时/打包游戏/Linux X11）；⑥ CI 增加 editor / Linux / Android 作业；⑦ Lua 绑定大幅补全（C ABI 由 ~330 增至 ~518）。
> 四次复核：2026-06-14（基于 `feature/engine-lib@4c9ecab0`，自三次复核 `f4d8e652` 后 +18 提交；仅以源码为准）。本轮主要变化：**新增 Web/WASM 平台后端**——`engine/platform/web`（Emscripten/WebGL2，336 行 `web_app.cpp/.h`）+ `apps/web_host` 宿主 + `engine/project/web_build`/`web_dist`（出包）+ `dse build/dist --target web` CLI（+7 单测）；GL 后端经 Emscripten 编到 WebGL2/GLES3.0，能力标志（`supports_ssbo_=false`）自动绕过 Compute/SSBO/Indirect（顶点 SSBO→UBO 降级）；2D/3D 渲染 + 键鼠 + 单指触屏 + 音频(BGM) 均在真机 Chrome/WebGL2 验证（M0–M5）；CMakePresets 增 `web-debug/release(-3d)`。导出目标由 2(Win/Android) 增至 **3(+Web/WASM MVP)**。
> 分支：`feature/engine-lib`
> 数据来源：代码库直接统计（`git ls-files` + `cat | wc -l` + `grep` 验证）+ 实际 ctest / `--gtest_list_tests` 运行结果

---

## 一、DSEngine 当前状态一览

### 1.1 基线数据（基于 `feature/engine-lib` 分支源码统计）

| 维度 | 数值 |
|------|:----:|
| **引擎核心 C++** | ~116,594 行（engine/ 99,806 · 378 文件 + modules/ 16,788 · 85 文件）<br>注：生成的着色器头现为构建期生成（`build_vs2022/`），不再入库；仓内仅 1 个 `.gen.h`；本轮 engine/ +974 行（新增 `engine/platform/web` 336 行等） |
| **编辑器 C++** | ~32,115 行（apps/editor_cpp/ · 139 文件） |
| **其他 apps** | ~1,181 行（standalone / runtime / **dse CLI** / **web_host**(132 行) 等工具宿主；apps/ 合计 33,296 行 · 146 文件） |
| **测试代码** | ~51,870 行（tests/ · 223 文件；本轮 +`web_build_test`/`web_dist_test`） |
| **Lua 脚本** | ~13,658 行（samples/ 10,387 · 68 文件 + examples/ 3,271 · 14 文件） |
| **自有代码合计** | ~21.5 万行（engine+modules+apps+tests+lua samples/examples = 215,418） |
| **第三方依赖** | ~33 个（depends/ 目录，其中 25 个为 git submodule；本轮新增 mimalloc 子模块；旧 LuaSocket 已移除） |
| **渲染后端** | OpenGL 4.5 / Vulkan / D3D11（基础渲染 + 全部后处理 + Compute 功能三端统一；Vulkan 桌面默认 ON）<br>Web：GL 后端经 Emscripten 编到 **WebGL2/GLES3.0**，能力标志（`supports_ssbo_=false`）自动绕过 Compute/SSBO/Indirect（顶点 SSBO→UBO 降级；ESSL300 变体） |
| **平台后端** | `glfw`(桌面 Win/Linux) + `android` + **`web`**(Emscripten/WebGL2，`engine/platform/web` 336 行 + `apps/web_host` 宿主)；导出目标 3：Win / Android APK / **Web/WASM**（`dse build/dist --target web`，`scripts/package_web.ps1`） |
| **内存子系统** | `engine/core/memory/`（~2,146 行）：Memory 门面（分配/释放 + per-tag 追踪/预算/泄漏报告）+ Linear/Frame/Pool/Object 分配器 + StlAllocator/Dse* 容器 + Handle/HandleTable；可选 mimalloc 后端（`DSE_MEM_BACKEND=mimalloc`，默认 system = 零新依赖） |
| **测试体系** | 实跑 ~2,512 个 GoogleTest 用例（3D OFF 构建：unit 1,990 + integration 470 + smoke 52，全绿；上轮实测，本轮未重测 3D OFF）；定义的 TEST 宏 2,920（含 disabled/参数化/3D 门控；本轮 +`web_build`/`web_dist` 单测） |
| **引擎库形态** | 默认**静态库** `DSEngine.lib`（Debug 后缀 `_debug`）；可选 `-DDSE_BUILD_SHARED=ON` 输出 `DSEngine.dll`。第三方库（glfw/freetype/lua/spine/imgui 等）编译进引擎库；网络依赖（GNS/protobuf/libsodium/IXWebSocket/OpenSSL）为可选，默认 OFF（旧 LuaSocket 已移除） |
| **脚本绑定** | Lua（原生 Lua C API + 代码生成 `.gen.cpp`，**非 sol2**；depends/sol2 仅代码中注释提及，未实际用于绑定）+ C#（`GameScripts/DSEngine/Native.gen.cs` 代码生成）；底层 `dse_*` C ABI ~530 个函数（native_api 头定义） |

### 1.2 功能完整度评分

```
核心基础设施          ████████████ 95%  ✅ ServiceLocator/EventBus/JobSystem
内存管理子系统        ███████████░ 85%  ✅ Memory门面+per-tag追踪/预算+Linear/Frame/Pool+Handle/HandleTable+可选mimalloc(默认system零依赖)
ECS 实体组件系统       ████████████ 95%  ✅ EnTT 驱动，102 个 *Component 结构体
2D 游戏玩法           ████████████ 90%  ✅ Sprite/UI/Spine/Tilemap/粒子
2D 物理 (Box2D)       ████████████ 95%  ✅ 功能丰富
3D 物理               ████████████ 88%  ✅ 默认 Jolt（PhysX 可选，二选一）+ 高级模块
3D 高级模块            ████████████ 85%  ✅ 布料/流体/破碎/布娃娃/载具/软体/绳索/浮力
渲染 OpenGL 后端      ████████████ 95%  ✅ PBR+后处理+阴影+延迟渲染+WBOIT+Water+LightShaft
渲染 Vulkan 后端       ███████████░ 88%  ✅ 基础渲染+全部后处理(预编译 SPIR-V)+Compute(DDGI/TressFX/Grass)
渲染 D3D11 后端        ███████████░ 88%  ✅ 基础渲染+全部后处理(HLSL)+Compute(DDGI/TressFX/Grass)
动画系统              ████████████ 85%  ✅ 2D/3D FSM/AnimLayer/IK/2DBlend/BoneAttachment/MorphTarget + C ABI
场景管理              ████████████ 85%  ✅ SubScene/Prefab/异步加载
资产管理              ████████████ 85%  ✅ 异步+LRU+热重载+PAK打包
编辑器                ████████████ 80%  ✅ 16 个面板模块 / 34 个 ImGui 窗口
脚本绑定 (Lua/C#)     ████████████ 92%  ✅ dse_* C ABI ~530 函数 + 代码生成 Lua/C# 绑定（本轮大幅补全子系统/服务层缺口）
测试体系              ████████████ 90%  ✅ ~2,920 个 TEST 宏；3D OFF 构建实跑 ~2,512 例（unit 1,990+integration 470+smoke 52）全绿
实时全局光照 (DDGI)    ███████████░ 85%  ✅ GL/Vulkan/D3D11 三端 Compute 均已移植（GLSL/SPIR-V/HLSL 三套源）
风格化渲染            ██████░░░░░░ 55%  ✅ Toon/Cel+Banding+Outline+VolumetricFog+LightShaft
音频系统              █████████░░░ 78%  ✅ BGM/SFX+3D空间+DSP混音总线+效果链(LPF/HPF/BPF/Delay)+Lua API；Web 端 miniaudio 单线程(NO_THREADING)同步模式真机出声
跨平台                █████████░░░ 75%  ✅ 平台抽象层(PlatformApp: glfw桌面 + android + web(Emscripten/WebGL2)后端)；Win(CI)+Linux+Android 三端均有完整构建路径与端到端验证脚本(engine静态库/Lua运行时/APK打包签名)，engine/ 残留 Win32 引用均 #ifdef 守卫；资产热重载 Win(ReadDirectoryChangesW)+Linux(inotify) 双端；CI 已加 editor/linux/android 作业；❌ macOS/iOS 无平台后端、CI 待额度恢复后首跑
Web/WASM (MVP)        ███████░░░░░ 60%  ✅ Emscripten→WebGL2：M0–M5(帧循环/2D/3D前向/键鼠/单指触屏/音频BGM)真机 Chrome 验证 + dse build/dist --target web 出包；❌ 单线程(无pthreads,DEBT-4)/资源全量预载(DEBT-1)/多指(DEBT-5)/Compute·GPU-Driven(WebGL2 上限)/WebGPU/真实GPU复验/CI build-web 待办
网络                  █████░░░░░░░ 52%  ✅ 传输层 GNS(可靠/非可靠UDP+lanes+加密,三端)+Lua dse.net/dse.http/dse.serialize；🟡 玩法级复制层 MVP(服务器权威Transform同步+spawn/despawn+全量快照+属主输入RPC，回环smoke)；❌ 仍缺快照-delta/预测/插值/AOI/大厅匹配
```

### 1.3 最近提交的增量（`feature/engine-lib` git log）

> 本分支**最新主线**（2026-06-14）为「**Web/WASM 平台后端**：Emscripten/WebGL2 + `apps/web_host` 宿主 + `dse build/dist --target web` + 音频/单指触屏，M0–M5 真机 Chrome 验证」；其前（2026-06-13）为「内存管理子系统 + 玩法级网络复制层 MVP + headless dse CLI / 启动 Splash / Linux 热重载 / CI 扩面 / Lua 绑定补全」；
> 此前主线为「网络层（GNS 集成 Phase 1–6）+ Lua 网络/REST/序列化绑定」，再之前为「引擎静态库化 + `dse_*` C ABI 抽取 + Codegen 驱动的 Lua/C# 绑定 + 动画/Gameplay3D 模块上提」（S1.x 里程碑）。

**最新增量（2026-06-14，自三次复核 `f4d8e652` 后 +18 提交，新增主线，按代码现状核实）：**

| 提交 | 功能 | 复杂度 |
|:----:|------|:------:|
| `b5b535df` / `b0d1ac5e` / `f206c9a1` | `dse build --target web`(+7 单测) · 修 Web BGM 静音(资源管理器 NO_THREADING) · 单指触屏→Input 真机验证 | 🔴 高 |
| `42afb2aa` / `661c63a2` | 真机 WebGL2 出画修复(顶点 SSBO→UBO 降级 + composite 采样器单元绑定) · `#version` 误报降级 + web-release-3d 产物验证 | 🔴 高 |
| `57bd892b` / `f273d00b` | **M5 尽力版 Web 3D 前向渲染**(WebGL2 立方体/光照/相机) + DEBT-6 观感打磨 | 🔴 高 |
| `70d09d08` | Web/WASM **M4 出包** + 资源预载 + 音频 + 后处理采样修正 | 🔴 高 |
| `0392d488` / `da5162b1` | Web **2D 渲染**(ESSL300 + 单线程门控) + 鼠标/键盘输入 + canvas resize/DPI | 🔴 高 |
| `7014d583` | **Emscripten/WebGL2 平台后端 + 引擎链路打通**(`engine/platform/web` + `apps/web_host`，M0–M1) | 🔴 高 |
| `bea4ba5f` | `scripts/bootstrap_windows.ps1` 一键 Windows 开发环境配置 | 🟡 中 |

> Web/WASM 定位为「可演示 demo」的**非主力**分支；GL 后端在 WebGL2 自动绕过 Compute/SSBO/Indirect（平台能力上限），单线程(DEBT-4)、资源全量预载(DEBT-1)、多指触控(DEBT-5)、WebGPU(远期)、真实 GPU 复验、CI `build-web` 均为显式延后项。详见 `docs/roadmap/A_WEB_AND_DOCS_PLAN.md` §2.3b/§2.3c/§2.6/§2.8c-d。

**上一轮增量（2026-06-13，自上次报告 `ec8dcdf3` 后 +88 提交，按代码现状核实）：**

| 提交 | 功能 | 复杂度 |
|:----:|------|:------:|
| `f4d8e652` | 修复 `ShutdownLuaRuntime` 调试器 detach 的 use-after-free 崩溃 | 🟡 中 |
| `cfb5de35` | legacy `Animator3D` 支持 2D blend space + 抽取可复用混合逻辑 | 🟡 中 |
| `994ce113` | **资产热重载 Linux 后端**（inotify）：`FileWatcher` 不再仅 Windows | 🔴 高 |
| `c9c2e0a8` | **玩法级网络复制层 MVP**：spawn/全量快照/属主输入 RPC 回环（`engine/net/replication/`） | 🔴 高 |
| `ffdd294f` | 内存 phase 7：`Handle`/`HandleTable` + 可选 mimalloc 后端（depends/mimalloc 子模块） | 🔴 高 |
| `e024d7b8` | 内存 phase 6：`StlAllocator` + `Dse*` 容器适配器 | 🟡 中 |
| `85acac5e` | 内存 phase 5：per-tag `MemoryBudget` + AssetManager 统一视图 | 🟡 中 |
| `6c445caa` | 内存 phase 4：定长 `PoolAllocator` + 原地 `ObjectPool` | 🟡 中 |
| `f695e1f0` | 内存 phase 3：linear/frame 分配器 + per-thread scratch | 🟡 中 |
| `96bed866` / `828a703e` | 内存 phase 1–2：Memory 门面 + `SystemAllocator` + per-tag 追踪/泄漏报告 | 🟡 中 |
| `b454645c` / `495a961d` / `1b7ae807` | **headless dse CLI** + 端到端加密资源打包 + 新 cpp 项目模板 + 编辑器 BuildGame | 🔴 高 |
| `a0c6fc7e` / `d4652094` / `3de474c6` | 启动 Splash（编辑器/运行时淡入淡出 + 打包游戏 `game.dsmanifest` + Linux/X11 原生） | 🟡 中 |
| `e0ff6664` / `9967947b` | CI 扩面：覆盖 editor、net/http smoke 注册进 ctest、SDK 打包闭环作业 | 🟡 中 |
| `0027c72f` / `0c1993c7` / `a45c7058` | Lua 绑定大幅补全（屏幕拾取/ECS 查询/存档/手柄/场景管理/UUID/FootIK），C ABI ~330→~518，API 文档 100% 覆盖 | 🔴 高 |
| `c6e4b2dd` / `d3cbac9f` / `b8aff6ae` | RHI 统一收口（三端 UBO 填充统一）+ Vulkan 桌面默认 ON + CI 全矩阵 | 🔴 高 |
| `5cf96314` / `ab144229` | 新增 `CMakePresets.json`（VS2022 打开即编译 + 子模块缺失检测，按构建类型组织 Ninja） | 🟡 中 |
| `e2fc95eb` / `fa9f38e2` / `fef746c8` / `8185967b` | 编辑器：崩溃捕获薄封装 + Visual Script 控制流→Lua + 碰撞体视图拖编 + Animation Retargeting 面板 | 🔴 高 |

> 上表为本轮新增主线，更早的「网络传输层（GNS）」与「S1.x 里程碑」增量见下两表（保留备查）。

**网络传输层增量（GNS，此前主线）：**

| 提交 | 功能 | 复杂度 |
|:----:|------|:------:|
| `ec8dcdf3` | `dse.serialize`：自描述二进制序列化 Lua 绑定（编解码任意 Lua 值/嵌套表） | 🟡 中 |
| `6ba5f2fe` | 将 GNS 绑到 Lua（`dse.net`）：游戏 UDP 传输 + 事件回调 + 回环 smoke | 🔴 高 |
| `72494462` | 自动化 Windows OpenSSL 预构建（HTTP 一条命令，无需手动安装） | 🟡 中 |
| `f91612d8` | `dse.http`：异步 HTTP(S) Lua 模块（IXWebSocket + OpenSSL TLS） | 🔴 高 |
| `f90208fc` | net Phase 5：三端 verify `--with-net` 回归全绿 | 🟡 中 |
| `f321f80e` | net Phase 4：固化 `engine/net` 抽象层（lanes/质量/事件）+ 可选 `dse_net_*` C ABI | 🔴 高 |
| `407f1c24` | net Phase 3：Android arm64-v8a 交叉编译打通 GNS（编译+链接） | 🔴 高 |
| `3a5bcdf4` | net Phase 2c：打通 Windows GNS 回环 smoke | 🔴 高 |
| `1c8e36c7` | net Phase 2b：`engine/net` 薄抽象 + GNS 后端 + Linux 回环 smoke | 🔴 高 |
| `b492a793` | net Phase 1：引入 protobuf/libsodium 子模块 + `DSE_ENABLE_NET` 开关（默认 OFF，三端零回归） | 🟡 中 |
| `c2003127` | net 选型：GNS 网络层集成方案 + 引入 GameNetworkingSockets 子模块 (v1.6.0) | 🟢 低 |

> 网络层默认 **OFF**（`DSE_ENABLE_NET` / `DSE_ENABLE_HTTP`），关闭时对现有构建零回归；`dse.serialize` 始终可用。
> 详见 `docs/roadmap/NETWORK_GNS_PROGRESS.md`、`HTTP_LUA_MODULE.md`、`SERIALIZE_LUA_MODULE.md`，API 见 `docs/api/LUA_API.md` §14–17。

**此前 S1.x 里程碑增量：**

| 提交 | 功能 | 复杂度 |
|:----:|------|:------:|
| `0747b2bc` | S1.8/S1.9 里程碑回顾：rendering 拆分迁移、模块收敛、L5 清理完成 | 🟡 中 |
| `27a12acd` | S1.9 phys3d 剩余内容 → L3/L5 `dse_*` C ABI（条件构建标志 setter + 零关联 registry + 测试） | 🔴 高 |
| `e3685b76` | S1.9 animation 模块：2D/3D FSM + AnimLayer + IK + BoneAttachment + MorphTarget C ABI | 🔴 高 |
| `2373c742` | S1.9 gameplay3d batch 3：Weather/SnowCover/Atmosphere/DayNight/VolumetricCloud | 🟡 中 |
| `b1ce6592` | S1.9 gameplay3d batch 2：Ragdoll/SoftBody/Vehicle/Rope/Buoyancy C ABI | 🔴 高 |
| `603c63d9` | S1.9 gameplay3d batch 1：fracture/cloth/fluid C ABI 上提 | 🔴 高 |
| `10b4ce8e` | S1.9 L5 batch 2：rigidbody/CCT/render C ABI 上提 | 🟡 中 |
| `556925ab` | S1.9 L5：physics_3d_raycast 手写 `dse_*` C ABI | 🟡 中 |
| `8d0b1a14` | S1.8-3b PostProcess 72 字段进 binding_defs（字段访问器端生成） | 🟡 中 |
| `4fde67e7` | S1.8-3a 按前缀分拆 `dse_api.gen.cpp` → `dse_api_<prefix>.gen.cpp` | 🟢 低 |
| `c340c638` | 补 Lua/C ABI 绑定并修复 DynamicObstacle dirty 标志 | 🟡 中 |
| `cd812554` | 修 NavMesh SKIP 测试 + 补新功能单元测试 + 扩 Lua/C ABI 绑定 | 🟡 中 |
| `919205a9` | 大世界地形分块 + 树实例化 + NavMesh 分块重烘焙 | 🔴 高 |

> 更早期已合入的渲染特性（按代码现状均存在且三端可用）：DDGI 实时 GI、TressFX 毛发、大型植被(草地)、GPU Driven 渲染(Hi-Z + Indirect Draw)、NavMesh 寻路、资源流式加载、Water/Ocean、WBOIT、延迟渲染、Compute Shader 三端基础管线、音频 DSP 节点图。

---

## 二、对比主流引擎

### 2.1 与商业引擎对比（UE5 / Unity 6）

| 维度 | DSEngine | Unity 6 | Unreal Engine 5 |
|------|:--------:|:-------:|:---------------:|
| **代码量** | ~21.4 万行自研 | ~数千万行 | ~数千万行 |
| **团队规模** | 个人/小团队 | 数千人 · 数十年 | 数千人 · 数十年 |
| **核心架构** | EnTT ECS ✅ 数据驱动 | OOP + DOTS ECS（双轨） | Actor OOP（无 ECS） |
| **多线程 JobSystem** | ✅ 工作窃取+依赖链 | ✅ C# Job System | ✅ TaskGraph |
| **RHI 抽象（多后端）** | ✅ OGL/VK/D3D11 | ✅ SRP | ✅ 多后端 |
| **Render Graph (DAG)** | ✅ 有实现 | ✅ (Unity 6 正式版) | ✅ RDG |
| **PBR 渲染** | ✅ 金属/粗糙度工作流 | ✅ | ✅ |
| **后处理链** | ✅ Bloom/SSAO/TAA/FXAA/DOF/MotionBlur/SSR/ACES/LightShaft/VolumetricFog | ✅ | ✅ |
| **GPU Driven 渲染** | ✅ Hi-Z Cull + Indirect Draw | ✅ GPU Resident Drawer | ✅ Nanite |
| **实时全局光照** | ✅ DDGI Probe（GL/VK/DX11 三端 Compute 均可用） | ✅ HDRP GI | ✅ Lumen |
| **跨平台** | 🟡 Win+Linux+Android + **Web/WASM(MVP)** 构建路径齐备（CI 已加 editor/linux/android 作业，待额度恢复后首跑；Web 经 `dse build --target web` 出包）；macOS/iOS 未支持 | ✅ 20+ 平台 | ✅ 20+ 平台 |
| **资源流式加载** | ✅ StreamingManager（Zone 距离触发 + 异步 IO） | ✅ Addressables | ✅ World Partition |
| **网络模块** | 🟡 传输层完备（GNS UDP + Lua dse.net/http/serialize）+ 玩法级复制层 MVP（Transform 同步/spawn/快照/属主输入 RPC），仍缺 delta/预测/AOI/匹配 | ✅ 完整 | ✅ 完整 |
| **编辑器** | ImGui（功能全但体验一般） | ✅ 可视化极成熟 | ✅ 可视化极成熟 |
| **生态/社区** | ❌ 无 | ✅ Asset Store | ✅ Marketplace |
| **包体大小** | ~10-20MB | ~100MB+ | ~1GB+ |

### 2.2 与开源引擎对比（Godot 4.x）

| 维度 | DSEngine | Godot 4.x | 谁胜出 |
|------|:--------:|:---------:|:------:|
| **代码量** | ~21.4 万行 | ~150 万行 | **DSE**（~7 倍精简） |
| **核心架构** | EnTT ECS ✅ 纯正数据驱动 | Node/Scene 树（非 ECS） | **DSE** |
| **3D 物理** | Jolt（默认）/ PhysX（可选） ✅ 行业标准 | GodotPhysics（自研） | **DSE** |
| **3D 高级模块** | 布料/流体/破碎/布娃娃/载具 | 需第三方插件 | **DSE** |
| **编辑器成熟度** | ImGui 实现，功能完整 | 原生 GUI，极其成熟 | **Godot** |
| **跨平台** | 🟡 Win/Linux/Android + **Web/WASM(MVP)**（CI 已配置作业，待额度恢复首跑）；macOS/iOS 未支持 | ✅ 全平台 | **Godot** |
| **脚本易用性** | C++ + Lua + C# | GDScript（类 Python） | **Godot** |
| **开源生态** | ❌ 闭源 | ✅ MIT 开源 | **Godot** |
| **代码可读性** | ✅ ~21.4 万行可通读全部源码 | 150 万行难掌握全貌 | **DSE** |
| **渲染能力** | PBR/后处理/阴影/DDGI GI | PBR/后处理/阴影 | **平手/DSE 略胜** |
| **2D 能力** | 完整 | 行业最佳之一 | **Godot 略胜** |

### 2.3 核心结论

> **DSEngine 在功能覆盖面 ≈ Godot 4.x**（都有 2D→3D→编辑器→物理→脚本的完整管线，且三端渲染含 Compute/GI），但自研代码量仅为 Godot 的 **~1/7**。
>
> 在**"架构/体量比"**这个独特维度上，DSE 找不到对手——用一个人能通读的 ~21.4 万行代码，实现了通常需要上百万行才能做到的引擎完整度。

---

## 三、DSEngine 的优势与劣势

### ✅ 核心优势

| 优势 | 说明 |
|:----:|------|
| **架构现代** | ECS + JobSystem + RenderGraph + RHI 抽象，与 2026 年行业最佳实践一致，甚至比 UE5（仍以 Actor OOP 为主）更前卫 |
| **功能广度完整** | 从 2D 到 3D、从物理到脚本、从编辑器到运行时的完整管线，90% 的独立游戏可以在上面直接开发 |
| **三端渲染对等** | OpenGL / Vulkan / D3D11 基础渲染、后处理、阴影、Instancing、LOD **以及 Compute 功能（DDGI/TressFX/Grass）三端统一**——每个 compute 着色器都提供 GLSL/SPIR-V/HLSL 三套源，经 `CreateComputeShaderEx` 按后端分派 |
| **C ABI + 双语言脚本** | 底层 `dse_*` C ABI（~530 函数）+ Codegen 自动生成 Lua/C# 绑定，引擎可作为静态库被宿主链接 |
| **统一内存子系统** | `engine/core/memory/`：Memory 门面（per-tag 追踪/预算/泄漏报告）+ Linear/Frame/Pool/Object 分配器 + Handle/HandleTable + 可选 mimalloc 后端（默认 system 零新依赖），为大世界/流式加载提供可观测的内存治理 |
| **代码量极小** | ~21.4 万行自研代码（含 Lua），个人可在数周内通读全部源码——这是 UE/Unity 数千万行完全做不到的 |
| **零外部锁死** | 核心依赖为 EnTT + Jolt/PhysX + GLFW + Lua（原生 C API）等开源库，无订阅制/分成/License 限制 |

### ❌ 核心劣势

| 劣势 | 影响等级 | 说明 |
|:----:|:--------:|------|
| **跨平台缺 Apple / CI 待首跑** | 🟡 中 | 平台抽象层(PlatformApp)已就位，Win+Linux+Android 三端 + **Web/WASM(MVP，Emscripten/WebGL2，`dse build --target web` 出包)** 均有完整构建路径与端到端验证脚本(`scripts/verify_linux_build.sh` 构建 engine 静态库+Lua 运行时 ELF；`scripts/verify_android_apk.ps1` arm64-v8a 交叉编译→NativeActivity→aapt2/zipalign/apksigner 打包签名)，engine/ 残留 Win32 引用均为 #ifdef 守卫的跨平台代码（dlopen/VK surface 等已有 Linux/Android/Apple 分支）；**CI 已加 build-linux / build-android / editor-build-d3d11 作业**（`.github/workflows/ci.yml`，复用上述验证脚本），待 CI 额度恢复后首次运行；macOS/iOS 尚无平台后端 |
| **~~无 GPU Driven 渲染~~** | ✅ 已完成 | Hi-Z Occlusion Culling + Compute 视锥剔除 + Mega VBO/IBO + MultiDrawIndexedIndirect，CPU readback 双保险 |
| **~~实时全局光照仅 GL~~** | ✅ 已完成 | DDGI Probe 的 Compute 着色器已提供 GLSL/SPIR-V/HLSL 三套源，GL/Vulkan/D3D11 三端均可运行 |
| **玩法级网络仅 MVP** | 🟡 中 | 传输层已完备（GNS：可靠/非可靠 UDP + lanes + 加密，Win/Linux 运行、Android 编译；Lua 侧 `dse.net`/`dse.http`/`dse.serialize`）；**玩法级复制层已有 MVP**（`engine/net/replication/`：`ReplicationServer`/`ReplicationClient`，服务器权威 `TransformComponent` 同步 + spawn/despawn + 全量快照 + 属主输入 RPC，`tests/net/repl_smoke.cpp` 回环验证）；但仍缺快照-delta/预测/插值/AOI、大厅/匹配，复杂同步逻辑仍需脚本层自行扩展。编辑器侧另有 ControlServer（IXWebSocket JSON-RPC，自动化/AI 桥接） |
| **~~无资源流式加载~~** | ✅ 已完成 | StreamingManager：Zone 距离触发 + 异步 IO 优先级队列 + 每帧预算 + 资源引用持有 + Lua API |
| **音频系统** | 🟢 中等 | BGM/SFX + 3D 空间 + DSP 混音总线 + LPF/HPF/BPF/Delay 节点图已实现 |
| **风格化渲染** | 🟢 中等 | Toon/Cel+Banding **及 Outline/Edge-Detect 均已实现**（`OutlinePass` + `edge_detect` 后处理，`builtin_passes.cpp:1551`；组件 `outline_enabled`/`outline_thickness`）；待更多 NPR 风格（描边变体/手绘/网点）深化 |
| **编辑器体验** | 🟢 中等 | ImGui 实现功能完整，但 UI/UX 不如原生 GUI 引擎 |
| **~~无 AI/导航~~** | ✅ 已完成 | NavMesh/寻路已集成 Recast/Detour，支持 Bake/Query/Serialize + NavMeshAgent ECS + Lua API |
| **~~TressFX 毛发仅 GL~~** | ✅ 已完成 | 毛发渲染（DrawHairStrands）与物理 Compute 模拟均已移植到 GL/Vulkan/D3D11 三端 |

---

## 四、距离发布第一个 SDK 测试版本还差什么

### 4.1 SDK 现状

当前已有 SDK 基础设施（均经代码核对存在）：

- ✅ `build_fast_sdk.bat` — 一键打包脚本
- ✅ `scripts/package_sdk.ps1` — 打包脚本
- ✅ `scripts/verify_sdk.ps1` — 端到端验证脚本（打包 → 安装 → 消费者编译 → 运行）
- ✅ `examples/sdk_consumer/` — 消费者示例工程
- ✅ `CMakeLists.txt` 支持 `cmake --install` + `COMPONENT sdk`
- ✅ `cmake/DSEngineConfig.cmake.in` — 配置时生成 `DSEngineConfig.cmake` / `DSEngineConfigVersion.cmake`（已见于 build_vs2022/）
- ✅ 公共头文件：`dse.h` / `dse_version.h` / `service_locator.h` 等
- ✅ 第三方头文件分发：glm / EnTT

**`scripts/verify_sdk.ps1` 现已覆盖完整的 profile × config 矩阵：**
```
Minimal (2D：3D/PhysX/Vulkan OFF)  ×  Debug / Release
Full    (3D + Jolt：3D/Jolt ON)    ×  Debug / Release
```

即 SDK 验证已覆盖**最小化 2D** 与 **完整 3D + Jolt** 两套配置，且 Debug/Release 均通过；关键修复：脚本现强制 `DSE_BUILD_SHARED=ON`（install 的 TARGETS/EXPORT/公共头规则都在该开关之下，静态构建产出的 SDK 不可用）。

### 4.2 发布 SDK 测试版必须完成的清单

按优先级从高到低排列：

#### 🔴 P0：必须先修好的（否则 SDK 不可用）

| 序号 | 任务 | 当前状态 | 预估工期 | 说明 |
|:----:|------|:--------:|:--------:|------|
| **1** | **SDK 支持完整配置（含 3D + Jolt）** | ✅ 已完成 | — | `verify_sdk.ps1` 现测试 Minimal(2D) + Full(3D+Jolt) × Debug/Release 四组合，均打包/安装/消费者编译/运行通过 |
| **2** | **~~SSBO → UBO fallback~~** | ✅ 已完成 | — | `gl_shader_manager.cpp` 有 `TransformSSBOToUBO` 动态降级，`light_buffer.cpp` 按 `SupportsSSBO()` 自动切换 |
| **3** | **SDK 头文件边界清理** | ✅ 已完成 | — | install 排除 RHI 三后端实现头（dx11/opengl/vulkan），仅留抽象层（rhi_factory/rhi_device/rhi_types）；经核实无公共头引用被排除头 |
| **4** | **SDK 版本号 + 更新日志** | ✅ 已完成 | — | 新增 `DSEngine_VERSION_PRERELEASE="alpha"`，`DSE_VERSION_STRING` → `0.1.0-alpha`；新增 `CHANGELOG.md` |
| **5** | **Release 构建脚本验证** | ✅ 已完成 | — | `verify_sdk.ps1` Release 配置下 Minimal/Full 两 profile 均通过 |

#### 🟡 P1：应该修好的（否则用户体验差）

| 序号 | 任务 | 当前状态 | 预估工期 | 说明 |
|:----:|:------:|:--------:|:--------:|------|
| **6** | **LTCG 编译优化** | ⬜ 待办 | **0.3 天** | `/GL` + `/LTCG` 降二进制 10-20% |
| **7** | **CMake 残留引用清理** | ⬜ 待办 | **0.2 天** | 核查 CMakeLists 是否还有不存在目录引用 |
| **8** | **消费者示例完善** | ⬜ 待办 | **1 天** | `examples/sdk_consumer/` 补一个完整 Demo（3D 场景 + Lua 脚本 + 物理） |
| **9** | **SDK 文档 / API 参考** | ✅ 已完成 | — | `docs/api/LUA_API.md` + `docs/api/CPP_API.md` + `docs/api/API_GAP_ANALYSIS.md` |
| **10** | **.dll/.lib 发布清单** | ⬜ 待办 | **0.3 天** | 明确 SDK 含哪些产物（VC 运行时、PhysX/Jolt 等）及 Redist 说明 |

#### 🟢 P2：锦上添花的

| 序号 | 任务 | 当前状态 | 预估工期 | 说明 |
|:----:|:------:|:--------:|:------:|------|
| **11** | **纹理 .dds / BCn 直接上传** | ⬜ 待办 | **1 天** | 降 VRAM 30-40% |
| **12** | **默认关闭非必需第三方依赖** | ⬜ 待办 | **0.5 天** | Assimp/Spine 默认 OFF，SDK 包体更小 |
| **13** | **D3D11 作为默认后端之一** | ✅ 已完成 | — | D3D11 已完整实现（含 Compute） |
| **14** | **Vulkan 后端在 SDK 中保持 OFF** | ✅ 设计如此 | — | Vulkan 作为可选能力，SDK 发布版先不默认开启 |

### 4.3 估算总工期

| 优先级 | 待办任务数 | 总工期 |
|:-----:|:-----:|:------:|
| 🔴 P0 | 0 项（全部完成） | — |
| 🟡 P1 | 4 项 | ~1.8 天 |
| 🟢 P2 | 2 项 | ~1.5 天 |
| **合计** | **6 项** | **~3.3 天** |

> **P0 已全部完成：v0.1.0-alpha 的 SDK 现可在本机端到端打包→安装→消费者编译运行（Minimal/Full × Debug/Release 四组合全通过）。剩余 P1/P2 为体验优化项，不阻塞 alpha 交付。**

### 4.4 引擎路线图（按代码现状标注）

```
Phase 1 — 基础能力补全  ✅
  ├── ✅ 音频 DSP 节点图（ma_lpf/hpf/bpf/delay node）
  ├── ✅ Compute Shader 三端基础管线（Create/Dispatch/Barrier/ImageBind + SSBO）
  └── ✅ NavMesh / 寻路（Recast/Detour + NavMeshSystem + NavAgentSystem + Lua API + 序列化）

Phase 2 — 渲染性能突破  ✅
  ├── ✅ Hi-Z Occlusion Culling（Compute Mip Chain + GPU Cull）
  ├── ✅ GPU Driven 渲染 Phase 1（Compute 视锥/Hi-Z 剔除 + SSBO DrawCommands + CPU readback）
  ├── ✅ GPU Driven 渲染 Phase 2（Mega VBO/IBO + MultiDrawIndexedIndirect + RenderStats）
  └── ✅ 资源流式加载（StreamingManager + 距离触发分块 + 异步 IO + Lua API）

Phase 3 — 视觉特性  ✅
  ├── ✅ Terrain 系统（GPU VBO/EBO + Per-Patch LOD + Skirt + Splatmap 4 层混合 + Lua API）
  ├── ✅ 大型植被系统（草地 GPU Instancing + 风场 Compute + LOD + Billboard + Chunk 缓存）
  │      风场 Compute 提供 GLSL/SPIR-V/HLSL 三套源，三端可用
  └── ✅ 毛发系统（TressFX Strand 物理 Compute + Kajiya-Kay 渲染 + LOD）
         渲染 + Compute 物理均已移植 GL/Vulkan/D3D11 三端

Phase 4 — 光照升级  ✅
  ├── ✅ DDGI 数据结构 + ECS 组件 + OctahedralMap 编解码
  ├── ✅ FramePipeline 集成 + DDGIUpdatePass + Compute Shader
  ├── ✅ GL/VK/DX11 PBR shader 间接漫反射采样注入
  └── ✅ Lua API + RSM MRT RenderTarget + RSMRenderPass
         DDGI Update Compute 提供 GLSL/SPIR-V/HLSL 三套源（ddgi_system.cpp），三端可用

Phase 5 — Shader 统一化 + 已知技术债务
  ├── ✅ Shader Unification（dse_shader_compiler 工具链 + GLSL 450 → SPIR-V/GLSL 430/HLSL gen.h）
  ├── ✅ 全部后处理 shader 迁移（GLSL 450 源 → gen.h，GL/DX11/VK 三端验证）
  ├── ✅ engine/ → modules/ 架构依赖违规修复（IBuiltinModules 接口 + AnimationStateMachine 搬迁）
  ├── ✅ DDGI / TressFX / Grass 三组 Compute → VK SPIR-V + DX11 HLSL 移植（已完成）
  ├── 🔄 引擎静态库化 + dse_* C ABI 抽取（feature/engine-lib 进行中：S1.x 里程碑）
  ├── ✅ 跨平台抽象层（PlatformApp 接口 + glfw 桌面 / android 后端；engine/ 残留 Win32 引用均 #ifdef 守卫，Win+Linux+Android 三端构建路径齐备）
  ├── 🟡 Linux/Android 已纳入 CI 配置（`.github/workflows/ci.yml`: build-linux / build-android jobs，复用本地已验证脚本），待 CI 额度恢复后首次运行验证
  └── ⬜ macOS/iOS 平台后端（需 Mac 硬件）

Phase 6 — 网络层（GNS 集成）✅ 传输层 / 🟡 玩法级 MVP
  ├── ✅ GNS 集成 Phase 1–5（Win/Linux 运行 + Android arm64 编译，engine/net 抽象层 + dse_net_* C ABI）
  ├── ✅ Lua dse.net（游戏 UDP 可靠/非可靠 + lanes + 质量 + 事件回调，回环 smoke 全绿）
  ├── ✅ Lua dse.http（异步 HTTP(S)，IXWebSocket + OpenSSL TLS，供 DeepSeek 等 AI NPC）
  ├── ✅ Lua dse.serialize（自描述二进制序列化，配合 dse.net 收发结构化消息）
  ├── 🟡 玩法级复制层 MVP（engine/net/replication：ReplicationServer/Client，服务器权威 Transform 同步 + spawn/despawn + 全量快照 + 属主输入 RPC，repl_smoke 回环验证）
  └── ⬜ 复制层深化（快照-delta/预测/插值/AOI）、大厅/匹配、P2P/ICE、iOS arm64（按需，暂未做）
       注：网络默认 OFF（DSE_ENABLE_NET / DSE_ENABLE_HTTP），关闭零回归

Phase 7 — 工程化 / 基础设施  ✅（2026-06-13 新增主线）
  ├── ✅ 内存管理子系统（engine/core/memory：Memory 门面 + per-tag 追踪/预算/泄漏报告 + Linear/Frame/Pool/Object 分配器 + Handle/HandleTable + 可选 mimalloc 后端，默认 system 零新依赖）
  ├── ✅ headless dse CLI（apps/tools/dse_cli）+ 端到端加密资源打包 + 新 cpp 项目模板 + 编辑器 BuildGame
  ├── ✅ 启动 Splash（engine/platform/splash_screen：编辑器/运行时淡入淡出 + 打包游戏 game.dsmanifest + Linux/X11 原生）
  ├── ✅ 资产热重载 Linux inotify 后端（asset_manager.cpp，原仅 Windows）
  └── ✅ CI 扩面（.github/workflows/ci.yml：build-and-test/editor-build-d3d11/build-linux/build-android + net/http smoke 注册进 ctest）

Phase 8 — Web/WASM 导出  ✅ MVP（2026-06-14 新增主线）
  ├── ✅ Emscripten/WebGL2 平台后端（engine/platform/web：WebApp + emscripten_set_main_loop 帧循环 + 鼠键/单指触屏 + canvas resize/HiDPI）
  ├── ✅ GL 后端 WebGL2/GLES3.0 路径（supports_ssbo_=false 自动绕过 Compute/SSBO/Indirect；顶点 SSBO→UBO 降级；ESSL300 着色器变体）
  ├── ✅ M0–M5：帧循环 / 2D 渲染 / 3D 前向(立方体/光照/相机) / 键鼠 / 音频(BGM, miniaudio NO_THREADING) — 真机 Chrome/WebGL2 验证(debug+release)
  ├── ✅ 出包链路：apps/web_host(index.html/.js/.wasm/.data) + engine/project/web_build+web_dist + dse build/dist --target web(+7 单测) + scripts/package_web.ps1
  └── ⬜ 延后(平台上限/技术债)：pthreads 多线程(DEBT-4) / 资源懒加载(DEBT-1) / 多指触控(DEBT-5) / WebGPU 第4后端 / 真实 GPU 复验 / CI build-web
```

#### 依赖关系（均已落地）

```
Compute Shader 管线 ──┬── ✅ GPU Driven 渲染
                      ├── ✅ 大型植被系统（风场 Compute · 三端）
                      ├── ✅ 毛发模拟（物理 Compute · 三端）
                      └── ✅ 实时全局光照（DDGI Probe · 三端）
✅ Terrain 系统 ─────── ✅ 大型植被系统
✅ Hi-Z Culling ──────── ✅ GPU Driven 渲染
✅ WBOIT ─────────────── ✅ 毛发渲染半透明混合
✅ GPU Instancing ────── ✅ 植被批量渲染
```

---

## 五、一句话总结

> **DSEngine 当前自研代码约 21.4 万行（C++ + Lua），功能覆盖 2D→3D→编辑器→物理→脚本→三端渲染→实时 GI→资源流式加载的完整管线，在"个人/小团队自研引擎"维度里完成度是顶级水准。**
>
> **与 UE5/Unity 的差距不在架构设计（ECS/JobSystem/RenderGraph 与行业最佳实践一致），而在功能覆盖深度——网络已具备完整传输层地基（GNS + Lua 绑定）与玩法级复制层 MVP（服务器权威 Transform 同步/spawn/快照/属主输入 RPC），但仍缺 delta/预测/插值/AOI/匹配；跨平台已具备 Win/Linux/Android + Web/WASM(MVP) 四目标构建路径（CI 已配置作业，待额度恢复首跑），尚缺 macOS/iOS。这些深度需要成千上万倍的人年投入。GPU Driven 渲染、地形植被、资源流式加载、DDGI 实时 GI、TressFX 毛发等核心特性均已实现，且 DDGI/TressFX/Grass 三组 Compute 着色器已统一移植到 GL/Vulkan/D3D11 三端（GLSL/SPIR-V/HLSL 三套源经 `CreateComputeShaderEx` 分派）。与 Godot 相比，DSE 在 ECS 架构和 3D 物理上占优，但在跨平台广度（Godot 全平台 vs DSE 四目标 Win/Linux/Android/Web-MVP，CI 已配置作业待首跑、无 Apple）、编辑器体验和社区生态上落后。**
>
> **当前最大技术债务：① 跨平台覆盖（抽象层已就位、Win/Linux/Android + Web/WASM(MVP) 构建路径齐备且 CI 已配置作业但待首跑，macOS/iOS 未支持）；② 玩法级网络仅 MVP（传输层 GNS + Lua `dse.net`/`dse.http`/`dse.serialize` 完备，`engine/net/replication` 提供 Transform 复制/spawn/快照/属主输入 RPC 的 MVP，仍缺 delta/预测/AOI/匹配）。原"VK/DX11 Compute 移植"债务已清偿。本分支 `feature/engine-lib` 已落地引擎静态库化与 `dse_*` C ABI 抽取（Codegen 驱动 Lua/C# 绑定，~518 个 C ABI 函数），并新增统一内存管理子系统（engine/core/memory）。**
>
> **SDK 测试版 P0 已全部完成：`verify_sdk.ps1` 覆盖 Minimal/Full × Debug/Release 四组合端到端验证、强制共享库构建、头文件边界清理（排除 RHI 后端实现头）、版本号 `0.1.0-alpha` + CHANGELOG。`package_sdk.ps1 -Enable3D` 可产出可被 `find_package(DSEngine)` 消费的发行包。v0.1.0-alpha 已就绪，剩余 P1/P2 为体验优化项。**

---

## 六、生产级就绪度评估（2026-06-13 代码复核）

### 6.1 结论

> **「引擎本体」已达生产级**：面向单平台（Windows）的 2D/3D 独立游戏可直接用于生产——功能广度与 Godot 4.x 相当，测试 ~2,512 例（3D OFF 构建）全绿。代码债务极低：全仓库手写源码中 `TODO`/`FIXME`/`HACK` 均为 **0**（经 2026-06-13 grep 复核，已随 `76e64aa6` 等提交清零；naïve grep 出的 "XXX" 几乎全是生成着色器头里的 HLSL `.xxx` swizzle，非真实标记）。零标记表明工作由 roadmap/文档而非散落注释跟踪，是代码整洁度的正向信号，并不代表功能缺失（缺失体现为缺整块子系统，见 6.3）。
> **「完整商业产品」尚未达生产级**：主要缺玩法级网络深度（仅 MVP）、Apple/console 平台 + CI 首跑看护、崩溃遥测聚合后端等运营工具，以及实战出货验证与生态。详见 6.3。

### 6.2 已具备的生产级能力（均经 grep 核实存在）

| 维度 | 能力 |
|------|------|
| **渲染** | 三后端 GL/VK/DX11 均含 Compute；PBR；**CSM 级联阴影**(`CSM_CASCADES=3`)；**反射探针**(`ReflectionProbeComponent` + Split-Sum IBL，`reflection_probe_system.h`)；SSR；**DDGI 实时 GI**(三端)；GPU Driven(Hi-Z + MultiDrawIndexedIndirect)；后处理链(Bloom/SSAO/TAA/FXAA/DOF/MotionBlur/SSR/ACES/LightShaft/VolumetricFog)；风格化(Toon/Cel/Outline) |
| **内容/系统** | 地形/植被/毛发(TressFX)；2D+3D 粒子；StreamingManager 流式加载；UI 系统 + 序列化；**本地化**(`localization_manager`)；存档/场景序列化 + Prefab；热重载(Win `ReadDirectoryChangesW` + Linux `inotify` + Lua reload) |
| **物理/动画** | Jolt(默认)/Box2D + 布料/流体/破碎/布娃娃/载具等高级模块；动画 FSM/IK/Blend/Morph + **编辑器动画时间轴**(`editor_animation_timeline`) |
| **工具链** | 编辑器多面板 + **撤销/重做**(`editor_entity_snapshot`) + 资产导入器 + Visual Script→Lua + Animation Retargeting + **CPU/内存/渲染性能分析器**；AssetBuilder/PAK/DSSL；**headless dse CLI** + 端到端加密资源打包；Codegen 驱动 Lua/C# 绑定(~518 C ABI) |
| **内存** | Memory 门面 + per-tag 追踪/预算/泄漏报告 + Linear/Frame/Pool/Object 分配器 + Handle/HandleTable + 可选 mimalloc 后端(默认 system 零依赖) |
| **交付** | SDK v0.1.0-alpha 打包(`verify_sdk.ps1` Minimal/Full × Debug/Release 四组合)；启动 Splash(编辑器/运行时/打包游戏/Linux X11)；**Web/WASM 出包**(`dse build/dist --target web` + `scripts/package_web.ps1`)；API 文档(Lua/C++) |

### 6.3 距「完整生产级」仍缺（按优先级）

| 优先级 | 缺口 | 说明 |
|:------:|------|------|
| 🟡 | **玩法级网络（仅 MVP）** | 传输层 GNS 完备；玩法级复制层已有 MVP（`engine/net/replication/`：服务器权威 Transform 同步 + spawn/快照 + 属主输入 RPC），仍缺快照-delta/预测/插值/AOI、大厅/匹配；复杂同步逻辑现需脚本层自实现 |
| 🔴 | **平台广度 / CI** | **Web/WASM 已新增平台后端**(MVP，Emscripten/WebGL2，可演示 demo)；无 macOS/iOS 平台后端（需 Mac 硬件）；Linux/Android/editor 已加入 CI 配置（`ci.yml` build-linux/build-android/editor-build-d3d11，复用本地已验证脚本），待 CI 额度恢复后首次运行验证（Web `build-web` 亦待办） |
| 🟢 | **运维可观测性（崩溃报告）** | ✅ 已实现 `engine/diagnostics` 进程级崩溃捕获：全局异常/信号处理器 + Windows minidump(.dmp) + 可读崩溃报告(.txt，含版本/异常/符号化调用栈/加载模块/面包屑/自定义元数据)，引擎启动默认安装（`DSE_CRASH_HANDLER=0` 可关）。上传做成服务器无关：可选注册 `UploadCallback`（自建端点复用 dse.http，或接 Sentry/BugSplat），引擎不内置后端，零服务器即可用。剩余可选项：聚合面板/趋势遥测（需后端） |
| 🟢 | **跨平台热重载** | ✅ `FileWatcher` 已覆盖 Win（`ReadDirectoryChangesW`）+ Linux（`inotify`，`asset_manager.cpp` `__linux__ && !__ANDROID__` 分支）；仅 Android 与 Apple 尚未接热重载 |
| 🟡 | **终端分发** | 仅 SDK 包 + Android APK，缺面向终端用户的游戏打包/安装器/Redist 清单 |
| 🟢 | **实战验证 / 生态** | 尚无用 DSE 出货的完整游戏(battle-testing)、无社区/插件市场；编辑器 UX 不及原生 GUI 引擎 |

### 6.4 一句话

> **引擎本体已生产级（单平台、功能广度对标 Godot、测试充分；近两轮新增统一内存子系统 + Web/WASM 平台后端 MVP）；商业产品尚未——主要缺网络玩法层深度（已有复制层 MVP，缺 delta/预测/AOI/匹配）、Apple/CI 首跑，以及实战出货与生态验证（崩溃报告、热重载 Linux 化、dse CLI、Web/WASM 出包均已落地，剩余为可选的聚合遥测后端）。**
