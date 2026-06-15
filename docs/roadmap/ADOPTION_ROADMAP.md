# DSEngine 独游采用路线图（Adoption Roadmap）

> 制定日期：2026-06-13 · 分支：`feature/engine-lib`
> 依据：基于源码现状核实（见 `PROGRESS_REPORT.md` 三次复核），非愿景文档。
> 目标读者：引擎维护者 / 决策者。
>
> **更新（2026-06-14）**：阶段 A 的 **A1 Web/WASM 导出已达 MVP（可演示 demo）并真机验证**（含音频 / 单指触屏 / `dse build --target web` 出包链路）——详见 `../plans/WEB_AND_DOCS_PLAN.md` §2.3b/§2.3c/§2.6/§2.8c-d。Web/WASM 暂作**非主力**分支保留在此水位；下一步推荐方向见 §五。
>
> **更新（2026-06-14）**：**A3 上手教程已落地**（`../getting-started/QUICKSTART.md` + `../getting-started/TUTORIAL_2D_FIRST_GAME.md`，对照源码核实并 llvmpipe 实跑验证）；顺带修复脚手架（`on_init/on_update`→`Awake/Update`、2D 模板默认相机+精灵+`local app=dse.app` 别名）。**A2 已开第一刀**：`game.dsmanifest` 记录 `entry_script`，导出包**双击即玩**（无需 `launch.bat`，加密包除外）。A2/A3 详细完成状态与剩余待办见 §六。

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
| **导出目标窄** | Win 桌面 + Android APK + **Web/WASM(MVP，可演示)**；平台后端 `glfw`(桌面) + `android` + **`web`** | 🟡 改善中：Web 已补齐(MVP)；仍缺 macOS / Linux 发行 / 主机 |
| **一键出包** | `dse dist` 雏形 + SDK 包 + APK | 🔴 高：无面向玩家的安装器 / Export Templates 体验 |
| **文档/教程** | 有 API 文档与设计文档，缺面向新手的 Getting Started / 教程系列 | 🔴 高：采用头号决定因素 |
| **示例模板** | `samples/`(68 Lua) + `examples/sdk_consumer` | 🟡 中：缺"clone 即玩"的完整品类模板 |
| **出货案例** | 无 | 🟡 中：缺信任背书与实战 battle-testing |
| **许可/社区** | 未明确 License；无 Discord/论坛 | 🟡 中：独游强烈偏好开源/免费 + 活跃社区 |
| **玩法级网络深度** | 复制层 MVP，缺 delta/预测/插值/AOI | 🟢 低：联机为细分需求 |

---

## 二、路线图（按 ROI 排序，分三阶段）

### 🔴 阶段 A（0–2 个月）：打通"能出货 + 能上手" —— 最高优先级

**A1. Web/WASM 导出** ✅ 已达 MVP（2026-06-14，可演示 demo）
- 新增 `engine/platform/web`(Emscripten + WebGL2) 后端，复用 `PlatformApp` 抽象
- RHI：GL 后端走 WebGL2；裁剪线程/文件系统依赖(用 IDBFS/预打包 VFS)
- `dse build --target web` 产出可上传 itch.io 的 `index.html + .wasm + .data`
- 验收：一个 2D 示例在浏览器跑通，CI 增加 `build-web` 作业
- 价值：覆盖 itch.io/Web Demo 这一独游命脉，"试玩门槛 = 一个链接"
- **完成状态（MVP）**：`engine/platform/web`(Emscripten/WebGL2) 后端 + `apps/web_host` 宿主已落地；GL 后端经**能力标志**在 WebGL2 自动绕过 Compute/SSBO/Indirect（无平台 `#ifdef` 污染 render 核心）；`dse build --target web` / `dse dist --target web` 出包链路打通；真机 Chrome/WebGL2 验证 2D/3D 出画 + 键鼠 + 单指触屏 + 音频(BGM)。debug+release 双产物均验过（release wasm≈3.74MB）。
- **本期有意延后（非 bug，平台边界 / 技术债）**：资源懒加载(DEBT-1)、pthreads+COOP/COEP 多线程(DEBT-4)、多指触控(DEBT-5)、WebGPU 第 4 后端（解锁 Web 端 Compute/GPU-Driven，远期质变）、真实 GPU 浏览器复验（本机无独显，现走 SwiftShader 软光栅）、CI `build-web`（无额度，暂略）。
- **定位**：作为「可演示 demo」水位的**非主力**分支保留；要扶正为主力时再上 WebGPU + 多线程两步。

