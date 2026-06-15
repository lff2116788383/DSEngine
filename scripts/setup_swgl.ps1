<#
.SYNOPSIS
    部署软件 OpenGL（Mesa3D llvmpipe），让编辑器 / GL 后端在无显卡的机器
    （CI runner、无 GPU 的服务器/VM）上也能创建 OpenGL 4.x 上下文并渲染。
.DESCRIPTION
    DSEngine 编辑器的 UI 走 ImGui 的 OpenGL3 后端，OpenGL 运行时后端同理，
    都需要桌面 OpenGL。纯软件渲染的机器没有 GL 4.x，会导致
    "Failed to create GLFW window"。本脚本下载 Mesa3D 的 Windows 发行包，
    把 opengl32.dll + libgallium_wgl.dll（llvmpipe 软件光栅器）落到 bin 目录，
    使其优先于系统 opengl32.dll 被加载。

    运行时无需额外环境变量即可自动回退到 llvmpipe；如需强制可设置
    GALLIUM_DRIVER=llvmpipe。
.PARAMETER BinDir
    DLL 落地目录，默认 <repo>/bin。
.PARAMETER Version
    Mesa3D 版本，默认取 pal1000/mesa-dist-win 最新 release。
.PARAMETER Force
    即使 DLL 已存在也重新下载覆盖。
.EXAMPLE
    pwsh scripts/setup_swgl.ps1
.EXAMPLE
    GALLIUM_DRIVER=llvmpipe ./bin/dsengine-editor.exe
.NOTES
    仅用于测试 / 无 GPU 环境；有真实显卡时不需要、也不应使用本脚本。
#>
[CmdletBinding()]
param(
    [string]$BinDir,
    [string]$Version = '',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BinDir) { $BinDir = Join-Path $repoRoot 'bin' }
New-Item -ItemType Directory -Force -Path $BinDir | Out-Null

$openglDll = Join-Path $BinDir 'opengl32.dll'
$galliumDll = Join-Path $BinDir 'libgallium_wgl.dll'
if ((Test-Path $openglDll) -and (Test-Path $galliumDll) -and -not $Force) {
    Write-Host "[swgl] Mesa DLLs already present in $BinDir (use -Force to refresh)."
    exit 0
}

$headers = @{ 'User-Agent' = 'dse-setup-swgl' }
if (-not $Version) {
    Write-Host '[swgl] Resolving latest Mesa3D release...'
    $rel = Invoke-RestMethod -UseBasicParsing -Headers $headers `
        -Uri 'https://api.github.com/repos/pal1000/mesa-dist-win/releases/latest'
    $Version = $rel.tag_name
}
Write-Host "[swgl] Mesa3D version: $Version"

$asset = "mesa3d-$Version-release-msvc.7z"
$url = "https://github.com/pal1000/mesa-dist-win/releases/download/$Version/$asset"

$work = Join-Path $env:TEMP "dse_swgl"
New-Item -ItemType Directory -Force -Path $work | Out-Null
$archive = Join-Path $work $asset

Write-Host "[swgl] Downloading $url ..."
Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri $url -OutFile $archive

# 取一个 7z 解压器：优先系统 7z，否则下载官方 7zr.exe（可解 .7z）。
$sevenZipCmd = Get-Command 7z -ErrorAction SilentlyContinue
$sevenZip = if ($sevenZipCmd) { $sevenZipCmd.Source } else { $null }
if (-not $sevenZip) {
    $sevenZip = Join-Path $work '7zr.exe'
    if (-not (Test-Path $sevenZip)) {
        Write-Host '[swgl] Fetching 7zr.exe ...'
        Invoke-WebRequest -UseBasicParsing -Headers $headers `
            -Uri 'https://www.7-zip.org/a/7zr.exe' -OutFile $sevenZip
    }
}

$outDir = Join-Path $work 'extract'
if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir }
Write-Host '[swgl] Extracting x64 DLLs ...'
& $sevenZip e $archive 'x64\opengl32.dll' 'x64\libgallium_wgl.dll' 'x64\dxil.dll' "-o$outDir" -y | Out-Null

foreach ($name in @('opengl32.dll', 'libgallium_wgl.dll', 'dxil.dll')) {
    $src = Join-Path $outDir $name
    if (Test-Path $src) {
        Copy-Item -Force $src (Join-Path $BinDir $name)
        Write-Host "[swgl] -> $name"
    }
}

Write-Host "[swgl] Done. Software OpenGL (llvmpipe) deployed to $BinDir."
Write-Host '[swgl] Run e.g.: $env:GALLIUM_DRIVER="llvmpipe"; ./bin/dsengine-editor.exe'
