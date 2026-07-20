<#
.SYNOPSIS
    CI helper: prebuild the Windows desktop network/HTTP dependencies that the
    v1 defaults require, then export their locations to $GITHUB_ENV.
.DESCRIPTION
    DSE_ENABLE_NET / DSE_ENABLE_HTTP default ON. On Windows the network layer
    (GameNetworkingSockets) needs prebuilt static libsodium + protobuf, and the
    HTTP layer needs OpenSSL; none are auto-provisioned on a fresh runner, so
    CMake configure fails at find_package(sodium/OpenSSL). This mirrors the
    proven layout produced by scripts/bootstrap_windows.ps1 (Build-NetDeps /
    Build-OpenSSL) so CI reuses the exact same recipe, but is standalone so a
    workflow can call it and cache its output without pulling in the full
    toolchain-install path.

    Idempotent: already-built artifacts are reused (so an actions/cache restore
    makes this a fast no-op). On success writes to $GITHUB_ENV:
      DSE_NET_SODIUM_DIR, DSE_NET_PROTOBUF_DIR, DSE_HTTP_OPENSSL_DIR
    Must run inside a VS2022 x64 developer environment (msbuild + cl on PATH),
    e.g. after the ilammy/msvc-dev-cmd action.
.PARAMETER NetDepsDir
    Root for the prebuilt libsodium/protobuf (default C:\dse_net_deps).
.PARAMETER OpenSSLDir
    OpenSSL install prefix (default C:\ossl-win64\install).
#>
[CmdletBinding()]
param(
    [string]$NetDepsDir = "C:\dse_net_deps",
    [string]$OpenSSLDir  = "C:\ossl-win64\install",
    # HTTP(OpenSSL) is opt-in: it needs Perl+NASM on the runner and is not part of
    # the validated NET path, so it is only built when the caller enables HTTP.
    [switch]$IncludeOpenSSL
)

$ErrorActionPreference = "Continue"
$SourceDir = (Resolve-Path "$PSScriptRoot\..\..").Path

function Write-Step($msg) { Write-Host "`n>> $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "   [OK] $msg" -ForegroundColor Green }
function Die($msg)        { Write-Host "   [FAIL] $msg" -ForegroundColor Red; exit 1 }

function Find-MSBuild {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $mb = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if ($mb) { return $mb }
    }
    return (Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\MSBuild.exe" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName)
}

$SodiumDir   = Join-Path $NetDepsDir "sodium"
$ProtobufDir = Join-Path $NetDepsDir "protobuf"

# ── libsodium (static x64 Release+Debug), arranged into Findsodium layout ──────
if (-not (Test-Path (Join-Path $SodiumDir "x64/Release/v144/static/libsodium.lib"))) {
    $MSBuild = Find-MSBuild
    if (-not $MSBuild) { Die "MSBuild.exe not found (need VS2022 C++ Build Tools to build libsodium)." }
    $sln = Join-Path $SourceDir "depends/libsodium/builds/msvc/vs2022/libsodium.sln"
    if (-not (Test-Path $sln)) { Die "missing libsodium submodule: git submodule update --init depends/libsodium" }
    Write-Step "Prebuild libsodium (StaticRelease/StaticDebug | x64)"
    & $MSBuild $sln /p:Configuration=StaticRelease /p:Platform=x64 /m /v:m
    if ($LASTEXITCODE -ne 0) { Die "libsodium StaticRelease build failed." }
    & $MSBuild $sln /p:Configuration=StaticDebug /p:Platform=x64 /m /v:m
    if ($LASTEXITCODE -ne 0) { Die "libsodium StaticDebug build failed." }
    $SodiumLibRel = Join-Path $SourceDir "depends/libsodium/bin/x64/Release/v143/static/libsodium.lib"
    $SodiumLibDbg = Join-Path $SourceDir "depends/libsodium/bin/x64/Debug/v143/static/libsodium.lib"
    New-Item -ItemType Directory -Force -Path (Join-Path $SodiumDir "include") | Out-Null
    Copy-Item (Join-Path $SourceDir "depends/libsodium/src/libsodium/include/sodium.h") (Join-Path $SodiumDir "include") -Force
    Copy-Item (Join-Path $SourceDir "depends/libsodium/src/libsodium/include/sodium") (Join-Path $SodiumDir "include") -Recurse -Force
    Copy-Item (Join-Path $SourceDir "depends/libsodium/builds/msvc/version.h") (Join-Path $SodiumDir "include/sodium/version.h") -Force
    # find_package may resolve the toolset suffix to v143 or v144; ship both.
    foreach ($tv in @("v143","v144")) {
        New-Item -ItemType Directory -Force -Path (Join-Path $SodiumDir "x64/Release/$tv/static") | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $SodiumDir "x64/Debug/$tv/static") | Out-Null
        Copy-Item $SodiumLibRel (Join-Path $SodiumDir "x64/Release/$tv/static/libsodium.lib") -Force
        Copy-Item $SodiumLibDbg (Join-Path $SodiumDir "x64/Debug/$tv/static/libsodium.lib") -Force
    }
    Write-OK "libsodium prebuilt: $SodiumDir"
} else { Write-OK "reuse prebuilt libsodium: $SodiumDir" }

