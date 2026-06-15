<#
.SYNOPSIS
    汇总所有第三方依赖的许可证，生成单一 THIRD_PARTY_LICENSES.md。
.DESCRIPTION
    扫描 depends/ 与 third_party/ 下每个组件目录，定位其许可证文件
    (LICENSE / COPYING / NOTICE / COPYRIGHT / UNLICENSE ...)，把全文汇总进
    一个 Markdown 文件。发布 SDK / 工具包 / 导出游戏时必须随包附带该文件，
    以满足 MIT / BSD / zlib / ISC / FTL 等许可证的"保留版权与许可声明"义务。

    找不到许可证文件的组件会被单列出来，提示人工补充，绝不静默忽略。
.PARAMETER SourceDir
    仓库根目录。默认取脚本所在目录的上一级。
.PARAMETER OutFile
    输出文件路径。默认 <SourceDir>\THIRD_PARTY_LICENSES.md。
.EXAMPLE
    pwsh scripts/collect_third_party_licenses.ps1
    pwsh scripts/collect_third_party_licenses.ps1 -OutFile build\install\THIRD_PARTY_LICENSES.md
#>
[CmdletBinding()]
param(
    [string]$SourceDir = "",
    [string]$OutFile = ""
)

$ErrorActionPreference = "Stop"

if (-not $SourceDir) { $SourceDir = (Resolve-Path "$PSScriptRoot\..").Path }
if (-not $OutFile)   { $OutFile   = Join-Path $SourceDir "THIRD_PARTY_LICENSES.md" }

# 依赖根目录（相对仓库根）。
$depRoots = @("depends", "third_party")

# 许可证文件名候选（不区分大小写）。
$licensePatterns = @(
    "LICENSE*", "LICENCE*", "COPYING*", "COPYRIGHT*",
    "NOTICE*", "UNLICENSE*", "*LICENSE*", "*COPYING*"
)

# 头文件内嵌许可证的组件（无独立 LICENSE 文件）：从指定源文件抽取许可证文本。
#   File        : 源文件（相对仓库根）
#   FromMatch   : 起始标记（首次出现）
#   FromLastMatch: 起始标记（最后一次出现，用于许可证在文件末尾）
#   UntilMatch  : 终止标记（可选，遇到则停）
#   MaxLines    : 最多抽取行数
#   Spdx / Note : 元信息
$supplemental = [ordered]@{
    "depends/lua"          = @{ File = "depends\lua\lua.h";                    FromLastMatch = "Copyright (C)"; UntilMatch = "#endif"; MaxLines = 30; Spdx = "MIT" }
    "depends/miniaudio"    = @{ File = "depends\miniaudio\miniaudio.h";        FromMatch = "This software is available as a choice"; MaxLines = 140; Spdx = "Public Domain OR MIT-0" }
    "third_party/imguizmo" = @{ File = "third_party\imguizmo\ImGuizmo.h";      FromMatch = "The MIT License"; UntilMatch = "----"; MaxLines = 30; Spdx = "MIT" }
    "depends/glad"         = @{ File = "depends\glad\include\KHR\khrplatform.h"; FromMatch = "Copyright"; UntilMatch = "*/"; MaxLines = 40; Spdx = "MIT (glad generator) / Khronos"; Note = "OpenGL 加载器由 glad 0.1.36 生成（生成器本身 MIT，生成代码置于公有领域）。以下为其依赖的 Khronos khrplatform.h 许可证。" }
    "depends/glm_ext"      = @{ InheritFrom = "depends/glm"; Spdx = "MIT (GLM)"; Note = "项目本地的 GLM 扩展头文件，遵循 GLM (MIT) 许可证（见 depends/glm）。" }
}

$codeExt = @(".cpp", ".h", ".hpp", ".c", ".cc", ".cmake", ".py", ".sh", ".bat", ".rs", ".cs", ".java", ".js", ".ts")

