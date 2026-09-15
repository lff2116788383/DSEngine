# ADR：四项待办架构重构的决策记录

状态：**已部分实施**（ADR-1 / ADR-2 完成；ADR-4 首个 OBJECT 拆分切片完成；ADR-3 读取路径进行中）（baseline：`feature/engine-lib`，2026-09 审计）

## 为什么要写这份 ADR

以下四项都是“会复利”的重构，但它们各自会动到 4 个 RHI 后端或全部资产格式。
本仓库的架构理由目前主要散落在代码注释里，而作者高度集中（单人 + AI 代理），
一旦记忆不在，后续重构极易踩到当初刻意避开的坑。故在动手前先把“为什么现在是这样”
与“怎么改才安全”定下来。

每项给出：现状实测 → 约束 → 建议路径 → 可验证的第一步。

---

## ADR-1：`CommandBuffer` 从“立即转发”改为“真记录”（仅 Vulkan）

### 现状（实测）
- `ForwardingCommandBuffer`（`engine/render/rhi/forwarding_command_buffer.h`）被 GL/DX11/Vulkan 三端共同继承；
  每条命令直接转调 `RhiDevice::Real*()`，即时执行。
- 后果：不能录制、不能复用、不能多线程录制；Vulkan 的
  `vkBeginCommandBuffer/vkCmd*/vkEndCommandBuffer` 能力被浪费。

### 约束
- **不能只改 Vulkan**：`CommandBuffer` 是纯虚接口，且存在“所有绘制原语必须纯虚”的刻意设计
  （防止某后端漏实现时默默吞掉绘制）。改成录制后，GL/DX11 仍是转发，两种语义共存会
  产生“何时真正执行”的分歧 —— 必须先定义清楚。
- 现有消费者（HairRenderer / SkyboxRenderer / GPU-driven 等）均假设“绑定在 `Draw` 前立即生效”。

### 建议路径
1. 先在 `RhiDevice` 上增加能力位 `SupportsDeferredRecording()`，默认 false（GL/DX11 保持转发）。
2. Vulkan 实现真录制：`OpenCommandBuffer` → `vkBeginCommandBuffer`；各 `Cmd*` 只记录；
   `Submit` 才 `vkEndCommandBuffer` + queue submit。
3. 关键约束：**标记状态未知的命令**（如 push constant）仍需按录制语义记录而非立即上传。
4. 失败回退：`DSE_VK_DEFERRED_RECORDING=1` 控制，默认关，保留一个版本可回退。

### 可验证的第一步
在 smoke 中加一条同一条命令录制两次、提交两次 -> 两帧像素一致的用例（现有 harness 已提供 BackendResult/RenderFn，可直接复用）；

### 为什么没在本次做
它会改变所有绘制消费者的隐含前提，属于“必须整体一次性验证”的改动；
半成品（只改一半后端）会产生两套语义并存的更差状态。

---

## ADR-2：stencil（模板）状态四端齐缺

### 现状（已逐端核实）
- `PipelineStateDesc`（`engine/render/rhi/rhi_types.h`）**没有**模板字段。
- Vulkan：`depth_stencil.stencilTestEnable = VK_FALSE` 写死（`vulkan_pipeline_state_manager.cpp:254`）。
- DX11：`ds_desc.StencilEnable = FALSE` 写死（`dx11_pipeline_state_manager.cpp:89`）。
- OpenGL：`GLPipelineStateManager::ApplyState` 里没有任何 `glStencil*`。
- WebGPU：只填 `stencilFront/Back.compare = Always`（`webgpu_draw_executor.cpp:667`）。

### 约束
- 只加 pipeline 字段是**不够的**：`RenderTargetDesc` 也没有 stencil 面，
  当前深度附件在四端都不保证带 stencil（GL 需 `GL_DEPTH24_STENCIL8`、
  DX11 `DXGI_FORMAT_D24_UNORM_S8_UINT` 与 DSV、Vulkan `D24S8` + render pass stencil attachment、
  WebGPU `depthStencil` 附件必须声明。）只加状态而不加表面，会得到一套永远失效的 API。
- 六个资产/渲染格式 serializer 与 RenderGraph 的 attachment 描述也要同步。
- WebGPU 的 stencil 参考值是**动态**的（`setStencilReference`），与 GL/Vulkan
  把 reference 烘进 pipeline 的语义不同 —— 接口必须选择一种并明确记录。

