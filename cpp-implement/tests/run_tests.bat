@echo off
cd /d "%~dp0\.."
setlocal

if not exist tests\out mkdir tests\out
del tests\out\report.txt 2>nul

set FAIL=0

echo === unit tests === >> tests\out\report.txt
build\unit_tests.exe >> tests\out\report.txt 2>&1 || set FAIL=1

for /d %%c in (tests\cases\*) do (
    echo === case %%c === >> tests\out\report.txt
    build\ui_test.exe --word tests\word.json --script %%c\steps.txt >> tests\out\report.txt 2>&1
    if errorlevel 1 (echo CASE %%c FAILED >> tests\out\report.txt & set FAIL=1)
)

echo === dpi matrix === >> tests\out\report.txt
for /d %%c in (tests\cases\*) do (
    build\ui_test.exe --word tests\word.json --dpi-matrix %%c\page.html >> tests\out\report.txt 2>&1
    if errorlevel 1 (echo DPI %%c FAILED >> tests\out\report.txt & set FAIL=1)
)

REM Feature tests: one folder per feature, each a steps.txt driven by
REM app_test.exe (headless, no launched process). See better_feature_test_setup.md.
if exist tests\features (
    echo === features === >> tests\out\report.txt
    for /d %%c in (tests\features\*) do (
        echo === feature %%c === >> tests\out\report.txt
        build\app_test.exe --script %%c\steps.txt >> tests\out\report.txt 2>&1
        if errorlevel 1 (echo FEATURE %%c FAILED >> tests\out\report.txt & set FAIL=1)
    )
)

echo === fuzz === >> tests\out\report.txt
build\fuzz.exe --seed 1 --iters 2000 >> tests\out\report.txt 2>&1 || set FAIL=1

type tests\out\report.txt
exit /b %FAIL%
