@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0go.ps1"
set ERR=%ERRORLEVEL%
endlocal & exit /b %ERR%