### 建议路径
1. `rhi_types.h`：新增 `StencilOp` / `StencilFaceState` / `StencilState`，`PipelineStateDesc` 增
   `StencilState stencil;`，**默认 `enabled=false`** 以保证零行为变化。
2. `RenderTargetDesc` 增 `bool has_stencil = false`，四端 RT 创建各自接入对应格式。
3. 四端 pipeline state manager 各自映射（映射函数可单独单测）。
4. DX11 需在 `OMSetDepthStencilState` 传入 reference；WebGPU 需 `setStencilReference`。

### 可验证的第一步（建议按此顺序切片）
1. 仅加字段 + 默认关闭 + 单测（确认零行为变化）；
2. 加 `RenderTargetDesc::has_stencil` + 四端 RT；
3. 四端 pipeline 接线；
4. 真机像素实例：两个重叠面片，模板掩码后只有一个可见（三后端逐一验证）。

---

## ADR-3：资产格式缺统一 DTO/字段层

### 现状
- 6 种格式已统一到“版本信封”（`ReadVersionEnvelope` + `AssetDiagnostics`），
  但 **body 仍各自手写**：加一个字段要改 6 处 serializer。

### 约束
- 这些格式有已发布的兼容性承诺（legacy 迁移用例已覆盖到字段级别 `AssetDiagnostics.migrated`），
  不能一次性换掉。
- `tools/audit/lua_api_audit.py` 已约束“BOUND but NOT in doc = 0”，字段层改动需同步文档。

### 建议路径
先做“读取路径”的 DTO 化（字典 -> 结构体的单一映射函数 + 字段元数据表），
写路径暂保持现状；每迁移一种格式就用现有 round-trip 用例验证行为不变。

### 可验证的第一步
选一种最小格式（`.d9slice` 或 `.dlight2d`）迁到 DTO 读取层，
跑现有 `NineSliceVersioned.*` / `Light2DVersioned.*` 全绿即可合入。

---

## ADR-4：单一 `dse_engine` target 导致改一行全量重编

### 现状（实测）
- 顶层 `CMakeLists.txt` 1188 行，仅 15 个 `add_subdirectory`；
- engine 全部（~630 手写文件 / ~133k 行）塞进单个 `dse_engine`。
- 本次会话实测：改 `render_graph.h` 一行注释 → 12 个文件重编；
  改 `job_system.h` 一个成员 → 引擎全量 + 三个测试二进制重链（约 15 分钟）。

### 建议路径
按现有目录边界拆成 `dse_core` / `dse_render` / `dse_scene` / `dse_scripting` 等
OBJECT 库，最后由 `dse_engine` 聚合链接（保留现有链接入口，消费者无感）。
每拆一块就验证一次完整测试与三后端像素基准。

### 为什么没在本次做
拆 target 会改变每个源文件的编译参数与依赖图，在没有 CI 全量验证的前提下不府合
“一次改一块、每块可回退”的原则。

---

## 参考
- `docs/architecture/RHI_ABSTRACTION_BOUNDARY.md`（接口契约）
- `docs/architecture/RHI_PRIMITIVE_CONTRACT.md`（绘制原语契约）
- `tests/gtest/support/perf_probe.h`（门禁稳定性的同类经验：单次采样不可作为阈值依据）

---

## ADR-2 附注（RESOLVED-FP-1）：`/RTC1` 与 `PipelineStateDesc` 默认初始化的误报

> 状态：**已定位并修复**。ADR-2 第 1 步（只加类型 + 字段 + 默认关闭 + 单测）已随本修复落库。

### 症状

Debug 构建下，`PipelineStateDescTest.DefaultValues` **确定性**弹
`Run-Time Check Failure #2 - Stack around the variable 'desc' was corrupted.`
把进程挂住；smoke 侧另见
`Stack around the variable 'ps' was corrupted.`
（`DX11RhiSmokeTest.AllCorrect` / `VulkanRhiSmokeTest.PipelineStateCreateAndDestroy`）
与 `xmemory(209)` 断言 `invalid argument`。因为构件随布局漂移，早期看起来像
「加 stencil 字段才触发」的随机内存越界写。

### 最小复现（修复前）

