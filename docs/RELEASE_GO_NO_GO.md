# DSEngine 发布 Go/No-Go 报告（P0-1 / P2-3）

> 结论：**NO-GO（V1 对外发布未放行）**。核心「干净机（clean-machine）发布全矩阵」门禁
> 仍为 **OPEN**。本报告记录已在真机 RTX 3070 开发机（**预装依赖，非干净机**）取得的部分证据，
> 以及必须在真正干净机器上补齐的剩余项。

## 一、验证环境

- 机器：Windows，NVIDIA GeForce RTX 3070 Laptop GPU（真实独显，`software=0`）。
- 性质：**开发机**，已预装 Visual Studio 2022 BuildTools / MSVC 14.44 / Vulkan runtime /
  .NET 8 / 各第三方依赖（sodium / protobuf / OpenSSL）。
- 因此本机 **不满足** clean-room 前置（`docs/CLEAN_ROOM_TEST.md` 第一节要求无 VC++ Redist、
  无 `VULKAN_SDK`）。本报告中的证据仅证明「发布流水线在已配置机器上端到端可跑通」，
  **不能**替代干净机验收。

## 二、已取得的部分证据（真机 RTX 3070，非干净机）

| 项 | 命令 / 目标 | 结果 |
| --- | --- | --- |
| 构建 | `dse_cli` + `dse_standalone` + `dse_gtest_integration_tests`（Debug） | 成功链接 `bin\dse.exe`、`bin\dsengine_game_*.exe` |
| 打包 | `dse build <project> --out=<dir>` → `dse dist --target=win --in=<dir>` | rc=0，产出 `*-win-x64.zip`（约 5.36 MB） |
| 产物冒烟 | 解包后运行导出 exe，`DSE_MAX_FRAMES=8` | 真 GPU 启动（OpenGL 后端；Hi-Z、GPU-Driven Rendering `supported=1`、Physics3D(Jolt)、NavMesh、Grass/Tree/Hair GPU compute 全 init），加载入口脚本 → 跑满 8 帧 → `DSE_MAX_FRAMES reached: 8` → 干净退出 **rc=0** |
| 发布矩阵门禁 | `dse_gtest_integration_tests --gtest_filter=ReleaseMatrixTest.*` | **10/10 PASS**（feature ledger、editor command vocabulary、Undo/BuildService/CrashHandler/GitClient/AssetDatabase/2D-tools API、CI/Release workflow 存在性） |
| 三 RHI GPU 稳定性（P2-2 交叉引用） | `ThreeRhiSoak.OpenGL/D3D11/Vulkan`，250 轮 | 三后端本地显存零增长、句柄稳定、工作集在预算内（详见 `tests/gtest/smoke/three_rhi_soak_test.cpp` 与 P2-2 台账证据） |

## 三、仍为 OPEN 的门禁（必须在真正干净机器上完成，否则 No-Go）

1. **干净机 clean-room 验证**：在无 VS / 无 VC++ Redist / 无 `VULKAN_SDK` 的机器上，对导出包运行
   `pwsh scripts/verify_clean_room.ps1 -Dir <包目录>`，五步全过：
   环境体检、静态 CRT 审计（禁 debug CRT）、许可证合规（`THIRD_PARTY_LICENSES.md`）、
   动态依赖导入表检查（`dumpbin /DEPENDENTS`）、启动冒烟（识别 `0xC0000135` 缺 DLL）。
2. **全平台矩阵产物验收**：`windows/{opengl,d3d11,vulkan}` 三后端 + `web/webgl2` + `web/webgpu`，
   均从「下载到的发布产物」在干净机上启动验证，而非在构建机上就地跑。
3. **无独显 / 远程桌面回退**：确认干净机在无独显环境下自动回退（D3D11 WARP / OpenGL）不黑屏。
4. **Go/No-Go 台账闸门**：`python tools/audit/verify_feature_ledger.py --release` 必须通过——
   要求所有 feature `complete`/`retired`、且 `editor_production_debt.json` 无遗留条目。当前尚有
   `in_progress` feature 与生产债目，故该闸门 **未通过**。

## 四、结论与放行条件

- **当前决策：NO-GO。** 发布流水线（构建→打包→产物启动→发布矩阵门禁）已在配置机上端到端验证可跑通，
  但 clean-machine 全矩阵验收未做，`--release` 台账闸门未过。
- **放行条件**：在真正干净机器上完成第三节 1–4 全部项并留证据，且 `verify_feature_ledger.py --release`
  返回「Go/No-Go gate passed」。
