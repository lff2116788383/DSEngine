# Download editor fonts for DSEngine Editor
# Run this script once to fetch all required font files.

$fontsDir = $PSScriptRoot

Write-Host "Downloading editor fonts to: $fontsDir"

# Inter Regular & Bold (from GitHub releases)
$interVersion = "4.0"
$interUrl = "https://github.com/rsms/inter/releases/download/v${interVersion}/Inter-${interVersion}.zip"
$interZip = Join-Path $fontsDir "inter.zip"

Write-Host "Downloading Inter font..."
Invoke-WebRequest -Uri $interUrl -OutFile $interZip
Expand-Archive -Path $interZip -DestinationPath (Join-Path $fontsDir "inter_tmp") -Force

# Copy the specific files we need
$interTmp = Join-Path $fontsDir "inter_tmp"
$interStatic = Join-Path (Join-Path (Join-Path $interTmp "Inter-${interVersion}") "InterDesktop") ""
if (Test-Path (Join-Path $interStatic "Inter-Regular.ttf")) {
    Copy-Item (Join-Path $interStatic "Inter-Regular.ttf") $fontsDir -Force
    Copy-Item (Join-Path $interStatic "Inter-Bold.ttf") $fontsDir -Force
} else {
    # Fallback: search recursively for the files
    $found = Get-ChildItem -Path $interTmp -Recurse -Filter "Inter-Regular.ttf" | Select-Object -First 1
    if ($found) { Copy-Item $found.FullName $fontsDir -Force }
    $foundBold = Get-ChildItem -Path $interTmp -Recurse -Filter "Inter-Bold.ttf" | Select-Object -First 1
    if ($foundBold) { Copy-Item $foundBold.FullName $fontsDir -Force }
}
Remove-Item $interZip -Force
Remove-Item (Join-Path $fontsDir "inter_tmp") -Recurse -Force
Write-Host "  Inter fonts extracted."

# Noto Sans SC (from Google Fonts / GitHub)
$notoUrl = "https://github.com/notofonts/noto-cjk/releases/download/Sans2.004/08_NotoSansCJKsc.zip"
$notoZip = Join-Path $fontsDir "noto_sc.zip"

Write-Host "Downloading Noto Sans SC..."
Invoke-WebRequest -Uri $notoUrl -OutFile $notoZip
$notoTmp = Join-Path $fontsDir "noto_tmp"
Expand-Archive -Path $notoZip -DestinationPath $notoTmp -Force
$notoTtf = Get-ChildItem -Path $notoTmp -Recurse -Filter "NotoSansSC-Regular.ttf" | Select-Object -First 1
if ($notoTtf) {
    Copy-Item $notoTtf.FullName (Join-Path $fontsDir "NotoSansSC-Regular.ttf") -Force
} else {
    # Try .otf
    $notoOtf = Get-ChildItem -Path $notoTmp -Recurse -Filter "NotoSansSC-Regular.otf" | Select-Object -First 1
    if ($notoOtf) {
        Copy-Item $notoOtf.FullName (Join-Path $fontsDir "NotoSansSC-Regular.ttf") -Force
    } else {
        # Try any NotoSansSC file
        $notoAny = Get-ChildItem -Path $notoTmp -Recurse -Filter "*NotoSans*SC*" -Include "*.ttf","*.otf" | Select-Object -First 1
        if ($notoAny) {
            Copy-Item $notoAny.FullName (Join-Path $fontsDir "NotoSansSC-Regular.ttf") -Force
        } else {
            Write-Host "  WARNING: Could not find NotoSansSC font in archive. Listing contents:"
            Get-ChildItem -Path $notoTmp -Recurse | ForEach-Object { Write-Host "    $($_.FullName)" }
        }
    }
}
Remove-Item $notoZip -Force
Remove-Item $notoTmp -Recurse -Force
Write-Host "  Noto Sans SC extracted."

# Font Awesome 6 Free Solid (16-bit PUA range, no WCHAR32 needed)
$faVersion = "6.5.1"
$faUrl = "https://github.com/FortAwesome/Font-Awesome/releases/download/${faVersion}/fontawesome-free-${faVersion}-web.zip"
$faZip = Join-Path $fontsDir "fa.zip"

Write-Host "Downloading Font Awesome 6 Free..."
Invoke-WebRequest -Uri $faUrl -OutFile $faZip
$faTmp = Join-Path $fontsDir "fa_tmp"
Expand-Archive -Path $faZip -DestinationPath $faTmp -Force
$faSolid = Get-ChildItem -Path $faTmp -Recurse -Filter "fa-solid-900.ttf" | Select-Object -First 1
if ($faSolid) {
    Copy-Item $faSolid.FullName (Join-Path $fontsDir "fa-solid-900.ttf") -Force
} else {
    Write-Host "  WARNING: fa-solid-900.ttf not found in archive"
    Get-ChildItem -Path $faTmp -Recurse -Filter "*.ttf" | ForEach-Object { Write-Host "    $($_.Name)" }
}
Remove-Item $faZip -Force
Remove-Item $faTmp -Recurse -Force
Write-Host "  Font Awesome extracted."

Write-Host ""
Write-Host "=== Font download complete ==="
Write-Host "Files in ${fontsDir}:"
Get-ChildItem $fontsDir -Filter "*.ttf" | ForEach-Object { Write-Host "  $_" }