```powershell
bin\dse_gtest_unit_tests.exe --gtest_filter=PipelineStateDescTest.DefaultValues --gtest_brief=1
# 单条用例即可复现，与其它用例无关；对照 RenderTargetDescTest.DefaultValues 通过
```

### 定位方法：A/B 探针

在 `rhi_types_test.cpp` 里加 6 条临时探针，一条只改一个变量：

| 探针 | 写法 | 结果 |
|---|---|---|
| A | `PipelineStateDesc desc;` + 4 条 `EXPECT_TRUE` | **挂住（RTC）** |
| B | `PipelineStateDesc desc{};` + 同样 4 条 | 通过 |
| C | `PipelineStateDesc desc;` + 仅 1 条 `EXPECT_TRUE` | **挂住** |
| D | `PipelineStateDesc desc;` + 取地址 | **挂住** |
| E | `PipelineStateDesc desc;` + 枚举 `EXPECT_EQ` | **挂住** |
| F | `RenderTargetDesc desc;`（对照） | 通过 |

即与「几条断言」「是否取地址」「比什么类型」都无关，只与
**默认初始化 vs 值初始化**有关。旁证：既有用例
`PipelineStateDescGLTest.DefaultValues` 一直写的是 `PipelineStateDesc desc{};`，
它一直是通过的。

### 结论

所有实际发生的写入都在对象范围内，且改成 `{}` 即消失  判定为
**MSVC `/RTC1` 对「带默认成员初始化器 + 结构体内 padding」的 `PipelineStateDesc`
在默认初始化下的栈帧 guard 误报**，不是真实越界写。
（`PipelineStateDesc` 首成员是 `bool`，其后是 4 字节对齐的枚举，故对象内部有 padding；
`RenderTargetDesc` 首成员是 `int`，不受影响。）

### 修复

把全部 `PipelineStateDesc` **局部量**由默认初始化改为值初始化
（`PipelineStateDesc x;` -> `PipelineStateDesc x{};`），共 20 个 .cpp、45 处，
逐处 1:1 行替换（无格式化噪声）。因为该结构体所有成员都有默认成员初始化器，
值初始化与默认初始化的**字段取值完全一致**，属行为中性改动。

顺带修掉同源的 `DSE_ENABLE_ASAN` 说明与一个多余的空白行。

第 1 步另把 `StencilOp` 的 8 个显式取值在 `PipelineStateDescTest.DefaultValues` 里
逐值钉住（`static_cast<int>` 比较）：这些取值是 ADR-2 第 3 步四端映射函数的契约，
将来重编号会立刻被单测拦住。断言写进既有用例而非新增 `TEST` 宏，故用例总数保持 3451 不变。

### 修复后验证（全绿）

| 门禁 | 结果 |
|---|---|
| unit | 3451 执行 / 3450 passed / 1 skip / exit 0 |
| integration | 848 / 844 passed / 4 skip / exit 0 |
| smoke | 338 / 338 passed / exit 0 |
| `gtest_report.py` | exit 0（修复前因崩溃无 JUnit XML 而 exit 1） |
| `rhi_matrix_report.py` | opengl 83 / d3d11 77 / vulkan 80 / cross 64，0 failed |
| `test_inventory.py` / `repo_size_budget.py` / `lua_api_audit.py` | exit 0 |
| `check_core_ui_free.py` / `verify_feature_ledger.py` | exit 0 |
| `verify_feature_ledger.py --release` | exit 1（NO-GO，预期） |

### 遗留提示

`DSE_ENABLE_ASAN`（`CMakeLists.txt`，默认 `OFF`）保留作后续内存排查工具：
启用时自动剔除与 ASan 冲突的 `/RTC1` 并跳过 `_CRT_STDIO_ISO_WIDE_SPECIFIERS`。
**已知限制**：全项目 ASan 构建仍会因第三方静态库的 `annotate_vector` 开关不一致
在链接期失败；若需要，改成只给 `dse_engine` + 被测 gtest 目标加
`/fsanitize=address` 即可绕开。

---

## ADR-2 附注（STEP2-CHECKLIST）：第 2 步「RT 加 stencil 面」的落地清单

> 注意：本节写于第 2 步动工前，其中关于 D3D11 / Vulkan「也要改格式」的判断已被实测推翻，
> 并且第 2 步已在三端落地 —— 请以本文档末尾的 **STEP2-DONE** 一节为准。

