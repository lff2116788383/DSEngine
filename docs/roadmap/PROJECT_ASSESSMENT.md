# DSEngine 项目评估报告（完成度 · 架构 · 竞争力 · 生态位）

> 评估日期：2026-06-14 · 分支：`feature/engine-lib@75479b8b`
> 评估依据：**直接实读源代码**（`git ls-files` + `grep` + 逐子系统翻代码核实），辅以已同步至代码现状的 `PROGRESS_REPORT.md`；不基于市场宣传或愿景文档。
> 目标读者：引擎维护者 / 决策者 / 潜在采用方
> 一句话定调：**引擎本体已是"准生产级的现代 C++ 3D 引擎"，技术雄心与工程质量在国产独立引擎里属第一梯队；但它现在是"引擎"，还不是"产品"——真正短板不在技术，而在生态 / 采用力 / 出货验证。**

---

## 〇、评分总览

| 维度 | 评级 | 说明 |
|---|:---:|---|
| 引擎本体（技术） | **A-** | 功能面对标 Godot 一档，工程质量高（零 TODO、强测试、清晰分层）。 |
| 架构合理性 | **A-** | 分层单向、真三后端 RHI、现代渲染范式齐全；隐忧是广度>深度、单人维护风险。 |
| 跨平台广度 | **B** | Win/Linux/Android + Web/WASM(MVP) 四目标；缺 macOS/iOS。 |
| 商业产品成熟度 | **C+** | 无出货案例、无资产/插件生态、文档教程稀薄、CI 未首跑、网络仅复制层 MVP。 |
| 国产独游竞争力 | **技术 A- / 生态 C** | 技术上能打，生态上还没上场。 |
| 综合潜力 | **A** | 一台被"严重低估自身采用力短板"的好引擎。 |

> 评级为相对主观判断，但每条依据均可在源码中核验；目的是给决策提供"硬话"，而非背书。

---

## 一、完成度评估（分两层看）

完成度必须拆成"引擎本体"与"商业产品"两层，二者差距正是本项目当前的核心矛盾。

### 1.1 引擎本体 —— 8.5/10（源码核实）

| 类别 | 现状 |
|---|---|
| 代码规模 | 自有代码 ~21.5 万行（engine 99,806/378 文件 + modules 16,788/85 + apps 33,296/146 + tests 51,870/223 + Lua samples/examples 13,658）。 |
| 测试 | 定义 TEST 宏 2,920，3D OFF 构建实跑 ~2,512 例（unit 1,990 + integration 470 + smoke 52）全绿；手写源码中 TODO/FIXME/HACK = 0。 |
| 渲染 | 三后端 RHI（Vulkan / DX11 / OpenGL 4.5）+ Web 上的 WebGL2/GLES3.0；DDGI 实时 GI、GPU-Driven 剔除、Hi-Z 遮挡、Clustered Forward+、CSM 级联阴影、Split-Sum IBL 反射探针、HDR 后处理链、WBOIT、风格化渲染均已落地。 |
| 运行时/架构 | EnTT 纯 ECS、RenderGraph(DAG)、Thin Snapshot 异步渲染、DSSL 着色器跨编、Mega VAO/VBO、Floating Origin。 |
| 脚本/库化 | Lua + C# 双语言绑定（codegen 自动生成）；底层 `dse_*` C ABI ~530 函数；默认静态库形态，可作宿主链接。 |
| 周边子系统 | 内存子系统（分配器 + Handle/HandleTable + 可选 mimalloc）、网络 GNS（可靠/非可靠 UDP + 加密）、资源加密打包（DPAK/BUN，AES）、资产热重载（Win + Linux）、编辑器（16 面板 / 34 ImGui 窗口）、headless `dse` CLI。 |

### 1.2 跨平台广度 —— 7/10

- 已覆盖：Windows / Linux（glfw）+ Android（APK）+ **Web/WASM（MVP，Emscripten/WebGL2，可演示 demo）**，共 4 个导出目标。
- 缺口：macOS / iOS 无平台后端（需 Mac 硬件）；Web 仅单线程、资源全量预载、单指触控、真实 GPU 未复验。

### 1.3 商业产品成熟度 —— 4/10

- 无真实出货案例；无插件 / 资产商店 / 模板生态；
- 文档与上手教程稀薄；
- CI（Linux/Android/editor 作业已配置）因额度未首跑；
- 网络仅"玩法级复制层 MVP"（缺 delta 快照 / 预测 / 插值 / AOI / 匹配）。

> **完成度的真相：**"做出一台对标 Godot 功能面的引擎"这件极难的事已经做到；"让别人能用它做出并卖出游戏"这件事才刚起步。

