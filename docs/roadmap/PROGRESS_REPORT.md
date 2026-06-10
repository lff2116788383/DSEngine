# DSEngine 当前进度 · 对比主流引擎 · 发布 SDK 差距分析

> 生成日期：2026-06-10（与代码现状重新核对，仅以源码为准）
> 分支：`feature/engine-lib`
> 数据来源：代码库直接统计（`git ls-files` + `wc -l` + `grep` 验证）+ 实际 ctest 运行结果

---

## 一、DSEngine 当前状态一览

### 1.1 基线数据（基于 `feature/engine-lib` 分支源码统计）

| 维度 | 数值 |
|------|:----:|
| **引擎核心 C++** | ~107,582 行（engine/ 90,985 · 326 文件 + modules/ 16,597 · 84 文件）<br>注：engine/ 含 ~5,304 行生成的 `.gen.h` 着色器头 |
| **编辑器 C++** | ~30,664 行（apps/editor_cpp/ · 126 文件） |
| **其他 apps** | ~364 行（standalone / runtime / 工具宿主，apps/ 合计 31,028 行） |
| **测试代码** | ~45,067 行（tests/ · 204 文件，其中 gtest 203 个源文件） |
| **Lua 脚本** | ~13,650 行（samples/ 10,387 · 68 文件 + examples/ 3,263 · 14 文件） |
| **自有代码合计** | ~197,000 行（engine+modules+apps+tests+lua samples/examples = 197,327） |
| **第三方依赖** | ~29 个（depends/ 目录，其中 23 个为 git submodule；旧 LuaSocket 子模块已移除） |
| **渲染后端** | OpenGL 4.5 / Vulkan / D3D11（基础渲染 + 全部后处理 + Compute 功能三端统一） |
| **测试体系** | ~2,600 个测试用例实跑（unit 2,048 + integration 510 + smoke 42；定义的 TEST 宏 2,780，含 disabled/参数化） |
| **引擎库形态** | 默认**静态库** `DSEngine.lib`（Debug 后缀 `_debug`）；可选 `-DDSE_BUILD_SHARED=ON` 输出 `DSEngine.dll`。第三方库（glfw/freetype/lua/spine/imgui 等）编译进引擎库；网络依赖（GNS/protobuf/libsodium/IXWebSocket/OpenSSL）为可选，默认 OFF（旧 LuaSocket 已移除） |
| **脚本绑定** | Lua（原生 Lua C API + 代码生成 `.gen.cpp`，**非 sol2**）+ C#（`GameScripts/DSEngine/Native.gen.cs` 代码生成）；底层 `dse_*` C ABI ~330 个函数 |

### 1.2 功能完整度评分

```
核心基础设施          ████████████ 95%  ✅ ServiceLocator/EventBus/JobSystem
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
脚本绑定 (Lua/C#)     ████████████ 90%  ✅ dse_* C ABI ~330 函数 + 代码生成 Lua/C# 绑定
测试体系              ████████████ 90%  ✅ 203 个 gtest 文件，~2,600 个测试用例（实跑全绿）
实时全局光照 (DDGI)    ███████████░ 85%  ✅ GL/Vulkan/D3D11 三端 Compute 均已移植（GLSL/SPIR-V/HLSL 三套源）
风格化渲染            ██████░░░░░░ 55%  ✅ Toon/Cel+Banding+Outline+VolumetricFog+LightShaft
音频系统              █████████░░░ 78%  ✅ BGM/SFX+3D空间+DSP混音总线+效果链(LPF/HPF/BPF/Delay)+Lua API
跨平台                ██░░░░░░░░░░ 15%  ❌ 仅 Windows（engine/ 12 文件 33 处 Win32 硬编码）
网络                  ████░░░░░░░░ 40%  ✅ 传输层 GNS(可靠/非可靠UDP+lanes+加密,三端)+Lua dse.net/dse.http/dse.serialize；❌ 无玩法级复制/同步层
```

### 1.3 最近提交的增量（`feature/engine-lib` git log）

> 本分支**最新主线**为「网络层（GNS 集成 Phase 1–6）+ Lua 网络/REST/序列化绑定」；
> 此前主线为「引擎静态库化 + `dse_*` C ABI 抽取 + Codegen 驱动的 Lua/C# 绑定 + 动画/Gameplay3D 模块上提」（S1.x 里程碑）。