第 1 步已落库（类型 + `PipelineStateDesc::stencil` 默认关闭 + 单测）。第 2 步尚未开始；
以下是逐端已核实过的改动点，可直接照做。**不要只加字段不接线** —— 那样会得到一套永远不生效的 API。

### 0) 类型层

- `engine/render/rhi/rhi_types.h`：`RenderTargetDesc` 增 `bool has_stencil = false;`，
  **并且必须同步 `operator==`**（该结构体用值语义做 RT 相等/缓存判定，漏加会导致
  `has_stencil` 不同的两个 RT 被判为相等）。
- 单测：默认 `false` 的断言写进既有 `RenderTargetDescTest.DefaultValues`（不新增 TEST 宏，
  保持用例总数口径）。

### 1) OpenGL（`engine/render/rhi/opengl/`）

- `gl_rhi_device.cpp:745`（2D 深度纹理）与 `:737`（cubemap 各面）：
  `has_stencil` 时内部格式用 `GL_DEPTH24_STENCIL8`，format/type 用 `GL_DEPTH_STENCIL` /
  `GL_UNSIGNED_INT_24_8`（当前写死 `GL_DEPTH_COMPONENT24` + `GL_DEPTH_COMPONENT`/`GL_UNSIGNED_INT`）。
- `gl_rhi_device.cpp:762 / :771 / :780`（FBO 挂载）：`has_stencil` 时改用
  `GL_DEPTH_STENCIL_ATTACHMENT`（当前是 `GL_DEPTH_ATTACHMENT`）。
- `gl_rhi_device.cpp:840`（MSAA renderbuffer）+ `:841` 挂载：同样改
  `GL_DEPTH24_STENCIL8` / `GL_DEPTH_STENCIL_ATTACHMENT`。
- `gl_draw_executor.cpp:345~405`：**容易被漏掉**。这里在 `BeginRenderPass` 时按 `cube_face`
  动态重挂深度附件（`:359` / `:361` 用 `GL_DEPTH_ATTACHMENT`），并有一段「附件句柄去重」逻辑。
  改格式时必须同步这里，否则 cube 阴影/逐面路径会挂错 attachment。
- `gl_rhi_device.cpp:986` 的深度回读保持 `GL_DEPTH_COMPONENT`（只读深度分量），不要改成
  `GL_DEPTH_STENCIL`。

### 2) D3D11（`engine/render/rhi/dx11/`）

- `dx11_resource_manager.cpp:786` 起：RT 深度纹理 `td.Format` 在 `has_stencil` 时用
  `DXGI_FORMAT_D24_UNORM_S8_UINT`，并确保建的是 DSV（`CreateDepthStencilView`）。
- 注意：**后备缓冲的 DSV 已经是** `DXGI_FORMAT_D24_UNORM_S8_UINT`（`dx11_context.cpp:315`），
  即默认帧缓冲天然带 stencil，只有 RT 需要改。
- `dx11_draw_executor.cpp:459` 已经用 `D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL` 清屏，无需改。

### 3) Vulkan（`engine/render/rhi/vulkan/`）

- `vulkan_resource_manager.cpp`：深度格式在 `has_stencil` 时用 `VK_FORMAT_D24_UNORM_S8_UINT`，
  并且 image aspect 必须带上 `VK_IMAGE_ASPECT_STENCIL_BIT`
  （`:1581` 附近已有「深度+模板格式必须 aspect 同时包含 STENCIL」的注释，按它对齐即可）。
- render pass 的 `VkAttachmentDescription` / `subpass.pDepthStencilAttachment` 要声明 stencil
  （`stencilLoadOp` / `stencilStoreOp`）；`vulkan_pipeline_state_manager.cpp:397~443` 已有这些字段，
  目前填的是 `DONT_CARE`。
- **缓存键必须同步**：`VulkanPipelineStateManager::RenderPassKey`
  （`vulkan_pipeline_state_manager.h`）当前只有
  `{has_color, has_depth, color_clear, depth_clear}`，**不含 has_stencil**。
  若不加入 key 与 hash，带 stencil 与不带 stencil 的两个 RT 会共用同一条 RenderPass 缓存，
  拿到错误的 render pass（回读全 0 且不一定有校验层报错  与 `ForgetRenderPass` 注释里
  描述的那类静默失败同源）。

