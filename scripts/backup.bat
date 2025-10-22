@echo off
setlocal enabledelayedexpansion

:: Optional summary parameter
set "param=%~1"

:: Get date and time for filename
for /f "tokens=2 delims==" %%I in ('"wmic os get localdatetime /value"') do set "datetime=%%I"
set "datetime=%datetime:~0,4%-%datetime:~4,2%-%datetime:~6,2%_%datetime:~8,2%-%datetime:~10,2%"
set "backupfile=Backup_%datetime%_%param%.txt"

:: Start writing to the backup file
> "%backupfile%" echo Merged Code (Generated on %datetime%)
if defined param >> "%backupfile%" echo Parameter: %param%
>> "%backupfile%" echo =====================================================
>> "%backupfile%" echo.

:: Backup src\main.cpp
if exist "src\main.cpp" (
    >> "%backupfile%" echo =======================================
    >> "%backupfile%" echo File: src\main.cpp
    >> "%backupfile%" echo =======================================
    type "src\main.cpp" >> "%backupfile%"
    >> "%backupfile%" echo.
    echo Added src\main.cpp
) else (
    >> "%backupfile%" echo WARNING: src\main.cpp not found! Skipping...
    echo WARNING: src\main.cpp not found! Skipping...
)

:: Require libs_list.txt
if not exist "libs_list.txt" (
    echo ERROR: libs_list.txt not found!
    >> "%backupfile%" echo ERROR: libs_list.txt not found!
    pause
    exit /b 1
)

:: Process libs_list.txt
for /f "usebackq tokens=* delims=" %%L in ("libs_list.txt") do (
    set "line=%%L"
    set "firstChar=!line:~0,1!"

    :: Skip empty and commented lines
    if not "!line!"=="" if not "!firstChar!"==";" (

        :: Handle +<file> inclusions
        if "!firstChar!"=="+" (
            set "incfile=!line:~1!"
            set "incfile=!incfile:"=!"
            set "incfile=!incfile:~0!"
            echo Processing include file: !incfile!

            if exist "!incfile!" (
                >> "%backupfile%" echo =======================================
                >> "%backupfile%" echo File: !incfile!
                >> "%backupfile%" echo =======================================
                type "!incfile!" >> "%backupfile%"
                >> "%backupfile%" echo.
                echo Added file !incfile!
            ) else (
                >> "%backupfile%" echo WARNING: Include file not found: !incfile!
                echo WARNING: Include file not found: !incfile!
            )

        ) else (
            :: Treat as library key. Pick most recent dated dir: lib\<Key><YYYYMMDD>...
            echo Processing library: %%L
            set "found_dir="
            for /f "delims=" %%D in ('dir "lib\%%L????????" /b /ad 2^>nul ^| sort') do (
                set "found_dir=lib\%%D"
            )

            if defined found_dir (
                echo Found: !found_dir!
                >> "%backupfile%" echo =======================================
                >> "%backupfile%" echo Library: !found_dir!
                >> "%backupfile%" echo =======================================
                >> "%backupfile%" echo.

                set "file_count=0"
                for %%F in ("!found_dir!\*.h" "!found_dir!\*.cpp" "!found_dir!\*.txt" "!found_dir!\*.ino") do (
                    if exist "%%~F" (
                        set /a file_count+=1
                        >> "%backupfile%" echo ---------------------------------------
                        >> "%backupfile%" echo File: %%~F
                        >> "%backupfile%" echo ---------------------------------------
                        type "%%~F" >> "%backupfile%"
                        >> "%backupfile%" echo.
                        >> "%backupfile%" echo.
                    )
                )
                if !file_count! equ 0 (
                    >> "%backupfile%" echo No source files found in !found_dir!
                    echo WARNING: No .h/.cpp/.txt/.ino files found in !found_dir!
                ) else (
                    echo Added !file_count! files from !found_dir!
                )
            ) else (
                >> "%backupfile%" echo WARNING: No directory found for lib\%%L*
                echo WARNING: No directory found for lib\%%L*
            )
        )
        echo.
    )
)

:: Backup include\ directory files
if exist "include" (
    >> "%backupfile%" echo =======================================
    >> "%backupfile%" echo Include Directory Files
    >> "%backupfile%" echo =======================================
    >> "%backupfile%" echo.

    set "include_count=0"
    for %%F in ("include\*.inc" "include\*.html" "include\*.txt" "include\*.h" "include\*.cpp") do (
        if exist "%%~F" (
            set /a include_count+=1
            >> "%backupfile%" echo ---------------------------------------
            >> "%backupfile%" echo File: %%~F
            >> "%backupfile%" echo ---------------------------------------
            type "%%~F" >> "%backupfile%"
            >> "%backupfile%" echo.
        )
    )
    if !include_count! equ 0 (
        >> "%backupfile%" echo No include files found
        echo No include files found
    ) else (
        echo Added !include_count! include files
    )
) else (
    >> "%backupfile%" echo WARNING: include directory not found! Skipping...
    echo WARNING: include directory not found! Skipping...
)

:: Summary
>> "%backupfile%" echo =======================================
>> "%backupfile%" echo Merge completed: %datetime%
>> "%backupfile%" echo =======================================

:: Completion message
echo.
echo ========================================
echo Backup completed successfully!
echo File saved as "%backupfile%"
echo ========================================
echo.
