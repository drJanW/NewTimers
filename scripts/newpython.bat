REM voer uit in een gewone CMD
rmdir /s /q %USERPROFILE%\.platformio\penv
rmdir /s /q %USERPROFILE%\.platformio\.cache
pio system prune -f
pio upgrade --dev
