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
.PARAMETER WithNet
    额外验证网络层(GNS)：预构建 libsodium + protobuf（缺失时自动构建到 NetDepsDir），
    以 DSE_ENABLE_NET=ON 配置独立构建目录(build_vs2022_net)，构建并运行 dse_net_smoke.exe。
.PARAMETER NetOnly
    仅执行 -WithNet 的网络层验证，跳过引擎(dse_engine)构建与校验。
.PARAMETER NetDepsDir
    网络层预构建依赖(libsodium/protobuf)的落地目录，默认 %USERPROFILE%\dse_net_deps。
.PARAMETER WithHttp
    额外验证异步 HTTP 客户端：预构建 Windows x64 OpenSSL（缺失时自动构建到 OpenSSLDir），
    以 DSE_ENABLE_HTTP=ON 配置独立构建目录(build_vs2022_http)，构建并运行
    dse_http_smoke.exe 与 dse_http_lua_smoke.exe。
.PARAMETER HttpOnly
    仅执行 -WithHttp 的 HTTP 层验证，跳过引擎(dse_engine)构建与校验。
.PARAMETER OpenSSLDir
    OpenSSL 预构建安装根目录，默认 C:\ossl-win64\install。
.EXAMPLE
    .\scripts\verify_windows_build.ps1
    .\scripts\verify_windows_build.ps1 -Config Release -WithTests
    .\scripts\verify_windows_build.ps1 -WithNet
    .\scripts\verify_windows_build.ps1 -WithNet -NetOnly
    .\scripts\verify_windows_build.ps1 -WithHttp
    .\scripts\verify_windows_build.ps1 -WithHttp -HttpOnly
#>

[CmdletBinding()]
param(
    [ValidateSet("Debug","Release")][string]$Config = "Debug",
    [string]$Arch = "x64",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$BuildDir = "",
    [switch]$WithTests,
    [switch]$WithNet,
    [switch]$NetOnly,
    [string]$NetDepsDir = "",
    [switch]$WithHttp,
    [switch]$HttpOnly,
    [string]$OpenSSLDir = "C:\ossl-win64\install"
)
if ($NetOnly)  { $WithNet  = $true }
if ($HttpOnly) { $WithHttp = $true }
# 引擎(dse_engine)构建仅在非 *Only 模式下执行
$RunEngine = -not ($NetOnly -or $HttpOnly)

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

if ($RunEngine) {
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
} # end if ($RunEngine)

