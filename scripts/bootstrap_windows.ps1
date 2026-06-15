<#
.SYNOPSIS
    全新 Windows 机器一键准备 DSEngine 开发环境：安装工具链(CMake/Ninja/VS2022 C++
    Build Tools) → 初始化 git submodule → (可选)进 VS dev shell 跑 configure/build/test。
.DESCRIPTION
    幂等，可在新虚拟机上重复执行：已安装的组件会被跳过。配合 CMakePresets 使用，
    与 scripts/win/build_fast_*.bat 互补（本脚本负责装环境，那些脚本负责日常构建）。
    需要管理员权限以安装 Chocolatey 包。
.PARAMETER Preset
    CMake preset，默认 windows-x64-debug（另有 -relwithdebinfo / -release）。
.PARAMETER SkipInstall
    跳过工具链安装（仅初始化 submodule 并构建）。
.PARAMETER SkipSubmodules
    跳过 git submodule 初始化。
.PARAMETER Build
    安装后立即 configure + build。
.PARAMETER Test
    构建后运行 ctest（隐含 -Build）。
.EXAMPLE
    # 新机首次：装工具链 + 拉依赖 + 构建并测试
    powershell -ExecutionPolicy Bypass -File scripts\bootstrap_windows.ps1 -Test
.EXAMPLE
    # 环境已就绪，仅重新构建
    powershell -ExecutionPolicy Bypass -File scripts\bootstrap_windows.ps1 -SkipInstall -SkipSubmodules -Build
#>

[CmdletBinding()]
param(
    [string]$Preset = "windows-x64-debug",
    [switch]$SkipInstall,
    [switch]$SkipSubmodules,
    [switch]$Build,
    [switch]$Test
)

# cmake/ctest 的 stderr 警告会被 PowerShell 当作终止错误，这里统一手动检查 $LASTEXITCODE
$ErrorActionPreference = "Continue"
$SourceDir = (Resolve-Path "$PSScriptRoot\..").Path

function Write-Step($msg) { Write-Host "`n>> $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "   [OK] $msg" -ForegroundColor Green }
function Die($msg) { Write-Host "   [FAIL] $msg" -ForegroundColor Red; exit 1 }

if ($Test) { $Build = $true }

$VsRoot = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"

# ── 1. 工具链 ────────────────────────────────────────────────────────────────
if (-not $SkipInstall) {
    Write-Step "安装工具链 (Chocolatey: cmake / ninja / VS2022 C++ Build Tools)"
    if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
        Die "未找到 Chocolatey。请先安装：https://chocolatey.org/install"
    }
    choco install -y cmake ninja
    if ($LASTEXITCODE -ne 0) { Die "安装 cmake/ninja 失败。" }
    choco install -y visualstudio2022buildtools visualstudio2022-workload-vctools
    if ($LASTEXITCODE -ne 0) { Die "安装 VS2022 C++ Build Tools 失败。" }
    Write-OK "工具链安装完成"
} else {
    Write-OK "跳过工具链安装 (-SkipInstall)"
}

# choco 装好后当前会话 PATH 可能尚未刷新，这里显式补上
$env:Path = "C:\Program Files\CMake\bin;C:\ProgramData\chocolatey\bin;" + $env:Path

# ── 2. 子模块 ────────────────────────────────────────────────────────────────
if (-not $SkipSubmodules) {
    Write-Step "初始化 git submodule (depends/)"
    git -C $SourceDir submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { Die "git submodule 初始化失败。" }
    Write-OK "子模块就绪"
} else {
    Write-OK "跳过子模块初始化 (-SkipSubmodules)"
}

if (-not $Build) {
    Write-Step "完成（未指定 -Build/-Test，跳过构建）"
    Write-Host "   后续构建： powershell -ExecutionPolicy Bypass -File scripts\bootstrap_windows.ps1 -SkipInstall -SkipSubmodules -Test" -ForegroundColor Yellow
    exit 0
}

# ── 3. 进 VS x64 开发者环境 ───────────────────────────────────────────────────
Write-Step "进入 VS2022 x64 开发者环境"
$DevShellDll = Join-Path $VsRoot "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
if (-not (Test-Path $DevShellDll)) { Die "未找到 VS dev shell：$DevShellDll。请先安装 VS2022 C++ Build Tools。" }
Import-Module $DevShellDll
Enter-VsDevShell -VsInstallPath $VsRoot -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
Set-Location $SourceDir
Write-OK "MSVC 环境就绪"

# ── 4. 配置 + 构建 ───────────────────────────────────────────────────────────
Write-Step "配置 (cmake --preset $Preset)"
cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { Die "CMake 配置失败。" }

Write-Step "构建 (cmake --build --preset $Preset)"
cmake --build --preset $Preset
if ($LASTEXITCODE -ne 0) { Die "构建失败。" }
Write-OK "构建完成，产物在 bin/"

# ── 5. 测试 ──────────────────────────────────────────────────────────────────
if ($Test) {
    Write-Step "运行 GoogleTest (ctest --preset $Preset)"
    ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { Die "测试失败。" }
    Write-OK "全部测试通过"
}

Write-Step "完成"
