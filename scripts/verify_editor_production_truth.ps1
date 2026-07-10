<#
.SYNOPSIS
    Prevents untracked mock or placeholder behavior from entering editor code.
.DESCRIPTION
    Known production debt is recorded in tools/audit/editor_production_debt.json.
    The check fails when a new suspicious implementation marker is introduced or
    when a baseline entry becomes stale without being removed from the ledger.
#>

[CmdletBinding()]
param(
    [string]$BaselinePath
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$SourceDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $BaselinePath) {
    $BaselinePath = Join-Path $SourceDir "tools\audit\editor_production_debt.json"
}
$BaselinePath = (Resolve-Path -LiteralPath $BaselinePath).Path

$Baseline = Get-Content -LiteralPath $BaselinePath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($Baseline.schemaVersion -ne 1) {
    throw "Unsupported editor production debt schema: $($Baseline.schemaVersion)"
}

$Entries = @($Baseline.entries)
$Ids = @{}
$Expected = @{}
foreach ($Entry in $Entries) {
    if ([string]::IsNullOrWhiteSpace($Entry.id) -or
        [string]::IsNullOrWhiteSpace($Entry.path) -or
        [string]::IsNullOrWhiteSpace($Entry.needle)) {
        throw "Every editor production debt entry requires id, path, and needle."
    }
    if ($Ids.ContainsKey($Entry.id)) {
        throw "Duplicate editor production debt id: $($Entry.id)"
    }
    $Ids[$Entry.id] = $true

    $NormalizedPath = $Entry.path.Replace("\", "/")
    $Key = "$NormalizedPath|$($Entry.needle.Trim())"
    if ($Expected.ContainsKey($Key)) {
        throw "Duplicate editor production debt marker: $Key"
    }
    $Expected[$Key] = $Entry
}

$SuspiciousPattern = [regex]::new(
    '(?:Demo (?:files|plugins)|' +
    'simulate (?:pull|push|merge|execution|~)|' +
    'Simulate the reload process|' +
    'In production this would|' +
    'mark as success|' +
    'placeholder$|' +
    'TODO: Call Emscripten build|' +
    'pre-built\), just zip it|' +
    'background placeholder)',
    [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
)

$Found = @{}
$Unexpected = [System.Collections.Generic.List[string]]::new()
$EditorSourceDir = Join-Path $SourceDir "apps\editor_cpp\src"
$SourceFiles = Get-ChildItem -LiteralPath $EditorSourceDir -Recurse -File |
    Where-Object { $_.Extension -in @(".cpp", ".h", ".inl") }

foreach ($File in $SourceFiles) {
    $RelativePath = $File.FullName.Substring($SourceDir.Length).TrimStart("\", "/").Replace("\", "/")
    $LineNumber = 0
    foreach ($Line in [System.IO.File]::ReadLines($File.FullName)) {
        $LineNumber++
        $Trimmed = $Line.Trim()
        if (-not $SuspiciousPattern.IsMatch($Trimmed)) {
            continue
        }
        $Key = "$RelativePath|$Trimmed"
        if ($Expected.ContainsKey($Key)) {
            if ($Found.ContainsKey($Key)) {
                $Unexpected.Add("$RelativePath`:$LineNumber duplicate known debt marker: $Trimmed")
            } else {
                $Found[$Key] = $true
            }
        } else {
            $Unexpected.Add("$RelativePath`:$LineNumber untracked production debt: $Trimmed")
        }
    }
}

$Stale = [System.Collections.Generic.List[string]]::new()
foreach ($Key in $Expected.Keys) {
    if (-not $Found.ContainsKey($Key)) {
        $Entry = $Expected[$Key]
        $Stale.Add("$($Entry.id): $($Entry.path) no longer contains '$($Entry.needle)'")
    }
}

if ($Unexpected.Count -gt 0 -or $Stale.Count -gt 0) {
    if ($Unexpected.Count -gt 0) {
        Write-Host "Untracked editor production debt:" -ForegroundColor Red
        $Unexpected | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    }
    if ($Stale.Count -gt 0) {
        Write-Host "Stale editor production debt baseline entries:" -ForegroundColor Red
        $Stale | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    }
    exit 1
}

Write-Host "Editor production debt baseline verified: $($Entries.Count) known entries, no untracked markers."
