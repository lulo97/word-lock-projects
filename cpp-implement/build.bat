@echo off
setlocal EnableExtensions EnableDelayedExpansion

:: Capture the script directory before arguments are shifted or VS is called.
set "SCRIPT_DIR=%~dp0"

:: --------------------------------------------------
:: Defaults
:: --------------------------------------------------
set "INPUTS="
set "OUTPUT=build\calculator.exe"

:: --------------------------------------------------
:: Parse arguments
::   build.bat --input calculator.cpp --output build/calculator.exe
:: --------------------------------------------------
:parse_args
if "%~1"=="" goto args_done

if /I "%~1"=="--input" (
    if "%~2"=="" goto missing_value

    set INPUTS=!INPUTS! "%~2"
    shift
    shift
    goto parse_args
)

if /I "%~1"=="--output" (
    if "%~2"=="" goto missing_value

    set "OUTPUT=%~2"
    shift
    shift
    goto parse_args
)

if /I "%~1"=="--help" goto usage
if /I "%~1"=="-h" goto usage

echo Unknown argument: %~1
goto usage_error

:missing_value
echo Missing value for %~1
exit /b 1

:usage
echo Usage: build.bat [--input source.cpp] [--output path\to\app.exe]
echo.
echo   --input   Main source file (default: calculator.cpp)
echo   --output  Output executable (default: build\calculator.exe)
exit /b 0

:usage_error
exit /b 1

:args_done

:: Normalize slashes.
set "OUTPUT=%OUTPUT:/=\%"

:: --------------------------------------------------
:: Visual Studio environment
:: --------------------------------------------------
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community"

call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x86

if errorlevel 1 (
    echo Failed to initialize Visual Studio.
    pause
    exit /b 1
)

cd /d "%SCRIPT_DIR%"

:: --------------------------------------------------
:: Resolve sources
:: --------------------------------------------------
if not defined INPUTS set INPUTS="calculator.cpp"

for %%F in (%INPUTS%) do (
    if not exist "%%~F" (
        echo Input file not found: %%~F
        exit /b 1
    )
)

:: Always compile the framework sources as well.
if exist "ui\*.cpp" (
    for %%F in ("ui\*.cpp") do set INPUTS=!INPUTS! "%%~F"
)

:: Include utility sources (recursively).
if exist "utils" (
    for /r "utils" %%F in (*.cpp) do set INPUTS=!INPUTS! "%%~F"
)

:: Include the new modular UI library.
if exist "ui_new" (
    for /r "ui_new" %%F in (*.cpp) do set INPUTS=!INPUTS! "%%~F"
)

:: Make sure the output directory exists.
for %%D in ("%OUTPUT%") do set "OUTDIR=%%~dpD"
if not "%OUTDIR%"=="" if not exist "%OUTDIR%" mkdir "%OUTDIR%"

:: Embed the application icon (and any other resources) in the .exe.
set "RESOURCE="
if exist "app.rc" (
    rc /nologo /fo app.res app.rc

    if errorlevel 1 (
        echo Failed to compile app.rc
        pause
        exit /b 1
    )

    set "RESOURCE=app.res"
)

:: --------------------------------------------------
:: Build
:: --------------------------------------------------
cl /EHsc /std:c++17 /O2 /I"ui_new" %INPUTS% user32.lib gdi32.lib shell32.lib winmm.lib gdiplus.lib /link /SUBSYSTEM:WINDOWS %RESOURCE% /OUT:"%OUTPUT%"

if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

if exist *.obj del *.obj
if exist *.res del *.res

echo Build successful: %OUTPUT%
pause
