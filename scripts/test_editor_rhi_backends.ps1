param(
    [Parameter(Mandatory = $true)]
    [string]$EditorExe,
    [string[]]$Backends = @("opengl", "d3d11", "vulkan"),
    [int]$Frames = 3,
    # 机器可读结果（逐后端 + 总结），供门禁/审计消费。
    [string]$Json = ""
)

$ErrorActionPreference = "Stop"

$editor = (Resolve-Path $EditorExe).Path
$logRoot = Join-Path ([IO.Path]::GetTempPath()) "dsengine-editor-rhi-smoke"
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

$savedBackend = $env:DSE_RHI_BACKEND
$savedSplash = $env:DSE_SPLASH
$savedStdio = $env:DSE_HEADLESS_STDIO
$savedVisible = $env:DSE_HEADLESS_VISIBLE

$results = @()
$failures = @()
try {
    $env:DSE_SPLASH = "0"
    $env:DSE_HEADLESS_STDIO = "1"

    foreach ($backend in $Backends) {
        $env:DSE_RHI_BACKEND = $backend
        $env:DSE_HEADLESS_VISIBLE = if ($backend -eq "vulkan") { "1" } else { $savedVisible }
        $stdout = Join-Path $logRoot "$backend.stdout.log"
        $stderr = Join-Path $logRoot "$backend.stderr.log"
        $process = Start-Process -FilePath $editor `
            -ArgumentList "--headless", "--max-frames=$Frames" `
            -RedirectStandardOutput $stdout `
            -RedirectStandardError $stderr `
            -PassThru -Wait

        if ($process.ExitCode -ne 0) {
            $details = if (Test-Path $stderr) {
                (Get-Content $stderr -Tail 80) -join [Environment]::NewLine
            } else {
                "No stderr log was produced."
            }
            $results += [ordered]@{ backend = $backend; exitCode = $process.ExitCode; verdict = "fail"; stdout = $stdout; stderr = $stderr }
            $failures += "backend '$backend' exited $($process.ExitCode)"
            throw "Editor RHI smoke failed for '$backend' (exit $($process.ExitCode)).`n$details"
        }

        $results += [ordered]@{ backend = $backend; exitCode = 0; verdict = "pass"; stdout = $stdout; stderr = $stderr }
        Write-Host "[editor-rhi-smoke] PASS $backend"
    }
} finally {
    # 无论成败都写出结果：CI 需要这份 artifact 才能把"编辑器窗口/ImGui 交换链三后端可用"
    # 从"人工目测"变成可审计的门禁证据。
    if ($Json) {
        $verdict = if ($failures.Count -gt 0) { "fail" } else { "pass" }
        [ordered]@{
            schemaVersion = 1
            editorExe     = $editor
            frames        = $Frames
            requested     = $Backends
            verdict       = $verdict
            failures      = @($failures)
            results       = @($results)
        } | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -Path $Json
        Write-Host "[editor-rhi-smoke] wrote $Json (verdict=$verdict)"
    }
    $env:DSE_RHI_BACKEND = $savedBackend
    $env:DSE_SPLASH = $savedSplash
    $env:DSE_HEADLESS_STDIO = $savedStdio
    $env:DSE_HEADLESS_VISIBLE = $savedVisible
}
