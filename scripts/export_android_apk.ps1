<#
.SYNOPSIS
    DSEngine 游戏 Android 导出：交叉编译引擎+宿主(arm64-v8a) → 把编辑器打包的
    游戏资源(game.dsmanifest / game.dpak / game.bun / launch.cfg)放进 APK assets/
    → aapt2/zipalign/apksigner 打包签名 → 产出可安装的游戏 APK。

.DESCRIPTION
    编辑器 Build Game(Android) 对话框调用本脚本完成 APK 生成；也可手工调用。
    与 scripts/verify_android_apk.ps1 共享同一工具链解析与打包流程，但面向
    "导出成品游戏"：自定义包名/应用名/版本、打入游戏资源、支持发布签名。

    工具链解析顺序：显式参数 > 环境变量(ANDROID_NDK_HOME/ANDROID_HOME/JAVA_HOME)。
    首次构建会全量交叉编译引擎（较慢）；构建目录默认缓存在 <repo>/../dse_build_android，
    后续导出增量编译。

.PARAMETER GameTitle   应用显示名（android:label）。
.PARAMETER PackageId   Android 包名（如 com.mystudio.mygame）。
.PARAMETER OutApk      产出 APK 的完整路径。
.PARAMETER AssetsDir   编辑器暂存的游戏资源目录，内容整体进入 APK assets/。
.PARAMETER Keystore    发布签名 keystore；缺省生成/复用调试 keystore。
.EXAMPLE
    ./export_android_apk.ps1 -GameTitle "My Game" -PackageId com.foo.mygame `
        -OutApk D:/out/MyGame.apk -AssetsDir D:/out/.dse_android_assets
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GameTitle,
    [Parameter(Mandatory = $true)][string]$PackageId,
    [Parameter(Mandatory = $true)][string]$OutApk,
    [string]$AssetsDir = "",
    [string]$Abi = "arm64-v8a",
    [int]$ApiLevel = 24,
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",
    [string]$BuildTools = "34.0.0",
    [string]$Platform = "android-34",
    [int]$VersionCode = 1,
    [string]$VersionName = "1.0.0",
    [string]$BuildDir = "",
    [string]$Ndk = "",
    [string]$Sdk = "",
    [string]$JavaHome = "",
    [string]$HostShaderCompiler = "",
    [string]$Keystore = "",
    [string]$KeystorePass = "",
    [string]$KeyAlias = ""
)

$ErrorActionPreference = "Stop"
$SourceDir = Split-Path -Parent $PSScriptRoot

function Write-Step($msg) { Write-Host "==> $msg" }
function Write-OK($msg)   { Write-Host "  OK $msg" }
function Die($msg)        { Write-Host "ERROR: $msg"; exit 1 }
function Find-First($paths) {
    foreach ($p in $paths) { if ($p -and (Test-Path $p)) { return $p } }
    return $null
}

if ($PackageId -notmatch '^[a-zA-Z][a-zA-Z0-9_]*(\.[a-zA-Z][a-zA-Z0-9_]*)+$') {
    Die "非法包名：$PackageId（须形如 com.studio.game）"
}

# ── 1. 解析工具链 ───────────────────────────────────────────────────────────
Write-Step "解析工具链"

if (-not $Ndk)  { $Ndk  = $env:ANDROID_NDK_HOME; if (-not $Ndk) { $Ndk = $env:ANDROID_NDK_ROOT } }
if (-not $Sdk)  { $Sdk  = $env:ANDROID_HOME;     if (-not $Sdk) { $Sdk = $env:ANDROID_SDK_ROOT } }
if (-not $JavaHome) { $JavaHome = $env:JAVA_HOME }

# 环境变量未设置时探测常见安装位置
if (-not $Sdk) {
    $Sdk = Find-First @(
        (Join-Path $env:LOCALAPPDATA "Android/Sdk"),
        "C:/Android/sdk", "$env:USERPROFILE/Android/Sdk", "/opt/android-sdk")
}
if (-not $Ndk) {
    $candidates = @()
    if ($Sdk) {
        $candidates += Get-ChildItem -Path (Join-Path $Sdk "ndk") -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending | ForEach-Object { $_.FullName }
    }
    $candidates += Get-ChildItem -Path "C:/Android" -Directory -Filter "android-ndk-*" -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | ForEach-Object { $_.FullName }
    $Ndk = Find-First $candidates
}
if (-not $JavaHome) {
    $candidates = @()
    foreach ($root in @("C:/Android/jdk", "C:/Program Files/Eclipse Adoptium", "C:/Program Files/Java")) {
        $candidates += Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending | ForEach-Object { $_.FullName }
    }
    $JavaHome = Find-First $candidates
}

if (-not $Ndk -or -not (Test-Path $Ndk)) { Die "找不到 Android NDK。用 -Ndk 指定或设置 ANDROID_NDK_HOME。" }
if (-not $Sdk -or -not (Test-Path $Sdk)) { Die "找不到 Android SDK。用 -Sdk 指定或设置 ANDROID_HOME。" }
if (-not $JavaHome -or -not (Test-Path $JavaHome)) { Die "找不到 JDK。用 -JavaHome 指定或设置 JAVA_HOME。" }
$env:JAVA_HOME = $JavaHome   # apksigner.bat 等子进程依赖

$Toolchain = Join-Path $Ndk "build/cmake/android.toolchain.cmake"
if (-not (Test-Path $Toolchain)) { Die "NDK 工具链文件不存在：$Toolchain" }

$exe = if ($IsWindows -or $env:OS -eq "Windows_NT") { ".exe" } else { "" }
$bat = if ($IsWindows -or $env:OS -eq "Windows_NT") { ".bat" } else { "" }

$BtDir      = Join-Path $Sdk "build-tools/$BuildTools"
$Aapt2      = Join-Path $BtDir "aapt2$exe"
$ZipAlign   = Join-Path $BtDir "zipalign$exe"
$ApkSigner  = Join-Path $BtDir "apksigner$bat"
$AndroidJar = Join-Path $Sdk "platforms/$Platform/android.jar"
$KeyTool    = Join-Path $JavaHome "bin/keytool$exe"
$Jar        = Join-Path $JavaHome "bin/jar$exe"

foreach ($t in @($Aapt2,$ZipAlign,$ApkSigner,$AndroidJar,$KeyTool,$Jar)) {
    if (-not (Test-Path $t)) { Die "缺少打包工具：$t（检查 build-tools=$BuildTools / platform=$Platform）。" }
}

if (-not $HostShaderCompiler) {
    $HostShaderCompiler = Find-First @(
        (Join-Path $SourceDir "bin/dse_shader_compiler$exe"),
        (Join-Path $SourceDir "bin/dse_shader_compiler"))
}
if (-not $HostShaderCompiler -or -not (Test-Path $HostShaderCompiler)) {
    Die "缺少 host 版 dse_shader_compiler（交叉编译需要）。先在桌面平台构建一次，或用 -HostShaderCompiler 指定。"
}

$CMakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
$CMake = if ($CMakeCmd) { $CMakeCmd.Source } else { Find-First @("C:/Program Files/CMake/bin/cmake.exe") }
if (-not $CMake) { Die "找不到 cmake。" }
$NinjaCmd = Get-Command ninja -ErrorAction SilentlyContinue
$Ninja = if ($NinjaCmd) { $NinjaCmd.Source } else { Find-First @("C:/ProgramData/chocolatey/bin/ninja.exe", (Join-Path $Ndk "prebuilt/windows-x86_64/bin/ninja.exe")) }
if (-not $Ninja) { Die "找不到 ninja。" }

Write-OK "NDK=$Ndk"
Write-OK "SDK=$Sdk  build-tools=$BuildTools  platform=$Platform"
Write-OK "JDK=$JavaHome"

# ── 2. 交叉编译引擎 + 宿主 .so ─────────────────────────────────────────────
if (-not $BuildDir) { $BuildDir = (Join-Path $SourceDir "..\dse_build_android") }
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)

Write-Step "配置/构建 dse_android_host ($Abi, android-$ApiLevel, $Config)"
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

# ── 3. 暂存 APK 内容（native libs + 游戏资源 + 清单）──────────────────────
Write-Step "暂存 APK 内容"
$Stage  = Join-Path $BuildDir "apk_export_stage"
$LibDir = Join-Path $Stage "lib/$Abi"
Remove-Item -Recurse -Force $Stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $LibDir | Out-Null

Copy-Item $HostSo (Join-Path $LibDir "libdse_android_host.so") -Force
if ($EngineSo) { Copy-Item $EngineSo.FullName (Join-Path $LibDir $EngineSo.Name) -Force }
$LibCxx = Find-First @(
    (Join-Path $Ndk "toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"),
    (Join-Path $Ndk "toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"))
if ($LibCxx -and $Abi -eq "arm64-v8a") {
    Copy-Item $LibCxx (Join-Path $LibDir "libc++_shared.so") -Force
}

$HasAssets = $false
if ($AssetsDir -and (Test-Path $AssetsDir)) {
    $AssetStage = Join-Path $Stage "assets"
    New-Item -ItemType Directory -Force -Path $AssetStage | Out-Null
    Copy-Item (Join-Path $AssetsDir "*") $AssetStage -Recurse -Force
    $count = (Get-ChildItem $AssetStage -Recurse -File | Measure-Object).Count
    if ($count -gt 0) { $HasAssets = $true; Write-OK "游戏资源: $count 个文件进入 assets/" }
}
if (-not $HasAssets) { Write-Host "  WARNING: 未提供游戏资源（-AssetsDir），APK 仅含引擎宿主。" }

# 生成带包名/应用名/版本的 AndroidManifest（以 apps/android_host 的清单为模板结构）
$ManifestPath = Join-Path $Stage "AndroidManifest.xml"
$Label = [System.Security.SecurityElement]::Escape($GameTitle)
@"
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="$PackageId"
          android:versionCode="$VersionCode"
          android:versionName="$VersionName">

    <uses-sdk android:minSdkVersion="$ApiLevel" android:targetSdkVersion="34" />

    <uses-feature android:glEsVersion="0x00030001" android:required="true" />

    <application
        android:label="$Label"
        android:hasCode="false"
        android:allowBackup="false">

        <activity
            android:name="android.app.NativeActivity"
            android:label="$Label"
            android:configChanges="orientation|keyboardHidden|screenSize"
            android:exported="true">

            <meta-data
                android:name="android.app.lib_name"
                android:value="dse_android_host" />

            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
"@ | Set-Content -Path $ManifestPath -Encoding UTF8

# ── 4. aapt2 link + 追加内容 + zipalign ────────────────────────────────────
Write-Step "打包 APK"
$OutDir = Split-Path -Parent ([System.IO.Path]::GetFullPath($OutApk))
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$LinkedApk  = Join-Path $Stage "linked.apk"
$AlignedApk = Join-Path $Stage "aligned.apk"

& $Aapt2 link -o $LinkedApk -I $AndroidJar --manifest $ManifestPath `
    --min-sdk-version $ApiLevel --target-sdk-version 34 `
    --version-code $VersionCode --version-name $VersionName
if ($LASTEXITCODE -ne 0) { Die "aapt2 link 失败。" }
Write-OK "aapt2 link 完成"

Push-Location $Stage
& $Jar uf $LinkedApk -C $Stage "lib"
if ($LASTEXITCODE -ne 0) { Pop-Location; Die "jar 追加 native libs 失败。" }
if ($HasAssets) {
    & $Jar uf $LinkedApk -C $Stage "assets"
    if ($LASTEXITCODE -ne 0) { Pop-Location; Die "jar 追加游戏资源失败。" }
}
Pop-Location
Write-OK "已加入 native libs$(if ($HasAssets) { ' + 游戏资源' })"

& $ZipAlign -p -f 4 $LinkedApk $AlignedApk
if ($LASTEXITCODE -ne 0) { Die "zipalign 失败。" }
Write-OK "zipalign 完成"

# ── 5. 签名（发布 keystore 或自动调试 keystore）────────────────────────────
Write-Step "签名 APK"
if ($Keystore) {
    if (-not (Test-Path $Keystore)) { Die "keystore 不存在：$Keystore" }
    if (-not $KeystorePass -or -not $KeyAlias) { Die "使用 -Keystore 时须同时提供 -KeystorePass 和 -KeyAlias。" }
    $UseKs = $Keystore; $UsePass = $KeystorePass; $UseAlias = $KeyAlias
    Write-OK "使用发布 keystore：$Keystore (alias=$KeyAlias)"
} else {
    $UseKs = Join-Path $BuildDir "dse_debug.keystore"
    $UsePass = "android"; $UseAlias = "dsengine"
    if (-not (Test-Path $UseKs)) {
        & $KeyTool -genkeypair -v -keystore $UseKs -alias $UseAlias `
            -keyalg RSA -keysize 2048 -validity 10000 `
            -storepass $UsePass -keypass $UsePass `
            -dname "CN=DSEngine Debug, OU=Dev, O=DSEngine, L=NA, S=NA, C=NA"
        if ($LASTEXITCODE -ne 0) { Die "keytool 生成调试签名失败。" }
    }
    Write-OK "使用调试 keystore（发布请提供 -Keystore/-KeystorePass/-KeyAlias）"
}
& $ApkSigner sign --ks $UseKs --ks-key-alias $UseAlias `
    --ks-pass "pass:$UsePass" --key-pass "pass:$UsePass" `
    --out $OutApk $AlignedApk
if ($LASTEXITCODE -ne 0) { Die "apksigner 签名失败。" }

# ── 6. 校验 ─────────────────────────────────────────────────────────────────
Write-Step "校验 APK"
& $ApkSigner verify $OutApk
if ($LASTEXITCODE -ne 0) { Die "apksigner 验证失败。" }
Write-OK "签名验证通过"

$badging = & $Aapt2 dump badging $OutApk 2>&1
if (-not ($badging | Select-String -Pattern "package: name='$PackageId'")) { Die "APK 包名不符合预期。" }
$entries = & $Jar tf $OutApk
if (-not ($entries | Select-String -Pattern "lib/$Abi/libdse_android_host.so")) { Die "APK 内未找到宿主 .so。" }
if ($HasAssets -and -not ($entries | Select-String -Pattern "^assets/")) { Die "APK 内未找到游戏资源。" }
Write-OK "包名/宿主库/游戏资源 校验通过"

Remove-Item -Recurse -Force $Stage -ErrorAction SilentlyContinue

$apkSizeMb = [math]::Round((Get-Item $OutApk).Length / 1MB, 1)
Write-Host "==> 导出完成: $OutApk  size=$apkSizeMb MB"
Write-Host "    安装: adb install -r `"$OutApk`""
exit 0
