@echo off
setlocal
cd /d "%~dp0"

set "PY=.venv\Scripts\python.exe"

if not exist "%PY%" (
    echo Creating virtual environment...
    where uv >nul 2>&1
    if not errorlevel 1 (
        uv venv .venv
    ) else (
        py -3 -m venv .venv
    )
    if errorlevel 1 goto :error
)

echo Installing dependencies...
where uv >nul 2>&1
if not errorlevel 1 (
    uv pip install --python "%PY%" pyinstaller customtkinter psutil pystray pillow
) else (
    "%PY%" -m ensurepip
    "%PY%" -m pip install --upgrade pip
    "%PY%" -m pip install pyinstaller customtkinter psutil pystray pillow
)
if errorlevel 1 goto :error

echo Generating icons...
"%PY%" assets\generate_icons.py
if errorlevel 1 goto :error

echo Building WordLock.exe...
"%PY%" -m PyInstaller --noconfirm --clean --onefile --windowed ^
    --name WordLock ^
    --icon "assets\icon.ico" ^
    --add-data "database;database" ^
    --add-data "assets;assets" ^
    --collect-all customtkinter ^
    app.py
if errorlevel 1 goto :error

echo.
echo Build complete: dist\WordLock.exe
exit /b 0

:error
echo.
echo Build failed.
exit /b 1
