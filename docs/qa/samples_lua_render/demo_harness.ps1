param(
    [string]$Backend = 'opengl',
    [int]$Frames = 70,
    [int]$ShotFrame = 55,
    [int]$TimeoutSec = 90,
    [int]$SettleSec = 5,        # GPU settle between demos (avoids transient device-lost on real HW)
    [int]$MaxRetries = 2,       # retry a demo if a transient device-lost (vkQueueSubmit) is detected
    [string]$OutRoot = 'C:\Users\Administrator\demo_runs_real'
)

$ErrorActionPreference = 'Continue'
$repo = 'C:\Users\Administrator\Desktop\Engine\DSEngine'
$exe  = Join-Path $repo 'bin\DSEngine_Game_relwithdebinfo.exe'
$script = Join-Path $repo 'samples\lua\main.lua'

# Enumerate demos: 2 top-level 2D + all 3d/*.lua
$demos = New-Object System.Collections.Generic.List[string]
$demos.Add('phase1_2d_showcase')
$demos.Add('phase1_2d_physics_showcase')
Get-ChildItem (Join-Path $repo 'samples\lua\3d') -Filter *.lua -File | Sort-Object Name | ForEach-Object {
    $base = [IO.Path]::GetFileNameWithoutExtension($_.Name)
    if ($base -eq 'triangle' -or $base -eq 'square' -or $base -eq 'cube') { $demos.Add("3d_$base") }
    elseif ($base -like '3d_*') { $demos.Add($base) }
}

$outDir = Join-Path $OutRoot $Backend
$logDir = Join-Path $outDir 'logs'
$shotDir = Join-Path $outDir 'shots'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null

# Real hardware: ensure NO software-driver overrides leak in
Remove-Item Env:VK_ICD_FILENAMES -ErrorAction SilentlyContinue
Remove-Item Env:VK_DRIVER_FILES -ErrorAction SilentlyContinue
Remove-Item Env:GALLIUM_DRIVER -ErrorAction SilentlyContinue

function Invoke-Demo {
    param([string]$d, [string]$log, [string]$shot)
    if (Test-Path $shot) { Remove-Item $shot -Force }
    $env:DSE_MAX_FRAMES = "$Frames"
    $env:DSE_SCREENSHOT_FRAME = "$ShotFrame"
    $env:DSE_SCREENSHOT_PATH = $shot
    $env:DSE_SCREENSHOT_TARGET = ''
    $env:DSE_RHI_BACKEND = $Backend
    $argList = @("--demo=$d", "--script=$script", "--rhi=$Backend")
    $p = Start-Process -FilePath $exe -ArgumentList $argList -WorkingDirectory $repo `
         -RedirectStandardOutput $log -RedirectStandardError "$log.err" -NoNewWindow -PassThru
    $done = $p.WaitForExit($TimeoutSec * 1000)
    $timedout = $false
    if (-not $done) { $timedout = $true; try { $p.Kill() } catch {}; Start-Sleep -Milliseconds 500 }
    if (Test-Path "$log.err") { Get-Content "$log.err" | Add-Content $log; Remove-Item "$log.err" -Force }
    $txt = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
    return [pscustomobject]@{
        exit       = if ($timedout) { 'TIMEOUT' } else { $p.ExitCode }
        txt        = $txt
        shotOk     = Test-Path $shot
        errors     = ([regex]::Matches($txt, '\[ERROR\]')).Count
        submitFail = ([regex]::Matches($txt, 'vkQueueSubmit failed')).Count
        uboOverflow= ([regex]::Matches($txt, 'UBO OVERFLOW')).Count
        shaderFail = ([regex]::Matches($txt, 'compile failed|link failed|Shader compile error')).Count
        adapter    = if ($txt -match 'adapter="([^"]+)"') { $matches[1] } else { '' }
        resolved   = if ($txt -match 'DSE_RENDER_DEVICE backend=(\S+)') { $matches[1] } else { '' }
        software   = if ($txt -match 'software=(\d)') { $matches[1] } else { '' }
    }
}

$summary = New-Object System.Collections.Generic.List[object]
$idx = 0
foreach ($d in $demos) {
    $idx++
    $log  = Join-Path $logDir  "$d.log"
    $shot = Join-Path $shotDir "$d.png"
    Write-Host ("[{0}/{1}] {2} ({3})" -f $idx, $demos.Count, $d, $Backend)

    Start-Sleep -Seconds $SettleSec
    $r = Invoke-Demo -d $d -log $log -shot $shot
    $retries = 0
    # Retry only on transient device-lost (vkQueueSubmit failures), not on script/asset errors.
    while ($r.submitFail -gt 0 -and $retries -lt $MaxRetries) {
        $retries++
        Write-Host ("    device-lost detected (submitFail={0}); retry {1}/{2} after longer settle" -f $r.submitFail, $retries, $MaxRetries)
        Start-Sleep -Seconds ($SettleSec * 2)
        $r = Invoke-Demo -d $d -log $log -shot $shot
    }

    $summary.Add([pscustomobject]@{
        demo=$d; backend=$Backend; reqResolved=$r.resolved; exit=$r.exit; shot=$r.shotOk;
        errors=$r.errors; submitFail=$r.submitFail; uboOverflow=$r.uboOverflow;
        shaderFail=$r.shaderFail; retries=$retries; software=$r.software; adapter=$r.adapter
    })
}

$summary | Export-Csv -Path (Join-Path $outDir 'summary.csv') -NoTypeInformation -Encoding UTF8
$summary | Format-Table demo,exit,shot,errors,submitFail,uboOverflow,shaderFail,retries -AutoSize
Write-Host "=== DONE ${Backend}: $($demos.Count) demos ==="
