# DSEngine 代码现状分析报告（基于 feature/engine-lib 分支，纯代码核实，不依赖文档）

> 统计口径：engine/modules/apps/tools/tests 下 C++ 源码约 **16.3 万行**（不含 third_party/depends），801 个提交，开发周期约 3 个月（2026-03 起）。

---

## 一、各模块完成进度（按代码量与实现深度核实）

| 模块 | 代码量 | 完成度估计 | 核实依据 |
|---|---|---|---|
| 渲染 (engine/render) | ~42,000 行 | **70-80%**，引擎核心资产 | RHI 三后端（GL 6.9k / DX11 7.7k / Vulkan 11.8k 行）；RenderGraph(DAG+自动剔除)；DDGI、光照/反射探针、Clustered 光照、GPU Skinning、Morph、Hair、静态合批、HiZ、58 个内置 shader（bloom/SSAO/DOF/motion blur/大气散射/deferred lighting 等） |
| 编辑器 (apps/editor_cpp) | ~32,000 行，139 个源文件 | **60-70%** | ImGui 编辑器：层级/Inspector/资产浏览器/资产数据库/动画状态机/动画 Retarget/曲线编辑器/Shader Graph/碰撞体编辑/打包构建/崩溃处理/自动保存/多语言/Lua 控制台/远程控制 server。功能面广，深度需实测 |
| 脚本 (engine/scripting) | ~20,000 行 | **75%+** | Lua 运行时 + 调试器 + 45 个 binding 文件（大量 .gen.cpp 来自代码生成，绑定一致性有保障）；C++ 业务热更新（business runtime + dynamic_library）；native_api C 接口层 |
| 3D Gameplay (modules/gameplay_3d) | ~12,300 行 | **60%** | 布料/流体/载具/布娃娃/破坏/绳索/软体/浮力/天气/雪/AI 等系统，单文件体量偏小（如 cloth/fluid/vehicle 各 1 个文件），多为单一实现而非生产级深度 |
| ECS (engine/ecs) | ~4,300 行 | **80%** | 基于 entt（成熟选型），组件覆盖 2D/3D 全领域（地形/植被/天气/破碎/导航等），浮动原点系统（大世界意识） |
| 运行时 (engine/runtime) | ~4,800 行 | **75%** | frame_pipeline(3k 行) + update graph + render shell + runtime context/services，结构清晰 |
| 资产 (engine/assets) | ~5,900 行 | **70%** | AssetManager、pak 读写、流式加载 StreamingManager、资产编译器、Android asset FS、本地化 |
| 物理 (engine/physics) | ~3,600 行 | **70%** | Box2D(2D) + Jolt(默认)/PhysX(可选互斥) 双后端抽象 i_physics3d_system |
| 场景 (engine/scene) | ~4,000 行 | **75%** | Scene/SubScene/八叉树/四叉树/Transform 系统/组件序列化 |
| 音频 (engine/audio) | ~1,650 行 | **50%** | miniaudio + AudioBus；代码中有 USE_FMOD_STUDIO 全局宏，集成方式偏粗糙 |
| 导航 (engine/navigation) | ~900 行 | **60%** | Recast/Detour 封装 + 动态障碍 + 运行时 rebake |
| 网络 (engine/net + http) | ~830 行 | **25-30%，最薄弱** | 仅 GNS transport 单文件(207 行 .cpp) + C API；HTTP client 287 行；CMake 默认 OFF。Lua 绑定以 `#ifdef` 守护，关闭时为空 TU、`dse.{net,http}.available()` 返回 false（public API 已弱化）；回环 smoke（net/http 各 2-3 个）已注册进 ctest，开 `-DDSE_ENABLE_NET/HTTP=ON` 时随 ctest 自检 |
| 输入/平台 | ~1,800 行 | **60%** | GLFW(桌面) + Android；无 macOS/iOS/主机原生层 |
| 工具链 (tools) | ~3,100 行 | **60%** | 自研 DSSL 着色器语言编译器(parser+codegen) + shader_compiler；codegen/mcp_adapter 目录为空 |
| 测试 (tests) | ~47,000 行，210 个测试文件 | **优秀** | unit/integration/smoke 三层；三个 RHI 后端各有 smoke 测试。已在 feature/engine-lib 本地实测：Debug 全量构建 0 错误、3 个 ctest 套件通过（2789 用例通过 / 19 GTEST_SKIP / 0 失败）。CI（Windows）覆盖 Debug/Release/RelWithDebInfo × Vulkan+D3D11+Jolt 三矩阵 + Linux GL + Android APK；新增 editor-build job 编译 editor，net/http opt-in smoke 进 ctest |

