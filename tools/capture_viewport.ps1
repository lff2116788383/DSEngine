param(
    [Parameter(Mandatory=$true)]
    [string]$OutputPath,
    [string]$Mode = "window",
    [string]$WindowTitle = "DSEngine",
    [string]$CoordsFile = "C:\temp\viewport_rect.txt",
    [int]$DelayMs = 200
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32Capture {
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

Start-Sleep -Milliseconds $DelayMs

function Capture-Region($x, $y, $w, $h, $path) {
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $gfx = [System.Drawing.Graphics]::FromImage($bmp)
    $gfx.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size($w, $h)))
    $gfx.Dispose()
    $dir = [System.IO.Path]::GetDirectoryName($path)
    if (!(Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "OK: $path ($w x $h)"
}

switch ($Mode) {
    "viewport" {
        if (Test-Path $CoordsFile) {
            $parts = (Get-Content $CoordsFile).Trim().Split(",")
            $x = [int]$parts[0]; $y = [int]$parts[1]
            $w = [int]$parts[2]; $h = [int]$parts[3]
            Capture-Region $x $y $w $h $OutputPath
        } else {
            Write-Error "Coords file not found: $CoordsFile"
            exit 1
        }
    }
    "window" {
        $proc = Get-Process | Where-Object { $_.MainWindowTitle -like "*$WindowTitle*" } | Select-Object -First 1
        if ($proc) {
            $rect = New-Object Win32Capture+RECT
            [Win32Capture]::GetWindowRect($proc.MainWindowHandle, [ref]$rect) | Out-Null
            $w = $rect.Right - $rect.Left
            $h = $rect.Bottom - $rect.Top
            Capture-Region $rect.Left $rect.Top $w $h $OutputPath
        } else {
            Write-Error "Window not found: $WindowTitle"
            exit 1
        }
    }
    "region" {
        $parts = $CoordsFile.Split(",")
        Capture-Region ([int]$parts[0]) ([int]$parts[1]) ([int]$parts[2]) ([int]$parts[3]) $OutputPath
    }
}