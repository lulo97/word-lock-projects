param([string]$Exe)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices;
public class TrayFlow {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern int GetWindowThreadProcessId(IntPtr h, out int pid);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
}
"@
[TrayFlow]::SetProcessDPIAware() | Out-Null
function Find-Window($pidValue, $title, $visibleOnly) {
 $script:found=[IntPtr]::Zero
 $cb=[TrayFlow+EnumProc]{ param($h,$l)
  $sb=New-Object System.Text.StringBuilder 256
  [TrayFlow]::GetWindowTextW($h,$sb,256)|Out-Null
  $wp=0; [TrayFlow]::GetWindowThreadProcessId($h,[ref]$wp)|Out-Null
  if ($wp -eq $pidValue -and $sb.ToString() -eq $title -and (!$visibleOnly -or [TrayFlow]::IsWindowVisible($h))) { $script:found=$h; return $false }
  return $true }
 [TrayFlow]::EnumWindows($cb,[IntPtr]::Zero)|Out-Null
 return $script:found
}
function Capture($h,$out) {
 $r=New-Object TrayFlow+RECT; [TrayFlow]::GetWindowRect($h,[ref]$r)|Out-Null
 $w=$r.Right-$r.Left; $ht=$r.Bottom-$r.Top
 $bmp=New-Object System.Drawing.Bitmap $w,$ht
 $g=[System.Drawing.Graphics]::FromImage($bmp)
 $g.CopyFromScreen($r.Left,$r.Top,0,0,(New-Object System.Drawing.Size $w,$ht))
 $g.Dispose(); $bmp.Save($out,[System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
 Write-Output "$w x $ht"
}
$p=Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 700
$count=Find-Window $p.Id 'Countdown' $true
Write-Output "count=$count"
if ($count -eq [IntPtr]::Zero) { $p.Kill(); exit 1 }
[TrayFlow]::PostMessageW($count,0x8065,[IntPtr]::Zero,[IntPtr]0x0400)|Out-Null
Start-Sleep -Milliseconds 400
$menu=Find-Window $p.Id 'Kotoba Lock' $true
Write-Output "menu=$menu"
if ($menu -ne [IntPtr]::Zero) {
  Capture $menu 'C:\Users\ADMIN\Desktop\te\cpp-implement\build\tray_menu.png'
  $xy = [IntPtr]([int](220 -shl 16) -bor 150)
  [TrayFlow]::PostMessageW($menu,0x0201,[IntPtr]1,$xy)|Out-Null
  [TrayFlow]::PostMessageW($menu,0x0202,[IntPtr]0,$xy)|Out-Null
}
Start-Sleep -Milliseconds 500
$settings=Find-Window $p.Id 'Kotoba Lock Settings' $true
Write-Output "settings=$settings"
if ($settings -ne [IntPtr]::Zero) { Capture $settings 'C:\Users\ADMIN\Desktop\te\cpp-implement\build\settings_app.png' }
Start-Sleep -Milliseconds 100
$p.Kill()
