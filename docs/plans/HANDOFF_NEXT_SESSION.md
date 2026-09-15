# 新会话任务指令：DSE 收尾（P1 剩余 + P2 切片）

交接时间：本会话结束时。**先读完整份再动手**，尤其是第 5 节的环境陷阱。

---

## 0. 起点状态（务必先核对）

| 项 | 值 |
|---|---|
| 仓库 | `C:/Users/Administrator/Desktop/Engine/DSEngine` |
| 分支 | `feature/engine-lib` |
| HEAD | `a8c80695`（**本会话所有改动均未提交**，31 个改动条目） |
| 构建目录 | `out/build/windows-x64-debug`（Ninja + MSVC 2022 BuildTools，Debug） |
| 关键开关 | `DSE_BUILD_EDITOR=ON`、`DSE_BUILD_GTESTS=ON`、`DSE_ENABLE_VULKAN=ON`、`DSE_ENABLE_D3D11=ON`、`DSE_ENABLE_PHYSX=OFF`、`DSE_ENABLE_MEM_TRACKING=ON`、`DSE_ENABLE_VIRTUAL_GEOMETRY=OFF` |
| 真机 GPU | NVIDIA RTX 3070 Laptop（`device.is_software=0`） |
| 测试二进制 | `<repo>/bin/dse_gtest_{unit,integration,smoke}_tests.exe`、`<repo>/bin/dsengine-editor.exe` |

**第一步永远是**：`git status --short` 确认这 31 个改动还在（本会话成果未经提交），
然后跑一遍第 3 节的基线复现确认环境可用。**不要** `git checkout .` / `git stash`。

---

## 1. 本会话已完成（不要重做）

### 引擎缺陷（8 项，全部已验证）
1. `RenderGraphIntegrationTest.TransientRT_*` 旧契约断言导致 EXPECT 失败后越界读，进程 abort，
   **193 个 integration 用例从未执行**（655/848）。已按真实契约重写并补上「跨帧复用同一物理 RT」覆盖。
2. `ImpostorSystemSmoke.GL_BakeAndRenderLifecycle` 红灯（断言早于主线程回写）。已改为断言
   Phase 1 契约（渲染线程不得回写 ECS）+ 主线程写回。
3. **JobSystem 静默丢任务**：`RecycleEntry` 非幂等，`ResolvePin` 允许在 refcount=0 时 re-pin，
   导致两次 `fetch_sub` 都返回 1、条目被压入 freelist 两次（自环）→ 同一 `JobEntry` 分给两个 job。
   修法：显式 `pooled` 标志使回收恰好一次。新增 `job_system_conservation_test.cpp`
   （100 轮 x 1000 job 守恒门禁）。
4. GL compute push 常量 UBO 缓存以 `ShaderHandle`（= GL program 名，可被驱动复用）为键，
   `DeleteComputeShader` 不清理 → 新 program 命中旧条目、跳过 `glUniformBlockBinding`
   → compute 静默不写、回读全 0。已修 + 顺带修掉 UBO 泄漏。
5. Vulkan `RenderPassKey` 只含 `{has_color, has_depth, color_clear, depth_clear}`（无尺寸/格式），
   `DeleteRenderTarget` 销毁 VkRenderPass 却不清 `render_pass_cache_`。已加 `ForgetRenderPass()`。
6. GL current context 被 `GLRhiSmokeTest` 抢走（GL 的 current context 是线程全局）。
   harness 加了 `MakeCurrent()` 每次重绑。
7. **Vulkan `prim_cubemap_` 粘滞标志**：只赋值、从不复位 → 画过天空盒后所有 `Draw()`
   （vertexless）误走天空盒 descriptor 分支 → 顶点数据全零 → 几何退化、静默无输出。
   已在 `BeginRenderPass` 复位 + 非 cube 绑定清标志。
8. 墙钟阈值抖动（unit x3 文件 + integration x1）：单次采样在共享机器上被放大 10~100 倍。
   改为 best-of-N（`tests/gtest/support/perf_probe.h`）并按实测基线重新标定预算。