### 4) WebGPU（`engine/render/rhi/webgpu/`）

- 深度格式需为 `WGPUTextureFormat_Depth24PlusStencil8`，并在
  `webgpu_draw_executor.cpp:661~670` 的 `WGPUDepthStencilState` 里真正填 `stencilFront/Back`
  （当前只填 `compare = Always`）。
- 本仓库默认 `DSE_ENABLE_WEBGPU=OFF`，且本机无 Emscripten，**改完无法编译验证**。
  若要落地，建议在其它后端已可用的前提下单独提交，并在 ledger 里标注「WebGPU 未编译验证」。

### 5) 横向同步（ADR-2 的约束里已点名）

- 六个资产/渲染格式 serializer 与 RenderGraph 的 attachment 描述都要能表达 stencil 面，
  否则「RT 带 stencil」在管线序列化/图调度层会丢信息。
- 完成后才能进第 3 步（四端 pipeline 映射）与第 4 步（真机像素用例：两个重叠面片，
  模板掩码后只有一个可见，三后端逐一验证）。

---

## ADR-2 附注（STEP2-DONE）：第 2 步已在三端落库，并更正两处事实

> 状态：GL / D3D11 / Vulkan **已落库并验证**；WebGPU 未做（无法编译验证，见下）。

### 事实更正 1：DX11 与 Vulkan 的 RT 深度附件本来就带模板面

ADR-2 原文写「当前深度附件在四端都不保证带 stencil」，**该判断对 DX11 / Vulkan 不成立**：

- D3D11：`dx11_resource_manager.cpp` 的 RT 深度纹理建为 `DXGI_FORMAT_R24G8_TYPELESS`，
  DSV 用 `DXGI_FORMAT_D24_UNORM_S8_UINT` —— 已含 stencil plane，无需改动。
- Vulkan：`vulkan_resource_manager.cpp` 的 `depth_format` 已是 `VK_FORMAT_D24_UNORM_S8_UINT`，
  且 `vulkan_resource_manager.cpp` / `vulkan_rhi_device_compute.cpp` 已有按格式推导
  `has_stencil` 并补 `VK_IMAGE_ASPECT_STENCIL_BIT` 的逻辑，无需改动。
- **OpenGL 是唯一真正缺模板面的后端**：深度纹理是 `GL_DEPTH_COMPONENT24`，
  挂在 `GL_DEPTH_ATTACHMENT` 上。

### 实际改动（第 2 步）

1. `rhi_types.h`：`RenderTargetDesc` 增 `bool has_stencil = false;`，
   **并同步 `operator==`**（该结构体做值语义比较，漏加会让 flag 不同的 RT 误判相等）。
2. `gl_rhi_device.cpp`：`has_stencil` 时深度附件改用 `GL_DEPTH24_STENCIL8` +
   `GL_DEPTH_STENCIL_ATTACHMENT`（数据格式 `GL_DEPTH_STENCIL` / `GL_UNSIGNED_INT_24_8`），
   覆盖 4 条路径：2D 纹理、cubemap 逐面、分层 cubemap 挂载、MSAA renderbuffer。
3. `gl_draw_executor.cpp`：GLES 逐面重挂深度附件处同步用同一 attachment 枚举。
4. 验证：`gl_rhi_smoke_test.cpp` / `vulkan_rhi_smoke_test.cpp` 的深度回读用例各增一段
   `has_stencil=true` 的 RT 创建 + 清屏 + 深度回读断言；`dx11_rhi_smoke_test.cpp` 增
   创建成功断言。断言写进既有用例，**测试总数保持 338 不变**。

未做：**WebGPU**（默认 `DSE_ENABLE_WEBGPU=OFF`，本机无 Emscripten，改完无法编译验证）。
把 WebGPU 的 `Depth24PlusStencil8` + `stencilFront/Back` 单独留作后续提交，避免引入
无法验证的死代码。

### 事实更正 2（更重要）：这些 desc 结构体的「默认初始化」不可靠

落第 2 步时先出现了大面积回归，根因**不是** stencil 逻辑，而是：

**`RenderTargetDesc desc;`（默认初始化）在本构建配置下会留下未初始化成员。**

