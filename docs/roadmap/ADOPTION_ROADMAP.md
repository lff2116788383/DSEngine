# DSEngine 独游采用路线图（Adoption Roadmap）

> 制定日期：2026-06-13 · 分支：`feature/engine-lib`
> 依据：基于源码现状核实（见 `PROGRESS_REPORT.md` 三次复核），非愿景文档。
> 目标读者：引擎维护者 / 决策者。

---

## 〇、一句话定位

> **引擎技术力已过剩，采用力严重不足。** 面向单平台(Windows)的 2D/3D 独立游戏，DSEngine 的引擎本体功能广度已对标 Godot 4.x；但"一个独游开发者能否顺利选用并出货"取决于**分发、上手成本、生态**——这三项目前是空白或半成品。
>
> 因此本路线图的核心主张是：**未来 3–6 个月不再堆引擎特性，all-in 开发者体验(DX) + 导出目标 + 文档/示例 + 社区。**

---

## 一、现状盘点（代码核实）

### 已具备（无需再投入）
- 渲染：GL/Vulkan/D3D11 三后端 + Compute(DDGI/TressFX/Grass) + 完整后处理链
- 玩法：ECS(EnTT, 102 组件) + 2D(Sprite/Tilemap/Spine) + 3D + 物理(Jolt/Box2D) + 动画(FSM/IK/Blend/Morph)
- 脚本：Lua(原生 C API) + C#，底层 `dse_*` C ABI ~518 函数(codegen)
- 系统：StreamingManager 流式加载、内存子系统(`engine/core/memory`)、崩溃捕获(`engine/diagnostics`)、热重载(Win+Linux)
- 工具：ImGui 编辑器、`dse` CLI(`new/build/pack/dist/assets/scenes/...`)、SDK 打包(`verify_sdk.ps1` 四组合)
- 网络：传输层 GNS + Lua `dse.net/http/serialize` + 玩法级复制层 **MVP**(`engine/net/replication`)

### 关键缺口（决定采用率）
| 缺口 | 现状 | 对独游影响 |
|------|------|-----------|
| **导出目标窄** | 仅 Win 桌面 + Android APK；平台后端仅 `glfw`(桌面) + `android` | 🔴 致命：缺 macOS / **Web(WASM)** / Linux 发行 / 主机 |
| **一键出包** | `dse dist` 雏形 + SDK 包 + APK | 🔴 高：无面向玩家的安装器 / Export Templates 体验 |
| **文档/教程** | 有 API 文档与设计文档，缺面向新手的 Getting Started / 教程系列 | 🔴 高：采用头号决定因素 |
| **示例模板** | `samples/`(68 Lua) + `examples/sdk_consumer` | 🟡 中：缺"clone 即玩"的完整品类模板 |
| **出货案例** | 无 | 🟡 中：缺信任背书与实战 battle-testing |
| **许可/社区** | 未明确 License；无 Discord/论坛 | 🟡 中：独游强烈偏好开源/免费 + 活跃社区 |
| **玩法级网络深度** | 复制层 MVP，缺 delta/预测/插值/AOI | 🟢 低：联机为细分需求 |

---

## 二、路线图（按 ROI 排序，分三阶段）

### 🔴 阶段 A（0–2 个月）：打通"能出货 + 能上手" —— 最高优先级

**A1. Web/WASM 导出**（投入大、回报最大）
- 新增 `engine/platform/web`(Emscripten + WebGL2/WebGPU) 后端，复用 `PlatformApp` 抽象
- RHI：GL 后端走 WebGL2(或新增 WebGPU 后端)；裁剪线程/文件系统依赖(用 IDBFS/预打包 VFS)
- `dse build --target web` 产出可上传 itch.io 的 `index.html + .wasm + .data`
- 验收：一个 2D 示例在浏览器跑通，CI 增加 `build-web` 作业
- 价值：覆盖 itch.io/Web Demo 这一独游命脉，"试玩门槛 = 一个链接"

**A2. 一键出包 + 平台安装器**（基于已有 `dse dist`）
- 把 `dse dist` 做成各平台 Export Template：Win(zip + 可选 NSIS/Inno 安装器)、Linux(AppImage/tar)、Android(APK 已有)、Web(A1)
- 自动收集运行时 DLL/Redist、资源加密打包(已有)、图标/启动 Splash(已有)注入
- 验收：从空项目 `dse new` → `dse build` → `dse dist --target <p>` 一条龙产出可分发包