### 验证链路 / 门禁（新增，均已进 CI）
- `tools/audit/gtest_report.py`：完整性（列出数 vs 实际执行数，抓 abort 吞用例）+ SKIP 逐条可见。
- `tools/audit/test_inventory.py`（`dse_test_inventory`）：源码 TEST 宏 vs 二进制实际用例对账。
- `tools/audit/rhi_matrix_report.py`：三后端覆盖矩阵 + 跨后端一致性证据，**某后端零覆盖即失败**。
- `tools/audit/repo_size_budget.py`（`dse_repo_size_budget`）：体积棘轮（8 MiB 每文件上限 + 总预算）。
- `tests/gtest/smoke/rhi_pixel_harness.{h,cpp}`：后端会话复用（GPU 电池 561 s → 103~140 s），
  含 `DSE_SMOKE_FRESH_DEVICE` / `DSE_SMOKE_GPU_DEBUG` / `RequestFreshDeviceForThisTest()` 三个出口。
- smoke 分层：`gtest.engine.smoke.headless`（无 GPU 依赖，进托管 CI）+ `gtest.engine.smoke.gpu`（真机）。
- CI：托管作业加完整性门禁；GPU 作业加 `schedule`（每夜）、`DSE_BUILD_EDITOR=ON`、RHI 矩阵、
  编辑器三后端矩阵。
- `scripts/verify_clean_room.ps1`：新增 `-RequireCleanHost` / `-Json` / `verdict=pass_on_dirty_host`。
- `scripts/test_editor_rhi_backends.ps1`：新增 `-Json`，逐后端记录 verdict。
- `docs/architecture/ADR_DEFERRED_REFACTORS.md`：四项待办重构的决策记录（**必读**）。

---

## 2. 剩余任务（按建议顺序）

### 任务 A（P2-1）：stencil 状态四端补齐 —— 第 1 步切片

**为什么先做它**：`PipelineStateDesc` 无模板字段是被点名最多次的「广度跑在深度前」证据，
且它是遮罩/描边/影体类特性的前置。ADR-2 已写清约束，**必须按 4 步切片，每步独立验证可回退**。

**第 1 步（本次只做这一步）**：加类型 + 加字段 + 默认关闭 + 单测，**确认零行为变化**。

验收：
```
1) rhi_types.h 新增 StencilOp / StencilFaceState / StencilState；
   PipelineStateDesc 增 `StencilState stencil;`，默认 enabled=false。
2) 四端 pipeline state manager 均不因默认值改变任何下发（Vulkan/DX11 仍写 stencilTestEnable=FALSE，
   GL 仍不调用 glStencil*）。
3) 新增单测：默认 desc 的 stencil.enabled == false；枚举映射（若本步引入）逐值正确。
4) 全量回归必须与第 3 节基线完全一致（尤其 smoke 338/338、unit 3451）。
```

后续步骤（第 2~4 步，见 ADR-2）：
- 第 2 步：`RenderTargetDesc` 加 `bool has_stencil=false`，四端 RT 创建接入对应格式
  （GL `GL_DEPTH24_STENCIL8` / DX11 `DXGI_FORMAT_D24_UNORM_S8_UINT` + DSV /
  Vulkan `D24S8` + render pass stencil attachment / WebGPU depthStencil 附件声明）。
- 第 3 步：四端 pipeline 接线（DX11 需把 reference 传给 `OMSetDepthStencilState`；
  WebGPU 需 `setStencilReference`）。
- 第 4 步：真机像素用例 —— 两个重叠面片，模板掩码后只有一个可见，三后端逐一验证。

**注意**：只做第 1、2 步而不做第 3 步会得到一套永远不生效的 API；每步合并前都要跑全量回归。

### 任务 B（P1-4 剩余）：干净机产物验收

**为什么没做完**：需要一台**无 VS / 无 VC++ Redist / 无 VULKAN_SDK** 的机器，本机不满足。

新会话若拿到干净机（VM / 云 Windows Server 镜像）：
```powershell
# 开发机：产出待测目录（SDK install 目录或 dse build 导出的 dist）
pwsh scripts/package_sdk.ps1            # 或 package_editor.ps1 / dse build
# 干净机：把目录拷过去后
pwsh scripts/verify_clean_room.ps1 -Dir <dist> -RequireCleanHost -Json clean_room.json
# 期望：verdict=pass / exit 0；若本机不干净则 verdict=fail / exit 1
```
验收：`clean_room.json.verdict == "pass"` 且 `host.clean == true`。

