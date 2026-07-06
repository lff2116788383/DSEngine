param([string]$Filter = "")
$env:GALLIUM_DRIVER = "llvmpipe"
$root = $PSScriptRoot
$exe = Join-Path $root "bin\dsengine-editor-uitest.exe"
$runArgs = @("--headless")
if ($Filter -ne "") { $runArgs += "--run-ui-tests=$Filter" } else { $runArgs += "--run-ui-tests" }
$p = Start-Process -FilePath $exe -ArgumentList $runArgs -PassThru -NoNewWindow
$p.WaitForExit(600000) | Out-Null
Write-Output ("EXIT=" + $p.ExitCode)
Write-Output "----- ui_test_summary.txt -----"
Get-Content (Join-Path $root "bin\ui_test_summary.txt")
$xml = Join-Path $root "bin\ui_test_results.xml"
if (Test-Path $xml) {
    $fails = Select-String -Path $xml -Pattern "<failure message"
    if ($fails) { Write-Output "----- failures -----"; $fails | ForEach-Object { $_.Line } }
}