# ── 5. 网络层 (GNS) 验证 ────────────────────────────────────────────────────
if ($WithNet) {
    if (-not $NetDepsDir) { $NetDepsDir = Join-Path $env:USERPROFILE "dse_net_deps" }
    $SodiumDir   = Join-Path $NetDepsDir "sodium"
    $ProtobufDir = Join-Path $NetDepsDir "protobuf"
    $NetBuildDir = Join-Path $SourceDir "build_vs2022_net"

    Write-Step "网络层依赖目录：$NetDepsDir"

    # 5.1 定位 MSBuild（构建 libsodium 自带 .sln 用）
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { Die "找不到 vswhere，无法定位 MSBuild。请安装 VS2022 Build Tools。" }
    # -products * 才能纳入 Build Tools 实例（默认只返回 VS IDE 产品）。
    $MSBuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
    if (-not $MSBuild) {
        $MSBuild = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\MSBuild.exe" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $MSBuild) { Die "找不到 MSBuild.exe。" }
    Write-OK "MSBuild=$MSBuild"

    # 5.2 预构建 libsodium（静态 x64 Release+Debug），整理成 Findsodium 期望布局
    $SodiumLibRel = Join-Path $SourceDir "depends/libsodium/bin/x64/Release/v143/static/libsodium.lib"
    $SodiumLibDbg = Join-Path $SourceDir "depends/libsodium/bin/x64/Debug/v143/static/libsodium.lib"
    if (-not (Test-Path (Join-Path $SodiumDir "x64/Release/v144/static/libsodium.lib"))) {
        $sln = Join-Path $SourceDir "depends/libsodium/builds/msvc/vs2022/libsodium.sln"
        if (-not (Test-Path $sln)) { Die "缺 libsodium 子模块：git submodule update --init depends/libsodium" }
        Write-Step "构建 libsodium (StaticRelease|x64)"
        & $MSBuild $sln /p:Configuration=StaticRelease /p:Platform=x64 /m /v:m
        if ($LASTEXITCODE -ne 0) { Die "libsodium StaticRelease 构建失败。" }
        Write-Step "构建 libsodium (StaticDebug|x64)"
        & $MSBuild $sln /p:Configuration=StaticDebug /p:Platform=x64 /m /v:m
        if ($LASTEXITCODE -ne 0) { Die "libsodium StaticDebug 构建失败。" }

        New-Item -ItemType Directory -Force -Path (Join-Path $SodiumDir "include") | Out-Null
        Copy-Item (Join-Path $SourceDir "depends/libsodium/src/libsodium/include/sodium.h") (Join-Path $SodiumDir "include") -Force
        Copy-Item (Join-Path $SourceDir "depends/libsodium/src/libsodium/include/sodium") (Join-Path $SodiumDir "include") -Recurse -Force
        Copy-Item (Join-Path $SourceDir "depends/libsodium/builds/msvc/version.h") (Join-Path $SodiumDir "include/sodium/version.h") -Force
        # 本机 MSVC 工具集后缀(Findsodium 可能算成 v144)与 .sln 实际输出(v143)都放一份。
        foreach ($tv in @("v143","v144")) {
            New-Item -ItemType Directory -Force -Path (Join-Path $SodiumDir "x64/Release/$tv/static") | Out-Null
            New-Item -ItemType Directory -Force -Path (Join-Path $SodiumDir "x64/Debug/$tv/static") | Out-Null
            Copy-Item $SodiumLibRel (Join-Path $SodiumDir "x64/Release/$tv/static/libsodium.lib") -Force
            Copy-Item $SodiumLibDbg (Join-Path $SodiumDir "x64/Debug/$tv/static/libsodium.lib") -Force
        }
        Write-OK "libsodium 预构建完成：$SodiumDir"
    } else { Write-OK "复用已预构建的 libsodium：$SodiumDir" }

    # 5.3 预构建并安装 protobuf v3.21.12（静态 /MD；CRT 宏须与引擎一致以免 LNK2038）
    if (-not (Test-Path (Join-Path $ProtobufDir "lib/libprotobuf.lib"))) {
        if (-not (Test-Path (Join-Path $SourceDir "depends/protobuf/CMakeLists.txt"))) { Die "缺 protobuf 子模块：git submodule update --init depends/protobuf" }
        $pbBuild = Join-Path $SourceDir "build_protobuf"
        $crt = "/D_CRT_STDIO_ISO_WIDE_SPECIFIERS=1 /D_CRT_NONSTDC_NO_WARNINGS=1 /D_CRT_DECLARE_NONSTDC_NAMES=1"
        Write-Step "配置 protobuf"
        & $CMake -S (Join-Path $SourceDir "depends/protobuf") -B $pbBuild -G $Generator -A $Arch `
            "-Dprotobuf_BUILD_TESTS=OFF" "-Dprotobuf_MSVC_STATIC_RUNTIME=OFF" `
            "-Dprotobuf_WITH_ZLIB=OFF" "-Dprotobuf_BUILD_SHARED_LIBS=OFF" `
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" "-DCMAKE_INSTALL_PREFIX=$ProtobufDir" `
            "-DCMAKE_CXX_FLAGS=$crt" "-DCMAKE_C_FLAGS=$crt"
        if ($LASTEXITCODE -ne 0) { Die "protobuf 配置失败。" }
        foreach ($c in @("Release","Debug")) {
            Write-Step "构建并安装 protobuf ($c)"
            & $CMake --build $pbBuild --config $c --target install -- /m
            if ($LASTEXITCODE -ne 0) { Die "protobuf $c 安装失败。" }
        }
        Write-OK "protobuf 预构建完成：$ProtobufDir"
    } else { Write-OK "复用已预构建的 protobuf：$ProtobufDir" }

    # 5.4 配置 + 构建 dse_net_smoke（DSE_ENABLE_NET=ON）
    if (-not (Test-Path (Join-Path $NetBuildDir "CMakeCache.txt"))) {
        Write-Step "配置 CMake (NET=ON, $Generator $Arch)"
        & $CMake -S $SourceDir -B $NetBuildDir -G $Generator -A $Arch `
            "-DDSE_BUILD_EDITOR=OFF" "-DDSE_BUILD_LAUNCHER=OFF" "-DDSE_ENABLE_3D=OFF" `
            "-DDSE_ENABLE_NET=ON" "-DDSE_NET_SODIUM_DIR=$SodiumDir" "-DDSE_NET_PROTOBUF_DIR=$ProtobufDir" `
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
        if ($LASTEXITCODE -ne 0) { Die "网络层 CMake 配置失败。" }
    } else { Write-OK "复用已有网络构建目录：$NetBuildDir" }

    Write-Step "构建 dse_net_smoke ($Config)"
    & $CMake --build $NetBuildDir --config $Config --target dse_net_smoke -- /m
    if ($LASTEXITCODE -ne 0) { Die "构建 dse_net_smoke 失败。" }

    Write-Step "构建 dse_net_capi_smoke ($Config)"
    & $CMake --build $NetBuildDir --config $Config --target dse_net_capi_smoke -- /m
    if ($LASTEXITCODE -ne 0) { Die "构建 dse_net_capi_smoke 失败。" }

    Write-Step "构建 dse_net_lua_smoke ($Config) — Lua 绑定 dse.net"
    & $CMake --build $NetBuildDir --config $Config --target dse_net_lua_smoke -- /m
    if ($LASTEXITCODE -ne 0) { Die "构建 dse_net_lua_smoke 失败。" }

    # 5.5 运行回环 smoke（reliable + unreliable + lane1），退出码 0 = 通过
    $smokeExe = Join-Path $SourceDir "bin/dse_net_smoke.exe"
    if (-not (Test-Path $smokeExe)) { Die "未找到 $smokeExe。" }
    Write-Step "运行网络回环 smoke (INetTransport)"
    & $smokeExe
    if ($LASTEXITCODE -ne 0) { Die "网络 smoke 失败 (exit=$LASTEXITCODE)。" }
    Write-OK "网络 smoke: 通过"

    # 5.6 运行 C ABI 回环 smoke（仅用 dse_net_* 接口），退出码 0 = 通过
    $capiExe = Join-Path $SourceDir "bin/dse_net_capi_smoke.exe"
    if (-not (Test-Path $capiExe)) { Die "未找到 $capiExe。" }
    Write-Step "运行网络回环 smoke (C ABI)"
    & $capiExe
    if ($LASTEXITCODE -ne 0) { Die "C ABI smoke 失败 (exit=$LASTEXITCODE)。" }
    Write-OK "C ABI smoke: 通过"

    # 5.7 运行 Lua 绑定回环 smoke（dse.net.* 全程，含 lane/事件回调），退出码 0 = 通过
    $luaExe = Join-Path $SourceDir "bin/dse_net_lua_smoke.exe"
    if (-not (Test-Path $luaExe)) { Die "未找到 $luaExe。" }
    Write-Step "运行网络回环 smoke (Lua 绑定 dse.net)"
    & $luaExe
    if ($LASTEXITCODE -ne 0) { Die "Lua 绑定 smoke 失败 (exit=$LASTEXITCODE)。" }
    Write-OK "Lua 绑定 smoke: 通过"
}

# ── 6. 异步 HTTP 客户端验证 ──────────────────────────────────────────────────
if ($WithHttp) {
    $HttpBuildDir = Join-Path $SourceDir "build_vs2022_http"

    # 6.1 预构建 OpenSSL（幂等：已构则秒过）
    Write-Step "预构建 Windows x64 OpenSSL → $OpenSSLDir"
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $SourceDir "scripts/build_windows_openssl.ps1") -Prefix $OpenSSLDir
    if ($LASTEXITCODE -ne 0) { Die "OpenSSL 预构建失败。" }

    # 6.2 配置 + 构建（DSE_ENABLE_HTTP=ON）
    if (-not (Test-Path (Join-Path $HttpBuildDir "CMakeCache.txt"))) {
        Write-Step "配置 CMake (HTTP=ON, $Generator $Arch)"
        & $CMake -S $SourceDir -B $HttpBuildDir -G $Generator -A $Arch `
            "-DDSE_BUILD_EDITOR=OFF" "-DDSE_BUILD_LAUNCHER=OFF" "-DDSE_ENABLE_3D=OFF" `
            "-DDSE_ENABLE_HTTP=ON" "-DDSE_HTTP_OPENSSL_DIR=$OpenSSLDir" `
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
        if ($LASTEXITCODE -ne 0) { Die "HTTP 层 CMake 配置失败。" }
    } else { Write-OK "复用已有 HTTP 构建目录：$HttpBuildDir" }

    foreach ($t in @("dse_http_smoke","dse_http_lua_smoke")) {
        Write-Step "构建 $t ($Config)"
        & $CMake --build $HttpBuildDir --config $Config --target $t -- /m
        if ($LASTEXITCODE -ne 0) { Die "构建 $t 失败。" }
    }

    # 6.3 运行 smoke（退出码 0 = 通过）
    foreach ($e in @("dse_http_smoke.exe","dse_http_lua_smoke.exe")) {
        $exe = Join-Path $SourceDir "bin/$e"
        if (-not (Test-Path $exe)) { Die "未找到 $exe。" }
        Write-Step "运行 $e"
        & $exe
        if ($LASTEXITCODE -ne 0) { Die "$e 失败 (exit=$LASTEXITCODE)。" }
        Write-OK "${e}: 通过"
    }
}

Write-Host "`n==================== RESULT ====================" -ForegroundColor Cyan
if ($RunEngine) { Write-OK "Windows 引擎构建验证通过 ($Config)" }
if ($WithNet)   { Write-OK "网络层 (GNS) 验证通过" }
if ($WithHttp)  { Write-OK "异步 HTTP 客户端验证通过" }
exit 0