**A2. 一键出包 + 平台安装器**（基于已有 `dse dist`）🟡 已开第一刀（2026-06-14）
- 把 `dse dist` 做成各平台 Export Template：Win(zip + 可选 NSIS/Inno 安装器)、Linux(AppImage/tar)、Android(APK 已有)、Web(A1)
- 自动收集运行时 DLL/Redist、资源加密打包(已有)、图标/启动 Splash(已有)注入
- 验收：从空项目 `dse new` → `dse build` → `dse dist --target <p>` 一条龙产出可分发包
- **已完成（2026-06-14）**：`game.dsmanifest` 增加 `entry_script` 字段，`dse build` 写入入口脚本；standalone 宿主未传 `--script` 时从 manifest 读取——**未加密导出包双击 exe 即玩**（详见 §六）。剩余项见 §六待办台账。

**A3. Getting Started + 一套上手教程**（采用头号杠杆）✅ 已落地（2026-06-14）
- `docs/getting-started.md`：5 分钟从安装到运行第一个窗口/精灵
- "做一个小游戏"教程系列(2D 为主)：输入→精灵→碰撞→分数→打包发布
- API 速查页 + 常见问题(FAQ)；中英双语(已有英文 README 基础)
- 验收：一个没接触过引擎的人能照文档独立做出并导出一个小游戏
- **已完成（2026-06-14）**：`../getting-started/QUICKSTART.md`（纯 CLI 黄金路径，30 分钟从零到可分发 2D 游戏，含 llvmpipe 软件渲染回退 + 故障排查表）+ `../getting-started/TUTORIAL_2D_FIRST_GAME.md`（金币收集教程：移动/碰撞/计分/通关，每个 `dse.*` 接口对照源码核实并实跑）。`GETTING_STARTED.md` 加横幅指向 QUICKSTART、`docs/README.md` 入门索引补两条。配套修复脚手架（见 §六）。
- **剩余（可选）**：英文版 QUICKSTART/教程；FAQ 独立页；更多品类教程（与 B2 模板互为素材）。

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
| 导出目标数 | **3(Win/Android/Web-MVP)** | 4(+Linux 发行) | 5(+macOS) |
| 从零到导出一个游戏的耗时 | 未知/高 | < 30 分钟(照文档) | < 15 分钟(用模板) |
| 上手文档/教程篇数 | ~0(面向新手) | ≥ 1 套完整教程 | ≥ 3 篇品类教程 |
| 可 clone 即玩的模板 | 0 | 1 | 3 |
| 出货案例 | 0 | 0 | 1(itch.io) |
| CI 覆盖平台 | Win/Linux/Android | +Web | +macOS |

---

## 五、近期可立即启动的第一步（建议）

> **A1 Web/WASM 导出已达 MVP（2026-06-14）**，且 Web 暂定为非主力。故下一步建议从以下里优先（按"采用力"杠杆排）：
> - **首推 · 最快见效**：**A3 Getting Started + 一套上手教程**（无需改引擎，纯文档/示例；采用头号杠杆，边际成本趋零）
> - **次推 · 最稳基建**：**A2 一键出包 / Export Templates**（已有 `dse dist` 雏形，把 Win/Linux/Android/Web 出包工程化为一条龙）
> - **再次 · clone 即玩**：**B2 品类模板工程**（2D 平台跳跃 / 俯视 RPG / 3D 第三人称，接 `dse new --template`），与 A3 教程互为素材
> - **桌面补全（按需）**：**B1 macOS 后端**走 MoltenVK（复用现有 Vulkan），成本远低于原生 Metal
>
> Web/WASM 若日后要扶正为主力：①**WebGPU 第 4 后端**（解锁 Compute/GPU-Driven，质变）②**pthreads+COOP/COEP 多线程**(DEBT-4)。

---

## 六、上手链路台账（A2/A3 进展 + 剩余待办）

> 2026-06-14 在写 A3 教程时把整条「上手链路」当试金石实跑，逐个暴露并记录卡点。**纯文档/脚手架/CLI 改动，未动渲染等引擎核心。** 以下为已修与待办，方便后续接手。

