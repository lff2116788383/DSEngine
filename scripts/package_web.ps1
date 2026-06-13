<#
.SYNOPSIS
    把一次 emscripten 构建的 Web 产物打包成可上传(itch.io 等)的 zip。
.DESCRIPTION
    收集 index.html / index.js / index.wasm（必需）以及 index.data / index.wasm.map
    （可选）到 -Out 目录，并压缩成 -Zip。与 `dse dist --target web` 同源：CLI 负责
    收集，本脚本额外做 zip（CI 无需依赖已构建的 dse.exe）。
    先用 web-release / web-debug 预设构建 dse_web_host，产物默认落在仓库 bin/。
.PARAMETER In
    emscripten 产物目录，默认 <repo>/bin。
.PARAMETER Out
    收集输出目录，默认 <repo>/dist/web。
.PARAMETER Zip
    输出 zip 路径，默认 <repo>/dist/DSEngine_web.zip。空字符串则跳过压缩。
.EXAMPLE
    # 构建后打包
    cmake --preset web-release; cmake --build --preset web-release
    powershell -ExecutionPolicy Bypass -File scripts\package_web.ps1
#>
[CmdletBinding()]
param(
    [string]$In = "",
    [string]$Out = "",
    [string]$Zip = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $In)  { $In  = Join-Path $RepoRoot "bin" }
if (-not $Out) { $Out = Join-Path $RepoRoot "dist/web" }
if ($null -eq $Zip) { $Zip = "" }
elseif (-not $Zip) { $Zip = Join-Path $RepoRoot "dist/DSEngine_web.zip" }

$Required = @("index.html", "index.js", "index.wasm")
$Optional = @("index.data", "index.wasm.map")

if (-not (Test-Path $In -PathType Container)) {
    throw "输入目录不存在: $In"
}

$missing = @()
foreach ($f in $Required) {
    if (-not (Test-Path (Join-Path $In $f) -PathType Leaf)) { $missing += $f }
}
if ($missing.Count -gt 0) {
    throw "缺少 Web 产物 ($($missing -join ', '))；请先用 web-release/web-debug 预设构建 dse_web_host。"
}

New-Item -ItemType Directory -Force -Path $Out | Out-Null

$collected = @()
$total = 0
foreach ($f in ($Required + $Optional)) {
    $src = Join-Path $In $f
    if (Test-Path $src -PathType Leaf) {
        Copy-Item $src (Join-Path $Out $f) -Force
        $collected += $f
        $total += (Get-Item $src).Length
    }
}

Write-Host "已收集 $($collected.Count) 个 Web 产物 -> $Out  ($total bytes)"
foreach ($f in $collected) { Write-Host "  + $f" }

if ($Zip) {
    $zipParent = Split-Path -Parent $Zip
    if ($zipParent) { New-Item -ItemType Directory -Force -Path $zipParent | Out-Null }
    if (Test-Path $Zip) { Remove-Item $Zip -Force }
    Compress-Archive -Path (Join-Path $Out "*") -DestinationPath $Zip -Force
    Write-Host "已压缩 -> $Zip  ($((Get-Item $Zip).Length) bytes)，可直接上传 itch.io。"
}
