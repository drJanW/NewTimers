@echo off
setlocal ENABLEEXTENSIONS

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

if defined OUTFILE (
  TREE /A %SHOWFILES% "%TARGET%" > "%OUTFILE%"
  echo Wrote tree to "%OUTFILE%"
) else (
  TREE /A %SHOWFILES% "%TARGET%"
)
exit /b 0

:help
echo Usage: dirtree [path] [--files|-f] [--out file.txt]
echo   path      Directory om te tonen. Default = huidige map.
echo   --files   Toon ook bestandsnamen.
echo   --out     Schrijf output naar bestand.
exit /b 0
