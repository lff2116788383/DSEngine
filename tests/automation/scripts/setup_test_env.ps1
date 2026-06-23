<#
.SYNOPSIS
    安装自动化测试框架所需 Python 依赖。
#>
$ErrorActionPreference = "Stop"
Write-Host "安装 dse_auto 依赖 ..." -ForegroundColor Cyan
python -m pip install --upgrade pip
python -m pip install PyYAML websocket-client psutil pillow numpy
Write-Host "完成。" -ForegroundColor Green
