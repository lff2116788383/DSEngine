# DSEngine 现状评估 + RHI 后端盘点 + 反序列化隐患清单 + 路线图（2026-06-28）

> 本文为一次只读代码走查的产出（未改动引擎逻辑）。目标：给出 `engine/` 的客观现状、
> 4 个 RHI 后端的完成度对比、反序列化器的健壮性隐患清单，以及按 ROI 排序的方向建议。
> 配套已落地的修复见文末「附：本轮已修」。

---

## 一、现状速览

DSEngine 已是 **0.1.0-alpha 对外 SDK**（共享库分发 + `find_package(DSEngine)` 消费），
代码规模与子系统广度都已相当可观，**不是早期玩具**。

| 维度 | 现状 |
| --- | --- |
| 引擎规模 | `engine/` ≈ 22 万行；`render/` 独占 ≈ 16.3 万行（绝对主体） |
| 渲染 RHI | 4 后端：OpenGL / D3D11 / Vulkan / WebGPU，共享一份 GLSL 离线编译多目标内嵌头 |
| 渲染特性 | 前向 + GBuffer、阴影、GI、hair、morph、skinning、static_batch、多 pass 管线、GPU-driven/indirect |
| 物理 | 2D（Box2D）+ 3D（Jolt，含 cloth / fluid / fracture） |
| 音频 | 3D 空间化 + 总线管理 |
| 动画 | 状态机 / 重定向 / 烘焙 / 曲线 |
| 其他子系统 | ECS(entt)、导航(navmesh)、网络(net/replication/serialize + GNS)、输入(含录制)、profiler、job system、内存池、事件总线、模块/插件、Lua(2 万行绑定) |
| 平台 | Windows / Linux / Android / Web(WASM 实验) |
| 出包 | `dse` headless CLI + 编辑器 Build Game；AES-128-CTR 加密包；SDK 分发 + 消费者示例 + verify 脚本 |
| 测试 | gtest **159 单测 + 58 集成 + 43 smoke**（含跨 GL/D3D11/Vulkan 像素 smoke、physics3d、lua 生命周期）；control-server yaml 自动化套件 |
| CI | `ci.yml` 在 master push/PR 构建 engine + 全部 gtest + ctest，另有 editor-build、sdk-verify 作业 |

**关键认知修正**：DSE 的「测试 + CI 地基」其实已经很强——补引擎单测/接 CI 基本是已完成项，
不应作为新方向。真正的短板见第三节。

---

## 二、RHI 后端完成度盘点（B）

四个后端**都是真实实现，非占位桩**（TODO/未实现标记极少），且都覆盖 compute / indirect / GPU-driven。

| 后端 | 文件数 | LOC | 未实现/不支持标记 | 完成度判断 |
| --- | --- | --- | --- | --- |
| **Vulkan** | 16 | 11,729 | 3 | 最完整、最大；surface 创建有「平台不支持」分支、ImmediateDraw 默认帧缓冲目标未支持、个别 shader stage 未支持 |
| **WebGPU** | 16 | 10,947 | 0 | 体量第二，覆盖面广（13 文件涉 compute/indirect） |
| **D3D11** | 15 | 6,927 | 0 | 完整，Windows 主力路径之一（editor-build CI 即用 D3D11） |
| **OpenGL** | 17 | 6,321 | 1 | 体量最小但功能齐；作为默认/回退后端（factory 未知后端回退 GL/Vulkan） |

**后端选择**：`rhi_factory.cpp` 经 `DSE_RHI_BACKEND` 环境变量或编译开关
（`DSE_ENABLE_VULKAN/_D3D11/_WEBGPU`）选择，缺省回退 OpenGL。

**结论与风险**：四后端均「能用」，但维护面 = 4×。每个渲染特性都要在 4 套后端验证，
对小团队是**最大的长期成本来源**。Vulkan/WebGPU 体量显著大于 GL/D3D11，
说明新特性主要往这两条线堆——但这也意味着 GL/D3D11 容易在新特性上**悄悄落后于** Vulkan/WebGPU。

**建议**：明确「一线后端」(如 D3D11 + Vulkan) 做特性同步保证，
GL/WebGPU 降为「兼容/实验」并在文档中标注特性差异矩阵，避免「四后端都声称支持但实际参差」。

---

## 三、反序列化健壮性隐患清单（C）

