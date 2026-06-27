# run_ui_tests.ps1 —— 一键跑编辑器真实 UI 控件级测试（内嵌 Dear ImGui Test Engine）。
#
# 前置：先以 -DDSE_EDITOR_UI_TESTS=ON 构建 dsengine-editor-uitest（与常规编辑器并存）：
#   cmake -S . -B build_uitest -DDSE_EDITOR_UI_TESTS=ON
#   cmake --build build_uitest --config Debug --target dse_editor_cpp
#
# 用法：
#   pwsh scripts/run_ui_tests.ps1                 # 跑全部 UI 用例
#   pwsh scripts/run_ui_tests.ps1 -Filter dse-panels   # 仅跑某组（test engine filter）
#
# 退出码：0=全过，非 0=有失败。结果摘要见 bin/ui_test_summary.txt，JUnit 见 bin/ui_test_results.xml。
[CmdletBinding()]
param(
    [string]$Filter = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exe  = Join-Path $root "bin\dsengine-editor-uitest.exe"

if (-not (Test-Path $exe)) {
    Write-Error "未找到 $exe；请先以 -DDSE_EDITOR_UI_TESTS=ON 构建 dse_editor_cpp。"
    exit 2
}

$runArg = if ($Filter) { "--run-ui-tests=$Filter" } else { "--run-ui-tests" }
& $exe "--headless" $runArg
$code = $LASTEXITCODE

$summary = Join-Path $root "bin\ui_test_summary.txt"
if (Test-Path $summary) {
    Write-Host "----- ui_test_summary.txt -----"
    Get-Content $summary
}

exit $code
