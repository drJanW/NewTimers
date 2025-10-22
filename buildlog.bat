@echo off
cd /d "%~dp0"
pio run -t clean >nul 2>&1
pio run > build.log 2>&1