**网络层增量（最新，自顶向下为最近提交）：**

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
| **代码量** | ~19.7 万行自研 | ~数千万行 | ~数千万行 |
| **团队规模** | 个人/小团队 | 数千人 · 数十年 | 数千人 · 数十年 |
| **核心架构** | EnTT ECS ✅ 数据驱动 | OOP + DOTS ECS（双轨） | Actor OOP（无 ECS） |
| **多线程 JobSystem** | ✅ 工作窃取+依赖链 | ✅ C# Job System | ✅ TaskGraph |
| **RHI 抽象（多后端）** | ✅ OGL/VK/D3D11 | ✅ SRP | ✅ 多后端 |
| **Render Graph (DAG)** | ✅ 有实现 | ✅ (Unity 6 正式版) | ✅ RDG |
| **PBR 渲染** | ✅ 金属/粗糙度工作流 | ✅ | ✅ |
| **后处理链** | ✅ Bloom/SSAO/TAA/FXAA/DOF/MotionBlur/SSR/ACES/LightShaft/VolumetricFog | ✅ | ✅ |
| **GPU Driven 渲染** | ✅ Hi-Z Cull + Indirect Draw | ✅ GPU Resident Drawer | ✅ Nanite |
| **实时全局光照** | ✅ DDGI Probe（GL/VK/DX11 三端 Compute 均可用） | ✅ HDRP GI | ✅ Lumen |
| **跨平台** | ❌ 仅 Windows | ✅ 20+ 平台 | ✅ 20+ 平台 |
| **资源流式加载** | ✅ StreamingManager（Zone 距离触发 + 异步 IO） | ✅ Addressables | ✅ World Partition |
| **网络模块** | 🟡 传输层完备（GNS UDP + Lua dse.net/http/serialize），无玩法级复制/同步层 | ✅ 完整 | ✅ 完整 |
| **编辑器** | ImGui（功能全但体验一般） | ✅ 可视化极成熟 | ✅ 可视化极成熟 |
| **生态/社区** | ❌ 无 | ✅ Asset Store | ✅ Marketplace |
| **包体大小** | ~10-20MB | ~100MB+ | ~1GB+ |

### 2.2 与开源引擎对比（Godot 4.x）

| 维度 | DSEngine | Godot 4.x | 谁胜出 |
|------|:--------:|:---------:|:------:|
| **代码量** | ~19.7 万行 | ~150 万行 | **DSE**（~7.6 倍精简） |
| **核心架构** | EnTT ECS ✅ 纯正数据驱动 | Node/Scene 树（非 ECS） | **DSE** |
| **3D 物理** | Jolt（默认）/ PhysX（可选） ✅ 行业标准 | GodotPhysics（自研） | **DSE** |
| **3D 高级模块** | 布料/流体/破碎/布娃娃/载具 | 需第三方插件 | **DSE** |
| **编辑器成熟度** | ImGui 实现，功能完整 | 原生 GUI，极其成熟 | **Godot** |
| **跨平台** | ❌ 仅 Windows | ✅ 全平台 | **Godot** |
| **脚本易用性** | C++ + Lua + C# | GDScript（类 Python） | **Godot** |
| **开源生态** | ❌ 闭源 | ✅ MIT 开源 | **Godot** |
| **代码可读性** | ✅ ~19.7 万行可通读全部源码 | 150 万行难掌握全貌 | **DSE** |
| **渲染能力** | PBR/后处理/阴影/DDGI GI | PBR/后处理/阴影 | **平手/DSE 略胜** |
| **2D 能力** | 完整 | 行业最佳之一 | **Godot 略胜** |

### 2.3 核心结论

> **DSEngine 在功能覆盖面 ≈ Godot 4.x**（都有 2D→3D→编辑器→物理→脚本的完整管线，且三端渲染含 Compute/GI），但自研代码量仅为 Godot 的 **~1/7.6**。
>
> 在**"架构/体量比"**这个独特维度上，DSE 找不到对手——用一个人能通读的 ~19.7 万行代码，实现了通常需要上百万行才能做到的引擎完整度。

