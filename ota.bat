@echo off
setlocal

echo [OTA] Building firmware for esp32_v3_ota...
pio run -e esp32_v3_ota
if errorlevel 1 (
	echo [OTA] Build failed, aborting upload.
	set "build_rc=%errorlevel%"
	endlocal & exit /b %build_rc%
)

echo.
echo [OTA] Build complete.
echo [OTA] In de webinterface: open OTA en klik 'Start OTA'.
echo [OTA] Het apparaat reboot over ongeveer 15 seconden; druk daarna op een toets om de upload te starten.
pause

echo [OTA] Uploaden via OTA...
pio run -e esp32_v3_ota -t nobuild -t upload
set "ota_rc=%errorlevel%"
endlocal & exit /b %ota_rc%
