param(
    [Parameter(Mandatory = $true)]
    [string]$EditorExe,
    [string[]]$Backends = @("opengl", "d3d11", "vulkan"),
    [int]$Frames = 3
)

$ErrorActionPreference = "Stop"

$editor = (Resolve-Path $EditorExe).Path
$logRoot = Join-Path ([IO.Path]::GetTempPath()) "dsengine-editor-rhi-smoke"
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

$savedBackend = $env:DSE_RHI_BACKEND
$savedSplash = $env:DSE_SPLASH
$savedStdio = $env:DSE_HEADLESS_STDIO
$savedVisible = $env:DSE_HEADLESS_VISIBLE

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
            throw "Editor RHI smoke failed for '$backend' (exit $($process.ExitCode)).`n$details"
        }

        Write-Host "[editor-rhi-smoke] PASS $backend"
    }
} finally {
    $env:DSE_RHI_BACKEND = $savedBackend
    $env:DSE_SPLASH = $savedSplash
    $env:DSE_HEADLESS_STDIO = $savedStdio
    $env:DSE_HEADLESS_VISIBLE = $savedVisible
}
