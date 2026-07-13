<#
.SYNOPSIS
    全新 Windows 机器一键准备 DSEngine 开发环境：安装工具链(CMake/Ninja/VS2022 C++
    Build Tools) → 初始化 git submodule → (可选)预构建网络/HTTP 依赖 → (可选)进 VS
    dev shell 跑 configure/build/test。
.DESCRIPTION
    幂等，可在新虚拟机上重复执行：已安装的组件会被跳过。配合 CMakePresets 使用，
    与 scripts/win/build_fast_*.bat 互补（本脚本负责装环境，那些脚本负责日常构建）。
    需要管理员权限以安装 Chocolatey 包。

    注意：DSE_ENABLE_NET/DSE_ENABLE_HTTP 默认 ON，preset 未关闭；Windows 桌面构建需先
    预构建 libsodium+protobuf(网络层 GNS) 与 OpenSSL(HTTP 层)，否则 cmake 配置阶段
    find_package(sodium/OpenSSL) 直接失败。故 -Build/-Test 时本脚本会在 configure 前
    自动预构建这些依赖并把目录传给 preset（幂等，可用 -SkipNet/-SkipHttp 关闭）。
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
.PARAMETER SkipNet
    跳过网络层依赖(libsodium+protobuf)预构建（仅当 preset 关闭 DSE_ENABLE_NET 时使用）。
.PARAMETER SkipHttp
    跳过 HTTP 层依赖(OpenSSL)预构建（仅当 preset 关闭 DSE_ENABLE_HTTP 时使用）。
.PARAMETER NetDepsDir
    网络层预构建依赖(libsodium/protobuf)落地目录，默认 %USERPROFILE%\dse_net_deps。
.PARAMETER OpenSSLDir
    OpenSSL 预构建安装根目录，默认 C:\ossl-win64\install。
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
    [switch]$Test,
    [switch]$SkipNet,
    [switch]$SkipHttp,
    [string]$NetDepsDir = "",
    [string]$OpenSSLDir = "C:\ossl-win64\install"
)

# cmake/ctest 的 stderr 警告会被 PowerShell 当作终止错误，这里统一手动检查 $LASTEXITCODE
$ErrorActionPreference = "Continue"
$SourceDir = (Resolve-Path "$PSScriptRoot\..").Path

