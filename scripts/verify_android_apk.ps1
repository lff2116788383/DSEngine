<#
.SYNOPSIS
    Android APK 端到端打包验证：交叉编译引擎(arm64-v8a) → 构建 NativeActivity 宿主
    → 用 aapt2/zipalign/apksigner 打包并签名 → 校验 APK 结构与签名。
.DESCRIPTION
    用于 CI 或本地验证「DSEngine 能成功打出可安装的 Android APK」。
    依赖工具链（任一缺失即失败，并提示安装方式）：
      - Android NDK（含 native_app_glue），由 -Ndk 或环境变量 ANDROID_NDK_HOME/ANDROID_NDK_ROOT 指定
      - Android SDK（platforms;android-34 + build-tools 含 aapt2/zipalign/apksigner），
        由 -Sdk 或 ANDROID_HOME/ANDROID_SDK_ROOT 指定
      - JDK（keytool，用于生成调试签名），由 -JavaHome 或 JAVA_HOME 指定
      - 交叉编译需要 host 版 dse_shader_compiler（先在桌面平台构建一次得到 bin/dse_shader_compiler[.exe]）
.PARAMETER Abi
    目标 ABI，默认 arm64-v8a。
.PARAMETER ApiLevel
    Android 平台 API（android-<N>），默认 24。
.PARAMETER BuildTools
    build-tools 版本，默认 34.0.0。
.PARAMETER Platform
    编译/打包所用 android.jar 的平台，默认 android-34。
.PARAMETER BuildDir
    CMake 构建目录，默认 <repo>/../dse_build_android。已存在则增量构建。
.PARAMETER Ndk / -Sdk / -JavaHome / -HostShaderCompiler
    显式指定工具链路径（覆盖环境变量自动探测）。
.PARAMETER KeepArtifacts
    保留中间产物（默认保留 APK，本开关额外保留 staging 目录）。
.EXAMPLE
    .\scripts\verify_android_apk.ps1
    .\scripts\verify_android_apk.ps1 -Abi arm64-v8a -ApiLevel 24
#>

[CmdletBinding()]
param(
    [string]$Abi = "arm64-v8a",
    [int]$ApiLevel = 24,
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Debug",
    [string]$BuildTools = "34.0.0",
    [string]$Platform = "android-34",
    [string]$BuildDir = "",
    [string]$Ndk = "",
    [string]$Sdk = "",
    [string]$JavaHome = "",
    [string]$HostShaderCompiler = "",
    [switch]$KeepArtifacts
)

# cmake 的 stderr 警告会被 PowerShell 当作终止错误，这里统一手动检查 $LASTEXITCODE
$ErrorActionPreference = "Continue"
$SourceDir = (Resolve-Path "$PSScriptRoot\..").Path

