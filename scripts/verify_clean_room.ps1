<#
.SYNOPSIS
    干净机（clean-room）发布验证：在一台没有 Visual Studio / 没有 Vulkan SDK 的
    普通 Windows 上，验证打包产物能否开箱即跑。
.DESCRIPTION
    CI 的构建/打包都在装了 VS 的开发机上跑，无法暴露“漏带运行时 DLL / 漏带 license /
    依赖只在开发机存在”这类问题。本脚本设计为在 **干净目标机** 上对最终发布目录执行：

      1. 环境体检：报告本机是否装了 VC++ Redist / Vulkan SDK / Vulkan runtime
         （若装了 VC++ Redist 或 VS，说明这不是真正的干净机，结果不可全信 -> 警告）。
      2. 静态审计：复用 audit_runtime_deps.ps1（禁止 debug CRT、要求 Release CRT 齐全）。
      3. 许可证合规：检查发布目录内存在 THIRD_PARTY_LICENSES.md。
      4. 动态依赖检查：对每个 .exe/.dll 用 dumpbin /DEPENDENTS 解析导入表，
         确认每个被导入的 DLL 要么 app-local（在发布目录内）、要么是已知系统 DLL；
         任何无法解析的导入 -> 失败（这正是干净机会崩的原因）。
      5. 启动冒烟：可选地启动游戏 exe，超时后结束；专门识别
         退出码 0xC0000135 (STATUS_DLL_NOT_FOUND) —— 即“缺 DLL 打不开”。

    任意一步失败 -> 退出码 1，便于接入 CI（在真正的 clean runner 上）或人工 gating。
.PARAMETER Dir
    待验证的发布目录（SDK install 目录 / dse build 导出的 dist 目录）。
.PARAMETER GameExe
    （可选）要做启动冒烟的可执行文件名或路径（默认在 Dir 内自动探测 *.exe）。
.PARAMETER SmokeSeconds
    启动冒烟的等待秒数（默认 6）。设为 0 跳过启动冒烟，仅做静态+依赖检查。
.EXAMPLE
    pwsh scripts/verify_clean_room.ps1 -Dir D:\dist\MyGame
    pwsh scripts/verify_clean_room.ps1 -Dir build_sdk_release\install -SmokeSeconds 0
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Dir,
    [string]$GameExe = "",
    [int]$SmokeSeconds = 6,
    # 把结果写成机器可读 JSON（含环境判定与逐步结果），供门禁/审计消费。
    [string]$Json = "",
    # 强制要求「本机确实是干净机」。默认关闭时，若本机装了 VS/VC++ Redist/VULKAN_SDK，
    # 只出警告；打开后同样的条件直接判失败 —— 发布门禁必须用这个开关跑，
    # 否则开发机上的「通过」会被误当成干净机验收结论。
    [switch]$RequireCleanHost
)

$ErrorActionPreference = "Stop"
$fail = @()
$warn = @()

function Test-RegLike([string]$pattern) {
    $roots = @(
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*"
    )
    foreach ($r in $roots) {
        $hit = Get-ItemProperty $r -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -like $pattern }
        if ($hit) { return $true }
    }
    return $false
}

if (-not (Test-Path $Dir)) { Write-Host "[fail] 目录不存在: $Dir" -ForegroundColor Red; exit 1 }
$Dir = (Resolve-Path $Dir).Path
$scriptDir = $PSScriptRoot