### 已修复（已提交到 `feature/engine-lib`，无 PR）
| 项 | 问题 | 修复 | 验证 |
|----|------|------|------|
| 脚手架生命周期钩子 | 模板生成 `on_init/on_update`，但运行时调用 `Awake/Update`，新手代码不执行 | `project_scaffold.cpp` 全模板(2d/3d/lua/cpp)改 `Awake/Update`；`dse help` 文案同步 | `dse new 2d`→`dse build`→运行，脚本生效 |
| 2D 模板空场景 | 旧模板无相机/精灵，跑起来黑屏，新手没有正反馈 | 2D 模板默认生成正交相机 + 可移动青色精灵 | 运行即见方块，WASD/方向键移动 |
| 输入 `nil 'app'` 崩溃 | 模板写 `app.get_key` 但 `app` 未定义（API 在 `dse.app`），每帧报错 | 模板加 `local app = dse.app` 别名 | 运行无报错，输入响应正常 |
| 双击 exe 空窗口（摩擦点①） | 裸跑 `<proj>.exe` 不加载入口脚本，必须用 `launch.bat` 传 `--script` | `game.dsmanifest` 增 `entry_script`；`dse build` 写入；standalone 未传 `--script` 时从 manifest 读取 | 裸跑 `<proj>.exe`（无参数）渲染+输入正常；单测 12/12 |

### 剩余待办进展（2026-06-14 全部落地）

> 下列 7 项均已在 `feature/engine-lib` 完成、构建+测试（ctest 3/3）通过；品类模板在 llvmpipe 软件渲染下三模板实跑 `Awake OK`。**仍为文档/脚手架/CLI/编辑器出包改动，未动渲染等引擎核心。**

1. ✅ **无预编译 `dse` 二进制下载**（A2，高）：新增 `scripts/package_dse_tools.ps1`，编译 `dse_cli`/`dse_standalone` 并收集 `dse.exe` + `DSEngine_Game.exe` + DLL 打成 `DSEngine-tools-vX.Y.Z-win-x64.zip`（含内嵌 README）；QUICKSTART 第 0 步改为「下载预编译 / bootstrap」二选一，README 提供 Releases 直链。`.gitignore` 忽略 `dist/`。
2. ✅ **编辑器 Build Game 与 CLI 对齐**（A2，中）：`apps/editor_cpp/src/editor_build_game.cpp` 改用 `WriteAppManifest` 写 `entry_script`，并把 `scripts/`+`scenes/` 松散拷到 exe 旁 → 编辑器出包同样双击即玩。
3. ✅ **加密资源包仍需 `launch.bat`**（A2，低/可接受）：维持现状（明文包为新手默认双击路径），并在 QUICKSTART 与新建 FAQ 中明确说明「加密包须随发 `launch.bat` 并以其启动」。
4. ✅ **无独显环境需手动部署软件 GL**（A2/DX，中）：`dse build` 增 `--with-swgl` 开关，校验 llvmpipe DLL 是否随包并在 launch 脚本设 `GALLIUM_DRIVER=llvmpipe`；QUICKSTART/FAQ 给出 `setup_swgl.ps1` 步骤。
5. ✅ **`dse dist` 各平台 Export Template**（A2 主体，高）：`dse dist --target win|linux` 把已构建目录工程化为 `.zip`/`.tar.gz`（Export Template），`--installer`（Inno Setup `iscc`）/`--appimage`（`appimagetool`）可选附带安装器。
6. ✅ **英文文档 + FAQ 独立页**（A3 收尾，中）：新增 `../getting-started/QUICKSTART.en.md`（英文版上手）与 `../getting-started/FAQ.md`（双语 FAQ：拿工具/运行/打包加密/分发/模板）；`docs/README.md` 与 `README.en.md` 加索引。
7. ✅ **品类模板工程**（B2，中）：`dse new` 增 `platformer`/`topdown`/`thirdperson` 三个品类模板（接 `dse new --template=`），各生成带完整玩法的可运行 `main.lua`（2D 平台跳跃 / 俯视 RPG / 3D 第三人称），仅用已文档化的 `dse.app`/`dse.ecs` API。

---

> 注：本文件为方向性路线图，不含具体排期承诺；实际推进顺序以维护者确认为准。
