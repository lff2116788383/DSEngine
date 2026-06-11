<#
.SYNOPSIS
    SDK end-to-end verification: package -> install -> consume -> compile -> run.
.DESCRIPTION
    Verifies the integrity of the SDK packaging flow for CI or local use.
    Tests a matrix of build profiles x configurations.

    Profiles (feature sets the SDK is built with):
      - Minimal : 2D-only path (3D / PhysX / Vulkan OFF). Fast smoke.
      - Full    : default 3D + Jolt physics path (3D ON, Jolt ON). The shipping
                  configuration. Vulkan stays OFF (optional capability, see
                  PROGRESS_REPORT P2).

    Every profile is built as a shared library (DSE_BUILD_SHARED=ON), which is
    required: the install/export rules (TARGETS + EXPORT DSEngineTargets +
    public headers) live under if(DSE_BUILD_SHARED); a static build installs
    only the package-config files and yields an unusable SDK.
.PARAMETER Configs
    Build configurations to test. Default @("Release", "Debug").
.PARAMETER Profiles
    Feature profiles to test. Default @("Minimal", "Full").
.PARAMETER Arch
    Target architecture. Default x64.
.PARAMETER Generator
    CMake generator. Default "Visual Studio 17 2022".
.PARAMETER KeepArtifacts
    Keep intermediate artifacts (build dirs) instead of cleaning on success.
.EXAMPLE
    .\scripts\verify_sdk.ps1
    .\scripts\verify_sdk.ps1 -Configs Release -Profiles Full -KeepArtifacts
#>

[CmdletBinding()]
param(
    [string[]]$Configs = @("Release", "Debug"),
    [string[]]$Profiles = @("Minimal", "Full"),
    [string]$Arch = "x64",
    [string]$Generator = "Visual Studio 17 2022",
    [switch]$KeepArtifacts
)

# Do not use "Stop": cmake's stderr warnings would be treated as terminating
# errors by PowerShell. All error checks are handled manually via $LASTEXITCODE.
$ErrorActionPreference = "Continue"

# Normalize comma-joined values. When invoked via `powershell -File`, array
# arguments like `-Configs Release,Debug` arrive as a single literal string
# "Release,Debug" (PS array syntax is only parsed in -Command mode). Split them
# so the same invocation works in both -File and -Command modes.
$Configs  = @($Configs  | ForEach-Object { $_ -split ',' } | Where-Object { $_ -ne '' })
$Profiles = @($Profiles | ForEach-Object { $_ -split ',' } | Where-Object { $_ -ne '' })

$SourceDir = (Resolve-Path "$PSScriptRoot\..").Path
$ConsumerDir = Join-Path $SourceDir "examples\sdk_consumer"
$passed = 0
$failed = 0
$results = @()

function Write-Step($msg) { Write-Host "`n>> $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "   [OK] $msg" -ForegroundColor Green }
function Write-FAIL($msg)  { Write-Host "   [FAIL] $msg" -ForegroundColor Red }

# Feature flags per profile. DSE_BUILD_SHARED=ON and editor/launcher/gtests OFF
# are common to all profiles and added below.
function Get-ProfileArgs($profile) {
    switch ($profile) {
        "Minimal" { return @("-DDSE_ENABLE_3D=OFF", "-DDSE_ENABLE_PHYSX=OFF", "-DDSE_ENABLE_VULKAN=OFF") }
        "Full"    { return @("-DDSE_ENABLE_3D=ON",  "-DDSE_ENABLE_JOLT=ON",   "-DDSE_ENABLE_VULKAN=OFF") }
        default   { throw "Unknown profile: $profile (expected Minimal or Full)" }
    }
}

foreach ($profile in $Profiles) {
foreach ($cfg in $Configs) {
    $testLabel = "$profile/$cfg"
    Write-Step "Testing profile/config: $testLabel"

    $tag           = "$($profile.ToLower())_$($cfg.ToLower())"
    $buildSdk      = Join-Path $SourceDir "build_verify_sdk_$tag"
    $installDir    = Join-Path $buildSdk  "install"
    $buildConsumer = Join-Path $SourceDir "build_verify_consumer_$tag"

    try {
        # -- Step 1/4: Configure SDK ------------------------------------------
        Write-Step "[$testLabel] Step 1/4 - CMake Configure (SDK)"
        $cmakeArgs = @(
            "-S", $SourceDir,
            "-B", $buildSdk,
            "-G", $Generator,
            "-A", $Arch,
            "-DCMAKE_INSTALL_PREFIX=$installDir",
            "-DDSE_BUILD_SHARED=ON",
            "-DDSE_BUILD_GTESTS=OFF",
            "-DDSE_BUILD_EDITOR=OFF",
            "-DDSE_BUILD_LAUNCHER=OFF"
        ) + (Get-ProfileArgs $profile)
        & cmake @cmakeArgs *> $null
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }
        Write-OK "Configure"

        # -- Step 2/4: Build SDK ----------------------------------------------
        Write-Step "[$testLabel] Step 2/4 - Build SDK"
        & cmake --build $buildSdk --config $cfg --target dse_engine --parallel *> $null
        if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }
        Write-OK "Build"

        # -- Step 3/4: Install SDK --------------------------------------------
        Write-Step "[$testLabel] Step 3/4 - Install SDK"
        & cmake --install $buildSdk --config $cfg --component sdk *> $null
        if ($LASTEXITCODE -ne 0) { throw "Install failed (exit $LASTEXITCODE)" }

        # Verify key files exist
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

        # -- Step 4/4: Build consumer example ---------------------------------
        Write-Step "[$testLabel] Step 4/4 - Build Consumer Example"
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

        # Run the consumer to confirm the DLL loads and the API executes.
        $exe = Join-Path $buildConsumer "$cfg\consumer_example.exe"
        if (-not (Test-Path $exe)) { throw "Consumer exe not found: $exe" }
        & $exe *> $null
        if ($LASTEXITCODE -ne 0) { throw "Consumer run failed (exit $LASTEXITCODE)" }
        Write-OK "Consumer run"

        $results += [PSCustomObject]@{ Profile=$profile; Config=$cfg; Status="PASS" }
        $passed++
    }
    catch {
        Write-FAIL "$($_.Exception.Message)"
        $results += [PSCustomObject]@{ Profile=$profile; Config=$cfg; Status="FAIL: $($_.Exception.Message)" }
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
}

# -- Summary ------------------------------------------------------------------
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
