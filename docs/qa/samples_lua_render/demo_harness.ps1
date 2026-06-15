param(
    [string]$Backend = 'opengl',
    [int]$Frames = 70,
    [int]$ShotFrame = 55,
    [int]$TimeoutSec = 90,
    [string]$OutRoot = 'C:\Users\Administrator\demo_runs'
)

$ErrorActionPreference = 'Continue'
$repo = 'C:\Users\Administrator\repos\DSEngine'
$exe  = Join-Path $repo 'bin\DSEngine_Game_release.exe'
$script = Join-Path $repo 'samples\lua\main.lua'

# 收集 demo 名称
$demos = New-Object System.Collections.Generic.List[string]
$demos.Add('phase1_2d_showcase')
$demos.Add('phase1_2d_physics_showcase')
Get-ChildItem (Join-Path $repo 'samples\lua\3d') -Filter *.lua -File | Sort-Object Name | ForEach-Object {
    $base = [IO.Path]::GetFileNameWithoutExtension($_.Name)
    if ($base -eq 'triangle' -or $base -eq 'square' -or $base -eq 'cube') { $demos.Add("3d_$base") }
    elseif ($base -like '3d_*') { $demos.Add($base) }
}

$outDir = Join-Path $OutRoot $Backend
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$logDir = Join-Path $outDir 'logs'
$shotDir = Join-Path $outDir 'shots'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null

$summary = New-Object System.Collections.Generic.List[object]
$idx = 0
foreach ($d in $demos) {
    $idx++
    $log  = Join-Path $logDir  "$d.log"
    $shot = Join-Path $shotDir "$d.png"
    Write-Host ("[{0}/{1}] {2} ({3})" -f $idx, $demos.Count, $d, $Backend)

    $env:GALLIUM_DRIVER = 'llvmpipe'
    if ($Backend -eq 'vulkan') {
        $icd = Join-Path $repo 'bin\lvp_icd.x86_64.json'
        $env:VK_ICD_FILENAMES = $icd
        $env:VK_DRIVER_FILES  = $icd
    }
    $env:DSE_MAX_FRAMES = "$Frames"
    $env:DSE_SCREENSHOT_FRAME = "$ShotFrame"
    $env:DSE_SCREENSHOT_PATH = $shot
    $env:DSE_SCREENSHOT_TARGET = ''
    $env:DSE_RHI_BACKEND = $Backend

    $argList = @("--demo=$d", "--script=$script", "--rhi=$Backend")
    $p = Start-Process -FilePath $exe -ArgumentList $argList -WorkingDirectory $repo `
         -RedirectStandardOutput $log -RedirectStandardError "$log.err" `
         -NoNewWindow -PassThru
    $done = $p.WaitForExit($TimeoutSec * 1000)
    $timedout = $false
    if (-not $done) {
        $timedout = $true
        try { $p.Kill() } catch {}
        Start-Sleep -Milliseconds 500
    }
    $exit = if ($timedout) { 'TIMEOUT' } else { $p.ExitCode }

    # 合并 stderr 到 log
    if (Test-Path "$log.err") { Get-Content "$log.err" | Add-Content $log; Remove-Item "$log.err" -Force }

    $txt = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
    $shotOk = Test-Path $shot
    $errCount = ([regex]::Matches($txt, '\[ERROR\]')).Count
    $shaderFail = ([regex]::Matches($txt, 'compile failed|link failed')).Count
    $adapter = if ($txt -match 'adapter="([^"]+)"') { $matches[1] } else { '' }
    $awakeOk = $txt -match 'Awake|Setup|加载 demo'

    $summary.Add([pscustomobject]@{
        demo=$d; backend=$Backend; exit=$exit; shot=$shotOk;
        errors=$errCount; shaderFail=$shaderFail; adapter=$adapter
    })
}

$summary | Export-Csv -Path (Join-Path $outDir 'summary.csv') -NoTypeInformation -Encoding UTF8
$summary | Format-Table -AutoSize
Write-Host "=== DONE ${Backend}: $($demos.Count) demos ==="
