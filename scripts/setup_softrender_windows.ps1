<#
.SYNOPSIS
    在无 GPU 的 Windows 机器上配置三后端「软件渲染」栈，供 RHI 跨后端测试使用：
      OpenGL  → Mesa llvmpipe (opengl32.dll 等放进 bin/，覆盖系统 GL)
      Vulkan  → Mesa lavapipe (vulkan_lvp.dll + ICD JSON，经注册表注册)
      D3D11   → Windows 内建 WARP（无需额外文件）
.DESCRIPTION
    幂等：已下载/解压/拷贝/注册的步骤会被跳过或覆盖，可重复执行。
    依赖：choco 自带的 7z（解压）、管理员权限（写 HKLM 注册表 Vulkan ICD）。
    需先完成一次构建（bin/ 存在），脚本会把 GL dll 与 vulkan-1.dll 放到 bin/ 紧邻测试 exe。
    跑测试：GALLIUM_DRIVER=llvmpipe ./bin/dse_gtest_*.exe
.NOTE
    管理员下 Vulkan loader 进入 secure 模式会忽略 VK_ICD_FILENAMES/VK_DRIVER_FILES，
    故 lavapipe 必须经注册表 HKLM\SOFTWARE\Khronos\Vulkan\Drivers 注册。
#>
[CmdletBinding()]
param(
    [string]$MesaVersion = "26.1.2",
    [string]$MesaRoot = "C:\Users\Administrator\mesa"
)

$ErrorActionPreference = "Stop"
$RepoRoot  = (Resolve-Path "$PSScriptRoot\..").Path
$BinDir    = Join-Path $RepoRoot "bin"
$Extracted = Join-Path $MesaRoot "extracted"
$Vk        = Join-Path $MesaRoot "vk"
$Archive   = Join-Path $MesaRoot "mesa.7z"
$X64       = Join-Path $Extracted "x64"

New-Item -ItemType Directory -Force -Path $MesaRoot, $Vk, $BinDir | Out-Null

# ── 1. 下载 Mesa MSVC 发行包（缺则下） ───────────────────────────────────────
if (-not (Test-Path $Archive)) {
    $url = "https://github.com/pal1000/mesa-dist-win/releases/download/$MesaVersion/mesa3d-$MesaVersion-release-msvc.7z"
    Write-Host ">> 下载 Mesa $MesaVersion : $url"
    Invoke-WebRequest -Uri $url -OutFile $Archive
}

# ── 2. 解压（缺 x64/opengl32.dll 则解） ──────────────────────────────────────
if (-not (Test-Path (Join-Path $X64 "opengl32.dll"))) {
    $sevenZip = "C:\ProgramData\chocolatey\bin\7z.exe"
    if (-not (Test-Path $sevenZip)) { $sevenZip = "7z" }
    Write-Host ">> 解压 $Archive -> $Extracted"
    & $sevenZip x $Archive "-o$Extracted" -y | Out-Null
}

# ── 3. OpenGL：llvmpipe GL dll 拷进 bin/（覆盖系统 opengl32，DLL 搜索就近命中） ──
foreach ($d in @("opengl32.dll", "libgallium_wgl.dll", "libGLESv2.dll", "libEGL.dll")) {
    Copy-Item (Join-Path $X64 $d) -Destination $BinDir -Force
}

# ── 4. Vulkan：lavapipe dll + ICD JSON + 注册表注册 ──────────────────────────
Copy-Item (Join-Path $X64 "vulkan_lvp.dll") -Destination $Vk -Force
$lvpDll  = (Join-Path $Vk "vulkan_lvp.dll").Replace('\', '\\')
$icdJson = Join-Path $Vk "lvp_icd.x86_64.json"
$icd = '{"ICD":{"api_version":"1.4.348","library_arch":"64","library_path":"' + $lvpDll + '"},"file_format_version":"1.0.1"}'
Set-Content -Path $icdJson -Value $icd -Encoding ascii
$reg = "HKLM:\SOFTWARE\Khronos\Vulkan\Drivers"
if (-not (Test-Path $reg)) { New-Item -Path $reg -Force | Out-Null }
New-ItemProperty -Path $reg -Name $icdJson -PropertyType DWord -Value 0 -Force | Out-Null

# ── 5. GALLIUM_DRIVER=llvmpipe（机器级 + 当前进程） ──────────────────────────
[Environment]::SetEnvironmentVariable("GALLIUM_DRIVER", "llvmpipe", "Machine")
$env:GALLIUM_DRIVER = "llvmpipe"

# ── 6. Vulkan loader：若已构建则拷到 bin/ 紧邻测试 exe ───────────────────────
$loader = Join-Path $RepoRoot "out\build\windows-x64-debug\third_party\vulkan-loader\loader\vulkan-1.dll"
if (Test-Path $loader) { Copy-Item $loader -Destination $BinDir -Force }

Write-Host ">> 软渲栈就绪：GL=llvmpipe / Vulkan=lavapipe(registry) / D3D11=WARP"
Write-Host "   ICD: $icdJson"
Write-Host "   bin GL dll: opengl32/libgallium_wgl/libGLESv2/libEGL"
