@echo off
setlocal

echo Stopping app.exe if running...
taskkill /F /IM app.exe >nul 2>&1

echo Configuring CMake...
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
if %errorlevel% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b %errorlevel%
)

echo Building Release configuration...
cmake --build build --config Release
if %errorlevel% neq 0 (
    echo CMake build failed!
    pause
    exit /b %errorlevel%
)

set "EXE_PATH=build\app.exe"
if exist "build\Release\app.exe" set "EXE_PATH=build\Release\app.exe"

if not exist "%EXE_PATH%" (
    echo Could not find app.exe in build\ or build\Release\
    pause
    exit /b 1
)

echo Launching %EXE_PATH%...
start "" "%EXE_PATH%"

endlocal