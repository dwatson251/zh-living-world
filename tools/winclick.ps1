# winclick.ps1 -ProcId <pid> -X <x> -Y <y> [-Right] [-Key <vk>]: post a mouse click (client-area coordinates) to a window of a process, without touching the real mouse or keyboard.
param([int]$ProcId, [int]$X, [int]$Y, [switch]$Right, [int]$Key = 0)
Add-Type @"
using System; using System.Runtime.InteropServices;
public class WC { [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l); public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 public static IntPtr Find(uint pid) { IntPtr r = IntPtr.Zero; EnumWindows(delegate(IntPtr h, IntPtr l) { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) { r = h; return false; } return true; }, IntPtr.Zero); return r; } }
"@
$h = [WC]::Find([uint32]$ProcId); if ($h -eq [IntPtr]::Zero) { 'no window'; exit 1 }
if ($Key -gt 0) { [WC]::PostMessage($h, 0x100, [IntPtr]$Key, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 80; [WC]::PostMessage($h, 0x101, [IntPtr]$Key, [IntPtr]0) | Out-Null; 'key ' + $Key; exit 0 }
$l = [IntPtr](($Y -shl 16) -bor ($X -band 0xffff))
$down = if ($Right) { 0x204 } else { 0x201 }; $up = if ($Right) { 0x205 } else { 0x202 }
[WC]::PostMessage($h, 0x200, [IntPtr]0, $l) | Out-Null; Start-Sleep -Milliseconds 150
[WC]::PostMessage($h, $down, [IntPtr]1, $l) | Out-Null; Start-Sleep -Milliseconds 120
[WC]::PostMessage($h, $up, [IntPtr]0, $l) | Out-Null
"clicked $X,$Y on window $h"