---

## 三、DSEngine 的优势与劣势

### ✅ 核心优势

| 优势 | 说明 |
|:----:|------|
| **架构现代** | ECS + JobSystem + RenderGraph + RHI 抽象，与 2026 年行业最佳实践一致，甚至比 UE5（仍以 Actor OOP 为主）更前卫 |
| **功能广度完整** | 从 2D 到 3D、从物理到脚本、从编辑器到运行时的完整管线，90% 的独立游戏可以在上面直接开发 |
| **三端渲染对等** | OpenGL / Vulkan / D3D11 基础渲染、后处理、阴影、Instancing、LOD **以及 Compute 功能（DDGI/TressFX/Grass）三端统一**——每个 compute 着色器都提供 GLSL/SPIR-V/HLSL 三套源，经 `CreateComputeShaderEx` 按后端分派 |
| **C ABI + 双语言脚本** | 底层 `dse_*` C ABI（~330 函数）+ Codegen 自动生成 Lua/C# 绑定，引擎可作为静态库被宿主链接 |
| **代码量极小** | ~19.7 万行自研代码（含 Lua），个人可在数周内通读全部源码——这是 UE/Unity 数千万行完全做不到的 |
| **零外部锁死** | 核心依赖为 EnTT + Jolt/PhysX + GLFW + Lua（原生 C API）等开源库，无订阅制/分成/License 限制 |

### ❌ 核心劣势

| 劣势 | 影响等级 | 说明 |
|:----:|:--------:|------|
| **仅 Windows** | 🔴 致命 | engine/ 仍有 12 文件 33 处 `_WIN32`/`<windows.h>` 硬编码（modules/ 基本为 0）；无法部署到移动端/主机，直接限制了 95% 的游戏分发渠道 |
| **~~无 GPU Driven 渲染~~** | ✅ 已完成 | Hi-Z Occlusion Culling + Compute 视锥剔除 + Mega VBO/IBO + MultiDrawIndexedIndirect，CPU readback 双保险 |
| **~~实时全局光照仅 GL~~** | ✅ 已完成 | DDGI Probe 的 Compute 着色器已提供 GLSL/SPIR-V/HLSL 三套源，GL/Vulkan/D3D11 三端均可运行 |
| **玩法级网络缺失** | 🟡 中 | 传输层已完备（GNS：可靠/非可靠 UDP + lanes + 加密，Win/Linux 运行、Android 编译；Lua 侧 `dse.net`/`dse.http`/`dse.serialize`），但缺玩法级复制/快照-delta/预测/AOI、大厅/匹配；同步逻辑需脚本层自行实现。编辑器侧另有 ControlServer（IXWebSocket JSON-RPC，自动化/AI 桥接） |
| **~~无资源流式加载~~** | ✅ 已完成 | StreamingManager：Zone 距离触发 + 异步 IO 优先级队列 + 每帧预算 + 资源引用持有 + Lua API |
| **音频系统** | 🟢 中等 | BGM/SFX + 3D 空间 + DSP 混音总线 + LPF/HPF/BPF/Delay 节点图已实现 |
| **风格化渲染** | 🟢 中等 | Toon/Cel+Banding 已完成，待 Outline/Edge Detection 深化 |
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

**但当前 `scripts/verify_sdk.ps1` 的测试配置是：**
```
-DDSE_ENABLE_PHYSX=OFF
-DDSE_ENABLE_3D=OFF
```

也就是说当前的 SDK 验证仅覆盖**最小化 2D 配置**，3D 路径未在 SDK 验证流程中覆盖。

### 4.2 发布 SDK 测试版必须完成的清单

按优先级从高到低排列：

#### 🔴 P0：必须先修好的（否则 SDK 不可用）

