@echo off
setlocal

echo Stopping KotobaCppApp.exe if running...
taskkill /F /IM KotobaCppApp.exe >nul 2>&1

echo Configuring CMake (release, app only)...
cmake -S . -B build-release -G "Visual Studio 18 2026" -A Win32 -DAPP_ONLY=ON
if %errorlevel% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b %errorlevel%
)

echo Building Release configuration...
cmake --build build-release --config Release
if %errorlevel% neq 0 (
    echo CMake build failed!
    pause
    exit /b %errorlevel%
)

set "EXE_PATH=build-release\KotobaCppApp.exe"
if exist "build-release\Release\KotobaCppApp.exe" set "EXE_PATH=build-release\Release\KotobaCppApp.exe"

if not exist "%EXE_PATH%" (
    echo Could not find KotobaCppApp.exe in build-release\ or build-release\Release\
    pause
    exit /b 1
)

echo Built %EXE_PATH%
echo Launching %EXE_PATH%...
start "" "%EXE_PATH%"

endlocal