若**拿不到**干净机，能做的是：把 `-RequireCleanHost` 接进 release 流程文档与
`docs/RELEASE_GO_NO_GO.md`，并把「无干净机」明确写成未完成项（不要用开发机结果替代）。

### 任务 C（P1-4 剩余）：web 后端渲染 golden

`platformMatrix` 保持 `false` 的原因：WebGL2 / WebGPU 只有编译器验证，没有渲染截图 golden。
参考既有做法：`tests/gtest/smoke/shader_graph_preview_golden_test.cpp`（三桌面后端已有真机 golden），
以及 ledger 里 `shader-graph` 的 blocker 描述（记录了 WebGL2/WebGPU 怎么做编译器验证：
Chrome/ANGLE 与 Deno/wgpu）。要补的是**渲染截图**，不是编译。

### 任务 D（P2-2）：`CommandBuffer` 录制化（仅 Vulkan）

照 ADR-1 执行。关键约束：`CommandBuffer` 全部原语**刻意纯虚**（防某后端漏实现时静默吞绘制），
所以必须先用能力位 `SupportsDeferredRecording()` 隔离语义，并用 `DSE_VK_DEFERRED_RECORDING`
开关保留回退。

### 任务 E（P2-3）：资产统一 DTO 层

照 ADR-3：先做**读取路径** DTO 化，选最小格式（`.d9slice` 或 `.dlight2d`），
跑现有 `NineSliceVersioned.*` / `Light2DVersioned.*` 全绿即可合并。

### 任务 F（P2-4）：拆 `dse_engine` 单 target

照 ADR-4。按目录边界拆 OBJECT 库，保留聚合 target 使消费方无感；每拆一块跑一次全量回归。

---

## 3. 基线复现（每个任务开始前 / 结束后都要跑）

### 3.1 构建
```powershell
$vc = "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat"
cmd /c "`"$vc`" && cmake -S . -B out/build/windows-x64-debug && cmake --build out/build/windows-x64-debug --parallel 14"
```

### 3.2 测试（当前基线数值）
```powershell
# unit：3451 执行 / 3450 passed / 1 skipped / exit 0（约 50~75 s）
bin/dse_gtest_unit_tests.exe --gtest_brief=1

# integration：848 执行 / 844 passed / 4 skipped / exit 0（约 75~105 s）
bin/dse_gtest_integration_tests.exe --gtest_brief=1

# smoke（真机三后端）：338 / 338 passed / 0 skipped / exit 0（当前负载下 103~250 s）
bin/dse_gtest_smoke_tests.exe --gtest_brief=1

# 完整性 + SKIP 可见性（任一失败即不可信）
python tools/audit/gtest_report.py --json tmp/gtest_report.json bin/dse_gtest_unit_tests.exe bin/dse_gtest_integration_tests.exe

# 三后端矩阵（先产出 XML）
bin/dse_gtest_smoke_tests.exe --gtest_output=xml:tmp/smoke_xml/smoke.xml
python tools/audit/rhi_matrix_report.py --xml tmp/smoke_xml/smoke.xml --json tmp/rhi_matrix.json
# 期望：opengl 83 / d3d11 77 / vulkan 80 / cross 64，0 failed

# 编辑器窗口/ImGui 交换链三后端矩阵（需 dsengine-editor.exe）
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_editor_rhi_backends.ps1 -EditorExe bin/dsengine-editor.exe -Frames 3 -Json tmp/editor_matrix.json
# 期望：opengl/d3d11/vulkan 全 PASS / exit 0（约 57 s）

