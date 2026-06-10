<#
.SYNOPSIS
    DSEngine SDK 打包脚本：配置 → 编译 → install → 打包 ZIP
.DESCRIPTION
    自动化生成 DSEngine SDK 发行包，供下游项目通过 find_package(DSEngine) 集成。
.PARAMETER Config
    编译配置 (Release | Debug | RelWithDebInfo)。默认 Release。
.PARAMETER Arch
    目标架构。默认 x64。
.PARAMETER EnableVulkan
    启用 Vulkan RHI 后端。
.PARAMETER EnablePhysx
    启用 PhysX 集成。
.PARAMETER Enable3D
    启用 3D 运行时路径。
.PARAMETER Generator
    CMake 生成器。默认 "Visual Studio 17 2022"。
.PARAMETER SkipBuild
    跳过编译步骤（仅重新打包已有的 install 输出）。
.EXAMPLE
    .\scripts\package_sdk.ps1 -Config Release
    .\scripts\package_sdk.ps1 -Config Debug -EnableVulkan
#>

[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",

    [string]$Arch = "x64",

    [switch]$EnableVulkan,
    [switch]$EnablePhysx,
    [switch]$Enable3D,

    [string]$Generator = "Visual Studio 17 2022",

    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

# ── 路径 ──────────────────────────────────────────────────────────
$SourceDir   = (Resolve-Path "$PSScriptRoot\..").Path
$BuildDir    = Join-Path $SourceDir "build_sdk_$($Config.ToLower())"
$InstallDir  = Join-Path $BuildDir  "install"

# 读取版本号（从 CMakeLists.txt）
$versionLine = Select-String -Path "$SourceDir\CMakeLists.txt" -Pattern 'project\(DSEngine\s+VERSION\s+(\S+)' | Select-Object -First 1
if ($versionLine) {
    $Version = $versionLine.Matches[0].Groups[1].Value
} else {
    $Version = "0.0.0"
}

$Platform    = "win-$Arch"
$SdkName     = "DSEngine-SDK-v${Version}-${Platform}-$($Config.ToLower())"
$OutputZip   = Join-Path $SourceDir "$SdkName.zip"

Write-Host "========================================"
Write-Host "  DSEngine SDK Packager"
Write-Host "  Version : $Version"
Write-Host "  Config  : $Config"
Write-Host "  Arch    : $Arch"
Write-Host "  Output  : $SdkName.zip"
Write-Host "========================================"

# ── CMake 配置 ────────────────────────────────────────────────────
if (-not $SkipBuild) {
    $cmakeArgs = @(
        "-S", $SourceDir,
        "-B", $BuildDir,
        "-G", $Generator,
        "-A", $Arch,
        "-DCMAKE_INSTALL_PREFIX=$InstallDir",
        # 必须开启共享库：install 的 TARGETS / EXPORT / 公共头规则都在
        # if(DSE_BUILD_SHARED) 之下，静态构建只会装出 package-config 文件，
        # 产出的 SDK 无法被 find_package 使用。
        "-DDSE_BUILD_SHARED=ON",
        "-DDSE_BUILD_GTESTS=OFF",
        "-DDSE_BUILD_EDITOR=OFF",
        "-DDSE_BUILD_LAUNCHER=OFF"
    )

    if ($EnableVulkan) {
        $cmakeArgs += "-DDSE_ENABLE_VULKAN=ON"
    } else {
        $cmakeArgs += "-DDSE_ENABLE_VULKAN=OFF"
    }

    if ($EnablePhysx) {
        $cmakeArgs += "-DDSE_ENABLE_PHYSX=ON"
    } else {
        $cmakeArgs += "-DDSE_ENABLE_PHYSX=OFF"
    }

    if ($Enable3D) {
        $cmakeArgs += "-DDSE_ENABLE_3D=ON"
    } else {
        $cmakeArgs += "-DDSE_ENABLE_3D=OFF"
    }

    Write-Host "`n[1/4] CMake Configure..."
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }

    # ── 编译 ──────────────────────────────────────────────────────
    # 只构建 dse_engine —— SDK 的 install 规则仅安装该目标 + 公共头 + 脚本，
    # 其余 smoke/工具可执行目标不属于 SDK，且在共享库配置下有各自的链接前提。
    Write-Host "`n[2/4] CMake Build ($Config)..."
    & cmake --build $BuildDir --config $Config --target dse_engine --parallel
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed (exit $LASTEXITCODE)" }

    # ── Install ───────────────────────────────────────────────────
    Write-Host "`n[3/4] CMake Install..."
    & cmake --install $BuildDir --config $Config --component sdk
    if ($LASTEXITCODE -ne 0) { throw "CMake install failed (exit $LASTEXITCODE)" }
}

# ── 验证 install 输出 ────────────────────────────────────────────
if (-not (Test-Path $InstallDir)) {
    throw "Install directory not found: $InstallDir. Run without -SkipBuild first."
}

# ── 添加元数据文件 ───────────────────────────────────────────────
$readmeContent = @"
# DSEngine SDK v${Version}

- **Platform**: ${Platform}
- **Config**: ${Config}
- **Built**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

## Usage (CMake)

```cmake
find_package(DSEngine REQUIRED)
target_link_libraries(your_app PRIVATE DSEngine::dse_engine)
```

## Directory Structure

- ``include/DSEngine/``  — Public headers
- ``lib/``               — Import libraries (.lib)
- ``bin/``               — Runtime DLLs
- ``lib/cmake/DSEngine/``— CMake package config
- ``share/DSEngine/``    — Lua scripts and data
"@

Set-Content -Path (Join-Path $InstallDir "README.md") -Value $readmeContent -Encoding UTF8

if (Test-Path "$SourceDir\LICENSE") {
    Copy-Item "$SourceDir\LICENSE" -Destination $InstallDir
}

# ── 打包 ZIP ─────────────────────────────────────────────────────
Write-Host "`n[4/4] Packaging to $SdkName.zip..."
if (Test-Path $OutputZip) { Remove-Item $OutputZip -Force }

# 使用 Compress-Archive，将 install 目录内容放到 zip 根目录
Compress-Archive -Path "$InstallDir\*" -DestinationPath $OutputZip -CompressionLevel Optimal

$zipSize = (Get-Item $OutputZip).Length / 1MB
Write-Host "`nDone! SDK package: $OutputZip ($([math]::Round($zipSize, 2)) MB)"
Write-Host "Extract and set CMAKE_PREFIX_PATH to use with find_package(DSEngine)."
