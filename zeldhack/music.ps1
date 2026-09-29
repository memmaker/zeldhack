Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Mci {
  [DllImport("winmm.dll", CharSet=CharSet.Unicode)]
  public static extern int mciSendString(string command, System.Text.StringBuilder buffer, int bufferSize, IntPtr hwndCallback);
}
"@

$base = Split-Path -Parent $MyInvocation.MyCommand.Path
$mp3  = Join-Path $base "music\ambience.mp3"

# Ouvre + joue en boucle (sans fenêtre)
[void][Mci]::mciSendString("open `"$mp3`" type mpegvideo alias bgm", $null, 0, [IntPtr]::Zero)
[void][Mci]::mciSendString("play bgm repeat", $null, 0, [IntPtr]::Zero)

# Garde le process vivant (la musique s'arrête si on tue ce PowerShell)
while ($true) { Start-Sleep -Seconds 60 }
