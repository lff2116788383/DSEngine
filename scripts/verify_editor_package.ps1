<#
.SYNOPSIS
    Verifies the contents of a staged or archived DSEngine editor package.
.PARAMETER PackagePath
    Path to the package directory or zip archive.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ResolvedPackage = (Resolve-Path -LiteralPath $PackagePath).Path
$ExtractionRoot = $null

try {
    if (Test-Path -LiteralPath $ResolvedPackage -PathType Leaf) {
        if ([System.IO.Path]::GetExtension($ResolvedPackage) -ne ".zip") {
            throw "Editor package must be a directory or zip archive: $ResolvedPackage"
        }
        $ExtractionRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
            "dse-editor-package-" + [Guid]::NewGuid().ToString("N")
        )
        New-Item -ItemType Directory -Force -Path $ExtractionRoot | Out-Null
        Expand-Archive -LiteralPath $ResolvedPackage -DestinationPath $ExtractionRoot
        $Roots = @(Get-ChildItem -LiteralPath $ExtractionRoot -Directory)
        if ($Roots.Count -ne 1) {
            throw "Editor package archive must contain exactly one root directory."
        }
        $PackageRoot = $Roots[0].FullName
    } else {
        $PackageRoot = $ResolvedPackage
    }

    $RequiredFiles = @(
        "dsengine-editor.exe",
        "AssetBuilder.exe",
        "dse.exe",
        "editor-package-manifest.json",
        "README.md",
        "CHANGELOG.md",
        "LICENSE",
        "NOTICE",
        "THIRD_PARTY_LICENSES.md",
        "fonts\Inter-Regular.ttf",
        "fonts\Inter-Bold.ttf",
        "fonts\NotoSansSC-Regular.ttf",
        "fonts\fa-solid-900.ttf",
        "data\icon\dse_icon.png",
        "script\application.lua",
        "managed\DSEngine.Runtime.dll",
        "managed\DSEngine.Game.dll",
        "managed\DSEngine.Runtime.runtimeconfig.json"
    )
    foreach ($RelativePath in $RequiredFiles) {
        $FullPath = Join-Path $PackageRoot $RelativePath
        if (-not (Test-Path -LiteralPath $FullPath -PathType Leaf)) {
            throw "Editor package is missing required file: $RelativePath"
        }
    }

    foreach ($RelativePath in @("data", "samples", "script", "fonts", "plugins", "tools\agent", "GameScripts")) {
        if (-not (Test-Path -LiteralPath (Join-Path $PackageRoot $RelativePath) -PathType Container)) {
            throw "Editor package is missing required directory: $RelativePath"
        }
    }

    $ManifestPath = Join-Path $PackageRoot "editor-package-manifest.json"
    $Manifest = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Manifest.schemaVersion -ne 1 -or $Manifest.product -ne "DSEngine Editor") {
        throw "Editor package manifest is invalid: $ManifestPath"
    }
    foreach ($Property in @("editor", "assetBuilder", "cli", "gameRuntime")) {
        $ExecutableName = $Manifest.executables.$Property
        if ([string]::IsNullOrWhiteSpace($ExecutableName)) {
            throw "Editor package manifest has no executable entry for: $Property"
        }
        if (-not (Test-Path -LiteralPath (Join-Path $PackageRoot $ExecutableName) -PathType Leaf)) {
            throw "Manifest executable is missing from package: $ExecutableName"
        }
    }

    $ForbiddenFiles = @(Get-ChildItem -LiteralPath $PackageRoot -Recurse -File | Where-Object {
        $_.Extension -in @(".lib", ".exp", ".ilk") -or
        $_.Name -like "dse_gtest_*" -or
        $_.Name -like "*-uitest.exe"
    })
    if ($ForbiddenFiles.Count -gt 0) {
        throw "Editor package contains development-only files: $($ForbiddenFiles.FullName -join ', ')"
    }

    $ForbiddenGameScriptDirectories = @(
        Get-ChildItem -LiteralPath (Join-Path $PackageRoot "GameScripts") -Recurse -Directory |
            Where-Object { $_.Name -in @("bin", "obj", ".vs", "__pycache__", "node_modules") }
    )
    if ($ForbiddenGameScriptDirectories.Count -gt 0) {
        throw "Editor package contains GameScripts build directories: $($ForbiddenGameScriptDirectories.FullName -join ', ')"
    }

    Write-Host "Editor package verified: $PackageRoot"
}
finally {
    if ($ExtractionRoot -and (Test-Path -LiteralPath $ExtractionRoot)) {
        Remove-Item -LiteralPath $ExtractionRoot -Recurse -Force
    }
}
