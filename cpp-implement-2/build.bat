@echo off
setlocal

cd /d "%~dp0"

set "CMAKE_EXE="
for /f "delims=" %%i in ('where cmake 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%i"
if not defined CMAKE_EXE set "CMAKE_EXE=D:\Programs\cmake-4.3.3-windows-x86_64\cmake-4.3.3-windows-x86_64\bin\cmake.exe"

if not exist "%CMAKE_EXE%" (
    echo CMake was not found on PATH or at the configured fallback path.
    exit /b 1
)

set "TOOLCHAIN=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
if defined VCPKG_ROOT set "TOOLCHAIN=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake"
if not defined VCPKG_ROOT if defined VSINSTALLDIR set "TOOLCHAIN=%VSINSTALLDIR%VC\vcpkg\scripts\buildsystems\vcpkg.cmake"

if not exist "build\CMakeCache.txt" (
    if not exist "%TOOLCHAIN%" (
        echo vcpkg toolchain was not found: %TOOLCHAIN%
        echo Install vcpkg dependencies or set VCPKG_ROOT before first build.
        exit /b 1
    )

    echo Configuring build...
    "%CMAKE_EXE%" -S . -B build -A x64 "-DCMAKE_TOOLCHAIN_FILE=%TOOLCHAIN%" -DVCPKG_TARGET_TRIPLET=x64-windows
    if errorlevel 1 exit /b 1
)

echo Building PomodoroLockCpp (Release)...
set "BUILD_LOG=%TEMP%\pomodorolock_build_%RANDOM%.txt"
"%CMAKE_EXE%" --build build --config Release --parallel > "%BUILD_LOG%" 2>&1
set "BUILD_RC=%ERRORLEVEL%"
type "%BUILD_LOG%"
if not "%BUILD_RC%"=="0" (
    del "%BUILD_LOG%" >nul 2>&1
    exit /b %BUILD_RC%
)

rem A new tests/*.cpp file makes CMake regenerate the projects mid-build; the
rem newly added test target only appears in the next build invocation.
findstr /C:"GLOB mismatch" /C:"CMake is re-running" "%BUILD_LOG%" >nul
if not errorlevel 1 (
    echo Test sources changed; rebuilding with regenerated projects...
    "%CMAKE_EXE%" --build build --config Release --parallel
    if errorlevel 1 (
        del "%BUILD_LOG%" >nul 2>&1
        exit /b 1
    )
)
del "%BUILD_LOG%" >nul 2>&1

echo.
echo Build complete: build\Release\PomodoroLockCpp.exe
exit /b 0