**总体完成度：以"可发布 v0.1.0-alpha SDK"为目标约 65-70%。**（CMake 中版本号即 0.1.0-alpha，且有 package_sdk.ps1 / verify_sdk.ps1 等发布脚本，说明发布工程已在收尾阶段）

---

## 二、架构合理性评价

### 做得好的（高于一般自研引擎水准）
1. **分层清晰**：base/core → ecs/scene → render(RHI 抽象) → runtime(frame_pipeline) → modules(gameplay) → apps（editor/runtime/standalone），依赖方向正确，引擎核可独立为 DLL（DSE_BUILD_SHARED + dse_export.h）。
2. **RHI 三后端统一抽象**：device/command buffer/draw executor/pipeline state manager/resource manager 各后端结构对称，且每个后端有独立 smoke 测试 —— 这是多数自研引擎做不到的纪律。
3. **RenderGraph（DAG 声明式 Pass + 拓扑排序 + 自动剔除）**：与 UE RDG / Frostbite FrameGraph 同一代设计思想，选型先进。
4. **代码生成驱动绑定**：Lua binding 与 native C API 大量 .gen.cpp，避免手写绑定漂移，这是工程成熟度的标志。
5. **第三方选型务实**：entt/Jolt/Box2D/Recast/miniaudio/sol2/glm/assimp/spirv-cross —— 全是行业标准件，没有重复造低价值轮子；自研集中在渲染管线、DSSL、编辑器等差异化部分。
6. **测试文化**：测试代码 4.7 万行 ≈ 引擎代码的 1/3，加 CI 多矩阵，远超典型个人/小团队自研引擎。
7. **功能开关粒度好**：2D/3D/Spine/Vulkan/D3D11/Lua/NavMesh/Net/HTTP/PCH/Unity-build 全部 CMake option 化，物理后端互斥校验。
8. **服务入口已收敛**：运行时服务统一走 `ServiceLocator` + `EngineInstance` 生命周期管理；`World::Instance()` 已是委托到 `ServiceLocator::Get<World>()` 的 `@deprecated` 兼容垫片（未注册时抛异常、不再 auto-create），生产代码零调用（仅测试/文档引用）——旧单例遗留已基本清退。

### 隐患与不合理处
1. **per-backend DrawExecutor 巨型化**：vulkan_draw_executor.cpp 4,148 行、dx11 2,609 行、GL 拆 3 个文件 —— 大量高层绘制逻辑沉到各后端重复实现，说明 RHI 抽象层级偏低（render 目录里也确实存在 RHI 统一的未完成任务清单）。这是当前架构最大的债，后端越多重复越多。
2. **builtin_passes.cpp 3,293 行 / frame_pipeline.cpp 3,031 行**：frame_pipeline.h 头文件 include 了几乎所有子系统，耦合面大、编译传染强（虽有 PCH 缓解）。但需澄清：frame_pipeline 偏厚的部分是**非-pass 职责**（子系统 Init/Shutdown、每帧全局状态准备如光源收集/cluster/probe SH/风场、渲染线程、编辑器集成、RT 管理、快照），**不是**渲染 pass 流程控制硬编码。
3. **passes 已是数据驱动，但物理分文件偏粗**：实际有 `RenderPipelineRegistry` + `RenderPipelineProfile`（ForwardPlusDefault/Lite/DebugDepth），36 个 pass 均实现 `IRenderPass`，`BuildRenderGraphInternal` 从 profile 遍历 → `registry.Create` → 模块 `RegisterRenderPasses` → 统一 `Setup(graph)` → `Compile()`；新 pass/后处理已走 RenderGraph+IRenderPass，**模块化在管线层已兑现**。残留问题只是这 36 个 pass 物理上集中在 `builtin_passes.cpp` 一个文件（拆分是可读性优化，非架构债）。
4. **gameplay_3d 子系统偏"演示级"**：cloth/fluid/vehicle 等都是单文件系统，作为 feature 列表很漂亮，但生产深度不足；建议首发收敛范围而非继续铺宽度。
5. **全局宏污染**：顶层 CMake add_definitions(USE_FMOD_STUDIO、GLM_FORCE_SWIZZLE 等) 全局生效，应下沉到目标级 target_compile_definitions。
6. **网络层与引擎其余部分不成比例**：800 行 vs 渲染 4.2 万行，若 v1 需要联机能力，这是硬缺口。

**结论：架构骨架是对的、现代的（ECS + RHI + RenderGraph + 模块化 + 代码生成），主要问题是渲染高层逻辑的"收口"未完成，属于可演进的债务而非推倒重来的错误。**