Write-Host "==== 1) 环境体检（确认这是干净机）===="
$hasRedist = Test-RegLike "*Visual C++ 2015-2022 Redistributable*"
$hasVS     = (Test-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe") -or
             (Test-Path "$env:ProgramFiles\Microsoft Visual Studio")
$hasVkSdk  = [bool]$env:VULKAN_SDK
$hasVkRt   = Test-Path (Join-Path $env:WINDIR "System32\vulkan-1.dll")
Write-Host ("  VC++ 2015-2022 Redistributable 已安装: {0}" -f $hasRedist)
Write-Host ("  Visual Studio 存在:                    {0}" -f $hasVS)
Write-Host ("  VULKAN_SDK 环境变量:                   {0}" -f $hasVkSdk)
Write-Host ("  系统 vulkan-1.dll 存在:                {0}" -f $hasVkRt)
if ($hasRedist -or $hasVS) {
$hostContaminated = @()
if ($hasRedist) { $hostContaminated += "VC++ 2015-2022 Redistributable" }
if ($hasVS)     { $hostContaminated += "Visual Studio" }
if ($hasVkSdk)  { $hostContaminated += "VULKAN_SDK 环境变量" }
$cleanHost = ($hostContaminated.Count -eq 0)
Write-Host ("  干净机判定: {0}" -f $(if ($cleanHost) { "是（未发现开发工具痕迹）" } else { "否（" + ($hostContaminated -join ", ") + "）" }))
if (-not $cleanHost) {
    $msg = "本机不是干净机（存在: " + ($hostContaminated -join ", ") + "）—— CRT/DLL 缺失类问题可能被本机已装的运行时掩盖，结论不可作为发布验收。"
    if ($RequireCleanHost) {
        $fail += $msg
    } else {
        $warn += $msg + " 发布门禁请加 -RequireCleanHost 跑，让该条件直接判失败。"
    }
}
}

Write-Host ""
Write-Host "==== 2) 静态审计 (audit_runtime_deps.ps1) ===="
$auditScript = Join-Path $scriptDir "audit_runtime_deps.ps1"
if (Test-Path $auditScript) {
    & $auditScript -Dir $Dir -RequireRuntime
    if ($LASTEXITCODE -ne 0) { $fail += "静态审计未通过（debug CRT / 缺 Release CRT）。" }
} else {
    $warn += "未找到 audit_runtime_deps.ps1，跳过静态审计。"
}

Write-Host ""
Write-Host "==== 3) 许可证合规 ===="
if (Test-Path (Join-Path $Dir "THIRD_PARTY_LICENSES.md")) {
    Write-Host "  [ok] THIRD_PARTY_LICENSES.md 存在"
} else {
    $fail += "发布目录缺少 THIRD_PARTY_LICENSES.md（第三方授权聚合）。"
}

Write-Host ""
Write-Host "==== 4) 动态依赖检查 (dumpbin /DEPENDENTS) ===="
# 已知由系统提供、可安全依赖的 DLL（Win10+ / UCRT / 显卡驱动）。
$systemDlls = @(
    "kernel32.dll","user32.dll","gdi32.dll","gdi32full.dll","shell32.dll","ole32.dll","oleaut32.dll",
    "advapi32.dll","ws2_32.dll","winmm.dll","setupapi.dll","cfgmgr32.dll","comdlg32.dll","comctl32.dll",
    "shlwapi.dll","version.dll","dbghelp.dll","bcrypt.dll","crypt32.dll","secur32.dll","userenv.dll",
    "imm32.dll","dwmapi.dll","uxtheme.dll","powrprof.dll","propsys.dll","rpcrt4.dll","sechost.dll",
    "ntdll.dll","msvcrt.dll","combase.dll","hid.dll","wintrust.dll","iphlpapi.dll","dnsapi.dll",
    "opengl32.dll","glu32.dll","gdiplus.dll","d3d11.dll","dxgi.dll","d3dcompiler_47.dll","dcomp.dll",
    "vulkan-1.dll","xinput1_4.dll","xinput9_1_0.dll","avrt.dll","mmdevapi.dll","audioses.dll",
    "kernelbase.dll","msvcp_win.dll","ucrtbase.dll","win32u.dll","ntmarta.dll","windows.storage.dll"
)
$ucrtPrefix = "api-ms-win-"  # Win10 起随系统提供
$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
$binaries = Get-ChildItem -LiteralPath $Dir -Recurse -File -Include *.dll,*.exe -ErrorAction SilentlyContinue
$localLc  = ($binaries | ForEach-Object { $_.Name.ToLower() })
if ($dumpbin) {
    $unresolved = @{}
    foreach ($b in $binaries) {
        $deps = & $dumpbin.Source /DEPENDENTS $b.FullName 2>$null |
            Select-String -Pattern '^\s{4,}([A-Za-z0-9_\-\.]+\.dll)\s*$' |
            ForEach-Object { $_.Matches[0].Groups[1].Value.ToLower() }
        foreach ($d in $deps) {
            if ($localLc -contains $d) { continue }                 # app-local
            if ($systemDlls -contains $d) { continue }              # 系统 DLL
            if ($d.StartsWith($ucrtPrefix)) { continue }            # UCRT
            if (Test-Path (Join-Path $env:WINDIR "System32\$d")) { $warn += "$($b.Name) 依赖 $d —— 本机 System32 有, 干净机未必有, 请确认。"; continue }
            if (-not $unresolved.ContainsKey($d)) { $unresolved[$d] = @() }
            $unresolved[$d] += $b.Name
        }
    }
    if ($unresolved.Count -gt 0) {
        foreach ($d in $unresolved.Keys) {
            $fail += "无法解析的依赖 DLL: $d（被 $($unresolved[$d] -join ', ') 导入）—— 干净机上会因缺 DLL 而启动失败。"
        }
    } else {
        Write-Host "  [ok] 所有导入的 DLL 均可由 app-local / 系统 / UCRT 解析"
    }
} else {
    $warn += "未找到 dumpbin.exe（VS 自带），跳过动态依赖检查。可在装了 VS 的机器上预先跑一遍此项。"
}

Write-Host ""
Write-Host "==== 5) 启动冒烟 ===="
if ($SmokeSeconds -gt 0) {
    $exe = $GameExe
    if (-not $exe) {
        $cand = $binaries | Where-Object { $_.Extension -eq ".exe" -and $_.BaseName -notmatch "(?i)test|tool|cli|dse$" } | Select-Object -First 1
        if ($cand) { $exe = $cand.FullName }
    } elseif (-not (Test-Path $exe)) {
        $exe = Join-Path $Dir $exe
    }
    if ($exe -and (Test-Path $exe)) {
        Write-Host "  启动: $exe（最多等 $SmokeSeconds 秒）"
        try {
            $p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -PassThru
            $exited = $p.WaitForExit($SmokeSeconds * 1000)
            if (-not $exited) {
                Write-Host "  [ok] 进程已持续运行 $SmokeSeconds 秒未崩溃（视为启动成功），结束之。"
                try { $p.Kill() } catch {}
            } else {
                $code = $p.ExitCode
                if ($code -eq -1073741515) {  # 0xC0000135 STATUS_DLL_NOT_FOUND
                    $fail += "启动失败：缺少 DLL (STATUS_DLL_NOT_FOUND 0xC0000135) —— 这正是干净机缺运行时的症状。"
                } elseif ($code -ne 0) {
                    $warn += "进程在 $SmokeSeconds 秒内退出，退出码 $code（可能是正常的快速退出，也可能是初始化失败，请人工确认日志）。"
                } else {
                    Write-Host "  [ok] 进程正常退出 (0)。"
                }
            }
        } catch {
            $fail += "无法启动 ${exe}: $($_.Exception.Message)"
        }
    } else {
        $warn += "未找到可启动的游戏 exe，跳过启动冒烟（可用 -GameExe 指定）。"
    }
} else {
    Write-Host "  （SmokeSeconds=0，跳过）"
}

Write-Host ""
Write-Host "==== 结果 ===="
foreach ($w in $warn) { Write-Host "  [warn] $w" -ForegroundColor Yellow }

if ($Json) {
    $verdict = if ($fail.Count -gt 0) { "fail" } elseif (-not $cleanHost) { "pass_on_dirty_host" } else { "pass" }
    $report = [ordered]@{
        schemaVersion     = 1
        dir               = $Dir
        host              = [ordered]@{
            clean                    = $cleanHost
            contaminants             = $hostContaminated
            vcRedistInstalled        = $hasRedist
            visualStudioInstalled    = $hasVS
            vulkanSdkEnvSet          = $hasVkSdk
            systemVulkanLoaderPresent = $hasVkRt
        }
        requireCleanHost  = [bool]$RequireCleanHost
        smokeSeconds      = $SmokeSeconds
        verdict           = $verdict
        failures          = @($fail)
        warnings          = @($warn)
    }
    $report | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -Path $Json
    Write-Host "  机器可读报告: $Json (verdict=$verdict)" -ForegroundColor Cyan
}
if ($fail.Count -gt 0) {
    Write-Host "  干净机验证未通过，共 $($fail.Count) 项：" -ForegroundColor Red
    foreach ($f in $fail) { Write-Host "   - $f" -ForegroundColor Red }
    exit 1
}
if (-not $cleanHost) {
    Write-Host "  检查项通过，但本机不是干净机 -> 结论不可作为发布验收（verdict=pass_on_dirty_host）。" -ForegroundColor Yellow
} else {
    Write-Host "  干净机验证通过。" -ForegroundColor Green
}
exit 0
