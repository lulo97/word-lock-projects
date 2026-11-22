# tests/run.ps1
#
# The single test driver. Replaces run_tests.bat: it discovers every test,
# runs it, and prints a failure summary at the end. PowerShell (not batch)
# to avoid the `timeout` PATH collision and working-directory problems.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tests\run.ps1                # everything
#   powershell -ExecutionPolicy Bypass -File tests\run.ps1 -Filter stop    # fast inner loop
#   powershell -ExecutionPolicy Bypass -File tests\run.ps1 -Update         # re-record goldens
#   powershell -ExecutionPolicy Bypass -File tests\run.ps1 -Seed 42        # fixed fuzz seed
#   powershell -ExecutionPolicy Bypass -File tests\run.ps1 -Seed random    # nightly fuzz
#
#   -Filter runs only tests whose path matches the pattern (substring).
#   -Update sets UPDATE_GOLDEN=1 so `golden` rewrites the baseline instead
#   of failing; only use it after a human checked the screenshots.
#   -Seed random picks a random fuzz seed and prints it (on failure, the
#   seed line is in the report so the case is replayable).
#
# Exit: 0 only if every test passes and every script/exe exists.
# Report: tests\out\report.txt

param(
    [string]$Filter = "",
    [switch]$Update,
    [string]$Seed = "1"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot          # project root (tests\.. )
Set-Location $root                                 # exes read tests/word.json etc. from CWD

if ($Update) { $env:UPDATE_GOLDEN = "1" } else { Remove-Item Env:\UPDATE_GOLDEN -ErrorAction SilentlyContinue }
if ($Seed -eq "random") { $Seed = (Get-Random -Minimum 1 -Maximum 2147483647).ToString() }

$reportDir = Join-Path $root "tests\out"
New-Item -ItemType Directory -Force $reportDir | Out-Null
$report = Join-Path $reportDir "report.txt"
Set-Content $report ""

$script:failed = @()

function Invoke-Test($name, $exe, $exeArgs) {
    # One test: run $exe, append its output to the report, record failure.
    # Missing exe or missing script => nonzero exit => recorded failure.
    Add-Content $report "=== $name ==="
    if (-not (Test-Path $exe)) {
        Add-Content $report "  missing exe: $exe"
        $script:failed += $name
        return
    }
    & $exe @exeArgs 2>&1 | ForEach-Object { Add-Content $report $_ }
    if ($LASTEXITCODE -ne 0) { $script:failed += $name }
}

function Match-Filter($path) {
    if ($Filter -eq "") { return $true }
    return ($path -like "*$Filter*")
}

# ---- discover ----
$caseScripts  = @(Get-ChildItem "tests\cases\*\steps*.txt"  | Sort-Object FullName)
$featureScripts = @(Get-ChildItem "tests\features\*\steps.txt" | Sort-Object FullName)

# A discovery that comes up empty is a failure, not a silent pass
# (e.g. a typo'd case folder would otherwise vanish from the suite).
if ($caseScripts.Count -eq 0)   { $script:failed += "discovery: no tests\cases\*\steps*.txt" }
if ($featureScripts.Count -eq 0) { $script:failed += "discovery: no tests\features\*\steps.txt" }

# ---- unit tests ----
if (Match-Filter "unit") {
    Invoke-Test "unit tests" "build\unit_tests.exe" @()
}

# ---- case scripts (steps*.txt: steps.txt, steps_dpi1.txt, ...) ----
foreach ($s in $caseScripts) {
    if (Match-Filter $s.FullName) {
        Invoke-Test ("case " + $s.Directory.Name + " " + $s.Name) "build\ui_test.exe" `
            @("--word", "tests\word.json", "--script", $s.FullName)
    }
}

# ---- metamorphic DPI matrix, per case page ----
foreach ($s in $caseScripts) {
    $page = Join-Path $s.Directory.FullName "page.html"
    if ((Test-Path $page) -and (Match-Filter $s.FullName)) {
        Invoke-Test ("dpi matrix " + $s.Directory.Name) "build\ui_test.exe" `
            @("--word", "tests\word.json", "--dpi-matrix", $page)
    }
}

# ---- feature tests: one folder per feature ----
foreach ($s in $featureScripts) {
    if (Match-Filter $s.FullName) {
        Invoke-Test ("feature " + $s.Directory.Name) "build\app_test.exe" `
            @("--script", $s.FullName)
    }
}

# ---- fuzz (fixed seed for gating, random for nightly) ----
if (Match-Filter "fuzz") {
    Invoke-Test "fuzz (seed $Seed)" "build\fuzz.exe" @("--seed", $Seed, "--iters", "2000")
}

# ---- the ONE real-process smoke test ----
if (Match-Filter "smoke") {
    # powershell is a command, not a file path, so Test-Path can't check it.
    Add-Content $report "=== smoke ==="
    & powershell -ExecutionPolicy Bypass -File (Join-Path $root "tests\smoke.ps1") 2>&1 | ForEach-Object { Add-Content $report $_ }
    if ($LASTEXITCODE -ne 0) { $script:failed += "smoke" }
}

# ---- summary: the failed list at the bottom, no scrolling needed ----
Add-Content $report ""
if ($script:failed.Count -eq 0) {
    Add-Content $report "=== ALL PASS ==="
} else {
    Add-Content $report "=== FAILED: $($script:failed.Count) ==="
    $script:failed | ForEach-Object { Add-Content $report "  $_" }
}
Get-Content $report
exit $(if ($script:failed.Count) { 1 } else { 0 })
