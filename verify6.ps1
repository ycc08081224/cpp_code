$ErrorActionPreference = 'SilentlyContinue'
$exeDir = 'd:\gongzuo\zuoye\huochairen\cpp_code\build_local'
$out = 'd:\gongzuo\zuoye\huochairen\cpp_code\shotE.png'

Get-Process -Name MultiMediaPlayer | Stop-Process -Force
Start-Sleep -Milliseconds 700

$env:PATH = "C:\Qt\6.7.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public struct RVB { public int L, T, R, B; }
public class WVB {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RVB r);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();

  public static IntPtr MainWindowOf(uint target) {
    IntPtr best = IntPtr.Zero; long bestArea = 0;
    EnumWindows((h, l) => {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid != target || !IsWindowVisible(h)) return true;
      RVB r; GetWindowRect(h, out r);
      long area = (long)(r.R - r.L) * (r.B - r.T);
      if (area > bestArea) { bestArea = area; best = h; }
      return true;
    }, IntPtr.Zero);
    return best;
  }

  // 列出该进程的可见窗口，用来确认音量浮层是否真的上屏
  public static List<string> VisibleWindows(uint target) {
    List<string> res = new List<string>();
    EnumWindows((h, l) => {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid != target || !IsWindowVisible(h)) return true;
      RVB r; GetWindowRect(h, out r);
      res.Add(string.Format("visible window rect={0},{1} {2}x{3}", r.L, r.T, r.R - r.L, r.B - r.T));
      return true;
    }, IntPtr.Zero);
    return res;
  }
}
"@
[WVB]::SetProcessDPIAware() | Out-Null

# 不带路径参数启动（中文路径经命令行传递会丢编码），靠上次保存的面板恢复
Start-Process -FilePath (Join-Path $exeDir 'MultiMediaPlayer.exe') -WorkingDirectory $exeDir
Start-Sleep -Seconds 4

$p = Get-Process -Name MultiMediaPlayer | Select-Object -First 1
if (-not $p) { Write-Output "no process"; exit 1 }

$r = New-Object RVB
$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 12; $i++) {
    $hwnd = [WVB]::MainWindowOf([uint32]$p.Id)
    if ($hwnd -ne [IntPtr]::Zero) {
        [WVB]::GetWindowRect($hwnd, [ref]$r) | Out-Null
        if (($r.R - $r.L) -gt 200 -and ($r.B - $r.T) -gt 200) { break }
    }
    Start-Sleep -Milliseconds 500
}
Write-Output ("rect {0},{1} {2}x{3}" -f $r.L, $r.T, ($r.R - $r.L), ($r.B - $r.T))

# 激活窗口：否则鼠标 hover 事件到不了播放器，控制条会一直隐藏
[WVB]::ShowWindow($hwnd, 9) | Out-Null
for ($i = 0; $i -lt 5; $i++) {
    $fg = [WVB]::GetForegroundWindow()
    if ($fg -ne $hwnd) {
        $fgPid = 0
        $fgTid = [WVB]::GetWindowThreadProcessId($fg, [ref]$fgPid)
        $myTid = [WVB]::GetCurrentThreadId()
        [WVB]::AttachThreadInput($myTid, $fgTid, $true) | Out-Null
        [WVB]::BringWindowToTop($hwnd) | Out-Null
        [WVB]::SetForegroundWindow($hwnd) | Out-Null
        [WVB]::AttachThreadInput($myTid, $fgTid, $false) | Out-Null
    }
    Start-Sleep -Milliseconds 300
    if ([WVB]::GetForegroundWindow() -eq $hwnd) { break }
}
Write-Output ("foreground is target = {0}" -f ([WVB]::GetForegroundWindow() -eq $hwnd))

# 窗口动画结束后重新取一次矩形，否则拿到的还是最大化过程中的旧位置
Start-Sleep -Milliseconds 600
[WVB]::GetWindowRect($hwnd, [ref]$r) | Out-Null
Write-Output ("rect2 {0},{1} {2}x{3}" -f $r.L, $r.T, ($r.R - $r.L), ($r.B - $r.T))

# 先进画面中央让控制条出现，再悬停到音量按钮上（弹出竖式音量条）
[WVB]::SetCursorPos([int](($r.L + $r.R) / 2), [int](($r.T + $r.B) / 2)) | Out-Null
Start-Sleep -Milliseconds 400
[WVB]::SetCursorPos([int]($r.L + 0.38 * ($r.R - $r.L)), [int]($r.T + 0.46 * ($r.B - $r.T))) | Out-Null
Start-Sleep -Milliseconds 200
[WVB]::mouse_event(0x0001, 1, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 150
[WVB]::mouse_event(0x0001, -1, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 1200

Write-Output "--- visible windows ---"
foreach ($line in [WVB]::VisibleWindows([uint32]$p.Id)) { Write-Output $line }

$w = $r.R - $r.L; $h = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $h))
$g.Dispose()
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "saved"

Get-Process -Name MultiMediaPlayer | Stop-Process -Force
