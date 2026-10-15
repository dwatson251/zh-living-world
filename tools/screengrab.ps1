# screengrab.ps1 -ProcId <pid> -Out <png>: copy the screen area of a process's visible window (the window must be uncovered) to a PNG.
param([int]$ProcId, [string]$Out)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class SG { [DllImport("user32.dll")] public static extern bool EnumWindows(EP p, IntPtr l); public delegate bool EP(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid); [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RC r); [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref PT p);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 public struct RC { public int l, t, r, b; } public struct PT { public int x, y; }
 public static IntPtr Find(uint pid) { IntPtr r = IntPtr.Zero; EnumWindows(delegate(IntPtr h, IntPtr l) { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) { r = h; return false; } return true; }, IntPtr.Zero); return r; } }
"@
$h = [SG]::Find([uint32]$ProcId); if ($h -eq [IntPtr]::Zero) { 'no window'; exit 1 }
[SG]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 700
$rc = New-Object SG+RC; [SG]::GetClientRect($h, [ref]$rc) | Out-Null; $pt = New-Object SG+PT; [SG]::ClientToScreen($h, [ref]$pt) | Out-Null
$w = $rc.r - $rc.l; $ht = $rc.b - $rc.t
$bmp = New-Object System.Drawing.Bitmap $w, $ht; $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($pt.x, $pt.y, 0, 0, $bmp.Size); $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); "saved $w x $ht"
