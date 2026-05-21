# glad 导出机制回归验证脚本
# 用途：验证 exports.def 删除后能自动重建，dse_engine Debug 可重新链接成功

$ErrorActionPreference = "Stop"
$BuildDir = "build_vs2022"
$DefFile = "$BuildDir\dse_engine.dir\Debug\exports.def"
$GladObj = "$BuildDir\dse_engine.dir\Debug\glad.obj"

Write-Host "=== glad 导出机制回归验证 ===" -ForegroundColor Cyan

# 1. 清理 stale artifacts
Write-Host "[1/4] 清理 stale artifacts..." -ForegroundColor Yellow
if (Test-Path $DefFile) { Remove-Item $DefFile -Force }
if (Test-Path $GladObj) { Remove-Item $GladObj -Force }
Write-Host "  ✓ 已删除 exports.def 和 glad.obj"

# 2. 重新构建 dse_engine
Write-Host "[2/4] 重新构建 dse_engine Debug..." -ForegroundColor Yellow
$result = cmake --build $BuildDir --config Debug --target dse_engine -- /p:CL_MPCount=8 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ 构建失败" -ForegroundColor Red
    Write-Host $result
    exit 1
}
Write-Host "  ✓ dse_engine Debug 构建成功"

# 3. 验证 exports.def 已重建
Write-Host "[3/4] 验证 exports.def 已重建..." -ForegroundColor Yellow
if (!(Test-Path $DefFile)) {
    Write-Host "  ✗ exports.def 未重建" -ForegroundColor Red
    exit 1
}
$defContent = Get-Content $DefFile -Raw
if ($defContent -notmatch "glad_") {
    Write-Host "  ✗ exports.def 不包含 glad 符号" -ForegroundColor Red
    exit 1
}
Write-Host "  ✓ exports.def 已重建且包含 glad 符号"

# 4. 验证 objects.txt 存在
Write-Host "[4/4] 验证 objects.txt 存在..." -ForegroundColor Yellow
$ObjectsFile = "$BuildDir\dse_engine.dir\Debug\objects.txt"
if (!(Test-Path $ObjectsFile)) {
    Write-Host "  ✗ objects.txt 不存在" -ForegroundColor Red
    exit 1
}
Write-Host "  ✓ objects.txt 存在"

Write-Host "=== 所有检查通过 ===" -ForegroundColor Green
Write-Host "glad 导出机制工作正常" -ForegroundColor Green