| 序号 | 任务 | 当前状态 | 预估工期 | 说明 |
|:----:|------|:--------:|:--------:|------|
| **1** | **SDK 支持完整配置（含 3D + Jolt）** | ⬜ 待办 | **1 天** | `verify_sdk.ps1` 当前关闭 3D。需验证 3D + Jolt 默认配置下打包/消费者编译/运行正常 |
| **2** | **~~SSBO → UBO fallback~~** | ✅ 已完成 | — | `gl_shader_manager.cpp` 有 `TransformSSBOToUBO` 动态降级，`light_buffer.cpp` 按 `SupportsSSBO()` 自动切换 |
| **3** | **SDK 头文件边界清理** | ⬜ 待办 | **0.5 天** | 确保 `cmake --install` 只安装公共 API 头文件，不暴露内部实现 |
| **4** | **SDK 版本号 + 更新日志** | ⬜ 待办 | **0.3 天** | `dse_version.h` 存在，首次 SDK 发布建议 v0.1.0-alpha |
| **5** | **Release 构建脚本验证** | ⬜ 待办 | **0.3 天** | 确保 Release 配置下 SDK 构建通过 |

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
| 🔴 P0 | 4 项 | ~2.1 天 |
| 🟡 P1 | 4 项 | ~1.8 天 |
| 🟢 P2 | 2 项 | ~1.5 天 |
| **合计** | **10 项** | **~5.4 天** |

> **如果全力投入，最快约 1 周可发布第一个 SDK alpha 版本；只做 P0 约 2-3 天即可。**

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
  └── ⬜ 跨平台抽象层（engine/ 12 文件 33 处 Win32 硬编码，modules/ ~0）

Phase 6 — 网络层（GNS 集成）✅ 传输层 / ⬜ 玩法级
  ├── ✅ GNS 集成 Phase 1–5（Win/Linux 运行 + Android arm64 编译，engine/net 抽象层 + dse_net_* C ABI）
  ├── ✅ Lua dse.net（游戏 UDP 可靠/非可靠 + lanes + 质量 + 事件回调，回环 smoke 全绿）
  ├── ✅ Lua dse.http（异步 HTTP(S)，IXWebSocket + OpenSSL TLS，供 DeepSeek 等 AI NPC）
  ├── ✅ Lua dse.serialize（自描述二进制序列化，配合 dse.net 收发结构化消息）
  └── ⬜ 玩法级复制/同步层（状态复制/快照-delta/预测/插值/AOI）、P2P/ICE、iOS arm64（按需，暂未做）
       注：网络默认 OFF（DSE_ENABLE_NET / DSE_ENABLE_HTTP），关闭零回归
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

> **DSEngine 当前自研代码约 19.7 万行（C++ + Lua），功能覆盖 2D→3D→编辑器→物理→脚本→三端渲染→实时 GI→资源流式加载的完整管线，在"个人/小团队自研引擎"维度里完成度是顶级水准。**
>
> **与 UE5/Unity 的差距不在架构设计（ECS/JobSystem/RenderGraph 与行业最佳实践一致），而在功能覆盖深度——网络已具备完整传输层地基（GNS + Lua 绑定），但缺玩法级复制/同步层；跨平台仍以 Windows 为主。这些深度需要成千上万倍的人年投入。GPU Driven 渲染、地形植被、资源流式加载、DDGI 实时 GI、TressFX 毛发等核心特性均已实现，且 DDGI/TressFX/Grass 三组 Compute 着色器已统一移植到 GL/Vulkan/D3D11 三端（GLSL/SPIR-V/HLSL 三套源经 `CreateComputeShaderEx` 分派）。与 Godot 相比，DSE 在 ECS 架构和 3D 物理上占优，但在跨平台、编辑器体验和社区生态上落后。**
>
> **当前最大技术债务：① 跨平台抽象层（engine/ 12 文件 33 处 Win32 硬编码）；② 玩法级网络缺失（传输层 GNS + Lua `dse.net`/`dse.http`/`dse.serialize` 已完备，但无复制/同步/预测/AOI）。原"VK/DX11 Compute 移植"债务已清偿。本分支 `feature/engine-lib` 正在推进引擎静态库化与 `dse_*` C ABI 抽取（Codegen 驱动 Lua/C# 绑定，~330 个 C ABI 函数）。**
>
> **SDK 测试版已具备打包脚本与验证框架，但 `verify_sdk.ps1` 仅覆盖 2D 最小配置。SSBO→UBO fallback 已实现。补齐 P0（含 3D+Jolt 完整配置验证）约需 2-3 天，完整的 v0.1.0-alpha 可在 1 周内就绪。**
