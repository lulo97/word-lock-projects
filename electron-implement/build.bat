@echo off
setlocal
cd /d "%~dp0"

where npm >nul 2>&1
if errorlevel 1 (
    echo Node.js and npm are required. Install them from https://nodejs.org/
    goto :error
)

echo Installing dependencies...
call npm install
if errorlevel 1 goto :error

echo Building Kotoba Lock...
call npm run dist
if errorlevel 1 goto :error

echo.
echo Build complete. Check the "dist" folder.
exit /b 0

:error
echo.
echo Build failed.
exit /b 1
