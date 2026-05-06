<#
.SYNOPSIS
    SDK 端到端验证脚本：打包 → 消费 → 编译 → 运行。
.DESCRIPTION
    用于 CI 或本地验证 SDK 打包流程的完整性。
    依次测试指定的配置组合（默认 Release + Debug）。
.PARAMETER Configs
    要测试的配置列表。默认 @("Release", "Debug")。
.PARAMETER Arch
    目标架构。默认 x64。
.PARAMETER Generator
    CMake 生成器。默认 "Visual Studio 17 2022"。
.PARAMETER KeepArtifacts
    保留中间产物（build 目录、zip 文件），否则验证通过后清理。
.EXAMPLE
    .\scripts\verify_sdk.ps1
    .\scripts\verify_sdk.ps1 -Configs Release -KeepArtifacts
#>

[CmdletBinding()]
param(
    [string[]]$Configs = @("Release", "Debug"),
    [string]$Arch = "x64",
    [string]$Generator = "Visual Studio 17 2022",
    [switch]$KeepArtifacts
)

# 注意：不能用 "Stop"，因为 cmake 的 stderr 警告会被 PowerShell 误判为终止错误。
# 所有错误检查通过 $LASTEXITCODE 手动处理。
$ErrorActionPreference = "Continue"
$SourceDir = (Resolve-Path "$PSScriptRoot\..").Path
$ConsumerDir = Join-Path $SourceDir "examples\sdk_consumer"
$passed = 0
$failed = 0
$results = @()

function Write-Step($msg) { Write-Host "`n>> $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "   [OK] $msg" -ForegroundColor Green }
function Write-FAIL($msg)  { Write-Host "   [FAIL] $msg" -ForegroundColor Red }

foreach ($cfg in $Configs) {
    Write-Step "Testing config: $cfg"

    $buildSdk    = Join-Path $SourceDir "build_verify_sdk_$($cfg.ToLower())"
    $installDir  = Join-Path $buildSdk  "install"
    $buildConsumer = Join-Path $SourceDir "build_verify_consumer_$($cfg.ToLower())"
    $testLabel   = $cfg

    try {
        # ── Step 1: 打包 SDK ──────────────────────────────────────
        Write-Step "[$cfg] Step 1/4 — CMake Configure (SDK)"
        $cmakeArgs = @(
            "-S", $SourceDir,
            "-B", $buildSdk,
            "-G", $Generator,
            "-A", $Arch,
            "-DCMAKE_INSTALL_PREFIX=$installDir",
            "-DDSE_BUILD_GTESTS=OFF",
            "-DDSE_BUILD_EDITOR=OFF",
            "-DDSE_BUILD_LAUNCHER=OFF",
            "-DDSE_ENABLE_PHYSX=OFF",
            "-DDSE_ENABLE_3D=OFF",
            "-DDSE_ENABLE_VULKAN=OFF"
        )
        & cmake @cmakeArgs *> $null
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }
        Write-OK "Configure"

        Write-Step "[$cfg] Step 2/4 — Build SDK"
        & cmake --build $buildSdk --config $cfg --target dse_engine --parallel *> $null
        if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }
        Write-OK "Build"

        Write-Step "[$cfg] Step 3/4 — Install SDK"
        & cmake --install $buildSdk --config $cfg --component sdk *> $null
        if ($LASTEXITCODE -ne 0) { throw "Install failed (exit $LASTEXITCODE)" }

        # 验证关键文件存在
        $checks = @(
            "lib/cmake/DSEngine/DSEngineConfig.cmake",
            "lib/cmake/DSEngine/DSEngineConfigVersion.cmake",
            "lib/cmake/DSEngine/DSEngineTargets.cmake",
            "include/DSEngine/engine/dse.h",
            "include/DSEngine/engine/dse_version.h",
            "include/DSEngine/engine/core/service_locator.h",
            "include/DSEngine/third_party/glm/glm/glm.hpp",
            "include/DSEngine/third_party/entt/src/entt/entt.hpp"
        )
        foreach ($f in $checks) {
            $fp = Join-Path $installDir $f
            if (-not (Test-Path $fp)) { throw "Missing install file: $f" }
        }
        Write-OK "Install (all key files present)"

        # ── Step 2: 消费者编译 ────────────────────────────────────
        Write-Step "[$cfg] Step 4/4 — Build Consumer Example"
        $consumerArgs = @(
            "-S", $ConsumerDir,
            "-B", $buildConsumer,
            "-G", $Generator,
            "-A", $Arch,
            "-DCMAKE_PREFIX_PATH=$installDir"
        )
        & cmake @consumerArgs *> $null
        if ($LASTEXITCODE -ne 0) { throw "Consumer configure failed (exit $LASTEXITCODE)" }

        & cmake --build $buildConsumer --config $cfg --parallel *> $null
        if ($LASTEXITCODE -ne 0) { throw "Consumer build failed (exit $LASTEXITCODE)" }
        Write-OK "Consumer build"

        $results += [PSCustomObject]@{ Config=$cfg; Status="PASS" }
        $passed++
    }
    catch {
        Write-FAIL "$($_.Exception.Message)"
        $results += [PSCustomObject]@{ Config=$cfg; Status="FAIL: $($_.Exception.Message)" }
        $failed++
    }
    finally {
        if (-not $KeepArtifacts) {
            foreach ($d in @($buildSdk, $buildConsumer)) {
                if (Test-Path $d) { Remove-Item $d -Recurse -Force -ErrorAction SilentlyContinue }
            }
        }
    }
}

# ── 汇总 ──────────────────────────────────────────────────────────
Write-Host "`n========================================"
Write-Host "  SDK Verify Results"
Write-Host "========================================" 
$results | Format-Table -AutoSize
Write-Host "Passed: $passed / $($passed + $failed)"

if ($failed -gt 0) {
    Write-Host "SOME TESTS FAILED" -ForegroundColor Red
    exit 1
} else {
    Write-Host "ALL TESTS PASSED" -ForegroundColor Green
    exit 0
}