**问题类别**：从磁盘/资源文件读入「长度/数量/偏移」字段后，未做上限/边界校验就直接
`resize()/reserve()/memcpy()`。畸形或损坏文件可触发 `bad_alloc`（超大分配）或越界读。
正常有效文件不受影响，但「打开损坏/他人工程不崩」是 alpha→beta 的体验底线。

### 高优先（运行时/出包路径，影响终端玩家）

| 位置 | 隐患 | 说明 |
| --- | --- | --- |
| `engine/assets/pak_reader.cpp:67-68` | `entries_.resize(header.entry_count)` / `index_.reserve(...)` **无任何 sanity 检查** | **最高优先**：`.dpak` 是**已发布游戏加载的包格式**；`entry_count` 为未校验 uint32，损坏包 → 近 4G 条目分配 → 崩溃 |
| `engine/assets/pak_reader.cpp:117` | `out_data.resize(entry.data_size)` | `data_size` 未校验即先分配再 `fread`，超大值先触发 bad_alloc |
| `engine/render/hair/hair_asset.cpp:66/70/102/103` | `vertices.resize(total_verts)` / `strands.resize(num_*)` | 顶点/发丝数量来自文件，需确认是否对文件大小做了上限校验 |
| `engine/render/font/truetype_font.cpp:146` | `atlas_bitmap_.resize(atlas_width_ * atlas_height_)` | 宽×高乘积可能整型溢出/超大分配 |

### 中优先（编辑器侧——本轮已修，见文末）

`editor_asset_db.cpp`、`editor_scene_io.cpp`、`editor_aux_panels.cpp`、`editor_layout_manager.cpp`
四处同类隐患已在本轮加固。

### 建议的统一做法

1. 给资源加载层提供统一的「带上限的读」辅助：`ReadCount(max)` / `ReadBlob(max)`，
   读到超限值即判损坏返回，而非直接分配。
2. `pak_reader` 增加：`entry_count` 上限（或与文件大小/TOC 区间交叉校验）、
   `data_offset + data_size <= 文件大小` 的区间校验。
3. 建一组**损坏样本测试**（截断 / 篡改长度字段 / 越界偏移）纳入 gtest，
   断言「优雅失败、不崩溃」。现有 `pak_roundtrip_test` / `dds_parser_test` 多测正常路径，缺畸形输入用例。

---

## 四、方向建议（按 ROI）

### 首选 ①：收敛 + 固化，冲 beta
- 选定 **1 个主力品类**（2D 或 3D）+ **1~2 个主力后端**，其余降为实验并标注差异矩阵。
- **冻结公共 API**（SDK 已 alpha，消费者已能集成 → API 稳定性此刻价值最高）。
- 做一个**完整可玩的样例游戏**跑通「建项目 → 搭场景 → 脚本 → 出包 → 运行」全链路。
- 补上手教程深度。
- 理由：alpha 引擎最该「把广度变深度」，证明全链路可用，而非继续铺特性。

### 次选 ②：让 CI 真跑 + 反序列化 fuzz
- 解决 CI 额度/触发问题（`ci.yml` 注释明示「CI 无额度时按需 workflow_dispatch」——
  **门禁存在但并非每次真跑**，这才是质量地基的真缺口）。
- 把本轮新增的编辑器 UI 测试门禁（`editor-automation.yml` 的 `l0b-ui-tests`）纳入常跑并首次真实验证。
- 落实第三节的反序列化统一校验 + 损坏样本测试集。
- 理由：纯地基、收益确定、不依赖产品定位。

### ③：按目标品类做深一个 pillar
- 2D（精灵/瓦片/物理/spine）或 3D（PBR/阴影/GI/骨骼）二选一做到「能做出像样游戏」。

**单点建议**：先做 ②（让已有的强测试真正跑起来，性价比最高），同时启动 ①（定品类、做 demo、冻 API）。

---

## 附：本轮已落地的修复（feature/engine-lib）

| 提交 | 内容 |
| --- | --- |
| `37d9fa11` | `editor_layout_manager.cpp`：`tellg()` 失败兜底，杜绝 `SIZE_MAX` 分配崩溃 |
| `c4190ddb` | `editor_asset_db` / `editor_scene_io` / `editor_aux_panels`：二进制读按未校验长度分配的加固（bad_alloc / 越界读） |

> `pak_reader.cpp` 等 `engine/` 侧的同类隐患（第三节高优先项）本文仅记录，**尚未修改**，
> 待方向 ② 确认后统一处理。