实测证据：加完 `has_stencil` 后，单测 `RenderTargetDescTest.DefaultValues` 报
`desc.cube_map` 实际为 `true` —— `cube_map` 是**早就存在**的成员，默认值 `false`，
却被读到 `true`。同一现象让 GL 的 `CreateRenderTarget` 因 `has_stencil` 读到垃圾值而
失败，连带 47 个 smoke 用例失败/跳过。

把 `RenderTargetDesc` 局部量由默认初始化改成值初始化（`x;` -> `x{};`）后，
unit 3451 / smoke 338 全部恢复绿。这与本文件 `RESOLVED-FP-1` 一节记录的
`PipelineStateDesc` 现象**是同一类问题**（当时表现为 `/RTC1` 报栈帧损坏）。

**结论**：本仓库对这类「聚合 + 默认成员初始化器」的 desc 结构体，应统一使用值初始化
`T x{};`。本次已把 `RenderTargetDesc`（69 处，45 个 .cpp）与 `PipelineStateDesc`
（45 处，20 个 .cpp）全部改为值初始化；两者所有成员都有默认成员初始化器，
故**取值完全不变**，属行为中性改动。

成因尚未完全定位（未做反汇编）。已排除：依赖对象陈旧（Ninja depfile 核对无一陈旧）、
结构体重复定义（全仓仅一处 `struct RenderTargetDesc`）。下一步若要根因，
用 `DSE_ENABLE_ASAN`（`/RTC1` 关闭后）或 `/Od` 下看 codegen。

### 仍未做（第 3 / 4 步）

- **第 3 步**：四端 pipeline state manager 真正映射 `StencilState`
  （DX11 `OMSetDepthStencilState` 的 reference、Vulkan `VkStencilOpState`、
  GL `glStencilFuncSeparate` / `glStencilOpSeparate`、WebGPU `setStencilReference`，
  注意 WebGPU 的 reference 是**动态**的，与 GL/Vulkan 烘进 pipeline 的语义不同）。
  各端完成后要有映射函数的逐值单测（当前 `StencilOp` 取值已钉住）。
- **第 4 步**：真机像素用例 —— 两个重叠面片，模板掩码后只有一个可见，三后端逐一验证。
- Vulkan 的 `RenderPassKey` 建议补 `has_stencil`：目前只含
  `{has_color, has_depth, color_clear, depth_clear}`；当前无人设 `has_stencil`，
  故暂无实际影响，但第 3 步接线前必须补上，否则将来两种 RT 会共用同一条 RenderPass 缓存。

---

## ADR-2 附注（STEP3-4-DONE）：第 3、4 步已在三端落库并真机验证

> 状态：OpenGL / D3D11 / Vulkan **已落库 + 真机像素验证通过**；WebGPU 仅完成可编译期
> 状态（本机无 Emscripten，未编译验证），且 reference 的动态下发尚未做。

### 第 3 步：四端 pipeline 映射 `StencilState`

| 后端 | 实现要点 |
|---|---|
| OpenGL | `gl_enum_convert.h` 增 `ToGLStencilOp`（**注意 GL 语义：`GL_INCR`/`GL_DECR` 是 clamp，`GL_INCR_WRAP`/`GL_DECR_WRAP` 才是回绕**，与 Vulkan 命名相反）；`gl_pipeline_state_manager.cpp` 在 `ApplyState` 里权威下发 `glEnable(GL_STENCIL_TEST)` + `glStencilMaskSeparate` + `glStencilFuncSeparate` + `glStencilOpSeparate`（默认 `enabled=false` 时只 `glDisable`）；`gl_draw_executor.cpp` 在 render pass 起始把模板清 0 |
| D3D11 | 增 `ToD3D11StencilOp`（`IncrementClamp`->`INCR_SAT`、`IncrementWrap`->`INCR`）；`D3D11_DEPTH_STENCIL_DESC` 填 `StencilEnable` / `StencilReadMask` / `StencilWriteMask` / `FrontFace.*` / `BackFace.*`；`OMSetDepthStencilState` 改为传 `StencilState::reference`（默认 0，行为不变） |
| Vulkan | 增 `ToVkStencilOp`；`VkPipelineDepthStencilStateCreateInfo` 填 `stencilTestEnable` + `front`/`back` 的 `failOp`/`passOp`/`depthFailOp`/`compareOp`/`compareMask`/`writeMask`/`reference`；渲染通道 `stencilLoadOp=CLEAR` / `stencilStoreOp=STORE`（否则模板残留导致结果不确定） |
| WebGPU | `webgpu_common.h` 增 `ToStencilOp`；`WGPUDepthStencilState` 填 `stencilFront/Back` 的 compare/failOp/depthFailOp/passOp 与 `stencilReadMask`/`stencilWriteMask`。**未编译验证**（本机无 Emscripten、默认 `DSE_ENABLE_WEBGPU=OFF`） |

