<#
.SYNOPSIS
    把 MSVC C/C++ 运行时 (VC++ Redistributable) DLL 以 app-local 方式拷到 -DestDir。
.DESCRIPTION
    DSEngine 的可执行文件 / DLL 动态链接 CRT（导入 VCRUNTIME140.dll / MSVCP140.dll
    等）。干净的 Windows 机器若未安装 "Visual C++ 2015-2022 Redistributable"，双击即
    报 "VCRUNTIME140.dll 缺失"。把这些 DLL 随包发行（Microsoft 允许 app-local 部署）
    即可开箱即用，且不改变 ABI、无需重新链接。

    仅发行 Release CRT，绝不发行 *140d.dll（调试版 CRT 不可再分发）。
    Universal CRT (api-ms-win-crt-*) 自 Windows 10 起随系统提供，默认不打包。
.PARAMETER DestDir
    目标目录（DLL 拷到这里）。
.PARAMETER IncludeUCRT
    额外打包 Universal CRT 重分发 DLL（面向更老的 Win7/8 目标，体积更大）。
.EXAMPLE
    pwsh scripts/collect_runtime_deps.ps1 -DestDir build\install\bin
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$DestDir,
    [switch]$IncludeUCRT
)

$ErrorActionPreference = "Stop"

# 必需 + 可选的 VC++ 运行时 DLL（Release）。
$requiredDlls = @("vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll")
$optionalDlls = @(
    "msvcp140_1.dll", "msvcp140_2.dll", "msvcp140_atomic_wait.dll",
    "msvcp140_codecvt_ids.dll", "concrt140.dll", "vccorlib140.dll"
)

function Get-VcRedistDir {
    # 1) 开发者环境变量（VS dev shell 中已设置）。
    if ($env:VCToolsRedistDir) {
        $d = Get-ChildItem -Path (Join-Path $env:VCToolsRedistDir "x64") -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending | Select-Object -First 1
        if ($d) { return $d.FullName }
    }
    # 2) vswhere 定位 VS 安装，再找 Redist。
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -property installationPath 2>$null
        if ($vsPath) {
            $d = Get-ChildItem -Path (Join-Path $vsPath "VC\Redist\MSVC") -Directory -ErrorAction SilentlyContinue |
                ForEach-Object { Get-ChildItem -Path (Join-Path $_.FullName "x64") -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue } |
                Sort-Object Name -Descending | Select-Object -First 1
            if ($d) { return $d.FullName }
        }
    }
    # 3) 常见安装路径兜底（Community/Professional/Enterprise/BuildTools）。
    $glob = @(
        "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT"
    )
    $d = Get-ChildItem -Path $glob -Directory -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($d) { return $d.FullName }
    return $null
}

if (-not (Test-Path $DestDir)) { New-Item -ItemType Directory -Force -Path $DestDir | Out-Null }

$redistDir = Get-VcRedistDir
$system32  = Join-Path $env:WINDIR "System32"
$copied = @()
$missing = @()

function Copy-FromSources([string]$name, [bool]$required) {
    # 优先 redist 目录（规范的可再分发副本），否则回退 System32。
    $src = $null
    if ($redistDir) {
        $p = Join-Path $redistDir $name
        if (Test-Path $p) { $src = $p }
    }
    if (-not $src) {
        $p = Join-Path $system32 $name
        if (Test-Path $p) { $src = $p }
    }
    if ($src) {
        Copy-Item $src (Join-Path $DestDir $name) -Force
        $script:copied += $name
        return $true
    }
    if ($required) { $script:missing += $name }
    return $false
}

if ($redistDir) {
    Write-Host "VC++ Redist 源: $redistDir"
} else {
    Write-Host "未找到 VC++ Redist 目录，回退到 $system32"
}

foreach ($d in $requiredDlls) { [void](Copy-FromSources $d $true) }
foreach ($d in $optionalDlls) { [void](Copy-FromSources $d $false) }

if ($IncludeUCRT) {
    $ucrtDir = $null
    if ($redistDir) {
        # Microsoft.VC*.CRT 同级常有 Microsoft.VC*.OPENMP；UCRT 在 Windows Kits 下。
    }
    $kit = "C:\Program Files (x86)\Windows Kits\10\Redist"
    $ucrt = Get-ChildItem -Path $kit -Directory -Recurse -Filter "x64" -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName "ucrtbase.dll") } |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($ucrt) {
        Get-ChildItem -Path $ucrt.FullName -Filter "api-ms-win-crt-*.dll" -ErrorAction SilentlyContinue |
            ForEach-Object { Copy-Item $_.FullName (Join-Path $DestDir $_.Name) -Force; $script:copied += $_.Name }
        $u = Join-Path $ucrt.FullName "ucrtbase.dll"
        if (Test-Path $u) { Copy-Item $u (Join-Path $DestDir "ucrtbase.dll") -Force; $script:copied += "ucrtbase.dll" }
    } else {
        Write-Host "未找到 UCRT 重分发目录，跳过 (-IncludeUCRT)。" -ForegroundColor Yellow
    }
}

Write-Host "已拷贝 $($copied.Count) 个运行时 DLL -> $DestDir"
$copied | Sort-Object -Unique | ForEach-Object { Write-Host "   + $_" }

if ($missing.Count -gt 0) {
    Write-Host "缺少必需的运行时 DLL: $($missing -join ', ')" -ForegroundColor Red
    Write-Host "请在本机安装 'Visual C++ 2015-2022 Redistributable (x64)' 后重试。" -ForegroundColor Red
    exit 1
}

exit 0
