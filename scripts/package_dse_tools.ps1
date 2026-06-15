<#
.SYNOPSIS
    打包"预编译 dse 工具包"：dse.exe + 运行时 DSEngine_Game.exe(+依赖) -> 一个 ZIP。
.DESCRIPTION
    新手无需先用 bootstrap_windows.ps1 从源码构建整套引擎，只要下载本工具包解压，
    即可用 `dse new / build / dist` 创建并打包游戏。运行时动态链接 MSVC CRT，
    本包已随附 app-local 的 VC++ 运行时 DLL，未装 VC++ Redistributable 也可开箱即用。

    流程：进 VS2022 x64 开发者环境 -> 用指定预设配置+编译 dse_cli 与 dse_standalone
    -> 从 bin/ 收集 dse.exe / DSEngine_Game.exe(+任何同目录 DLL) -> 写 README -> 打 ZIP。

    适合作为 GitHub Release 资产或 CI 产物：把生成的 zip 直链挂到 README 下载处即可。
.PARAMETER Preset
    CMake 预设。默认 windows-x64-release（产出无 _debug 后缀的运行时 DSEngine_Game.exe）。
.PARAMETER OutDir
    ZIP 输出目录。默认 <repo>/dist。
.PARAMETER SkipBuild
    跳过配置/编译，直接用现有 bin/ 产物打包（要求已构建过对应预设）。
.PARAMETER SkipInstall
    跳过工具链安装检查（沿用当前已就绪的 MSVC 环境）。
.EXAMPLE
    pwsh scripts/package_dse_tools.ps1
.EXAMPLE
    pwsh scripts/package_dse_tools.ps1 -Preset windows-x64-debug -SkipBuild
.NOTES
    仅打包命令行工具链（dse + 运行时）。完整 C++ SDK（find_package）见 package_sdk.ps1。
#>
[CmdletBinding()]
param(
    [string]$Preset = "windows-x64-release",
    [string]$OutDir = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$SourceDir = (Resolve-Path "$PSScriptRoot\..").Path
if (-not $OutDir) { $OutDir = Join-Path $SourceDir "dist" }

# ── 版本号（与 package_sdk.ps1 同源：CMakeLists.txt 的 VERSION + PRERELEASE）──
$versionLine = Select-String -Path "$SourceDir\CMakeLists.txt" -Pattern 'project\(DSEngine\s+VERSION\s+(\S+)' | Select-Object -First 1
$Version = if ($versionLine) { $versionLine.Matches[0].Groups[1].Value } else { "0.0.0" }
$prLine = Select-String -Path "$SourceDir\CMakeLists.txt" -Pattern 'set\(DSEngine_VERSION_PRERELEASE\s+"([^"]*)"' | Select-Object -First 1
if ($prLine -and $prLine.Matches[0].Groups[1].Value -ne "") {
    $Version = "$Version-$($prLine.Matches[0].Groups[1].Value)"
}

$PkgName   = "DSEngine-tools-v${Version}-win-x64"
$BinDir    = Join-Path $SourceDir "bin"
$StageDir  = Join-Path $SourceDir "build_dse_tools\$PkgName"
$OutputZip = Join-Path $OutDir "$PkgName.zip"

Write-Host "========================================"
Write-Host "  DSEngine 工具包打包器"
Write-Host "  Version : $Version"
Write-Host "  Preset  : $Preset"
Write-Host "  Output  : $OutputZip"
Write-Host "========================================"

# ── 1. 配置 + 编译（除非 -SkipBuild）─────────────────────────────────
if (-not $SkipBuild) {
    $VsRoot = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
    $DevShellDll = Join-Path $VsRoot "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    if (-not (Test-Path $DevShellDll)) {
        throw "未找到 VS dev shell：$DevShellDll。请先安装 VS2022 C++ Build Tools（见 bootstrap_windows.ps1）。"
    }
    $env:Path = "C:\Program Files\CMake\bin;C:\ProgramData\chocolatey\bin;" + $env:Path
    Import-Module $DevShellDll
    Enter-VsDevShell -VsInstallPath $VsRoot -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
    Set-Location $SourceDir

    Write-Host "`n[1/4] 配置 (cmake --preset $Preset)..."
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败 (exit $LASTEXITCODE)" }

    Write-Host "`n[2/4] 编译 dse_cli + dse_standalone..."
    & cmake --build --preset $Preset --target dse_cli dse_standalone
    if ($LASTEXITCODE -ne 0) { throw "CMake 编译失败 (exit $LASTEXITCODE)" }
}

# ── 2. 收集产物 ─────────────────────────────────────────────────────
Write-Host "`n[3/4] 收集 dse 工具链产物..."
if (Test-Path $StageDir) { Remove-Item -Recurse -Force $StageDir }
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

# dse.exe（CLI）必需。
$dseExe = Join-Path $BinDir "dse.exe"
if (-not (Test-Path $dseExe)) {
    throw "未找到 $dseExe。请去掉 -SkipBuild 先构建，或先 cmake --build --preset $Preset --target dse_cli。"
}
Copy-Item $dseExe (Join-Path $StageDir "dse.exe") -Force