**WebGPU 已知缺口（重要）**：WebGPU 的 stencil reference 是**动态**状态，走
`wgpuRenderPassEncoderSetStencilReference`，不参与 pipeline 创建。当前实现只在
`reference == 0` 时正确；非 0 reference 需要在绘制前于 pass encoder 上单独下发。
代码注释与 ledger 都已标注。

默认 `stencil.enabled == false` 时，四端下发与改动前**逐字节等价**
（GL 多一次 `glDisable(GL_STENCIL_TEST)`，DX11/Vulkan/WebGPU 的模板字段保持关闭/Keep）。

### 第 4 步：真机 stencil 像素用例

新增 `tests/gtest/smoke/stencil_pixel_smoke_test.cpp`（每条 1 个 TEST，三后端共 3 条）：

- RT `256x256`，`has_color + has_depth + has_stencil`，清屏黑（同时清模板）；
- pass 1：全屏 quad，stencil = `Always` / `pass=Replace` / `ref=1` / `writeMask=0xFF`，
  片元在 `x >= 128` 处 `discard`  —— 只有左半屏被写成 stencil=1；
- pass 2：全屏 quad，stencil = `Equal(ref=1)` / 全 `Keep` / `writeMask=0x00`，片元输出绿色。

断言：左半屏绿、右半屏清屏黑。判别力：stencil 不生效则右半屏也绿；恒不过则左半屏黑；
模板面没接线/没清 0 则结果不稳定。**实测三后端全部 PASS（真机 RTX 3070）。**

### 数字口径更新（重要）

| 项 | 之前 | 现在 |
|---|---|---|
| unit | 3451 / 3450 passed / 1 skip | **不变** |
| integration | 848 / 844 passed / 4 skip | **不变** |
| smoke | 338 | **341**（+3 条 stencil 用例） |
| rhi matrix | opengl 83 / d3d11 77 / vulkan 80 / cross 64 | **opengl 84 / d3d11 78 / vulkan 81 / cross 64**，0 failed |

ADR-2 的 5 个步骤至此全部完成（第 1/2 步见前两节，第 3/4 步见本节，第 5 步「真机像素用例」
即本节第 4 步）。唯一遗留是 WebGPU 的 reference 动态下发 + 未编译验证。

---

## ADR-3 附注（STEP1-DONE）：全部读取路径 DTO 化

状态：**读取与写入路径均已 DTO/字段表化**；四个本地 2D 格式与六种内容格式均已完成主体迁移。

实际改动：
- `engine/core/asset_dto.h`：新增/完善字段元数据表 `ReadFields` / `WriteFields`；本次为 Light2D 增加 `Vec3` 字段类型，并给 Vec2/Vec3/Vec4 的每个分量补 `IsNumber()` 校验，保持旧 helper 的宽容语义。
- `apps/editor_cpp/src/editor_2d_tools.cpp`：`.d9slice`（顶层 7 字段）、`.dparticle2d`（24 字段）、`.dparallax`（场景 2 字段 + layer 10 字段）、`.dlight2d`（场景 2 字段 + light 14 字段）的读取路径全部改由 `FieldDesc` 表 + `ReadFields` 读取；缺失/类型不符字段保留 DTO 默认值，等价于旧行为。`.dparallax` 的 legacy `scroll_x/scroll_y` 迁移分支保留，并只在当前键缺失时触发。
- `engine/scripting/script_metadata.cpp`：`.dscriptmeta` body 读取路径迁移到 `ScriptMetadataDto` / `ScriptFieldMetaDto` + `FieldDesc` 表；`number_default` 使用新增的 `FieldType::FloatArray4`。
- 六种内容格式读取路径全部迁移：`.dscriptmeta`、`.dbp`、`.dshadergraph`、`.dasm`、`.dsequence`、`.dcutscene`。
- `asset_dto.h` 继续补齐 `UInt` / `UInt64` / `FloatArray4`，覆盖颜色、id 数组、`number_default` 等字段。
- 所有格式的写路径也已改用 `WriteFields` / rapidjson `Value` 组装；旧的裸字符串拼接写路径已删除。