**A3. Getting Started + 一套上手教程**（采用头号杠杆）
- `docs/getting-started.md`：5 分钟从安装到运行第一个窗口/精灵
- "做一个小游戏"教程系列(2D 为主)：输入→精灵→碰撞→分数→打包发布
- API 速查页 + 常见问题(FAQ)；中英双语(已有英文 README 基础)
- 验收：一个没接触过引擎的人能照文档独立做出并导出一个小游戏

### 🟡 阶段 B（2–4 个月）：降低门槛 + 建立信任

**B1. macOS 桌面后端**（补齐 Apple 桌面）
- `glfw` 已跨平台，主要工作在 RHI：新增 Metal 后端 或 走 MoltenVK(Vulkan→Metal)
- 优先 MoltenVK 路线复用现有 Vulkan 后端，成本远低于原生 Metal
- 验收：编辑器与运行时在 macOS 跑通，CI 增加 `build-macos`(需 Mac runner)

**B2. 品类模板工程**（clone 即玩）
- `templates/`：2D 平台跳跃 / 2D 俯视角 RPG / 3D 第三人称 各一个完整可玩骨架
- 接入 `dse new --template <name>`
- 验收：`dse new mygame --template platformer-2d` 后即可运行与改造

**B3. 一款出货 Demo 游戏**
- 用 DSE 做一个小而完整的游戏，上架 itch.io(免费)
- 作为信任背书 + 实战暴露引擎缺陷(battle-testing) + 教程素材来源

### 🟢 阶段 C（4–6 个月）：生态与深度

**C1. 许可与社区基建**
- 明确 License(是否开源/免费商用——直接决定独游意愿)
- 建 Discord/Issue 模板/贡献指南；建立 Bug 快速响应机制

**C2. 编辑器 UX 打磨**
- 资产拖拽、撤销重做覆盖率、属性面板一致性、首次启动引导

**C3. 玩法级网络深化（按需）**
- 复制层 delta/快照压缩、客户端预测+回滚、插值、AOI、大厅/匹配
- 仅当出现联机独游需求或 Demo 需要时推进

**C4. 插件/资产生态（远期）**
- 插件机制 + 资产导入流水线规范，为社区贡献铺路

---

## 三、为什么这样排（决策依据）

1. **Godot 的崛起证明：独游选引擎看的是 免费/开源 + 文档 + 导出便利 + 社区，而非渲染特性上限。** DSE 渲染特性已经超出绝大多数独游所需。
2. **Web 导出是独游获客的最低门槛**：一个可点击试玩链接 > 任何特性列表。
3. **文档是采用的复利投资**：一份好教程能持续转化用户，边际成本趋零。
4. **网络深度优先级低**：联机是少数品类需求，复制层 MVP 已能覆盖原型；过早投入收益低。

---

## 四、衡量成功的指标（建议）

| 指标 | 现状 | 阶段 A 目标 | 阶段 B 目标 |
|------|------|------------|------------|
| 导出目标数 | 2(Win/Android) | 4(+Web/Linux 发行) | 5(+macOS) |
| 从零到导出一个游戏的耗时 | 未知/高 | < 30 分钟(照文档) | < 15 分钟(用模板) |
| 上手文档/教程篇数 | ~0(面向新手) | ≥ 1 套完整教程 | ≥ 3 篇品类教程 |
| 可 clone 即玩的模板 | 0 | 1 | 3 |
| 出货案例 | 0 | 0 | 1(itch.io) |
| CI 覆盖平台 | Win/Linux/Android | +Web | +macOS |

---

## 五、近期可立即启动的第一步（建议）

> 三选一作为起点，均可独立交付：
> - **最高获客杠杆**：A1 Web/WASM 导出（技术挑战最大，回报最大）
> - **最快见效**：A3 Getting Started + 教程（无需改引擎，纯文档/示例）
> - **最稳基建**：A2 一键出包（已有 `dse dist` 雏形，工程化即可）

> 注：本文件为方向性路线图，不含具体排期承诺；实际推进顺序以维护者确认为准。