function Write-Step($msg) { Write-Host "`n>> $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "   [OK] $msg" -ForegroundColor Green }
function Die($msg) { Write-Host "   [FAIL] $msg" -ForegroundColor Red; exit 1 }

# ── 网络/HTTP 依赖预构建 helpers ──────────────────────────────────────────────
# 与 scripts/verify_windows_build.ps1 的 -WithNet/-WithHttp 布局一致，便于两者复用同一份预构建产物。
function Find-MSBuild {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        # -products * 才能纳入 Build Tools 实例（默认只返回 VS IDE 产品）。
        $mb = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if ($mb) { return $mb }
    }
    return (Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\MSBuild.exe" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName)
}

# 预构建 libsodium(静态 x64 Release+Debug) 与 protobuf(静态 /MD)，整理成 Findsodium/find_package 期望布局。返回依赖目录。
function Build-NetDeps($NetDepsDir) {
    $SodiumDir   = Join-Path $NetDepsDir "sodium"
    $ProtobufDir = Join-Path $NetDepsDir "protobuf"

    if (-not (Test-Path (Join-Path $SodiumDir "x64/Release/v144/static/libsodium.lib"))) {
        $MSBuild = Find-MSBuild
        if (-not $MSBuild) { Die "找不到 MSBuild.exe（预构建 libsodium 需要 VS2022 Build Tools）。" }
        $sln = Join-Path $SourceDir "depends/libsodium/builds/msvc/vs2022/libsodium.sln"
        if (-not (Test-Path $sln)) { Die "缺 libsodium 子模块：git submodule update --init depends/libsodium" }
        Write-Step "预构建 libsodium (StaticRelease/StaticDebug|x64)"
        & $MSBuild $sln /p:Configuration=StaticRelease /p:Platform=x64 /m /v:m
        if ($LASTEXITCODE -ne 0) { Die "libsodium StaticRelease 构建失败。" }
        & $MSBuild $sln /p:Configuration=StaticDebug /p:Platform=x64 /m /v:m
        if ($LASTEXITCODE -ne 0) { Die "libsodium StaticDebug 构建失败。" }
        $SodiumLibRel = Join-Path $SourceDir "depends/libsodium/bin/x64/Release/v143/static/libsodium.lib"
        $SodiumLibDbg = Join-Path $SourceDir "depends/libsodium/bin/x64/Debug/v143/static/libsodium.lib"
        New-Item -ItemType Directory -Force -Path (Join-Path $SodiumDir "include") | Out-Null
        Copy-Item (Join-Path $SourceDir "depends/libsodium/src/libsodium/include/sodium.h") (Join-Path $SodiumDir "include") -Force
        Copy-Item (Join-Path $SourceDir "depends/libsodium/src/libsodium/include/sodium") (Join-Path $SodiumDir "include") -Recurse -Force
        Copy-Item (Join-Path $SourceDir "depends/libsodium/builds/msvc/version.h") (Join-Path $SodiumDir "include/sodium/version.h") -Force
        # 本机 MSVC 工具集后缀(find_package 可能算成 v144)与 .sln 实际输出(v143)都放一份。
        foreach ($tv in @("v143","v144")) {
            New-Item -ItemType Directory -Force -Path (Join-Path $SodiumDir "x64/Release/$tv/static") | Out-Null
            New-Item -ItemType Directory -Force -Path (Join-Path $SodiumDir "x64/Debug/$tv/static") | Out-Null
            Copy-Item $SodiumLibRel (Join-Path $SodiumDir "x64/Release/$tv/static/libsodium.lib") -Force
            Copy-Item $SodiumLibDbg (Join-Path $SodiumDir "x64/Debug/$tv/static/libsodium.lib") -Force
        }
        Write-OK "libsodium 预构建完成：$SodiumDir"
    } else { Write-OK "复用已预构建 libsodium：$SodiumDir" }

    if (-not (Test-Path (Join-Path $ProtobufDir "lib/libprotobuf.lib"))) {
        if (-not (Test-Path (Join-Path $SourceDir "depends/protobuf/CMakeLists.txt"))) { Die "缺 protobuf 子模块：git submodule update --init depends/protobuf" }
        $pbBuild = Join-Path $SourceDir "build_protobuf"
        # CRT 宏须与引擎一致以免 LNK2038。
        $crt = "/D_CRT_STDIO_ISO_WIDE_SPECIFIERS=1 /D_CRT_NONSTDC_NO_WARNINGS=1 /D_CRT_DECLARE_NONSTDC_NAMES=1"
        Write-Step "预构建 protobuf (静态 /MD)"
        cmake -S (Join-Path $SourceDir "depends/protobuf") -B $pbBuild -G "Visual Studio 17 2022" -A x64 `
            "-Dprotobuf_BUILD_TESTS=OFF" "-Dprotobuf_MSVC_STATIC_RUNTIME=OFF" `
            "-Dprotobuf_WITH_ZLIB=OFF" "-Dprotobuf_BUILD_SHARED_LIBS=OFF" `
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" "-DCMAKE_INSTALL_PREFIX=$ProtobufDir" `
            "-DCMAKE_CXX_FLAGS=$crt" "-DCMAKE_C_FLAGS=$crt"
        if ($LASTEXITCODE -ne 0) { Die "protobuf 配置失败。" }
        foreach ($c in @("Release","Debug")) {
            cmake --build $pbBuild --config $c --target install -- /m
            if ($LASTEXITCODE -ne 0) { Die "protobuf $c 安装失败。" }
        }
        Write-OK "protobuf 预构建完成：$ProtobufDir"
    } else { Write-OK "复用已预构建 protobuf：$ProtobufDir" }

    return @{ Sodium = $SodiumDir; Protobuf = $ProtobufDir }
}

# 预构建 Windows x64 OpenSSL（build_windows_openssl.ps1 本身幂等）。
function Build-OpenSSL($OpenSSLDir) {
    if (Test-Path (Join-Path $OpenSSLDir "lib/libssl.lib")) {
        Write-OK "复用已预构建 OpenSSL：$OpenSSLDir"; return
    }
    Write-Step "预构建 Windows x64 OpenSSL → $OpenSSLDir"
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $SourceDir "scripts/build_windows_openssl.ps1") -Prefix $OpenSSLDir
    if ($LASTEXITCODE -ne 0) { Die "OpenSSL 预构建失败。" }
    Write-OK "OpenSSL 预构建完成：$OpenSSLDir"
}

if ($Test) { $Build = $true }

$VsRoot = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"

# ── 1. 工具链 ────────────────────────────────────────────────────────────────
if (-not $SkipInstall) {
    Write-Step "安装工具链 (Chocolatey: cmake / ninja / VS2022 C++ Build Tools)"
    if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
        Die "未找到 Chocolatey。请先安装：https://chocolatey.org/install"
    }
    choco install -y cmake ninja dotnet-8.0-sdk
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

# ── 3b. 网络/HTTP 依赖预构建 ─────────────────────────────────────────────────
# DSE_ENABLE_NET/HTTP 默认 ON 且 preset 未关闭，Windows 需预构建依赖并把目录传给 configure。
$presetArgs = @()
if (-not $SkipNet) {
    if (-not $NetDepsDir) { $NetDepsDir = Join-Path $env:USERPROFILE "dse_net_deps" }
    $net = Build-NetDeps $NetDepsDir
    $presetArgs += "-DDSE_NET_SODIUM_DIR=$(($net.Sodium)   -replace '\\','/')"
    $presetArgs += "-DDSE_NET_PROTOBUF_DIR=$(($net.Protobuf) -replace '\\','/')"
} else { Write-OK "跳过网络层依赖预构建 (-SkipNet)" }
if (-not $SkipHttp) {
    Build-OpenSSL $OpenSSLDir
    $presetArgs += "-DDSE_HTTP_OPENSSL_DIR=$($OpenSSLDir -replace '\\','/')"
} else { Write-OK "跳过 HTTP 层依赖预构建 (-SkipHttp)" }

# ── 4. 配置 + 构建 ───────────────────────────────────────────────────────────
Write-Step "配置 (cmake --preset $Preset $presetArgs)"
cmake --preset $Preset @presetArgs
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
