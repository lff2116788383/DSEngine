<#
.SYNOPSIS
    依次运行全部冒烟/回归套件。
.EXAMPLE
    .\tests\automation\scripts\run_all.ps1 `
        -EditorExe "C:\...\bin\dsengine-editor.exe" `
        -WorkRoot  "C:\dse-autotest"
#>
param(
    [string]$EditorExe = "",
    [string]$WorkRoot = "$env:TEMP\dse-auto",
    [switch]$IncludeNightly
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$Auto = Join-Path $RepoRoot "tests\automation\dse_auto.py"

if (-not $EditorExe) {
    $EditorExe = Join-Path $RepoRoot "bin\dsengine-editor.exe"
}
$env:EDITOR_EXE = $EditorExe
$env:WORK_ROOT = $WorkRoot
Write-Host "EDITOR_EXE = $EditorExe"
Write-Host "WORK_ROOT  = $WorkRoot"

$suites = @(
    @{ name = "editor-batch-smoke"; runId = "batch_smoke" },
    @{ name = "editor-api-smoke";   runId = "api_smoke" },
    @{ name = "editor-regression";  runId = "regression" }
)
if ($IncludeNightly) {
    $suites += @{ name = "editor-nightly"; runId = "nightly" }
}

$failed = 0
foreach ($s in $suites) {
    Write-Host "`n=== Running suite: $($s.name) ===" -ForegroundColor Cyan
    python $Auto run `
        --suite "$RepoRoot\tests\automation\suites\$($s.name).yaml" `
        --run-id $($s.runId)
    if ($LASTEXITCODE -ne 0) {
        Write-Host "suite $($s.name) FAILED (exit $LASTEXITCODE)" -ForegroundColor Red
        $failed++
    }
}

Write-Host "`n=== Done. failed suites = $failed ===" -ForegroundColor (if ($failed) { "Red" } else { "Green" })
exit $failed
