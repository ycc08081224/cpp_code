$ErrorActionPreference = 'SilentlyContinue'
$exeDir = 'd:\gongzuo\zuoye\huochairen\cpp_code\build_local'
$key = 'HKCU\Software\MultiMediaPlayer'
$bak = Join-Path $env:TEMP 'mm_settings_backup.reg'
$out = 'd:\gongzuo\zuoye\huochairen\cpp_code\shotE.png'

Get-Process -Name MultiMediaPlayer | Stop-Process -Force
Start-Sleep -Milliseconds 700
& reg export $key $bak /y | Out-Null
& reg delete $key /f | Out-Null

$env:PATH = "C:\Qt\6.7.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH
$vid = 'd:\gongzuo\zuoye\huochairen\src\com\ycc\games\视频库\异常.mp4'

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public struct RVA { public int L, T, R, B; }
public class WVA {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RVA r);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
"@
[WVA]::SetProcessDPIAware() | Out-Null

Start-Process -FilePath (Join-Path $exeDir 'MultiMediaPlayer.exe') -WorkingDirectory $exeDir `
    -ArgumentList @("`"$vid`"")
Start-Sleep -Seconds 4

$p = Get-Process -Name MultiMediaPlayer | Select-Object -First 1
if (-not $p) { Write-Output "no process"; exit 1 }
$h = $p.MainWindowHandle
$r = New-Object RVA
[WVA]::GetWindowRect($h, [ref]$r) | Out-Null
Write-Output ("window {0},{1} {2}x{3}" -f $r.L, $r.T, ($r.R - $r.L), ($r.B - $r.T))

[WVA]::SetCursorPos([int](($r.L + $r.R) / 2), [int](($r.T + $r.B) / 2)) | Out-Null
Start-Sleep -Milliseconds 300
[WVA]::SetCursorPos([int]($r.R - 80), [int]($r.B - 64)) | Out-Null
Start-Sleep -Milliseconds 250
[WVA]::SetCursorPos([int]($r.R - 79), [int]($r.B - 64)) | Out-Null
Start-Sleep -Milliseconds 900

$w = $r.R - $r.L; $hh = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[WVA]::PrintWindow($h, $hdc, 2) | Out-Null
$g.ReleaseHdc($hdc)
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output "saved"

Get-Process -Name MultiMediaPlayer | Stop-Process -Force
Start-Sleep -Milliseconds 700
if (Test-Path $bak) { & reg import $bak | Out-Null; Remove-Item $bak -Force }
Write-Output "restored"