---

## 二、架构合理性（项目最强项）

### 2.1 做得对的

1. **分层干净且单向**：`apps → modules → engine → depends`，engine 不反向依赖 modules/apps，天然支持裁剪与库化。
2. **RHI 是"真三后端"而非贴皮**：通过能力标志门控（`SupportsCompute / SupportsSSBO / SupportsSSBOCompute / SupportsIndirectDraw`）让同一套渲染在 Vulkan 全开、在 WebGL2 自动降级。`engine/render/rhi/opengl/gl_rhi_device.h` 中 `supports_ssbo_=false` 即让 WebGL2 全自动绕过 Compute/SSBO/Indirect——这是工业级抽象的实证。
3. **现代范式齐备**：纯 ECS（EnTT）、RenderGraph(DAG)、Thin Snapshot 异步渲染、DSSL 着色器跨编、Mega VAO/VBO、Floating Origin——是 AAA 引擎的设计语汇。
4. **产品化预留**：C ABI(~530) + codegen 绑定 + 静态库形态，为"被宿主链接 / 多语言脚本 / SDK 分发"留好了空间。
5. **平台宏隔离**：平台相关 `#ifdef`（如 `__EMSCRIPTEN__`）集中在 `engine/platform/*`、`apps/web_host`、`engine/audio`、GL 后端 loader/shader 层，渲染高层逻辑保持平台无关。

### 2.2 隐忧（务必正视）

1. **广度可能跑在深度前面**。特性面铺得极宽，但相当一部分仅验证到 demo / 单测级，缺真实项目的极端负载打磨（Web 端官方定位也只敢说"可演示 demo"）。
2. **Bus factor ≈ 1 的维护风险**。~21.5 万行高级渲染 / 网络 / 编辑器基本是单人体量，长期维护、回归与文档同步的成本会指数上升——这是独立引擎"做得出、养不起"的典型陷阱。
3. **高级渲染的 ROI 存疑**。GPU-Driven / Hi-Z / Clustered 很强，但独立游戏极少用得上；这些工程算力更应优先换成"采用力"。

---

## 三、作为国产独立引擎的竞争力

- **技术面：有真竞争力。** 国内开源 / 独立的通用 3D 引擎屈指可数（Cocos 偏 2D、3D 偏弱；大厂自研引擎不开放）。DSE 的"现代 C++20 + 纯 ECS + 多后端 RHI + 高级渲染管线 + 源码可控"组合，在国产里相当稀缺，对标的是 **Godot 这一档**而非 Cocos。
- **生态面：几乎为零。** 引擎的胜负手从来不是参数，而是**文档 / 教程 / 资产商店 / 插件 / 出货案例 / 社区**。Unity、Godot 都赢在这里，而 DSE 目前全缺。

> 结论：**"技术上能打，生态上还没上场。"**

---

## 四、生态位定位（建议）

### 4.1 不要做的事

不要正面去抢 Unity / Unreal / Godot 的"全能通用引擎"市场——那是生态战，靠纯技术赢不了。

### 4.2 适合的生态位（按现实度排序）

1. **"可控自研引擎"底座** —— 面向不愿被商业引擎绑定 / 抽成、需要源码级掌控的团队或个人；C ABI + 静态库形态正好支撑。
2. **数据驱动 / 大规模实体的特定品类** —— 把 ECS + GPU-Driven 的真优势用在 RTS、大世界、模拟经营这类"实体量大、逻辑数据化"的游戏，做到"在这个品类比 Godot 跑得动"。
3. **现代引擎架构的学习 / 参考实现** —— 零 TODO、强测试、清晰分层，对学习引擎开发者极具价值，也是天然的影响力 / 招人入口。
4. **国产自主可控** —— 在教育、信创、特定行业有政策面与叙事价值。

---

## 五、结论与关键一招

- **评级：技术 A-，产品 C+，潜力 A。** 这是一台被"严重低估自身采用力短板"的好引擎。
- **关键一招（与 `ADOPTION_ROADMAP.md` 判断一致）：冻结新引擎特性，把下一阶段全部 ROI 押在"采用力"上**：
  1. Getting Started 教程 + 第一篇 2D 教程（最快见效、杠杆最高）；
  2. 一键出包 / Export Templates（工程化 Win/Linux/Android/Web 出包）；
  3. 2~3 个 clone 即玩的品类模板（2D 平台跳跃 / 俯视 RPG / 3D 第三人称）；
  4. **一个用 DSE 真做出来并发布的小游戏 demo**。

> 一个真实出货案例对"国产独游引擎"叙事的价值，胜过再加十个渲染特性。
