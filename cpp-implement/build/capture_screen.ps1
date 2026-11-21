param([string]$Exe, [string]$Out, [string]$Title)
Add-Type -AssemblyName System.Drawing
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 700
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W32b {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string cls, string title);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[W32b]::SetProcessDPIAware() | Out-Null
$h=[W32b]::FindWindowW('UI_New_WindowHost',$Title)
if ($h -eq [IntPtr]::Zero) { Write-Output 'no window'; $p.Kill(); exit 1 }
$r=New-Object W32b+RECT; [W32b]::GetWindowRect($h,[ref]$r)|Out-Null
$w=$r.Right-$r.Left; $ht=$r.Bottom-$r.Top
$bmp=New-Object System.Drawing.Bitmap $w,$ht
$g=[System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Left,$r.Top,0,0,(New-Object System.Drawing.Size $w,$ht))
$g.Dispose(); $bmp.Save($Out,[System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
Write-Output "$w x $ht at $($r.Left),$($r.Top)"
$p.Kill()