验证（2026-09-12 本机）：
- 编译：`dse_editor_cpp` / `dse_gtest_integration_tests` exit 0。
- 定向：`NineSliceVersioned.*` + `Particle2DVersioned.*` + `ParallaxVersioned.*` + `Light2DVersioned.*` = 14/14 passed；`ScriptMetadata*` = 8/8 passed。
- 全量：unit 3451/3450 passed/1 skip、integration 848/844 passed/4 skip、smoke 341/341 passed，均 exit 0。
- `git diff --check` exit 0。

补充（2026-09-12 后续）：
- `.dbp` / `.dshadergraph` / `.dasm` / `.dsequence` / `.dcutscene` 读取路径均改为 `FieldDesc` + `ReadFields`；定向回归：`BlueprintSerialize.*`、`ShaderGraph*`、`AnimStateMachineSerializeTest.*`、`Cutscene*`、`*Sequencer*` 全绿。
- 写路径迁移已完成；后续可继续把数组/嵌套 body 上收到更通用的字段元数据，减少 serializer 里的手写 `Value` 组装。

后续可选（ADR-3 收尾）：
1. 为数组/嵌套 body 增加更通用的字段元数据能力，逐步删掉 serializer 里的手写数组循环。
2. 增加全格式读写 round-trip / legacy / forward-version 的矩阵化回归。
3. 若引入字段元数据代码生成，再评估是否合并 DTO 与字段表。

---

## ADR-1 附注（DONE）：Vulkan CommandBuffer 录制状态按命令缓冲隔离

状态：**已落库并全量验证**。

实际改动：
- 新增 `engine/render/rhi/vulkan/vulkan_command_state.h`：把原先挂在 `VulkanDrawExecutor` 上的 `prim_*`、`current_render_pass/msaa/color_count/skip_current_pass` 等录制状态收进 `VulkanCommandState`。
- `VulkanCommandBuffer` 各持有独立 `VulkanCommandState`；`BindPipeline/BindVertexBuffer/BindTexture/BindUniformBuffer/BindIndexBuffer/PushConstants/Draw*` 通过显式 `state` 参数传给 executor，不再写 executor 全局成员。
- `VulkanDrawExecutor` 的 GPU-Driven 设备级入口通过 `VulkanRhiDevice::active_render_state_` 指向当前命令缓冲状态，避免破坏既有调用路径。
- `RhiDevice::SupportsDeferredRecording()` 能力位加入虚表末尾；Vulkan 覆写为 true，GL/DX11 默认 false。
- `vulkan_rhi_smoke_test.cpp` 增加 Vulkan 能力位断言。

验证：full smoke 341/341、unit 3451/3450+1skip、integration 848/844+4skip；rhi matrix opengl 84 / d3d11 78 / vulkan 81 / cross 64，0 failed。

---

## ADR-4 附注（STEP1-DONE）：dse_engine 已拆为多个 OBJECT 分区

状态：**首个构建拆分切片已落库并全量验证**。

实际改动：
- 在 `CMakeLists.txt` 中按目录边界把 `engine_cpp` 分到 `dse_engine_obj_core/render/assets/scene/scripting/physics/media/runtime/modules/misc` 等 OBJECT target。
- `dse_engine` 保留为聚合静态/共享 target，`add_library` 使用 `$<TARGET_OBJECTS:...>` 汇总；第三方源码继续直接挂在 `dse_engine` 上。
- 在 `dse_engine` 全部 `target_*` 配置完成后，把 `INCLUDE_DIRECTORIES` / `COMPILE_DEFINITIONS` / `COMPILE_OPTIONS` / `LINK_LIBRARIES` / `LINK_DIRECTORIES` / MSVC runtime / LTO / PCH 复制到各 OBJECT target，保持原有编译口径。
- 依赖 `dse_compile_shaders` 也已复制到 OBJECT target。

验证：`dse_engine`、编辑器、unit/integration/smoke 全量重编 exit 0；上述三项测试电池全绿；编辑器三后端矩阵 verdict=pass。