function Find-LicenseFile([string]$componentDir) {
    # 在组件目录内（最多向下两层）匹配许可证文件，挑选层级最浅、文件名最短的一个。
    $candidates = @()
    foreach ($pat in $licensePatterns) {
        $candidates += Get-ChildItem -Path $componentDir -Filter $pat -File -Recurse -Depth 2 -ErrorAction SilentlyContinue |
            Where-Object { $_.Length -gt 0 -and $codeExt -notcontains $_.Extension }
    }
    if (-not $candidates) { return $null }
    $best = $candidates |
        Sort-Object @{ Expression = { ($_.FullName.Substring($componentDir.Length)).Split([char]'\').Count } },
                    @{ Expression = { $_.Name.Length } } |
        Select-Object -First 1
    return $best.FullName
}

function Get-RelPath([string]$full, [string]$base) {
    $b = $base.TrimEnd('\')
    if ($full.StartsWith($b, [System.StringComparison]::OrdinalIgnoreCase)) {
        return ($full.Substring($b.Length).TrimStart('\')) -replace '\\', '/'
    }
    return ($full -replace '\\', '/')
}

function Test-NonEmptyDir([string]$dir) {
    return [bool](Get-ChildItem -LiteralPath $dir -Recurse -File -Force -ErrorAction SilentlyContinue | Select-Object -First 1)
}

function Get-EmbeddedLicense($entry) {
    # 从源文件抽取内嵌许可证文本；失败返回 $null。
    if ($entry.Contains('InheritFrom')) { return $null }
    $f = Join-Path $SourceDir $entry.File
    if (-not (Test-Path $f)) { return $null }
    $lines = Get-Content -LiteralPath $f -ErrorAction SilentlyContinue
    if (-not $lines) { return $null }
    $startIdx = -1
    if ($entry.Contains('FromLastMatch')) {
        for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i].Contains($entry.FromLastMatch)) { $startIdx = $i } }
    } elseif ($entry.Contains('FromMatch')) {
        for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i].Contains($entry.FromMatch)) { $startIdx = $i; break } }
    }
    if ($startIdx -lt 0) { return $null }
    $max = if ($entry.Contains('MaxLines')) { [int]$entry.MaxLines } else { 60 }
    $endIdx = [Math]::Min($lines.Count - 1, $startIdx + $max - 1)
    if ($entry.Contains('UntilMatch')) {
        for ($i = $startIdx + 1; $i -le $endIdx; $i++) { if ($lines[$i].Contains($entry.UntilMatch)) { $endIdx = $i - 1; break } }
    }
    $block = $lines[$startIdx..$endIdx] | ForEach-Object {
        ($_ -replace '^\s*(//+|/\*+|\*+/|\*)\s?', '').TrimEnd()
    }
    return (($block -join "`n").Trim())
}

$components = @()
foreach ($root in $depRoots) {
    $rootPath = Join-Path $SourceDir $root
    if (-not (Test-Path $rootPath)) { continue }
    foreach ($dir in (Get-ChildItem -Path $rootPath -Directory -ErrorAction SilentlyContinue | Sort-Object Name)) {
        $name = "$root/$($dir.Name)"

        # 跳过未初始化的空 submodule 目录（不参与构建/发布）。
        if (-not (Test-NonEmptyDir $dir.FullName)) {
            $components += [PSCustomObject]@{ Name = $name; Dir = $dir.FullName; Status = "empty"; Source = ""; Spdx = ""; Note = ""; Text = "" }
            continue
        }

        $lic = Find-LicenseFile $dir.FullName
        if ($lic) {
            $text = Get-Content -LiteralPath $lic -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
            if (-not $text) { $text = Get-Content -LiteralPath $lic -Raw -ErrorAction SilentlyContinue }
            $components += [PSCustomObject]@{ Name = $name; Dir = $dir.FullName; Status = "file"; Source = (Get-RelPath $lic $SourceDir); Spdx = ""; Note = ""; Text = $text.TrimEnd() }
            continue
        }

        # 无独立文件：查内嵌/继承映射。
        $sup = $supplemental[$name]
        if ($sup) {
            if ($sup.Contains('InheritFrom')) {
                $components += [PSCustomObject]@{ Name = $name; Dir = $dir.FullName; Status = "inherited"; Source = $sup.InheritFrom; Spdx = $sup.Spdx; Note = $sup.Note; Text = "" }
                continue
            }
            $emb = Get-EmbeddedLicense $sup
            if ($emb) {
                $components += [PSCustomObject]@{ Name = $name; Dir = $dir.FullName; Status = "embedded"; Source = ($sup.File -replace '\\', '/'); Spdx = $sup.Spdx; Note = $sup.Note; Text = $emb }
                continue
            }
        }

        $components += [PSCustomObject]@{ Name = $name; Dir = $dir.FullName; Status = "missing"; Source = ""; Spdx = ""; Note = ""; Text = "" }
    }
}