# 门禁
python tools/audit/verify_feature_ledger.py                 # exit 0
python tools/audit/test_inventory.py --bin-dir bin          # exit 0
python tools/audit/repo_size_budget.py                      # exit 0
python tools/audit/lua_api_audit.py                         # exit 0
python scripts/check_core_ui_free.py                        # exit 0
python tools/audit/verify_feature_ledger.py --release       # 期望 exit 1（NO-GO，未完成）
```

### 3.3 数字口径（对外引用必须用这个）
- **执行用例 = 4637**（unit 3451 + integration 848 + smoke 338），不是 4689（源码 TEST 宏数）。
- 差额 71 = 4 个特性开关的条件编译（PHYSX / JOLT / VIRTUAL_GEOMETRY / MEM_TRACKING），
  +19 来自生成器宏。由 `test_inventory.py` 每次自动核对。

---

## 4. 禁止事项

1. **不要提交/丢弃本会话的改动**：31 个改动条目是本次全部成果，未经提交。
2. **不要把开发机结果当干净机验收**：`verify_clean_room.ps1` 不加 `-RequireCleanHost` 时
   verdict 只会是 `pass_on_dirty_host`，它不是发布验收。
3. **不要为了让门禁变绿而调 acceptance**：`--release` 当前 exit 1 是**正确**的
   （`cleanMachine` 0/19、`platformMatrix` 2/19）。ADR 与 blockers 文本可以更新为事实，
   但判定不许改。
4. **不要用单次采样做性能阈值断言**：一律走 `perf_probe.h` 的 best-of-N
   （本会话实测同一个用例连跑 4 次得到 3 组不同失败集合）。
5. **不要手工放宽棘轮**：`repo_size_budget.py --update-baseline` 只许收紧；
   要放宽必须显式 `--force` 且写清理由。

---

## 5. 环境陷阱（本会话踩过，务必先读）

### 5.1 写文件会丢字符（最高频的坑）
用 PowerShell 写文件（here-string 落盘）时，**General Punctuation / Latin-1 符号块会被静默丢掉**。
实测会丢：em dash、右箭头、省略号、弯引号、中点、正负号、乘号、约等号、大于等于号。
**能活的**：ASCII + CJK 汉字 + 全角标点（`\uff08` `\uff09` `\uff1a` `\u300c` `\u300d`）。

做法：写源码/文档一律走 Python 脚本，需要这些符号时用 `\uXXXX` 转义；写完必须回读校验。
本会话因此修复过 5 处被吞掉的注释与 1 处被截断的整行。

### 5.2 `.ps1` 需要 BOM
`scripts/*.ps1` 原本带 UTF-8 BOM，**Windows PowerShell 5.1 没有 BOM 会把非 ASCII 注释按
ANSI 解析**。改写这些文件时用 `utf-8-sig` 读、写回时补回 `b'\xef\xbb\xbf'`，并验证：
```powershell
$null = [System.Management.Automation.PSParser]::Tokenize((Get-Content <file> -Raw), [ref]$null)
```

### 5.3 机器常年被别的会话压到 85~100% CPU
本会话所有墙钟数字都受此影响（同一用例：min-of-15 = 542 ms / 最差 1140 ms / 单次采样曾 3350 ms）。
**只引用 min-of-N**，且不要把耗时当验收证据；验收用「执行数 / 退出码 / 像素比对 / 标签选择」。

### 5.4 长命令必须在后台跑
工具单次调用 300 s 超时。构建、smoke 电池、integration 都要后台执行 + 轮询日志：
写成 `.cmd` 后用 `Start-Process cmd.exe -ArgumentList '/c','tmp/run.cmd' -WindowStyle Hidden`，
输出重定向到 `tmp/xxx.log` 并追加 `EXIT=%ERRORLEVEL%`。`tmp/` 已在 `.gitignore` 内，可随意使用。

**注意**：在 PowerShell here-string 里写 `'@` 会提前终止 here-string，
写这类脚本时用占位符再替换（本会话踩过）。

### 5.5 其他
- `ctest -T Test` 需要 `DartConfiguration.tcl`，Ninja 生成器**不产出**它；本机用不带 `-T` 的 `ctest`。
- 测试二进制在 `<repo>/bin`（不是构建目录）；`--bin-dir` 参数按需传。
- `dumpbin` 只在 VS 开发环境（`vcvars64.bat`）里可用。
- 改 `engine/` 下的**公共头**（如 `job_system.h`）会触发引擎全量 + 三个测试二进制重链（约 15 分钟）；
  改 `.cpp` 只重编该文件。排任务时把同类改动合并。
- 构建目录已开 `DSE_BUILD_EDITOR=ON`；若另建构建目录记得重新打开，
  否则 `dsengine-editor.exe` 不存在、编辑器矩阵跑不了。
- GL 的 current context 是**线程全局**：任何自建 GL 上下文的测试都可能抢走它，
  harness 里 `GLSession::MakeCurrent()` 是为此加的，不要删。

---

## 6. 交付标准（新会话收尾时）

1. 第 3 节全部命令复现通过，且没有一项数值低于基线。
2. 新增/修改的每个行为都有对应测试；断言失败信息能区分「真问题」与「机器噪声」。
3. `git diff --check` 干净；改动文件编码审计通过（UTF-8 无意外 BOM、CRLF 一致）。
4. 被改动的门禁语义变化（若有）必须写进 `docs/` 并说明理由。
5. 未完成项要显式留在 ledger 的 blockers 里，**不许静默降级为通过**。

---

## BASELINE-UPDATE（后续会话追加）

ADR-2（stencil）四步落库后，基线数字有变化，第 3 节以本表为准：

| 项 | 数字 |
|---|---|
| unit | 3451 执行 / 3450 passed / 1 skip / exit 0（不变） |
| integration | 848 / 844 passed / 4 skip / exit 0（不变） |
| smoke | **341** / 341 passed / 0 skip / exit 0（原 338，新增 3 条 stencil 用例） |
| rhi matrix | opengl **84** / d3d11 **78** / vulkan **81** / cross 64，0 failed（原 83/77/80/64） |

其余门禁（gtest_report / test_inventory / repo_size_budget / lua_api_audit / check_core_ui_free / verify_feature_ledger / 编辑器三后端矩阵）全部 exit 0；`verify_feature_ledger.py --release` 仍 exit 1（NO-GO，预期）。

已定位并修掉的历史拦路问题见 docs/architecture/ADR_DEFERRED_REFACTORS.md 的 RESOLVED-FP-1 与 STEP2-DONE 两节（desc 结构体默认初始化会留未初始化成员，已统一改值初始化 `T x{};`）。

## ADR-3-STEP1（后续会话追加）

- 四个本地 2D 格式的读取路径已迁移到 `engine/core/asset_dto.h` 的 `FieldDesc` + `ReadFields`：`.d9slice` / `.dparticle2d` / `.dparallax` / `.dlight2d`；写路径未动。
- 定向测试：`NineSliceVersioned.*` / `Particle2DVersioned.*` / `ParallaxVersioned.*` / `Light2DVersioned.*` = 14/14 全绿。
- 全量基线：unit 3451/3450+1skip、integration 848/844+4skip、smoke 341/341，均 exit 0。
- ADR-3 读取与写入路径均已覆盖四个本地 2D 格式 + 六种内容格式；后续仅剩数组/嵌套 body 元数据通用化与矩阵化回归。

## ADR-1 / ADR-4 / ADR-3 更新（后续会话追加）

- **ADR-1 完成**：Vulkan `CommandBuffer` 录制状态已迁入 `VulkanCommandState`，按命令缓冲隔离；`SupportsDeferredRecording()` 已加入 RHI 接口并在 Vulkan smoke 中断言。
- **ADR-4 首个切片完成**：`CMakeLists.txt` 已把 `engine_cpp` 拆为多个 `dse_engine_obj_*` OBJECT target，`dse_engine` 保留聚合 target；原有 include/def/option/link/LTO/PCH 通过配置结束后复制保持。
- **ADR-3 读写路径完成**：四个本地 2D 格式 + 六种内容格式（`.dbp` / `.dshadergraph` / `.dasm` / `.dsequence` / `.dcutscene` / `.dscriptmeta`）的读取与写入路径均已迁移到 DTO/字段表；后续仅剩数组/嵌套 body 的元数据通用化与矩阵化回归。
- 本轮全量：unit 3451/3450+1skip、integration 848/844+4skip、smoke 341/341、rhi matrix 84/78/81/64、编辑器三后端矩阵 pass；release 仍 exit 1（预期 NO-GO）。
- 构建环境注意：当前机器 CMake/MSVC 增量依赖解析依赖 `VSLANG=1033`，本轮完整重编时已在 build 命令中设置，否则 `/showIncludes` 本地化会导致 Ninja depfile 为空。
