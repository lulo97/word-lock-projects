@echo off
setlocal

cd /d "%~dp0"

call "%~dp0build.bat"
if errorlevel 1 exit /b 1

set "CMAKE_EXE="
for /f "delims=" %%i in ('where cmake 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%i"
if not defined CMAKE_EXE set "CMAKE_EXE=D:\Programs\cmake-4.3.3-windows-x86_64\cmake-4.3.3-windows-x86_64\bin\cmake.exe"

set "CTEST_EXE="
if exist "%CMAKE_EXE%" for %%f in ("%CMAKE_EXE%") do set "CTEST_EXE=%%~dpfctest.exe"
if not exist "%CTEST_EXE%" set "CTEST_EXE=ctest.exe"

if "%~1"=="" goto run_all

set "TEST_NAME=%~n1"
echo Running test: %TEST_NAME%
"%CTEST_EXE%" --test-dir build -C Release -R "^%TEST_NAME%$" --output-on-failure
exit /b %ERRORLEVEL%

:run_all
echo Running all tests...
"%CTEST_EXE%" --test-dir build -C Release --output-on-failure
exit /b %ERRORLEVEL%