$shipped   = $components | Where-Object { $_.Status -ne "empty" }
$resolved  = $components | Where-Object { $_.Status -in @("file", "embedded", "inherited") }
$missing   = $components | Where-Object { $_.Status -eq "missing" }
$empty     = $components | Where-Object { $_.Status -eq "empty" }

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("# Third-Party Licenses")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("DSEngine 发行包包含以下第三方组件。各组件版权归其各自作者所有，")
[void]$sb.AppendLine("以下完整收录其许可证文本。本文件由 ``scripts/collect_third_party_licenses.ps1`` 自动生成。")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("- 生成时间: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
[void]$sb.AppendLine("- 参与发布的组件: $($shipped.Count)（已收录许可证 $($resolved.Count)，待补充 $($missing.Count)）")
[void]$sb.AppendLine("- 未初始化/未启用 (空 submodule，不参与发布): $($empty.Count)")
[void]$sb.AppendLine("")

if ($missing.Count -gt 0) {
    [void]$sb.AppendLine("> 注意: 以下已启用组件未能定位许可证文件，发布前需人工确认并补充：")
    foreach ($m in $missing) { [void]$sb.AppendLine("> - $($m.Name)") }
    [void]$sb.AppendLine("")
}

[void]$sb.AppendLine("## 组件清单")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("| 组件 | 许可证 | 来源 |")
[void]$sb.AppendLine("|------|--------|------|")
foreach ($c in ($shipped | Sort-Object Name)) {
    $lictype = if ($c.Spdx) { $c.Spdx } elseif ($c.Status -eq "file") { "见全文" } else { "(待补充)" }
    $src = if ($c.Source) { $c.Source } else { "-" }
    [void]$sb.AppendLine("| $($c.Name) | $lictype | $src |")
}
[void]$sb.AppendLine("")

foreach ($c in ($resolved | Sort-Object Name)) {
    [void]$sb.AppendLine("---")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("## $($c.Name)")
    [void]$sb.AppendLine("")
    if ($c.Spdx) { [void]$sb.AppendLine("许可证: **$($c.Spdx)**") ; [void]$sb.AppendLine("") }
    if ($c.Note) { [void]$sb.AppendLine($c.Note) ; [void]$sb.AppendLine("") }
    if ($c.Status -eq "inherited") {
        [void]$sb.AppendLine("> 许可证文本见 **$($c.Source)** 一节。")
        [void]$sb.AppendLine("")
        continue
    }
    [void]$sb.AppendLine("来源: ``$($c.Source)``")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine('```text')
    [void]$sb.AppendLine($c.Text)
    [void]$sb.AppendLine('```')
    [void]$sb.AppendLine("")
}

$outDir = Split-Path -Parent $OutFile
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Force -Path $outDir | Out-Null }

# UTF-8 (无 BOM) 写出，便于 GitHub 正确渲染。
[System.IO.File]::WriteAllText($OutFile, $sb.ToString(), (New-Object System.Text.UTF8Encoding($false)))

Write-Host "THIRD_PARTY_LICENSES -> $OutFile"
Write-Host "  参与发布: $($shipped.Count)  已收录: $($resolved.Count)  待补充: $($missing.Count)  空 submodule: $($empty.Count)"
if ($missing.Count -gt 0) {
    Write-Host "  待补充组件: $(($missing | ForEach-Object { $_.Name }) -join ', ')" -ForegroundColor Yellow
}

exit 0