function Write-Step($msg) { Write-Host "`n>> $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "   [OK] $msg" -ForegroundColor Green }
function Write-FAIL($msg) { Write-Host "   [FAIL] $msg" -ForegroundColor Red }
function Die($msg) { Write-FAIL $msg; exit 1 }

function Find-First($candidates) {
    foreach ($c in $candidates) {
        if ($c -and (Test-Path $c)) { return (Resolve-Path $c).Path }
    }
    return $null
}

# ── 1. 解析工具链 ───────────────────────────────────────────────────────────
Write-Step "解析工具链"

if (-not $Ndk)  { $Ndk  = $env:ANDROID_NDK_HOME; if (-not $Ndk) { $Ndk = $env:ANDROID_NDK_ROOT } }
if (-not $Sdk)  { $Sdk  = $env:ANDROID_HOME;     if (-not $Sdk) { $Sdk = $env:ANDROID_SDK_ROOT } }
if (-not $JavaHome) { $JavaHome = $env:JAVA_HOME }

if (-not $Ndk -or -not (Test-Path $Ndk)) { Die "找不到 Android NDK。用 -Ndk 指定或设置 ANDROID_NDK_HOME。" }
if (-not $Sdk -or -not (Test-Path $Sdk)) { Die "找不到 Android SDK。用 -Sdk 指定或设置 ANDROID_HOME。" }
if (-not $JavaHome -or -not (Test-Path $JavaHome)) { Die "找不到 JDK。用 -JavaHome 指定或设置 JAVA_HOME（keytool 需要）。" }

$Toolchain = Join-Path $Ndk "build/cmake/android.toolchain.cmake"
if (-not (Test-Path $Toolchain)) { Die "NDK 工具链文件不存在：$Toolchain" }

$exe = if ($IsWindows -or $env:OS -eq "Windows_NT") { ".exe" } else { "" }
$bat = if ($IsWindows -or $env:OS -eq "Windows_NT") { ".bat" } else { "" }

$BtDir   = Join-Path $Sdk "build-tools/$BuildTools"
$Aapt2   = Join-Path $BtDir "aapt2$exe"
$ZipAlign= Join-Path $BtDir "zipalign$exe"
$ApkSigner = Join-Path $BtDir "apksigner$bat"
$AndroidJar = Join-Path $Sdk "platforms/$Platform/android.jar"
$KeyTool = Join-Path $JavaHome "bin/keytool$exe"

foreach ($t in @($Aapt2,$ZipAlign,$ApkSigner,$AndroidJar,$KeyTool)) {
    if (-not (Test-Path $t)) { Die "缺少打包工具：$t（检查 build-tools=$BuildTools / platform=$Platform）。" }
}

# 交叉编译所需 host 版 shader 编译器
if (-not $HostShaderCompiler) {
    $HostShaderCompiler = Find-First @(
        (Join-Path $SourceDir "bin/dse_shader_compiler$exe"),
        (Join-Path $SourceDir "bin/dse_shader_compiler"))
}
if (-not $HostShaderCompiler -or -not (Test-Path $HostShaderCompiler)) {
    Die "缺少 host 版 dse_shader_compiler（交叉编译需要）。先在桌面平台构建一次以生成 bin/dse_shader_compiler[.exe]，或用 -HostShaderCompiler 指定。"
}

# cmake / ninja
$CMakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
$CMake = if ($CMakeCmd) { $CMakeCmd.Source } else { Find-First @("C:/Program Files/CMake/bin/cmake.exe") }
if (-not $CMake) { Die "找不到 cmake。" }
$NinjaCmd = Get-Command ninja -ErrorAction SilentlyContinue
$Ninja = if ($NinjaCmd) { $NinjaCmd.Source } else { Find-First @("C:/ProgramData/chocolatey/bin/ninja.exe", (Join-Path $Ndk "prebuilt/windows-x86_64/bin/ninja.exe")) }
if (-not $Ninja) { Die "找不到 ninja。" }

Write-OK "NDK=$Ndk"
Write-OK "SDK=$Sdk  build-tools=$BuildTools  platform=$Platform"
Write-OK "JDK=$JavaHome"
Write-OK "host shader compiler=$HostShaderCompiler"

# ── 2. CMake 配置 + 构建宿主 .so ───────────────────────────────────────────
if (-not $BuildDir) { $BuildDir = (Join-Path $SourceDir "..\dse_build_android") }
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)

Write-Step "配置/构建 dse_android_host ($Abi, $Platform)"
if (-not (Test-Path (Join-Path $BuildDir "build.ninja"))) {
    & $CMake -S $SourceDir -B $BuildDir -G Ninja `
        "-DCMAKE_TOOLCHAIN_FILE=$Toolchain" `
        "-DCMAKE_MAKE_PROGRAM=$Ninja" `
        "-DANDROID_ABI=$Abi" `
        "-DANDROID_PLATFORM=android-$ApiLevel" `
        "-DCMAKE_BUILD_TYPE=$Config" `
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" `
        "-DDSE_BUILD_SHARED=OFF" `
        "-DDSE_BUILD_GTESTS=OFF" `
        "-DDSE_ENABLE_D3D11=OFF" `
        "-DDSE_ENABLE_VULKAN=OFF" `
        "-DDSE_ENABLE_PHYSX=OFF" `
        "-DDSE_ENABLE_JOLT=ON" `
        "-DDSE_HOST_SHADER_COMPILER=$HostShaderCompiler"
    if ($LASTEXITCODE -ne 0) { Die "CMake 配置失败。" }
}
& $CMake --build $BuildDir --target dse_android_host
if ($LASTEXITCODE -ne 0) { Die "构建 dse_android_host 失败。" }

$HostSo   = Join-Path $BuildDir "libdse_android_host.so"
$EngineSo = Get-ChildItem -Path $BuildDir -Filter "libDSEngine*.so" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not (Test-Path $HostSo)) { Die "未找到 libdse_android_host.so。" }
Write-OK "宿主库: $HostSo"
if ($EngineSo) { Write-OK "引擎库: $($EngineSo.FullName)" }

# ── 3. 打包 APK ─────────────────────────────────────────────────────────────
Write-Step "打包 APK"
$Stage   = Join-Path $BuildDir "apk_stage"
$OutDir  = Join-Path $BuildDir "apk_out"
$LibDir  = Join-Path $Stage "lib/$Abi"
Remove-Item -Recurse -Force $Stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $LibDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Copy-Item $HostSo (Join-Path $LibDir "libdse_android_host.so") -Force
if ($EngineSo) { Copy-Item $EngineSo.FullName (Join-Path $LibDir $EngineSo.Name) -Force }
# 若引擎依赖 libc++_shared.so（动态 STL），一并打入
$LibCxx = Find-First @(
    (Join-Path $Ndk "toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"),
    (Join-Path $Ndk "toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"))
if ($LibCxx -and $Abi -eq "arm64-v8a") {
    # 仅当任一 .so NEEDED 了 libc++_shared.so 才需要；保守起见若存在就带上（不影响运行）
    Copy-Item $LibCxx (Join-Path $LibDir "libc++_shared.so") -Force
    Write-OK "已打入 libc++_shared.so"
}

