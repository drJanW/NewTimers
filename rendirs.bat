@echo off
setlocal

REM Gebruik: rendirs [-n]
set "DRYRUN="
if /I "%~1"=="-n" set "DRYRUN=-n"

REM Start PowerShell script met dezelfde parameters
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0rendirs.ps1" %DRYRUN%

REM Doorgeven exit code
exit /b %errorlevel%