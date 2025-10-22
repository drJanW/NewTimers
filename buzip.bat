@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0buzip.ps1" %*
