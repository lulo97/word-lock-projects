param([string]$Exe)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class Flow {
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
[Flow]::SetProcessDPIAware() | Out-Null
function Find-Window($pidValue, $title, $visibleOnly) {
  $script:found = [IntPtr]::Zero
  $cb = [Flow+EnumProc]{
    param($h,$l)
    $sb = New-Object System.Text.StringBuilder 256
    [Flow]::GetWindowTextW($h,$sb,256) | Out-Null
    $wp = 0; [Flow]::GetWindowThreadProcessId($h,[ref]$wp) | Out-Null
    if ($wp -eq $pidValue -and $sb.ToString() -eq $title -and (!$visibleOnly -or [Flow]::IsWindowVisible($h))) {
      $script:found = $h; return $false
    }
    return $true
  }
  [Flow]::EnumWindows($cb,[IntPtr]::Zero) | Out-Null
  return $script:found
}
function Capture($h, $out) {
  $r=New-Object Flow+RECT; [Flow]::GetWindowRect($h,[ref]$r)|Out-Null
  $w=$r.Right-$r.Left; $ht=$r.Bottom-$r.Top
  $bmp=New-Object System.Drawing.Bitmap $w,$ht
  $g=[System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left,$r.Top,0,0,(New-Object System.Drawing.Size $w,$ht))
  $g.Dispose(); $bmp.Save($out,[System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
}
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 4400
$input = Find-Window $p.Id 'Kotoba Lock' $true
Write-Output "input=$input"
if ($input -eq [IntPtr]::Zero) { $p.Kill(); exit 1 }
foreach ($ch in 'ashita'.ToCharArray()) {
  [Flow]::PostMessageW($input,0x0102,[IntPtr][int][char]$ch,[IntPtr]::Zero) | Out-Null
}
Start-Sleep -Milliseconds 80
Capture $input 'C:\Users\ADMIN\Desktop\te\cpp-implement\build\flow_correct.png'
Start-Sleep -Milliseconds 500
$info = Find-Window $p.Id 'Kotoba Lock' $true
Write-Output "info=$info"
if ($info -ne [IntPtr]::Zero) {
  Capture $info 'C:\Users\ADMIN\Desktop\te\cpp-implement\build\flow_info.png'
  [Flow]::PostMessageW($info,0x0100,[IntPtr]13,[IntPtr]::Zero) | Out-Null
}
Start-Sleep -Milliseconds 500
$count = Find-Window $p.Id 'Countdown' $true
Write-Output "countdown=$count"
if ($count -ne [IntPtr]::Zero) { Capture $count 'C:\Users\ADMIN\Desktop\te\cpp-implement\build\flow_countdown.png' }
$p.Kill()