---

## 三、作为自研引擎的整体评价

- 3 个月、16 万行、覆盖渲染/物理/动画/脚本/编辑器/资产管线/测试/CI 的完整纵深，**完成速度和工程纪律都明显高于典型个人自研引擎**（多数个人引擎停留在 forward renderer + 无编辑器 + 无测试阶段）。
- 技术雷达上的亮点：DDGI 实时 GI、Clustered Lighting、GPU-driven 痕迹（indirect draw）、HiZ、GPU skinning、自研 DSSL 着色器语言 + 编译器、C++ 热更新业务层、浮动原点大世界 —— 这些是"现代引擎"的标志特性，不是教学引擎配置。
- 定位上更像 **"Windows 优先、Lua+C++ 双脚本、带完整 ImGui 编辑器的 3D/2D 通用引擎"**，体量和能力大致相当于 **早期 Godot 2.x / Cocos2d-x 转 3D 初期** 的水平，但渲染技术栈比同体量引擎更新一代。
- 最大的风险不是技术，而是**宽度 vs 深度**：feature 面已经铺得非常宽（毛发/流体/破坏/载具/天气/雪……），每个再往生产级打磨都需要数倍投入。

---

## 四、距离第一个版本（v0.1.0-alpha SDK）还差什么

按"能交付给第三方做出一个小游戏"的标准（2026-06-11 已按代码重新核实，下列状态为核实后）：

**已完成 / 不再阻塞（核实纠偏）**
- ~~**RHI 统一收尾**~~ **已完成**：`engine/render/RHI_UNIFICATION_TASKS.md` 7 项全部 `[x]`，`docs/roadmap/rhi_unification_closeout_plan.md` 阶段 A/B 均收口；UBO 填充逻辑统一到 `engine/render/rhi/draw_executor_common.h` 的 `Prepare*` 共用函数、跨端类型收敛到 `ubo_types.h`。per-backend draw_executor 文件仍大（体量问题），但当年「跨端重复逻辑」债已清，**不再是 P0**。

**P0（发布阻塞）**
1. **Vulkan 后端稳定性转正或明确降级**：勘误——Vulkan 在桌面默认 **ON**（非 Android），CI Windows 三个配置（Debug/Release/RelWithDebInfo）均开 Vulkan 构建；所以问题不是“默认关/未启用”，而是运行时稳定性与 GL/DX11 是否同级（需画面/回归验证）。发布前要么达到同级稳定，要么 v0.1 明确只发 GL+DX11。
2. **端到端标杆 demo 收尾**：examples/KF_Framework 能跑（有 `script/main.lua` + `cooked/` + `screenshots/`），但 ALIGNMENT_TASKS / NEXT_SESSION_TASKS / TROUBLESHOOTING / TASK_UI_REPLICATION 仍在，骑乘攻击动作/移动手感/Mutant 贴图&血条等 1:1 复刻未完——SDK 需要一个「打磨完成、可展示」的标杆 demo。
3. **SDK 打包验证闭环进 CI**：`scripts/package_sdk.ps1` / `scripts/verify_sdk.ps1` / `examples/sdk_consumer`（CMakeLists+main.cpp）均已具备，但 `.github/workflows/ci.yml` **未集成**「打包→第三方消费→运行」，需固化为 CI 常态防回归。

**P1（alpha 可缺、beta 必须）**
4. 网络层从 800 行原型补到可用（房间/同步/序列化策略），或 v0.1 明确声明"无联机"。
5. 音频收口：实际音频走 miniaudio；`CMakeLists.txt:37` 的 `add_definitions(-D USE_FMOD_STUDIO)` 是**全局定义但全仓零引用的死宏**（grep 无任何 .cpp/.h 使用）。整改 = 删死宏 + 确认 miniaudio 收口，工作量极小、低风险。
6. 编辑器稳定性专项：139 个面板源文件功能广但需要 crash/undo/资产损坏等鲁棒性扫雷（已有 crash handler 与 autosave 是好基础）。
7. 性能基准：`examples/stress_test`（`run_benchmark.py` + `benchmark_results.csv`）**已具备并有数据**。当前数据暴露异常——gpu-driven 5000 实体下 **DX11 ~6 fps（帧时 160ms）vs OpenGL ~34 fps**（需先复核是否 CI 软件渲染/无独显所致）。建议：查 DX11 gpu-driven 路径性能 + 把 benchmark 固化为 CI 回归门槛。

