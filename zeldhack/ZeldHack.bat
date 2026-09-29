@echo off
setlocal
cd /d "%~dp0"

rem Lance la musique (fenêtre console minimisée, pas de focus)
start "" /min "%~dp0ZeldHackMusic.exe" ^
  --no-video --loop-file=inf --no-terminal --force-window=no --audio-display=no --volume=77 ^
  "%~dp0music\ambience.mp3"

rem Petite pause
timeout /t 1 >nul

rem Lance NetHack et attend qu'il se ferme
start "" /wait "%~dp0NetHackW.exe"

rem Coupe la musique
taskkill /IM ZeldHackMusic.exe /F >nul 2>&1
