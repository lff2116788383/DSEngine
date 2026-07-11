<#
.SYNOPSIS
    Packages a self-contained Windows DSEngine editor artifact.
.DESCRIPTION
    Stages only the editor executables, runtime dependencies, and repository
    resources required by the packaged editor. The script intentionally avoids
    copying the entire bin directory so stale build outputs cannot leak into a
    release.
.PARAMETER Version
    Release version without or with a leading "v".
.PARAMETER BinDir
    Directory containing built Release executables and DLLs.
.PARAMETER OutputDir
    Directory that receives the staging directory and zip archive.
.PARAMETER IncludeSymbols
    Include PDB files next to packaged executables.
.PARAMETER SkipArchive
    Keep the staged package but do not create a zip archive.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$BinDir,
    [string]$OutputDir,

    [switch]$IncludeSymbols,
    [switch]$SkipArchive
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$SourceDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $BinDir) {
    $BinDir = Join-Path $SourceDir "bin"
}
if (-not $OutputDir) {
    $OutputDir = Join-Path $SourceDir "artifacts"
}

$BinDir = [System.IO.Path]::GetFullPath($BinDir)
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
$NormalizedVersion = $Version.Trim()
if ($NormalizedVersion.StartsWith("v", [System.StringComparison]::OrdinalIgnoreCase)) {
    $NormalizedVersion = $NormalizedVersion.Substring(1)
}
if ([string]::IsNullOrWhiteSpace($NormalizedVersion)) {
    throw "Version must not be empty."
}
if (-not (Test-Path -LiteralPath $BinDir -PathType Container)) {
    throw "Editor bin directory does not exist: $BinDir"
}

$PackageName = "DSEngine-editor-v$NormalizedVersion-win-x64"
$StageRoot = Join-Path $OutputDir $PackageName
$ArchivePath = Join-Path $OutputDir "$PackageName.zip"

function Copy-RequiredFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Required editor package file is missing: $Source"
    }
    $DestinationDir = Split-Path -Parent $Destination
    if ($DestinationDir) {
        New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Copy-RequiredDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Required editor package directory is missing: $Source"
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}

function Copy-SourceDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Required editor package source directory is missing: $Source"
    }
    $ResolvedSource = (Resolve-Path -LiteralPath $Source).Path
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Get-ChildItem -LiteralPath $ResolvedSource -Recurse -File | Where-Object {
        $RelativePath = $_.FullName.Substring($ResolvedSource.Length).TrimStart("\", "/")
        $Segments = $RelativePath -split "[\\/]"
        -not ($Segments | Where-Object { $_ -in @("bin", "obj", ".vs", "__pycache__", "node_modules") })
    } | ForEach-Object {
        $RelativePath = $_.FullName.Substring($ResolvedSource.Length).TrimStart("\", "/")
        $TargetPath = Join-Path $Destination $RelativePath
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $TargetPath) | Out-Null
        Copy-Item -LiteralPath $_.FullName -Destination $TargetPath -Force
    }
}

function Find-FirstFile {
    param([string[]]$Names)

    foreach ($Name in $Names) {
        $Candidate = Join-Path $BinDir $Name
        if (Test-Path -LiteralPath $Candidate -PathType Leaf) {
            return $Candidate
        }
    }
    throw "None of the required files were found in ${BinDir}: $($Names -join ', ')"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
if (Test-Path -LiteralPath $StageRoot) {
    Remove-Item -LiteralPath $StageRoot -Recurse -Force
}
if (Test-Path -LiteralPath $ArchivePath) {
    Remove-Item -LiteralPath $ArchivePath -Force
}
New-Item -ItemType Directory -Force -Path $StageRoot | Out-Null

$Executables = [ordered]@{
    editor = Find-FirstFile @("dsengine-editor.exe")
    assetBuilder = Find-FirstFile @("AssetBuilder.exe")
    cli = Find-FirstFile @("dse.exe")
    gameRuntime = Find-FirstFile @(
        "dsengine_game_release.exe",
        "dsengine_game.exe"
    )
}

foreach ($Entry in $Executables.GetEnumerator()) {
    Copy-RequiredFile $Entry.Value (Join-Path $StageRoot (Split-Path -Leaf $Entry.Value))
}

Get-ChildItem -LiteralPath $BinDir -File -Filter "*.dll" | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $StageRoot $_.Name) -Force
}

if ($IncludeSymbols) {
    foreach ($Executable in $Executables.Values) {
        $PdbPath = [System.IO.Path]::ChangeExtension($Executable, ".pdb")
        if (Test-Path -LiteralPath $PdbPath -PathType Leaf) {
            Copy-Item -LiteralPath $PdbPath -Destination (Join-Path $StageRoot (Split-Path -Leaf $PdbPath)) -Force
        }
    }
}

Copy-RequiredDirectory (Join-Path $BinDir "data") (Join-Path $StageRoot "data")
Copy-RequiredDirectory (Join-Path $BinDir "samples") (Join-Path $StageRoot "samples")
Copy-RequiredDirectory (Join-Path $BinDir "script") (Join-Path $StageRoot "script")
Copy-RequiredDirectory (Join-Path $BinDir "fonts") (Join-Path $StageRoot "fonts")
Copy-RequiredDirectory (Join-Path $SourceDir "plugins") (Join-Path $StageRoot "plugins")
Copy-RequiredDirectory (Join-Path $SourceDir "tools\agent") (Join-Path $StageRoot "tools\agent")
Copy-SourceDirectory (Join-Path $SourceDir "GameScripts") (Join-Path $StageRoot "GameScripts")
Copy-RequiredDirectory (Join-Path $BinDir "managed") (Join-Path $StageRoot "managed")

foreach ($DocumentationFile in @("README.md", "CHANGELOG.md", "LICENSE", "NOTICE", "THIRD_PARTY_LICENSES.md")) {
    Copy-RequiredFile (Join-Path $SourceDir $DocumentationFile) (Join-Path $StageRoot $DocumentationFile)
}

$Manifest = [ordered]@{
    schemaVersion = 1
    product = "DSEngine Editor"
    version = $NormalizedVersion
    platform = "win-x64"
    generatedUtc = [DateTime]::UtcNow.ToString("o")
    executables = [ordered]@{
        editor = Split-Path -Leaf $Executables.editor
        assetBuilder = Split-Path -Leaf $Executables.assetBuilder
        cli = Split-Path -Leaf $Executables.cli
        gameRuntime = Split-Path -Leaf $Executables.gameRuntime
    }
}
$Manifest | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $StageRoot "editor-package-manifest.json") -Encoding UTF8

if (-not $SkipArchive) {
    Compress-Archive -LiteralPath $StageRoot -DestinationPath $ArchivePath -CompressionLevel Optimal
    Write-Host "Editor package archive: $ArchivePath"
}

Write-Host "Editor package stage: $StageRoot"
Write-Output ([PSCustomObject]@{
    PackageName = $PackageName
    StageRoot = $StageRoot
    ArchivePath = if ($SkipArchive) { $null } else { $ArchivePath }
})