**P2（1.0 之前）**
8. 平台扩展（macOS/iOS 缺位，Android 已有但需真机验证闭环）。
9. builtin_passes.cpp 按 pass 拆分到独立文件（可读性优化；管线已是 registry+profile+IRenderPass 数据驱动，非架构债）。

---

## 五、与主流/自研引擎对比

| 维度 | DSEngine | Unity | Unreal | Godot | 典型个人自研 |
|---|---|---|---|---|---|
| 渲染架构 | RenderGraph+RHI 三后端+DDGI/Clustered，**思想与 UE5 同代** | SRP | RDG/Nanite/Lumen | 4.x 才有 RD 抽象 | 通常单后端 forward |
| ECS | entt，纯 ECS | GameObject+DOTS 并存 | Actor 组件 | Node 树 | 多数无 |
| 脚本 | Lua + C++ 热更 + 代码生成绑定 | C# | C++/BP | GDScript/C# | 通常无或裸 Lua |
| 编辑器 | ImGui 全家桶 139 文件，含动画状态机/ShaderGraph/打包 | 成熟 | 成熟 | 成熟 | 通常没有 |
| 物理 | Jolt/PhysX 双后端可切 | PhysX | Chaos | Godot Physics/Jolt | 通常单一 |
| 测试/CI | 4.7 万行测试+三矩阵 CI，**超过 Godot 早期** | 内部 | 内部 | 较弱 | 几乎没有 |
| 平台 | Win(GL/DX11/VK)+Android | 全平台 | 全平台 | 全平台 | 单平台 |
| 生态/文档/资产商店 | 无 | 巨大 | 巨大 | 大 | 无 |
| 网络 | 原型 | NGO/Fishnet 生态 | 内置复制 | 内置高层 API | 无 |

**一句话定位**：DSEngine 已经越过了"玩具引擎"和"渲染 demo"两个阶段，处于"垂直自研引擎发布前夜"——技术栈现代、工程纪律好、宽度惊人；首发前的关键动作是**收口（RHI 统一已完成；剩 Vulkan 画面验证、标杆 demo 收尾、SDK 打包闭环进 CI）而不是继续加 feature**。对标商业引擎缺的是生态与打磨深度，这正常且不应是 v0.1 的目标。

## 六、2026-06-11 复核与本次改动（对齐代码现状）

针对“FramePipeline 变薄 / 服务入口统一 / 默认构建=默认验收 / net+http”四点做了代码核实，结论：

- **FramePipeline 变薄**：基本已做到，不存在“新 pass 硬编进 frame 流程”。pass 由 `RenderPipelineRegistry` + profile 数据驱动，36 个 pass 均为 `IRenderPass`。无需为此项做架构改动。
- **服务入口统一**：基本已完成。`World::Instance()` 仅为 `ServiceLocator` 委托垫片，生产零调用。无需改动（可选纯清理）。
- **默认构建=默认验收**：默认 preset 已含 editor+Vulkan+D3D11+gtests（含 integration）；原缺口是 **CI 不编译 editor**（`DSE_BUILD_EDITOR=OFF`）。已修复。
- **net/http**：public API 已通过 `#ifdef` 弱化；smoke 存在但未进默认验收/ctest。已将回环 smoke 注册进 ctest（opt-in）。

**本次提交（直推 feature/engine-lib）**：
1. `.github/workflows/ci.yml` 新增 `editor-build` job（`DSE_BUILD_EDITOR=ON` + D3D11、关 Vulkan，编译 `dse_editor_cpp`），使 CI 与默认 preset 对齐；本地同配置实测构建通过。
2. 根 `CMakeLists.txt` 在 `enable_testing()` 后用 `if(TARGET ...)` 守护注册 5 个回环 smoke（`dse_http_smoke` / `dse_http_lua_smoke` / `dse_net_smoke` / `dse_net_capi_smoke` / `dse_net_lua_smoke`）进 ctest（labels `smoke;net_http`）；对默认构建/CI 零影响。

未做（避免过度设计）：FramePipeline/builtin_passes 拆分、删 `World::Instance()` 垫片、net 默认开 smoke（会拉 GNS/webrtc 巨依赖）均为可选低优先项，维持现状。

---

*备注：分析全部来自源码本身。构建健康度已在 feature/engine-lib 本地实测验证（VS2022 + MSVC 14.44 + CMake 4.3.3 + Ninja）：Debug 全量构建 0 错误、`ctest -L gtest` 3 套件通过（2789 通过 / 19 跳过 / 0 失败）；editor（`dse_editor_cpp`，D3D11+OpenGL）本地构建通过。*