# ── protobuf (static, /MD to match the engine CRT) ─────────────────────────────
if (-not (Test-Path (Join-Path $ProtobufDir "lib/libprotobuf.lib"))) {
    if (-not (Test-Path (Join-Path $SourceDir "depends/protobuf/CMakeLists.txt"))) { Die "missing protobuf submodule: git submodule update --init depends/protobuf" }
    $pbBuild = Join-Path $SourceDir "build_protobuf"
    $crt = "/D_CRT_STDIO_ISO_WIDE_SPECIFIERS=1 /D_CRT_NONSTDC_NO_WARNINGS=1 /D_CRT_DECLARE_NONSTDC_NAMES=1"
    Write-Step "Prebuild protobuf (static /MD)"
    cmake -S (Join-Path $SourceDir "depends/protobuf") -B $pbBuild -G "Visual Studio 17 2022" -A x64 `
        "-Dprotobuf_BUILD_TESTS=OFF" "-Dprotobuf_MSVC_STATIC_RUNTIME=OFF" `
        "-Dprotobuf_WITH_ZLIB=OFF" "-Dprotobuf_BUILD_SHARED_LIBS=OFF" `
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" "-DCMAKE_INSTALL_PREFIX=$ProtobufDir" `
        "-DCMAKE_CXX_FLAGS=$crt" "-DCMAKE_C_FLAGS=$crt"
    if ($LASTEXITCODE -ne 0) { Die "protobuf configure failed." }
    foreach ($c in @("Release","Debug")) {
        cmake --build $pbBuild --config $c --target install -- /m
        if ($LASTEXITCODE -ne 0) { Die "protobuf $c install failed." }
    }
    Write-OK "protobuf prebuilt: $ProtobufDir"
} else { Write-OK "reuse prebuilt protobuf: $ProtobufDir" }

# ── OpenSSL (HTTP layer, opt-in) — reuse the repo's idempotent build script ────
$opensslFwd = ""
if ($IncludeOpenSSL) {
    if (Test-Path (Join-Path $OpenSSLDir "lib/libssl.lib")) {
        Write-OK "reuse prebuilt OpenSSL: $OpenSSLDir"
    } else {
        Write-Step "Prebuild Windows x64 OpenSSL -> $OpenSSLDir"
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $SourceDir "scripts/build_windows_openssl.ps1") -Prefix $OpenSSLDir
        if ($LASTEXITCODE -ne 0) { Die "OpenSSL prebuild failed." }
        Write-OK "OpenSSL prebuilt: $OpenSSLDir"
    }
    $opensslFwd = ($OpenSSLDir -replace '\\','/')
}

# ── export dirs to the workflow environment (forward slashes for CMake) ────────
$sodiumFwd   = ($SodiumDir   -replace '\\','/')
$protobufFwd = ($ProtobufDir -replace '\\','/')
if ($env:GITHUB_ENV) {
    Add-Content -Path $env:GITHUB_ENV -Value "DSE_NET_SODIUM_DIR=$sodiumFwd"
    Add-Content -Path $env:GITHUB_ENV -Value "DSE_NET_PROTOBUF_DIR=$protobufFwd"
    if ($opensslFwd) { Add-Content -Path $env:GITHUB_ENV -Value "DSE_HTTP_OPENSSL_DIR=$opensslFwd" }
}
Write-Host "DSE_NET_SODIUM_DIR=$sodiumFwd"
Write-Host "DSE_NET_PROTOBUF_DIR=$protobufFwd"
if ($opensslFwd) { Write-Host "DSE_HTTP_OPENSSL_DIR=$opensslFwd" }
