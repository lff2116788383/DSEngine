<#
.SYNOPSIS
    预构建 Windows x64 OpenSSL 静态库（供 dse_http / IXWebSocket TLS 后端使用）。
.DESCRIPTION
    幂等：若目标 prefix 下已有 libssl.lib + libcrypto.lib + 头文件，则直接跳过。
    否则自动：①确保 Strawberry Perl（OpenSSL 的 Configure 需要，msys/git 自带 perl
    会被拒绝）②获取 OpenSSL 源码（优先复用本机已有源码目录，否则下载 tar.gz 解压）
    ③用 MSVC(vcvars64) 以 VC-WIN64A no-asm no-shared no-tests 构建并 install_sw。

    关键：**不能**带 no-deprecated。IXWebSocket 的 IXSocketOpenSSL.cpp 用到
    OpenSSL_add_ssl_algorithms / SSL_load_error_strings / SSL_set_ecdh_auto 等
    1.0/1.1 兼容初始化符号，no-deprecated 会把它们裁掉导致编译失败。

    与 verify_windows_build.ps1 -WithHttp 及 cmake/CMakeLists.txt.http 自动兜底配套。
.PARAMETER Prefix
    安装根目录，默认 C:\ossl-win64\install（cmake/CMakeLists.txt.http 默认在此查找）。
.PARAMETER Version
    OpenSSL 版本，默认 1.1.1w。
.PARAMETER WorkDir
    源码/构建工作目录，默认 <Prefix 的父目录>。
.PARAMETER Force
    即使已构建也强制重建。
.EXAMPLE
    .\scripts\build_windows_openssl.ps1
    .\scripts\build_windows_openssl.ps1 -Prefix C:\ossl-win64\install
#>
[CmdletBinding()]
param(
    [string]$Prefix  = "C:\ossl-win64\install",
    [string]$Version = "1.1.1w",
    [string]$WorkDir = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
function Write-Step($m) { Write-Host "`n>> $m" -ForegroundColor Cyan }
function Write-OK($m)   { Write-Host "   [OK] $m" -ForegroundColor Green }
function Die($m) { Write-Host "   [FAIL] $m" -ForegroundColor Red; exit 1 }

$Prefix = [System.IO.Path]::GetFullPath($Prefix)
if (-not $WorkDir) { $WorkDir = Split-Path $Prefix -Parent }
$WorkDir = [System.IO.Path]::GetFullPath($WorkDir)

# ── 0) 幂等检查 ──────────────────────────────────────────────────────────────
$libSsl    = Join-Path $Prefix "lib\libssl.lib"
$libCrypto = Join-Path $Prefix "lib\libcrypto.lib"
$hdr       = Join-Path $Prefix "include\openssl\ssl.h"
if (-not $Force -and (Test-Path $libSsl) -and (Test-Path $libCrypto) -and (Test-Path $hdr)) {
    Write-OK "OpenSSL 已存在，跳过构建：$Prefix"
    Write-Host "OPENSSL_PREBUILT_OK $Prefix"
    exit 0
}

New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

# ── 1) 确保 Strawberry Perl（OpenSSL Configure 需要；优先它而非 msys/git perl）──
Write-Step "确保 Strawberry Perl"
$perl = $null
if (Test-Path "C:\Strawberry\perl\bin\perl.exe") {
    $perl = "C:\Strawberry\perl\bin\perl.exe"
} else {
    $choco = Get-Command choco -ErrorAction SilentlyContinue
    if (-not $choco) { Die "缺 Strawberry Perl 且未找到 choco。请安装 Strawberry Perl（https://strawberryperl.com）或 Chocolatey 后重试。" }
    Write-Host "   未发现 Strawberry Perl，用 choco 安装..." -ForegroundColor Yellow
    & choco install strawberryperl -y --no-progress
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path "C:\Strawberry\perl\bin\perl.exe")) { Die "Strawberry Perl 安装失败。" }
    $perl = "C:\Strawberry\perl\bin\perl.exe"
}
Write-OK "perl=$perl"