# 运行时：release 为 DSEngine_Game.exe，debug 为 DSEngine_Game_debug.exe，任取其一。
$runtimeNames = @("DSEngine_Game.exe", "DSEngine_Game_release.exe", "DSEngine_Game_debug.exe")
$runtimeSrc = $null
foreach ($n in $runtimeNames) {
    $cand = Join-Path $BinDir $n
    if (Test-Path $cand) { $runtimeSrc = $cand; break }
}
if (-not $runtimeSrc) {
    throw "未找到运行时 DSEngine_Game(.exe)。请构建 dse_standalone 目标。"
}
# 统一落为 DSEngine_Game.exe，使 dse build 的 LocateRuntimeExe 在同目录即可命中。
Copy-Item $runtimeSrc (Join-Path $StageDir "DSEngine_Game.exe") -Force

# 同目录的依赖 DLL（引擎 / 插件运行时 DLL，有则一并带上）。
$dllCount = 0
Get-ChildItem $BinDir -Filter *.dll -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $StageDir $_.Name) -Force
    $dllCount++
}
Write-Host "  + dse.exe"
Write-Host "  + DSEngine_Game.exe (源: $(Split-Path -Leaf $runtimeSrc))"
Write-Host "  + $dllCount 个依赖 DLL"

if (Test-Path "$SourceDir\LICENSE") { Copy-Item "$SourceDir\LICENSE" $StageDir -Force }

# ── 第三方许可证聚合（合规） ──
Write-Host "`n[+] 聚合第三方许可证 (THIRD_PARTY_LICENSES.md)..."
& "$PSScriptRoot\collect_third_party_licenses.ps1" -SourceDir $SourceDir -OutFile (Join-Path $StageDir "THIRD_PARTY_LICENSES.md")
if ($LASTEXITCODE -ne 0) { throw "第三方许可证聚合失败 (exit $LASTEXITCODE)" }

# ── 随包附带 VC++ 运行时 (app-local)：dse.exe / DSEngine_Game.exe 动态链接 CRT，
#    未装 VC++ Redistributable 的机器靠这些 DLL 才能启动 ──
Write-Host "`n[+] 收集 VC++ 运行时 DLL..."
& "$PSScriptRoot\collect_runtime_deps.ps1" -DestDir $StageDir
if ($LASTEXITCODE -ne 0) { throw "VC++ 运行时收集失败 (exit $LASTEXITCODE)；请安装 'Visual C++ 2015-2022 Redistributable (x64)' 后重试" }

# ── 发行前依赖审计：揪出误带的 debug/dev DLL，确认运行时齐备 ──
Write-Host "`n[+] 依赖审计 (发行前)..."
& "$PSScriptRoot\audit_runtime_deps.ps1" -Dir $StageDir -RequireRuntime
if ($LASTEXITCODE -ne 0) { throw "依赖审计未通过 (exit $LASTEXITCODE)" }

# ── 3. README ───────────────────────────────────────────────────────
$readme = @"
# DSEngine 工具包 v${Version} (win-x64)

预编译命令行工具链，开箱即用，无需从源码构建引擎。

## 包含
- ``dse.exe``            —— 命令行工具：new / pack / build / dist
- ``DSEngine_Game.exe``  —— standalone 运行时（被 ``dse build`` 复制进出包）
- ``vcruntime140*.dll`` / ``msvcp140*.dll`` —— app-local VC++ 运行时（开箱即用）
- ``THIRD_PARTY_LICENSES.md`` —— 第三方组件许可证汇总

## 快速上手
将本目录加入 PATH（或在本目录内执行），然后：

``````
dse new lua MyGame          # 生成项目骨架
dse build MyGame --out MyGame\build\dist
                            # 出包：exe + game.bun + game.dsmanifest，双击即玩
dse dist --target win --in MyGame\build\dist
                            # 打成可分发 zip（Export Template）
``````

无独显机器（远程桌面 / VM）首次启动报 "Failed to create GLFW window" 时，
先用仓库 ``scripts/setup_swgl.ps1`` 部署软件 OpenGL，再 ``dse build --with-swgl``。

构建时间: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
"@
Set-Content -Path (Join-Path $StageDir "README.md") -Value $readme -Encoding UTF8

# ── 4. 打 ZIP ───────────────────────────────────────────────────────
Write-Host "`n[4/4] 打包 -> $OutputZip ..."
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (Test-Path $OutputZip) { Remove-Item $OutputZip -Force }
Compress-Archive -Path "$StageDir\*" -DestinationPath $OutputZip -CompressionLevel Optimal

$zipSize = (Get-Item $OutputZip).Length / 1MB
Write-Host "`n完成！工具包: $OutputZip ($([math]::Round($zipSize, 2)) MB)"
Write-Host "可作为 GitHub Release 资产上传，并把直链挂到 README 下载处。"
