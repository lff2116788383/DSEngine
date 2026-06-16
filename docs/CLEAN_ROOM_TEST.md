# 干净机（Clean-Room）发布验证清单

CI 的构建与打包都在装了 Visual Studio + Vulkan SDK 的开发机上运行，**无法**暴露以下
面向最终用户的发布问题：

- 漏带 VC++ 运行时（`vcruntime140.dll` / `msvcp140.dll`）—— 未装 “VC++ 2015-2022
  Redistributable” 的用户双击即报错。
- 漏带第三方许可证（`THIRD_PARTY_LICENSES.md`）—— 公开发布的合规缺口。
- 某依赖 DLL 只在开发机存在（如 Vulkan SDK 带来的 DLL），干净机上缺失。
- 无独显 / 远程桌面 / VM 环境下的 GPU 后端回退是否真的生效。

因此每次发布前，必须在一台 **干净机** 上跑一遍实测。

## 一、准备干净机

满足以下任一即可（越干净越好）：

- 一台从未安装过 Visual Studio / Windows SDK / Vulkan SDK 的 Windows 10/11；或
- 全新的 Windows VM（Hyper-V / VMware / 云上 Windows Server 镜像）；或
- GitHub Actions 上的 `windows-latest` runner **加一步**：先卸载或不依赖已装的
  VC++ Redist（注意默认 runner 已装 VS，不是理想干净机，仅作兜底）。

确认这台机器：**没有**装 VC++ Redistributable、**没有** `VULKAN_SDK` 环境变量。

## 二、产出待测包

在开发机上正常构建并打包，把产物（整目录）拷到干净机：

- SDK 包：`scripts/package_sdk.ps1` 的 install 目录。
- 工具包：`scripts/package_dse_tools.ps1` 的产物。
- 导出游戏：`dse build <project> --out <dir>` 的 `dist` 目录。

## 三、自动化验证

在干净机上对待测目录运行：

```powershell
pwsh scripts/verify_clean_room.ps1 -Dir <待测目录>
```

脚本会依次执行并在任一步失败时返回非零退出码（可接入 CI gating）：

1. **环境体检** —— 报告本机是否装了 VC++ Redist / VS / Vulkan SDK；若装了会
   告警（说明这台不是真正的干净机）。
2. **静态审计** —— 复用 `audit_runtime_deps.ps1`：禁止 debug CRT、要求 Release CRT 齐全。
3. **许可证合规** —— 确认目录内有 `THIRD_PARTY_LICENSES.md`。
4. **动态依赖检查** —— `dumpbin /DEPENDENTS` 解析每个 exe/dll 的导入表，任何既不在
   包内、又非已知系统/UCRT 的 DLL 视为失败（干净机会因此启动崩溃）。
5. **启动冒烟** —— 启动游戏 exe，专门识别退出码 `0xC0000135`（缺 DLL）。

> 第 4 步依赖 `dumpbin.exe`（VS 自带）。若干净机上没有，可先在开发机上对同一个包
> 跑一遍第 4 步，再到干净机上跑 1/2/3/5。

## 四、人工复核（脚本之外）

- [ ] 双击游戏 exe，**不**弹出任何“缺少 XXX.dll”对话框，窗口正常出现。
- [ ] 在 **无独显 / 远程桌面** 环境下双击，确认自动回退到 D3D11 WARP 或 OpenGL，
      画面正常、不黑屏（见 `frame_pipeline.cpp` 的后端回退、`dx11_context.cpp` 的
      WARP 回退）。可用 `DSE_RHI_BACKEND=opengl` 强制验证 GL 路径。
- [ ] 打开 `THIRD_PARTY_LICENSES.md`，确认“待补充”为 0、各依赖许可证全文齐全。
- [ ] 跑一遍 SDK 的 `find_package(DSEngine)` → consumer 构建并运行（即
      `scripts/verify_sdk.ps1` 的端到端链路），且 consumer 的产物也能在干净机上跑。

全部通过后方可对外发布。
