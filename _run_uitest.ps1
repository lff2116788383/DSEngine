param([string]$Filter = "")
$env:GALLIUM_DRIVER = "llvmpipe"
$exe = "C:\Users\Administrator\repos\DSEngine\bin\dsengine-editor-uitest.exe"
$args = @("--headless")
if ($Filter -ne "") { $args += "--run-ui-tests=$Filter" } else { $args += "--run-ui-tests" }
$p = Start-Process -FilePath $exe -ArgumentList $args -PassThru -NoNewWindow
$p.WaitForExit(600000) | Out-Null
Write-Output ("EXIT=" + $p.ExitCode)
Write-Output "----- ui_test_summary.txt -----"
Get-Content "C:\Users\Administrator\repos\DSEngine\bin\ui_test_summary.txt"
$xml = "C:\Users\Administrator\repos\DSEngine\bin\ui_test_results.xml"
if (Test-Path $xml) {
    $fails = Select-String -Path $xml -Pattern "<failure message"
    if ($fails) { Write-Output "----- failures -----"; $fails | ForEach-Object { $_.Line } }
}
