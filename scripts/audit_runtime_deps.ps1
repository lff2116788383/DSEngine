<#
.SYNOPSIS
    审计一个待发布目录的运行时依赖：揪出误带的 debug/dev DLL，并（可选）确认
    必需的 VC++ 运行时 DLL 齐备。
.DESCRIPTION
    发布包绝不能包含调试版 CRT（*140d.dll / ucrtbased.dll，微软不允许再分发），
    也不应混入 _debug 后缀的引擎/运行时产物。本脚本对给定目录做文件名审计：

      - 命中调试 CRT 黑名单            -> 失败（exit 1）
      - 命中 _debug 后缀的 .dll/.exe   -> 失败（exit 1）
      - -RequireRuntime 时缺少必需 CRT -> 失败（exit 1）

    若环境里有 dumpbin.exe，则额外对每个 .exe/.dll 做导入表扫描，命中调试 CRT
    导入即失败（更强的保证；缺少 dumpbin 时自动跳过这步）。
.PARAMETER Dir
    待审计目录（递归扫描）。
.PARAMETER RequireRuntime
    要求 vcruntime140.dll / vcruntime140_1.dll / msvcp140.dll 必须存在。
.EXAMPLE
    pwsh scripts/audit_runtime_deps.ps1 -Dir build_sdk_release\install -RequireRuntime
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Dir,
    [switch]$RequireRuntime
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Dir)) { Write-Host "审计目录不存在: $Dir" -ForegroundColor Red; exit 1 }

# 绝不可再分发的调试版 CRT（精确名单，避免误伤 glad.dll 之类以 d 结尾的合法名）。
$forbiddenDebugCrt = @(
    "vcruntime140d.dll", "vcruntime140_1d.dll",
    "msvcp140d.dll", "msvcp140_1d.dll", "msvcp140_2d.dll",
    "msvcp140_codecvt_idsd.dll", "msvcp140_atomic_waitd.dll",
    "concrt140d.dll", "vccorlib140d.dll", "ucrtbased.dll"
)
# 发布包必备的 Release CRT。
$requiredRuntime = @("vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll")

$violations = @()
$warnings   = @()

$binaries = Get-ChildItem -LiteralPath $Dir -Recurse -File -Include *.dll, *.exe -ErrorAction SilentlyContinue
$names    = $binaries | ForEach-Object { $_.Name }
$namesLc  = $names | ForEach-Object { $_.ToLower() }

# 1) 调试版 CRT 黑名单
foreach ($b in $binaries) {
    if ($forbiddenDebugCrt -contains $b.Name.ToLower()) {
        $violations += "调试版 CRT 不可发行: $($b.FullName)"
    }
}

# 2) _debug 后缀的引擎/运行时产物（不应进入 release 包）
foreach ($b in $binaries) {
    if ($b.BaseName.ToLower().EndsWith("_debug")) {
        $violations += "调试产物混入发布包: $($b.FullName)"
    }
}

# 3) 必需运行时
if ($RequireRuntime) {
    foreach ($r in $requiredRuntime) {
        if ($namesLc -notcontains $r.ToLower()) {
            $violations += "缺少必需运行时 DLL: $r（未在 $Dir 中找到）"
        }
    }
}

# 4) 可选：dumpbin 导入表扫描（命中调试 CRT 导入即失败）
$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if ($dumpbin) {
    foreach ($b in $binaries) {
        $imports = & $dumpbin.Source /DEPENDENTS $b.FullName 2>$null
        foreach ($dbg in $forbiddenDebugCrt) {
            if ($imports -match [Regex]::Escape($dbg)) {
                $violations += "$($b.Name) 导入调试 CRT: $dbg"
            }
        }
    }
} else {
    $warnings += "未找到 dumpbin.exe，跳过导入表扫描（已做文件名审计）。"
}

Write-Host "依赖审计: $Dir"
Write-Host "  扫描二进制: $($binaries.Count)"
foreach ($w in $warnings) { Write-Host "  [warn] $w" -ForegroundColor Yellow }

if ($violations.Count -gt 0) {
    Write-Host "  审计未通过，发现 $($violations.Count) 项问题:" -ForegroundColor Red
    foreach ($v in $violations) { Write-Host "   - $v" -ForegroundColor Red }
    exit 1
}

Write-Host "  审计通过：无调试/dev DLL$(if ($RequireRuntime) { '，必需运行时齐备' })。" -ForegroundColor Green
exit 0
