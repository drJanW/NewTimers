@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

REM defaults
set "TARGET="
set "SHOWFILES="
set "OUTFILE="

:parse
if "%~1"=="" goto run
if /I "%~1"=="--files"  (set "SHOWFILES=/F" & shift & goto parse)
if /I "%~1"=="-f"       (set "SHOWFILES=/F" & shift & goto parse)
if /I "%~1"=="--out"    (set "OUTFILE=%~2"  & shift & shift & goto parse)
if /I "%~1"=="--help"   goto help
if not defined TARGET   (set "TARGET=%~1"   & shift & goto parse)
shift
goto parse

:run
if not defined TARGET set "TARGET=%CD%"

REM normaliseer pad
for %%P in ("%TARGET%") do set "TARGET=%%~fP"

if defined OUTFILE (
    call :PrintTree "%TARGET%" "%SHOWFILES%" > "%OUTFILE%"
    echo Wrote tree to "%OUTFILE%"
) else (
    call :PrintTree "%TARGET%" "%SHOWFILES%"
)
exit /b 0


:PrintTree
REM %1 = root path, %2 = "/F" of leeg
set "ROOT=%~1"
set "FILESFLAG=%~2"

echo Folder PATH listing
echo %ROOT%
call :Tree "%ROOT%" "  " "%FILESFLAG%"
exit /b 0


:Tree
REM %1 = current path
REM %2 = indent string
REM %3 = "/F" of leeg

set "CURPATH=%~1"
set "INDENT=%~2"
set "FILESFLAG=%~3"

REM subdirectories (skip namen die met . beginnen)
for /D %%D in ("%CURPATH%\*") do (
    set "NAME=%%~nxD"
    if not "!NAME:~0,1!"=="." (
        echo %INDENT%!NAME!
        call :Tree "%%~fD" "%INDENT%  " "%FILESFLAG%"
    )
)

REM optioneel files tonen
if /I "%FILESFLAG%"=="/F" (
    for /F "delims=" %%F in ('dir /B /A-D "%CURPATH%"') do (
        echo %INDENT%%%F
    )
)

exit /b 0


:help
echo Usage: dirtree [path] [--files ^|-f] [--out file.txt]
echo   path      Directory om te tonen. Default = huidige map.
echo   --files   Toon ook bestandsnamen.
echo   --out     Schrijf output naar bestand.
echo   Directories waarvan de naam met . begint (en hun inhoud) worden overgeslagen.
exit /b 0