$Manifest = Join-Path $SourceDir "apps/android_host/AndroidManifest.xml"
if (-not (Test-Path $Manifest)) { Die "找不到 AndroidManifest.xml：$Manifest" }

$BaseApk    = Join-Path $OutDir "base.apk"
$LinkedApk  = Join-Path $OutDir "linked.apk"
$AlignedApk = Join-Path $OutDir "dsengine-host-unsigned.apk"
$FinalApk   = Join-Path $OutDir "dsengine-host.apk"

# aapt2 link：无资源工程，仅清单 + 平台 android.jar
& $Aapt2 link -o $LinkedApk -I $AndroidJar --manifest $Manifest `
    --min-sdk-version $ApiLevel --target-sdk-version 34 `
    --version-code 1 --version-name "0.1.0"
if ($LASTEXITCODE -ne 0) { Die "aapt2 link 失败。" }
Write-OK "aapt2 link 完成"

# 把 native libs 追加进 APK（APK 即 zip；用 zip 工具加入 lib/<abi>/*.so，存储方式不压缩 .so）
Push-Location $Stage
# 优先使用 jar（JDK 自带）以避免对 7z 的依赖
$Jar = Join-Path $JavaHome "bin/jar$exe"
if (Test-Path $Jar) {
    & $Jar uf $LinkedApk -C $Stage "lib"
    if ($LASTEXITCODE -ne 0) { Pop-Location; Die "jar 追加 native libs 失败。" }
} else {
    Pop-Location; Die "找不到 jar 工具（$Jar）。"
}
Pop-Location
Write-OK "已加入 native libs (lib/$Abi/)"

# zipalign（4 字节对齐；.so 需 page 对齐由 -p 处理）
& $ZipAlign -p -f 4 $LinkedApk $AlignedApk
if ($LASTEXITCODE -ne 0) { Die "zipalign 失败。" }
Write-OK "zipalign 完成"

# ── 4. 签名 ─────────────────────────────────────────────────────────────────
Write-Step "签名 APK"
$Keystore = Join-Path $OutDir "debug.keystore"
$StorePass = "android"
$KeyAlias = "dsengine"
if (-not (Test-Path $Keystore)) {
    & $KeyTool -genkeypair -v -keystore $Keystore -alias $KeyAlias `
        -keyalg RSA -keysize 2048 -validity 10000 `
        -storepass $StorePass -keypass $StorePass `
        -dname "CN=DSEngine Debug, OU=Dev, O=DSEngine, L=NA, S=NA, C=NA"
    if ($LASTEXITCODE -ne 0) { Die "keytool 生成调试签名失败。" }
}
& $ApkSigner sign --ks $Keystore --ks-key-alias $KeyAlias `
    --ks-pass "pass:$StorePass" --key-pass "pass:$StorePass" `
    --out $FinalApk $AlignedApk
if ($LASTEXITCODE -ne 0) { Die "apksigner 签名失败。" }
Write-OK "签名完成: $FinalApk"

# ── 5. 校验 ─────────────────────────────────────────────────────────────────
Write-Step "校验 APK"
& $ApkSigner verify --verbose $FinalApk
if ($LASTEXITCODE -ne 0) { Die "apksigner 验证失败。" }
Write-OK "apksigner 验证通过"

# aapt2 dump：确认包名/启动 Activity/native lib 元数据
$badging = & $Aapt2 dump badging $FinalApk 2>&1
if ($LASTEXITCODE -ne 0) { Die "aapt2 dump badging 失败。" }
$pkgOk = $badging | Select-String -Pattern "package: name='com.dsengine.androidhost'"
if (-not $pkgOk) { Die "APK 包名不符合预期。" }
Write-OK "包名校验通过 (com.dsengine.androidhost)"

# 确认 APK 内含 native .so
$contents = & $Aapt2 dump resources $FinalApk 2>$null
$libOk = (& $Jar tf $FinalApk) | Select-String -Pattern "lib/$Abi/libdse_android_host.so"
if (-not $libOk) { Die "APK 内未找到 lib/$Abi/libdse_android_host.so。" }
Write-OK "native 库校验通过 (lib/$Abi/libdse_android_host.so)"

$apkSizeMb = [math]::Round((Get-Item $FinalApk).Length / 1MB, 1)
if (-not $KeepArtifacts) { Remove-Item -Recurse -Force $Stage -ErrorAction SilentlyContinue }

Write-Host "`n==================== RESULT ====================" -ForegroundColor Cyan
Write-OK "APK 打包验证全部通过"
Write-Host ("   APK: {0}  size={1} MB" -f $FinalApk, $apkSizeMb) -ForegroundColor Green
Write-Host ("   安装: adb install -r `"{0}`"" -f $FinalApk) -ForegroundColor Green
exit 0