# ── 2) 获取源码 ──────────────────────────────────────────────────────────────
Write-Step "准备 OpenSSL $Version 源码"
$srcName = "openssl-$Version"
$srcDir  = Join-Path $WorkDir $srcName
$candidates = @($srcDir, "C:\ossl-win64\$srcName", "C:\Android\$srcName")
$found = $candidates | Where-Object { Test-Path (Join-Path $_ "Configure") } | Select-Object -First 1
if ($found) {
    $srcDir = $found
    Write-OK "复用已有源码：$srcDir"
} else {
    $tarball = Join-Path $WorkDir "$srcName.tar.gz"
    $urls = @(
        "https://www.openssl.org/source/old/1.1.1/$srcName.tar.gz",
        "https://github.com/openssl/openssl/releases/download/OpenSSL_$($Version)/$srcName.tar.gz"
    )
    $got = $false
    foreach ($u in $urls) {
        try {
            Write-Host "   下载 $u" -ForegroundColor Yellow
            Invoke-WebRequest -Uri $u -OutFile $tarball -UseBasicParsing
            $got = $true; break
        } catch { Write-Host "   下载失败：$u" -ForegroundColor Yellow }
    }
    if (-not $got) { Die "无法下载 OpenSSL 源码（尝试了 $($urls -join ', ')）。请手动放置源码到 $srcDir。" }
    Write-Step "解压源码"
    # Windows 10+ 自带 bsdtar，可解 .tar.gz
    & tar -xzf $tarball -C $WorkDir
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path (Join-Path $srcDir "Configure"))) { Die "解压 OpenSSL 源码失败。" }
    Write-OK "源码就绪：$srcDir"
}

# ── 3) 定位 vcvars64.bat ─────────────────────────────────────────────────────
Write-Step "定位 MSVC 环境 (vcvars64.bat)"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vcvars = $null
if (Test-Path $vswhere) {
    $vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
    if ($vsRoot) {
        $cand = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $cand) { $vcvars = $cand }
    }
}
if (-not $vcvars) {
    $vcvars = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\VC\Auxiliary\Build\vcvars64.bat" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $vcvars) { Die "找不到 vcvars64.bat。请安装 VS2022 Build Tools (C++ v143)。" }
Write-OK "vcvars=$vcvars"

# ── 4) 构建 + 安装 ───────────────────────────────────────────────────────────
Write-Step "构建 OpenSSL $Version (VC-WIN64A no-asm no-shared no-tests)"
$ssldir = Join-Path $Prefix "ssl"
# 把整段放进一个 cmd 批处理执行：vcvars → 加 Strawberry perl 到 PATH → Configure → nmake → install
$perlBin = Split-Path $perl -Parent
$bat = @"
@echo off
call "$vcvars" || exit /b 1
cd /d "$srcDir" || exit /b 1
set PATH=$perlBin;%PATH%
nmake clean 1>nul 2>nul
perl Configure VC-WIN64A no-asm no-shared no-tests --prefix="$Prefix" --openssldir="$ssldir" || exit /b 1
nmake || exit /b 1
nmake install_sw || exit /b 1
echo OPENSSL_NMAKE_OK
"@
$batPath = Join-Path $WorkDir "_build_openssl.bat"
Set-Content -Path $batPath -Value $bat -Encoding ASCII
& cmd /c "`"$batPath`""
if ($LASTEXITCODE -ne 0) { Die "OpenSSL 构建失败 (exit=$LASTEXITCODE)。" }

# ── 5) 校验产物 ──────────────────────────────────────────────────────────────
if (-not ((Test-Path $libSsl) -and (Test-Path $libCrypto) -and (Test-Path $hdr))) {
    Die "构建后仍缺产物（$libSsl / $libCrypto / $hdr）。"
}
$sslMb    = [math]::Round((Get-Item $libSsl).Length / 1MB, 1)
$cryptoMb = [math]::Round((Get-Item $libCrypto).Length / 1MB, 1)
Write-OK ("libssl.lib={0}MB  libcrypto.lib={1}MB" -f $sslMb, $cryptoMb)
Write-OK "OpenSSL 预构建完成：$Prefix"
Write-Host "OPENSSL_PREBUILT_OK $Prefix"
exit 0
