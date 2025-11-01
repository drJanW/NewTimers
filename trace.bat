@echo off
setlocal
set "PORT=%~1"
if "%PORT%"=="" set "PORT=6"
set "PIO=%USERPROFILE%\.platformio\penv\Scripts\platformio.exe"
if not exist "%PIO%" (
    echo PlatformIO executable not found: "%PIO%"
    exit /b 1
)
"%PIO%" device monitor --port COM%PORT% --baud 115200
