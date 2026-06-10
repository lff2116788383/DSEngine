<#
.SYNOPSIS
    Windows(MSVC) 端到端构建验证：CMake 配置(VS2022 x64) → 构建 dse_engine
    静态库（可选连同 gtest 目标）→ 校验产物 DSEngine_<cfg>.lib 存在。
.DESCRIPTION
    用于 CI 或本地确认引擎在 Windows/MSVC 可构建。与 verify_linux_build.sh /
    verify_android_apk.ps1 配套，三端各一个一键构建验证脚本。
    需要：Visual Studio 2022 (C++ v143 工具集) + CMake。
.PARAMETER Config
    构建配置 Debug/Release，默认 Debug。
.PARAMETER Arch
    目标架构，默认 x64。
.PARAMETER Generator
    CMake 生成器，默认 "Visual Studio 17 2022"。
.PARAMETER BuildDir
    构建目录，默认 <repo>/build_vs2022。已存在则增量构建。
.PARAMETER WithTests
    一并构建 gtest 目标（unit/integration/smoke）。
.EXAMPLE
    .\scripts\verify_windows_build.ps1
    .\scripts\verify_windows_build.ps1 -Config Release -WithTests
#>

[CmdletBinding()]
param(
    [ValidateSet("Debug","Release")][string]$Config = "Debug",
    [string]$Arch = "x64",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$BuildDir = "",
    [switch]$WithTests
)

# cmake 的 stderr 警告会被 PowerShell 当作终止错误，这里统一手动检查 $LASTEXITCODE
$ErrorActionPreference = "Continue"
$SourceDir = (Resolve-Path "$PSScriptRoot\..").Path

function Write-Step($msg) { Write-Host "`n>> $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "   [OK] $msg" -ForegroundColor Green }
function Write-FAIL($msg) { Write-Host "   [FAIL] $msg" -ForegroundColor Red }
function Die($msg) { Write-FAIL $msg; exit 1 }

# ── 1. 工具链 ───────────────────────────────────────────────────────────────
Write-Step "检查工具链"
$CMakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
$CMake = if ($CMakeCmd) { $CMakeCmd.Source } elseif (Test-Path "C:/Program Files/CMake/bin/cmake.exe") { "C:/Program Files/CMake/bin/cmake.exe" } else { $null }
if (-not $CMake) { Die "找不到 cmake。请安装 CMake 或加入 PATH。" }
Write-OK "cmake=$CMake"
Write-OK "源码目录=$SourceDir"

if (-not $BuildDir) { $BuildDir = Join-Path $SourceDir "build_vs2022" }
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)

# ── 2. 配置 ─────────────────────────────────────────────────────────────────
Write-Step "配置 CMake ($Generator $Arch, $Config)"
$testsFlag = if ($WithTests) { "ON" } else { "OFF" }
if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
    & $CMake -S $SourceDir -B $BuildDir -G $Generator -A $Arch `
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" `
        "-DDSE_BUILD_SHARED=OFF" `
        "-DDSE_ENABLE_3D=ON" `
        "-DDSE_ENABLE_JOLT=ON" `
        "-DDSE_ENABLE_PHYSX=OFF" `
        "-DDSE_ENABLE_D3D11=ON" `
        "-DDSE_ENABLE_VULKAN=OFF" `
        "-DDSE_BUILD_GTESTS=$testsFlag"
    if ($LASTEXITCODE -ne 0) { Die "CMake 配置失败。" }
} else {
    Write-OK "复用已有构建目录：$BuildDir"
}

# 着色器嵌入头(engine/render/shaders/generated/)在源码树共享。非 Windows 构建用的
# 着色器编译器无法生成 DXBC(DX11 字节码)，会把这些头覆盖成不含 DXBC 的版本，导致此处
# DX11 路径编译报 'k*_dxbc 未声明'。检测到缺 DXBC 时用 Windows 编译器重新生成。
Write-Step "检查 DX11 着色器嵌入头 (DXBC)"
$genEmbed  = Join-Path $SourceDir "engine/render/shaders/generated/embed"
$shaderExe = Join-Path $SourceDir "bin/dse_shader_compiler.exe"
$hasDxbc = $false
if (Test-Path $genEmbed) {
    foreach ($f in (Get-ChildItem $genEmbed -Filter *.gen.h -ErrorAction SilentlyContinue)) {
        if (Select-String -Path $f.FullName -Pattern "dxbc" -SimpleMatch -Quiet) { $hasDxbc = $true; break }
    }
}
if (-not $hasDxbc) {
    if (Test-Path $shaderExe) {
        Write-Host "   生成的着色器头缺少 DXBC（可能被非 Windows 构建覆盖），用 Windows 编译器重生成..." -ForegroundColor Yellow
        & $shaderExe --input-dir (Join-Path $SourceDir "engine/render/shaders/src") `
                     --output-dir (Join-Path $SourceDir "engine/render/shaders/generated") `
                     --target all --embed
        if ($LASTEXITCODE -ne 0) { Die "重新生成着色器(含 DXBC)失败。" }
    } else {
        Write-Host "   [WARN] 未找到 bin/dse_shader_compiler.exe；若 DX11 编译失败，请先在桌面平台构建一次。" -ForegroundColor Yellow
    }
}
Write-OK "DXBC 嵌入头检查完成"

# ── 3. 构建 ─────────────────────────────────────────────────────────────────
Write-Step "构建 dse_engine"
& $CMake --build $BuildDir --target dse_engine --config $Config -- /m
if ($LASTEXITCODE -ne 0) { Die "构建 dse_engine 失败。" }

if ($WithTests) {
    Write-Step "构建 gtest 目标 (unit/integration/smoke)"
    foreach ($t in @("dse_gtest_unit_tests","dse_gtest_integration_tests","dse_gtest_smoke_tests")) {
        & $CMake --build $BuildDir --target $t --config $Config -- /m
        if ($LASTEXITCODE -ne 0) { Die "构建 $t 失败。" }
    }
}

# ── 4. 校验产物 ─────────────────────────────────────────────────────────────
Write-Step "校验产物"
$postfix = if ($Config -eq "Debug") { "_debug" } else { "" }
$libName = "DSEngine$postfix.lib"
$lib = Get-ChildItem -Path $BuildDir -Recurse -Filter $libName -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $lib) { Die "未找到引擎静态库 $libName。" }
$libMb = [math]::Round($lib.Length / 1MB, 1)
Write-OK ("引擎静态库: {0} ({1} MB)" -f $lib.FullName, $libMb)

Write-Host "`n==================== RESULT ====================" -ForegroundColor Cyan
Write-OK "Windows 构建验证全部通过 ($Config)"
Write-Host ("   引擎库: {0}" -f $lib.FullName) -ForegroundColor Green
exit 0
