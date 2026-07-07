<#
.SYNOPSIS
    DSEngine 远程隧道交互脚本 — 通过 cpolar 隧道操作远端工作区。
.DESCRIPTION
    封装隧道 API（ping / files / exec），简化远程文件浏览和命令执行。
    所有函数自动附带 Authorization header 和工作区目录。
.EXAMPLE
    . .\tunnel.ps1                        # 加载
    Tunnel-Ping                           # 测试连通性
    Tunnel-Ls "scripts"                   # 列目录
    Tunnel-Exec "git status"              # 执行命令
    Tunnel-Cat "CMakeLists.txt"           # 查看文件内容
    Tunnel-Exec "cmake --version" -Timeout 30  # 带超时(秒)
#>

# ── 配置 ───────────────────────────────────────────────────────────────────────
$script:TunnelUrl   = "http://223e5758.r6.cpolar.cn"
$script:TunnelToken = "Bearer b53e3a39857c03e48413d774c20a4c2e"
$script:Workspace   = "c:\Users\Administrator\Desktop\Engine\DSEngine"

function script:Headers {
    @{ "Authorization" = $script:TunnelToken }
}

# ── Tunnel-Ping : 测试连通性 ───────────────────────────────────────────────────
function Tunnel-Ping {
    try {
        $r = Invoke-RestMethod -Uri "$($script:TunnelUrl)/api/ping" -Headers (Headers) -TimeoutSec 10
        Write-Host "[Tunnel] OK  workspace=$($r.workspace)  readonly=$($r.readonly)  exec=$($r.exec)" -ForegroundColor Green
        return $r
    } catch {
        Write-Host "[Tunnel] FAIL: $_" -ForegroundColor Red
        return $null
    }
}

# ── Tunnel-Ls : 列目录 ────────────────────────────────────────────────────────
function Tunnel-Ls {
    param([string]$Path = ".")
    $uri = "$($script:TunnelUrl)/api/files?path=$([System.Uri]::EscapeDataString($Path))"
    $r = Invoke-RestMethod -Uri $uri -Headers (Headers) -TimeoutSec 15
    $r.entries | Format-Table -Property type, @{L='size';E={$_.size}}, name -AutoSize
    return $r.entries
}

# ── Tunnel-Exec : 执行远端命令 ─────────────────────────────────────────────────
function Tunnel-Exec {
    param(
        [Parameter(Mandatory)][string]$Command,
        [string]$Cwd = $script:Workspace,
        [int]$Timeout = 300
    )
    $body = @{
        cmd     = $Command
        cwd     = $Cwd
        timeout = $Timeout * 1000   # API 接受毫秒
    } | ConvertTo-Json

    $r = Invoke-RestMethod -Uri "$($script:TunnelUrl)/api/exec" `
         -Method Post -Headers (Headers) `
         -Body $body -ContentType "application/json" `
         -TimeoutSec ($Timeout + 30)

    # 输出
    if ($r.stdout) { Write-Host $r.stdout }
    if ($r.stderr) { Write-Host $r.stderr -ForegroundColor Yellow }

    if (-not $r.ok) {
        Write-Host "[Tunnel-Exec] FAILED (exit=$($r.exitCode))" -ForegroundColor Red
    }
    return $r
}

# ── Tunnel-Cat : 读取远端文件内容 ──────────────────────────────────────────────
function Tunnel-Cat {
    param(
        [Parameter(Mandatory)][string]$FilePath
    )
    $r = Tunnel-Exec -Command "powershell -Command `"[Console]::OutputEncoding=[System.Text.Encoding]::UTF8; Get-Content '$FilePath' -Raw -Encoding UTF8`""
    return $r.stdout
}

# ── Tunnel-Upload : 将本地文件内容写入远端 ─────────────────────────────────────
function Tunnel-Upload {
    param(
        [Parameter(Mandatory)][string]$LocalPath,
        [Parameter(Mandatory)][string]$RemotePath
    )
    $bytes = [System.IO.File]::ReadAllBytes($LocalPath)
    $b64 = [Convert]::ToBase64String($bytes)

    # 大文件分块写入（每块 ~6000 字符避免命令行过长）
    $chunkSize = 6000
    $tempB64 = "$RemotePath.b64tmp"
    for ($i = 0; $i -lt $b64.Length; $i += $chunkSize) {
        $end = [Math]::Min($i + $chunkSize, $b64.Length)
        $chunk = $b64.Substring($i, $end - $i)
        if ($i -eq 0) {
            $op = ">"
        } else {
            $op = ">>"
        }
        $null = Tunnel-Exec -Command "cmd /c `"echo|set /p=`"$chunk`" $op `"$tempB64`"`""
    }
    # 解码 base64 → 目标文件
    $cmd = "powershell -Command `"[IO.File]::WriteAllBytes('$RemotePath', [Convert]::FromBase64String((Get-Content '$tempB64' -Raw))); Remove-Item '$tempB64' -ErrorAction SilentlyContinue; Write-Output 'OK'`""
    $r = Tunnel-Exec -Command $cmd
    return $r
}

# ── Tunnel-Patch : 将本地 git 补丁应用到远端 ────────────────────────────────────
function Tunnel-Patch {
    param(
        [Parameter(Mandatory)][string]$LocalRepoPath,
        [string]$CommitRange = "HEAD~1..HEAD"
    )
    # 生成补丁
    $patchFile = [IO.Path]::GetTempFileName() + ".patch"
    Push-Location $LocalRepoPath
    git format-patch $CommitRange --stdout > $patchFile
    Pop-Location
    # 上传并应用
    $remotePatch = "C:\temp_devin_patch.patch"
    Tunnel-Upload -LocalPath $patchFile -RemotePath $remotePatch
    $r = Tunnel-Exec -Command "cd $($script:Workspace) && git am `"$remotePatch`" && del `"$remotePatch`""
    Remove-Item $patchFile -ErrorAction SilentlyContinue
    return $r
}

# ── Tunnel-Git : 在工作区执行 git 命令 ─────────────────────────────────────────
function Tunnel-Git {
    param([Parameter(Mandatory)][string]$Args_)
    return Tunnel-Exec -Command "git $Args_"
}

Write-Host "`n[tunnel.ps1] Loaded. Functions: Tunnel-Ping, Tunnel-Ls, Tunnel-Exec, Tunnel-Cat, Tunnel-Upload, Tunnel-Patch, Tunnel-Git" -ForegroundColor Cyan
Write-Host "[tunnel.ps1] Workspace: $($script:Workspace)`n" -ForegroundColor Cyan
