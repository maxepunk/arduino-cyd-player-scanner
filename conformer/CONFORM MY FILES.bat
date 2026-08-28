@echo off
REM Windows launcher. Double-click this file.
REM
REM %~dp0 is this file's own folder; a double-clicked .bat may start
REM elsewhere, so everything is anchored to it.

cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "lib\conform.ps1"

echo.
echo Press any key to close this window.
pause >nul
